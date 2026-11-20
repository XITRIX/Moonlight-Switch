# Switch OLED LL27: daily-use low-latency build for upstream review

## A note from Francisco

I am not a programmer. I developed and iterated on this community build with AI-assisted "vibe coding" and many real tests on my own Switch OLED. I am sharing both the implementation and the compiled NRO so the Moonlight-Switch maintainers and other users can inspect, test, improve, or selectively integrate the changes.

LL27 Stable is the version I am now using for everyday play. On my setup, the responsiveness feels close to Moonlight running under Android on the same Switch, which I also tested. Switching to Android was inconvenient for my docked use, so my goal was to improve the native Horizon OS homebrew experience.

This is my personal experience, not a controlled benchmark or a claim of Android-equivalent performance on every device. In an earlier comparison, before the final changes, high-speed photographs of a stopwatch showed roughly 50 ms difference for the homebrew build versus 25 ms for Android. Those earlier numbers must not be advertised as a new LL27 measurement. Android's decode counter also used a different implementation and cannot be directly equated with the homebrew counter. I welcome independent measurements.

Thank you to XITRIX, the Moonlight, Sunshine, Borealis, FFmpeg/NVTEGRA and libnx developers, and the authors of related PRs. The existing hardware decoder and zero-copy path are their work. This contribution tunes the surrounding policies and exposes useful choices; it does not introduce a new hardware decoder.

## Scope and baseline

- Target: original Switch / Switch OLED, homebrew with full application memory, Deko3D/NVTEGRA path. This is not a Switch 2 optimization claim.
- Upstream snapshot: `841fc353aa5152df659aad4f6e1907d4ad3ba1d5` from XITRIX/Moonlight-Switch.
- Exact locally built LL27 source commit: `cd4ecc28c34969a47111e63a57a5f24764004bdb` in the imported-snapshot development history. The review branch is attached to the real upstream history instead of asking maintainers to merge that local import history.
- Compiled test artifact: `Moonlight-LL27-Stable.nro`, SHA-256 `1f2c3062583f4a9c7dc2a5d70311f02d2b76c47b56250773bfddf1aa82e10dad`.
- "Stable" describes my daily-use variant, not an official upstream release or completed validation on every platform.

## Improvements and how to integrate them

### 1. Selectable low-latency versus smooth video

**Problem:** a presentation reserve and an additional adaptive playout clock can delay a decoded frame even when it is ready to display.

**Implementation:** `app/src/streaming/AVFrameHolder.{hpp,cpp}` adds `useLatestFrameOnly` to queue configuration. In that mode, each push keeps only the newest decoded pending frame; `pop()` consumes it at the renderer's next opportunity without waiting to refill a reserve or satisfy a second estimated-source clock. If there is no new frame, the currently displayed frame remains alive. Reference ownership, mutex protection and frame recycling remain intact.

Only decoded output is superseded. Do not implement this by dropping arbitrary compressed H.264/HEVC reference packets, disabling GPU fences, or freeing the frame currently displayed. Queue depth zero does not mean that decoder, GPU, compositor or display latency is zero.

**Integration:** carry the queue API change, the decoder call-site wiring, `Settings::low_latency_video`, JSON persistence, the settings selector and translations together. LL27 defaults to low latency on Switch 1; maintainers can choose an opt-in upstream default. Preserve the existing smooth mode for users who prefer buffering over immediate presentation.

**Tests:** `test_frame_queue.py` exercises actual production queue code with AVFrame fixtures: latest selection, references/reuse, bursts, cleanup, smooth mode and concurrent producer/consumer operation.

### 2. Reconnect statistics reset and bounded stalled-video recovery

**Implementation:** decoder session counters/timing windows are reset across setup/cleanup so previous connections do not pollute new-stream statistics. `VideoRecovery.hpp` tracks actual new frames consumed separately from potentially stale decode counters. On Switch 1 low-latency playback, sustained poor connection plus very low presentation progress first requests an IDR after five seconds, then allows a reconnect after an eight-second window if progress did not return. The recovery helper has a two-reconnect budget per session lifetime; a late frame or a static desktop alone should not trigger it.

**Integration:** port the recovery helper with its `MoonlightSession.cpp` call sites, suspend/resume/reset behavior and existing session-generation checks. It is a fallback, not a substitute for fixing packet delivery. Broader existing reconnect logic also needs review; the helper budget is not a claim that every application reconnect path is bounded.

**Tests:** `test_video_recovery.py`, `test_stream_lifecycle.py`, `test_switch_network.py`.

### 3. Accurate stream-start failures

