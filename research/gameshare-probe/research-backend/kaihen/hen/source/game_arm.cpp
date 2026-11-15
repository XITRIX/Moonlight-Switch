// SPDX-License-Identifier: GPL-2.0-only
#include <cstring>
#include <switch.h>
#include "game_arm.hpp"
#include "../../common/package.hpp"
#include "../../common/result.hpp"
#include "../../libkaihen/source/cmif_server.hpp"
#include "../../common/policy_package.hpp"
#include <switch/arm/counter.h>
#include "../../common/research_codefs.h"

namespace {

constexpr Result UnexpectedState = MAKERESULT(Module_Libnx, LibnxError_BadInput);
/// PM's result for a process ID or program ID that no process has.
constexpr Result ProcessNotFound = MAKERESULT(15, 1);
/// FS's result for a path that does not exist.
constexpr Result PathNotFound = MAKERESULT(2, 1);
/// The content attributes of the game's code file system, in Loader's
/// OpenCodeFileSystem request and in readSignedPolicy.
constexpr u32 GameAttributes = 1;
// Native titles observed here use the 0x04 namespace. This is the Switch 2
// analogue of Atmosphere's ncm::IsApplicationId range check; the exact native
// endpoints still need live qualification against more than one game title.
constexpr u64 ApplicationProgramIdMin = 0x0400000000010000ull;
constexpr u64 ApplicationProgramIdMax = 0x04ffffffffffffffull;
constexpr u32 NativeNpadRevision = 5;
constexpr char GamePackagePath[] = "@Sdcard:/kaihen/bin/game/exefs.nsp";
constexpr char GameMainPath[] = "/kaihen/bin/game/main";
constexpr char GamePolicyPath[] = "/kaihen/bin/game/main.npdm";
constexpr char GamePackageFile[] = "/kaihen/bin/game/exefs.nsp";
/// The first launch tries to capture the game's policy this many times, once
/// a second, while the game runs.
constexpr u32 CaptureAttempts = 20;
constexpr u64 CaptureIntervalNs = 1000000000;
/// How long sampleR waits for each controller to post a new sample.
constexpr u64 InputSampleIntervalNs = 50000000;

bool isApplicationProgram(u64 program_id) {
    return program_id >= ApplicationProgramIdMin && program_id <= ApplicationProgramIdMax;
}

/// The value of a fault trace: the trace point that preceded the fault in the
/// high half, and that point's value (or the gate step that failed) in the low.
u32 faultCause(u32 point, u32 value) {
    return (point << 16) | (value & 0xffff);
}

bool containsEntry(const ounce::DomainEntry entries[ounce::MaxDomainEntries], u32 count,
                   u64 address) {
    for (u32 index = 0; index < count; index++) {
        if (entries[index].address == address) {
            return true;
        }
    }
    return false;
}

} // namespace

GameArm::~GameArm() {
    if (input_ready_) {
        hidExit();
    }
    ounce::closeHandle(&filesystem_root_);
    ounce::closeHandle(&loader_root_);
    ounce::closeHandle(&monitor_);
    ounce::closeHandle(&reader_session_);
    ounce::closeHandle(&filesystem_process_);
    ounce::closeHandle(&filesystem_notifier_);
    ounce::closeHandle(&memory_config_.storage);
    for (Handle& worker : gate_config_.workers) {
        ounce::closeHandle(&worker);
    }
    ounce::closeHandle(&gate_config_.server_copy);
    ounce::closeHandle(&gate_config_.event_read);
    ounce::closeHandle(&gate_config_.event_write);
}

