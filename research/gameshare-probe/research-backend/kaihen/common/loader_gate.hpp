// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "result.hpp"
#include "retained_memory.hpp"
extern "C" {
#include <switch/arm/counter.h>
#include <switch/kernel/svc.h>
}

namespace ounce {

struct LoaderGateConfig {
    u64 server;
    u64 vptr;
    u64 domain;
    u64 metadata;
    u64 request;
    u64 waiters[7];
    Handle workers[7];
    Handle receive;
    Handle original_wait;
    Handle park;
    Handle server_copy;
    Handle event_read;
    Handle event_write;
};

struct LoaderSessionView {
    u64 vptr;
    u8 reserved_x8[0x20];
    u64 parent;
    Handle wait;
    u32 reserved_x34;
    u64 tag;
    u32 state;
    u32 receive_result;
    Handle receive;
    u8 reserved_x4c[0x14];
    u64 request;
    u64 capacity;
    u64 domain;
    u64 metadata;
    u64 reserved_x80;
};

struct LoaderGateProbe {
    u64 parent;
    u64 epoch;
    u64 selection;
    u32 wait;
    u32 receive;
    u32 state;
    u32 receive_result;
    Result read_result;
    Result event_result;
    Result server_result;
    u32 waiter_mask;
};
static_assert(sizeof(LoaderGateProbe) == 56);
static_assert(sizeof(LoaderSessionView) == 0x88);
static_assert(__builtin_offsetof(LoaderSessionView, request) == 0x60);

// The step codes that markStep() records in diagnostic_step_. They name the
// point a failing operation reached, so a fault report identifies it. The
// numeric values are stable: a host-side fault decoder reads them. Several
// observe-loop and restore-loop codes add the attempt index to a base, so the
// trailing nibble(s) of a recorded value give the attempt.
enum GateStep : u32 {
    // park()/beginStep()/step() preamble.
    GateStep_InspectView = 1,
    GateStep_RequireParked = 2,
    GateStep_CheckPending = 3,
    GateStep_ReadOriginalHeader = 4,
    GateStep_MarkRequest = 5,
    // One observe-loop iteration (base + attempt).
    GateStep_ObserveSelected = 0x100,
    GateStep_ObserveRequestReady = 0x300,
    GateStep_PendingRecheck = 0x400,
    GateStep_PendingChanged = 0x500,
    GateStep_ObserveInspect = 0x600,
    GateStep_ObserveEpoch = 0x700,
    GateStep_ObserveHeader = 0x800,
    GateStep_ObserveCancel = 0x900,
    GateStep_ObserveInject = 0xb00,
    GateStep_ObserveEmptyComplete = 0xc00,
    GateStep_ObserveReadRequest = 0xd00,
    GateStep_StepDeadline = 0x1100,
    GateStep_FailAfterInjection = 0x1200,
    // inject().
    GateStep_InjectReadWaiter = 0xb10,
    GateStep_InjectInspect = 0xb20,
    GateStep_InjectReadHeader = 0xb30,
    GateStep_InjectSelect = 0xb40,
    // restore() phases.
    GateStep_RestoreInspect = 0xe00,
    GateStep_RestoreReceiptSelected = 0xe01,
    GateStep_RestoreReceiptInspect = 0xe02,
    GateStep_RestoreReceiptHeader = 0xe03,
    GateStep_RestoreReceiptRollback = 0xe04,
    GateStep_RestorePublishWait = 0xe05,
    GateStep_RestoreArmCookie = 0xe06,
    GateStep_RestoreSendSignal = 0xe07,
    GateStep_RestoreCaptureBaseline = 0xe08,
    GateStep_RestorePoll = 0xe10,
    GateStep_RestoreReceiptSeen = 0x1010,
    GateStep_RestoreParentMoved = 0x1110,
    GateStep_RestoreTimeout = 0xe30,
};

template <MemoryWriter Writer>
class LoaderGate {
  public:
    using ProgressCallback = void (*)(void*, u32);

    LoaderGate(RetainedMemory<Writer>& memory, const LoaderGateConfig& config)
        : memory_(memory), config_(config) {}

    void setProgressCallback(ProgressCallback callback, void* context) {
        progress_callback_ = callback;
        progress_context_ = context;
    }

