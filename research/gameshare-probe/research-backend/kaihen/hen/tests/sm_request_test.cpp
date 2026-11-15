// SPDX-License-Identifier: GPL-2.0-only
// Host test of the SM proxy's request decoder and reply encoders
// (source/sm_request.hpp): each request form that libounce-nx's SM client
// sends, malformed requests, and random messages.

#include <cstdio>
#include <cstring>
#include <random>
#include "../source/sm_request.hpp"

using namespace ounce::sm;

namespace {

int g_failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        g_failures++;
    }
}

SmServiceName name(const char* text) {
    SmServiceName out = {};
    std::strncpy(out.name, text, sizeof(out.name));
    return out;
}

bool sameName(const SmServiceName& a, const char* text) {
    const SmServiceName b = name(text);
    return std::memcmp(a.name, b.name, sizeof(a.name)) == 0;
}

/// A CMIF request with `size` bytes of arguments.
void cmifRequest(u8* message, u32 type, u32 command, const void* arguments, size_t size,
                 bool send_pid = false, u32 num_copy_handles = 0, u32 num_send_buffers = 0) {
    std::memset(message, 0, MessageSize);
    const u32 num_data_words = static_cast<u32>((0x10 + sizeof(CmifInHeader) + size + 3) / 4);
    HipcRequest hipc = hipcMakeRequestInline(message,
        .type = type,
        .num_send_buffers = num_send_buffers,
        .num_data_words = num_data_words,
        .send_pid = send_pid,
        .num_copy_handles = num_copy_handles,
    );
    auto* header = static_cast<CmifInHeader*>(cmifGetAlignedDataStart(hipc.data_words, message));
    header->magic = CMIF_IN_HEADER_MAGIC;
    header->command_id = command;
    if (size)
        std::memcpy(header + 1, arguments, size);
}

/// A TIPC request with `size` bytes (whole words) of arguments.
void tipcRequest(u8* message, u32 command, const void* arguments, size_t size,
                 bool send_pid = false) {
    std::memset(message, 0, MessageSize);
    HipcRequest hipc = hipcMakeRequestInline(message,
        .type = TipcRequestTypeBase + command,
        .num_data_words = static_cast<u32>(size / 4),
        .send_pid = send_pid,
    );
    if (size)
        std::memcpy(hipc.data_words, arguments, size);
}

void testGetService() {
    alignas(16) u8 message[MessageSize];
    const SmServiceName fsp = name("fsp-srv");
    cmifRequest(message, CmifCommandType_Request, 1, &fsp, sizeof(fsp));
    Request r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.protocol == Protocol::Cmif &&
           r.command == Command::GetServiceHandle && sameName(r.name, "fsp-srv"),
           "CMIF GetServiceHandle");

    const SmServiceName full = {{'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'}};
    cmifRequest(message, CmifCommandType_Request, 1, &full, sizeof(full));
    expect(decodeRequest(message).kind == RequestKind::Command, "eight-character name");

    const SmServiceName empty = {};
    cmifRequest(message, CmifCommandType_Request, 1, &empty, sizeof(empty));
    expect(decodeRequest(message).kind == RequestKind::Invalid, "empty name");

    const SmServiceName gap = {{'a', '\0', 'b'}};
    cmifRequest(message, CmifCommandType_Request, 1, &gap, sizeof(gap));
    expect(decodeRequest(message).kind == RequestKind::Invalid, "character after NUL");

    // CMIF pads the arguments to its alignment, so a short name cannot be
    // told from a padded one (SM cannot either). The decoder must only stay
    // inside the message buffer, which ASan checks.
    cmifRequest(message, CmifCommandType_Request, 1, &fsp, 4);
    (void)decodeRequest(message);

    tipcRequest(message, 1, &fsp, sizeof(fsp));
    r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.protocol == Protocol::Tipc &&
           sameName(r.name, "fsp-srv"), "TIPC GetServiceHandle");

    const u8 twelve[12] = {'a'};
    tipcRequest(message, 1, twelve, sizeof(twelve));
    expect(decodeRequest(message).kind == RequestKind::Invalid, "TIPC name of the wrong size");
}

