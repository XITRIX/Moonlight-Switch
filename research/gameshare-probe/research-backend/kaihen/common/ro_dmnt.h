// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <switch/types.h>

// Private ro:dmnt commands, which kaiHEN uses to read and patch the FS
// process through RO's process-memory mapping capability. The caller copies
// the FS process handle in the request; RO checks that it is FS and that the
// range is FS code memory.
constexpr u32 RoProcessReadCommand = 0x524f0016;  ///< Out buffer: the bytes read, at most 4 MiB.
constexpr u32 RoProcessWriteCommand = 0x524f0017; ///< In buffer: the bytes to write, at most 256.

/// The input of RoProcessReadCommand and RoProcessWriteCommand.
struct RoProcessMemoryRequest {
    u64 process_id; ///< The FS process ID; it must match the copied handle.
    u64 address;
    u64 size;
    u64 reserved;
};
