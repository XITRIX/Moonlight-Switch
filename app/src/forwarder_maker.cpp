//
//  forwarder_maker.cpp
//  Moonlight
//
//  Adapted from sphaira's forwarder builder/installer flow.
//

#include <forwarder_maker.hpp>
#include <Settings.hpp>

#include "BoxArtManager.hpp"
#include "Data.hpp"

#ifdef __SWITCH__

#include <switch.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Warray-bounds"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#include "../../extern/borealis/library/include/borealis/extern/nanovg/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#define STBI_WRITE_NO_STDIO
#include "../../extern/borealis/library/lib/extern/glfw/deps/stb_image_write.h"
#pragma GCC diagnostic pop

namespace {

std::vector<u8> readFileBytes(const std::string& path);

constexpr u32 IVFC_MAX_LEVEL = 6;
constexpr u32 IVFC_HASH_BLOCK_SIZE = 0x4000;
constexpr u32 PFS0_EXEFS_HASH_BLOCK_SIZE = 0x10000;
constexpr u32 PFS0_LOGO_HASH_BLOCK_SIZE = 0x1000;
constexpr u32 PFS0_META_HASH_BLOCK_SIZE = 0x1000;
constexpr u32 PFS0_PADDING_SIZE = 0x200;
constexpr u32 ROMFS_ENTRY_EMPTY = 0xFFFFFFFF;
constexpr u32 ROMFS_FILEPARTITION_OFS = 0x200;

constexpr u32 NCA0_MAGIC = 0x3041434E;
constexpr u32 NCA2_MAGIC = 0x3241434E;
constexpr u32 NCA3_MAGIC = 0x3341434E;
constexpr u32 NCA_SECTION_TOTAL = 0x4;

constexpr std::array<u8, 0x10> HEADER_KEK_SRC = {
    0x1F, 0x12, 0x91, 0x3A, 0x4A, 0xCB, 0xF0, 0x0D,
    0x4C, 0xDE, 0x3A, 0xF6, 0xD5, 0x23, 0x88, 0x2A,
};

constexpr std::array<u8, 0x20> HEADER_KEY_SRC = {
    0x5A, 0x3E, 0xD8, 0x4F, 0xDE, 0xC0, 0xD8, 0x26,
    0x31, 0xF7, 0xE2, 0x5D, 0x19, 0x7B, 0xF5, 0xD0,
    0x1C, 0x9B, 0x7B, 0xFA, 0xF6, 0x28, 0x18, 0x3D,
    0x71, 0xF6, 0x4D, 0x73, 0xF1, 0x50, 0xB9, 0xD2,
};

struct Keys {
    std::array<u8, 0x20> header_key{};
};

namespace npdm {

struct Meta {
    u32 magic;
    u32 signature_key_generation;
    u32 _0x8;
    u8 flags;
    u8 _0xD;
    u8 main_thread_priority;
    u8 main_thread_core_num;
    u32 _0x10;
    u32 sys_resource_size;
    u32 version;
    u32 main_thread_stack_size;
    char title_name[0x10];
    char product_code[0x10];
    u8 _0x40[0x30];
    u32 aci0_offset;
    u32 aci0_size;
    u32 acid_offset;
    u32 acid_size;
};

struct Acid {
    u8 rsa_sig[0x100];
    u8 rsa_pub[0x100];
    u32 magic;
    u32 size;
    u8 version;
    u8 _0x209[0x1];
    u8 _0x20A[0x2];
    u32 flags;
    u64 program_id_min;
    u64 program_id_max;
    u32 fac_offset;
    u32 fac_size;
    u32 sac_offset;
    u32 sac_size;
    u32 kac_offset;
    u32 kac_size;
    u8 _0x238[0x8];
};

struct Aci0 {
    u32 magic;
    u8 _0x4[0xC];
    u64 program_id;
    u8 _0x18[0x8];
    u32 fac_offset;
    u32 fac_size;
    u32 sac_offset;
    u32 sac_size;
    u32 kac_offset;
    u32 kac_size;
    u8 _0x38[0x8];
};

} // namespace npdm

namespace nca {

enum DistributionType {
    DistributionType_System = 0x0,
};

enum ContentType {
    ContentType_Program = 0x0,
    ContentType_Meta = 0x1,
    ContentType_Control = 0x2,
};

enum FileSystemType {
    FileSystemType_RomFS = 0x0,
    FileSystemType_PFS0 = 0x1,
};

enum HashType {
    HashType_HierarchicalSha256 = 0x2,
    HashType_HierarchicalIntegrity = 0x3,
};

enum EncryptionType {
    EncryptionType_None = 0x1,
};

struct SectionTableEntry {
    u32 media_start_offset;
    u32 media_end_offset;
    u8 _0x8[0x4];
    u8 _0xC[0x4];
};

struct LayerRegion {
    u64 offset;
    u64 size;
};

struct HierarchicalSha256Data {
    u8 master_hash[0x20];
    u32 block_size;
    u32 layer_count;
    LayerRegion hash_layer;
    LayerRegion pfs0_layer;
    LayerRegion unused_layers[3];
    u8 _0x78[0x80];
};
static_assert(sizeof(HierarchicalSha256Data) == 0xF8);

#pragma pack(push, 1)
struct HierarchicalIntegrityVerificationLevelInformation {
    u64 logical_offset;
    u64 hash_data_size;
    u32 block_size;
    u32 _0x14;
};
#pragma pack(pop)

struct InfoLevelHash {
    u32 max_layers;
    HierarchicalIntegrityVerificationLevelInformation levels[6];
    u8 signature_salt[0x20];
};

struct IntegrityMetaInfo {
    u32 magic;
    u32 version;
    u32 master_hash_size;
    InfoLevelHash info_level_hash;
    u8 master_hash[0x20];
    u8 _0xE0[0x18];
};
static_assert(sizeof(IntegrityMetaInfo) == 0xF8);

struct PatchInfo {
    u8 data[0x40];
};
static_assert(sizeof(PatchInfo) == 0x40);

struct CompressionInfo {
    u8 data[0x28];
};
static_assert(sizeof(CompressionInfo) == 0x28);

struct FsHeader {
    u16 version;
    u8 fs_type;
    u8 hash_type;
    u8 encryption_type;
    u8 metadata_hash_type;
    u8 _0x6[0x2];
    union {
        HierarchicalSha256Data hierarchical_sha256_data;
        IntegrityMetaInfo integrity_meta_info;
    } hash_data;
    PatchInfo patch_info;
    u64 section_ctr;
    u8 spares_info[0x30];
    CompressionInfo compression_info;
    u8 meta_data_hash_data_info[0x30];
    u8 reserved[0x30];
};
static_assert(sizeof(FsHeader) == 0x200);

struct SectionHeaderHash {
    u8 sha256[0x20];
};

struct KeyArea {
    u8 area[0x10];
};

struct Header {
    u8 rsa_fixed_key[0x100];
    u8 rsa_npdm[0x100];
    u32 magic;
    u8 distribution_type;
    u8 content_type;
    u8 old_key_gen;
    u8 kaek_index;
    u64 size;
    u64 program_id;
    u32 context_id;
    union {
        u32 sdk_version;
        struct {
            u8 sdk_revision;
            u8 sdk_micro;
            u8 sdk_minor;
            u8 sdk_major;
        };
    };
    u8 key_gen;
    u8 sig_key_gen;
    u8 _0x222[0xE];
    FsRightsId rights_id;
    SectionTableEntry fs_table[NCA_SECTION_TOTAL];
    SectionHeaderHash fs_header_hash[NCA_SECTION_TOTAL];
    KeyArea key_area[NCA_SECTION_TOTAL];
    u8 _0x340[0xC0];
    FsHeader fs_header[NCA_SECTION_TOTAL];
};
static_assert(sizeof(Header) == 0xC00);

} // namespace nca

#pragma pack(push, 1)
struct RomfsHeader {
    u64 headerSize;
    u64 dirHashTableOff;
    u64 dirHashTableSize;
    u64 dirTableOff;
    u64 dirTableSize;
    u64 fileHashTableOff;
    u64 fileHashTableSize;
    u64 fileTableOff;
    u64 fileTableSize;
    u64 fileDataOff;
};

struct RomfsDirEntry {
    u32 parent;
    u32 sibling;
    u32 childDir;
    u32 childFile;
    u32 nextHash;
    u32 nameLen;
    char name[];
};

struct RomfsFileEntry {
    u32 parent;
    u32 sibling;
    u64 dataOff;
    u64 dataSize;
    u32 nextHash;
    u32 nameLen;
    char name[];
};
#pragma pack(pop)

struct BufHelper {
    void write(const void* data, u64 size) {
        if (offset + size > buf.size()) {
            buf.resize(offset + size);
        }
        std::memcpy(buf.data() + offset, data, size);
        offset += size;
    }

