// SPDX-License-Identifier: ISC
// Startup for an kaiHEN plugin (include/kaihen/plugin.h).
//
// The plugin registers the temporary service mig:usr. The bootstrap connects
// to it and sends AdoptHandles with copies of its SM session and FS root. The
// reply has the runtime version (PluginRuntimeVersion) and three handles: a
// release event, a ready event and the client end of the plugin's control
// session, or INVALID_HANDLE for a plugin without a service. Once the
// bootstrap has the reply it signals the release event, and the plugin
// unregisters mig:usr. Then the plugin installs the SM session and FS root as
// its services.
#include <switch.h>

#include "../include/kaihen/plugin.h"
#include "plugin_startup.h"
#include "cmif_server.hpp"

extern "C" {

// A plugin that initializes FS starts with one session. Its own strong
// definition can select a larger pool when it needs concurrent FS calls.
__attribute__((weak)) u32 __nx_fs_num_sessions = 1;

}

namespace {

using ounce::closeHandle;

constexpr u32 MigrationServiceSpace = 1;
constexpr u64 ReleaseTimeoutNs = 5000000000ull;

/// The handles that the plugin keeps from the bootstrap's AdoptHandles.
struct ReceivedHandles {
    Handle sm_session = INVALID_HANDLE;
    Handle fs_root = INVALID_HANDLE;
    Handle ready_event = INVALID_HANDLE;     ///< Writable end; the plugin signals it.
    Handle control_session = INVALID_HANDLE; ///< Server end, when the plugin publishes a service.
};

/// The bootstrap's ends of the objects that the AdoptHandles reply creates.
/// The reply copies them, so the plugin closes these after sending it.
struct AdoptReply {
    Handle release_event = INVALID_HANDLE;  ///< Writable end.
    Handle ready_event = INVALID_HANDLE;    ///< Readable end.
    Handle control_client = INVALID_HANDLE; ///< When the plugin publishes a service.

