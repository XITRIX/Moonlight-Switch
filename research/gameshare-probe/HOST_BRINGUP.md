# Native GameShare bring-up, 2026-11-15

## Latest outcome, 23:12 UTC (November 16 local time)

**Native initialization succeeds; hosting is not established.** Matchmaking
commands 504 and 511 consistently return 0x2fab1f (module 287, description
6101). Its precise semantic name/condition is unknown. Do not call it an
invalid-argument, title-support or permission error without further evidence.
No matchmaking applet, advertisement, guest connection or image stream exists.

Successful actual-console operations now include:

- Factory cmd0 with both u32 0x30000 and u32 0x12. The latter comes from the
  public emulator source; both produce the same later matchmaking failure.
- Cmd1/9 sizing: 45,940,736 bytes for type0/one guest; global 0x200000,
  first 0, second 0x2700000, third 0x2d0000. Modes 0 and 1 have identical
  sizes and the same matchmaking result. Type1 was queried only, not hosted.
- Cmd2 initialization with send-PID and the source-derived 16-byte layout.
- Cmd7 event, cmd300 HID shared memory (0x100000 bytes at offset zero),
  cmd8 state (disconnected), cmd502/503 configured type/mode.
- Real allocation and transfer memories, cmd505 global registration and
  cmd506 per-guest registration. SDK order is global/first/third/second;
  per-guest handles/sizes are first/third/second, including a zero first part.
- Cmd25 controller styles and cmd510 audio settings.
- Separate VI2 player1 display, default endpoint, layer create/attach/select.
- Normal native communication check: pctl:a/s/r are denied (0x1015), but
  the standard libnx fallback pctl opens, domain/factory0/PID succeeds,
  cmd1021 succeeds, and cmd1022 ends communication successfully. No parental
  setting was changed or bypassed.
- Finalize cmd3, detach/clear/close guest layer/display, close transfer handles,
  and free work memory all succeed. Job24 closed all remaining owned diagnostic
  HEN/FS/SM and StreamPlay handles. The resource/handle snapshots are empty.

The probe is open and idle. HEN/RO code and files were not modified. Existing
HEN interfaces were used to obtain its documented SM proxy for read-only ARP
inspection; no ARP writer, identity override or metadata registration was used.

### Probe update and verification

Installed probe 0.3.1 is 440,517 bytes, SHA-256
79ff7da764e2d3de8416912b98fd6644141029f034639d73b305bdf43c2f0392.
FTP staging/readback verified it, retained the pairing key and backed up the old
NRO. Exact old ELF/NRO are in evidence/probe-builds/0.3.0/.

0.3.1 shows recent native logs, adds X to reset the NRO's network while idle,
and resolves probeReconnectNetwork (void(void)) to queue that reset only after
the worker exits. The callback restored remote access across repeated cycles.
The original network failure's cause was not measured; do not conclude that
simultaneous PC networking/GameShare is impossible. Manual X was not tested.

0.3.0 repeated its RNG-derived session ID across two launches. 0.3.1 mixes
the system tick into the ID; the new observed session is 2237b8032fe0f17c.
Old session IDs alone are not a reliable restart guard.

All executed AArch64 payloads and the NRO compile with warnings as errors.
All 14 existing Python tests passed with loopback access. The first sandboxed
test attempt could not connect to its own local servers. These host tests do
not establish the new native behavior; actual console evidence does.

### Identity and metadata checks

PID and ARUID are 0x13b; ProgramId is 0400569027ef8000. ARP reader cmd3 maps
that PID to ApplicationInstanceId 0x48. Cmd1 with the instance ID returned the
actual registered control property: compressed title Stellar Blade Demo;
all eight local communication IDs equal its own ProgramId. This title's
relationship to the matchmaking failure is unproven; no support bit was found.

Correction: the first ARP cmd1 attempt incorrectly used PID/ARUID and returned
0xcc9d. Modern ARP requires ApplicationInstanceId. The corrected read succeeded;
the first error is not evidence of a missing application registration.

Two explicit configuration hypotheses did not help: a selected-user UID in
the opaque 16-byte tail, or the current application's declared communication
ID at offset16. These tests do not establish tail field meanings. The account
UID was retained privately in probe state and not logged.

### Latest evidence and reproducibility

All folders below are under evidence/jobs/:

| Folder | Result |
| --- | --- |
| 20261115T224053.339244Z | Old local cycle; recovered device-journal.json confirms all queries/finalize |
| 20261115T225048.682060Z | 0.3.1 successful cycle and network recovery |
| 20261115T225059.142602Z | Transfer memories accepted; cmd504 fails |
| 20261115T225205.296876Z | Cmd511 also fails |
| 20261115T225533.835698Z | Native pctl check succeeds; same error |
| 20261115T225840.669579Z | Guest display/layer accepted; same error |
| 20261115T230020.202759Z | Own identity and local user presence |
| 20261115T230139.264449Z | UID-tail hypothesis; same error |
| 20261115T230216.415119Z | Audio settings accepted; same error |
| 20261115T230719.641784Z | Correct ARP control read; output.bin is NACP |
| 20261115T230828.563629Z | Own communication-ID hypothesis; same error |
| 20261115T230852.706816Z | Mode1; same error |
| 20261115T231041.468749Z | Factory0x12 accepted |
| 20261115T231051.618211Z | Preparation with factory0x12; same error |
| 20261115T231209.235016Z | Final cleanup successful, no owned resources |

