# Public SDK research, 2026-11-15

Follow-up hardware results are in HOST_BRINGUP.md: native initialization,
memory/display setup and native communication checks succeed. Matchmaking
still fails with 0x2fab1f. Both factory parameters 0x30000 and 0x12 were
accepted on hardware; earlier notes about only one qualified value are
superseded. No host advertisement or image stream is established.

No verified downloadable Nintendo Switch 2 SDK NSO or full development SDK was
obtained. Direct GitHub code search did find reconstructed Nintendo SDK code
and an exact-firmware IPC inventory, substantially beyond the earlier stubs.
Selected source is in evidence/sources/public-sdk-20261115/, with pinned URLs
and hashes in source-manifest.json. No downloaded code was executed.

## Primary sources

- vexcited/fefw, c57bc31f85bb67a322b816ceeb53d4027fd50b3a:
  https://github.com/vexcited/fefw/tree/c57bc31f85bb67a322b816ceeb53d4027fd50b3a/src/nn/streamplay
  Reconstructed Fire Emblem Fortune's Weave SDK code includes initialization,
  work-buffer/transfer-memory allocation, matchmaking, StartStreaming,
  individual displays and guest HID. Its README says no game files are
  included, no byte matches yet, and behavior verification runs locally in an
  emulator. Several files remain pseudocode with unresolved data/functions.
  The clean IUserService header has reserved slots and is incomplete.

- borntohonk/Switch-Ghidra-Guides, 5732ea48463db991eee2db49a70b9006254c934e:
  https://github.com/borntohonk/Switch-Ghidra-Guides/blob/5732ea48463db991eee2db49a70b9006254c934e/ipc_json_dumps/ns2/23.0.1/programs/Chat_services.json
  Static IPC inventory for our exact Switch 2 firmware, including factory
  input size and scoped-command sizes/handles/PID annotations. Most command
  names are unknown. Only the two user interfaces were retained locally.

- Dittobyte/Recompia, 5e0186dab872ab94270464c2af434fe34cae0132:
  https://github.com/Dittobyte/Recompia/blob/5e0186dab872ab94270464c2af434fe34cae0132/runtime/src/services/misc_streamplay.cpp
  Comments map original SDK proxies to command IDs and data shapes. The
  emulator deliberately supplies fake page-size memory sizes, empty guest HID
  and cancelled matchmaking. It is not a working host; never copy its fake
  sizes into a console host. Its ABI analysis is useful but unverified here.

- rickyoliva/hatch, f0fbf822d8cdb18844bf1ceb9b7e99da3c61624d:
  https://github.com/rickyoliva/hatch/blob/f0fbf822d8cdb18844bf1ceb9b7e99da3c61624d/docs/11-gameshare-host-bringup.md
  Proposed Switch 1 host investigation, not a success report or SDK dump.
  Its complete-host-stack claim remains a hypothesis; interface presence does
  not prove Switch 1 hosting capability.

## Candidate scoped calls

Names are hypotheses from SDK/emulator analysis. Raw sizes and handle/PID
annotations are independently present in the exact 23.0.1 IPC inventory.
These calls have not been hardware-qualified in our session.

| Command | Candidate meaning | Input / output bytes |
| --- | --- | --- |
| 1 | Required memory size | 8 / 4; candidate u8 type, u8 mode, padding, u32 guests |
| 2 | Initialize | 16 / 0; send PID; confirm exact PID field placement |
| 3 | Finalize | 0 / 0 |
| 4 | Start a guest stream | 4 / 0; not a whole host opener |
| 7 | Connection event | 0 / 0; one output handle |
| 8 | Connection state | 0 / 8 |
| 9 | Work-buffer sizes | 8 / 32; four u64 outputs |
| 300 | Guest HID shared memory | 0 / 16; one output handle |
| 502, 503 | Configured type, mode | 0 / 1 each |
| 504 | Older matchmaking preparation | 32 / 128 |
| 505 | Global work buffer | 8 / 0; one input handle |
| 506 | Per-guest work buffers | 32 / 0; three input handles |
| 510 | Audio output settings | 16 / 0 |
| 511 | Additional/newer matchmaking form | 64 / 128; needs semantic reconciliation |

The reconstructed newer matchmaking path uses a vtable slot corresponding to
511. Do not assume 504 alone reproduces all SDKs' matchmaking. Vtable slots
differ across versions: correlate wire writers and exact-firmware inventory,
not offsets copied between unrelated SDKs. The confirmed factory input stays
u32 0x30000; an emulator's guessed 0x12 does not override the hardware result.

Next native check: new scoped object, bounded queries 1 and 9 for real memory
sizes, optionally 300 with all returned handles closed. Then resolve type/mode
values and initialization layout before sending session-opening commands.
No new native commands were executed during this public-source search.

## Recent dump claim

Original claim: https://x.com/invokertr/status/2121349164369944905
Correction linked by reporter: https://x.com/gezine_dev/status/2121589257857060919
Article:
https://twistedvoxel.com/switch-2-firmware-23-5-0-hacked-days-after-nintendo-patched-major-exploit/

Direct X fetching returned 403. The article's update attributes the dump to a
Switch 1 cartridge binary, not Switch 2 OS firmware. No downloadable module
was found through this claim. A dump demonstration is not evidence that a
downloadable SDK or GameShare implementation was released.

## Civ7 runtime audit

Read-only finite JS inspection found NintendoSdk_nnSdk-18_3_1-Release and NVN
18.3.1. All 23,046 dynamic symbols were scanned; no exported streamplay or
GameShare names matched. This applies only to this module/version, not every
game SDK. Unused game features do not necessarily remove shared SDK exports.

MountCode's getter opens fsp-ldr; its live cached-object pointer and init guard
were zero. We did not invoke the getter. Two historically recorded localhost
retained-FS endpoints timed out on retained-info. Neither supplied usable
current access; historical process-local handles were not reused.

Evidence: evidence/civ7-sdk/20261115T220747.926316Z/, including report.json,
version-and-plt.json, loader-getter.json, loader-cache.json and
old-retained-endpoints.json. No HEN or RO files/code were modified. Civ7
terminal is the last known active console app. No host session, image stream
or cartridge SDK extraction has succeeded.
