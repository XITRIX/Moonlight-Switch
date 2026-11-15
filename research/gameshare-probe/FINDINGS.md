# GameShare hosting investigation: device findings

## Native initialization now confirmed; matchmaking still fails

Probe 0.3.1 is installed. Actual console tests confirm initialization, real
transfer-memory registration, a separate player1 display/layer, guest HID
memory and the normal native communication check. All succeed. Both
matchmaking commands 504/511 return 0x2fab1f (287/6101), whose exact semantic
meaning remains unknown. No advertisement, guest connection or image stream
exists. Test resources were cleaned up and the probe remains open and idle.
See HOST_BRINGUP.md for exact evidence, corrections and remaining dependency.
Older sections predate these results.

## Latest public-source and Civ7 follow-up

Civ7's actual loaded SDK is 18.3.1; all 23,046 dynamic symbols were scanned,
with no exported StreamPlay/GameShare names. Its loader cache is uninitialized.
No existing usable retained-FS endpoint was recovered. HEN and RO were not
modified, and no SDK was extracted.

Public reconstructed SDK code and our exact firmware's scoped IPC command
layouts have now been found. They provide new initialization, real memory-size
query and matchmaking leads, but no working host or Nintendo SDK binary.
See PUBLIC_SDK_RESEARCH.md for evidence and precise limitations.

## First console run, 2026-11-15

The user launched GameShare Probe 0.1.0, then made FTP available again. A bounded
FTP read retrieved `sd:/switch/gameshare-probe/report.json`; the probe HTTP port
was no longer accepting connections. The report's last uptime was 8.884 seconds.
This establishes a completed probe run, not a currently reachable probe process.
Raw local evidence is `evidence/device-report-sd.json`.

Observed environment: firmware **23.0.1**, `ounce-hbloader`, kaiHEN hint true.
The HOS version hint was 20.0.0 while the actual successful settings query returned
23.0.1. Do not mistake that SDK fallback hint for the actual firmware version.
Network initialization and listener creation succeeded during the run.

| Service | Runtime SM result | Staged ACI0 / ACID client permission |
| --- | --- | --- |
| `stpl:u` | Opened and closed, success | Present / present |
| `chat:u` | Opened and closed, success | Present / present |
| `ldn:u` | Opened and closed, success | Present / present |
| `lrp:u` | `0x1015` | Absent / absent |
| `stpl:sys` | `0x1015` | Absent / absent |
| `chat:sp` | `0x1015` | Absent / absent |
| `ldn:s` | `0x1015` | Absent / absent |
| `lp2p:app` | `0x1015` | Absent / absent |
| `spl:lrp` | `0x1015` | Absent / absent |

`0x1015` decodes to module 21, description 8: `NotAllowed` in the
[Atmosphere SM result definitions](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libvapours/include/vapours/results/sm_results.hpp).
It does not mean that the service is not registered.

The separately read staged host policy is `SB`, program ID `0400569027ef8000`,
SHA-256 `b033fa9961f1da43f3cf6d0124e3964f5bf82f7750a1adf9f33b745299f168d5`.
Decoded metadata is retained in `evidence/device-game-policy.json`; signed policy
bytes were not saved by the audit. Static staged metadata is not a fresh query
of the running kernel policy, but its service entries agree with every observed
open result. No policy was modified.

Own-process mapping enumeration completed with 29 regions and two recognized
MOD0 modules. Each exposed three dynamic symbols; neither contained a selected
StreamPlay/SharePlay/Npad symbol. This says nothing about the exports of SDKs
inside other games or system processes. It confirms that direct symbol lookup
in this NRO's process does not currently supply the hosting API.

## Implication

The native application-facing `stpl:u` path is reachable under the existing
kaiHEN deployment. Direct access to LRP or other system-level interfaces is not
needed merely to investigate that path. Whether `stpl:u` supports a homebrew
hosting session still requires interface creation, exact startup/display/input
ABIs and actual video/input evidence.