    void write(std::span<const u8> data) {
        write(data.data(), data.size());
    }

    void seek(u64 where_to) {
        offset = where_to;
    }

    [[nodiscard]] u64 tell() const {
        return offset;
    }

    std::vector<u8> buf{};
    u64 offset = 0;
};

struct NcaEntry {
    NcaEntry() = default;

    NcaEntry(const BufHelper& in_buf, NcmContentType in_type)
        : data(in_buf.buf)
        , type(in_type) {
        sha256CalculateHash(hash, data.data(), data.size());
    }

    std::vector<u8> data{};
    u8 type = 0;
    u8 hash[SHA256_HASH_SIZE]{};
};

struct CnmtHeader {
    u64 title_id;
    u32 title_version;
    u8 meta_type;
    u8 _0xD;
    NcmContentMetaHeader meta_header;
    u8 install_type;
    u8 _0x17;
    u32 required_sys_version;
    u8 _0x1C[0x4];
};
static_assert(sizeof(CnmtHeader) == 0x20);

struct ForwarderContentMetaData {
    NcmContentMetaHeader header;
    NcmApplicationMetaExtendedHeader extended;
    NcmContentInfo infos[3];
};

struct ContentStorageRecord {
    NcmContentMetaKey key;
    u8 storage_id;
    u8 padding[0x7];
};

struct NcaMetaEntry {
    explicit NcaMetaEntry(const BufHelper& in_buf, NcmContentType type)
        : nca_entry(in_buf, type) {}

