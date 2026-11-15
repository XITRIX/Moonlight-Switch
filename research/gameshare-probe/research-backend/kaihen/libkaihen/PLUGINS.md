# Native plugins

Each `.nso` in `/kaihen/plugins` starts as a separate process, after HEN.
Bootstrap temporarily reaches the plugin through `mig:usr` and gives it an
initialized SM session and FS root of its own: a session on HEN's SM proxy
and a clone of HEN's FS root, so that a plugin cannot close FS's SM session
or the FS root for everyone. `pluginRuntimeInitialize()` receives those handles,
releases the temporary service registration, and installs them as libounce-nx
overrides. The plugin can then use ordinary `smGetService()` calls. A plugin
that needs files calls `fsInitialize()` before its first `fs*()` call and
`fsExit()` before `pluginRuntimeExit()`. Plugins that never use FS do not
open or clone FS sessions.
Call `pluginRuntimeExit()` before returning from `main`.

`source/plugin_runtime.cpp` provides that startup code for plugin NSOs, and
`source/plugin_server.cpp` the optional service below; both use the server-side
CMIF helpers in `source/cmif_server.hpp`. Their public
declaration is `include/kaihen/plugin.h` (C), which the devkit installs with
the runtime as `-lkaihen_plugin`. `source/plugin_startup.h` is the bootstrap's
side of the handoff. The plugin runtime defaults `__nx_fs_num_sessions` to
one when FS is initialized. A plugin can define its own value if it needs
concurrent FS calls. Its returned ready event
belongs to the runtime; the plugin signals it after its own initialization.
The bootstrap waits up to 5 seconds for it. A plugin that is not ready by
then keeps running, but HEN does not list its service. The bootstrap skips a
plugin whose file name is too long or whose package it cannot write. A
failure while starting a plugin process or handing it the services stops the
plugins after it, because the plugin may still hold `mig:usr`; HEN is
running already, so startup goes on. `/kaihen/plugin-status.txt` has one line for each plugin:
running, not listed, or not started, with the stage and result of a failure.

## Optional IPC service

The ordinary `pluginRuntimeInitialize(&ready)` path does not publish a
service. A plugin that wants homebrew to contact it calls
`pluginRuntimeInitializeWithService(&ready, &control_session)` instead. The
runtime creates an unnamed session pair and returns its borrowed server
handle. Bootstrap passes the client handle to HEN after the plugin reports
ready. HEN lists it under the `.nso` filename without the suffix, with the
plugin's process ID. Before it returns the plugin count and before it opens a
plugin, HEN asks `pm:dmnt` whether each listed process still exists and stops
listing the ones that have exited. It also stops listing a plugin whose
control session is closed when it is opened.

The handoff reply always has three handles: the release event, the ready
event and the control session's client end, which is `INVALID_HANDLE` for a
plugin without a service (the kernel passes it through unchanged). Its data
is the runtime version, `PluginRuntimeVersion` in `source/plugin_startup.h`. The
first runtime sent a 0/1 "publishes a service" flag in that word and a third
handle only for 1; the bootstrap still accepts that reply, so plugins built
with the earlier devkit keep working.

`pluginServiceStart` runs a scalar CMIF command server on that session. A
control thread answers HEN's session requests and never runs plugin code, so
HEN does not wait for a slow command; a worker thread serves the client
sessions. The plugin provides a callback that receives a command ID and
up to 64 input bytes and writes up to 64 reply bytes. Call
`pluginServiceStop` before `pluginRuntimeExit`. A plugin that needs mapped
buffers, transferred handles or domain objects can serve the same control
session with libounce-nx's `svcCreateSession`, `svcReplyAndReceive` and
HIPC/CMIF primitives instead.

CMIF rounds input data to whole words, so the callback's input size may
include trailing padding. Define each command's payload size and read only
those fields.

Homebrew obtains HEN's session through RO as before, calls
`henGetPluginCount` and `henGetPluginName` to enumerate published plugins,
then `henOpenPlugin` to obtain a fresh session. HEN requests it through the retained
control session for each open; the plugin creates a new session pair and
returns the client end. Callers do not share one plugin session. HEN closes
its control-session copy at exit; the plugin owns its server end until
`pluginRuntimeExit`. These new commands require protocol version 6.

Plugins link `-lounce_core`, libounce without mimalloc, and keep newlib's
malloc.
