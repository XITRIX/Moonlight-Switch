// SPDX-License-Identifier: GPL-2.0-only
#pragma once
extern "C" {
#include <switch.h>
#include <switch/services/fs.h>
}
#include "ro_dmnt.h"
#include "result.hpp"

namespace ounce {

struct RetainedMemoryConfig {
    Handle storage;
    s64 data_offset;
    s64 pointer_offset;
};

enum class MemoryWriter {
    RetainedStorage,
    RoProcess,
};

template <MemoryWriter Writer>
class RetainedMemory {
  public:
    RetainedMemory(const RetainedMemoryConfig& config, Handle reader_session,
                   Handle filesystem_process)
        : config_(config), filesystem_process_(filesystem_process) {
        serviceCreate(&storage_.s, config.storage);
        serviceCreate(&reader_, reader_session);
    }

    Result begin() {
        Result rc = svcGetProcessId(&filesystem_pid_, filesystem_process_);
        if (R_SUCCEEDED(rc)) {
            rc = fsStorageRead(&storage_, config_.pointer_offset, &original_target_,
                               sizeof(original_target_));
        }
        return rc;
    }

    Result read(u64 address, void* output, u64 size) {
        const struct {
            u64 process_id;
            u64 address;
            u64 size;
            u64 reserved;
        } request = {filesystem_pid_, address, size, 0};
        return serviceDispatchIn(&reader_, RoProcessReadCommand, request,
            .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_Out |
                             SfBufferAttr_HipcMapTransferAllowsNonSecure},
            .buffers = {{output, size}},
            .in_num_handles = 1,
            .in_handles = {filesystem_process_},
        );
    }

    Result write(u64 address, const void* input, u64 size) {
        if constexpr (Writer == MemoryWriter::RoProcess) {
            return writeThroughReader(address, input, size);
        } else {
            Result rc = select(address);
            if (R_SUCCEEDED(rc)) {
                rc = fsStorageWrite(&storage_, config_.data_offset, input, size);
            }
            return rc;
        }
    }

    // The Loader gate changes known FS bytes. Match the working resident
    // writer's preimage check and stable-write readback before returning.
    Result replace(u64 address, const void* expected, const void* replacement, u64 size) {
        if constexpr (Writer == MemoryWriter::RoProcess) {
            return replaceViaReader(address, expected, replacement, size);
        } else {
            (void)expected;
            return write(address, replacement, size);
        }
    }

    Result replaceAndCancel(u64 address, const void* expected, const void* replacement,
                            u64 size, const Handle* workers, u32 worker_count,
                            void (*progress)(void*, u32) = nullptr, void* progress_context = nullptr) {
        if constexpr (Writer == MemoryWriter::RetainedStorage) {
            (void)expected;
            (void)progress;
            (void)progress_context;
            R_TRY(write(address, replacement, size));
            for (u32 index = 0; index < worker_count; index++) {
                R_TRY(svcCancelSynchronization(workers[index]));
            }
            return 0;
        } else {
            R_UNLESS(expected && replacement && size && size <= 256 &&
                         (!worker_count || workers),
                     MAKERESULT(Module_Libnx, LibnxError_BadInput));

            // Verify the preimage, then write the replacement.
            u8 observed[256];
            R_TRY(read(address, observed, size));
            R_UNLESS(__builtin_memcmp(observed, expected, size) == 0,
                     MAKERESULT(Module_Libnx, LibnxError_BadInput));
            Result rc = writeThroughReader(address, replacement, size);
            if (progress) {
                progress(progress_context, 0xb41);
            }

            // Cancel the workers even if the write failed, keeping the first
            // error. Report it after the cancellation round.
            for (u32 index = 0; index < worker_count; index++) {
                Result cancel_result = svcCancelSynchronization(workers[index]);
                if (R_SUCCEEDED(rc) && R_FAILED(cancel_result)) {
                    rc = cancel_result;
                }
            }
            if (progress) {
                progress(progress_context, 0xb42);
            }
            R_TRY(rc);

            // The Loader workers may consume and replace the selected waiter as
            // soon as cancellation returns. The working resident checks the
            // preimage, writes, cancels, then observes the waiter epoch instead
            // of requiring this transient field to still hold our value.
            if (worker_count) {
                R_SUCCEED();
            }

            rc = read(address, observed, size);
            if (progress) {
                progress(progress_context, 0xb43);
            }
            R_UNLESS(R_FAILED(rc) || __builtin_memcmp(observed, replacement, size) == 0,
                     MAKERESULT(Module_Libnx, LibnxError_BadInput));
            return rc;
        }
    }

    // No storage handle is closed here. RO-backed game writes do not retarget
    // the retained-storage pointer; bootstrap restores the original target.
    Result restore() {
        if constexpr (Writer == MemoryWriter::RoProcess) {
            return 0;
        } else {
            return select(original_target_);
        }
    }

    Result replaceViaReader(u64 address, const void* expected, const void* replacement,
                            u64 size) {
        R_UNLESS(expected && replacement && size && size <= 256,
                 MAKERESULT(Module_Libnx, LibnxError_BadInput));

        u8 observed[256];
        R_TRY(read(address, observed, size));
        R_UNLESS(__builtin_memcmp(observed, expected, size) == 0,
                 MAKERESULT(Module_Libnx, LibnxError_BadInput));

        R_TRY(writeThroughReader(address, replacement, size));
        Result rc = read(address, observed, size);
        if (R_SUCCEEDED(rc) && __builtin_memcmp(observed, replacement, size)) {
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        }
        return rc;
    }

  private:
    Result writeThroughReader(u64 address, const void* input, u64 size) {
        const struct {
            u64 process_id;
            u64 address;
            u64 size;
            u64 reserved;
        } request = {filesystem_pid_, address, size, 0};
        return serviceDispatchIn(&reader_, RoProcessWriteCommand, request,
            .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_In |
                             SfBufferAttr_HipcMapTransferAllowsNonSecure},
            .buffers = {{input, size}},
            .in_num_handles = 1,
            .in_handles = {filesystem_process_},
        );
    }

    Result select(u64 address) {
        return fsStorageWrite(&storage_, config_.pointer_offset, &address, sizeof(address));
    }

    RetainedMemoryConfig config_;
    Handle filesystem_process_ = INVALID_HANDLE;
    FsStorage storage_ = {};
    Service reader_ = {};
    u64 filesystem_pid_ = 0;
    u64 original_target_ = 0;
};

} // namespace ounce