examples/streamplay_prepare.c reproduces the autonomous preparation and cleanup.
Its optional guest-display calls use installed libounce VI2 declarations and
the probe's own existing functions. examples/own_vi2_symbols.h and
build/own-vi2-symbols.json record the exact offsets/16-byte guards for 0.3.1.
ELF/NRO bytes were compared on PC; the openService anchor and each target are
checked again at runtime. Old offset-qualified helpers need the old NRO or
newly generated profiles. No target function in HEN/RO is patched.

The unresolved dependency is the complete MatchmakeConfig/wire contract or
the Chat service's validation of 0x2fab1f. Public reconstructions leave fields
and some writers opaque. A real host call/configuration capture, suitable SDK
module, or server disassembly would identify the condition. No applet was
launched using fabricated zero arguments. Guest connection should wait for a
real host advertisement.

Scope: invoke native StreamPlay from the persistent NRO. Do not modify HEN or
RO files/runtime code. All new code executes in the NRO's own JIT allocation.

## Early hardware observations (superseded by the section above)

Agent session 2237bdade0aa7af9, Switch 2 firmware 23.0.1, LAN 192.168.1.182.

1. Job 1: ordinary service_open(stpl:u) succeeded, agent resource 1.
2. Job 2: managed factory command 0, input u32 0x30000 succeeded, scoped
   agent resource 2. Earlier raw-only qualification now also works managed.
3. Job 3: scoped commands 1 and 9 succeeded for type=0, mode=0, one guest:

   | Field | Actual bytes |
   | --- | ---: |
   | Required memory | 45,940,736 (0x2bd0000) |
   | Global | 2,097,152 (0x200000) |
   | First per-guest part | 0 |
   | Second per-guest part | 40,894,464 (0x2700000) |
   | Third per-guest part | 2,949,120 (0x2d0000) |

   These sum exactly. They are real service outputs, not emulator mock sizes.

4. Job 4: submitted command 2 Initialize, 16-byte input plus send-PID:
   type=0 at byte 0, mode=0 at byte 1, guest count=1 at byte 4, zero u64
   PID placeholder at byte 8. This layout is an explicit hypothesis based on
   Recompia's argument analysis plus the exact firmware's IPC sizes.
   The POST returned accepted/job 4/thread_create. Subsequent LAN requests
   timed out. User then read the physical screen: Job 4 completed, return 0x0.
   **Initialize returned success**, established by user-visible confirmation,
   not by a final PC HTTP response. See user-observation.json in the job folder.
   LAN loss is consistent with a radio-mode change, not directly measured.
   The command was not resent. A local initialize/status/finalize diagnostic
   has been compiled. Asked the user to exit/relaunch the NRO to restore LAN
   before that diagnostic. Wired-LAN availability was asked but not answered.

Evidence: evidence/jobs/20261115T223053.051800Z,
20261115T223103.393595Z, 20261115T223127.979652Z, and
20261115T223218.750617Z (accepted response only for the last one).

## Payloads

- examples/streamplay_memory.c: read-only size queries, bounded type/mode and
  guest count. Output 40 bytes: u32 required size, padding, four u64 sizes.
- examples/streamplay_initialize.c: one initialization attempt, records its
  result and a persistent success flag only after success.
- examples/streamplay_status.c: commands 8/502/503/7/300; all duplicated event
  and shared-memory handles are closed through the agent registry. No mapping.
- examples/streamplay_initialize_cycle.c: locally sequences initialization,
  status queries and finalization without depending on further LAN messages.
  It is compiled offline (1400 bytes) but has not been sent to the console.

State offsets 128..133 are reserved for the initialized flag and type/mode;
the original agent StreamPlay IDs remain at 8 and 12. Success flags do not
survive NRO restart and must never be applied to a different agent session.

## Further source reconciliation

The fefw reconstructed ShowMatchmakeSupportCore creates library applet
0x1000024 for type 0 or 0x1030024 for type 1, pushes CommonArguments version
0x10000, pushes the service-produced 128-byte argument, starts it and reads
176 output bytes. These are source-derived leads, not hardware results.
Its actual matchmaking function uses the newer command 511 form; the
Recompia implementation describes the older 504 form. Do not mix vtable
offsets or fabricate the opaque service-produced applet argument.

No matchmaking preparation, applet launch, advertisement, guest connection or
image submission has been attempted in this native session yet. Do not ask
the user to connect the guest until an actual host advertisement is observed.
