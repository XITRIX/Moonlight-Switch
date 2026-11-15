// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <optional>
#include <switch.h>
#include "../../common/game_arm.h"
#include "../../common/domain_mount.hpp"
#include "../../common/package.hpp"
#include "../../common/loader_gate.hpp"
#include "game_trace.hpp"

using GameMemory = ounce::RetainedMemory<ounce::MemoryWriter::RoProcess>;
using GameGate = ounce::LoaderGate<ounce::MemoryWriter::RoProcess>;
using GamePackage = ounce::NativePackage<ounce::MemoryWriter::RoProcess>;
using GameMount = ounce::DomainMount<ounce::MemoryWriter::RoProcess>;

class GameArm {
  public:
    ~GameArm();

    Result configure(const HenGameConfig& config);
    Result adoptHandles0(const Handle handles[8]);
    Result adoptHandles1(const Handle handles[7]);
    /// False when pm:dmnt reports that the process no longer exists. Before
    /// HEN has the pm:dmnt session, every process counts as existing.
    bool processExists(u64 process_id);
    Result adoptLoaderRoot(Handle root);
    bool researchLoaderAvailable() const { return loader_root_ != INVALID_HANDLE; }
    Result openResearchCodeFs(u64 program_id, u8 attributes, Handle* out);
    Result start(Handle filesystem_root, Handle bootstrap_process);
    void step();
    HenGameStatus status() const;

  private:
    enum class InputChoice {
        Unknown,
        Released,
        Pressed,
    };

    enum class State {
        Inactive,
        OpenPackage,
        WaitForInput,
        ParkGate,
        WaitRequest,
        WaitProcess,
        CapturePolicy,
        WaitExit,
        RecoveringGate,
        Fault,
    };

    Result openPackage();
    Result waitForInput();
    Result parkGate();
    Result receiveGameMount();
    Result waitForProcess();
    Result capturePolicy();
    Result pollProcessExit();
    Result restoreGate(Result result, State on_success = State::WaitProcess);
    void resumeAfterRestore(State target);
    Result inspectGameRequest(const u8 request[ounce::LoaderRequestSize], bool& game_mount,
                              u64& title, u8& storage) const;
    Result initializeInput();
    InputChoice sampleR();
    void fault(Result result);

    HenGameConfig configuration_ = {};
    ounce::RetainedMemoryConfig memory_config_ = {};
    ounce::LoaderGateConfig gate_config_ = {};
    ounce::DomainMountConfig mount_config_ = {};
    Handle monitor_ = INVALID_HANDLE;
    Handle reader_session_ = INVALID_HANDLE;
    Handle filesystem_process_ = INVALID_HANDLE;
    Handle filesystem_notifier_ = INVALID_HANDLE;
    Handle filesystem_root_ = INVALID_HANDLE;
    Handle loader_root_ = INVALID_HANDLE;
    ounce::DomainMountConfig current_mount_ = {};
    std::optional<GameMemory> memory_;
    std::optional<GamePackage> package_;
    std::optional<GameGate> gate_;
    std::optional<GameMount> mount_;
    ounce::DomainEntry first_ = {};
    ounce::DomainEntry code_ = {};
    ounce::DomainEntry before_step_[ounce::MaxDomainEntries] = {};
    u32 before_step_count_ = 0;
    alignas(16) u8 request_[ounce::LoaderRequestSize] = {};
    bool step_started_ = false;
    HenGameStatus status_ = {};
    GameTrace trace_ = {};
    State state_ = State::Inactive;
    bool first_closed_ = false;
    bool input_ready_ = false;
    u64 active_title_ = 0;
    u8 active_storage_ = 0;
    bool configured_ = false;
    bool bootstrap_restore_pending_ = false;
    bool allow_bridge_process_ = false;
    bool handles0_ = false;
    bool handles1_ = false;
    bool started_ = false;
    bool policy_ready_ = false;
    bool capture_pending_ = false;
    u32 capture_attempts_ = 0;
    u64 capture_next_tick_ = 0;
    Result recovery_primary_result_ = 0;
    State recovery_target_ = State::Fault;
    Result last_restore_result_ = 0;
    u32 gate_failure_step_ = 0;
};
