// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <switch.h>

// Private bootstrap-to-HEN protocol. Kernel handles are sent in two COPY
// batches and replace the process-local handle values in this configuration.
struct HenGameConfig {
    s64 data_offset;
    s64 pointer_offset;
    u64 server;
    u64 vptr;
    u64 domain;
    u64 metadata;
    u64 request;
    u64 waiters[7];
    u64 table;
    u64 loader_root_id;
    // Handle values in FS's process. These are memory coordinates, not local
    // kernel handles and must not be sent as HIPC COPY handles.
    u32 receive;
    u32 original_wait;
    u32 park;
    // Private bootstrap handoff: 1 means original_wait was published and
    // its receipt cookie/event are still owned by HEN after startup.
    u32 bootstrap_restore_pending;
};
static_assert(sizeof(HenGameConfig) == 144);

enum HenGamePhase : u32 {
    HenGamePhase_Unconfigured,
    HenGamePhase_OpeningPackage,
    HenGamePhase_Parking,
    HenGamePhase_WaitingForGame,
    HenGamePhase_WaitingForExit,
    HenGamePhase_Fault,
    HenGamePhase_WaitingForBridgeExit,
    HenGamePhase_AwaitingDiagnosticArm,
    HenGamePhase_RecoveringGate,
    HenGamePhase_WaitingForProcess,
    HenGamePhase_DiagnosticDone,
};

struct HenGameStatus {
    HenGamePhase phase;
    Result result;
    u32 serial;
    u32 mounts;
    u64 process_id;
};
static_assert(sizeof(HenGameStatus) == 24);