    Result park() {
        Result rc = inspectForPark();
        if (R_SUCCEEDED(rc)) rc = replaceWaitForPark();
        if (R_SUCCEEDED(rc)) rc = cancelWorkersForPark();
        if (R_SUCCEEDED(rc)) rc = emptyStepForPark();
        return rc;
    }

    Result inspectForPark() {
        LoaderSessionView view;
        R_TRY(inspect(view, false));
        R_UNLESS(idle(view), UnexpectedState);
        R_SUCCEED();
    }

    Result replaceWaitForPark() {
        // Publish responsibility before writing: a transport error does not
        // prove that the remote store did not happen.
        parked_ = true;
        return memory_.replace(config_.server + WaitOffset, &config_.receive,
                               &config_.park, sizeof(Handle));
    }

    Result cancelWorkersForPark() { return cancelWorkers(); }

    Result emptyStepForPark() {
        Result rc = step(nullptr, true);
        if (R_SUCCEEDED(rc)) {
            park_established_ = true;
        }
        return rc;
    }

    u64 waitAddress() const { return config_.server + WaitOffset; }

    Result pending(bool& ready) {
        s32 index;
        Result rc = svcWaitSynchronization(&index, &config_.server_copy, 1, 0);
        ready = R_SUCCEEDED(rc);
        return rc == KERNELRESULT(TimedOut) ? 0 : rc;
    }

    Result beginStep(bool empty = false) {
        // The session must be ours, parked, and not already mid-exchange.
        markStep(GateStep_InspectView);
        LoaderSessionView view;
        R_TRY(inspect(view, true));

        markStep(GateStep_RequireParked);
        R_UNLESS(parked_, UnexpectedState);
        R_UNLESS(!outstanding_, UnexpectedState);
        R_UNLESS(!step_active_, UnexpectedState);
        R_UNLESS(idle(view), UnexpectedState);

        bool ready;
        markStep(GateStep_CheckPending);
        R_TRY(pending(ready));
        R_UNLESS(ready != empty, UnexpectedState);

        // Replace the request header with the receipt marker so a landing
        // request is visible, keeping the original header for rollback.
        markStep(GateStep_ReadOriginalHeader);
        R_TRY(memory_.read(config_.request, &original_header_, sizeof(original_header_)));
        outstanding_ = true;
        injected_ = false;
        markStep(GateStep_MarkRequest);
        R_TRY(memory_.replace(config_.request, &original_header_, &ReceiptMarker,
                              sizeof(ReceiptMarker)));

        step_active_ = true;
        step_empty_ = empty;
        step_started_tick_ = armGetSystemTick();
        step_next_tick_ = 0;
        step_observations_ = 0;
        R_SUCCEED();
    }

    // One observation of the parked session, for HEN's resident loop. Returns
    // without blocking; the caller polls again after step_next_tick_. On
    // completion it sets complete and, for a real (non-empty) request, copies
    // the request message out.
    Result pollStep(void* request, bool& complete, bool allow_injection = true,
                    bool fail_after_injection = false) {
        complete = false;
        R_UNLESS(step_active_, UnexpectedState);
        const u64 now = armGetSystemTick();
        if (step_next_tick_ && now < step_next_tick_) R_SUCCEED();

        const u32 attempt = step_observations_++;

        LoaderSessionView view;
        bool selected;
        u64 epoch;
        u32 header;
        R_TRY(observe(attempt, view, selected, epoch, header));

        if (emptyReceiptSettled(view, step_empty_, selected, header, epoch)) {
            // The empty parking receipt landed on another waiter, so our
            // request slot is free again: roll the header back and finish.
            markStep(GateStep_ObserveEmptyComplete + attempt);
            const Result rc = memory_.replace(config_.request, &ReceiptMarker,
                                              &original_header_, sizeof(original_header_));
            if (R_SUCCEEDED(rc)) {
                finishStep();
                complete = true;
            }
            return rc;
        }

        if (requestArrived(view, header)) {
            // A real request has overwritten the marker: deliver it.
            markStep(GateStep_ObserveRequestReady + attempt);
            R_UNLESS(!step_empty_, UnexpectedState);
            markStep(GateStep_ObserveReadRequest + attempt);
            const Result rc = memory_.read(config_.request, request, MessageSize);
            if (R_SUCCEEDED(rc)) {
                finishStep();
                complete = true;
            }
            return rc;
        }

        R_TRY(advanceStep(attempt, view, selected, epoch, header, step_empty_,
                          allow_injection, fail_after_injection));

        step_last_action_ = diagnostic_step_;
        if (armTicksToNs(armGetSystemTick() - step_started_tick_) >= StepDeadlineNs) {
            markStep(GateStep_StepDeadline);
            return KERNELRESULT(TimedOut);
        }
        step_next_tick_ = armGetSystemTick() + armNsToTicks(StepPollNs);
        R_SUCCEED();
    }

