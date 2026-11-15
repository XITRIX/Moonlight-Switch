// SPDX-License-Identifier: GPL-2.0-only
#pragma once
#include <switch.h>
#include "game_arm.h"
#include "../libkaihen/include/kaihen.h"

// kaiHEN's CMIF interface
enum HenControlCommand {
    HenControlCommand_GetStatus = HenHomebrewCommand_GetStatus,
    HenControlCommand_CreateHomebrewSession = 2,
    HenControlCommand_AdoptFilesystemRoot = 3,
    HenControlCommand_ConfigureGame = 4,
    HenControlCommand_AdoptGameHandles0 = 5,
    HenControlCommand_AdoptGameHandles1 = 6,
    HenControlCommand_StartGameArm = 7,
    HenControlCommand_GetGameStatus = 8,
    HenControlCommand_AdoptLoaderRoot = 10,
    HenControlCommand_AdoptServiceManager = 11,
    HenControlCommand_AdoptPlugin = 12,
    HenControlCommand_ReleaseControlPort = 13,
    HenControlCommand_CreateServiceManagerSession = 14,
    HenControlCommand_CloneFilesystemRoot = 15,
};



/// Create a fresh client session for an RO-initialized process. The caller
/// keeps its process handle and owns the returned client session.
NX_INLINE Result henCreateHomebrewSession(Service* s, Handle process, Handle* out) {
    return serviceDispatch(s, HenControlCommand_CreateHomebrewSession,
        .in_num_handles = 1,
        .in_handles = {process},
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = out,
    );
}

/// Give HEN a COPY of the initialized FS proxy root. The caller keeps its handle.
NX_INLINE Result henAdoptFilesystemRoot(Service* s, Handle root) {
    return serviceDispatch(s, HenControlCommand_AdoptFilesystemRoot,
        .in_num_handles = 1,
        .in_handles = {root},
    );
}

/// Supply the current Loader layout and game identity. The following two
/// calls copy the handles that are local to the bootstrap process.
NX_INLINE Result henConfigureGame(Service* s, const HenGameConfig* config) {
    return serviceDispatchIn(s, HenControlCommand_ConfigureGame, *config);
}

/// COPY the retained memory channel followed by the seven Loader workers.
NX_INLINE Result henAdoptGameHandles0(Service* s, Handle storage, const Handle workers[7]) {
    return serviceDispatch(s, HenControlCommand_AdoptGameHandles0,
        .in_num_handles = 8,
        .in_handles = {storage, workers[0], workers[1], workers[2], workers[3],
                       workers[4], workers[5], workers[6]},
    );
}

/// COPY the local server endpoint, event read/write and PM monitor handles,
/// followed by the RO process-reader session, real FS process handle, and
/// owned FS notifier session. Retaining the notifier keeps the FS-local
/// parking event alive after the bootstrap application closes.
/// FS-process receive/wait/park values travel in HenGameConfig instead.
NX_INLINE Result henAdoptGameHandles1(Service* s, const Handle handles[7]) {
    return serviceDispatch(s, HenControlCommand_AdoptGameHandles1,
        .in_num_handles = 7,
        .in_handles = {handles[0], handles[1], handles[2], handles[3],
                       handles[4], handles[5], handles[6]},
    );
}

/// Give HEN a COPY of the initialized fsp-ldr root for first-launch policy capture.
NX_INLINE Result henAdoptLoaderRoot(Service* s, Handle root) {
    return serviceDispatch(s, HenControlCommand_AdoptLoaderRoot,
        .in_num_handles = 1,
        .in_handles = {root},
    );
}

/// Give HEN a COPY of the initialized FS SM session. It remains a borrowed
/// protocol connection; only kernel handles are closed by HEN and its clients.
NX_INLINE Result henAdoptServiceManager(Service* s, Handle sm) {
    return serviceDispatch(s, HenControlCommand_AdoptServiceManager,
        .in_num_handles = 1,
        .in_handles = {sm},
    );
}

/// A plugin that publishes a service.
typedef struct {
    HenPluginName name; ///< The plugin's SD filename without .nso.
    u64 process_id;     ///< HEN stops listing the plugin once this process has exited.
} HenPluginRecord;

/// Give HEN a COPY of an opt-in plugin's control session, with its name and process.
NX_INLINE Result henAdoptPlugin(Service* s, const HenPluginRecord* plugin, Handle control_session) {
    return serviceDispatchIn(s, HenControlCommand_AdoptPlugin, *plugin,
        .in_num_handles = 1,
        .in_handles = {control_session});
}

/// Unregister mig:usr and close HEN's port. The sessions already open stay.
/// The bootstrap calls it once it is connected, so that each plugin can
/// register mig:usr for its own handoff.
NX_INLINE Result henReleaseControlPort(Service* s) {
    return serviceDispatch(s, HenControlCommand_ReleaseControlPort);
}

/// Get a new session on HEN's SM proxy, for a plugin. The caller owns it.
NX_INLINE Result henCreateServiceManagerSession(Service* s, Handle* out) {
    return serviceDispatch(s, HenControlCommand_CreateServiceManagerSession,
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = out,
    );
}

/// Get a new clone of HEN's FS root, for a plugin. The caller owns it.
NX_INLINE Result henCloneFilesystemRoot(Service* s, Handle* out) {
    return serviceDispatch(s, HenControlCommand_CloneFilesystemRoot,
        .out_handle_attrs = { SfOutHandleAttr_HipcCopy },
        .out_handles = out,
    );
}

/// Start game arming. HEN closes its COPY of the bootstrap process handle.
NX_INLINE Result henStartGameArm(Service* s, Handle bootstrap_process) {
    return serviceDispatch(s, HenControlCommand_StartGameArm,
        .in_num_handles = 1,
        .in_handles = {bootstrap_process},
    );
}

NX_INLINE Result henGetGameStatus(Service* s, HenGameStatus* out) {
    return serviceDispatchOut(s, HenControlCommand_GetGameStatus, *out);
}