**Implementation:** `MoonlightSession.cpp` uses `LiGetStageName()` rather than a locally indexed stage-name array. The start-failure dialog includes the failing stage, stage error and connection return code.

**Integration:** preserve this independently of the latency settings. It is useful for diagnosis without enabling continuous tracing. Do not replace the library stage enumeration with an assumed ordering.

### 4. Advertise the Switch's 60 Hz display cadence

**Implementation:** Switch 1 sets `STREAM_CONFIGURATION::clientRefreshRateX100 = 6000` in `MoonlightSession.cpp`. This describes the display cadence separately from requested stream FPS.

**Integration:** keep the original/OLED platform guard; do not blindly impose it on Switch 2 or other displays. It does not force the host game to exactly 60 FPS.

### 5. Request one slice per frame on the NVTEGRA hardware path

**Implementation:** `FFmpegVideoDecoder::capabilities()` advertises `CAPABILITY_SLICES_PER_FRAME(1)` for Switch 1 Deko3D hardware decoding instead of requesting four software-oriented slices.

**Integration:** limit this to the hardware path and retain the other platforms' capabilities. Single-slice requests can change host encoding tradeoffs; this is not a proven universal decode-speed improvement. Compare both host encoding time and decode/display latency on actual hardware.

### 6. Battery versus maximum NVDEC performance

**Implementation:** `FFmpegVideoDecoderDekoHelpers.cpp` wraps `av_nvtegra_dfs_init` and `av_nvtegra_dfs_update`. Battery mode keeps the library's dynamic frequency scaling. Maximum mode retains the initial system-clamped NVDEC request instead of letting subsequent DFS updates lower it for that stream. Only the NVDEC channel is intercepted; library teardown remains responsible for releasing its request.

**Integration:** include the corresponding linker `--wrap` options, settings and profile values. Prefer a supported NVTEGRA API or a narrowly reviewed dependency change upstream if available. This is not a CPU/GPU overclock, bypass of system limits, or a guarantee of sub-2-ms decode. Power and thermal effects should be measured.

**Tests:** `test_nvdec_power.py` checks the wrapper policy with fixtures, not real frequency behavior.

### 7. Optional VSync with a bounded unsynchronized render cadence

**Implementation:** expose `video_vsync`; the Deko3D renderer applies swap interval 1 or 0. In the unsynchronized case, the acquire wrapper uses a roughly 16,667-us CPU deadline with sleep and late-deadline reset. This prevents an unthrottled UI render loop from flooding the GPU/compositor when swap interval zero removes normal backpressure.

**Integration:** port the setting, renderer use and acquire pacing together. Turning off VSync alone caused failures during development. Keep the library's synchronization and resource lifetime rules; this option does not guarantee immediate scanout or absence of tearing. The current acquire deadline is static to that wrapper and deserves lifecycle review upstream.

### 8. Two active presentation images

**Implementation:** a `dkSwapchainCreate` wrapper caps the active image count at two. This avoids changing Borealis class ABI in the personal build. Borealis may still allocate storage for a third image; the claim is two active presentation images, not guaranteed removal of every third buffer allocation or every extra frame of latency.

**Integration:** an explicit supported Borealis swapchain configuration is preferable upstream to a linker wrapper. Audit acquisition/presentation indices, GPU fences and all menu/stream transitions before integrating. A single image was not adopted because it reduces scheduling overlap and may stall presentation.

**Tests for 7-8:** `test_swapchain_buffers.py`; native TLS/link checks do not replace real Deko3D tests.

### 9. Independent handheld and docked stream profiles

**Implementation:** `StreamProfile.hpp` stores resolution, FPS, bitrate and the NVDEC power preference. `Settings.{hpp,cpp}` persists both profiles and exposes a `StreamProfileLatch`. At session creation, `appletGetOperationMode()` selects a profile once. Getter values use that immutable session snapshot until session destruction. Docking/undocking mid-stream does not renegotiate, reconnect, or silently change the active preferences.

**Integration:** port settings serialization/migration, selector controls, session begin/end and all profile-aware getters as one unit. Existing preferences initialize profiles; when old resolution was automatic, initialize portable 720p and docked 1080p. Changes apply when a new session begins.

**Tests:** `test_stream_profiles.py` covers snapshots, subsequent starts and configuration edits. Real dock hotplug behavior should also be tested.

### 10. Full-width horizontal settings tabs

**Implementation:** `settings_tab.cpp`, `settings_tab.hpp`, XML and translations expose General / Handheld / Docked / Debug along the top, reachable using L/R or touch. A page occupies the full content width beneath the tabs. Focus moves off the previous page before it is destroyed. Profile controls are separated by page, and log controls remain in Debug.