    Result step(void* request, bool empty = false) {
        // The small mapped bootstrap uses this bounded synchronous step for
        // its empty parking receipt. Game requests use beginStep/pollStep on
        // HEN's resident loop so they are not limited by this observation count.
        markStep(GateStep_InspectView);
        LoaderSessionView view;
        R_TRY(inspect(view, true));

        markStep(GateStep_RequireParked);
        R_UNLESS(parked_, UnexpectedState);
        R_UNLESS(!outstanding_, UnexpectedState);
        R_UNLESS(idle(view), UnexpectedState);

        bool ready;
        markStep(GateStep_CheckPending);
        R_TRY(pending(ready));
        R_UNLESS(ready != empty, UnexpectedState);

        markStep(GateStep_ReadOriginalHeader);
        R_TRY(memory_.read(config_.request, &original_header_, sizeof(original_header_)));
        outstanding_ = true;
        injected_ = false;
        markStep(GateStep_MarkRequest);
        R_TRY(memory_.replace(config_.request, &original_header_, &ReceiptMarker,
                              sizeof(ReceiptMarker)));

        for (u32 attempt = 0; attempt < ObservationCount; attempt++) {
            bool selected;
            u64 epoch;
            u32 header;
            R_TRY(observe(attempt, view, selected, epoch, header));

            if (emptyReceiptSettled(view, empty, selected, header, epoch)) {
                markStep(GateStep_ObserveEmptyComplete + attempt);
                const Result rc = memory_.replace(config_.request, &ReceiptMarker,
                                                  &original_header_, sizeof(original_header_));
                if (R_SUCCEEDED(rc)) outstanding_ = false;
                return rc;
            }

            if (requestArrived(view, header)) {
                markStep(GateStep_ObserveRequestReady + attempt);
                outstanding_ = false;
                R_UNLESS(!empty, UnexpectedState);
                markStep(GateStep_ObserveReadRequest + attempt);
                return memory_.read(config_.request, request, MessageSize);
            }

            R_TRY(advanceStep(attempt, view, selected, epoch, header, empty, true, false));
        }
        return KERNELRESULT(TimedOut);
    }

    u32 diagnosticStep() const { return diagnostic_step_; }
    u32 failureStep() const {
        return diagnostic_step_ == GateStep_StepDeadline ? step_last_action_ : diagnostic_step_;
    }
    bool pendingChangedDuringEmptyStep() const {
        return diagnostic_step_ >= GateStep_PendingChanged &&
               diagnostic_step_ < GateStep_PendingChanged + MaxStepTraceObservations;
    }

    // Undoes park(): returns the session's wait handle to the original, wakes
    // the parked receive, and waits for the loader to resume. It runs in
    // phases guarded by member flags so a caller can retry it after a transient
    // failure, and so a second owner can adopt a receipt the bootstrap armed
    // (see adoptPendingReceipt).
    Result restore(u32 observation_count = ObservationCount) {
        if (!parked_ && !restoring_) R_SUCCEED();

        if (parked_) {
            LoaderSessionView view;
            R_TRY(restoreUnpark(view));
            // restoreUnpark sets restored_ only when the park write never
            // reached FS, in which case the rollback is already complete.
            if (restored_) R_SUCCEED();
        }

        R_TRY(restoreCaptureBaseline());
        R_TRY(restoreSendWake());
        return restorePoll(observation_count);
    }

    bool restored() const { return restored_; }