    NcaEntry nca_entry;
    NcmContentMetaHeader content_meta_header{};
    NcmContentMetaKey content_meta_key{};
    ContentStorageRecord content_storage_record{};
    ForwarderContentMetaData content_meta_data{};
};

struct Pfs0Header {
    u32 magic;
    u32 total_files;
    u32 string_table_size;
    u32 padding;
};

struct Pfs0FileTable {
    u64 data_offset;
    u64 data_size;
    u32 name_offset;
    u32 padding;
};

struct FileEntry {
    std::string name;
    std::vector<u8> data;
};

using FileEntries = std::vector<FileEntry>;

struct NpdmPatch {
    char title_name[0x10]{"Application"};
    char product_code[0x10]{};
    u64 tid = 0;
};

struct NacpPatch {
    std::string name;
    std::string author;
    u64 tid = 0;
};

struct RomfsDirContext {
    u32 entry_offset;
    RomfsDirContext* parent;
    RomfsDirContext* child;
    RomfsDirContext* sibling;
    struct RomfsFileContext* file;
    RomfsDirContext* next;
};

struct RomfsFileContext {
    u32 entry_offset;
    u64 offset;
    u64 size;
    RomfsDirContext* parent;
    RomfsFileContext* sibling;
    RomfsFileContext* next;
};

struct RomfsContext {
    RomfsFileContext* files;
    u64 num_dirs;
    u64 num_files;
    u64 dir_table_size;
    u64 file_table_size;
    u64 dir_hash_table_size;
    u64 file_hash_table_size;
    u64 file_partition_size;
};

Result deriveHeaderKey(Keys& out) {
    std::array<u8, 0x20> header_kek{};
    Result rc = splCryptoGenerateAesKek(HEADER_KEK_SRC.data(), 0, 0, header_kek.data());
    if (R_FAILED(rc)) {
        return rc;
    }

    rc = splCryptoGenerateAesKey(header_kek.data(), HEADER_KEY_SRC.data(), out.header_key.data());
    if (R_FAILED(rc)) {
        return rc;
    }

    return splCryptoGenerateAesKey(header_kek.data(), HEADER_KEY_SRC.data() + 0x10, out.header_key.data() + 0x10);
}

u32 align32(u32 offset, u32 alignment) {
    const u32 mask = ~(alignment - 1);
    return (offset + (alignment - 1)) & mask;
}

u64 align64(u64 offset, u64 alignment) {
    const u64 mask = ~(alignment - 1);
    return (offset + (alignment - 1)) & mask;
}

u64 write_padding(BufHelper& buf, u64 off, u64 block) {
    const u64 size = block - (off % block);
    if (size != 0) {
        std::vector<u8> padding(size);
        buf.write(padding.data(), padding.size());
    }
    return size;
}

RomfsDirEntry* romfsGetDirEntry(RomfsDirEntry* directories, u32 offset) {
    return reinterpret_cast<RomfsDirEntry*>(reinterpret_cast<u8*>(directories) + offset);
}

RomfsFileEntry* romfsGetFileEntry(RomfsFileEntry* files, u32 offset) {
    return reinterpret_cast<RomfsFileEntry*>(reinterpret_cast<u8*>(files) + offset);
}

u32 calcPathHash(u32 parent, const u8* path, u32 start, u32 path_len) {
    u32 hash = parent ^ 123456789;
    for (u32 i = 0; i < path_len; i++) {
        hash = (hash >> 5) | (hash << 27);
        hash ^= path[start + i];
    }

    return hash;
}

u32 romfsGetHashTableCount(u32 num_entries) {
    if (num_entries < 3) {
        return 3;
    }

    if (num_entries < 19) {
        return num_entries | 1;
    }

    u32 count = num_entries;
    while (count % 2 == 0 || count % 3 == 0 || count % 5 == 0 || count % 7 == 0 ||
           count % 11 == 0 || count % 13 == 0 || count % 17 == 0) {
        count++;
    }

    return count;
}

void romfsVisitDir(const FileEntries& entries, RomfsDirContext* parent, RomfsContext* romfs_ctx) {
    RomfsDirContext* child_dir_tree = nullptr;
    RomfsFileContext* child_file_tree = nullptr;

    for (const auto& entry : entries) {
        auto* cur_file = static_cast<RomfsFileContext*>(std::calloc(1, sizeof(RomfsFileContext)));
        if (cur_file == nullptr) {
            continue;
        }

        romfs_ctx->num_files++;
        cur_file->parent = parent;
        cur_file->size = entry.data.size();
        romfs_ctx->file_table_size += sizeof(RomfsFileEntry) + align32(entry.name.length() - 1, 4);

        if (child_file_tree == nullptr) {
            cur_file->sibling = child_file_tree;
            child_file_tree = cur_file;
        } else {
            auto* prev = child_file_tree;
            auto* child = child_file_tree->sibling;
            prev->sibling = cur_file;
            cur_file->sibling = child;
        }

        if (romfs_ctx->files == nullptr) {
            cur_file->next = romfs_ctx->files;
            romfs_ctx->files = cur_file;
        } else {
            auto* prev = romfs_ctx->files;
            auto* child = romfs_ctx->files->next;
            prev->next = cur_file;
            cur_file->next = child;
        }
    }

    parent->child = child_dir_tree;
    parent->file = child_file_tree;
}

void buildRomfsIntoFile(const FileEntries& entries, BufHelper& buf) {
    auto* root_ctx = static_cast<RomfsDirContext*>(std::calloc(1, sizeof(RomfsDirContext)));
    if (root_ctx == nullptr) {
        return;
    }
    root_ctx->parent = root_ctx;

    RomfsContext romfs_ctx{};
    romfs_ctx.dir_table_size = sizeof(RomfsDirEntry);
    romfs_ctx.num_dirs = 1;

    romfsVisitDir(entries, root_ctx, &romfs_ctx);

    const u32 dir_hash_table_entry_count = romfsGetHashTableCount(romfs_ctx.num_dirs);
    const u32 file_hash_table_entry_count = romfsGetHashTableCount(romfs_ctx.num_files);
    romfs_ctx.dir_hash_table_size = 4 * dir_hash_table_entry_count;
    romfs_ctx.file_hash_table_size = 4 * file_hash_table_entry_count;

    RomfsHeader header{};
    std::vector<u32> dir_hash_table(dir_hash_table_entry_count, ROMFS_ENTRY_EMPTY);
    std::vector<u32> file_hash_table(file_hash_table_entry_count, ROMFS_ENTRY_EMPTY);

    auto* dir_table = static_cast<RomfsDirEntry*>(std::calloc(1, romfs_ctx.dir_table_size));
    auto* file_table = static_cast<RomfsFileEntry*>(std::calloc(1, romfs_ctx.file_table_size));
    if (dir_table == nullptr || file_table == nullptr) {
        std::free(root_ctx);
        std::free(dir_table);
        std::free(file_table);
        return;
    }

    auto* cur_file = romfs_ctx.files;
    u32 entry_offset = 0;
    for (const auto& entry : entries) {
        romfs_ctx.file_partition_size = align64(romfs_ctx.file_partition_size, 0x10);
        cur_file->offset = romfs_ctx.file_partition_size;
        cur_file->entry_offset = entry_offset;
        romfs_ctx.file_partition_size += cur_file->size;
        entry_offset += sizeof(RomfsFileEntry) + align32(entry.name.length() - 1, 4);
        cur_file = cur_file->next;
    }

    root_ctx->entry_offset = 0x0;

    cur_file = romfs_ctx.files;
    for (const auto& entry : entries) {
        auto* cur_entry = romfsGetFileEntry(file_table, cur_file->entry_offset);
        cur_entry->parent = cur_file->parent->entry_offset;
        cur_entry->sibling = cur_file->sibling == nullptr ? ROMFS_ENTRY_EMPTY : cur_file->sibling->entry_offset;
        cur_entry->dataOff = cur_file->offset;
        cur_entry->dataSize = cur_file->size;

        const u32 name_size = entry.name.length() - 1;
        const u32 hash = calcPathHash(cur_file->parent->entry_offset, reinterpret_cast<const u8*>(entry.name.c_str()), 1, name_size);
        cur_entry->nextHash = file_hash_table[hash % file_hash_table_entry_count];
        file_hash_table[hash % file_hash_table_entry_count] = cur_file->entry_offset;

        cur_entry->nameLen = name_size;
        std::memcpy(cur_entry->name, entry.name.c_str() + 1, name_size);
        cur_file = cur_file->next;
    }

    auto* cur_dir = root_ctx;
    while (cur_dir != nullptr) {
        auto* cur_entry = romfsGetDirEntry(dir_table, cur_dir->entry_offset);
        cur_entry->parent = cur_dir->parent->entry_offset;
        cur_entry->sibling = cur_dir->sibling == nullptr ? ROMFS_ENTRY_EMPTY : cur_dir->sibling->entry_offset;
        cur_entry->childDir = cur_dir->child == nullptr ? ROMFS_ENTRY_EMPTY : cur_dir->child->entry_offset;
        cur_entry->childFile = cur_dir->file == nullptr ? ROMFS_ENTRY_EMPTY : cur_dir->file->entry_offset;

        const u32 hash = calcPathHash(0, nullptr, 0, 0);
        cur_entry->nextHash = dir_hash_table[hash % dir_hash_table_entry_count];
        dir_hash_table[hash % dir_hash_table_entry_count] = cur_dir->entry_offset;
        cur_entry->nameLen = 0;

        auto* temp = cur_dir;
        cur_dir = cur_dir->next;
        std::free(temp);
    }

    header.headerSize = sizeof(header);
    header.fileHashTableSize = romfs_ctx.file_hash_table_size;
    header.fileTableSize = romfs_ctx.file_table_size;
    header.dirHashTableSize = romfs_ctx.dir_hash_table_size;
    header.dirTableSize = romfs_ctx.dir_table_size;
    header.fileDataOff = ROMFS_FILEPARTITION_OFS;
    header.dirHashTableOff = align64(romfs_ctx.file_partition_size + ROMFS_FILEPARTITION_OFS, 4);
    header.dirTableOff = header.dirHashTableOff + romfs_ctx.dir_hash_table_size;
    header.fileHashTableOff = header.dirTableOff + romfs_ctx.dir_table_size;
    header.fileTableOff = header.fileHashTableOff + romfs_ctx.file_hash_table_size;

    buf.write(&header, sizeof(header));

    cur_file = romfs_ctx.files;
    for (const auto& entry : entries) {
        buf.seek(cur_file->offset + ROMFS_FILEPARTITION_OFS);
        buf.write(entry.data.data(), entry.data.size());

        auto* temp = cur_file;
        cur_file = cur_file->next;
        std::free(temp);
    }

    buf.seek(header.dirHashTableOff);
    buf.write(dir_hash_table.data(), romfs_ctx.dir_hash_table_size);
    buf.write(dir_table, romfs_ctx.dir_table_size);
    buf.write(file_hash_table.data(), romfs_ctx.file_hash_table_size);
    buf.write(file_table, romfs_ctx.file_table_size);

    std::free(dir_table);
    std::free(file_table);
}

std::vector<u8> buildRomfs(const FileEntries& entries, u64* out_size) {
    BufHelper buf;
    buildRomfsIntoFile(entries, buf);
    buf.seek(buf.buf.size());
    *out_size = buf.tell();
    write_padding(buf, buf.tell(), IVFC_HASH_BLOCK_SIZE);
    return buf.buf;
}

bool patchNpdmKernelCaps(std::vector<u8>& npdm, u32 off, u32 size, u32 bitmask, u32 value) {
    const u32 pattern = BIT(bitmask) - 1;
    const u32 mask = BIT(bitmask) | pattern;

    for (u32 i = 0; i < size; i += 4) {
        u32 cap = 0;
        std::memcpy(&cap, npdm.data() + off + i, sizeof(cap));
        if ((cap & mask) == pattern) {
            cap = value | pattern;
            std::memcpy(npdm.data() + off + i, &cap, sizeof(cap));
            return true;
        }
    }

    return false;
}

void patchNpdm(std::vector<u8>& npdm_data, const NpdmPatch& patch) {
    npdm::Meta meta{};
    npdm::Aci0 aci0{};
    npdm::Acid acid{};

    std::memcpy(&meta, npdm_data.data(), sizeof(meta));
    std::memcpy(&aci0, npdm_data.data() + meta.aci0_offset, sizeof(aci0));
    std::memcpy(&acid, npdm_data.data() + meta.acid_offset, sizeof(acid));

    std::memcpy(meta.title_name, patch.title_name, sizeof(meta.title_name));
    std::memcpy(meta.product_code, patch.product_code, sizeof(patch.product_code));
    aci0.program_id = patch.tid;
    acid.program_id_min = patch.tid;
    acid.program_id_max = patch.tid;

    Result rc = splInitialize();
    if (R_SUCCEEDED(rc)) {
        u64 ver = 0;
        const auto exosphere_version = static_cast<SplConfigItem>(65000);
        if (R_SUCCEEDED(splGetConfig(exosphere_version, &ver))) {
            ver >>= 40;
            if (ver >= MAKEHOSVERSION(1, 8, 0)) {
                patchNpdmKernelCaps(npdm_data, meta.aci0_offset + aci0.kac_offset, aci0.kac_size, 16, BIT(19));
                patchNpdmKernelCaps(npdm_data, meta.acid_offset + acid.kac_offset, acid.kac_size, 16, BIT(19));
            }
        }
        splExit();
    }

    std::memcpy(npdm_data.data(), &meta, sizeof(meta));
    std::memcpy(npdm_data.data() + meta.aci0_offset, &aci0, sizeof(aci0));
    std::memcpy(npdm_data.data() + meta.acid_offset, &acid, sizeof(acid));
}

void patchNacp(NacpStruct& nacp, const NacpPatch& patch) {
    if (!patch.name.empty()) {
        for (auto& lang : nacp.lang) {
            std::strncpy(lang.name, patch.name.c_str(), sizeof(lang.name) - 1);
        }
    }

    if (!patch.author.empty()) {
        for (auto& lang : nacp.lang) {
            std::strncpy(lang.author, patch.author.c_str(), sizeof(lang.author) - 1);
        }
    }

    nacp.startup_user_account = 0x00;
    nacp.user_account_switch_lock = 0x00;
    nacp.add_on_content_registration_type = 0x01;
    nacp.screenshot = 0x00;
    nacp.video_capture = 0x02;
    nacp.logo_type = 0x02;
    nacp.logo_handling = 0x00;
    nacp.data_loss_confirmation = 0x00;
    nacp.required_network_service_license_on_launch = 0x00;
    nacp.application_error_code_category = 0;

    const char error_code[] = "moonlite";
    std::memcpy(&nacp.application_error_code_category, error_code, sizeof(error_code) - 1);

    nacp.presence_group_id = patch.tid;
    nacp.save_data_owner_id = patch.tid;
    nacp.pseudo_device_id_seed = patch.tid;
    nacp.add_on_content_base_id = patch.tid ^ 0x1000;
    for (auto& id : nacp.local_communication_id) {
        id = patch.tid;
    }

    nacp.play_log_policy = 0x00;
    nacp.play_log_query_capability = 0x00;

    nacp.user_account_save_data_size = 0;
    nacp.user_account_save_data_journal_size = 0;
    nacp.device_save_data_size = 0;
    nacp.device_save_data_journal_size = 0;
    nacp.user_account_save_data_size_max = 0;
    nacp.user_account_save_data_journal_size_max = 0;
    nacp.device_save_data_size_max = 0;
    nacp.device_save_data_journal_size_max = 0;

    if (nacp.supported_language_flag == 0) {
        nacp.supported_language_flag = BIT(0);
    }
}

void addFileEntry(FileEntries& entries, const char* name, const void* data, u64 size) {
    FileEntry entry;
    entry.name = name;
    entry.data.resize(size);
    std::memcpy(entry.data.data(), data, size);
    entries.emplace_back(std::move(entry));
}

void addFileEntry(FileEntries& entries, const char* name, std::span<const u8> data) {
    addFileEntry(entries, name, data.data(), data.size());
}

std::vector<u8> buildIvfcMasterHash(std::span<const u8> level1) {
    std::vector<u8> hash(SHA256_HASH_SIZE);
    sha256CalculateHash(hash.data(), level1.data(), level1.size());
    return hash;
}

std::vector<u8> buildPfs0(const FileEntries& entries) {
    BufHelper buf;
    Pfs0Header header{};
    std::vector<Pfs0FileTable> file_table(entries.size());
    std::vector<char> string_table;

    u64 string_offset = 0;
    u64 data_offset = 0;
    for (u32 i = 0; i < entries.size(); i++) {
        file_table[i].data_offset = data_offset;
        file_table[i].data_size = entries[i].data.size();
        file_table[i].name_offset = string_offset;
        file_table[i].padding = 0;

        string_table.resize(string_offset + entries[i].name.length() + 1);
        std::memcpy(string_table.data() + string_offset, entries[i].name.c_str(), entries[i].name.length() + 1);

        data_offset += entries[i].data.size();
        string_offset += entries[i].name.length() + 1;
    }

    string_table.resize((string_table.size() + 0x1F) & ~0x1F);

    header.magic = 0x30534650;
    header.total_files = entries.size();
    header.string_table_size = string_table.size();
    header.padding = 0;

    buf.write(&header, sizeof(header));
    buf.write(file_table.data(), sizeof(Pfs0FileTable) * file_table.size());
    buf.write(string_table.data(), string_table.size());

    for (const auto& entry : entries) {
        buf.write(entry.data.data(), entry.data.size());
    }

    return buf.buf;
}

std::vector<u8> buildPfs0HashTable(const std::vector<u8>& pfs0, u32 block_size) {
    BufHelper buf;
    u8 hash[SHA256_HASH_SIZE]{};
    u32 read_size = block_size;

    for (u32 i = 0; i < pfs0.size(); i += read_size) {
        if (i + read_size >= pfs0.size()) {
            read_size = pfs0.size() - i;
        }
        sha256CalculateHash(hash, pfs0.data() + i, read_size);
        buf.write(hash, sizeof(hash));
    }

    return buf.buf;
}

std::vector<u8> buildPfs0MasterHash(const std::vector<u8>& pfs0_hash_table) {
    std::vector<u8> hash(SHA256_HASH_SIZE);
    sha256CalculateHash(hash.data(), pfs0_hash_table.data(), pfs0_hash_table.size());
    return hash;
}

void writeNcaPadding(BufHelper& buf) {
    write_padding(buf, buf.tell(), 0x200);
}

void encryptNcaHeader(nca::Header* header, std::span<const u8> key) {
    Aes128XtsContext ctx{};
    aes128XtsContextCreate(&ctx, key.data(), key.data() + 0x10, true);

    u8 sector = 0;
    for (u64 pos = 0; pos < sizeof(*header); pos += 0x200) {
        aes128XtsContextResetSector(&ctx, sector++, true);
        aes128XtsEncrypt(&ctx, reinterpret_cast<u8*>(header) + pos, reinterpret_cast<const u8*>(header) + pos, 0x200);
    }
}

void writeNcaSection(nca::Header& nca_header, u8 index, u64 start, u64 end) {
    auto& section = nca_header.fs_table[index];
    section.media_start_offset = start / 0x200;
    section.media_end_offset = end / 0x200;
    section._0x8[0] = 0x1;
}

void writeNcaFsHeaderPfs0(nca::Header& nca_header, u8 index, const std::vector<u8>& master_hash, u64 hash_table_size, u32 block_size) {
    auto& fs_header = nca_header.fs_header[index];
    fs_header.hash_type = nca::HashType_HierarchicalSha256;
    fs_header.fs_type = nca::FileSystemType_PFS0;
    fs_header.version = 0x2;
    fs_header.hash_data.hierarchical_sha256_data.layer_count = 0x2;
    fs_header.hash_data.hierarchical_sha256_data.block_size = block_size;
    fs_header.encryption_type = nca::EncryptionType_None;
    fs_header.hash_data.hierarchical_sha256_data.hash_layer.size = hash_table_size;
    std::memcpy(fs_header.hash_data.hierarchical_sha256_data.master_hash, master_hash.data(), master_hash.size());
    sha256CalculateHash(&nca_header.fs_header_hash[index], &fs_header, sizeof(fs_header));
}

void writeNcaFsHeaderRomfs(nca::Header& nca_header, u8 index) {
    auto& fs_header = nca_header.fs_header[index];
    fs_header.hash_type = nca::HashType_HierarchicalIntegrity;
    fs_header.fs_type = nca::FileSystemType_RomFS;
    fs_header.version = 0x2;
    fs_header.hash_data.integrity_meta_info.magic = 0x43465649;
    fs_header.hash_data.integrity_meta_info.version = 0x20000;
    fs_header.hash_data.integrity_meta_info.master_hash_size = SHA256_HASH_SIZE;
    fs_header.hash_data.integrity_meta_info.info_level_hash.max_layers = 0x7;
    fs_header.encryption_type = nca::EncryptionType_None;
    fs_header.hash_data.integrity_meta_info.info_level_hash.levels[5].block_size = 0x0E;
    sha256CalculateHash(&nca_header.fs_header_hash[index], &fs_header, sizeof(fs_header));
}

void writeNcaPfs0(nca::Header& nca_header, u8 index, const FileEntries& entries, u32 block_size, BufHelper& buf) {
    const auto pfs0 = buildPfs0(entries);
    const auto pfs0_hash_table = buildPfs0HashTable(pfs0, block_size);
    const auto pfs0_master_hash = buildPfs0MasterHash(pfs0_hash_table);

    buf.write(pfs0_hash_table.data(), pfs0_hash_table.size());
    const auto padding_size = write_padding(buf, pfs0_hash_table.size(), PFS0_PADDING_SIZE);

    nca_header.fs_header[index].hash_data.hierarchical_sha256_data.pfs0_layer.offset = pfs0_hash_table.size() + padding_size;
    nca_header.fs_header[index].hash_data.hierarchical_sha256_data.pfs0_layer.size = pfs0.size();

    buf.write(pfs0.data(), pfs0.size());
    writeNcaPadding(buf);

    const auto section_start = index == 0 ? sizeof(nca_header) : nca_header.fs_table[index - 1].media_end_offset * 0x200;
    writeNcaSection(nca_header, index, section_start, buf.tell());
    writeNcaFsHeaderPfs0(nca_header, index, pfs0_master_hash, pfs0_hash_table.size(), block_size);
}

std::vector<u8> createIvfcLevel(const std::vector<u8>& src) {
    BufHelper buf;
    u8 hash[SHA256_HASH_SIZE]{};
    u64 read_size = IVFC_HASH_BLOCK_SIZE;

    for (u32 i = 0; i < src.size(); i += read_size) {
        if (i + read_size >= src.size()) {
            read_size = src.size() - i;
        }
        sha256CalculateHash(hash, src.data() + i, read_size);
        buf.write(hash, sizeof(hash));
    }

    write_padding(buf, buf.tell(), IVFC_HASH_BLOCK_SIZE);
    return buf.buf;
}

void writeNcaRomfs(nca::Header& nca_header, u8 index, const FileEntries& entries, BufHelper& buf) {
    auto& fs_header = nca_header.fs_header[index];
    auto& meta_info = fs_header.hash_data.integrity_meta_info;
    auto& info_level_hash = meta_info.info_level_hash;

    std::vector<u8> ivfc[IVFC_MAX_LEVEL];
    ivfc[5] = buildRomfs(entries, &info_level_hash.levels[5].hash_data_size);

    for (int level = 4; level >= 0; level--) {
        ivfc[level] = createIvfcLevel(ivfc[level + 1]);
        info_level_hash.levels[level].hash_data_size = ivfc[level].size();
        info_level_hash.levels[level].block_size = 0x0E;
    }

    info_level_hash.levels[0].logical_offset = 0;
    for (int level = 1; level <= 5; level++) {
        info_level_hash.levels[level].logical_offset =
            info_level_hash.levels[level - 1].logical_offset + info_level_hash.levels[level - 1].hash_data_size;
    }

    for (const auto& level : ivfc) {
        buf.write(level.data(), level.size());
    }

    writeNcaPadding(buf);

    const auto ivfc_master_hash = buildIvfcMasterHash(ivfc[0]);
    std::memcpy(meta_info.master_hash, ivfc_master_hash.data(), sizeof(meta_info.master_hash));

    const auto section_start = index == 0 ? sizeof(nca_header) : nca_header.fs_table[index - 1].media_end_offset * 0x200;
    writeNcaSection(nca_header, index, section_start, buf.tell());
    writeNcaFsHeaderRomfs(nca_header, index);
}

void finalizeNcaHeader(nca::Header& nca_header, u64 tid, const Keys& keys, nca::ContentType type, BufHelper& buf) {
    nca_header.magic = NCA3_MAGIC;
    nca_header.distribution_type = nca::DistributionType_System;
    nca_header.content_type = type;
    nca_header.program_id = tid;
    nca_header.sdk_version = 0x000C1100;
    nca_header.size = buf.tell();

    encryptNcaHeader(&nca_header, keys.header_key);
    buf.seek(0);
    buf.write(&nca_header, sizeof(nca_header));
}

NcaEntry createProgramNca(u64 tid, const Keys& keys, const FileEntries& exefs, const FileEntries& romfs_entries, const FileEntries& logo) {
    BufHelper buf;
    nca::Header nca_header{};
    buf.write(&nca_header, sizeof(nca_header));

    writeNcaPfs0(nca_header, 0, exefs, PFS0_EXEFS_HASH_BLOCK_SIZE, buf);
    writeNcaRomfs(nca_header, 1, romfs_entries, buf);
    if (logo.size() == 2 && !logo[0].data.empty() && !logo[1].data.empty()) {
        writeNcaPfs0(nca_header, 2, logo, PFS0_LOGO_HASH_BLOCK_SIZE, buf);
    }

    finalizeNcaHeader(nca_header, tid, keys, nca::ContentType_Program, buf);
    return {buf, NcmContentType_Program};
}

NcaEntry createControlNca(u64 tid, const Keys& keys, const FileEntries& romfs_entries) {
    BufHelper buf;
    nca::Header nca_header{};
    buf.write(&nca_header, sizeof(nca_header));

    writeNcaRomfs(nca_header, 0, romfs_entries, buf);
    finalizeNcaHeader(nca_header, tid, keys, nca::ContentType_Control, buf);

    return {buf, NcmContentType_Control};
}

NcaMetaEntry createMetaNca(u64 tid, const Keys& keys, NcmStorageId storage_id, const std::vector<NcaEntry>& ncas) {
    CnmtHeader cnmt_header{};
    NcmApplicationMetaExtendedHeader cnmt_extended{};
    NcmPackagedContentInfo packaged_content_info[2]{};
    u8 digest[0x20]{};
    BufHelper buf;

    cnmt_header.title_id = tid;
    cnmt_header.title_version = 0;
    cnmt_header.meta_type = NcmContentMetaType_Application;
    cnmt_header.meta_header.extended_header_size = sizeof(cnmt_extended);
    cnmt_header.meta_header.content_count = 0x2;
    cnmt_header.meta_header.content_meta_count = 0x1;
    cnmt_header.meta_header.attributes = 0x0;
    cnmt_header.meta_header.storage_id = storage_id;
    cnmt_extended.patch_id = cnmt_header.title_id | 0x800;

    for (u32 i = 0; i < ncas.size(); i++) {
        std::memcpy(packaged_content_info[i].hash, ncas[i].hash, sizeof(packaged_content_info[i].hash));
        std::memcpy(&packaged_content_info[i].info.content_id, ncas[i].hash, sizeof(packaged_content_info[i].info.content_id));
        packaged_content_info[i].info.content_type = ncas[i].type;
        ncmU64ToContentInfoSize(ncas[i].data.size(), &packaged_content_info[i].info);
    }

    BufHelper cnmt_buf;
    cnmt_buf.write(&cnmt_header, sizeof(cnmt_header));
    cnmt_buf.write(&cnmt_extended, sizeof(cnmt_extended));
    cnmt_buf.write(&packaged_content_info, sizeof(packaged_content_info));
    cnmt_buf.write(digest, sizeof(digest));

    FileEntries cnmt_entries;
    char cnmt_name[34];
    std::snprintf(cnmt_name, sizeof(cnmt_name), "Application_%016lX.cnmt", tid);
    addFileEntry(cnmt_entries, cnmt_name, cnmt_buf.buf.data(), cnmt_buf.buf.size());

    nca::Header nca_header{};
    buf.write(&nca_header, sizeof(nca_header));
    writeNcaPfs0(nca_header, 0, cnmt_entries, PFS0_META_HASH_BLOCK_SIZE, buf);
    finalizeNcaHeader(nca_header, tid, keys, nca::ContentType_Meta, buf);

    NcaMetaEntry entry(buf, NcmContentType_Meta);
    entry.content_meta_header = cnmt_header.meta_header;
    entry.content_meta_header.content_count++;
    entry.content_meta_header.storage_id = 0;

    entry.content_meta_key.id = cnmt_header.title_id;
    entry.content_meta_key.version = cnmt_header.title_version;
    entry.content_meta_key.type = cnmt_header.meta_type;
    entry.content_meta_key.install_type = NcmContentInstallType_Full;
    std::memset(entry.content_meta_key.padding, 0, sizeof(entry.content_meta_key.padding));

    entry.content_storage_record.key = entry.content_meta_key;
    entry.content_storage_record.storage_id = storage_id;
    std::memset(entry.content_storage_record.padding, 0, sizeof(entry.content_storage_record.padding));

    entry.content_meta_data.header = entry.content_meta_header;
    entry.content_meta_data.extended = cnmt_extended;

    std::memcpy(&entry.content_meta_data.infos[0].content_id, entry.nca_entry.hash, sizeof(entry.content_meta_data.infos[0].content_id));
    entry.content_meta_data.infos[0].content_type = entry.nca_entry.type;
    entry.content_meta_data.infos[0].attr = 0;
    ncmU64ToContentInfoSize(cnmt_buf.buf.size(), &entry.content_meta_data.infos[0]);
    entry.content_meta_data.infos[0].id_offset = 0;

    entry.content_meta_data.infos[1] = packaged_content_info[0].info;
    entry.content_meta_data.infos[2] = packaged_content_info[1].info;

    return entry;
}

Result pushApplicationRecord(u64 tid, const ContentStorageRecord* records, u32 count) {
    Service app_manager{};
    Result rc = nsGetApplicationManagerInterface(&app_manager);
    if (R_FAILED(rc)) {
        return rc;
    }

    const struct {
        u8 last_modified_event;
        u8 padding[0x7];
        u64 tid;
    } in = {0x3, {0}, tid};

    rc = serviceDispatchIn(&app_manager, 16, in,
        .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_In},
        .buffers = {{records, sizeof(*records) * count}});
    serviceClose(&app_manager);
    return rc;
}

