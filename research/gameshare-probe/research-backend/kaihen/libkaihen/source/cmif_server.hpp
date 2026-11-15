// SPDX-License-Identifier: ISC
// The server side of a CMIF session, for the plugin runtime and HEN: parse
// the request that svcReplyAndReceive left in the thread's message buffer,
// write the reply into the same buffer, and send it. libounce-nx has only the
// client side (sf/service.h).
#pragma once

#include <cstring>
#include <switch.h>

namespace ounce {

/// Size of the IPC message buffer at the start of the thread's TLS.
constexpr size_t MessageSize = 0x100;

/// CMIF control command 3, QueryPointerBufferSize: the client asks for the
/// size of the server's pointer buffer. The plugin servers have none.
constexpr u32 CmifControl_QueryPointerBufferSize = 3;

/// A request in the message buffer.
struct CmifRequest {
    HipcParsedRequest hipc;
    bool valid;          ///< The CMIF header is complete and has the request magic.
    u32 command_id;
    const u8* data;      ///< The bytes after the CMIF header.
    size_t data_size;    ///< Rounded up to whole words, so it can include padding.
};

/// Closes a handle that is open and marks it closed.
inline void closeHandle(Handle* handle) {
    if (*handle != INVALID_HANDLE) {
        svcCloseHandle(*handle);
        *handle = INVALID_HANDLE;
    }
}

inline CmifRequest parseRequest(void* message) {
    CmifRequest request = {};
    request.hipc = hipcParseRequest(message);

    const auto* header = static_cast<const CmifInHeader*>(
        cmifGetAlignedDataStart(request.hipc.data.data_words, message));
    const auto* data = reinterpret_cast<const u8*>(header + 1);
    const auto* end = reinterpret_cast<const u8*>(
        request.hipc.data.data_words + request.hipc.meta.num_data_words);
    const auto* message_end = static_cast<const u8*>(message) + MessageSize;

    request.valid = end <= message_end && data <= end &&
        header->magic == CMIF_IN_HEADER_MAGIC && header->version <= 1;
    if (request.valid) {
        request.command_id = header->command_id;
        request.data = data;
        request.data_size = static_cast<size_t>(end - data);
    }
    return request;
}

/// True for a request with inline data only: no buffers and no handles.
inline bool hasOnlyInlineData(const CmifRequest& request) {
    const HipcMetadata& meta = request.hipc.meta;
    return !meta.num_send_statics && !meta.num_recv_statics &&
        !meta.num_send_buffers && !meta.num_recv_buffers &&
        !meta.num_exch_buffers && !meta.num_copy_handles &&
        !meta.num_move_handles;
}

inline bool isQueryPointerBufferSize(const CmifRequest& request) {
    return request.valid && request.hipc.meta.type == CmifCommandType_Control &&
        request.command_id == CmifControl_QueryPointerBufferSize &&
        hasOnlyInlineData(request);
}

/// Closes the handles that a request carried. Call it unless the server
/// keeps them.
inline void closeRequestHandles(const CmifRequest& request) {
    for (u32 index = 0; index < request.hipc.meta.num_copy_handles; index++) {
        svcCloseHandle(request.hipc.data.copy_handles[index]);
    }
    for (u32 index = 0; index < request.hipc.meta.num_move_handles; index++) {
        svcCloseHandle(request.hipc.data.move_handles[index]);
    }
}

/// Writes a reply into the message buffer: the result, `size` bytes of data
/// after the CMIF header, and copies of the given handles.
inline void writeReply(void* message, Result result, const void* data, size_t size,
                       const Handle* copy_handles = nullptr, u32 num_copy_handles = 0) {
    std::memset(message, 0, MessageSize);

    // The CMIF header and the data, in whole words, after up to 16 bytes of
    // alignment padding.
    const u32 num_data_words = static_cast<u32>((0x10 + sizeof(CmifOutHeader) + size + 3) / 4);
    HipcRequest reply = hipcMakeRequestInline(message,
        .num_data_words = num_data_words,
        .num_copy_handles = num_copy_handles,
    );
    for (u32 index = 0; index < num_copy_handles; index++) {
        reply.copy_handles[index] = copy_handles[index];
    }

    auto* header = static_cast<CmifOutHeader*>(
        cmifGetAlignedDataStart(reply.data_words, message));
    header->magic = CMIF_OUT_HEADER_MAGIC;
    header->result = result;
    if (size) {
        std::memcpy(header + 1, data, size);
    }
}

/// Replies to a QueryPointerBufferSize request: the size is 0.
inline void writePointerBufferSizeReply(void* message) {
    const u16 pointer_buffer_size = 0;
    writeReply(message, 0, &pointer_buffer_size, sizeof(pointer_buffer_size));
}

/// Sends the reply in the message buffer on the session.
inline Result sendReply(Handle session) {
    s32 index = -1;
    // With nothing to wait for, the call returns TimedOut once the reply is sent.
    const Result rc = svcReplyAndReceive(&index, nullptr, 0, session, 0);
    return rc == KERNELRESULT(TimedOut) ? 0 : rc;
}

} // namespace ounce