    bool pendingRestoreWithReceipt() const {
        return restoring_ && restore_signal_sent_ && restore_receipt_armed_ && !restored_;
    }

    void adoptPendingReceipt() {
        // The bootstrap already published original_wait and signalled the
        // exclusive park event. This owner may observe the same server's
        // receipt and clear that event; it must never rewrite the cookie.
        restoring_ = true;
        restore_signal_sent_ = true;
        restore_observer_armed_ = true;
        restore_receipt_armed_ = true;
    }

    LoaderGateProbe probe() {
        LoaderGateProbe result = {};
        LoaderSessionView view = {};
        result.read_result = memory_.read(config_.server, &view, sizeof(view));
        if (R_FAILED(result.read_result)) {
            return result;
        }
        result.parent = view.parent;
        result.wait = view.wait;
        result.receive = view.receive;
        result.state = view.state;
        result.receive_result = view.receive_result;
        // Sample all seven waiters in one read; this mask is diagnostic, not a
        // cleanup acknowledgement because their stores are not an atomic
        // snapshot.
        alignas(8) u8 waiters[6 * WaiterStride + 0x18] = {};
        result.read_result = memory_.read(config_.waiters[0] + SelectionOffset,
                                          waiters, sizeof(waiters));
        if (R_SUCCEEDED(result.read_result)) {
            for (u32 index = 0; index < 7; index++) {
                const u8* row = waiters + index * WaiterStride;
                u64 selected = 0;
                u64 epoch = 0;
                u32 active = 0;
                __builtin_memcpy(&selected, row, sizeof(selected));
                __builtin_memcpy(&epoch, row + 8, sizeof(epoch));
                __builtin_memcpy(&active, row + 0x14, sizeof(active));
                if (active) result.waiter_mask |= 1u << index;
                if (selected == config_.server) result.waiter_mask |= 1u << (index + 8);
                if (config_.waiters[index] == view.parent) {
                    result.selection = selected;
                    result.epoch = epoch;
                }
            }
        }
        s32 index = -1;
        result.event_result = svcWaitSynchronization(&index, &config_.event_read, 1, 0);
        result.server_result = svcWaitSynchronization(&index, &config_.server_copy, 1, 0);
        return result;
    }

  private:
    void markStep(u32 step) {
        diagnostic_step_ = step;
        if (progress_callback_) {
            progress_callback_(progress_context_, step);
        }
    }

    void finishStep() {
        outstanding_ = false;
        step_active_ = false;
    }

    // Reads the shared state one observe-loop iteration needs: whether the
    // server is a selected waiter, the current session view, the waiting
    // parent's epoch, and the request header.
    Result observe(u32 attempt, LoaderSessionView& view, bool& selected, u64& epoch,
                   u32& header) {
        markStep(GateStep_ObserveSelected + attempt);
        Result rc = isSelected(selected);
        if (R_SUCCEEDED(rc)) {
            markStep(GateStep_ObserveInspect + attempt);
            rc = inspect(view, true);
        }
        if (R_FAILED(rc)) return rc;

        epoch = 0;
        if (knownWaiter(view.parent)) {
            markStep(GateStep_ObserveEpoch + attempt);
            R_TRY(memory_.read(view.parent + EpochOffset, &epoch, sizeof(epoch)));
        }

        markStep(GateStep_ObserveHeader + attempt);
        R_TRY(memory_.read(config_.request, &header, sizeof(header)));
        R_SUCCEED();
    }

    // The observe-loop tail, once neither completion case applies: either the
    // session was disturbed (cancel the workers and look again) or it is idle
    // and still pending (inject our server as the selected waiter).
    Result advanceStep(u32 attempt, const LoaderSessionView& view, bool selected, u64 epoch,
                       u32 header, bool is_empty, bool allow_injection,
                       bool fail_after_injection) {
        if (sessionDisturbed(view, selected, header)) {
            markStep(GateStep_ObserveCancel + attempt);
            return cancelWorkers();
        }

        bool ready;
        markStep(GateStep_PendingRecheck + attempt);
        R_TRY(pending(ready));
        if (ready == is_empty) {
            markStep(GateStep_PendingChanged + attempt);
            return UnexpectedState;
        }
        if (!allow_injection) R_SUCCEED();

        markStep(GateStep_ObserveInject + attempt);
        R_TRY(inject(view.parent, epoch));
        if (fail_after_injection && injected_) {
            markStep(GateStep_FailAfterInjection);
            return KERNELRESULT(TimedOut);
        }
        R_SUCCEED();
    }

