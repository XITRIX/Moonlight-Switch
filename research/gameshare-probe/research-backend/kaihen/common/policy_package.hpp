// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <switch.h>

struct SignedPolicy {
    static constexpr size_t Capacity = 0x2000;
    u8 bytes[Capacity] = {};
    u32 size = 0;
};

// Read the signed policy from an installed title through an initialized,
// borrowed fsp-ldr session. The caller keeps the root session open.
Result readSignedPolicy(Handle loader_root, u64 program_id, u8 storage_id,
                        u8 attributes, SignedPolicy* out);

// Read a signed policy saved on SD through an initialized FS root.
Result readSavedPolicy(Handle fs_root, const char* path, SignedPolicy* out);

// Save the policy separately so later HEN boots can rebuild the package from
// the current executable without another stock game launch.
Result saveSignedPolicy(Handle fs_root, const char* path, const SignedPolicy& policy);

// Build a flat ExeFS package with /main from SD and /main.npdm from policy.
Result writeExeFsPackage(Handle fs_root, const char* executable_path,
                         const char* package_path, const SignedPolicy& policy);