**Integration:** keep focus ownership and platform guards; exercise controller and touch navigation. This is a usability change that can be reviewed separately from playback.

### 11. Configurable small audio reserve, 0-100 ms

**Implementation:** `AudioReserve.hpp` implements startup/rebuffer readiness. `Settings::set_audio_reserve_ms()` accepts 0 through 100 in 10-ms steps, invalid values fall back to 10 ms. The selector sits under General / Audio beside the audio backend. A valid existing setting is retained; new installs default to 10 ms. The active renderer snapshots the value on initialization, so changes apply on the next stream.

**Integration:** port the validation, persistence, UI and renderer together. Higher reserve explicitly adds audio delay; it is not free latency reduction or a repair for host-side discontinuities. Zero disables the intentional reserve, not every audio buffer.

**Tests:** `test_stable_audio_settings.py` compiles the actual extracted setter and checks XML placement/selector values, while `test_audio_reserve.py` covers startup and recovery for every offered value.

### 12. Bounded Audren PCM pipeline and dedicated playback pump

**Problem:** a playback feed tied only to packet callbacks can suffer when packet arrivals are bursty or the renderer is not serviced promptly. Audren has four hardware wave slots; a large number of tiny packets cannot simply be treated as additional hardware slots.

**Implementation:** `AudrenAudioRenderer.{hpp,cpp}` decodes Opus, coalesces short packets into at least roughly 10-ms blocks, and transfers them into a bounded preallocated PCM pool. A dedicated 64-KiB-stack thread services playback, waking on ready PCM and sleeping while idle; while audio is active it uses up-to-5-ms timed waits, subject to OS scheduling. There is no per-packet allocation or busy-wait. A mutex protects ownership and teardown; cleanup disables acceptance, wakes the thread, joins without holding the mutex, closes the driver, then frees buffers. Opus state still advances for decoded data that is later discarded; null data uses Opus concealment.

`AudioSampleQueue.hpp` accounts for accepted samples and the driver's played-sample counter, including wrap/reset behavior, instead of treating a partly played buffer as wholly pending. `AudioQueuePolicy.hpp` bounds output backlog relative to the chosen reserve, avoids discarding late incoming audio in a way that starves the output, and separates input/output/capacity drop reasons.

**Integration:** review as one audio subsystem change; do not transplant only a queue constant. Check service ownership, thread priorities, channel counts, all init-failure paths, suspend/reconnect and whether any other platform shares the class. The shipped Switch sink remains stereo; this work does not add Dolby encoding or 5.1 output.

### 13. Gradual audio backlog catch-up and discontinuity smoothing

**Implementation:** surplus audio can be drained with a pitch correction bounded at 2%, slewed by small steps instead of abrupt frequency jumps. `AudioSeam.hpp` applies an approximately 2-ms ramp only after a known discontinuity, working on newly decoded PCM rather than driver-owned buffers. Contiguous PCM is unchanged. Hard backlog limits remain as a last resort so delay cannot grow indefinitely.

**Integration:** keep sample accounting, seam markers, drop policy and catch-up thresholds consistent. Pitch correction can be audible; maintainers may prefer a pitch-preserving rate-matching implementation. A seam is mitigation, not recovery of lost samples.

**Tests for 12-13:** `test_audio_sample_queue.py`, `test_audio_queue_policy.py`, `test_audio_seam.py`, `test_audren_integration.py`, `test_audren_threads.py`. Fixtures cover multiple packet durations, every reserve, long simulated playback, injected driver errors/jitter, actual host threads, repeated starts and concurrent cleanup. They do not emulate the real Audren scheduler or Wi-Fi.

### 14. Daily-use logging cleanup and lower diagnostic overhead

**Implementation:** `MOONLIGHT_STABLE_BUILD` removes the detailed per-packet audio trace callbacks, recorder thread and diagnostic TLS from the shipped binary. RAW per-frame video logs/stopwatches were removed. Optional basic logging is in Debug, defaults off, and migrates previous investigation builds to off once. Basic files contain the version: `moonlight-LL27-log.txt` and its previous-log counterpart. The normal overlay retains essential receive/decode/render/queue statistics, refreshed on a cached interval.

**Integration:** keep diagnostics optional and versioned. The review source includes the older recorder and its tests for reference, but it is excluded from the stable runtime. The moonlight-common-c changes are diagnostic instrumentation and compile-out support, not a new streaming protocol or codec. Maintainers can omit that instrumentation entirely when integrating only the stable playback improvements.