    static constexpr u64 WaitOffset = 0x30;
    static constexpr u64 ReceiveResultOffset = 0x44;
    static constexpr u64 SelectionOffset = 0x10;
    static constexpr u64 EpochOffset = 0x18;
    static constexpr u64 WaiterStride = 0x110;
    static constexpr u32 MessageSize = 0x100;
    static constexpr u32 ReceiptMarker = 0xffffffff;
    static constexpr u8 RestoreReceiptCookie = 0xff;
    static constexpr u32 EmptyReceive = 3;
    static constexpr u32 ObservationCount = 30;
    static constexpr u32 MaxStepTraceObservations = 0x100;
    static constexpr u64 StepPollNs = 25000000;
    static constexpr u64 StepDeadlineNs = 1000000000;
    static constexpr u64 RestorePollSleepNs = 25000000;
    static constexpr Result UnexpectedState = MAKERESULT(Module_Libnx, LibnxError_BadInput);

    bool knownWaiter(u64 address) const {
        for (u64 waiter : config_.waiters) {
            if (waiter == address) return true;
        }
        return false;
    }

    bool idle(const LoaderSessionView& view) const {
        return view.state == 1 && knownWaiter(view.parent);
    }

    // The server's identity fields still match the configuration: the session
    // we parked, not one the loader has recycled.
    bool sessionIdentityMatches(const LoaderSessionView& view) const {
        return view.vptr == config_.vptr && view.domain == config_.domain &&
               view.metadata == config_.metadata && view.receive == config_.receive &&
               view.request == config_.request && view.capacity == MessageSize;
    }

    // The empty parking receipt completed on a waiter other than the one we
    // injected, so our marked request slot can be rolled back.
    bool emptyReceiptSettled(const LoaderSessionView& view, bool is_empty, bool selected,
                             u32 header, u64 epoch) const {
        return is_empty && injected_ && !selected && header == ReceiptMarker && idle(view) &&
               view.receive_result == EmptyReceive &&
               (view.parent != injected_parent_ || epoch != injected_epoch_);
    }

    // A real request has replaced the receipt marker in an idle session.
    bool requestArrived(const LoaderSessionView& view, u32 header) const {
        return header != ReceiptMarker && idle(view);
    }

    // The session is not cleanly parked-and-pending: the marker is gone, the
    // server is already a selected waiter, or the session is not idle.
    bool sessionDisturbed(const LoaderSessionView& view, bool selected, u32 header) const {
        return header != ReceiptMarker || selected || !idle(view);
    }

    Result inspect(LoaderSessionView& view, bool parked) {
        R_TRY(inspectIdentity(view));
        R_UNLESS(view.wait == (parked ? config_.park : config_.original_wait), UnexpectedState);
        R_SUCCEED();
    }

    Result inspectIdentity(LoaderSessionView& view) {
        R_TRY(memory_.read(config_.server, &view, sizeof(view)));
        R_UNLESS(sessionIdentityMatches(view), UnexpectedState);
        R_SUCCEED();
    }

    Result isSelected(bool& selected) {
        alignas(8) u8 bytes[6 * WaiterStride + sizeof(u64)];
        R_TRY(memory_.read(config_.waiters[0] + SelectionOffset, bytes, sizeof(bytes)));
        selected = false;
        for (u32 i = 0; i < 7; i++) {
            u64 address;
            __builtin_memcpy(&address, bytes + i * WaiterStride, sizeof(address));
            if (address == config_.server) selected = true;
        }
        R_SUCCEED();
    }

    Result cancelWorkers() {
        for (Handle worker : config_.workers) {
            R_TRY(svcCancelSynchronization(worker));
        }
        R_SUCCEED();
    }

