// SPDX-License-Identifier: GPL-2.0-only
#include <cstring>
#include "sd_writer.hpp"

namespace {

constexpr Result InvalidPath = MAKERESULT(Module_Libnx, LibnxError_BadInput);

/// IFileSystemProxy command 18, OpenSdCardFileSystem. HEN opens the SD card
/// on its own copy of the FS root, not on libnx's FS session, so it sends the
/// command itself.
constexpr u32 FsProxyCommand_OpenSdCardFileSystem = 18;

/// An absolute path on the SD card that fits FS's path buffer.
bool isSdFilePath(const char* path) {
    return path && path[0] == '/' && path[1] != '\0' && std::strlen(path) < FS_MAX_PATH;
}

Result openSdCardFileSystem(Handle root, FsFileSystem* out) {
    Service service = {};
    serviceCreate(&service, root);
    service.own_handle = false;
    return serviceDispatch(&service, FsProxyCommand_OpenSdCardFileSystem,
        .out_num_objects = 1,
        .out_objects = &out->s,
    );
}

Result createParents(FsFileSystem* filesystem, const char* path) {
    char parent[FS_MAX_PATH] = {};
    std::strncpy(parent, path, sizeof(parent) - 1);

    for (char* separator = parent + 1; *separator; separator++) {
        if (*separator != '/') {
            continue;
        }
        *separator = '\0';
        FsDirEntryType type = FsDirEntryType_File;
        Result rc = fsFsGetEntryType(filesystem, parent, &type);
        if (R_SUCCEEDED(rc)) {
            if (type != FsDirEntryType_Dir) {
                return InvalidPath;
            }
        } else {
            rc = fsFsCreateDirectory(filesystem, parent);
            if (R_FAILED(rc)) {
                return rc;
            }
        }
        *separator = '/';
    }
    return 0;
}

} // namespace

SdWriter::~SdWriter() {
    close();
}

Result SdWriter::begin(Handle root, const char* path, u64 size) {
    close();
    if (root == INVALID_HANDLE || !isSdFilePath(path) || size > INT64_MAX) {
        return InvalidPath;
    }

    Result rc = openSdCardFileSystem(root, &filesystem_);
    if (R_FAILED(rc)) {
        return rc;
    }
    filesystem_open_ = true;

    rc = createParents(&filesystem_, path);
    if (R_FAILED(rc)) {
        close();
        return rc;
    }

    FsDirEntryType type = FsDirEntryType_File;
    rc = fsFsGetEntryType(&filesystem_, path, &type);
    if (R_SUCCEEDED(rc)) {
        if (type != FsDirEntryType_File) {
            close();
            return InvalidPath;
        }
        rc = fsFsDeleteFile(&filesystem_, path);
        if (R_FAILED(rc)) {
            close();
            return rc;
        }
    }

    rc = fsFsCreateFile(&filesystem_, path, static_cast<s64>(size), 0);
    if (R_SUCCEEDED(rc)) {
        rc = fsFsOpenFile(&filesystem_, path, FsOpenMode_Write, &file_);
    }
    if (R_FAILED(rc)) {
        close();
        return rc;
    }
    file_open_ = true;
    expected_size_ = size;
    return 0;
}

Result SdWriter::write(const void* bytes, size_t size) {
    if (!file_open_ || size > expected_size_ - offset_) {
        return InvalidPath;
    }
    Result rc = fsFileWrite(&file_, static_cast<s64>(offset_), bytes, size,
                            FsWriteOption_None);
    if (R_SUCCEEDED(rc)) {
        offset_ += size;
    }
    return rc;
}

Result SdWriter::finish() {
    if (!file_open_ || offset_ != expected_size_) {
        return InvalidPath;
    }
    Result rc = fsFileFlush(&file_);
    fsFileClose(&file_);
    file_open_ = false;
    if (R_SUCCEEDED(rc)) {
        rc = fsFsCommit(&filesystem_);
    }
    close();
    return rc;
}

void SdWriter::close() {
    if (file_open_) {
        fsFileClose(&file_);
        file_open_ = false;
    }
    if (filesystem_open_) {
        fsFsClose(&filesystem_);
        filesystem_open_ = false;
    }
    offset_ = 0;
    expected_size_ = 0;
}

SdReader::~SdReader() {
    close();
}

Result SdReader::begin(Handle root, const char* path, u64* size) {
    close();
    if (root == INVALID_HANDLE || !isSdFilePath(path) || !size) {
        return InvalidPath;
    }

    Result rc = openSdCardFileSystem(root, &filesystem_);
    if (R_FAILED(rc)) {
        return rc;
    }
    filesystem_open_ = true;

    rc = fsFsOpenFile(&filesystem_, path, FsOpenMode_Read, &file_);
    if (R_FAILED(rc)) {
        close();
        return rc;
    }
    file_open_ = true;

    s64 signed_size = 0;
    rc = fsFileGetSize(&file_, &signed_size);
    if (R_FAILED(rc) || signed_size < 0) {
        close();
        return R_FAILED(rc) ? rc : InvalidPath;
    }
    *size = static_cast<u64>(signed_size);
    return 0;
}

Result SdReader::read(void* bytes, size_t capacity, u64* count) {
    if (!file_open_ || !bytes || !count) {
        return InvalidPath;
    }
    Result rc = fsFileRead(&file_, static_cast<s64>(offset_), bytes, capacity,
                           FsReadOption_None, count);
    if (R_SUCCEEDED(rc)) {
        offset_ += *count;
    }
    return rc;
}

void SdReader::close() {
    if (file_open_) {
        fsFileClose(&file_);
        file_open_ = false;
    }
    if (filesystem_open_) {
        fsFsClose(&filesystem_);
        filesystem_open_ = false;
    }
    offset_ = 0;
}
