# LL26: repair PIC TLS address generation

LL25's software thread-pointer change was incomplete. Inspection of the linked
getter revealed a helper call followed by `ldr x0,[x0,x0]`, an invalid address
calculation for this TLS variable. Its callback wrapper similarly used a
self-indexed store. The audio TLS symbol had disappeared from the object.
The LL25 log remains truncated during startup; no console exception backtrace
is available. The malformed machine code is independently observable.

LL26 uses `-mtp=soft -mtls-dialect=trad -ftls-model=local-exec` in both CMake
and the native builder. Local-exec TLS is appropriate for this statically
linked NRO and avoids the malformed descriptor sequence generated for PIC.
The getter now obtains the libnx thread pointer, adds 0x18, and loads from
that address. The TLS symbol is present at offset 8; the remaining 16 bytes
are the AArch64 thread-control-block bias.

The object migration checks TLS symbols and references to `__aarch64_read_tp`
in addition to TLS relocations, since the malformed LL25 object lacked the
TLS relocations that the first migration check looked for. The old audio and
FFmpeg logging objects were recompiled. Versioned ABI stamps prevent stale
objects from silently surviving this change.

The native gate now tests both PIC and PIE modes in C and C++, rejects the
self-indexed load, requires the actual TLS symbol, and checks the linked
offset calculation against that symbol plus the TCB bias. It continues to
scan executable sections for incompatible direct TPIDR_EL0 reads. The gate
rejected the stale LL25 audio object before allowing this NRO to be produced.
The previous host-only and helper-presence checks were insufficient.

All 17 suites pass. Actual console startup remains pending verification.
No audio playback-policy or video-rendering change was made. Diagnostic names
are `moonlight-LL26-log.txt` and `moonlight-LL26-audio-log.txt`. Keep 30 ms and
Debug enabled for the console comparison; stop normally to flush the recorder.