## Persistent agent: verified console execution

The installed probe 0.3.0 has SHA-256
`e610e55d1f269b4731d6a3bf5a8ff56f6f330714ff38fc343c204eb841fa0fb0`.
Session `34f3dbb056d8d7c4` accepted native AArch64 snippets through its paired
HTTP listener. Two hello jobs returned 42 and persistent counters 1 then 2.
Evidence: `evidence/jobs/20261115T205639.664381Z` and
`evidence/jobs/20261115T205657.696584Z`. This establishes real JIT execution,
publication, return and state retention, beyond an offline build.

## StreamPlay scoped interface created successfully

The older no-input creator documented on
[Switch 2 Chat services](https://switchbrew.org/wiki/Switch_2%3A_Chat_services)
did not match this firmware: CMIF without input and TIPC without input each
returned kernel `0xf601` (ConnectionClosed), including fresh raw sessions that
did not send the SDK's automatic pointer-buffer control query. Consequently,
these failures do **not** establish that the control query caused the closure.

Job 8 sent CMIF command 0 with one u32 **0x30000** and succeeded, receiving one
moved scoped-interface handle. This was a bounded compatibility hypothesis
from firmware 23's documented versioned creator on
[NGC services](https://switchbrew.org/wiki/NGC_services), then tested directly
on the Switch 2. Evidence: `evidence/jobs/20261115T210330.283360Z`.
The raw root and child are retained in shared state offsets 32 and 36.
This validates the factory request on firmware 23.0.1; it does not establish
that the rest of the Switch 1 shim ABI applies to Switch 2.

**No host initialization, discovery advertisement, guest connection, video,
audio, input forwarding or simultaneous PC/GameShare networking is established.**
The next missing evidence is the scoped object's actual initialization,
session-opening and display/streaming request layouts.

## Read-only system executable investigation

The existing HEN protocol 6 provided a homebrew session, cloned FS proxy and
retained FS-identity SM copy. HEN status initialization_result was zero.
Evidence: `evidence/jobs/20261115T210559.381085Z`.
The identity SM copy must never receive Initialize or CMIF Close; only close
the copied kernel handle when finished. The original RO override and the
agent's private SM are borrowed and must not be closed.

NCM BuiltInSystem metadata listed 90 SystemProgram records, including native
Chat `040000000000004a`. Evidence:
`evidence/jobs/20261115T210742.716675Z/output.bin` (two s32 counts, then 16-byte
NcmContentMetaKey records). Opening `fsp-ldr` returned `0x615`
(SM OutOfSessions). Known loader overrides exposed no `fsp-ldr` handle.

Job 13 used official libnx NCM layouts to resolve Chat's Program content ID and
its installed path successfully. Opening CodeFS through FS proxy command 10
with that path failed with `0xbcaee202`; command 7 without a path had also
failed (`0xbc6ee202`). These are recorded service errors, not absent content.
Evidence: `evidence/jobs/20261115T211738.493328Z`.
No system executable was read through these requests. No loader connections,
program policies, boot files or saves were changed.

Raw owned HEN/FS/SM copies occupy shared state offsets 64, 68 and 72; NCM root
and database are at 96 and 100. These raw sessions bypass the agent registry
and need explicit kernel-handle cleanup or process exit. Managed resource ID 1
is the stale, closed-by-server session from the original failed creator.

Public source exposes StreamPlay SDK names including Initialize,
CreateIndividualDisplays, OpenSessionWithMatchmakeSupport and StartStreaming,
but the available implementations are emulator stubs, not the real IPC call
layouts. A matching installed game's SDK or the native Chat executable is
needed to derive those requests before attempting host startup.

Additional read-only jobs:

- Job 14 listed Application/Patch metadata on BuiltInUser, SD and GameCard.
  Evidence: `evidence/jobs/20261115T212209.338830Z`. The inserted card returned
  application key `0100c3601518c000`, version 0; the only installed Patch key
  returned was `0400569027ef8800`, version 0x10000. This is a snapshot of these
  NCM lists, not proof that another card lacks native SDK code or that the
  user owns no other titles.
- Job 15 queried the documented HEN plugin catalog and returned count **0**.
  Evidence: `evidence/jobs/20261115T212528.602084Z`. No plugin was opened.

The official kaiHEN 20261114d source was inspected separately and selected
source files/hashes retained under `evidence/sources/kaihen-20261114d/`.
Its GameArm retains a loader root internally for policy capture; the documented
homebrew commands 16 through 22 do not export it. The source is a newer public
snapshot, **not a byte-for-byte identification of the running older HEN**.
The older bundled SDK pins ouncehen 9451c44; that revision was not found in the
public kaihen repository. No core replacement or binary patch was prepared or
deployed. An available source of read-only executable access remains the
prerequisite for deriving further startup ABIs.

## Pikmin demo reference check

The original Switch Pikmin 4 demo is not a GameShare host reference. Nintendo's
[Switch 2 Edition product page](https://www.nintendo.com/en-gb/Games/Nintendo-Switch-2-Edition/Pikmin-4-Nintendo-Switch-2-Edition-Dandori-Academy-3188744.html)
confirms GameShare for Dandori Battles in the full game. Its separate
[demo announcement](https://www.nintendo.com/en-ca/whatsnew/try-the-pikmin-4-nintendo-switch-2-edition-dandori-academy-demo/)
confirms a Switch 2 demo exists, but does not establish demo GameShare support.
Neither product-level feature tags nor the existence of a demo establish that
the demo can host. An otherwise disabled demo may still contain useful SDK
code, which would require inspecting that specific artifact.

The user identified their demo as the Switch 2 Edition and supplied a photo of
Kirby Air Riders and Kirby and the Forgotten Land Switch 2 Edition cartridges.
They have not updated the cartridges and cannot connect this console to Nintendo
servers. Preserve that constraint. Nintendo's
[Air Riders update notes](https://en-americas-support.nintendo.com/app/answers/detail/a_id/68635/p/1095/c/950)
say its GameShare feature was added in 1.3.0. Its base cartridge therefore is
not a working GameShare UI reference, but its SDK **may** still contain the
necessary general StreamPlay functions. That requires artifact inspection;
neither presence nor absence of those functions has been established.

## Public protocol implementation located

The subsequent online search located
[NS2GameShare](https://github.com/mizuyoukanao/NS2GameShare), an MIT-licensed
Windows receiver with LDN/LRP/LSP, H.264/Opus and input-feedback source and
documentation. Its protocol reference is explicitly incomplete and its client
does not verify host signatures. It implements the opposite direction from our
requested native host, so it does not replace the missing scoped-interface ABI.
Pinned source, hashes, extraction-tool findings and limitations are recorded in
`ONLINE_RESEARCH.md`. No public Kirby SDK dump was obtained, and no additional
console commands, game launches, dumps or core changes were made during this
online search.

## Probe-based cartridge adapter

`sdk_extract.py` and `examples/sdk_extract.c` now provide cartridge inventory
and a bounded SDK transfer implementation. The inventory was verified on the
inserted base Kirby Air Riders card. Extraction is still blocked: `fsp-ldr`
returns OutOfSessions; the current HEN lacks a loader-root/CodeFS export; an
alternate native-attribute CodeFS mount through the ordinary FS proxy also
failed. **No cartridge SDK was extracted.**

The user prohibited HEN/RO writes. A locally built experimental HEN extension
was not deployed and is no longer an authorized route. A separately authorized
read-only copy of the installed HEN matches the local testship image; its static
dispatcher was inspected for an existing interface. See `SDK_EXTRACTION.md` for
the exact evidence, limitations and tests. HEN and RO remain unchanged.
