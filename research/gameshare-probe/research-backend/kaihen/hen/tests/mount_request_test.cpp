// SPDX-License-Identifier: GPL-2.0-only
// Host test of decodeCodeMountRequest (bootstrap/source/domain_mount.hpp),
// which replaced two parsers of Loader's OpenCodeFileSystem request that read
// fixed byte offsets: one in the bootstrap's DomainMount::inspectRequest and
// one in HEN's GameArm::inspectGameRequest. The old offsets are kept here as
// the reference. For requests that cover each branch, the decoder must find
// the same request, with the same fields, and fail on the same requests.

#include <cstdio>
#include <cstring>
extern "C" {
#include <switch/services/ncm_types.h>
}
#include "../../common/code_mount_request.hpp"

namespace {

constexpr u64 RootId = 0x31;
int g_failures = 0;

/// The old parsers' common part, at the byte offsets they used.
struct OldDecode {
    Result result;
    bool open_code;
    u8 attributes;
    u8 storage;
    u64 title;
};

OldDecode oldDecode(const u8 request[0x100], u64 root_id) {
    OldDecode out = {};
    HipcParsedRequest parsed = hipcParseRequest(const_cast<u8*>(request));
    if (parsed.meta.type != CmifCommandType_Request &&
        parsed.meta.type != CmifCommandType_RequestWithContext) {
        return out;
    }
    const u8* domain = static_cast<const u8*>(
        cmifGetAlignedDataStart(parsed.data.data_words, const_cast<u8*>(request)));
    if (domain + 48 > request + 0x100) {
        out.result = MAKERESULT(Module_Libnx, LibnxError_BadInput);
        return out;
    }
    u32 object = 0;
    u32 command = 0;
    std::memcpy(&object, domain + 4, sizeof(object));
    std::memcpy(&command, domain + 24, sizeof(command));
    if (domain[0] != 1 || object != root_id || std::memcmp(domain + 16, "SFCI", 4) != 0 ||
        command != 0) {
        return out;
    }
    out.open_code = true;
    out.attributes = domain[32];
    out.storage = domain[33];
    std::memcpy(&out.title, domain + 40, sizeof(out.title));
    return out;
}

struct Case {
    u32 type;
    u32 num_send_statics;
    u32 num_send_buffers;
    u32 num_copy_handles;
    u8 domain_type;
    u32 object_id;
    u32 magic;
    u32 command_id;
    u8 attributes;
    u8 storage;
    u64 title;
};

/// Builds a request as a client would send it to a domain object.
void build(const Case& c, u8 message[0x100]) {
    std::memset(message, 0, 0x100);
    // Each static descriptor and copy handle moves the data words, and so the
    // aligned data start, further into the buffer: with enough of them, the
    // arguments no longer fit.
    HipcRequest request = hipcMakeRequestInline(message,
        .type = c.type,
        .num_send_statics = c.num_send_statics,
        .num_send_buffers = c.num_send_buffers,
        .num_data_words = 16,
        .num_copy_handles = c.num_copy_handles,
    );
    u8* data = static_cast<u8*>(cmifGetAlignedDataStart(request.data_words, message));
    u8 payload[48] = {};
    CmifDomainInHeader domain = {};
    domain.type = c.domain_type;
    domain.object_id = c.object_id;
    CmifInHeader header = {};
    header.magic = c.magic;
    header.command_id = c.command_id;
    ounce::CodeMountArguments arguments = {};
    arguments.content_attributes = c.attributes;
    arguments.storage_id = c.storage;
    arguments.program_id = c.title;
    std::memcpy(payload, &domain, sizeof(domain));
    std::memcpy(payload + 16, &header, sizeof(header));
    std::memcpy(payload + 32, &arguments, sizeof(arguments));
    // Write what fits in the buffer, as a long request would leave it.
    const size_t room = static_cast<size_t>(message + 0x100 - data);
    std::memcpy(data, payload, room < sizeof(payload) ? room : sizeof(payload));
}

void check(const Case& c) {
    alignas(16) u8 message[0x100];
    build(c, message);

    const OldDecode expected = oldDecode(message, RootId);
    ounce::CodeMountRequest decoded;
    const Result result = ounce::decodeCodeMountRequest(message, RootId, decoded);

    const bool same = result == expected.result && decoded.open_code == expected.open_code &&
        (!expected.open_code ||
         (decoded.arguments.content_attributes == expected.attributes &&
          decoded.arguments.storage_id == expected.storage &&
          decoded.arguments.program_id == expected.title));
    if (!same) {
        std::printf("FAIL type=%u statics=%u copies=%u domain=%u object=%#x magic=%#x "
                    "command=%u: old (%#x, %d) new (%#x, %d)\n",
                    c.type, c.num_send_statics, c.num_copy_handles, c.domain_type,
                    c.object_id, c.magic,
                    c.command_id, expected.result, expected.open_code, result,
                    decoded.open_code);
        g_failures++;
    }
}

} // namespace

int main() {
    const u32 types[] = {CmifCommandType_Request, CmifCommandType_RequestWithContext,
                         CmifCommandType_Control, CmifCommandType_Close};
    const u32 statics[] = {0, 15};
    const u32 buffers[] = {0, 2};
    const u32 copies[] = {0, 1, 7, 15};
    const u8 domain_types[] = {CmifDomainRequestType_SendMessage, CmifDomainRequestType_Close};
    const u32 objects[] = {RootId, RootId + 1};
    const u32 magics[] = {CMIF_IN_HEADER_MAGIC, CMIF_OUT_HEADER_MAGIC};
    const u32 commands[] = {0, 1, 2};
    const u8 attributes[] = {1, 0};
    const u8 storages[] = {NcmStorageId_BuiltInSystem, NcmStorageId_SdCard};
    const u64 titles[] = {0x0100000000001000ull, 0x0400000000010000ull};

    unsigned cases = 0;
    unsigned found = 0;
    unsigned too_short = 0;
    for (u32 type : types)
    for (u32 sends : statics)
    for (u32 buffer : buffers)
    for (u32 copy : copies)
    for (u8 domain_type : domain_types)
    for (u32 object : objects)
    for (u32 magic : magics)
    for (u32 command : commands)
    for (u8 attribute : attributes)
    for (u8 storage : storages)
    for (u64 title : titles) {
        const Case c = {type, sends, buffer, copy, domain_type, object, magic, command, attribute,
                        storage, title};
        check(c);
        alignas(16) u8 message[0x100];
        build(c, message);
        const OldDecode reference = oldDecode(message, RootId);
        found += reference.open_code;
        too_short += R_FAILED(reference.result);
        cases++;
    }
    std::printf("%u requests: %u OpenCodeFileSystem on the root, %u too short\n", cases,
                found, too_short);
    if (g_failures || !found || !too_short) {
        std::printf("mount request test: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("mount request test: PASS\n");
    return 0;
}