    Result inject(u64 parent, u64 expected_epoch) {
        struct { u64 selected; u64 epoch; } waiter;
        markStep(GateStep_InjectReadWaiter);
        Result rc = memory_.read(parent + SelectionOffset, &waiter, sizeof(waiter));
        if (R_FAILED(rc) || waiter.selected || waiter.epoch != expected_epoch) return rc;

        LoaderSessionView view;
        markStep(GateStep_InjectInspect);
        R_TRY(inspect(view, true));
        if (view.parent != parent || !idle(view)) R_SUCCEED();
        R_UNLESS(view.tag == 1, UnexpectedState);

        u32 header;
        markStep(GateStep_InjectReadHeader);
        rc = memory_.read(config_.request, &header, sizeof(header));
        if (R_FAILED(rc) || header != ReceiptMarker) return rc;

        // No FS request may intervene between selection and cancellation.
        markStep(GateStep_InjectSelect);
        const u64 empty_selection = 0;
        rc = memory_.replaceAndCancel(parent + SelectionOffset, &empty_selection,
                                      &config_.server, sizeof(config_.server),
                                      config_.workers, 7,
                                      [](void* context, u32 step) {
                                          static_cast<LoaderGate*>(context)->markStep(step);
                                      }, this);
        if (R_SUCCEEDED(rc)) {
            injected_ = true;
            injected_parent_ = parent;
            injected_epoch_ = waiter.epoch;
        }
        return rc;
    }

    // restore() phase 1: verify the session is still ours, roll back an
    // in-flight request receipt, arm the wake observer, and republish the
    // original wait handle. Sets restored_ (and returns 0) in the one case
    // where the park write never changed FS memory and no further work is due.
    Result restoreUnpark(LoaderSessionView& view) {
        markStep(GateStep_RestoreInspect);
        R_TRY(inspectIdentity(view));
        R_UNLESS(view.wait == config_.park || view.wait == config_.original_wait, UnexpectedState);

        if (view.wait == config_.original_wait && !restore_observer_armed_) {
            // The park write can fail before changing FS memory. No request was
            // started, so the original wait is already the complete rollback.
            R_UNLESS(!outstanding_, UnexpectedState);
            R_UNLESS(!step_active_, UnexpectedState);
            parked_ = false;
            restored_ = true;
            R_SUCCEED();
        }

        if (view.wait == config_.park) {
            R_UNLESS(idle(view), UnexpectedState);
        }
        if (outstanding_ && view.wait == config_.park) {
            R_TRY(restoreRollbackReceipt(view));
        }
        if (!restore_observer_armed_) {
            R_TRY(restoreArmObserver(view));
            restore_observer_armed_ = true;
        }

        markStep(GateStep_RestorePublishWait);
        if (view.wait == config_.park) {
            R_TRY(memory_.replace(config_.server + WaitOffset, &config_.park,
                                  &config_.original_wait, sizeof(Handle)));
        }
        parked_ = false;
        restoring_ = true;
        R_SUCCEED();
    }

    // Our request slot still holds the receipt marker from an outstanding
    // step: put the original header back so the loader sees no spurious request.
    Result restoreRollbackReceipt(LoaderSessionView& view) {
        bool selected;
        markStep(GateStep_RestoreReceiptSelected);
        R_TRY(isSelected(selected));
        R_UNLESS(!selected, UnexpectedState);

        markStep(GateStep_RestoreReceiptInspect);
        R_TRY(inspectIdentity(view));
        if (view.wait == config_.park) {
            R_UNLESS(idle(view), UnexpectedState);
        }

        u32 header;
        markStep(GateStep_RestoreReceiptHeader);
        R_TRY(memory_.read(config_.request, &header, sizeof(header)));
        if (header == ReceiptMarker) {
            markStep(GateStep_RestoreReceiptRollback);
            R_TRY(memory_.replace(config_.request, &ReceiptMarker, &original_header_,
                                  sizeof(original_header_)));
        }
        R_SUCCEED();
    }