Result invalidateApplicationControlCache(u64 tid) {
    Service app_manager{};
    Result rc = nsGetApplicationManagerInterface(&app_manager);
    if (R_FAILED(rc)) {
        return rc;
    }

    rc = serviceDispatchIn(&app_manager, 404, tid);
    serviceClose(&app_manager);
    return rc;
}

std::vector<u8> readFileBytes(const std::string& path) {
    Data data = Data::read_from_file(path);
    if (data.is_empty()) {
        return {};
    }

    return {data.bytes(), data.bytes() + data.size()};
}

std::string encodeAppName(const std::string& name) {
    std::string encoded;
    encoded.reserve(name.size());

    for (const char ch : name) {
        if (ch == ' ') {
            encoded += "&#160;";
        } else {
            encoded.push_back(ch);
        }
    }

    return encoded;
}

std::string formatNroPathForForwarder(std::string path) {
    if (!path.starts_with("sdmc:")) {
        path = "sdmc:" + path;
    }

    if (path.find(' ') != std::string::npos) {
        return '\"' + path + '\"';
    }

    return path;
}

std::vector<u8> resizeRgbaImage(const std::vector<u8>& input, int in_width, int in_height, int out_width, int out_height) {
    constexpr int channels = 4;
    std::vector<u8> output(out_width * out_height * channels);

    const auto sample_bilinear = [&](double src_x, double src_y, u8* out_pixel) {
        const double clamped_x = std::clamp(src_x, 0.0, static_cast<double>(in_width - 1));
        const double clamped_y = std::clamp(src_y, 0.0, static_cast<double>(in_height - 1));

        const int x0 = static_cast<int>(std::floor(clamped_x));
        const int y0 = static_cast<int>(std::floor(clamped_y));
        const int x1 = std::min(x0 + 1, in_width - 1);
        const int y1 = std::min(y0 + 1, in_height - 1);

        const double frac_x = clamped_x - static_cast<double>(x0);
        const double frac_y = clamped_y - static_cast<double>(y0);

        const size_t offset00 = (y0 * in_width + x0) * channels;
        const size_t offset10 = (y0 * in_width + x1) * channels;
        const size_t offset01 = (y1 * in_width + x0) * channels;
        const size_t offset11 = (y1 * in_width + x1) * channels;

        for (int channel = 0; channel < channels; channel++) {
            const double top = input[offset00 + channel] * (1.0 - frac_x) + input[offset10 + channel] * frac_x;
            const double bottom = input[offset01 + channel] * (1.0 - frac_x) + input[offset11 + channel] * frac_x;
            out_pixel[channel] = static_cast<u8>(std::clamp(top * (1.0 - frac_y) + bottom * frac_y, 0.0, 255.0));
        }
    };

    for (int y = 0; y < out_height; y++) {
        const double src_y = ((static_cast<double>(y) + 0.5) * static_cast<double>(in_height) / static_cast<double>(out_height)) - 0.5;
        for (int x = 0; x < out_width; x++) {
            const double src_x = ((static_cast<double>(x) + 0.5) * static_cast<double>(in_width) / static_cast<double>(out_width)) - 0.5;
            const size_t dst_offset = (y * out_width + x) * channels;
            sample_bilinear(src_x, src_y, output.data() + dst_offset);
        }
    }

    return output;
}