Result GameArm::openResearchCodeFs(u64 program_id, u8 attributes, Handle* out) {
    const ResearchCodeFsOpen in={attributes,2,{},program_id};
    if (!out || !researchCodeFsValid(&in)) return UnexpectedState;
    *out=INVALID_HANDLE;
    if (loader_root_==INVALID_HANDLE)
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    // Clone the existing root instead of acquiring another fsp-ldr connection.
    Handle copy=INVALID_HANDLE;
    Result rc=cmifCloneCurrentObject(loader_root_,&copy);
    if (R_FAILED(rc)) return rc;
    Service loader={}; serviceCreate(&loader,copy);
    u8 code_info[0x1a4]={}; FsFileSystem filesystem={};
    rc=serviceDispatchIn(&loader,0,in,
        .buffer_attrs={SfBufferAttr_HipcMapAlias | SfBufferAttr_Out},
        .buffers={{code_info,sizeof(code_info)}},
        .out_num_objects=1,.out_objects=&filesystem.s);
    serviceClose(&loader);
    if (R_FAILED(rc)) return rc;
    // A newly returned CodeFS is a non-domain session. Do not silently detach
    // a domain object, whose ID would be lost when copied to a different client.
    if (!filesystem.s.session || filesystem.s.object_id) {
        fsFsClose(&filesystem); return UnexpectedState;
    }
    *out=filesystem.s.session;
    filesystem.s={}; // Ownership passes to the reply, which closes after COPY.
    return 0;
}

Result GameArm::configure(const HenGameConfig& config) {
    R_UNLESS(!started_, UnexpectedState);
    R_UNLESS(!configured_, UnexpectedState);
    R_UNLESS(config.bootstrap_restore_pending <= 1, UnexpectedState);

    configuration_ = config;
    bootstrap_restore_pending_ = config.bootstrap_restore_pending != 0;
    memory_config_.data_offset = config.data_offset;
    memory_config_.pointer_offset = config.pointer_offset;
    gate_config_.server = config.server;
    gate_config_.vptr = config.vptr;
    gate_config_.domain = config.domain;
    gate_config_.metadata = config.metadata;
    gate_config_.request = config.request;
    gate_config_.receive = config.receive;
    gate_config_.original_wait = config.original_wait;
    gate_config_.park = config.park;
    std::memcpy(gate_config_.waiters, config.waiters, sizeof(config.waiters));
    mount_config_.table = config.table;
    mount_config_.domain = config.domain;
    mount_config_.root_id = config.loader_root_id;
    configured_ = true;
    R_SUCCEED();
}

Result GameArm::adoptHandles0(const Handle handles[8]) {
    R_UNLESS(configured_, UnexpectedState);
    R_UNLESS(!started_, UnexpectedState);
    R_UNLESS(!handles0_, UnexpectedState);

    memory_config_.storage = handles[0];
    std::memcpy(gate_config_.workers, handles + 1, sizeof(gate_config_.workers));
    handles0_ = true;
    R_SUCCEED();
}

Result GameArm::adoptHandles1(const Handle handles[7]) {
    R_UNLESS(configured_, UnexpectedState);
    R_UNLESS(!started_, UnexpectedState);
    R_UNLESS(!handles1_, UnexpectedState);

    gate_config_.server_copy = handles[0];
    gate_config_.event_read = handles[1];
    gate_config_.event_write = handles[2];
    monitor_ = handles[3];
    reader_session_ = handles[4];
    filesystem_process_ = handles[5];
    filesystem_notifier_ = handles[6];
    handles1_ = true;
    R_SUCCEED();
}

bool GameArm::processExists(u64 process_id) {
    if (monitor_ == INVALID_HANDLE) {
        return true;
    }
    // The same session that openPackage installs for game arming.
    serviceCreate(pmdmntGetServiceSession(), monitor_);
    u64 program_id = 0;
    return pmdmntGetProgramId(&program_id, process_id) != ProcessNotFound;
}

Result GameArm::adoptLoaderRoot(Handle root) {
    R_UNLESS(configured_, UnexpectedState);
    R_UNLESS(!started_, UnexpectedState);
    R_UNLESS(loader_root_ == INVALID_HANDLE, UnexpectedState);
    R_UNLESS(root != INVALID_HANDLE, UnexpectedState);

    loader_root_ = root;
    R_SUCCEED();
}

