// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// The requests that HEN's SM proxy (sm_proxy.cpp) accepts from a homebrew
// session, decoded from the message buffer, and the replies it writes. Only
// the libnx types, HIPC, CMIF and TIPC headers, so that the decoder also
// builds in host tests.
//
// The request layouts are those of libounce-nx's SM client
// (source/services/sm.c): GetServiceHandle through CMIF, the other commands
// through CMIF or TIPC, and the Switch 2 TIPC forms of RegisterService and
// UnregisterService that carry a service space set.
extern "C" {
#include <switch/types.h>
#include <switch/result.h>
#include <switch/sf/hipc.h>
#include <switch/sf/cmif.h>
#include <switch/services/sm.h>
}

#include <cstring>

namespace ounce::sm {

/// Size of the IPC message buffer at the start of the thread's TLS.
constexpr size_t MessageSize = 0x100;

/// SM commands, the same IDs in CMIF and TIPC.
enum class Command : u32 {
    RegisterClient = 0,
    GetServiceHandle = 1,
    RegisterService = 2,
    UnregisterService = 3,
    DetachClient = 4,
};

enum class Protocol { Cmif, Tipc };

enum class RequestKind {
    Command,                ///< A known SM command with valid arguments.
    UnsupportedCommand,     ///< A well-formed request for another command.
    QueryPointerBufferSize, ///< The CMIF control request.
    Close,                  ///< The client closes the session.
    Invalid,                ///< Anything else: the proxy closes the session.
};

/// HIPC message type of a TIPC request: the command ID plus this.
constexpr u32 TipcRequestTypeBase = 16;
constexpr u32 TipcCloseType = 15;
constexpr u32 CmifControl_QueryPointerBufferSize = 3;

struct Request {
    RequestKind kind = RequestKind::Invalid;
    Protocol protocol = Protocol::Cmif;
    Command command = Command::RegisterClient;
    SmServiceName name = {};
    /// The Switch 2 TIPC RegisterService and UnregisterService forms.
    bool has_space_set = false;
    u32 space_set = 0;
    bool is_light = false;
    s32 max_sessions = 0;
};

/// SM's rule for a service name: a nonempty name, and after the first NUL,
/// only NULs.
inline bool isValidServiceName(const SmServiceName& name) {
    if (name.name[0] == '\0')
        return false;
    bool ended = false;
    for (char c : name.name) {
        if (c == '\0')
            ended = true;
        else if (ended)
            return false;
    }
    return true;
}

namespace detail {

/// The data of a request, after the CMIF header or (TIPC) after the HIPC
/// header.
struct Payload {
    const u8* data = nullptr;
    size_t size = 0;
};

inline bool carriesBuffersOrHandles(const HipcMetadata& meta) {
    return meta.num_send_statics || meta.num_recv_statics || meta.num_send_buffers ||
        meta.num_recv_buffers || meta.num_exch_buffers || meta.num_copy_handles ||
        meta.num_move_handles;
}

template <typename T>
T readAt(const Payload& payload, size_t offset) {
    T value;
    std::memcpy(&value, payload.data + offset, sizeof(T));
    return value;
}

inline bool decodeArguments(Request& request, const Payload& payload) {
    const bool tipc = request.protocol == Protocol::Tipc;
    switch (request.command) {
    case Command::RegisterClient:
    case Command::DetachClient:
        // The proxy answers these itself; their arguments do not matter.
        return true;
    case Command::GetServiceHandle:
        if (tipc ? payload.size != 8 : payload.size < 8)
            return false;
        request.name = readAt<SmServiceName>(payload, 0);
        return isValidServiceName(request.name);
    case Command::RegisterService:
        if (!tipc) {
            // name, u8 is_light, padding, s32 max_sessions
            if (payload.size < 16)
                return false;
            request.name = readAt<SmServiceName>(payload, 0);
            request.is_light = readAt<u8>(payload, 8) != 0;
            request.max_sessions = readAt<s32>(payload, 12);
        } else if (payload.size == 16) {
            // name, s32 max_sessions, u8 is_light
            request.name = readAt<SmServiceName>(payload, 0);
            request.max_sessions = readAt<s32>(payload, 8);
            request.is_light = readAt<u8>(payload, 12) != 0;
        } else if (payload.size == 20) {
            // u32 space_set, name, s32 max_sessions, u8 is_light
            request.has_space_set = true;
            request.space_set = readAt<u32>(payload, 0);
            request.name = readAt<SmServiceName>(payload, 4);
            request.max_sessions = readAt<s32>(payload, 12);
            request.is_light = readAt<u8>(payload, 16) != 0;
        } else {
            return false;
        }
        return isValidServiceName(request.name);
    case Command::UnregisterService:
        if (!tipc) {
            if (payload.size < 8)
                return false;
            request.name = readAt<SmServiceName>(payload, 0);
        } else if (payload.size == 8) {
            request.name = readAt<SmServiceName>(payload, 0);
        } else if (payload.size == 12) {
            request.has_space_set = true;
            request.space_set = readAt<u32>(payload, 0);
            request.name = readAt<SmServiceName>(payload, 4);
        } else {
            return false;
        }
        return isValidServiceName(request.name);
    }
    return false;
}

} // namespace detail

/// Decodes the request in a message buffer of MessageSize bytes. The proxy
/// never sends these bytes on: it builds its own request to SM from the
/// decoded fields.
inline Request decodeRequest(const void* message) {
    Request request;
    const HipcParsedRequest hipc = hipcParseRequest(const_cast<void*>(message));
    const auto* begin = static_cast<const u8*>(message);
    const auto* end = reinterpret_cast<const u8*>(hipc.data.data_words + hipc.meta.num_data_words);
    if (end > begin + MessageSize)
        return request;

    const u32 type = hipc.meta.type;
    if (type == CmifCommandType_Close || type == TipcCloseType) {
        request.kind = RequestKind::Close;
        return request;
    }
    // A request that carries buffers or handles is not one of SM's.
    if (detail::carriesBuffersOrHandles(hipc.meta))
        return request;

    detail::Payload payload;
    u32 command_id = 0;
    if (type == CmifCommandType_Request || type == CmifCommandType_RequestWithContext ||
        type == CmifCommandType_Control) {
        const auto* header = static_cast<const CmifInHeader*>(
            cmifGetAlignedDataStart(hipc.data.data_words, const_cast<void*>(message)));
        const auto* data = reinterpret_cast<const u8*>(header + 1);
        if (data > end || header->magic != CMIF_IN_HEADER_MAGIC || header->version > 1)
            return request;
        payload = {data, static_cast<size_t>(end - data)};
        command_id = header->command_id;
        request.protocol = Protocol::Cmif;
        if (type == CmifCommandType_Control) {
            if (command_id == CmifControl_QueryPointerBufferSize)
                request.kind = RequestKind::QueryPointerBufferSize;
            else
                request.kind = RequestKind::UnsupportedCommand;
            return request;
        }
    } else if (type >= TipcRequestTypeBase) {
        payload = {reinterpret_cast<const u8*>(hipc.data.data_words),
                   hipc.meta.num_data_words * sizeof(u32)};
        command_id = type - TipcRequestTypeBase;
        request.protocol = Protocol::Tipc;
    } else {
        return request;
    }

    if (command_id > static_cast<u32>(Command::DetachClient)) {
        request.kind = RequestKind::UnsupportedCommand;
        return request;
    }
    request.command = static_cast<Command>(command_id);
    if (!detail::decodeArguments(request, payload))
        return request;
    request.kind = RequestKind::Command;
    return request;
}

/// Writes a reply into the message buffer, in the request's protocol: the
/// result, and the handle (if any) moved to the client.
inline void writeReply(void* message, Protocol protocol, Result result,
                       Handle move_handle = INVALID_HANDLE) {
    std::memset(message, 0, MessageSize);
    const u32 num_move_handles = move_handle != INVALID_HANDLE ? 1 : 0;
    if (protocol == Protocol::Tipc) {
        // TIPC: the result is the first data word.
        HipcRequest reply = hipcMakeRequestInline(message,
            .num_data_words = 1,
            .num_move_handles = num_move_handles,
        );
        reply.data_words[0] = result;
        if (num_move_handles)
            reply.move_handles[0] = move_handle;
        return;
    }
    // CMIF: the output header after up to 16 bytes of alignment padding.
    const u32 num_data_words = static_cast<u32>((0x10 + sizeof(CmifOutHeader) + 3) / 4);
    HipcRequest reply = hipcMakeRequestInline(message,
        .num_data_words = num_data_words,
        .num_move_handles = num_move_handles,
    );
    if (num_move_handles)
        reply.move_handles[0] = move_handle;
    auto* header = static_cast<CmifOutHeader*>(cmifGetAlignedDataStart(reply.data_words, message));
    header->magic = CMIF_OUT_HEADER_MAGIC;
    header->result = result;
}

/// Replies to QueryPointerBufferSize: the proxy has no pointer buffer.
inline void writePointerBufferSizeReply(void* message) {
    std::memset(message, 0, MessageSize);
    const u16 size = 0;
    const u32 num_data_words = static_cast<u32>((0x10 + sizeof(CmifOutHeader) + sizeof(size) + 3) / 4);
    HipcRequest reply = hipcMakeRequestInline(message, .num_data_words = num_data_words);
    auto* header = static_cast<CmifOutHeader*>(cmifGetAlignedDataStart(reply.data_words, message));
    header->magic = CMIF_OUT_HEADER_MAGIC;
    header->result = 0;
    std::memcpy(header + 1, &size, sizeof(size));
}

} // namespace ounce::sm
