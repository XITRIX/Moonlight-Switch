# Native Switch 2 port

The `PLATFORM_SWITCH2` target builds a standalone NRO for the installed Ounce SDK
(libounce and OunceVK). It shares Moonlight's native controller, touch, keyboard,
font, networking and audren audio integration. The UI and video renderer use
SDL3 for lifecycle/input and native Vulkan over OunceVK for all drawing.
There is no GL, EGL or Zink renderer in the Switch 2 build. Switch 1 retains deko3d and its
existing FFmpeg archives.

H.264 is the default and uses the SDK hardware video-decoder service when
hardware decoding is enabled. AV1 Main8 is also available through that service.
H.264 NV12 frames retain their decoder slot in a reference-counted lease.
Fresh allocations are imported with `VK_EXT_external_memory_host` before native
pool registration; the video fragment shader reads the same bytes as an SSBO.
An unavailable/rejected import explicitly uses a CPU copy into coherent Vulkan
buffer storage. AV1 retains CPU copy-back. Native-buffer sharing in this new
backend was qualified on the device before the SDL/fork migration. The SDL 3.4.20
build requires a fresh device check. HEVC is labelled as software because the installed SDK exposes only
H.264, VP9, AV1 and VP8 codec entries. The original Switch NVTEGRA context stays
excluded from this target. Native H.264 1920x1080 output has been measured at
119.94 decoded FPS over a 204.466-second high-rate interval. AV1 remains
unqualified on the device; HDR and upscaling are not enabled.

## Build on Windows

The other local ports use MSYS2 at `%USERPROFILE%/.dirt2-tools/msys64` and the SDK
at `../devkitPro`. Run from this repository in PowerShell:

```powershell
./scripts/switch2/build.ps1
# After dependencies have built:
./scripts/switch2/build.ps1 -Target app
```

Use `-MsysRoot` and `-DevkitProDirectory` for different installations. Host tools
are MSYS2 CMake, Ninja, Git, make, curl, tar and Python, plus UCRT64 GCC.
Only the build process environment changes. The Ounce SDK supplies devkitA64,
libounce and NRO packaging tools. Nothing is installed into the shared SDK.

On Linux/MSYS2, export `DEVKITPRO`, then run `bash scripts/switch2/build.sh`.
`libraries` builds just dependencies; `app` builds the application. Outputs:

- `build/deps/switch2`: static dependencies and headers, rebuilt for libounce.
- `build/switch2/Moonlight-Switch.nro`: NRO with embedded resources.
- `build/switch2/validation.json`: executable hash and host validation evidence.

`cmake --preset switch2` and `cmake --build --preset switch2` are also available
after dependencies and recursive submodules have been prepared.

Borealis and SDL3 are consumed directly from the user-owned Git forks:
`extern/borealis` (`XITRIX/borealis`) and its nested
`library/lib/extern/SDL3` (`XITRIX/SDL`). Initialize them with
`git submodule update --init --recursive`. No Borealis/SDL patch application or
SDL source archive is used. SDL is built by Borealis's CMake target from its
pinned submodule, upgraded to upstream stable SDL 3.4.20. It retains the complete
Switch 1 port, the Switch lifecycle/input additions, and the existing VisionOS
fix. Switch 2 automatic output sizing and EGL-optional native windows are in the
SDL fork. The native Vulkan UI context, GPU submission resource ownership, and
NanoVG Vulkan backend now belong to Borealis; Moonlight retains the streaming
renderer and FFmpeg frame-lease adapter. Other dependency versions are pinned in
`scripts/switch2/dependencies.sh`. mbedTLS uses the SDK's `csrng` service;
Expat hash salts use its OS-seeded ChaCha generator.
Downloaded source archives are checked against `scripts/switch2/sources.sha256`.

## GitHub Actions

`.github/workflows/switch.yml` builds Switch 1 and Switch 2 in a matrix and also
supports manual dispatch. `all-builds.yml` includes it as the Switch group; tag
builds publish both platforms' artifacts in the release.