### 15. Correct Switch TLS ABI and native executable verification

**Implementation:** Switch 1 C/C++ compilation uses `-mtp=soft -mtls-dialect=trad -ftls-model=local-exec`. Earlier experimental diagnostics exposed invalid TLS code generation: direct TPIDR_EL0 access was incompatible with this libnx path, and soft-thread-pointer alone still produced an invalid descriptor/address sequence under PIC. The native Windows helper rebuilds affected TLS objects and runs `test_switch_tls_abi.py` on the linked ELF before packaging. The test also verifies detailed recorder symbols are absent in LL27 Stable.

**Integration:** review against the project's supported compiler/libnx combination and apply consistent flags to participating C/C++ targets. Do a clean rebuild when ABI flags change; relinking stale objects is insufficient. The ELF checks and C/C++ PIC/PIE probes should become an appropriate build gate rather than relying on local paths or a version-specific diagnostic symbol forever.

### 16. Reproducible local build notes and host regression coverage

**Implementation:** dependency initialization and Switch CURL lookup were corrected for the available SDK; local helpers document native Windows compilation when MSYS shells could not run. `build-from-generated-makefiles.py` reuses an already configured CMake build; it is not a clean configure replacement and does not implement the complete header dependency graph. The original source archive includes exploratory CI workflow additions, but those workflows were not used to produce the supplied NRO. The review branch leaves upstream workflows unchanged to avoid presenting unvalidated CI work as a completed pipeline.

**Integration:** prefer the project's normal devkitPro toolchain and CI. Add portable tests in small groups; make SDK paths/configuration explicit before adopting the local native helper. Keep dependency versions and third-party licenses with distributed artifacts.

## Important things this contribution does not claim

- Zero dead zone and controller mapping are available in this build, but the upstream baseline already had dead-zone controls and zero defaults. They must not be credited here as a new implementation.
- The custom moon/controller icon is cosmetic; maintainers can retain the official icon. No new right to use Nintendo or other third-party branding is claimed.
- Displaying profile bitrate as Mbps is a requested follow-up, not an LL27 feature. Do not advertise it as implemented here.
- The multi-megabyte socket-initialization override tried in LL2 was reverted. The final build retains the original Borealis/libnx socket initialization and diagnostic SO_RCVBUF readback. It does not claim a new network buffer policy.
- The user's early freezes stopped after pinning the Switch to 5-GHz Wi-Fi instead of 2.4 GHz. This was a material environmental change, so those results cannot be attributed to code alone.
- Some audio crackles remain. The same periodic crackles were reproduced on an iOS Moonlight client, while local PC audio sounded clean. Host capture, Sunshine and the shared network remain under investigation; this does not prove the Switch audio path is fault-free or that Sunshine is at fault. This PR must not be presented as a universal crackle fix.
- Sunshine's virtual controller choice on Windows is a host-side setting. LL27 does not force Xbox device emulation or change in-game button glyph assets.

## Validation and recommended upstream review order

All 18 local host suites passed on 2026-11-20. The ARM64 native build linked, passed the executable TLS ABI check and produced a validated NRO/ASET package. The JSON report records `hardware_tested: false` because those automated suites are host fixtures; that does not negate my separate manual tests on my Switch. No CI run or independent LL27 latency benchmark is claimed.

I suggest splitting review into: (1) queue mode and lifecycle/errors; (2) profiles/UI; (3) NVDEC/presentation choices; (4) audio subsystem; (5) build/diagnostic support. These are review boundaries, not assurances that every file can be cherry-picked independently. Please request narrower PRs if preferred.

Test both smooth/low-latency modes, VSync on/off, battery/maximum NVDEC, handheld/docked starts and hotplug during a session, repeated reconnects, sleep/resume, 720p60 and 1080p60, all offered audio reserves, controller/touch focus and a sustained session. Compare end-to-end delay with the same host, codec, bitrate, network, display and scene; do not use the decoder counter alone as total latency.

Related prior work reviewed during development includes [#339](https://github.com/XITRIX/Moonlight-Switch/pull/339), [#358](https://github.com/XITRIX/Moonlight-Switch/pull/358), [#349](https://github.com/XITRIX/Moonlight-Switch/pull/349), [#334](https://github.com/XITRIX/Moonlight-Switch/pull/334) and [Rubylight #32](https://github.com/RamazanKara/Rubylight/issues/32). Similar goals are not exclusive inventions of this contribution; existing proposed solutions should be compared before merging overlapping implementations.
