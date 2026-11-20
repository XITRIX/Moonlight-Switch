# LL25: repair Switch TLS compilation

LL24 introduced C `_Thread_local` storage to correlate RTP sequence numbers
across callback stages. Its native C compile flags lacked `-mtp=soft`.
The linked accessor loaded from TPIDR_EL0 directly, while the installed libnx
runtime's `__aarch64_read_tp` obtains the correct pointer from TPIDRRO_EL0's
thread context. This is a concrete ABI mismatch and a plausible explanation
for the startup crash. The uploaded logs show two truncated sessions: setup
succeeded and reception/startup-discard tracing began, without a normal exit.
No crash backtrace was supplied, so physical confirmation remains necessary.

The corrected CMake configuration sets `-mtp=soft` before dependencies are
added. The native Windows builder also applies it to every compilation. On
the first corrected build, existing ELF objects with AArch64 TLS relocations
are rebuilt even if their source timestamps did not change. This caught both
AudioStream.c and an existing C++ FFmpeg logging TLS object. The video pipeline
and audio playback policy are unchanged.

Before producing an NRO, the builder now executes the native ABI check. It
compiles C and C++ TLS probes, verifies that the linked audio accessor calls
the libnx helper, and scans all executable ELF sections for direct TPIDR_EL0
read instructions. The previous LL24 accessor would fail this check. Host-only
functional tests could not detect this Switch-specific compilation error.

The diagnostic recorder is retained. Files now use LL25 names:
`moonlight-LL25-log.txt` and `moonlight-LL25-audio-log.txt`. The offline analyzer
accepts session headers from both earlier and newer versions.

All 17 suites pass, including the new native machine-code check. Successful
startup on actual Switch hardware and diagnosis of the original audio crackle
remain pending console validation. Retain 30 ms and enable Debug for the next
comparison; end the stream normally to drain the detailed recorder.
