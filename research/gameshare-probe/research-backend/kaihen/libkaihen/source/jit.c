/* SPDX-License-Identifier: ISC
 * Copyright 2026 kaiHEN contributors
 *
 * kaiHEN's code mapping for libounce's Jit API.
 *
 * libounce's jit.c calls these weak hooks on Switch 2. The game's policy
 * does not allow the code mapping syscalls, so RO makes them with the
 * process handle of its ldr:ro session. hbloader passes that session to
 * each NRO as the ldr:ro service override; it is already initialized for
 * this process. RO records each mapping, and hbloader unmaps what an NRO
 * leaves mapped when the NRO returns.
 */
#include <kaihen.h>
#include <stdlib.h>
#include <string.h>

static Service g_ro_service;
static Mutex g_ro_service_mutex;

// The ldr:ro session from hbloader. The service does not own the handle.
static Result _kaihenJitGetRoService(Service** out_service)
{
    mutexLock(&g_ro_service_mutex);
    if (!serviceIsActive(&g_ro_service))
        kaihenGetRoSession(&g_ro_service);
    const bool active = serviceIsActive(&g_ro_service);
    mutexUnlock(&g_ro_service_mutex);

    if (!active)
        return MAKERESULT(Module_Libnx, LibnxError_JitUnavailable);

    *out_service = &g_ro_service;
    return 0;
}

static Result _kaihenJitDispatch(u32 command_id, void* rx_addr, void* src_addr, size_t size)
{
    Service* service = NULL;
    Result rc = _kaihenJitGetRoService(&service);
    if (R_FAILED(rc))
        return rc;

    const struct {
        u64 pid_placeholder;
        u64 destination;
        u64 source;
        u64 size;
    } in = { 0, (u64)rx_addr, (u64)src_addr, size };

    return serviceDispatchIn(service, command_id, in, .in_send_pid = true);
}

Result __nx_jit_map_code(void* rx_addr, void* src_addr, size_t size)
{
    return _kaihenJitDispatch(KaihenRoCommand_MapJitCode, rx_addr, src_addr, size);
}

Result __nx_jit_unmap_code(void* rx_addr, void* src_addr, size_t size)
{
    return _kaihenJitDispatch(KaihenRoCommand_UnmapJitCode, rx_addr, src_addr, size);
}

Result __nx_jit_map_alias(void* rx_addr, size_t size)
{
    Service* service = NULL;
    Result rc = _kaihenJitGetRoService(&service);
    if (R_FAILED(rc))
        return rc;

    const struct {
        u64 pid_placeholder;
        u64 destination;
        u64 size;
    } in = { 0, (u64)rx_addr, size };
    KaihenJitAliasInfo info = {0};
    return serviceDispatchInOut(service, KaihenRoCommand_MapJitAlias, in, info,
                                .in_send_pid = true);
}

Result __nx_jit_publish_alias(void* rx_addr, const void* data, size_t size)
{
    Service* service = NULL;
    Result rc = _kaihenJitGetRoService(&service);
    if (R_FAILED(rc))
        return rc;

    const struct {
        u64 pid_placeholder;
        u64 destination;
        u64 offset;
        u64 size;
    } in = { 0, (u64)rx_addr, 0, size };
    return serviceDispatchIn(service, KaihenRoCommand_PublishJitAlias, in,
                             .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
                             .buffers = { { data, size } },
                             .in_send_pid = true);
}

Result kaihenJitPublishRanges(Jit* code, const KaihenJitRange* ranges, size_t count)
{
    if (!count)
        return 0;
    if (!code || !ranges || code->rw_addr == code->src_addr)
        return MAKERESULT(Module_Libnx, LibnxError_JitUnavailable);
    if (count > SIZE_MAX / sizeof(*ranges))
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    KaihenJitRange* merged = malloc(count * sizeof(*merged));
    if (!merged)
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

    size_t merged_count = 0;
    size_t total = 0;
    Result rc = 0;
    for (size_t i = 0; i < count; i++) {
        const KaihenJitRange range = ranges[i];
        if (!range.size || range.offset > code->size || range.size > code->size - range.offset ||
            range.size > SIZE_MAX - total) {
            rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
            break;
        }
        total += range.size;
        if (merged_count && merged[merged_count - 1].offset + merged[merged_count - 1].size ==
                                range.offset) {
            merged[merged_count - 1].size += range.size;
        }
        else {
            merged[merged_count++] = range;
        }
    }
    if (R_FAILED(rc)) {
        free(merged);
        return rc;
    }

    u8* payload = malloc(total);
    if (!payload) {
        free(merged);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    size_t payload_offset = 0;
    for (size_t i = 0; i < merged_count; i++) {
        memcpy(payload + payload_offset,
               (const u8*)code->rw_addr + merged[i].offset, merged[i].size);
        payload_offset += merged[i].size;
    }

    Service* service = NULL;
    rc = _kaihenJitGetRoService(&service);
    if (R_SUCCEEDED(rc)) {
        const struct {
            u64 pid_placeholder;
            u64 destination;
            u64 count;
        } in = { 0, (u64)code->rx_addr, merged_count };
        rc = serviceDispatchIn(service, KaihenRoCommand_PublishJitRanges, in,
                               .buffer_attrs = {
                                   SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
                                   SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
                               },
                               .buffers = {
                                   { merged, merged_count * sizeof(*merged) },
                                   { payload, total },
                               },
                               .in_send_pid = true);
    }
    if (R_SUCCEEDED(rc)) {
        for (size_t i = 0; i < merged_count; i++)
            armICacheInvalidate((u8*)code->rx_addr + merged[i].offset, merged[i].size);
    }
    free(payload);
    free(merged);
    return rc;
}