Result GameArm::start(Handle filesystem_root, Handle bootstrap_process) {
    R_UNLESS(configured_, UnexpectedState);
    R_UNLESS(handles0_, UnexpectedState);
    R_UNLESS(handles1_, UnexpectedState);
    R_UNLESS(!started_, UnexpectedState);
    R_UNLESS(loader_root_ != INVALID_HANDLE, UnexpectedState);
    R_UNLESS(filesystem_root != INVALID_HANDLE, UnexpectedState);
    R_UNLESS(bootstrap_process != INVALID_HANDLE, UnexpectedState);

    SignedPolicy saved = {};
    Result rc = readSavedPolicy(filesystem_root, GamePolicyPath, &saved);
    if (R_SUCCEEDED(rc)) {
        R_TRY(writeExeFsPackage(filesystem_root, GameMainPath, GamePackageFile, saved));
        policy_ready_ = true;
        trace_.write(GameTracePoint_SavedPolicyReady);
    } else if (rc != PathNotFound) {
        return rc;
    }
    R_TRY(initializeInput());
    filesystem_root_ = filesystem_root;
    svcCloseHandle(bootstrap_process);
    started_ = true;
    state_ = State::OpenPackage;
    status_.phase = HenGamePhase_OpeningPackage;
    return 0;
}

HenGameStatus GameArm::status() const {
    return status_;
}

void GameArm::fault(Result result) {
    const GameTraceSnapshot previous = trace_.snapshot();
    const u32 cause = gate_failure_step_
                          ? faultCause(GameTracePoint_AfterStep, gate_failure_step_)
                          : faultCause(previous.point, previous.value);
    trace_.write(GameTracePoint_Fault, result, cause);
    status_.result = result;
    status_.phase = HenGamePhase_Fault;
    state_ = State::Fault;
}

void GameArm::step() {
    Result rc = 0;
    switch (state_) {
    case State::Inactive:
    case State::Fault:
        return;
    case State::OpenPackage:
        rc = openPackage();
        break;
    case State::WaitForInput:
        rc = waitForInput();
        break;
    case State::ParkGate:
        rc = parkGate();
        break;
    case State::WaitRequest:
        rc = receiveGameMount();
        break;
    case State::WaitProcess:
        rc = waitForProcess();
        break;
    case State::CapturePolicy:
        rc = capturePolicy();
        break;
    case State::WaitExit:
        rc = pollProcessExit();
        break;
    case State::RecoveringGate: {
        const Result gate_result = gate_->restore(1);
        const Result memory_result = memory_->restore();
        const Result restore_result = R_FAILED(gate_result) ? gate_result : memory_result;
        if (restore_result != last_restore_result_) {
            trace_.write(GameTracePoint_GateRecovery, restore_result,
                         gate_->diagnosticStep());
            last_restore_result_ = restore_result;
        }
        if (R_SUCCEEDED(restore_result)) {
            trace_.write(GameTracePoint_GateRestored, 0, gate_->diagnosticStep());
            if (R_FAILED(recovery_primary_result_) && recovery_target_ == State::Fault) {
                fault(recovery_primary_result_);
            } else {
                status_.result = recovery_primary_result_;
                resumeAfterRestore(recovery_target_);
            }
        }
        break;
    }
    }
    if (R_FAILED(rc)) {
        fault(rc);
    }
}

