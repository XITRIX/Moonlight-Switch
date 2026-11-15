// SPDX-License-Identifier: GPL-2.0-only
#include "sm_proxy.hpp"

#include <cstring>
#include <new>

using ounce::sm::Command;
using ounce::sm::Protocol;
using ounce::sm::Request;
using ounce::sm::RequestKind;

namespace {

/// The result for a well-formed request that the proxy does not serve.
constexpr Result UnsupportedCommand = MAKERESULT(Module_Libnx, LibnxError_NotFound);

/// Holds a mutex for a scope.
class LockGuard {
  public:
    explicit LockGuard(Mutex* mutex) : mutex_(mutex) { mutexLock(mutex_); }
    ~LockGuard() { mutexUnlock(mutex_); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

  private:
    Mutex* mutex_;
};

/// Closes the handles that a request carried. The proxy accepts no handles,
/// but the kernel has already put them into HEN.
void closeCarriedHandles(void* message) {
    const HipcParsedRequest hipc = hipcParseRequest(message);
    for (u32 index = 0; index < hipc.meta.num_copy_handles; index++)
        svcCloseHandle(hipc.data.copy_handles[index]);
    for (u32 index = 0; index < hipc.meta.num_move_handles; index++)
        svcCloseHandle(hipc.data.move_handles[index]);
}

} // namespace

void SmProxy::setMaster(Handle master) {
    LockGuard guard(&lock_);
    master_ = master;
}

/// Joins and frees the sessions whose thread has ended. Under lock_.
void SmProxy::reapFinishedSessions() {
    Session** link = &sessions_;
    while (Session* session = *link) {
        if (session->running.load(std::memory_order_acquire)) {
            link = &session->next;
            continue;
        }
        threadWaitForExit(&session->thread);
        threadClose(&session->thread);
        *link = session->next;
        delete session;
    }
}

Result SmProxy::openSession(Handle* out_client) {
    LockGuard guard(&lock_);
    if (master_ == INVALID_HANDLE)
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    reapFinishedSessions();

    Session* session = new (std::nothrow) Session;
    if (!session)
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

    Handle client = INVALID_HANDLE;
    Result rc = svcCreateSession(&session->server, &client, 0, 0);
    if (R_FAILED(rc)) {
        delete session;
        return rc;
    }

    session->proxy = this;
    session->running.store(true, std::memory_order_release);
    rc = threadCreate(&session->thread, sessionMain, session, nullptr,
                      ThreadStackSize, ThreadPriority, -2);
    if (R_SUCCEEDED(rc)) {
        rc = threadStart(&session->thread);
        if (R_FAILED(rc))
            threadClose(&session->thread);
    }
    if (R_FAILED(rc)) {
        svcCloseHandle(session->server);
        svcCloseHandle(client);
        delete session;
        return rc;
    }

    session->next = sessions_;
    sessions_ = session;
    *out_client = client;
    return 0;
}

void SmProxy::sessionMain(void* argument) {
    Session* session = static_cast<Session*>(argument);
    session->proxy->serve(*session);
}

void SmProxy::serve(Session& session) {
    Handle reply_target = INVALID_HANDLE;
    for (;;) {
        s32 index = -1;
        const Result rc = svcReplyAndReceive(&index, &session.server, 1, reply_target, UINT64_MAX);
        reply_target = INVALID_HANDLE;
        // SessionClosed: the program closed its end (or exited).
        if (R_FAILED(rc) || !respond())
            break;
        reply_target = session.server;
    }
    svcCloseHandle(session.server);
    session.server = INVALID_HANDLE;
    session.running.store(false, std::memory_order_release);
}

bool SmProxy::respond() {
    void* const message = armGetTls();
    // A forwarded request reuses the message buffer: decode the program's
    // request completely first.
    const Request request = ounce::sm::decodeRequest(message);

    switch (request.kind) {
    case RequestKind::Close:
        return false;
    case RequestKind::Invalid:
        closeCarriedHandles(message);
        return false;
    case RequestKind::QueryPointerBufferSize:
        ounce::sm::writePointerBufferSizeReply(message);
        return true;
    case RequestKind::UnsupportedCommand:
        ounce::sm::writeReply(message, request.protocol, UnsupportedCommand);
        return true;
    case RequestKind::Command:
        break;
    }

    Handle handle = INVALID_HANDLE;
    Result rc = 0;
    switch (request.command) {
    case Command::RegisterClient:
    case Command::DetachClient:
        // The master is registered once, as FS. A program must not change
        // that registration for the others.
        rc = 0;
        break;
    case Command::GetServiceHandle:
        rc = lookUpService(request.name, &handle);
        break;
    case Command::RegisterService:
    case Command::UnregisterService:
        // A registration through the master would be FS's, and would
        // outlive the program. Programs register with SM sessions of their
        // own.
        rc = UnsupportedCommand;
        break;
    }
    ounce::sm::writeReply(message, request.protocol, rc,
                          R_SUCCEEDED(rc) ? handle : INVALID_HANDLE);
    return true;
}

Result SmProxy::lookUpService(const SmServiceName& name, Handle* out_handle) {
    Service master;
    serviceCreate(&master, master_);
    master.own_handle = false;
    return serviceDispatchIn(&master, static_cast<u32>(Command::GetServiceHandle), name,
        .out_handle_attrs = { SfOutHandleAttr_HipcMove },
        .out_handles = out_handle,
    );
}
