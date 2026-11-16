# Matchmaking contract audit, 2026-11-16

This is a public-source audit, not another console execution result. No console
request, HEN/RO modification, downloaded-binary execution or identity change was
performed for this audit.

## Supported conclusions

The currently indexed fefw revision is
`b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96`. Its reconstructed
`OpenSessionWithMatchmakeSupport` explicitly compares the service result with
decimal 3123999, i.e. **0x2fab1f, module 287 / description 6101**. It also lists
6012, 6015, 6102–6105, 6401, 6408 and 6601. No semantic names are supplied.
`IsAbortNeeded` treats module 287 descriptions 5000–7999 as abort-worthy. Thus
the observed failure is an anticipated SDK failure family, not an unknown
transport error; this does **not** identify its exact cause or permission policy.

Its API-side checks constrain these MatchmakeConfig locations:

| Offset | Constraint observed in reconstruction | Meaning status |
| --- | --- | --- |
| 0, 1 | first >= 1, first <= second, second <= 3 | Consistent with min/max guest count |
| 2, 3 | first >= 1, first <= second, second <= 2 | Consistent with min/max pads per guest |
| 8 | u32 < 64; bits 3 and 4 cannot both be set | Flag meanings incomplete |
| 12 | u16 < 32 | Meaning unknown |
| 14 | u16 in {0, 1, 3} | Meaning unknown |

The existing `{1,1,1,1}`, flags 8, feature 0, other 0 configuration meets these
checks. Fields +4 and +16..31 remain unestablished. Flag bits 3/4 control whether
the SDK calls the ordinary StreamPlay parental communication check, with bit 3
also selecting error UI behavior. These facts do not establish whether lower
flag bits select a transport, guest policy or another option.

The newer call builds a zeroed additional 32-byte local structure, putting its
`StreamingFrameRate` argument in the first byte, before calling vtable +408.
That is consistent with cmd511's known 64-byte wire input, but its missing
writer prevents independently proving the concatenation/order. The older
Recompia proxy analysis identifies cmd504's 32-byte input and 128-byte output.

The SDK obtains the 128-byte applet argument **from the service**, then creates
the matchmaking library applet, pushes CommonArguments version 0x10000 plus the
argument, and reads 176 output bytes. Commands associated with cleanup after an
unsuccessful applet (vtable +392 / +376, mapped to 509 / 507 by prior analysis)
are reached after the applet, not additional pre-matchmaking prerequisites.
Do not insert them before preparation or fabricate the applet argument.

## Search boundary and remaining gap

The current fefw tree still only forward-declares MatchmakeConfig and keeps
matching IUserService slots reserved. Selected CMIF wrapper routines call
unprovided `SdkRoutine_*` writers; searching those named writers did not find
their definitions. The current Recompia implementation still supplies zero
matching arguments to a cancelled emulator applet. BananzaNative's StreamPlay
implementation expressly streams to nobody. These are useful ABI references,
not proven interoperable native hosts.

No error name for 6101 or full native matching configuration was found through
the bounded public code/web searches. Exact hexadecimal searches also returned
unrelated color tables; they are not console evidence. This does not establish
global absence of a definition.

If server code cannot be read, useful finite tests are a source-valid flags=0
baseline and isolated low-bit variations, preserving all actual parental
checks. Their semantics remain hypotheses: only a changed result would justify
expanding them. Repeating the same 0x2fab1f across variations is a reason to
obtain server validation/call-trace evidence, not an invitation to brute-force
opaque fields or spoof title metadata. A real known-good host trace remains
the strongest missing reference.

## Primary references