Result GameArm::openPackage() {
    if (!memory_) {
        trace_.write(GameTracePoint_WorkerEntered);
        Service* monitor = pmdmntGetServiceSession();
        serviceCreate(monitor, monitor_);
        trace_.write(GameTracePoint_MonitorReady);

        memory_.emplace(memory_config_, reader_session_, filesystem_process_);
        Result rc = memory_->begin();
        trace_.write(GameTracePoint_MemoryReady, rc);
        if (R_FAILED(rc)) {
            return rc;
        }
    }

    trace_.write(GameTracePoint_CycleEntered);
    status_.phase = HenGamePhase_OpeningPackage;
    status_.result = 0;
    status_.mounts = 0;
    status_.process_id = 0;
    first_ = {};
    code_ = {};
    first_closed_ = false;
    active_title_ = 0;
    active_storage_ = 0;
    capture_pending_ = false;
    gate_failure_step_ = 0;
    step_started_ = false;
    before_step_count_ = 0;
    current_mount_ = mount_config_;
    gate_.emplace(*memory_, gate_config_);
    mount_.emplace(*memory_, current_mount_);
    if (bootstrap_restore_pending_) {
        // Bootstrap left the original Loader wait installed and its park
        // event signalled. Keep that event until this server's receipt is
        // observed. The current application may be the SmileBASIC bridge or
        // the stock game that supplied the receipt; wait for it to exit.
        gate_->adoptPendingReceipt();
        bootstrap_restore_pending_ = false;
        allow_bridge_process_ = true;
        recovery_primary_result_ = 0;
        recovery_target_ = State::WaitProcess;
        state_ = State::RecoveringGate;
        status_.phase = HenGamePhase_RecoveringGate;
        return 0;
    }
    state_ = State::WaitForInput;
    status_.phase = HenGamePhase_WaitingForGame;
    return 0;
}

Result GameArm::waitForInput() {
    if (sampleR() != InputChoice::Pressed) {
        return 0;
    }

    if (!policy_ready_) {
        // The first R launch stays stock. PM identifies the running title,
        // then its live CodeFS supplies the signed policy for later launches.
        capture_pending_ = true;
        capture_attempts_ = 0;
        capture_next_tick_ = 0;
        trace_.write(GameTracePoint_FirstLaunchCapture);
        state_ = State::WaitProcess;
        status_.phase = HenGamePhase_WaitingForProcess;
        return 0;
    }
    state_ = State::ParkGate;
    status_.phase = HenGamePhase_Parking;
    return 0;
}

Result GameArm::parkGate() {
    status_.phase = HenGamePhase_Parking;
    Result rc = gate_->park();
    trace_.write(GameTracePoint_GateParked, rc, gate_->diagnosticStep());
    if (R_FAILED(rc)) {
        return restoreGate(rc);
    }
    status_.phase = HenGamePhase_WaitingForGame;
    state_ = State::WaitRequest;
    trace_.write(GameTracePoint_ReceiveLoop);
    return 0;
}

Result GameArm::inspectGameRequest(const u8 request[ounce::LoaderRequestSize],
                                   bool& game_mount, u64& title, u8& storage) const {
    game_mount = false;
    title = 0;
    storage = 0;
    ounce::CodeMountRequest decoded;
    const Result rc = ounce::decodeCodeMountRequest(request, mount_config_.root_id, decoded);
    if (R_FAILED(rc)) {
        return UnexpectedState;
    }
    // Any application from any storage: the R selection decides which launch
    // is armed, not the title.
    const ounce::CodeMountArguments& arguments = decoded.arguments;
    if (!decoded.open_code || arguments.content_attributes != GameAttributes ||
        !isApplicationProgram(arguments.program_id)) {
        return 0;
    }
    title = arguments.program_id;
    storage = arguments.storage_id;
    game_mount = true;
    return 0;
}

Result GameArm::initializeInput() {
    R_TRY(hidInitialize());
    input_ready_ = true;

    // Configure the pad set; on any failure the single cleanup below runs.
    // HEN's migration role used revision 5 in the live Npad probe. The
    // libounce client sends the native HID commands and returns their Result.
    const auto configure = [&]() -> Result {
        R_TRY(hidActivateNpadWithRevision(NativeNpadRevision));
        const u32 styles = HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadSystemExt;
        R_TRY(hidSetSupportedNpadStyleSet(styles));
        const HidNpadIdType ids[] = {
            HidNpadIdType_No1, HidNpadIdType_No2, HidNpadIdType_No3,
            HidNpadIdType_No4, HidNpadIdType_No5, HidNpadIdType_No6,
            HidNpadIdType_No7, HidNpadIdType_No8, HidNpadIdType_Handheld,
        };
        return hidSetSupportedNpadIdType(ids, sizeof(ids) / sizeof(ids[0]));
    };
    const Result rc = configure();
    if (R_FAILED(rc)) {
        hidExit();
        input_ready_ = false;
    }
    return rc;
}

