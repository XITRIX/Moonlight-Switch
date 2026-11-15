// SPDX-License-Identifier: GPL-2.0-only
#include <cstring>
#include "server.hpp"
#include "../../common/research_codefs.h"

namespace {

/// The service space that HEN registers mig:usr in.
constexpr u32 MigrationServiceSpace = 1;
/// How long the loop sleeps when no request is waiting.
constexpr u64 IdleSleepNs = 1000000;

bool isRequest(u32 type) {
    return type == CmifCommandType_Request || type == CmifCommandType_RequestWithContext;
}

bool hasBuffers(const HipcMetadata& meta) {
    return meta.num_send_statics || meta.num_recv_statics || meta.num_send_buffers ||
        meta.num_recv_buffers || meta.num_exch_buffers;
}

} // namespace

HenServer::HenServer() = default;

HenServer::~HenServer() {
    ounce::closeHandle(&fs_root_);
    ounce::closeHandle(&sm_session_);
    if (settings_ready_) {
        setsysExit();
    }
    for (Handle& session : sessions_) {
        ounce::closeHandle(&session);
    }
    for (Handle& session : homebrew_sessions_) {
        ounce::closeHandle(&session);
    }
    for (Handle& process : homebrew_processes_) {
        ounce::closeHandle(&process);
    }
    ounce::closeHandle(&port_);
    for (PluginRecord& plugin : plugins_) {
        ounce::closeHandle(&plugin.control_session);
    }
}

Result HenServer::initialize() {
    status_.protocol_version = HEN_PROTOCOL_VERSION;
    Result rc = svcGetProcessId(&status_.process_id, CUR_PROCESS_HANDLE);
    if (R_SUCCEEDED(rc)) {
        rc = setsysInitialize();
        settings_ready_ = R_SUCCEEDED(rc);
    }
    if (R_SUCCEEDED(rc)) {
        rc = smRegisterServiceInSpace(&port_, MigrationServiceSpace, smEncodeName("mig:usr"),
                                      false, MaxSessions);
    }
    status_.initialization_result = rc;
    return rc;
}

//-----------------------------------------------------------------------------
// The loop
//-----------------------------------------------------------------------------

Result HenServer::run() {
    for (;;) {
        WaitList waits;
        collectWaits(waits);

        std::memset(armGetTls(), 0, ounce::MessageSize);
        s32 index = -1;
        const Result rc = svcReplyAndReceive(&index, waits.handles, waits.count,
                                             INVALID_HANDLE, 0);
        if (index < 0 || index >= waits.count) {
            if (rc == KERNELRESULT(TimedOut)) {
                stepBetweenRequests();
                continue;
            }
            if (R_FAILED(rc)) {
                return rc;
            }
            continue;
        }
        if (R_FAILED(rc) && !(index != waits.port_index && rc == KERNELRESULT(ConnectionClosed))) {
            return rc;
        }

        if (index == waits.port_index) {
            acceptSession();
            continue;
        }
        const SessionRef session = waits.sessions[index];
        if (rc == KERNELRESULT(ConnectionClosed)) {
            closeSession(session);
            continue;
        }
        const Result serve_result = serveSession(session);
        if (R_FAILED(serve_result)) {
            return serve_result;
        }
    }
}

void HenServer::collectWaits(WaitList& waits) const {
    waits.count = 0;
    waits.port_index = -1;
    if (port_ != INVALID_HANDLE) {
        waits.port_index = waits.count;
        waits.handles[waits.count++] = port_;
    }
    for (u32 index = 0; index < MaxSessions; index++) {
        if (sessions_[index] != INVALID_HANDLE) {
            waits.sessions[waits.count] = {SessionKind::Control, index};
            waits.handles[waits.count++] = sessions_[index];
        }
    }
    for (u32 index = 0; index < MaxSessions; index++) {
        if (homebrew_sessions_[index] != INVALID_HANDLE) {
            waits.sessions[waits.count] = {SessionKind::Homebrew, index};
            waits.handles[waits.count++] = homebrew_sessions_[index];
        }
    }
}

/// Advances game arming between incoming requests.
void HenServer::stepBetweenRequests() {
    game_.step();
    svcSleepThread(IdleSleepNs);
}

void HenServer::acceptSession() {
    Handle session = INVALID_HANDLE;
    if (R_FAILED(svcAcceptSession(&session, port_))) {
        return;
    }
    for (Handle& available : sessions_) {
        if (available == INVALID_HANDLE) {
            available = session;
            return;
        }
    }
    svcCloseHandle(session);
}

