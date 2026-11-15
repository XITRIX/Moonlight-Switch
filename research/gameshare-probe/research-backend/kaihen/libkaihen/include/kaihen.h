/* SPDX-License-Identifier: ISC
 * Copyright 2026 kaiHEN contributors
 *
 * Client functions for the kaiHEN services that Switch 2 homebrew can use.
 *
 * An NRO that hbloader starts gets RO's ldr:ro session as a service
 * override. On it, the NRO can open a HEN homebrew session:
 *
 *     Service ro, hen;
 *     HenStatus status;
 *     Result rc = kaihenGetRoSession(&ro);
 *     if (R_SUCCEEDED(rc))
 *         rc = kaihenOpenHomebrewSession(&ro, &hen);
 *     if (R_SUCCEEDED(rc)) {
 *         rc = henGetStatus(&hen, &status);
 *         serviceClose(&hen);
 *     }
 *
 * libounce's Jit API uses RO's JIT commands through the hooks in libkaihen
 * (link -lkaihen). A program does not send those commands itself.
 */
#pragma once

#include <switch.h>

#include "kaihen/protocol.h"

/// Gets the ldr:ro session that hbloader passes to the NRO. The Service does
/// not own the handle: do not close it. Returns LibnxError_NotFound when the
/// program did not start from hbloader.
NX_INLINE Result kaihenGetRoSession(Service* out) {
    const Handle session = smGetServiceOverride(smEncodeName("ldr:ro"));
    if (session == INVALID_HANDLE)
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    serviceCreate(out, session);
    out->own_handle = 0;
    return 0;
}

/// Retains a writable RO view of an executable JIT mapping. The caller
/// keeps the RX mapping alive and later closes it with jitClose. The returned
/// address belongs to RO and is useful only for diagnostics.
NX_INLINE Result kaihenMapJitAlias(Jit* code, KaihenJitAliasInfo* out_info) {
    Service ro;
    Result rc = kaihenGetRoSession(&ro);
    if (R_FAILED(rc))
        return rc;
    const struct {
        u64 pid_placeholder;
        u64 destination;
        u64 size;
    } in = {0, (u64)jitGetRxAddr(code), code->size};
    return serviceDispatchInOut(&ro, KaihenRoCommand_MapJitAlias, in, *out_info,
                                .in_send_pid = true);
}

/// Copies staged bytes into the retained RO alias and cleans that alias's
/// data cache. After success, invalidate the matching RX instruction range
/// in the game before executing it.
NX_INLINE Result kaihenPublishJitAlias(Jit* code, size_t offset,
                                         const void* bytes, size_t size) {
    Service ro;
    Result rc = kaihenGetRoSession(&ro);
    if (R_FAILED(rc))
        return rc;
    const struct {
        u64 pid_placeholder;
        u64 destination;
        u64 offset;
        u64 size;
    } in = {0, (u64)jitGetRxAddr(code), offset, size};
    return serviceDispatchIn(&ro, KaihenRoCommand_PublishJitAlias, in,
                             .buffer_attrs = {SfBufferAttr_HipcMapAlias | SfBufferAttr_In},
                             .buffers = {{bytes, size}},
                             .in_send_pid = true);
}

/**
 * @brief Publishes staged JIT ranges in one RO request.
 * @param code JIT object using kaiHEN's persistent alias backend.
 * @param ranges Byte ranges within jitGetRwAddr(code), in publication order.
 * @param count Number of ranges.
 * @note This invalidates the calling thread's RX instruction ranges after RO
 *       has cleaned its writes. Other executing threads must synchronize and
 *       invalidate before entering updated code. Do not modify a running block.
 */
#ifdef __cplusplus
extern "C" {
#endif
Result kaihenJitPublishRanges(Jit* code, const KaihenJitRange* ranges, size_t count);
#ifdef __cplusplus
}
#endif

/// Opens a new HEN homebrew session through RO. The caller owns *out and
/// closes it with serviceClose.
NX_INLINE Result kaihenOpenHomebrewSession(Service* ro, Service* out) {
    Handle session = INVALID_HANDLE;
    const u64 pid_placeholder = 0;
    const Result rc = serviceDispatchIn(ro, KaihenRoCommand_GetHomebrewSession,
        pid_placeholder,
        .in_send_pid = true,
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = &session,
    );
    if (R_SUCCEEDED(rc))
        serviceCreate(out, session);
    return rc;
}

/// Gets HEN's status. The caller keeps ownership of the session.
NX_INLINE Result henGetStatus(Service* s, HenStatus* out) {
    return serviceDispatchOut(s, HenHomebrewCommand_GetStatus, *out);
}

/// Gets a new clone of HEN's FS proxy root session. The caller owns the
/// handle.
NX_INLINE Result henGetFilesystemRoot(Service* s, Handle* out) {
    return serviceDispatch(s, HenHomebrewCommand_GetFilesystemRoot,
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = out,
    );
}

/// Gets a new clone of HEN's set:sys session. The caller owns the handle.
NX_INLINE Result henGetSettingsSession(Service* s, Handle* out) {
    return serviceDispatch(s, HenHomebrewCommand_GetSettingsSession,
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = out,
    );
}

/// Gets a COPY of the calling program's process handle. The caller owns it.
NX_INLINE Result henGetProcessHandle(Service* s, Handle* out) {
    return serviceDispatch(s, HenHomebrewCommand_GetProcessHandle,
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = out,
    );
}

/// Gets a borrowed FS-identity SM session by COPY. The caller owns its
/// kernel handle but must not send SM Initialize or Close on this session.
NX_INLINE Result henGetServiceManager(Service* s, Handle* out) {
    return serviceDispatch(s, HenHomebrewCommand_GetServiceManager,
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = out,
    );
}

/// Returns the number of plugins that published an IPC service.
NX_INLINE Result henGetPluginCount(Service* s, u32* out_count) {
    return serviceDispatchOut(s, HenHomebrewCommand_GetPluginCount, *out_count);
}

/// Returns the name at index. The name is NUL-terminated.
NX_INLINE Result henGetPluginName(Service* s, u32 index, HenPluginName* out_name) {
    return serviceDispatchInOut(s, HenHomebrewCommand_GetPluginName, index, *out_name);
}

/// Opens a fresh session to a published plugin. The caller closes it.
NX_INLINE Result henOpenPlugin(Service* s, const HenPluginName* name, Service* out) {
    Handle session = INVALID_HANDLE;
    Result rc = serviceDispatchIn(s, HenHomebrewCommand_OpenPlugin, *name,
        .out_handle_attrs = {SfOutHandleAttr_HipcCopy},
        .out_handles = &session);
    if (R_SUCCEEDED(rc))
        serviceCreate(out, session);
    return rc;
}
