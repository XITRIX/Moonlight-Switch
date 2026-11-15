// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <switch.h>

enum GameTracePoint : u32 {
    GameTracePoint_WorkerEntered = 1,
    GameTracePoint_MonitorReady = 2,
    GameTracePoint_MemoryReady = 3,
    GameTracePoint_CycleEntered = 4,
    GameTracePoint_PackageOpened = 5,
    GameTracePoint_GateParked = 6,
    GameTracePoint_ReceiveLoop = 7,
    GameTracePoint_WaitBridgeExit = 8,
    GameTracePoint_BridgeExited = 9,
    GameTracePoint_RequestReady = 10,
    GameTracePoint_FirstSnapshotDone = 11,
    GameTracePoint_BeforeStep = 12,
    GameTracePoint_SecondSnapshotDone = 13,
    GameTracePoint_RequestInspected = 14,
    GameTracePoint_AfterStep = 15,
    GameTracePoint_InputSample = 16,
    GameTracePoint_GateRecovery = 45,
    GameTracePoint_GateRestored = 46,
    GameTracePoint_SavedPolicyReady = 47,
    GameTracePoint_FirstLaunchCapture = 48,
    GameTracePoint_PolicyCaptureAttempt = 49,
    GameTracePoint_PolicyCaptured = 50,
    GameTracePoint_PolicyCaptureFailed = 51,
    GameTracePoint_Fault = 0xff,
};

struct GameTraceSnapshot {
    u32 sequence;
    u32 point;
    Result result;
    u32 value;
    u32 requests;
    u32 steps;
};
static_assert(sizeof(GameTraceSnapshot) == 24);

// The resident loop writes and reads these fields without an FS request.
class GameTrace {
  public:
    void write(u32 point, Result result = 0, u32 value = 0) {
        snapshot_.point = point;
        snapshot_.result = result;
        snapshot_.value = value;
        if (point == GameTracePoint_RequestReady) {
            snapshot_.requests++;
        } else if (point == GameTracePoint_BeforeStep) {
            snapshot_.steps++;
        }
        snapshot_.sequence++;
    }

    GameTraceSnapshot snapshot() const { return snapshot_; }

  private:
    GameTraceSnapshot snapshot_ = {};
};
