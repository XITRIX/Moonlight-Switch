// SPDX-License-Identifier: GPL-2.0-only
#include <cstring>

#include "policy_package.hpp"
#include "sd_writer.hpp"

namespace {

constexpr Result InvalidPackage = MAKERESULT(Module_Libnx, LibnxError_BadInput);
constexpr size_t CopyChunk = 0x1000;

Result readPolicyFile(FsFileSystem* filesystem, SignedPolicy* out) {
    FsFile file = {};
    Result rc = fsFsOpenFile(filesystem, "/main.npdm", FsOpenMode_Read, &file);
    if (R_FAILED(rc)) return rc;

    s64 size = 0;
    rc = fsFileGetSize(&file, &size);
    if (R_SUCCEEDED(rc) && (size <= 0 || size > static_cast<s64>(SignedPolicy::Capacity))) {
        rc = InvalidPackage;
    }
    if (R_SUCCEEDED(rc)) {
        u64 read = 0;
        rc = fsFileRead(&file, 0, out->bytes, size, FsReadOption_None, &read);
        if (R_SUCCEEDED(rc) && read != static_cast<u64>(size)) rc = InvalidPackage;
        if (R_SUCCEEDED(rc)) out->size = static_cast<u32>(size);
    }
    fsFileClose(&file);
    return rc;
}

u64 align32(u64 value) {
    return (value + 0x1f) & ~u64(0x1f);
}

} // namespace

Result readSignedPolicy(Handle loader_root, u64 program_id, u8 storage_id,
                        u8 attributes, SignedPolicy* out) {
    if (!out || loader_root == INVALID_HANDLE) return InvalidPackage;
    *out = {};

    Service loader = {};
    serviceCreate(&loader, loader_root);
    loader.own_handle = false;
    const struct {
        u8 attributes;
        u8 storage_id;
        u64 program_id;
    } request = {attributes, storage_id, program_id};
    u8 code_info[0x1a4] = {};
    FsFileSystem filesystem = {};
    Result rc = serviceDispatchIn(&loader, 0, request,
        .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_Out},
        .buffers = {{code_info, sizeof(code_info)}},
        .out_num_objects = 1,
        .out_objects = &filesystem.s,
    );
    if (R_FAILED(rc)) return rc;
    rc = readPolicyFile(&filesystem, out);
    fsFsClose(&filesystem);
    return rc;
}

Result readSavedPolicy(Handle fs_root, const char* path, SignedPolicy* out) {
    if (!out) return InvalidPackage;
    *out = {};
    SdReader reader;
    u64 size = 0;
    Result rc = reader.begin(fs_root, path, &size);
    if (R_FAILED(rc)) return rc;
    if (!size || size > SignedPolicy::Capacity) return InvalidPackage;

    u64 read = 0;
    rc = reader.read(out->bytes, static_cast<size_t>(size), &read);
    if (R_SUCCEEDED(rc) && read != size) rc = InvalidPackage;
    if (R_SUCCEEDED(rc)) out->size = static_cast<u32>(size);
    return rc;
}

Result saveSignedPolicy(Handle fs_root, const char* path, const SignedPolicy& policy) {
    if (!policy.size || policy.size > SignedPolicy::Capacity) return InvalidPackage;
    SdWriter writer;
    Result rc = writer.begin(fs_root, path, policy.size);
    if (R_SUCCEEDED(rc)) rc = writer.write(policy.bytes, policy.size);
    if (R_SUCCEEDED(rc)) rc = writer.finish();
    return rc;
}

Result writeExeFsPackage(Handle fs_root, const char* executable_path,
                         const char* package_path, const SignedPolicy& policy) {
    if (!policy.size || policy.size > SignedPolicy::Capacity) return InvalidPackage;
    SdReader reader;
    u64 main_size = 0;
    Result rc = reader.begin(fs_root, executable_path, &main_size);
    if (R_FAILED(rc)) return rc;

    constexpr u64 HeaderSize = 16 + 2 * 24 + sizeof("main") + sizeof("main.npdm");
    const u64 policy_offset = align32(main_size);
    const u64 package_size = HeaderSize + policy_offset + policy.size;
    SdWriter writer;
    rc = writer.begin(fs_root, package_path, package_size);
    if (R_FAILED(rc)) return rc;

    u8 header[HeaderSize] = {};
    std::memcpy(header, "PFS0", 4);
    const u32 count = 2;
    const u32 names_size = sizeof("main") + sizeof("main.npdm");
    const u32 policy_name_offset = sizeof("main");
    std::memcpy(header + 4, &count, sizeof(count));
    std::memcpy(header + 8, &names_size, sizeof(names_size));
    std::memcpy(header + 16 + 8, &main_size, sizeof(main_size));
    std::memcpy(header + 40, &policy_offset, sizeof(policy_offset));
    const u64 policy_size = policy.size;
    std::memcpy(header + 48, &policy_size, sizeof(policy_size));
    std::memcpy(header + 56, &policy_name_offset, sizeof(policy_name_offset));
    std::memcpy(header + 64, "main\0main.npdm\0", names_size);
    rc = writer.write(header, sizeof(header));

    u8 buffer[CopyChunk];
    u64 remaining = main_size;
    while (R_SUCCEEDED(rc) && remaining) {
        const size_t wanted = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
        u64 received = 0;
        rc = reader.read(buffer, wanted, &received);
        if (R_SUCCEEDED(rc) && received != wanted) rc = InvalidPackage;
        if (R_SUCCEEDED(rc)) rc = writer.write(buffer, wanted);
        remaining -= received;
    }
    reader.close();

    if (R_SUCCEEDED(rc) && policy_offset > main_size) {
        const u8 padding[0x20] = {};
        rc = writer.write(padding, policy_offset - main_size);
    }
    if (R_SUCCEEDED(rc)) rc = writer.write(policy.bytes, policy.size);
    if (R_SUCCEEDED(rc)) rc = writer.finish();
    return rc;
}