GameArm::InputChoice GameArm::sampleR() {
    constexpr HidNpadIdType ids[] = {
        HidNpadIdType_No1, HidNpadIdType_No2, HidNpadIdType_No3,
        HidNpadIdType_No4, HidNpadIdType_No5, HidNpadIdType_No6,
        HidNpadIdType_No7, HidNpadIdType_No8, HidNpadIdType_Handheld,
    };
    u64 previous[sizeof(ids) / sizeof(ids[0])] = {};
    bool baseline_valid[sizeof(ids) / sizeof(ids[0])] = {};
    for (size_t index = 0; index < sizeof(ids) / sizeof(ids[0]); index++) {
        HidNpadSystemExtState state = {};
        if (hidGetNpadStatesSystemExt(ids[index], &state, 1) == 1 &&
            (state.attributes & HidNpadAttribute_IsConnected) &&
                   state.sampling_number != 0) {
            previous[index] = state.sampling_number;
            baseline_valid[index] = true;
        }
    }
    svcSleepThread(InputSampleIntervalNs);
    bool fresh = false;
    bool incomplete = false;
    for (size_t index = 0; index < sizeof(ids) / sizeof(ids[0]); index++) {
        HidNpadSystemExtState state = {};
        if (!baseline_valid[index]) {
            const bool observed = hidGetNpadStatesSystemExt(ids[index], &state, 1) == 1;
            if (!observed || (state.attributes & HidNpadAttribute_IsConnected)) {
                incomplete = true;
            }
            continue;
        }
        if (hidGetNpadStatesSystemExt(ids[index], &state, 1) != 1 ||
            state.sampling_number <= previous[index] ||
            !(state.attributes & HidNpadAttribute_IsConnected)) {
            incomplete = true;
            continue;
        }
        fresh = true;
        if (state.buttons & HidNpadButton_R) {
            trace_.write(GameTracePoint_InputSample, 0, static_cast<u32>(index + 1));
            return InputChoice::Pressed;
        }
    }
    const InputChoice choice = fresh && !incomplete ? InputChoice::Released
                                                    : InputChoice::Unknown;
    trace_.write(GameTracePoint_InputSample, 0,
                 choice == InputChoice::Released ? 0 : 0xffffffffu);
    return choice;
}