std::vector<u8> resizeRgbaImageCover(const std::vector<u8>& input, int in_width, int in_height, int out_width, int out_height) {
    constexpr int channels = 4;
    std::vector<u8> output(out_width * out_height * channels);

    const double scale = std::max(
        static_cast<double>(out_width) / static_cast<double>(in_width),
        static_cast<double>(out_height) / static_cast<double>(in_height));
    const double source_window_width = static_cast<double>(out_width) / scale;
    const double source_window_height = static_cast<double>(out_height) / scale;
    const double source_origin_x = (static_cast<double>(in_width) - source_window_width) * 0.5;
    const double source_origin_y = (static_cast<double>(in_height) - source_window_height) * 0.5;

    const auto sample_bilinear = [&](double src_x, double src_y, u8* out_pixel) {
        const double clamped_x = std::clamp(src_x, 0.0, static_cast<double>(in_width - 1));
        const double clamped_y = std::clamp(src_y, 0.0, static_cast<double>(in_height - 1));

        const int x0 = static_cast<int>(std::floor(clamped_x));
        const int y0 = static_cast<int>(std::floor(clamped_y));
        const int x1 = std::min(x0 + 1, in_width - 1);
        const int y1 = std::min(y0 + 1, in_height - 1);

        const double frac_x = clamped_x - static_cast<double>(x0);
        const double frac_y = clamped_y - static_cast<double>(y0);

        const size_t offset00 = (y0 * in_width + x0) * channels;
        const size_t offset10 = (y0 * in_width + x1) * channels;
        const size_t offset01 = (y1 * in_width + x0) * channels;
        const size_t offset11 = (y1 * in_width + x1) * channels;

        for (int channel = 0; channel < channels; channel++) {
            const double top = input[offset00 + channel] * (1.0 - frac_x) + input[offset10 + channel] * frac_x;
            const double bottom = input[offset01 + channel] * (1.0 - frac_x) + input[offset11 + channel] * frac_x;
            out_pixel[channel] = static_cast<u8>(std::clamp(top * (1.0 - frac_y) + bottom * frac_y, 0.0, 255.0));
        }
    };

    for (int y = 0; y < out_height; y++) {
        const double src_y = source_origin_y + ((static_cast<double>(y) + 0.5) * source_window_height / static_cast<double>(out_height)) - 0.5;
        for (int x = 0; x < out_width; x++) {
            const double src_x = source_origin_x + ((static_cast<double>(x) + 0.5) * source_window_width / static_cast<double>(out_width)) - 0.5;
            const size_t dst_offset = (y * out_width + x) * channels;
            sample_bilinear(src_x, src_y, output.data() + dst_offset);
        }
    }

    return output;
}