- [fefw matching API and error comparisons](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/src/nn/streamplay/streamplay_api_impl.c)
- [fefw result policy and real transfer-memory order](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/src/nn/streamplay/detail/streamplay_detail_util.cpp)
- [fefw matchmaking applet bridge](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/src/nn/streamplay/detail/unplaced.c)
- [fefw partially recovered interface](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/include/nn/streamplay/detail/streamplay_user_service.hpp)
- [fefw CMIF wrappers with missing writers](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/src/nn/sf/cmif/client/unplaced_2.c)
- [Recompia proxy analysis and emulator stubs](https://github.com/Dittobyte/Recompia/blob/54156d1d52a50b5c11b187ab9064eb2152dbcce7/runtime/src/services/misc_streamplay.cpp)
- [Recompia observed emulator call-path notes](https://github.com/Dittobyte/Recompia/blob/54156d1d52a50b5c11b187ab9064eb2152dbcce7/runtime/docs/IPC_ANSWERS.md)
- [BananzaNative offline stubs](https://github.com/CrownParkComputing/BananzaNative/blob/f63884203c609462609cb2942c5eb2435538a4b4/host/nn_streamplay.c)

## Follow-up: public server-code search

The original borntohonk repository's current HEAD remains
`5732ea48463db991eee2db49a70b9006254c934e` (2026-11-14). A complete recursive
tree enumerated 331 entries with `truncated=false`. Its only Chat-specific
artifact is `ipc_json_dumps/ns2/23.0.1/programs/Chat_services.json`; no Chat
binary, assembly, pseudocode or result-definition file is in that tree. This is
a repository-scoped absence, not an assertion about private or deleted files.

The accompanying [ipcserver README](https://github.com/borntohonk/Switch-Ghidra-Guides/blob/5732ea48463db991eee2db49a70b9006254c934e/ipcserver/README.md)
explains that `func` is a heuristic match to a command implementation address;
the heuristic does not always work. The JSON's cmd504 `0x71004FD2D0` and cmd511
`0x71004FD400` therefore locate candidate implementations **in the original
module**, but do not provide their bytes or validation logic. Exact-address
public code searches found no Chat function body. An address collision in an
SSBU function dump is an unrelated binary, not a usable Chat decompilation.

Further searches for result naming variants and 2287-6101 did not yield a
native definition of description 6101. Search results for generic multi-term
queries frequently included unrelated domains; only inspected relevant files
are evidence. The known hatch and NS2GameShare repositories' current heads are
unchanged from the preserved source manifests; their trees do not supply the
missing native Chat host configuration/validation implementation. An apparent
new `IUserService`/matchmaking probe in wwz-switch-lab concerns a game's own
online services, not Nintendo StreamPlay, and is not a lead for this error.

No new source justifies an extra SetupSession call before preparation or assigns
semantics to the lower matching flag bits. The next discriminator still needs
actual server validation code or a known-good host request. Arbitrary additional
setup commands are unsupported by the inspected SDK call path.

## Initialize argument ordering check

The SDK reconstruction calls its Initialize proxy with a process-ID argument
before the type/mode/guest arguments. C++ parameter order is not sufficient
evidence for placing PID first on the wire. Atmosphere's
[CMIF serializer](https://github.com/Atmosphere-NX/Atmosphere/blob/3e8915011e1de89f595462d929c1cc3294470896/libraries/libstratosphere/include/stratosphere/sf/impl/sf_impl_command_serialization.hpp)
`RawDataOffsetCalculator` stable-sorts raw arguments by **ascending alignment**.
ClientProcessId remains an ordinary eight-byte raw argument as well as selecting
the HIPC send-PID flag. With separate u8 type/mode, u32 count and u64 PID fields,
this convention predicts the probe's existing offsets 0/1, 4 and 8, regardless
of logical signature order. This is a corroborating public implementation, not
the missing original StreamPlay writer.

The primary investigator subsequently reported actual-console job12: existing
layout, type0/mode1, command502 returned0 and command503 returned1. Matchmaking
still returned6101. This runtime echo distinguishes the intended type/mode
bytes from a PID-first reinterpretation; no alternate PID-first request was
sent. See the primary session evidence for job12, rather than attributing this
hardware observation to the read-only source audit.

## System-service alternative

Public searches found `stpl:sys` and `ISystemShimScopedObject` in Eden-family
emulators and nxemu. The inspected implementations only create the system
object and retain names such as `Cmd106`, `Cmd108`, `Cmd208` etc.; operations
return unsupported or have no implementation. They supply no semantic host
initialization contract. An emulator test named `stpl_host` checks service
registration and error responses, not a discoverable native GameShare host.

The fefw tree contains the application IUserService client but no reconstructed
StreamPlay system client. The Adubbz loader resource contains service names
only. Original Switch splay client IPC metadata shows use of `stpl:sys`, but
matching commands remain unnamed. Many entries are explicitly `stub_only`
with no recorded callers; their presence is not an observed execution trace.
Switch2 SplayApplet/SystemApplet service lists also include `stpl:sys`; those
lists do not establish a host-only alternative. No inspected source assigns
safe semantics to Chat's system scoped commands3,6,8,208,210,211 or the second
ISystemService interface. Input/output sizes alone cannot prove an operation
is read-only or valid out of session.

References for the system-service search:

- [Eglantine system-service stubs](https://github.com/Eglantine-Community/Eglantine/blob/cf6cfe9caa6e160d3562742308a38eb3c9014789/src/core/hle/service/ngc/ngc.cpp)
- [nxemu copied command names and error-only handling](https://github.com/N3xoX1/nxemu/blob/f9069927b662e893037e1318e62b6ac0d30f12af/src/nxemu-os/core/hle/service/ngc/streamplay.cpp)
- [original Switch splay client IPC metadata](https://github.com/borntohonk/Switch-Ghidra-Guides/blob/5732ea48463db991eee2db49a70b9006254c934e/ipc_json_dumps/ns1/23.0.1/applets/splay.json)
- [Switch2 SplayApplet metadata](https://github.com/borntohonk/Switch-Ghidra-Guides/blob/5732ea48463db991eee2db49a70b9006254c934e/ipc_json_dumps/ns2/23.0.1/applets/SplayApplet.json)
