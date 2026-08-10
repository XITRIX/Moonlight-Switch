# Artemis Switch → Moonlight-Switch feature parity notes

This document is for **XITRIX** and other Moonlight-Switch maintainers.

**Thank you first.** Artemis Switch exists because of [Moonlight-Switch](https://github.com/XITRIX/Moonlight-Switch). The Borealis UI, GameStream client, FFmpeg/NVDEC path, deko3d renderer, and input stack remain the foundation. Nothing here is a demand to merge Artemis code as-is — only an optional map of Switch-focused ideas that proved useful in the fork, offered as inspiration.

Upstream repo: https://github.com/XITRIX/Moonlight-Switch  
Fork: https://github.com/antoinebou12/Artemis-Switch

## Design principles used in Artemis

1. Prefer small, testable helpers over large UI rewrites.
2. Keep Sunshine as the default host path; gate Apollo-only behavior behind capability checks.
3. Prefer read-only diagnostics (clocks, battery, controller state) over write APIs.
4. Do not invent GameStream APIs that do not exist (for example there is no remote “add app”; use the host web UI on `:47990`).

## Feature areas (inspiration checklist)

### Presentation / video

| Idea | Why it helped on Switch | Artemis pointers |
| --- | --- | --- |
| Unified Fit / Fill / Stretch / Zoom-Pan / Full-range | One presentation path avoids fighting filtered vs direct renderer modes | `DisplayTransform`, `RendererPresentationPolicy`, deko3d texture path |
| Live display transform without swapping renderers | Safer mid-stream tweaks | Settings + overlay Options |

### Performance visibility

| Idea | Why it helped on Switch | Artemis pointers |
| --- | --- | --- |
| Lite performance snapshot (decode / render / FPS / queue / Wi‑Fi) | Players can see lag sources without a heavy overlay | `PerformanceLite`, Performance tab |
| Read-only Switch clocks (CPU / GPU / EMC), mode, battery | Explains docked vs handheld behavior; no `clkrst` writes | `collectSwitchRuntimeMetadata()` |

### Input / controllers

| Idea | Why it helped on Switch | Artemis pointers |
| --- | --- | --- |
| Multi-slot controller diagnostics + rumble tests | Detached Joy-Cons show up as separate pads | `ControllerDiagnostics`, overlay diagnostics |
| Rumble both motors / rumble all pads | Low/high alone only hits one Joy-Con motor mapping | Borealis `sendRumble(slot, high, low)` |
| Mouse speed as true 0.1x–2.0x linear scale | Clearer than a mislabeled “acceleration” curve | `Settings::mouse_speed_scale()` |
| Host keyboard shortcuts + server commands (capability-gated) | Apollo/Sunshine extras without breaking plain Sunshine | `HostKeyboardShortcuts`, host capabilities |

### Stream UX

| Idea | Why it helped on Switch | Artemis pointers |
| --- | --- | --- |
| Slim Quick Actions (keyboard, move L/R, shortcuts, mouse) | Less clutter mid-stream | In-game overlay Quick tab |
| Host web config entry (`https://host:47990/`) | Honest “add apps on the PC” path | App list + Host tab |
| Handheld / docked resolution presets in settings | Keeps virtual-display style choices out of the live menu | Artemis settings presets |

### Apollo (optional)

| Idea | Why it helped | Artemis pointers |
| --- | --- | --- |
| Capability detection before showing Apollo-only controls | Avoids dead UI on Sunshine | `HostCapabilities`, Apollo host options store |

## What we deliberately did **not** push as “must merge”

- Auto-tune / broken performance-writing paths that fought the session.
- Fake catalog write APIs for adding apps over GameStream.
- Clock overclocking (read-only metadata only).

## Suggested upstream adoption order

If any of this is useful upstream, a gentle order would be:

1. Credits / About link to keep fork provenance clear (already common courtesy).
2. Performance lite rows (pure UI + existing session stats).
3. Mouse speed remapping + label clarity.
4. Controller diagnostics / dual-motor rumble test.
5. Presentation policy helpers (larger surface; review carefully).
6. Apollo capability gating (only if you want Apollo UX in mainline).

## Contact

Maintainer of the Artemis Switch fork: **antoinebou12**  
Original Moonlight-Switch author: **XITRIX** — thank you again.
