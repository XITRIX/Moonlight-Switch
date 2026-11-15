// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "retained_memory.hpp"
#include "code_mount_request.hpp"

namespace ounce {

struct DomainMountConfig {
    u64 table;
    u64 domain;
    u64 root_id;
    u64 donor_entry;
    u64 donor_object;
    u64 donor_metadata;
    u64 donor_vptr;
    u64 title;
};

struct DomainEntry {
    u64 address;
    u64 object;
    u64 metadata;
};

/// The most objects a domain mount snapshot lists.
constexpr u32 MaxDomainEntries = 128;

template <MemoryWriter Writer>
class DomainMount {
  public:
    DomainMount(RetainedMemory<Writer>& memory, const DomainMountConfig& config)
        : memory_(memory), config_(config) {}

    /// Whether the request opens the code of config.title from built-in
    /// system storage, the one mount the bootstrap expects.
    Result inspectRequest(const u8 request[LoaderRequestSize], bool& mount) {
        mount = false;
        CodeMountRequest decoded;
        Result rc = decodeCodeMountRequest(request, config_.root_id, decoded);
        if (R_FAILED(rc) || !decoded.open_code) {
            return rc;
        }
        const CodeMountArguments& arguments = decoded.arguments;
        if (arguments.content_attributes != 1 ||
            arguments.storage_id != NcmStorageId_BuiltInSystem ||
            arguments.program_id != config_.title) {
            return UnexpectedState;
        }
        mount = true;
        return 0;
    }

    Result snapshot(DomainEntry entries[MaxDomainEntries], u32& count) {
        count = 0;
        u64 next;
        Result rc = memory_.read(config_.domain + 0x20, &next, sizeof(next));
        while (R_SUCCEEDED(rc) && next != config_.domain + 0x20) {
            if (count == MaxDomainEntries || next < config_.table ||
                (next - config_.table) % 0x30) return UnexpectedState;
            u8 raw[0x30];
            rc = memory_.read(next, raw, sizeof(raw));
            if (R_FAILED(rc)) break;
            u64 owner;
            __builtin_memcpy(&owner, raw + 0x18, sizeof(owner));
            if (owner != config_.domain) return UnexpectedState;
            for (u32 i = 0; i < count; i++) {
                if (entries[i].address == next) return UnexpectedState;
            }
            entries[count].address = next;
            __builtin_memcpy(&entries[count].object, raw + 0x20, 8);
            __builtin_memcpy(&entries[count].metadata, raw + 0x28, 8);
            count++;
            __builtin_memcpy(&next, raw, sizeof(next));
        }
        return rc;
    }

    Result added(const DomainEntry before[MaxDomainEntries], u32 before_count,
                 const DomainEntry after[MaxDomainEntries], u32 after_count, DomainEntry& entry) {
        u32 found = 0;
        for (u32 i = 0; i < after_count; i++) {
            bool existed = false;
            for (u32 j = 0; j < before_count; j++) {
                if (after[i].address == before[j].address &&
                    after[i].object == before[j].object) existed = true;
            }
            if (!existed) {
                found++;
                entry = after[i];
            }
        }
        return found == 1 ? 0 : UnexpectedState;
    }

    Result exchange(const DomainEntry& code) {
        if (code.metadata != config_.donor_metadata ||
            code.address == config_.donor_entry || code.object == config_.donor_object)
            return UnexpectedState;
        u64 code_vptr, donor_vptr, current_donor, current_code;
        Result rc = memory_.read(code.object, &code_vptr, 8);
        if (R_SUCCEEDED(rc)) rc = memory_.read(config_.donor_object, &donor_vptr, 8);
        if (R_SUCCEEDED(rc)) rc = memory_.read(config_.donor_entry + 0x20, &current_donor, 8);
        if (R_SUCCEEDED(rc)) rc = memory_.read(code.address + 0x20, &current_code, 8);
        if (R_FAILED(rc)) return rc;
        if (code_vptr != config_.donor_vptr || donor_vptr != config_.donor_vptr ||
            current_donor != config_.donor_object || current_code != code.object)
            return UnexpectedState;
        rc = memory_.write(code.address + 0x20, &config_.donor_object, 8);
        if (R_FAILED(rc)) return rc;
        rc = memory_.write(config_.donor_entry + 0x20, &code.object, 8);
        if (R_FAILED(rc)) {
            memory_.write(code.address + 0x20, &code.object, 8);
            return rc;
        }
        u64 first, second;
        rc = memory_.read(code.address + 0x20, &first, 8);
        if (R_SUCCEEDED(rc)) rc = memory_.read(config_.donor_entry + 0x20, &second, 8);
        if (R_FAILED(rc)) return rc;
        return first == config_.donor_object && second == code.object ? 0 : UnexpectedState;
    }

  private:
    static constexpr Result UnexpectedState = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    RetainedMemory<Writer>& memory_;
    const DomainMountConfig& config_;
};

} // namespace ounce
