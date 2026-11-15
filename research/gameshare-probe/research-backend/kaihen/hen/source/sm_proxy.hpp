// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <atomic>
#include <switch.h>
#include "sm_request.hpp"

/// HEN's SM proxy. HEN keeps FS's initialized SM session (the master) to
/// itself and gives each homebrew program a session of its own on this
/// proxy instead of a copy of the master. Every program shared the master
/// before, so one malformed request, after which SM closes the session, took
/// SM away from all homebrew until HEN restarted.
///
/// A proxy session speaks SM's protocol (CMIF and TIPC), so the program's
/// sm: override works unchanged. The proxy answers RegisterClient and
/// DetachClient itself, and forwards GetServiceHandle as a request that it
/// builds from the decoded name (sm_request.hpp): the master never receives
/// the program's bytes. A request that the proxy cannot decode closes that
/// program's session only.
///
/// It refuses RegisterService and UnregisterService. A registration through
/// the master would be FS's, and would outlive the program that made it.
/// HEN, RO and the plugin runtime register with SM sessions of their own.
///
/// Each proxy session has a thread of its own, so that a lookup that waits
/// in SM does not stop HEN's main loop. It does not keep the other
/// sessions' lookups going: all of them go through the one master session,
/// and SM answers the requests on a session in turn. A lookup of a service
/// that is never registered therefore stalls every program's lookups, as it
/// did when programs shared the master itself. The waiting thread cannot
/// notice that its program has exited until SM answers.
class SmProxy {
  public:
    SmProxy() = default;
    SmProxy(const SmProxy&) = delete;
    SmProxy& operator=(const SmProxy&) = delete;

    /// Sets the master session. HEN keeps ownership of it.
    void setMaster(Handle master);

    /// Creates a proxy session and the thread that serves it. *out_client is
    /// the client end, which the caller gives to the program (and closes).
    /// There is no fixed number of sessions: it fails only when HEN cannot
    /// allocate the session or create its thread.
    Result openSession(Handle* out_client);

  private:
    static constexpr size_t ThreadStackSize = 0x4000;
    static constexpr int ThreadPriority = 0x2C;

    /// One proxy session. It lives until its program closes the client end:
    /// the thread then ends, and the next openSession joins it and frees
    /// the session (a thread cannot free its own stack).
    struct Session {
        SmProxy* proxy = nullptr;
        Session* next = nullptr;
        Thread thread = {};
        Handle server = INVALID_HANDLE;
        std::atomic<bool> running{false};
    };

    static void sessionMain(void* argument);
    void serve(Session& session);
    /// Answers the request in the thread's message buffer. Returns false
    /// when the session is to be closed.
    bool respond();
    /// Asks SM for the service on the master session. The session SM returns
    /// is in *out_handle.
    Result lookUpService(const SmServiceName& name, Handle* out_handle);
    void reapFinishedSessions();

    Handle master_ = INVALID_HANDLE;
    Mutex lock_ = {};
    /// The sessions whose thread has not been joined yet. Under lock_.
    Session* sessions_ = nullptr;
};
