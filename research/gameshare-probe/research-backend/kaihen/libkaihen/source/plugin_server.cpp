// SPDX-License-Identifier: ISC
// A small CMIF server for opt-in kaiHEN plugin services
// (include/kaihen/plugin.h).
//
// HEN sends CreateSession on the control session for each homebrew program
// that opens the plugin, and waits for the reply in its only thread. The
// control thread answers it: it creates a session pair, keeps the server end
// in the session table and returns the client end. It never runs the
// plugin's handler, so a slow command cannot hold HEN.
//
// The worker thread waits on the client sessions in the table and passes
// their requests to the handler. The control thread wakes it with the wake
// event after it adds a session, so that it waits on the new one too.
#include <cstring>
#include <switch.h>

#include "../include/kaihen/plugin.h"
#include "cmif_server.hpp"

namespace {

using ounce::closeHandle;

constexpr u32 MaxSessions = KAIHEN_PLUGIN_MAX_SESSIONS;
constexpr size_t ThreadStackSize = 0x4000;
constexpr int ThreadPriority = 0x2c;

// The control thread waits on the control session and the stop event.
constexpr s32 ControlWait_Control = 0;
constexpr s32 ControlWait_Stop = 1;

// The worker waits on the stop event, the wake event, then the sessions.
constexpr s32 WorkerWait_Stop = 0;
constexpr s32 WorkerWait_Wake = 1;
constexpr s32 WorkerWait_FirstSession = 2;

//-----------------------------------------------------------------------------
// The control thread
//-----------------------------------------------------------------------------

bool isCreateSession(const ounce::CmifRequest& request) {
    return request.valid && request.hipc.meta.type == CmifCommandType_Request &&
        request.command_id == HEN_PLUGIN_CONTROL_CREATE_SESSION &&
        ounce::hasOnlyInlineData(request);
}

/// Creates a session pair in a free slot of the table, wakes the worker and
/// returns the client end.
Result createClientSession(PluginService* service, Handle* out_client) {
    Result rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    mutexLock(&service->sessions_lock);
    for (u32 slot = 0; slot < MaxSessions; slot++) {
        if (service->sessions[slot] == INVALID_HANDLE) {
            rc = svcCreateSession(&service->sessions[slot], out_client, false, 0);
            break;
        }
    }
    mutexUnlock(&service->sessions_lock);

    if (R_SUCCEEDED(rc))
        svcSignalEvent(service->wake_write);
    return rc;
}

/// Serves a request on the control session: CreateSession.
Result serveControlRequest(PluginService* service) {
    void* message = armGetTls();
    const ounce::CmifRequest request = ounce::parseRequest(message);
    Handle client = INVALID_HANDLE;

    Result rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (isCreateSession(request))
        rc = createClientSession(service, &client);
    ounce::closeRequestHandles(request);

    if (R_SUCCEEDED(rc))
        ounce::writeReply(message, 0, nullptr, 0, &client, 1);
    else
        ounce::writeReply(message, rc, nullptr, 0);

    const Result send_rc = ounce::sendReply(service->control_session);
    // The reply gave HEN a copy of the client end.
    closeHandle(&client);
    return send_rc;
}

/// Serves the control session until the stop event is signaled or HEN
/// closes the session.
Result runControl(PluginService* service) {
    const Handle handles[] = {service->control_session, service->stop_read};
    for (;;) {
        s32 index = -1;
        Result rc = svcReplyAndReceive(&index, handles, 2, INVALID_HANDLE, UINT64_MAX);
        if (rc == KERNELRESULT(ConnectionClosed))
            return 0;
        if (R_FAILED(rc))
            return rc;
        if (index == ControlWait_Stop)
            return 0;

        rc = serveControlRequest(service);
        if (rc == KERNELRESULT(ConnectionClosed))
            return 0;
        if (R_FAILED(rc))
            return rc;
    }
}

void controlThread(void* argument) {
    auto* service = static_cast<PluginService*>(argument);
    service->control_result = runControl(service);
}

//-----------------------------------------------------------------------------
// The worker thread
//-----------------------------------------------------------------------------

/// The handles that the worker waits on, and the table slot of each session.
struct WaitList {
    Handle handles[WorkerWait_FirstSession + MaxSessions] = {};
    u32 slots[WorkerWait_FirstSession + MaxSessions] = {};
    s32 count = 0;
};

WaitList makeWaitList(PluginService* service) {
    WaitList list;
    list.handles[WorkerWait_Stop] = service->stop_read;
    list.handles[WorkerWait_Wake] = service->wake_read;
    list.count = WorkerWait_FirstSession;

    mutexLock(&service->sessions_lock);
    for (u32 slot = 0; slot < MaxSessions; slot++) {
        if (service->sessions[slot] == INVALID_HANDLE)
            continue;
        list.handles[list.count] = service->sessions[slot];
        list.slots[list.count] = slot;
        list.count++;
    }
    mutexUnlock(&service->sessions_lock);
    return list;
}

/// Closes a client session and frees its slot. Only the worker closes
/// sessions, and the control thread only fills free slots, so a slot in the
/// worker's wait list still holds the same session.
void closeClientSession(PluginService* service, u32 slot) {
    mutexLock(&service->sessions_lock);
    closeHandle(&service->sessions[slot]);
    mutexUnlock(&service->sessions_lock);
}

void closeAllClientSessions(PluginService* service) {
    mutexLock(&service->sessions_lock);
    for (Handle& session : service->sessions) {
        closeHandle(&session);
    }
    mutexUnlock(&service->sessions_lock);
}

bool isPluginCommand(const ounce::CmifRequest& request) {
    const u32 type = request.hipc.meta.type;
    return request.valid && ounce::hasOnlyInlineData(request) &&
        (type == CmifCommandType_Request || type == CmifCommandType_RequestWithContext);
}

/// Calls the plugin's handler with a copy of the request data, so that the
/// handler may make IPC calls of its own.
Result callHandler(PluginService* service, const ounce::CmifRequest& request,
                   u8 output[KAIHEN_PLUGIN_MESSAGE_SIZE], size_t* output_size) {
    if (request.data_size > KAIHEN_PLUGIN_MESSAGE_SIZE)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    u8 input[KAIHEN_PLUGIN_MESSAGE_SIZE] = {};
    std::memcpy(input, request.data, request.data_size);
    const Result rc = service->handler(request.command_id,
                                       request.data_size ? input : nullptr,
                                       request.data_size, output, output_size,
                                       service->context);
    if (*output_size > KAIHEN_PLUGIN_MESSAGE_SIZE)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    return rc;
}

/// Serves a request on a client session.
Result serveSessionRequest(PluginService* service, Handle session) {
    void* message = armGetTls();
    const ounce::CmifRequest request = ounce::parseRequest(message);
    if (ounce::isQueryPointerBufferSize(request)) {
        ounce::writePointerBufferSizeReply(message);
        return ounce::sendReply(session);
    }

    u8 output[KAIHEN_PLUGIN_MESSAGE_SIZE] = {};
    size_t output_size = 0;
    Result rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (isPluginCommand(request))
        rc = callHandler(service, request, output, &output_size);
    ounce::closeRequestHandles(request);

    if (R_FAILED(rc))
        output_size = 0;
    ounce::writeReply(message, rc, output, output_size);
    return ounce::sendReply(session);
}

/// Handles what the worker received on a client session: a request, a close
/// request, or the client closing the session.
Result serveSession(PluginService* service, u32 slot, Result receive_rc) {
    if (receive_rc == KERNELRESULT(ConnectionClosed)) {
        closeClientSession(service, slot);
        return 0;
    }
    if (R_FAILED(receive_rc))
        return receive_rc;

    if (hipcParseRequest(armGetTls()).meta.type == CmifCommandType_Close) {
        closeClientSession(service, slot);
        return 0;
    }

    const Result rc = serveSessionRequest(service, service->sessions[slot]);
    if (rc == KERNELRESULT(ConnectionClosed)) {
        closeClientSession(service, slot);
        return 0;
    }
    return rc;
}

/// Serves the client sessions until the stop event is signaled or a wait
/// fails.
Result runWorker(PluginService* service) {
    for (;;) {
        const WaitList list = makeWaitList(service);
        s32 index = -1;
        Result rc = svcReplyAndReceive(&index, list.handles, list.count,
                                       INVALID_HANDLE, UINT64_MAX);

        if (index == WorkerWait_Stop && R_SUCCEEDED(rc))
            return 0;

        // A session was added. Clear the event before the next wait list is
        // made, so that a later addition signals it again.
        if (index == WorkerWait_Wake && R_SUCCEEDED(rc)) {
            svcClearEvent(service->wake_read);
            continue;
        }

        if (index < WorkerWait_FirstSession || index >= list.count) {
            if (R_FAILED(rc))
                return rc;
            continue;
        }

        rc = serveSession(service, list.slots[index], rc);
        if (R_FAILED(rc))
            return rc;
    }
}

void workerThread(void* argument) {
    auto* service = static_cast<PluginService*>(argument);
    service->worker_result = runWorker(service);
    closeAllClientSessions(service);
}

Result startThread(PluginService* service, Thread* thread, ThreadFunc entry) {
    Result rc = threadCreate(thread, entry, service, nullptr, ThreadStackSize, ThreadPriority, -2);
    if (R_FAILED(rc))
        return rc;

    rc = threadStart(thread);
    if (R_FAILED(rc))
        threadClose(thread);
    return rc;
}

void stopThread(Thread* thread) {
    threadWaitForExit(thread);
    threadClose(thread);
}

void closeEvents(PluginService* service) {
    closeHandle(&service->wake_write);
    closeHandle(&service->wake_read);
    closeHandle(&service->stop_write);
    closeHandle(&service->stop_read);
}

} // namespace