struct DecodedRgbaImage {
    std::vector<u8> rgba;
    int width = 0;
    int height = 0;
};

DecodedRgbaImage decodeRgbaImage(std::span<const u8> bytes) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(bytes.data(), bytes.size(), &width, &height, &channels, 4);
    if (decoded == nullptr || width <= 0 || height <= 0) {
        return {};
    }

    DecodedRgbaImage image;
    image.width = width;
    image.height = height;
    image.rgba.assign(decoded, decoded + (width * height * 4));
    stbi_image_free(decoded);
    return image;
}

void blendBadgePixel(std::vector<u8>& canvas, size_t canvas_offset, const std::vector<u8>& badge, size_t badge_offset) {
    const int alpha = badge[badge_offset + 3];
    if (alpha == 0) {
        return;
    }

    const int inverse_alpha = 255 - alpha;
    for (int channel = 0; channel < 3; channel++) {
        const int base = canvas[canvas_offset + channel];
        const int overlay = badge[badge_offset + channel];
        canvas[canvas_offset + channel] = static_cast<u8>((overlay * alpha + base * inverse_alpha) / 255);
    }
    canvas[canvas_offset + 3] = 255;
}

void overlayMoonlightBadge(std::vector<u8>& rgba, int width, int height) {
    auto badge_bytes = readFileBytes("romfs:/img/moonlight_96.png");
    if (badge_bytes.empty()) {
        return;
    }

    auto badge = decodeRgbaImage(badge_bytes);
    if (badge.rgba.empty()) {
        return;
    }

    constexpr int badge_margin = 6;
    const int target_size = std::min(width, height) / 4;
    int badge_width = badge.width;
    int badge_height = badge.height;
    if (badge_width != target_size || badge_height != target_size) {
        badge.rgba = resizeRgbaImage(badge.rgba, badge.width, badge.height, target_size, target_size);
        badge.width = target_size;
        badge.height = target_size;
        badge_width = target_size;
        badge_height = target_size;
    }

    const int dst_x = std::max(0, width - badge_width - badge_margin);
    const int dst_y = badge_margin;
    for (int y = 0; y < badge_height; y++) {
        for (int x = 0; x < badge_width; x++) {
            const size_t canvas_offset = ((dst_y + y) * width + (dst_x + x)) * 4;
            const size_t badge_offset = (y * badge_width + x) * 4;
            blendBadgePixel(rgba, canvas_offset, badge.rgba, badge_offset);
        }
    }
}

