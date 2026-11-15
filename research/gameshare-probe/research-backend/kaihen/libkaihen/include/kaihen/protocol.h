/* SPDX-License-Identifier: ISC
 * Copyright 2026 kaiHEN contributors
 *
 * The kaiHEN commands that Switch 2 homebrew can use, and their data.
 * kaihen.h has the client functions. This file needs only the libnx
 * types, so the RO and HEN servers and their host tests can include it.
 */
#pragma once

#include <switch/types.h>

/// The version of HEN's protocol, in HenStatus::protocol_version.
#define HEN_PROTOCOL_VERSION 6

#define HEN_PLUGIN_NAME_SIZE 64
#define HEN_MAX_PLUGINS 16

/// Private request on the plugin's control session to create a client session.
#define HEN_PLUGIN_CONTROL_CREATE_SESSION 0

typedef struct {
    char name[HEN_PLUGIN_NAME_SIZE];
} HenPluginName;

/// RO's private commands on the ldr:ro session. hbloader passes this session
/// to each NRO as the "ldr:ro" service override. It is initialized for the
/// NRO's process. Each command sends the caller's PID.
typedef enum {
    /// Returns a new HEN homebrew session by COPY. The caller owns it.
    KaihenRoCommand_GetHomebrewSession = 65004,
    /// Maps code for libounce's Jit API: input destination, source and size.
    KaihenRoCommand_MapJitCode = 65010,
    /// Unmaps code that KaihenRoCommand_MapJitCode mapped: input
    /// destination, source and size.
    KaihenRoCommand_UnmapJitCode = 65011,
    /// Unmaps all JIT code of the session. hbloader sends it when an NRO
    /// returns. The reply value is the number of mappings unmapped.
    KaihenRoCommand_ReleaseJitCode = 65012,
    /// Keeps a writable alias in RO of an already mapped executable JIT
    /// range. Input: destination and size. The reply value is RO's alias.
    KaihenRoCommand_MapJitAlias = 65013,
    /// Copies one input buffer into the retained alias and cleans its data
    /// cache. Input: destination, offset, size; one send buffer of size bytes.
    KaihenRoCommand_PublishJitAlias = 65014,
    /// Publishes several ranges in one request. Input: destination and range
    /// count; send buffers contain KaihenJitRange[] and concatenated bytes.
    KaihenRoCommand_PublishJitRanges = 65015,
} KaihenRoCommand;

/// A byte range within a JIT mapping. Payload bytes follow range order.
typedef struct {
    u64 offset;
    u64 size;
} KaihenJitRange;

/// RO's local view of a persistent JIT alias, returned by MapJitAlias.
typedef struct {
    u64 address;
    u32 memory_type;
    u32 permission;
    u32 attribute;
    u32 reserved;
} KaihenJitAliasInfo;

#ifdef __cplusplus
static_assert(sizeof(KaihenJitAliasInfo) == 24, "JIT alias info layout");
#else
_Static_assert(sizeof(KaihenJitAliasInfo) == 24, "JIT alias info layout");
#endif

/// HEN's commands on a homebrew session.
typedef enum {
    /// Returns HenStatus.
    HenHomebrewCommand_GetStatus = 0,
    /// Returns a new clone of HEN's FS proxy root session by COPY.
    HenHomebrewCommand_GetFilesystemRoot = 16,
    /// Returns a new clone of HEN's set:sys session by COPY.
    HenHomebrewCommand_GetSettingsSession = 17,
    /// Returns a COPY of the calling program's process handle.
    HenHomebrewCommand_GetProcessHandle = 18,
    /// Returns a new session on HEN's SM proxy by COPY. The caller owns it.
    HenHomebrewCommand_GetServiceManager = 19,
    /// Returns the number of plugins that opted into IPC publication.
    HenHomebrewCommand_GetPluginCount = 20,
    /// Takes a u32 index and returns HenPluginName.
    HenHomebrewCommand_GetPluginName = 21,
    /// Takes HenPluginName and returns a new plugin session by COPY.
    HenHomebrewCommand_OpenPlugin = 22,
} HenHomebrewCommand;

/// The reply of HenHomebrewCommand_GetStatus.
typedef struct {
    u64 process_id;               ///< HEN's process ID.
    u32 protocol_version;         ///< HEN_PROTOCOL_VERSION.
    Result initialization_result; ///< The result of HEN's startup.
} HenStatus;

#ifdef __cplusplus
static_assert(sizeof(HenStatus) == 16, "HenStatus layout");
#else
_Static_assert(sizeof(HenStatus) == 16, "HenStatus layout");
#endif