/// Answers the request that the session sent.
Result HenServer::serveSession(SessionRef session) {
    const ReceivedRequest request = receiveRequest();
    if (request.type == CmifCommandType_Close) {
        closeSession(session);
        return 0;
    }

    CommandReply reply = serveRequest(request, session);
    if (!reply.keeps_request_handles) {
        for (u32 index = 0; index < request.num_copy_handles; index++) {
            svcCloseHandle(request.copy_handles[index]);
        }
    }

    void* message = armGetTls();
    const u32 num_copy_handles = reply.copy_handle != INVALID_HANDLE ? 1 : 0;
    ounce::writeReply(message, reply.result, reply.data, reply.data_size,
                      &reply.copy_handle, num_copy_handles);
    const Result rc = ounce::sendReply(sessionHandle(session));
    if (reply.close_copy_after_send) {
        ounce::closeHandle(&reply.copy_handle);
    }
    if (rc == KERNELRESULT(ConnectionClosed)) {
        closeSession(session);
        return 0;
    }
    return rc;
}

Handle& HenServer::sessionHandle(SessionRef session) {
    return session.kind == SessionKind::Homebrew ? homebrew_sessions_[session.index]
                                                 : sessions_[session.index];
}

void HenServer::closeSession(SessionRef session) {
    ounce::closeHandle(&sessionHandle(session));
    if (session.kind == SessionKind::Homebrew) {
        ounce::closeHandle(&homebrew_processes_[session.index]);
    }
}

//-----------------------------------------------------------------------------
// Requests
//-----------------------------------------------------------------------------

/// Copies the request out of the message buffer. A request that is not
/// accepted still has its handles, so that they can be closed.
HenServer::ReceivedRequest HenServer::receiveRequest() const {
    void* message = armGetTls();
    const ounce::CmifRequest parsed = ounce::parseRequest(message);
    const HipcMetadata& meta = parsed.hipc.meta;

    ReceivedRequest request = {};
    request.type = meta.type;
    request.num_copy_handles = meta.num_copy_handles < MaxCopyHandles ? meta.num_copy_handles
                                                                      : MaxCopyHandles;
    for (u32 index = 0; index < request.num_copy_handles; index++) {
        request.copy_handles[index] = parsed.hipc.data.copy_handles[index];
    }
    // HEN takes no moved handles; close them now.
    for (u32 index = 0; index < meta.num_move_handles; index++) {
        svcCloseHandle(parsed.hipc.data.move_handles[index]);
    }

    request.accepted = parsed.valid && !hasBuffers(meta) && !meta.num_move_handles;
    if (request.accepted) {
        request.command_id = parsed.command_id;
        request.data_size = parsed.data_size < sizeof(request.data) ? parsed.data_size
                                                                    : sizeof(request.data);
        std::memcpy(request.data, parsed.data, request.data_size);
    }
    return request;
}

/// Answers a request. The status commands answer on both kinds of session;
/// the others on one kind only. Anything else is BadInput.
HenServer::CommandReply HenServer::serveRequest(const ReceivedRequest& request,
                                                SessionRef session) {
    CommandReply reply;
    if (!request.accepted) {
        return reply;
    }
    if (request.type == CmifCommandType_Control &&
        request.command_id == ounce::CmifControl_QueryPointerBufferSize) {
        // HEN has no pointer buffer.
        const u16 pointer_buffer_size = 0;
        reply.result = 0;
        reply.setData(pointer_buffer_size);
        return reply;
    }
    if (!isRequest(request.type)) {
        return reply;
    }

    switch (request.command_id) {
    case HenControlCommand_GetStatus:
        if (!request.num_copy_handles) {
            reply.result = 0;
            reply.setData(status_);
        }
        return reply;
    case HenControlCommand_GetGameStatus:
        if (!request.num_copy_handles) {
            reply.result = 0;
            reply.setData(game_.status());
        }
        return reply;
    default:
        break;
    }

    if (session.kind == SessionKind::Control) {
        serveControlCommand(request, reply);
    } else {
        serveHomebrewCommand(request, session.index, reply);
    }
    return reply;
}