Result pluginServiceStart(PluginService* service, Handle control_session,
                          PluginServiceHandler handler, void* context) {
    if (!service || !handler || control_session == INVALID_HANDLE)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);

    *service = {};
    service->control_session = control_session;
    service->handler = handler;
    service->context = context;
    mutexInit(&service->sessions_lock);

    Result rc = svcCreateEvent(&service->stop_write, &service->stop_read);
    if (R_FAILED(rc))
        return rc;

    rc = svcCreateEvent(&service->wake_write, &service->wake_read);
    if (R_FAILED(rc))
        goto fail_events;

    rc = startThread(service, &service->worker_thread, workerThread);
    if (R_FAILED(rc))
        goto fail_events;

    rc = startThread(service, &service->control_thread, controlThread);
    if (R_FAILED(rc))
        goto fail_worker;

    service->started = true;
    return 0;

fail_worker:
    svcSignalEvent(service->stop_write);
    stopThread(&service->worker_thread);
fail_events:
    closeEvents(service);
    return rc;
}

Result pluginServiceStop(PluginService* service) {
    if (!service || !service->started)
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);

    Result rc = svcSignalEvent(service->stop_write);
    if (R_FAILED(rc))
        return rc;

    // Stop the control thread first, so that no session is added while the
    // worker closes them.
    stopThread(&service->control_thread);
    stopThread(&service->worker_thread);
    closeEvents(service);
    service->started = false;

    return R_FAILED(service->worker_result) ? service->worker_result : service->control_result;
}