    void close() {
        closeHandle(&release_event);
        closeHandle(&ready_event);
        closeHandle(&control_client);
    }
};

ReceivedHandles g_received;
bool g_publish_service = false;
bool g_initialized = false;

void closeReceivedHandles() {
    closeHandle(&g_received.control_session);
    closeHandle(&g_received.ready_event);
    closeHandle(&g_received.fs_root);
    closeHandle(&g_received.sm_session);
}

bool isAdoptHandles(const ounce::CmifRequest& request) {
    const HipcMetadata& meta = request.hipc.meta;
    return request.valid && meta.type == CmifCommandType_Request &&
        request.command_id == PluginStartupCommand_AdoptHandles &&
        meta.num_copy_handles == 2 && !meta.num_move_handles;
}

/// Creates the events and the control session that the AdoptHandles reply
/// returns. The plugin keeps one end of each, and `release_event` receives
/// the readable end of the release event.
Result createAdoptObjects(AdoptReply* reply, Handle* release_event) {
    Result rc = svcCreateEvent(&reply->release_event, release_event);
    if (R_FAILED(rc))
        return rc;

    rc = svcCreateEvent(&g_received.ready_event, &reply->ready_event);
    if (R_FAILED(rc))
        goto fail_release_event;

    if (g_publish_service) {
        rc = svcCreateSession(&g_received.control_session, &reply->control_client, false, 0);
        if (R_FAILED(rc))
            goto fail_ready_event;
    }
    return 0;

fail_ready_event:
    closeHandle(&g_received.ready_event);
    closeHandle(&reply->ready_event);
fail_release_event:
    closeHandle(release_event);
    closeHandle(&reply->release_event);
    return rc;
}

/// Adopts the SM session and FS root of an AdoptHandles request and writes
/// the reply. On success, `reply` holds the bootstrap's ends to close after
/// sending it.
Result adoptHandles(void* message, const ounce::CmifRequest& request,
                    AdoptReply* reply, Handle* release_event) {
    Result rc = MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
    if (g_received.sm_session == INVALID_HANDLE)
        rc = createAdoptObjects(reply, release_event);

    if (R_FAILED(rc)) {
        ounce::closeRequestHandles(request);
        ounce::writeReply(message, rc, nullptr, 0);
        return rc;
    }

    g_received.sm_session = request.hipc.data.copy_handles[0];
    g_received.fs_root = request.hipc.data.copy_handles[1];

    // The kernel passes an INVALID_HANDLE in the copy list through unchanged,
    // so a plugin without a service sends one in the third place.
    const u32 runtime_version = PluginRuntimeVersion;
    const Handle handles[] = {reply->release_event, reply->ready_event, reply->control_client};
    ounce::writeReply(message, 0, &runtime_version, sizeof(runtime_version), handles, 3);
    return 0;
}

/// Serves one request on the mig:usr session. `adopted` is set when it was
/// AdoptHandles and the plugin now has the bootstrap's handles.
Result serveBootstrapRequest(Handle session, Handle* release_event, bool* adopted) {
    void* message = armGetTls();
    const ounce::CmifRequest request = ounce::parseRequest(message);
    AdoptReply reply;
    Result rc;

    if (ounce::isQueryPointerBufferSize(request)) {
        ounce::writePointerBufferSizeReply(message);
        rc = 0;
    } else if (isAdoptHandles(request)) {
        rc = adoptHandles(message, request, &reply, release_event);
        *adopted = R_SUCCEEDED(rc);
    } else {
        rc = MAKERESULT(Module_Libnx, LibnxError_BadInput);
        ounce::closeRequestHandles(request);
        ounce::writeReply(message, rc, nullptr, 0);
    }

    const Result send_rc = ounce::sendReply(session);
    reply.close();
    return R_FAILED(send_rc) ? send_rc : rc;
}

/// Accepts the bootstrap's connection to mig:usr and serves it until the
/// bootstrap has sent AdoptHandles. The session stays open, so that the
/// bootstrap can receive the reply; the caller closes it.
Result receiveBootstrapHandles(Handle port, Handle* out_session, Handle* release_event) {
    Handle session = INVALID_HANDLE;
    for (;;) {
        const Handle handles[] = {port, session};
        const s32 count = session == INVALID_HANDLE ? 1 : 2;
        s32 index = -1;
        Result rc = svcReplyAndReceive(&index, handles, count, INVALID_HANDLE, UINT64_MAX);
        if (R_FAILED(rc))
            return rc;

        if (index == 0) {
            rc = svcAcceptSession(&session, port);
            if (R_FAILED(rc))
                return rc;
            continue;
        }

        bool adopted = false;
        rc = serveBootstrapRequest(session, release_event, &adopted);
        if (R_FAILED(rc) || adopted) {
            *out_session = session;
            return rc;
        }
    }
}

/// Registers mig:usr, receives the bootstrap's handles, and unregisters
/// mig:usr once the bootstrap signals that it has the reply.
Result receiveThroughMigrationService() {
    Handle port = INVALID_HANDLE;
    Result rc = smRegisterServiceInSpace(&port, MigrationServiceSpace,
                                         smEncodeName("mig:usr"), false, 1);
    if (R_FAILED(rc))
        return rc;

    Handle session = INVALID_HANDLE;
    Handle release_event = INVALID_HANDLE;
    rc = receiveBootstrapHandles(port, &session, &release_event);
    if (R_SUCCEEDED(rc))
        rc = svcWaitSynchronizationSingle(release_event, ReleaseTimeoutNs);
    closeHandle(&session);
    closeHandle(&release_event);

    if (R_SUCCEEDED(rc))
        rc = smUnregisterServiceInSpace(MigrationServiceSpace, smEncodeName("mig:usr"));
    svcCloseHandle(port);
    return rc;
}

/// Installs the bootstrap's SM session and FS root for later service use.
Result installReceivedServices() {
    smAddOverrideHandle(smEncodeName("sm:"), g_received.sm_session);
    smAddOverrideHandle(smEncodeName("fsp-srv"), g_received.fs_root);
    return smInitialize();
}

} // namespace

Result pluginRuntimeInitialize(Handle* out_ready_event) {
    Result rc = smInitialize();
    if (R_FAILED(rc))
        return rc;

    rc = receiveThroughMigrationService();
    // mig:usr was registered with the plugin's own SM session. Close it
    // before the bootstrap's session replaces it.
    smExit();

    if (R_SUCCEEDED(rc))
        rc = installReceivedServices();
    if (R_FAILED(rc)) {
        closeReceivedHandles();
        return rc;
    }

    g_initialized = true;
    *out_ready_event = g_received.ready_event;
    return 0;
}

Result pluginRuntimeInitializeWithService(Handle* out_ready_event,
                                          Handle* out_control_session) {
    g_publish_service = true;
    Result rc = pluginRuntimeInitialize(out_ready_event);
    if (R_SUCCEEDED(rc))
        *out_control_session = g_received.control_session;
    else
        g_publish_service = false;
    return rc;
}

void pluginRuntimeExit(void) {
    if (g_initialized) {
        smExit();
        g_initialized = false;
    }
    closeReceivedHandles();
    g_publish_service = false;
}
