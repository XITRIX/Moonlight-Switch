// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <switch.h>

// One SD file replacement owned by the PC upload connection. The caller
// supplies an already-initialized FS root; the writer never reinitializes it.
class SdWriter {
  public:
    ~SdWriter();

    Result begin(Handle root, const char* path, u64 size);
    Result write(const void* bytes, size_t size);
    Result finish();
    void close();

  private:
    FsFileSystem filesystem_ = {};
    FsFile file_ = {};
    u64 offset_ = 0;
    u64 expected_size_ = 0;
    bool filesystem_open_ = false;
    bool file_open_ = false;
};

class SdReader {
  public:
    ~SdReader();

    Result begin(Handle root, const char* path, u64* size);
    Result read(void* bytes, size_t capacity, u64* count);
    void close();

  private:
    FsFileSystem filesystem_ = {};
    FsFile file_ = {};
    u64 offset_ = 0;
    bool filesystem_open_ = false;
    bool file_open_ = false;
};
