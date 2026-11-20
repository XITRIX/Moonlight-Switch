# LL27 Stable

Daily-use build. Audio buffer is grouped with the audio driver in General / Audio: 0 through 100 ms in 10 ms steps. Fresh installations default to 10 ms; valid saved values remain unchanged. Larger reserves add audio delay.

The stable executable excludes the detailed audio recorder, its writer thread, per-packet trace hooks and diagnostic TLS. Per-frame RAW video logging and experimental overlay counters were removed. Optional basic logging remains under Debug and is disabled when migrating to stable; later explicit choices persist.

The audio playback thread, bounded preallocated queues, gradual catch-up and seam handling remain. Video recovery, NVTEGRA zero-copy, two presentation buffers, VSync choice, controller settings, handheld/docked profiles and icon are preserved.

Validation: all 18 host suites passed, including reserve settings, production audio renderer integration, concurrent teardown, stream lifecycle, profiles, frame queues and video recovery. The native Switch build linked and passed its executable TLS ABI check. That check also verifies that detailed recorder exports and RAW frame diagnostics are absent. NRO structure and package integrity are checked during packaging.

This build has not been physically tested on the Switch here. Crackles also reported on iOS suggest a shared host or network cause, but this does not establish the exact cause or guarantee their elimination.