std::vector<u8> encodeJpeg(std::span<const u8> rgba, int width, int height) {
    std::vector<u8> output;
    output.reserve(width * height * 4);

    const auto callback = [](void* context, void* data, int size) {
        auto* buffer = static_cast<std::vector<u8>*>(context);
        const auto offset = buffer->size();
        buffer->resize(offset + size);
        std::memcpy(buffer->data() + offset, data, size);
    };

    if (stbi_write_jpg_to_func(callback, &output, width, height, 4, rgba.data(), 93) != 0) {
        return output;
    }

    return {};
}

std::vector<u8> normalizeForwarderIcon(std::span<const u8> bytes, bool add_moonlight_logo) {
    auto image = decodeRgbaImage(bytes);
    if (image.rgba.empty()) {
        return {bytes.begin(), bytes.end()};
    }

    if (image.width != 256 || image.height != 256) {
        image.rgba = resizeRgbaImageCover(image.rgba, image.width, image.height, 256, 256);
        image.width = 256;
        image.height = 256;
    }

    if (add_moonlight_logo) {
        overlayMoonlightBadge(image.rgba, image.width, image.height);
    }

    const auto jpeg = encodeJpeg(image.rgba, image.width, image.height);
    if (!jpeg.empty()) {
        return jpeg;
    }

    return {bytes.begin(), bytes.end()};
}

std::string buildForwarderArgs(const Host& host, const App& app) {
    std::string args;
    auto append = [&](const std::string& value) {
        if (!args.empty()) {
            args.push_back(' ');
        }
        args += value;
    };

    if (!host.mac.empty()) {
        append("--host=" + host.mac);
    } else {
        const auto preferredAddress = host.preferred_address();
        if (!preferredAddress.empty()) {
            append("--ip=" + preferredAddress);
        }
    }

    append("--appid=" + std::to_string(app.app_id));
    append("--appname=" + encodeAppName(app.name));
    return args;
}