    // When the empty park completed (park_established_), mark the server's
    // receive-result slot with a cookie so restorePoll can tell that the woken
    // receive has run. A park that never completed needs no cookie.
    Result restoreArmObserver(LoaderSessionView& view) {
        if (!park_established_) R_SUCCEED();

        bool selected;
        R_TRY(isSelected(selected));
        R_UNLESS(!selected, UnexpectedState);
        R_UNLESS(idle(view), UnexpectedState);
        R_UNLESS(view.tag == 1, UnexpectedState);

        // The woken receive must not have run yet, or the cookie would mask a
        // real result.
        s32 index;
        const Result rc = svcWaitSynchronization(&index, &config_.event_read, 1, 0);
        if (rc != KERNELRESULT(TimedOut)) {
            return R_FAILED(rc) ? rc : UnexpectedState;
        }

        const u8 previous = static_cast<u8>(view.receive_result);
        const u8 cookie = RestoreReceiptCookie;
        markStep(GateStep_RestoreArmCookie);
        R_TRY(memory_.replace(config_.server + ReceiveResultOffset, &previous,
                              &cookie, sizeof(cookie)));
        restore_receipt_armed_ = true;
        R_SUCCEED();
    }

    // restore() phase 2: record the waiting parent once, before the wake, so
    // phase 4 can tell that the loader moved on. Sampled after original_wait is
    // published so a pre-publication change cannot rebuild another park array.
    Result restoreCaptureBaseline() {
        if (restore_baseline_captured_) R_SUCCEED();

        LoaderSessionView baseline;
        markStep(GateStep_RestoreCaptureBaseline);
        R_TRY(inspect(baseline, false));
        if (knownWaiter(baseline.parent)) {
            restoring_parent_ = baseline.parent;
            restore_baseline_captured_ = true;
        }
        R_SUCCEED();
    }

    // restore() phase 3: wake the parked receive, once.
    Result restoreSendWake() {
        if (restore_signal_sent_) R_SUCCEED();

        markStep(GateStep_RestoreSendSignal);
        R_TRY(svcSignalEvent(config_.event_write));
        restore_signal_sent_ = true;
        R_SUCCEED();
    }

    // restore() phase 4: wait for the loader to resume, shown either by the
    // armed receipt cookie being overwritten with a real receive result or by
    // the waiting parent moving on from the baseline.
    Result restorePoll(u32 observation_count) {
        for (u32 attempt = 0; attempt < observation_count; attempt++) {
            LoaderSessionView current;
            markStep(GateStep_RestorePoll + attempt);
            R_TRY(inspect(current, false));

            const u8 receipt = static_cast<u8>(current.receive_result);
            const bool receipt_seen = restore_receipt_armed_ && receipt <= 3;
            const bool parent_moved = restore_baseline_captured_ &&
                                      knownWaiter(current.parent) &&
                                      current.parent != restoring_parent_;
            if (receipt_seen || parent_moved) {
                markStep((receipt_seen ? GateStep_RestoreReceiptSeen
                                       : GateStep_RestoreParentMoved) + attempt);
                const Result rc = svcClearEvent(config_.event_read);
                if (R_SUCCEEDED(rc)) {
                    restoring_ = false;
                    restored_ = true;
                    finishStep();
                }
                return rc;
            }

            if (attempt + 1 < observation_count) {
                svcSleepThread(RestorePollSleepNs);
            }
        }
        markStep(GateStep_RestoreTimeout);
        return KERNELRESULT(TimedOut);
    }

    RetainedMemory<Writer>& memory_;
    const LoaderGateConfig& config_;
    u32 original_header_ = 0;
    u64 injected_parent_ = 0;
    u64 injected_epoch_ = 0;
    bool parked_ = false;
    bool park_established_ = false;
    bool restoring_ = false;
    bool restore_signal_sent_ = false;
    bool restore_observer_armed_ = false;
    bool restore_receipt_armed_ = false;
    bool restore_baseline_captured_ = false;
    u64 restoring_parent_ = 0;
    bool outstanding_ = false;
    bool step_active_ = false;
    bool step_empty_ = false;
    u32 step_observations_ = 0;
    u32 step_last_action_ = 0;
    u64 step_started_tick_ = 0;
    u64 step_next_tick_ = 0;
    bool injected_ = false;
    bool restored_ = false;
    u32 diagnostic_step_ = 0;
    ProgressCallback progress_callback_ = nullptr;
    void* progress_context_ = nullptr;
};

} // namespace ounce