Result GameArm::receiveGameMount() {
    Result rc = 0;
    if (!step_started_) {
        bool ready = false;
        rc = gate_->pending(ready);
        if (R_FAILED(rc)) return restoreGate(rc);
        if (!ready) return 0;
        trace_.write(GameTracePoint_RequestReady, 0, status_.mounts);

        rc = mount_->snapshot(before_step_, before_step_count_);
        trace_.write(GameTracePoint_FirstSnapshotDone, rc, before_step_count_);
        if (R_FAILED(rc)) return restoreGate(rc);

        trace_.write(GameTracePoint_BeforeStep, 0, before_step_count_);
        rc = gate_->beginStep();
        if (R_FAILED(rc)) {
            gate_failure_step_ = gate_->failureStep();
            trace_.write(GameTracePoint_AfterStep, rc, gate_failure_step_);
            return restoreGate(rc);
        }
        step_started_ = true;
    }

    bool complete = false;
    rc = gate_->pollStep(request_, complete);
    if (R_FAILED(rc)) {
        gate_failure_step_ = gate_->failureStep();
        trace_.write(GameTracePoint_AfterStep, rc, gate_failure_step_);
        step_started_ = false;
        // The exchange has not begun. Once the gate is restored, Loader can
        // continue this request against its original CodeFS object.
        return restoreGate(rc, State::WaitProcess);
    }
    if (!complete) return 0;
    step_started_ = false;
    trace_.write(GameTracePoint_AfterStep, 0, gate_->diagnosticStep());

    ounce::DomainEntry after[ounce::MaxDomainEntries], added;
    u32 after_count = 0;
    rc = mount_->snapshot(after, after_count);
    trace_.write(GameTracePoint_SecondSnapshotDone, rc, after_count);
    if (R_FAILED(rc)) return restoreGate(rc);

    if (status_.mounts == 1 && !containsEntry(after, after_count, first_.address)) {
        first_closed_ = true;
    }
    bool matching = false;
    u64 title = 0;
    u8 storage = 0;
    rc = inspectGameRequest(request_, matching, title, storage);
    trace_.write(GameTracePoint_RequestInspected, rc, matching ? 1 : 0);
    if (R_FAILED(rc)) {
        return restoreGate(rc);
    }
    if (!matching) {
        return 0;
    }
    if (status_.mounts == 0) {
        active_title_ = title;
        active_storage_ = storage;
        current_mount_.title = title;
    } else if (title != active_title_ || storage != active_storage_) {
        return 0;
    }
    rc = mount_->added(before_step_, before_step_count_, after, after_count, added);
    if (R_FAILED(rc)) {
        return restoreGate(rc);
    }
    if (status_.mounts == 0) {
        first_ = added;
        status_.mounts = 1;
        return 0;
    }
    if (status_.mounts != 1 || !first_closed_ || added.object == first_.object) {
        return restoreGate(UnexpectedState);
    }

    code_ = added;
    status_.mounts = 2;
    // Opening a game without R never parks Loader. Once R arms a request,
    // the selection stays latched through both CodeFS mounts.
    package_.emplace(*memory_, filesystem_root_);
    rc = package_->open(GamePackagePath, current_mount_);
    trace_.write(GameTracePoint_PackageOpened, rc);
    if (R_FAILED(rc)) {
        // No CodeFS exchange was attempted. Once Loader's original wait is
        // restored, the pending title can continue on its stock CodeFS.
        return restoreGate(rc, State::WaitProcess);
    }
    rc = mount_->exchange(code_);
    return restoreGate(rc, R_SUCCEEDED(rc) ? State::WaitProcess : State::Fault);
}

Result GameArm::waitForProcess() {
    u64 pid = 0;
    Result rc = active_title_ ? pmdmntGetProcessId(&pid, active_title_)
                              : pmdmntGetApplicationProcessId(&pid);
    if (rc == ProcessNotFound) {
        return 0;
    }
    if (R_FAILED(rc) || !pid) {
        return R_FAILED(rc) ? rc : UnexpectedState;
    }
    if (!active_title_) {
        u64 title = 0;
        rc = pmdmntGetProgramId(&title, pid);
        if (rc == ProcessNotFound) {
            return 0;
        }
        if (R_FAILED(rc)) {
            return rc;
        }
        if (!allow_bridge_process_ && !isApplicationProgram(title)) {
            return UnexpectedState;
        }
        active_title_ = title;
    }
    status_.process_id = pid;
    if (capture_pending_) {
        state_ = State::CapturePolicy;
        return 0;
    }
    status_.phase = HenGamePhase_WaitingForExit;
    state_ = State::WaitExit;
    return 0;
}

