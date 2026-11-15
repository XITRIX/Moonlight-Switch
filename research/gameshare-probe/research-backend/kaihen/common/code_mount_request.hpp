// SPDX-License-Identifier: GPL-2.0-only
#pragma once
// Loader's OpenCodeFileSystem request, as HEN and the bootstrap see it in
// Loader's message buffer. Only the libnx types, HIPC and CMIF headers, so
// that the decoder also builds in host tests.
extern "C" {
#include <switch/types.h>
#include <switch/result.h>
#include <switch/sf/hipc.h>
#include <switch/sf/cmif.h>
}

namespace ounce {

/// Size of a request that Loader's server receives.
constexpr size_t LoaderRequestSize = 0x100;

/// The arguments of OpenCodeFileSystem (fsp-ldr command 0), after the CMIF
/// header of a request on Loader's domain root.
struct CodeMountArguments {
    u8 content_attributes;
    u8 storage_id;
    u8 padding[6];
    u64 program_id;
};
static_assert(sizeof(CodeMountArguments) == 16);

/// A request that Loader received, decoded as far as HEN and the bootstrap
/// need it.
struct CodeMountRequest {
    /// True when the request is OpenCodeFileSystem on the domain root object
    /// root_id; the arguments are then valid.
    bool open_code;
    CodeMountArguments arguments;
};

/// The fsp-ldr command that opens a program's code file system.
constexpr u32 FsProxyForLoaderCommand_OpenCodeFileSystem = 0;

/// Decodes a request in Loader's message buffer. A request that is not a
/// domain message to root_id, or not OpenCodeFileSystem, is not an error:
/// the result is success with open_code false. A request too short for its
/// arguments fails.
inline Result decodeCodeMountRequest(const u8 request[LoaderRequestSize], u64 root_id,
                                     CodeMountRequest& out) {
    out = {};
    u8* message = const_cast<u8*>(request);
    const HipcParsedRequest parsed = hipcParseRequest(message);
    if (parsed.meta.type != CmifCommandType_Request &&
        parsed.meta.type != CmifCommandType_RequestWithContext) {
        return 0;
    }

    const u8* data = static_cast<const u8*>(
        cmifGetAlignedDataStart(parsed.data.data_words, message));
    if (data + sizeof(CmifDomainInHeader) + sizeof(CmifInHeader) +
            sizeof(CodeMountArguments) > request + LoaderRequestSize) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    CmifDomainInHeader domain;
    CmifInHeader header;
    __builtin_memcpy(&domain, data, sizeof(domain));
    __builtin_memcpy(&header, data + sizeof(domain), sizeof(header));
    if (domain.type != CmifDomainRequestType_SendMessage || domain.object_id != root_id ||
        header.magic != CMIF_IN_HEADER_MAGIC ||
        header.command_id != FsProxyForLoaderCommand_OpenCodeFileSystem) {
        return 0;
    }
    __builtin_memcpy(&out.arguments, data + sizeof(domain) + sizeof(header),
                     sizeof(out.arguments));
    out.open_code = true;
    return 0;
}

}