The Switch 2 job runs on a GitHub-hosted Ubuntu runner in
`devkitpro/devkita64:20260219`, matching the public devkit's Dockerfile. It installs the public
[ounce-devkit 20261115b release](https://github.com/ouncebrew/ounce-devkit/releases/tag/devkit-20261115b)
after checking the archive against the SHA-256 pinned in the workflow. No private
SDK access or self-hosted runner is needed. The verifier accepts both the current
`ounce-devkit-version.txt` record and the earlier `ounce-sdk-version.txt` name.

The job caches project-local dependencies using the SDK checksum and dependency
build inputs. It runs the host regressions, compiles and validates shaders, and
checks the NRO's ABI and native Vulkan link before packaging. The outputs are:

- `Moonlight-Switch2`: `Moonlight-Switch2.zip`, containing
  `switch/Moonlight-Switch/Moonlight-Switch.nro` for extraction to the SD root.
- `Moonlight-Switch2-debug`: ELF, link map, and build/shader validation records.
- Build diagnostics on failure.

The SDL dependency now tracks the linear `XITRIX/SDL` `moonlight` branch, rebased
onto SDL 3.4.20 with the full Switch 1 port, VisionOS fix and Switch 2 changes
preserved. Its pre-rebase remote head is retained on
`codex/moonlight-before-rebase-20261114`. Borealis pins the rebased commit.

## Formatting and lint

Borealis and SDL additions use each fork's `.clang-format`; SDL's own
`.editorconfig` is retained. Moonlight's new root `.clang-format` follows the
Borealis style with a 120-column limit for streaming code. Python tooling is
formatted and checked with Ruff, shell tooling with shfmt and Bash syntax checks,
and the new workflow/caller/release chain with actionlint. Changed regions of
existing application files are formatted alongside the new native source files.

The 2026-11-14 fork migration passed the Moonlight build (397 translation
units, zero unresolved symbols, no GL/EGL/Zink), a standalone Borealis demo NRO
build with the same native renderer checks, and 56 SDL platform compilation
checks against libnx/libounce across native, EGL and Vulkan configurations.
Entropy, touch and native-decoder/frame-lease host regressions also passed.
`build/switch2/fork-migration-validation.json` records the published dependency
commits and build evidence. This SDL 3.4.20 package was uploaded and readback-verified at
`/switch/Moonlight-Switch/Moonlight-Switch.nro`. The previous qualified NRO was
retained as a backup. Runtime testing of the updated package remains pending.

The initial build on 2026-11-13 passed compilation of all 187 application and UI
translation units, AArch64 PIE and CPU/software-TLS checks, zero unresolved
symbols, and the link-map check for the SDK's `libouncevk-gl.a`. The entropy
adapter passed host mocks for service initialization failure, random-read failure,
successful reads and balanced service cleanup. The current package hash and size are recorded in `build/switch2/validation.json`.
These are host results; device qualification remains pending.

## Native display resolution and refresh

The current native Vulkan target follows libounce's automatic window dimensions:
1920x1080 handheld and the active configured TV output when docked. The Vulkan
viewport and swapchain match those physical dimensions; Borealis keeps its
logical UI scaling. Stream resolution is a separate setting (default 720p).
`nwindowSetFrameRates(120, 60, true)` requests 120 Hz/VRR where the system permits,
with 60 Hz fallback. This request alone does not measure scanout or throughput.

Earlier GL-based builds first rendered at 720p despite an AM 1080p message. A
fixed-1080p EGL/viewport update was tested successfully, with VI2 nominal 120 Hz,
VRR true and a 1/120 presentation interval recorded. Those results describe
the earlier backend. The native Vulkan backend's mode and stream counters need
a fresh device run. Settings/pairing data remain in `sdmc:/switch/Moonlight-Switch`.

The touch mapper converts the HID sensor's 1280x720 coordinate space to drawable
pixels before dividing by the UI scale. Previously it divided raw HID coordinates
directly by 1.5 at 1080p, moving taps toward the top-left and missing controls.
`scripts/switch2/test_touch.py` checks real button hit targets at 1080p, capped
UI scale, center/edge/fractional positions and the original 720p mapping. The
native build and host regression pass; touchscreen alignment still needs a
fresh console check after deployment.

The display-build device log subsequently confirmed a 1920x1080 framebuffer,
VI2 nominal 120 Hz with VRR true, a 1/120 presentation interval, successful
queries and the first-UI-frame marker. This establishes the selected display
mode, not measured scanout or streaming throughput. The user then reported
misaligned touchscreen hits; the corrected touch build was uploaded and
readback-verified, retaining the display build as a backup.

## Native hardware decoder

`Switch2VideoDecoder` initializes the SDK's `VideoDecoder` on the first keyframe.
H.264 SPS/PPS are collected from Moonlight's parameter buffers and normalized
to four-byte Annex-B start codes. The complete access unit is submitted once.
AV1 configuration remains in its keyframe. The client requests limited-range
Rec.709; both the service color policy and the output AVFrame use that space.

The output reader and submission callback serialize SDK access. Each acquired
SDK image either becomes a leased H.264 frame or is copied/released for AV1.
Stop joins the reader before dropping its pool reference and clearing frames.
The last lease/import closes the pool; a GL fence retains each slot through GPU
completion. Deferred release callbacks never issue SDK calls under queue locks.
Failures request another IDR and recreate the native decoder; a partially
accepted/timed-out sample is never retried in full. H.264 native failures do not
silently switch to software. HEVC explicitly uses the software backend; disabling
hardware decoding also selects the software backend (which has no AV1 decoder
in this build).

The SDK header documents a 60-FPS maximum and a 248,832,000-pixel/s budget.
Inspection of the exact shipped `decoder.o` shows that 60 is used by the
default-FPS calculation when `frame_rate` is zero. An explicit rate passes to
`viddecDecoderInitialize`, whose shipped wrapper serializes it without a
60-FPS guard. This does not prove that the service accepts or sustains 120 FPS.
The port passes the requested FPS unchanged and logs initialization results,
the first copied hardware frame, and received/decoded FPS counters. The user's
reported CivHomebrew H.264 120-FPS result is a separate runtime path and is not
treated as a measurement of this native backend.

`scripts/switch2/test_decoder.py` compiles the real backend with host SDK and
frame-queue mocks. It checks deferred IDR setup, parameter normalization,
120-FPS pass-through, leased NV12 lifetime, malformed access units,
failed initialization, partial-submission timeout recovery, copy/allocation
failure cleanup, codec selection and repeated start/stop. These checks and the
native build pass; the NRO has no unresolved symbols and
contains the SDK decode entry points. The candidate was uploaded with SHA-256
readback verification and a previous-NRO backup. Native H.264/AV1 streaming and
120-FPS service acceptance were initially unverified.

### Device creation failure and 60-FPS control

The first 1080p/120 native H.264 run repeatedly failed in `videoDecoderCreate`
with `0x17a26f` (module 111, description 3025), producing no hardware frames.
The same user's 1080p/60 control initialized successfully and returned 286
hardware frames in the captured interval. Reported one-second decoded rates
varied from about 30 to 58 FPS and tracked incoming frames; this is not evidence
of locked 60 FPS. A subsequent 120-FPS attempt again failed at creation.
Evidence is in `build/switch2/hardware-failure-validation.json` and
`hardware-control-validation.json` with their captured logs. The numeric result
has no independently established symbolic meaning here.

The next diagnostic build wraps the SDK's typed creator, image-pool,
configuration, registration and start calls to identify the failing stage.
Failed creations are retried at most once per second, and received-frame
counters include frames received before successful decoder initialization.

For this observed result only, a requested registration above 60 retries once
with the documented registration budget (60 at 1080p, 30 at 4K). The stream's negotiated FPS and every input
frame remain unchanged. Initialization logs explicitly distinguish stream FPS
from registration FPS. Host mocks verify all 120 input frames still reach the
SDK under this retry; only device output counters can qualify actual throughput.
The diagnostic NRO was uploaded with readback verification. The following run
qualified the registration workaround and H.264 output above 60 FPS.

### Confirmed native H.264 1080p120 output

On 2026-11-13, the user confirmed that the registration-workaround build streams
successfully. The log shows 120-FPS configuration failing specifically inside
`viddecDecoderInitialize` with `0x17a26f`, followed by the otherwise-identical
60-FPS configuration succeeding. Image registration and decoder start succeed,
then real SDK frames are copied as 1920x1080 NV12. The stream stays at 120 FPS.

The longest consecutive high-rate interval (both reported input and output at
least 115 FPS, adjacent samples less than two seconds apart, and increasing
decoded totals) spans device times 22:20:00.644–22:23:25.110: **24,524 frames in
204.466 seconds, or 119.942 decoded FPS**. Individual reports are 116.1–121.0
FPS. Slower intervals elsewhere track slower incoming frame rates and are not
included in this high-rate measurement. Three native initializations succeeded;
the capture contains no native acquire, submit, copy or output-stall failures.

This demonstrates that the service's accepted registration rate does not impose
a matching output-throughput ceiling in this tested path. VI2 also reports a
1920x1080 framebuffer, nominal 120 Hz, VRR true and interval 1/120. Decoder
counts and the reported display mode do not independently establish drop-free
scanout, render FPS or end-to-end input latency.

Tested NRO SHA-256:
`5fbc94cab4242edc7d5b6a12a3df4ab533084553a9c5043560730dc9897800b2`.
The unchanged tested NRO remains installed at
`/switch/Moonlight-Switch/Moonlight-Switch.nro`.
`build/switch2/hardware-120-validation.json` records the measured interval,
capture hash and the test limits. Source capture:
`device-hardware-registration-20261113-212724.log`. This qualifies H.264;
AV1 hardware output and HEVC software performance remain separate checks.

## Console qualification

Copy the NRO to its own folder, for example
`sdmc:/switch/Moonlight-Switch/Moonlight-Switch.nro`, and launch in the working native
HEN/HBMenu title-takeover session used by the other ports. Settings, pairing
keys, cache and logs use `sdmc:/switch/Moonlight-Switch`; existing settings and
pairing data are preserved during deployment. `startup.log` records renderer initialization,
decoder selection, and `SWITCH2_HEALTH: first UI frame completed`.

A build and zero unresolved symbols do not establish device functionality.
Qualification requires a visible UI, controller/touch/native-keyboard checks,
discovery and PIN pairing, an H.264 stream with stereo audio and input, an HEVC
stream, disconnect/reconnect, and clean return to HBMenu. Record frame pacing,
decode time and resolution/FPS during that run. No console execution or streaming
success is claimed by the host verifier.


## Native Vulkan UI and video

The Switch 2 target selects a dedicated native Vulkan backend, analogous to the
app's Metal backend. NanoVG uses a pinned Vulkan C backend at
`extern/borealis/library/lib/extern/nanovg-vulkan/upstream.json`, with its license and documented local
changes. The existing Borealis UI, fonts, images and input API are retained.
SDL is rebuilt with GL/EGL disabled and only owns the native window/lifecycle.
`brls::Switch2VulkanContext` in the Borealis fork implements its SDLVideoContext interface with a
Vulkan device, VI surface and swapchain; it never creates a GL/EGL context.

The UI and video share one Vulkan device, graphics/present queue and render
pass. The swapchain requests only COLOR_ATTACHMENT usage (supported by the
installed SDK); video converts NV12/YUV420 into that target and NanoVG adds the
UI. It does not copy a composed offscreen image to the swapchain. The backend
keeps two frame slots, each with its own command buffer, completion fence,
acquire semaphore, upload storage and video descriptors. Render-finished
semaphores belong to swapchain images, preserving presentation reuse rules.
Acquisition/presentation out-of-date or suboptimal results trigger recreation.
Resize waits for the device, releases frame resources and replaces swapchain
attachments while preserving the compatible NanoVG render pass and UI assets.

The native video reader continues using typed SDK APIs. H.264 frames own a
native-pool lease; submissions retain that lease through GPU fence completion.
Queue drops defer SDK releases without IPC under queue locks. The SDK pool
create/close wrappers import fresh host allocations before native registration
and unimport before backing is freed, including failed-creation unwind. A pool
registration rejected after import is retried without import only after a full
native unwind. If import is unavailable, playback uses a logged CPU-copy path;
there is no silent GL renderer fallback. AV1 and software HEVC currently use
Vulkan upload buffers. A global memory barrier before each render pass covers
externally written storage after the native producer has completed.

All four UI/video SPIR-V modules pass spirv-val. Native video modules are built
from local GLSL during each application build. Host regression tests cover
keyframe configuration, registration budget fallback, frame/pool lifetimes,
multiple submissions retaining the same frame, queue drops, errors and recovery.
The final link verifier requires native Vulkan/NanoVG symbols and rejects
GL/EGL/Zink archives and API symbols. These are build/ownership checks, not GPU
or display measurements. Native Vulkan startup, streaming, zero-copy counters
and resizing require console validation for the final NRO hash.

### Superseded GL experiment

The first direct-buffer candidate (NRO
`1bc47580bc36127fcf0004c85664421cfda06700f70c43af8fcbe4680bcc7b7e`)
started with GL 4.6 Core, advertised pinned memory and rendered the user's
stream, but import after native pool registration returned GL_INVALID_OPERATION
(0x502). Playback used texture upload. Evidence is retained in
`build/switch2/zero-copy-first-validation.json` and its identified log. A later
shared-GL allocation-order candidate was built/uploaded but was superseded by
the user's explicit native-Vulkan requirement before qualification. Neither
candidate establishes native Vulkan or zero-copy operation. Those GL renderer
sources were removed from the active source tree; generic GL platforms and
Apple Metal remain separate backends.

### Texel sampling and frame-driven presentation

H.264's shared decoder allocation can also expose R8/R8G8 uniform texel-buffer
views. The video shader reads those views without a frame upload or GPU copy.
Unsupported formats, view limits or pipeline creation retain direct storage-buffer
sampling; a rejected texel-capable import retries storage-only import first.
AV1 and software-decoded frames retain their explicit copy path.

Full-screen video submits on a newly consumed decoded frame, with a 34 ms
keepalive. Decoder arrivals wake the input/UI loop; that loop continues running
when an unchanged GPU frame is skipped. Stream menus restore continuous UI
presentation. A held swapchain acquisition is reused until submission, and its
semaphore is consumed before swapchain recreation.

`SWITCH2_VULKAN_VIDEO: source=direct-decoder-buffer sampling=texel-buffer` names
the selected path. `SWITCH2_VULKAN_PRESENT` counts submitted new/repeated video
frames, direct/copy sources, texel/storage sampling and skipped presentations.
Command-recording counts are separate and cannot qualify submitted video.

After testing H.264 at 60 and 120 FPS, close Moonlight and reopen FTP:

```sh
python3 scripts/switch2/capture.py --host 192.168.1.182 \
  --require-direct-texel --require-frame-driven
```

For a non-default build directory, pass `--build-dir` to both `deploy.py` and
`capture.py`. Capture checks the installed NRO hash and the latest startup build
ID before qualifying the sharing/submission counters. This verifies the
application's buffer path, not undisclosed driver-internal copies or scanout.

## Automatic docked output and 4K streaming

The native window uses `nwindowSetModeDimensions(window, 0, 0, 0, 0)`. The SDK
specifies 1920x1080 handheld and the console's configured TV output docked,
including 3840x2160. This follows the active console output rather than the TV's
maximum EDID resolution. Native window changes cause Vulkan swapchain/attachment
recreation. Changes are detected even when the console stays docked and only
its TV resolution setting changes. Borealis uses the actual framebuffer while
retaining logical UI/touch scaling, with host hit tests at 1080p/1440p/4K.

The resolution menu adds `2160p (4K)` for a 3840x2160 stream. `Native` reads the
current native-window size when starting a stream. Display resizing during a
stream scales the existing decoded size; a different negotiated stream size
takes effect on the next connection. Frame rate remains a separate setting.

Service registration now starts within both its 60-FPS declaration and
248,832,000-pixel/s registration budget: up to 60 at 1080p and 30 at 4K. The
negotiated stream FPS stays unchanged and every input frame is submitted.
Native-buffer sharing at 4K was subsequently confirmed below. TV hot-switch
behavior and sustained 4K/120 decoding remain unverified.


### Native Vulkan device qualification and shared-memory bridge

NRO `b0ce5410b2196654a4e1139fde393dbc32ac4d8dfbc288154d2187acad22ff2a`
(build `3609cf5cb5fd96f61772`) was confirmed by the user to display the UI and
stream correctly. The identified log is `device-vulkan-20261113-223713.log`,
SHA-256 `260893a8b119771bfe6c42ff3f433f3f6a9760aaef1ecc5e40ed12fbd4e6b3e5`.
It identifies the native NVK/OunceVK device and 1920x1080 color-attachment
swapchain. The tested stream was actually H.264 3840x2160, requested at 120 FPS,
with service registration at 30. Input and decoded rates varied together; this
run does not establish 4K/120 decoding. The UI initially had no dynamic viewport
or scissor; initializing them at every Vulkan frame fixed the black UI.

That run had zero direct-buffer draws: importing into Vulkan succeeded, but
ordinary native-memory creation then returned 0xd401. It attempted to set the
same native memory attribute a second time. The explicit CPU-copy path kept
playback working. This result qualifies native UI/4K streaming, not zero-copy.

The next bridge captures GPU range handles returned by the public
`gsvGpuRegisterMemory` during the Vulkan host import. For a fully covered range,
it reserves native image memory and uses `gsvRegisterImageMemory` with that GPU
handle and the exact range offset, as the SDK's GPU-backed image path does.
The native memory object borrows the GPU pin (`has_attribute=false`). Native
images/memory are closed first, then the Vulkan import is released, then the
SDK frees the original allocation. Missing coverage or a rejected view uses the
existing explicit fallback. No memory-query results, kernel permissions or
private driver object layouts are modified. This bridge still needs device
qualification for its own build ID and NRO hash.


### Confirmed 4K native-buffer sharing and 4K60 recovery candidate

NRO `c26b949079ee98d63e6d90829777080a9893b2751b3d3e7e901598a4d1bb2f14`
(build `d61348820d43544767d6`) successfully registered GPU-backed native images
and rendered the user's working 4K H.264 stream. The identified log is
`device-vulkan-20261113-224819.log`, SHA-256
`83622a199530853cddc9416ffc05a0a9bb0b6512ad7ffffca67ecd1eade1b7f8`.
The final uninterrupted session logged 844 direct-buffer draws and zero copy
draws; decoded rates were approximately 58-61 FPS in its central samples, with
lower boundary samples. The stream requested 120 FPS and registered at 30.
This verifies sharing/no pixel-copy fallback, not 4K/120 decode or scanout.
The record is `build/switch2/native-vulkan-zero-copy-validation.json`.

The same capture exposed two recovery problems. A 4K60 registration failed with
0x17bc6f, which differed from the 120-FPS rejection that triggered the old
fallback. Also, output leases retained an old decoder session after a submission
timeout, so a frozen last frame could keep consuming its registration budget.
The new candidate uses the size/rate registration budget up front, stops/closes
the native decoder session immediately during recovery, and keeps only its
images/work allocation until final GPU references are released. Host mocks
require that a new decoder can be created while the old frame/pool is retained.
Large access units receive a bounded 50-ms submission allowance; dropped output
slots are released again after draining and before submitting. The full sample
is still never retried after a partial timeout. This candidate was subsequently qualified on the console, as recorded below.


### Confirmed native Vulkan 4K60 and reconnect (2026-11-14)

The installed NRO is `3cf961d6766c1b3f90e2cab0b9972eb3993f883288aa75a20a7bb8ff85697c3f`,
build `8696edbc5b8c1e85107a`. The user confirmed both 4K60 and a disconnect/reconnect.
The log `device-vulkan-20261113-232256.log` (UTC capture filename; console run
00:22 on November 14) has SHA-256
`da202e9f88dfa15d533397f31cf2d9ccae2b2920b2336280247108b91455246e`.
The device NRO hash, startup build ID and expected build all match.

Both sessions initialized at 3840x2160, stream FPS 60 and registration FPS 30.
All sixteen GPU-backed native image registrations succeeded. The periodic
snapshots recorded 1,089 and 726 direct-buffer draws (1,815 total), with zero
copy draws. The final decode snapshots were 380 and 303 frames. No native
decoder creation, submission, acquisition or import errors appeared. Decoded
rates followed the incoming cadence, with reported ranges of 27.3-60.0 and
33.9-56.9 FPS; these short variable-input sessions do not establish locked
60-FPS decode or scanout. Direct draw counts include re-presentation of retained
frames and are not unique decoded-frame counts.

Evidence is `build/switch2/native-vulkan-4k60-validation.json`. The matching
validation/deployment records mark native Vulkan, H.264 4K60 setting, zero-copy
and reconnect verified. This run used a 1920x1080 swapchain. Automatic docked
sizing is implemented, but actual dock/TV resolution switching remains untested.