Result GameArm::capturePolicy() {
    u64 program_id = 0;
    Result rc = pmdmntGetProgramId(&program_id, status_.process_id);
    if (rc == ProcessNotFound) {
        status_.phase = HenGamePhase_WaitingForExit;
        state_ = State::WaitExit;
        return 0;
    }
    if (R_FAILED(rc)) return rc;
    if (program_id != active_title_) return UnexpectedState;

    const u64 now = armGetSystemTick();
    if (capture_next_tick_ && now < capture_next_tick_) return 0;
    capture_next_tick_ = now + armNsToTicks(CaptureIntervalNs);
    capture_attempts_++;
    trace_.write(GameTracePoint_PolicyCaptureAttempt, 0, capture_attempts_);

    SignedPolicy policy = {};
    // The live game's CodeFS may reside on a game card, built-in user
    // storage, or SD. The first launch remains stock, so its Loader request
    // does not pass through our mount observer to supply the storage ID.
    const u8 storages[] = {
        NcmStorageId_BuiltInUser,
        NcmStorageId_SdCard,
        NcmStorageId_GameCard,
    };
    for (u8 storage : storages) {
        rc = readSignedPolicy(loader_root_, active_title_, storage, GameAttributes, &policy);
        if (R_SUCCEEDED(rc)) {
            rc = writeExeFsPackage(filesystem_root_, GameMainPath, GamePackageFile, policy);
            if (R_SUCCEEDED(rc)) {
                rc = saveSignedPolicy(filesystem_root_, GamePolicyPath, policy);
            }
            if (R_SUCCEEDED(rc)) {
                policy_ready_ = true;
                active_storage_ = storage;
                trace_.write(GameTracePoint_PolicyCaptured, 0, storage);
                break;
            }
        }
    }
    if (policy_ready_ || capture_attempts_ >= CaptureAttempts) {
        if (!policy_ready_) {
            trace_.write(GameTracePoint_PolicyCaptureFailed, rc, capture_attempts_);
        }
        status_.result = policy_ready_ ? 0 : rc;
        status_.phase = HenGamePhase_WaitingForExit;
        state_ = State::WaitExit;
    }
    return 0;
}

Result GameArm::pollProcessExit() {
    u64 program_id = 0;
    Result rc = pmdmntGetProgramId(&program_id, status_.process_id);
    if (R_SUCCEEDED(rc)) {
        return program_id == active_title_ ? 0 : UnexpectedState;
    }
    if (rc != ProcessNotFound) {
        return rc;
    }

    if (!capture_pending_) {
        ounce::DomainEntry entries[ounce::MaxDomainEntries];
        u32 count = 0;
        GameMount final_mount(*memory_, mount_config_);
        R_TRY(final_mount.snapshot(entries, count));
        if (containsEntry(entries, count, code_.address)) return 0;
    }
    if (package_) package_->close();
    package_.reset();
    mount_.reset();
    gate_.reset();
    allow_bridge_process_ = false;
    status_.serial++;
    state_ = State::OpenPackage;
    return 0;
}

Result GameArm::restoreGate(Result result, State on_success) {
    if (on_success == State::WaitProcess && !active_title_) {
        // A failed park or first mount may not have yielded a title yet.
        // Let the next application continue through stock Loader, then
        // observe its exit before arming another launch.
        allow_bridge_process_ = true;
    }
    Result restore_result = gate_->restore(1);
    Result channel_result = memory_->restore();
    const Result cleanup_error = R_FAILED(restore_result) ? restore_result : channel_result;
    if (R_FAILED(cleanup_error)) {
        recovery_primary_result_ = result;
        recovery_target_ = on_success;
        last_restore_result_ = cleanup_error;
        status_.result = R_FAILED(result) ? result : cleanup_error;
        status_.phase = HenGamePhase_RecoveringGate;
        state_ = State::RecoveringGate;
        trace_.write(GameTracePoint_GateRecovery, cleanup_error,
                     gate_->diagnosticStep());
        return 0;
    }
    if (on_success != State::Fault) {
        trace_.write(GameTracePoint_GateRestored, 0, gate_->diagnosticStep());
        status_.result = result;
        resumeAfterRestore(on_success);
        return 0;
    }
    return result;
}

void GameArm::resumeAfterRestore(State target) {
    if (target == State::WaitProcess) {
        status_.phase = HenGamePhase_WaitingForProcess;
    }
    state_ = target;
}