/// The commands of the bootstrap's control session.
void HenServer::serveControlCommand(const ReceivedRequest& request, CommandReply& reply) {
    const u32 copies = request.num_copy_handles;
    const Handle* handles = request.copy_handles;

    switch (request.command_id) {
    case HenControlCommand_ConfigureGame:
        if (!copies && request.data_size >= sizeof(HenGameConfig)) {
            HenGameConfig configuration;
            std::memcpy(&configuration, request.data, sizeof(configuration));
            reply.result = game_.configure(configuration);
        }
        break;
    case HenControlCommand_AdoptGameHandles0:
        if (copies == 8) {
            reply.result = game_.adoptHandles0(handles);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_AdoptGameHandles1:
        if (copies == 7) {
            reply.result = game_.adoptHandles1(handles);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_AdoptLoaderRoot:
        if (copies == 1) {
            reply.result = game_.adoptLoaderRoot(handles[0]);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_StartGameArm:
        if (copies == 1 && fs_root_ != INVALID_HANDLE) {
            reply.result = startGameArm(handles[0]);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_CreateHomebrewSession:
        if (copies == 1) {
            reply.result = createHomebrewSession(handles[0], &reply.copy_handle);
            reply.close_copy_after_send = R_SUCCEEDED(reply.result);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_AdoptFilesystemRoot:
        if (copies == 1) {
            reply.result = adoptFilesystemRoot(handles[0]);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_AdoptServiceManager:
        if (copies == 1) {
            reply.result = adoptServiceManager(handles[0]);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_ReleaseControlPort:
        if (!copies) {
            reply.result = releaseControlPort();
        }
        break;
    case HenControlCommand_CreateServiceManagerSession:
        // A plugin's SM session, on the proxy as a homebrew program's is.
        if (!copies) {
            reply.result = sm_proxy_.openSession(&reply.copy_handle);
            reply.close_copy_after_send = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_CloneFilesystemRoot:
        // A plugin's FS root: a session of its own, not HEN's root.
        if (!copies && fs_root_ != INVALID_HANDLE) {
            reply.result = cmifCloneCurrentObject(fs_root_, &reply.copy_handle);
            reply.close_copy_after_send = R_SUCCEEDED(reply.result);
        }
        break;
    case HenControlCommand_AdoptPlugin:
        if (copies == 1 && request.data_size >= sizeof(HenPluginRecord)) {
            HenPluginRecord plugin;
            std::memcpy(&plugin, request.data, sizeof(plugin));
            reply.result = adoptPlugin(plugin, handles[0]);
            reply.keeps_request_handles = R_SUCCEEDED(reply.result);
        }
        break;
    default:
        break;
    }
}

/// The commands of a homebrew process's session.
void HenServer::serveHomebrewCommand(const ReceivedRequest& request, u32 index,
                                     CommandReply& reply) {
    if (request.num_copy_handles) {
        return;
    }

    switch (request.command_id) {
    case ResearchCodeFs_Info: {
        const ResearchCodeFsInfo info={ResearchCodeFs_Magic,ResearchCodeFs_Abi,
            game_.researchLoaderAvailable()?1u:0u,0};
        reply.result=0; reply.setData(info); break;
    }
    case ResearchCodeFs_Open: {
        // CMIF includes transport alignment padding after the inline input.
        if (request.data_size<sizeof(ResearchCodeFsOpen)) break;
        ResearchCodeFsOpen in; std::memcpy(&in,request.data,sizeof(in));
        if (!researchCodeFsValid(&in)) break;
        reply.result=game_.openResearchCodeFs(in.program_id,in.attributes,&reply.copy_handle);
        reply.close_copy_after_send=R_SUCCEEDED(reply.result);
        break;
    }
    case HenHomebrewCommand_GetFilesystemRoot:
        if (fs_root_ == INVALID_HANDLE) {
            reply.result = MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
            break;
        }
        reply.result = cmifCloneCurrentObject(fs_root_, &reply.copy_handle);
        reply.close_copy_after_send = R_SUCCEEDED(reply.result);
        break;
    case HenHomebrewCommand_GetSettingsSession:
        reply.result = cmifCloneCurrentObject(setsysGetServiceSession()->session,
                                              &reply.copy_handle);
        reply.close_copy_after_send = R_SUCCEEDED(reply.result);
        break;
    case HenHomebrewCommand_GetProcessHandle:
        // The process that RO initialized this session for. HEN keeps it.
        reply.result = 0;
        reply.copy_handle = homebrew_processes_[index];
        break;
    case HenHomebrewCommand_GetServiceManager:
        // A session of the program's own on the SM proxy, never the master:
        // SM closes a session after a malformed request, and the master
        // serves every program.
        reply.result = sm_proxy_.openSession(&reply.copy_handle);
        reply.close_copy_after_send = R_SUCCEEDED(reply.result);
        break;
    case HenHomebrewCommand_GetPluginCount:
        removeExitedPlugins();
        reply.result = 0;
        reply.setData(plugin_count_);
        break;
    case HenHomebrewCommand_GetPluginName: {
        u32 plugin = 0;
        if (request.data_size < sizeof(plugin)) {
            break;
        }
        std::memcpy(&plugin, request.data, sizeof(plugin));
        if (plugin >= plugin_count_) {
            reply.result = MAKERESULT(Module_Libnx, LibnxError_NotFound);
            break;
        }
        reply.result = 0;
        reply.setData(plugins_[plugin].name);
        break;
    }
    case HenHomebrewCommand_OpenPlugin: {
        HenPluginName name;
        if (request.data_size < sizeof(name)) {
            break;
        }
        std::memcpy(&name, request.data, sizeof(name));
        removeExitedPlugins();
        reply.result = openPlugin(name, &reply.copy_handle);
        reply.close_copy_after_send = R_SUCCEEDED(reply.result);
        break;
    }
    default:
        break;
    }
}

//-----------------------------------------------------------------------------
// Control commands
//-----------------------------------------------------------------------------

/// Starts game arming on a clone of HEN's FS root. The game arm closes the
/// bootstrap's process handle once it has started.
Result HenServer::startGameArm(Handle bootstrap_process) {
    Handle root = INVALID_HANDLE;
    Result rc = cmifCloneCurrentObject(fs_root_, &root);
    if (R_FAILED(rc)) {
        return rc;
    }
    rc = game_.start(root, bootstrap_process);
    if (R_FAILED(rc)) {
        svcCloseHandle(root);
    }
    return rc;
}

/// Creates a session for a process that RO initialized, and keeps the
/// process handle for GetProcessHandle.
Result HenServer::createHomebrewSession(Handle copied_process, Handle* out_client) {
    for (u32 index = 0; index < MaxSessions; index++) {
        if (homebrew_sessions_[index] != INVALID_HANDLE) {
            continue;
        }
        Handle server = INVALID_HANDLE;
        Handle client = INVALID_HANDLE;
        Result rc = svcCreateSession(&server, &client, 0, 0);
        if (R_SUCCEEDED(rc)) {
            homebrew_sessions_[index] = server;
            homebrew_processes_[index] = copied_process;
            *out_client = client;
        }
        return rc;
    }
    return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
}

Result HenServer::adoptFilesystemRoot(Handle copied_root) {
    if (fs_root_ != INVALID_HANDLE) {
        return MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
    }
    fs_root_ = copied_root;
    return 0;
}

Result HenServer::adoptServiceManager(Handle copied_session) {
    if (sm_session_ != INVALID_HANDLE) {
        return MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
    }
    sm_session_ = copied_session;
    sm_proxy_.setMaster(sm_session_);
    return 0;
}

Result HenServer::releaseControlPort() {
    if (port_ == INVALID_HANDLE) {
        return 0;
    }
    const Result rc = smUnregisterServiceInSpace(MigrationServiceSpace, smEncodeName("mig:usr"));
    if (R_FAILED(rc)) {
        return rc;
    }
    ounce::closeHandle(&port_);
    return 0;
}

Result HenServer::adoptPlugin(const HenPluginRecord& plugin, Handle control_session) {
    const HenPluginName& name = plugin.name;
    if (!name.name[0] || !std::memchr(name.name, '\0', sizeof(name.name))) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }
    for (u32 index = 0; index < plugin_count_; index++) {
        if (!std::strcmp(plugins_[index].name.name, name.name)) {
            return MAKERESULT(Module_Libnx, LibnxError_AlreadyInitialized);
        }
    }
    if (plugin_count_ == HEN_MAX_PLUGINS) {
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    plugins_[plugin_count_++] = {name, plugin.process_id, control_session};
    return 0;
}

//-----------------------------------------------------------------------------
// Homebrew commands
//-----------------------------------------------------------------------------

/// Asks the named plugin for a new client session.
Result HenServer::openPlugin(const HenPluginName& name, Handle* out_session) {
    if (!std::memchr(name.name, '\0', sizeof(name.name))) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }
    for (u32 index = 0; index < plugin_count_; index++) {
        if (std::strcmp(plugins_[index].name.name, name.name)) {
            continue;
        }
        Service control = {};
        serviceCreate(&control, plugins_[index].control_session);
        control.own_handle = false;
        const Result rc = serviceDispatch(&control, HEN_PLUGIN_CONTROL_CREATE_SESSION,
            .out_handle_attrs = {SfOutHandleAttr_HipcCopy},
            .out_handles = out_session,
        );
        // The plugin closed its control session, so it no longer serves.
        if (rc == KERNELRESULT(ConnectionClosed)) {
            removePlugin(index);
            return MAKERESULT(Module_Libnx, LibnxError_NotFound);
        }
        return rc;
    }
    return MAKERESULT(Module_Libnx, LibnxError_NotFound);
}

void HenServer::removePlugin(u32 index) {
    svcCloseHandle(plugins_[index].control_session);
    plugin_count_--;
    for (u32 next = index; next < plugin_count_; next++) {
        plugins_[next] = plugins_[next + 1];
    }
    plugins_[plugin_count_] = {};
}

/// Stops listing the plugins whose process has exited. It runs before the
/// count is returned and before a plugin is opened, so a list read between
/// the two can still name a plugin that exits in the meantime.
void HenServer::removeExitedPlugins() {
    for (u32 index = plugin_count_; index > 0; index--) {
        if (!game_.processExists(plugins_[index - 1].process_id)) {
            removePlugin(index - 1);
        }
    }
}
