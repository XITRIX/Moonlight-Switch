# Application metadata and initializer audit, 2026-11-16

This is a public-source and local-source audit. No console command, metadata
registration, title identity change, debugger attachment, save operation or
HEN/RO modification was performed by this investigation.

## NACP: what is established

The standard application control property is **0x4000 bytes total**. Its opaque
`PlatformSpecificRegion` occupies **0x3c00..0x3fff**, not a region beginning at
0x4000. Both the local public libounce header and fefw's control-property type
agree with this total size. The current libnx/nxdumptool declarations leave
the platform-specific bytes without named fields.

Switchbrew's NACP reconstruction names a new firmware-23 byte at **0x3569**:
`StreamPlaySaveDataKeyType`. The public Atmosphere generated error-report schema
independently contains the field name `AcpStreamPlaySaveDataKeyType` (numeric
U8, report-field ID 790). Neither source explains its possible values or proves
that it enables hosting. The name alone does not justify changing it. The
same NACP table names `SuppressForceConsoleMode` at 0x34d9; this is not documented
as a GameShare switch either.

No documented `GameShareEnabled`, `IsGameShareSupported` or
`IsStreamPlaySupported` field/API was found in the inspected public headers and
bounded code searches. This is a search result, not proof that firmware has
no metadata checks. In particular, a successful ARP control-property read and
ordinary local-communication IDs do not establish that the StreamPlay service
accepts that application's metadata for every hosting operation.

The GameShare guest applet is documented as receiving application control data
and JPEG images during setup. That proves metadata participates in the protocol,
but not a specific bit that the probe should set. The guest also verifies signed
advertising data. A replacement raw-LDN advertisement is consequently not known
to be sufficient for native interoperability.

## One source-supported initializer difference

The reconstructed initializer `nn_streamplay_764aa0` in fefw's
`src/nn/streamplay/unplaced_2.c` has this ordering:

1. Initialize the user service (vtable +40, mapped to cmd2).
2. Obtain its connection-state event (vtable +80, mapped to cmd7), and attach
   the returned readable handle to the SDK's event wrapper.
3. Read audio output/autotune settings, register the local SDK callbacks, and
   publish those settings (vtable +400, mapped to cmd510).
4. Mark the local SDK initialized flag.

The API's own process work-buffer pointer/size and the event wrapper are local
bookkeeping, not additional metadata publication. No NACP/ARP reader or writer
is visible in this initializer. Firmware could still read metadata internally.

The original probe preparation calls cmd2 then cmd510, while its separate
diagnostic cycle queried cmd7 and finalized afterwards. A single attempt that
also obtains cmd7 **within the same initialization lifetime**, retains only the
owned duplicate, and closes it during cleanup would close this small ordering
gap. The getter is likely incidental, but that is an inference; retaining the
exact normal SDK order is a better finite check than inventing opaque metadata.

The inspected `OpenSessionWithMatchmakeSupport` reconstruction then performs the
ordinary native free-communication check when configured, queries type/mode,
registers the real global/per-guest transfer memories, and asks the service for
the applet argument. No additional NACP registration is apparent in that code.
Guest display creation is a distinct API and can happen before a session;
its availability does not establish matchmaking acceptance.

## Search boundary

Inspected current libnx/libounce NACP declarations, nxdumptool's public header,
Switchbrew NACP and Splay research, fefw's current full recursive source tree
and relevant initializer/matchmaking files, Atmosphere's generated report
schema, and nxnative's documented guest-display behavior. Direct code searches
for GameShare plus ApplicationControlProperty returned no hits; StreamPlay plus
ApplicationControlProperty returned parental-control wrappers and IPC metadata.
The only exact StreamPlaySaveDataKeyType hits were generated Atmosphere report
schemas and their copies. No field mutation is recommended from these results.

## Primary references

- [NACP layout and new firmware-23 names](https://switchbrew.org/w/index.php?title=NACP&oldid=15076)
- [libnx NACP header](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/nacp.h)
- [nxdumptool control-property declaration](https://github.com/DarkMatterCore/nxdumptool/blob/rewrite/include/core/nacp.h)
- [Atmosphere report-field schema, pinned revision](https://github.com/Atmosphere-NX/Atmosphere/blob/3e8915011e1de89f595462d929c1cc3294470896/libraries/libstratosphere/include/stratosphere/erpt/erpt_ids.autogen.hpp)
- [fefw control-property type, pinned revision](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/include/nn/ns/ns_application_control_property.hpp)
- [fefw initializer ordering](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/src/nn/streamplay/unplaced_2.c)
- [fefw matchmaking code](https://github.com/vexcited/fefw/blob/b6ca8d2c2cc3fbdb74ba6ed7f72b4f9b318aee96/src/nn/streamplay/streamplay_api_impl.c)
- [GameShare guest applet protocol observations](https://switchbrew.org/wiki/Splay_Applet)
- [nxnative guest-display observations](https://github.com/seedds/nxnative/blob/74181da849b9d78bb2070c059339d2b300fd6890/docs/reference/services.md#gameshare-guest-displays)