std::vector<u8> resolveForwarderIcon(const App& app, bool add_moonlight_logo) {
    auto icon = readFileBytes(BoxArtManager::get_texture_path(app.app_id));
    if (!icon.empty()) {
        return normalizeForwarderIcon(icon, add_moonlight_logo);
    }

    icon = readFileBytes("romfs:/forwarder/control/icon_AmericanEnglish.dat");
    if (!icon.empty()) {
        return normalizeForwarderIcon(icon, add_moonlight_logo);
    }

    return {};
}

NacpStruct getBaseNacp() {
    NacpStruct nacp{};
    if (R_FAILED(appletGetMainAppletApplicationControlProperty(&nacp))) {
        std::snprintf(nacp.display_version, sizeof(nacp.display_version), "%s", APP_VERSION);
        nacp.supported_language_flag = BIT(0);
    }
    return nacp;
}

Result installForwarder(const Host& host, const App& app, bool add_moonlight_logo) {
    const std::string nro_path = formatNroPathForForwarder(Settings::instance().launch_path());
    if (nro_path.empty()) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    const auto icon = resolveForwarderIcon(app, add_moonlight_logo);
    if (icon.empty()) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    const auto main_nso = readFileBytes("romfs:/forwarder/exefs/main");
    const auto main_npdm = readFileBytes("romfs:/forwarder/exefs/main.npdm");
    if (main_nso.empty() || main_npdm.empty()) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    Result rc = splCryptoInitialize();
    if (R_FAILED(rc)) {
        return rc;
    }

    Keys keys{};
    rc = deriveHeaderKey(keys);
    if (R_FAILED(rc)) {
        splCryptoExit();
        return rc;
    }

    rc = ncmInitialize();
    if (R_FAILED(rc)) {
        splCryptoExit();
        return rc;
    }

    rc = nsInitialize();
    if (R_FAILED(rc)) {
        ncmExit();
        splCryptoExit();
        return rc;
    }

    std::string args = buildForwarderArgs(host, app);
    if (args.empty()) {
        args = nro_path;
    } else {
        args = nro_path + ' ' + args;
    }

    u64 hash_data[SHA256_HASH_SIZE / sizeof(u64)]{};
    const auto hash_path = nro_path + args;
    sha256CalculateHash(hash_data, hash_path.data(), hash_path.length());
    const u64 old_tid = 0x0100000000000000ULL | (hash_data[0] & 0x00FFFFFFFFFFF000ULL);
    const u64 tid = 0x0500000000000000ULL | (hash_data[0] & 0x00FFFFFFFFFFF000ULL);

    std::vector<NcaEntry> nca_entries;

    FileEntries exefs;
    addFileEntry(exefs, "main", main_nso.data(), main_nso.size());
    addFileEntry(exefs, "main.npdm", main_npdm.data(), main_npdm.size());

    FileEntries program_romfs;
    addFileEntry(program_romfs, "/nextArgv", args.data(), args.length());
    addFileEntry(program_romfs, "/nextNroPath", nro_path.data(), nro_path.length());

    FileEntries logo;

    NpdmPatch npdm_patch{};
    npdm_patch.tid = tid;
    patchNpdm(exefs[1].data, npdm_patch);
    nca_entries.emplace_back(createProgramNca(tid, keys, exefs, program_romfs, logo));

    NacpStruct nacp = getBaseNacp();
    NacpPatch nacp_patch{
        .name = app.name,
        .author = host.hostname.empty() ? "Moonlight" : host.hostname,
        .tid = tid,
    };
    patchNacp(nacp, nacp_patch);

    FileEntries control_romfs;
    addFileEntry(control_romfs, "/control.nacp", &nacp, sizeof(nacp));
    addFileEntry(control_romfs, "/icon_AmericanEnglish.dat", icon.data(), icon.size());
    nca_entries.emplace_back(createControlNca(tid, keys, control_romfs));

    const auto meta_entry = createMetaNca(tid, keys, NcmStorageId_SdCard, nca_entries);
    nca_entries.emplace_back(meta_entry.nca_entry);

    NcmContentStorage cs{};
    rc = ncmOpenContentStorage(&cs, NcmStorageId_SdCard);
    if (R_FAILED(rc)) {
        nsExit();
        ncmExit();
        splCryptoExit();
        return rc;
    }

    for (const auto& nca_entry : nca_entries) {
        NcmContentId content_id{};
        NcmPlaceHolderId placeholder_id{};
        std::memcpy(&content_id, nca_entry.hash, sizeof(content_id));

        rc = ncmContentStorageGeneratePlaceHolderId(&cs, &placeholder_id);
        if (R_FAILED(rc)) {
            ncmContentStorageClose(&cs);
            nsExit();
            ncmExit();
            splCryptoExit();
            return rc;
        }

        ncmContentStorageDeletePlaceHolder(&cs, &placeholder_id);

        rc = ncmContentStorageCreatePlaceHolder(&cs, &content_id, &placeholder_id, nca_entry.data.size());
        if (R_FAILED(rc)) {
            ncmContentStorageClose(&cs);
            nsExit();
            ncmExit();
            splCryptoExit();
            return rc;
        }

        rc = ncmContentStorageWritePlaceHolder(&cs, &placeholder_id, 0, nca_entry.data.data(), nca_entry.data.size());
        if (R_FAILED(rc)) {
            ncmContentStorageClose(&cs);
            nsExit();
            ncmExit();
            splCryptoExit();
            return rc;
        }

        ncmContentStorageDelete(&cs, &content_id);

        rc = ncmContentStorageRegister(&cs, &content_id, &placeholder_id);
        if (R_FAILED(rc)) {
            ncmContentStorageClose(&cs);
            nsExit();
            ncmExit();
            splCryptoExit();
            return rc;
        }
    }
    ncmContentStorageClose(&cs);

    NcmContentMetaDatabase db{};
    rc = ncmOpenContentMetaDatabase(&db, NcmStorageId_SdCard);
    if (R_FAILED(rc)) {
        nsExit();
        ncmExit();
        splCryptoExit();
        return rc;
    }

    rc = ncmContentMetaDatabaseSet(&db, &meta_entry.content_meta_key, &meta_entry.content_meta_data, sizeof(meta_entry.content_meta_data));
    if (R_SUCCEEDED(rc)) {
        rc = ncmContentMetaDatabaseCommit(&db);
    }
    ncmContentMetaDatabaseClose(&db);
    if (R_FAILED(rc)) {
        nsExit();
        ncmExit();
        splCryptoExit();
        return rc;
    }

    const Result delete_old_rc = nsDeleteApplicationCompletely(old_tid);
    if (R_FAILED(delete_old_rc) && delete_old_rc != 0x410) {
        std::printf("Failed to remove old forwarder: 0x%X\n", delete_old_rc);
    }

    nsDeleteApplicationEntity(tid);

    rc = pushApplicationRecord(tid, &meta_entry.content_storage_record, 1);
    if (R_SUCCEEDED(rc)) {
        rc = invalidateApplicationControlCache(tid);
    }

    nsExit();
    ncmExit();
    splCryptoExit();
    return rc;
}

} // namespace

int makeForwarder(const Host& host, const App& app, bool add_moonlight_logo) {
    return static_cast<int>(installForwarder(host, app, add_moonlight_logo));
}

#else

int makeForwarder(const Host& host, const App& app, bool add_moonlight_logo) {
    (void)host;
    (void)app;
    (void)add_moonlight_logo;
    return EXIT_FAILURE;
}

#endif