void testRegisterService() {
    alignas(16) u8 message[MessageSize];
    struct {
        SmServiceName name;
        u8 is_light;
        s32 max_sessions;
    } cmif_in = {name("mysrv"), 1, 7};
    cmifRequest(message, CmifCommandType_Request, 2, &cmif_in, sizeof(cmif_in));
    Request r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.command == Command::RegisterService &&
           sameName(r.name, "mysrv") && r.is_light && r.max_sessions == 7 && !r.has_space_set,
           "CMIF RegisterService");

    struct {
        SmServiceName name;
        s32 max_sessions;
        u8 is_light;
    } tipc_in = {name("mysrv"), 9, 0};
    tipcRequest(message, 2, &tipc_in, sizeof(tipc_in));
    r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.protocol == Protocol::Tipc &&
           sameName(r.name, "mysrv") && !r.is_light && r.max_sessions == 9 && !r.has_space_set,
           "TIPC RegisterService");

    struct {
        u32 space_set;
        SmServiceName name;
        s32 max_sessions;
        u8 is_light;
    } space_in = {3, name("mig:usr"), 1, 1};
    static_assert(sizeof(space_in) == 20);
    tipcRequest(message, 2, &space_in, sizeof(space_in));
    r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.has_space_set && r.space_set == 3 &&
           sameName(r.name, "mig:usr") && r.max_sessions == 1 && r.is_light,
           "TIPC RegisterService with a space set");

    const u8 odd[24] = {'a'};
    tipcRequest(message, 2, odd, sizeof(odd));
    expect(decodeRequest(message).kind == RequestKind::Invalid, "TIPC RegisterService of the wrong size");
}

void testUnregisterService() {
    alignas(16) u8 message[MessageSize];
    const SmServiceName srv = name("mysrv");
    cmifRequest(message, CmifCommandType_Request, 3, &srv, sizeof(srv));
    Request r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.command == Command::UnregisterService &&
           sameName(r.name, "mysrv"), "CMIF UnregisterService");

    tipcRequest(message, 3, &srv, sizeof(srv));
    r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && !r.has_space_set, "TIPC UnregisterService");

    struct {
        u32 space_set;
        SmServiceName name;
    } space_in = {2, name("mig:usr")};
    static_assert(sizeof(space_in) == 12);
    tipcRequest(message, 3, &space_in, sizeof(space_in));
    r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.has_space_set && r.space_set == 2 &&
           sameName(r.name, "mig:usr"), "TIPC UnregisterService with a space set");
}

void testClientCommands() {
    alignas(16) u8 message[MessageSize];
    const u64 pid_placeholder = 0;
    cmifRequest(message, CmifCommandType_Request, 0, &pid_placeholder, sizeof(pid_placeholder), true);
    Request r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.command == Command::RegisterClient,
           "CMIF RegisterClient with the PID");

    tipcRequest(message, 4, nullptr, 0, true);
    r = decodeRequest(message);
    expect(r.kind == RequestKind::Command && r.command == Command::DetachClient,
           "TIPC DetachClient");
}

void testOtherRequests() {
    alignas(16) u8 message[MessageSize];
    const SmServiceName srv = name("fsp-srv");

    cmifRequest(message, CmifCommandType_Request, 1, &srv, sizeof(srv), false, 1);
    expect(decodeRequest(message).kind == RequestKind::Invalid, "request with a copied handle");

    cmifRequest(message, CmifCommandType_Request, 1, &srv, sizeof(srv), false, 0, 1);
    expect(decodeRequest(message).kind == RequestKind::Invalid, "request with a buffer");

    cmifRequest(message, CmifCommandType_Close, 0, nullptr, 0);
    expect(decodeRequest(message).kind == RequestKind::Close, "CMIF Close");

    std::memset(message, 0, sizeof(message));
    hipcMakeRequestInline(message, .type = TipcCloseType);
    expect(decodeRequest(message).kind == RequestKind::Close, "TIPC Close");

    cmifRequest(message, CmifCommandType_Control, 3, nullptr, 0);
    expect(decodeRequest(message).kind == RequestKind::QueryPointerBufferSize,
           "QueryPointerBufferSize");

    cmifRequest(message, CmifCommandType_Control, 0, nullptr, 0);
    expect(decodeRequest(message).kind == RequestKind::UnsupportedCommand,
           "ConvertCurrentObjectToDomain");

    cmifRequest(message, CmifCommandType_Request, 7, &srv, sizeof(srv));
    expect(decodeRequest(message).kind == RequestKind::UnsupportedCommand, "CMIF command 7");

    tipcRequest(message, 9, &srv, sizeof(srv));
    expect(decodeRequest(message).kind == RequestKind::UnsupportedCommand, "TIPC command 9");

    cmifRequest(message, CmifCommandType_Request, 1, &srv, sizeof(srv));
    auto* header = static_cast<CmifInHeader*>(cmifGetAlignedDataStart(
        hipcParseRequest(message).data.data_words, message));
    header->magic = CMIF_OUT_HEADER_MAGIC;
    expect(decodeRequest(message).kind == RequestKind::Invalid, "wrong CMIF magic");

    // A data word count past the end of the message buffer.
    std::memset(message, 0, sizeof(message));
    hipcMakeRequestInline(message, .type = TipcRequestTypeBase + 1, .num_data_words = 1023);
    expect(decodeRequest(message).kind == RequestKind::Invalid, "data past the message buffer");

    // HIPC types that are neither CMIF nor TIPC requests.
    for (u32 type : {0u, 1u, 3u, 7u, 14u}) {
        std::memset(message, 0, sizeof(message));
        hipcMakeRequestInline(message, .type = type, .num_data_words = 8);
        expect(decodeRequest(message).kind == RequestKind::Invalid, "other HIPC type");
    }
}

