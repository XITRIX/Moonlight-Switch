// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include "domain_mount.hpp"
extern "C" {
#include <switch/services/fs.h>
}

namespace ounce {

// The FS proxy session is borrowed from the retained bridge. Opening the
// package here gives Loader an FS-owned ExeFS view directly from SD.
template <MemoryWriter Writer>
class NativePackage {
  public:
    NativePackage(RetainedMemory<Writer>& memory, Handle fs_root) : memory_(memory) {
        serviceCreate(&root_, fs_root);
    }

    Result open(const char* path, DomainMountConfig& mount) {
        char package_path[FS_MAX_PATH] = {};
        __builtin_strncpy(package_path, path, sizeof(package_path) - 1);

        // The root was initialized before it was passed to bootstrap. Dispatch
        // on that same session so FS keeps its existing process identity.
        // Switch 2 FS command 10.
        const struct {
            u8 attributes;
            u32 filesystem_type;
            u64 program_id;
        } request = {FsContentAttributes_None, FsFileSystemType_ApplicationPackage, 0};

        Result rc = serviceDispatchIn(&root_, 10, request,
            .buffer_attrs = {SfBufferAttr_HipcPointer | SfBufferAttr_In},
            .buffers = {{package_path, sizeof(package_path)}},
            .out_num_objects = 1,
            .out_objects = &filesystem_.s);
        if (R_FAILED(rc)) return rc;
        opened_ = true;

        rc = serviceConvertToDomain(&filesystem_.s);
        if (R_FAILED(rc)) return rc;
        const u32 id = serviceGetObjectId(&filesystem_.s);
        if (!id) return MAKERESULT(Module_Libnx, LibnxError_BadInput);

        const u64 entry = mount.table + (static_cast<u64>(id) - 1) * 0x30;
        u64 object = 0, metadata = 0, vptr = 0;
        rc = memory_.read(entry + 0x20, &object, sizeof(object));
        if (R_SUCCEEDED(rc)) rc = memory_.read(entry + 0x28, &metadata, sizeof(metadata));
        if (R_SUCCEEDED(rc)) rc = memory_.read(object, &vptr, sizeof(vptr));
        if (R_FAILED(rc)) return rc;

        mount.donor_entry = entry;
        mount.donor_object = object;
        mount.donor_metadata = metadata;
        mount.donor_vptr = vptr;
        return 0;
    }

    void close() {
        if (opened_) {
            fsFsClose(&filesystem_);
            opened_ = false;
        }
        // root_ is borrowed; never close it here.
    }

  private:
    RetainedMemory<Writer>& memory_;
    Service root_ = {};
    FsFileSystem filesystem_ = {};
    bool opened_ = false;
};

} // namespace ounce