void testReplies() {
    alignas(16) u8 message[MessageSize];
    writeReply(message, Protocol::Tipc, 0x1234, 0x5678);
    HipcParsedRequest parsed = hipcParseRequest(message);
    expect(parsed.meta.num_move_handles == 1 && parsed.data.move_handles[0] == 0x5678 &&
           parsed.meta.num_data_words == 1 && parsed.data.data_words[0] == 0x1234,
           "TIPC reply with a moved handle");

    writeReply(message, Protocol::Cmif, 0xABCD);
    parsed = hipcParseRequest(message);
    const auto* header = static_cast<const CmifOutHeader*>(
        cmifGetAlignedDataStart(parsed.data.data_words, message));
    expect(parsed.meta.num_move_handles == 0 && header->magic == CMIF_OUT_HEADER_MAGIC &&
           header->result == 0xABCD, "CMIF reply");

    writePointerBufferSizeReply(message);
    parsed = hipcParseRequest(message);
    header = static_cast<const CmifOutHeader*>(cmifGetAlignedDataStart(parsed.data.data_words, message));
    u16 size = 1;
    std::memcpy(&size, header + 1, sizeof(size));
    expect(header->result == 0 && size == 0, "QueryPointerBufferSize reply");
}

/// Random messages: the decoder must not read outside the buffer (ASan) and
/// must give a known kind.
void testRandomMessages() {
    std::mt19937 random(12345);
    alignas(16) u8 message[MessageSize];
    unsigned commands = 0;
    for (int round = 0; round < 200000; round++) {
        for (u8& byte : message)
            byte = static_cast<u8>(random());
        // Make a share of them requests with random arguments (a clean
        // HIPC header: no buffers, handles or PID), so the deeper paths run.
        if (round % 2) {
            auto* words = reinterpret_cast<u32*>(message);
            const u32 type = round % 4 == 1
                ? static_cast<u32>(CmifCommandType_Request)
                : TipcRequestTypeBase + static_cast<u32>(random() % 6);
            words[0] = type;
            words[1] = random() % 12; // data words, no special header
            if (type == CmifCommandType_Request) {
                words[1] = 8 + random() % 4;
                auto* header = static_cast<CmifInHeader*>(cmifGetAlignedDataStart(
                    hipcParseRequest(message).data.data_words, message));
                header->magic = CMIF_IN_HEADER_MAGIC;
                header->version = 0;
                header->command_id = random() % 6;
            }
        }
        const Request r = decodeRequest(message);
        commands += r.kind == RequestKind::Command;
        expect(r.kind >= RequestKind::Command && r.kind <= RequestKind::Invalid, "known kind");
    }
    std::printf("random messages: %u decoded as commands\n", commands);
}

} // namespace

int main() {
    testGetService();
    testRegisterService();
    testUnregisterService();
    testClientCommands();
    testOtherRequests();
    testReplies();
    testRandomMessages();
    if (g_failures) {
        std::printf("sm request test: FAIL (%d)\n", g_failures);
        return 1;
    }
    std::printf("sm request test: PASS\n");
    return 0;
}
