# Read-only Chat code access investigation, 2026-11-16

This note records source and local-artifact analysis only. The author of this
note did not contact the console, attach a debugger, invoke a console SVC, or
change HEN/RO. Runtime observations reported elsewhere remain separate.

## Safe own-process syscall availability inspection

The original local `build/gameshare-probe.elf` has SHA-256
`3405fe15e29911053bf90ca805c7be3c2e86ef1775e52eb6e750388e3c4ac8f1`, matching
the ELF hash recorded for the installed 0.3.1 NRO. Cross-nm/objdump found:

| Symbol | Own NRO-relative offset | Qualification |
| --- | ---: | --- |
| openService | 0x1d80 | Existing checked-in profile anchor |
| envIsSyscallHinted | 0x22280 | First 16 bytes `1ffc027108010054e10b00f0027c0653` |
| g_syscallHints | 0x1a18d0 | Three u64 words in this NRO's BSS |

The envIsSyscallHinted guard was also read directly from the original local
NRO, SHA-256 `79ff7da764e2d3de8416912b98fd6644141029f034639d73b305bdf43c2f0392`,
and compared equal to the ELF executable segment at the same relative address.

`envIsSyscallHinted` performs no SVC. Its disassembly bounds the argument to
0..0xbf and tests the corresponding bit of those three words. After checking
the installed-runtime anchor and function bytes, reading those 24 bytes, or
calling this exact `bool(unsigned)` function, is an own-process read-only
operation. Addresses from a rebuilt NRO must not be substituted.

These are loader-provided hints, not independently measured kernel permission.
[libnx env.c](https://github.com/switchbrew/libnx/blob/master/nx/source/runtime/env.c)
loads them from homebrew ABI entries 6 and 17; its NSO path defaults to all ones.
A zero hint is a reason to avoid a privileged SVC. A positive hint is not enough
to establish that attaching to an unrelated system process is permitted.

The bundled libounce `svc.h` documents `InfoType_IsSvcPermitted` (26) as usable
only for syscall 0x36 at present. It is not a general query for debug SVCs.
`InfoType_DebuggerAttached` (8) only describes an already-attached debugger.

## Read-only process/module metadata IPC

The primary [libnx pm.c](https://github.com/switchbrew/libnx/blob/master/nx/source/services/pm.c)
uses these modern pm:dmnt operations:

- Command 2: input u64 ProgramId, output u64 PID.
- Command 7: input u64 PID, output u64 ProgramId.

The primary [libnx ldr.c](https://github.com/switchbrew/libnx/blob/master/nx/source/services/ldr.c)
uses ldr:dmnt command 2 for module metadata: input u64 PID, output s32 count,
and an output HipcPointer buffer. Each `LoaderModuleInfo` is 0x30 bytes:
32-byte build ID, u64 base address, u64 size.

These calls provide metadata, not executable bytes or a process handle. Their
successful use through HEN's existing FS-identity SM proxy would not grant the
calling NRO additional kernel capabilities. The proxy source forwards service
lookup through its already-registered master SM identity; it does not change
the homebrew process's SVC policy.

## Why a debug attach is not a passive alternative

The installed SDK marks svcDebugActiveProcess (0x60),
svcQueryDebugProcessMemory (0x69), and svcReadDebugProcessMemory (0x6a) privileged.
A disallowed SVC must not be used as a speculative permission test: it may
produce an exception in the homebrew process instead of a normal denial.

The public [Atmosphere kernel reconstruction](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libmesosphere/source/svc/kern_svc_debug.cpp)
also checks debug-mode/force-debug conditions and target debugging permission
before attaching. This is evidence of a separate permission layer, not proof
that the Switch 2 kernel implements every condition identically. Attachment
creates a debugger relationship and affects target execution, so it is not a
strictly passive memory read. No attach was attempted here.

## Recorded ordinary-FS CodeFS failure

The recorded `0xc56ee202` decomposes as:

```
module = result & 0x1ff = 2
description = (result >> 9) & 0x1fff = 6001
low module/description result = 0x2ee202
remaining high bits = 0xc5400000 (meaning not established)
```

[Atmosphere fs_results.hpp](https://github.com/Atmosphere-NX/Atmosphere/blob/master/libraries/libvapours/include/vapours/results/fs_results.hpp)
places 6001 at the beginning of its FS InvalidArgument range, 6001..6199. Thus
this particular result does not establish missing FS permission. The high bits
have not been explained and must be retained in raw evidence.

The pinned [exact 23.0.1 IPC inventory](https://github.com/borntohonk/Switch-Ghidra-Guides/blob/5732ea48463db991eee2db49a70b9006254c934e/ipc_json_dumps/ns2/23.0.1/programs/Filesystem_Services.json)
confirms:

| Interface / command | Wire contract |
| --- | --- |
| IFileSystemProxy / 7 OpenFileSystemWithPatch | 16 input bytes, returned filesystem object |
| IFileSystemProxy / 10 OpenFileSystemWithId | 16 input bytes, input buffer attribute 25, fixed entry size 0x301, returned filesystem object |
| IFileSystemProxyForLoader / 0 OpenCodeFileSystem | 16 input bytes, output buffer attribute 6, returned filesystem object |
| IFileSystemProxyForLoader / 2 SetCurrentProcess | 8 input bytes and send PID |

The existing command-10 snippet agrees with the public libnx modern layout:
u8 content attributes at byte 0, u32 FS type at byte 4, u64 program ID at byte 8,
plus a 0x301-byte input path. The inventory exposes sizes rather than field
semantics, so it cannot rule out an undiscovered Switch 2 semantic extension.

The public [libnx fs.h](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/fs.h)
and bundled libounce enum begin their supported FileSystemType entries at
2 (Logo), followed by Control/Manual/Meta/Data/Package/RegisteredUpdate. Neither
declares 0 (the attempted Code type) or 1. This supports the hypothesis that
ordinary fsp-srv Code requests are unsupported and dedicated fsp-ldr is needed;
it does not prove the exact service-side branch behind 6001.

No source-backed alternative platform field or content-attribute value was
found that would justify repeatedly varying command-10 input bytes. The
dedicated loader interface remains the documented route. Do not close a
borrowed loader session or change HEN/RO to obtain it under the current scope.

## Bounds and remaining gap

The public exact-firmware inventory directory contains FS/RO/Chat services,
but no PM/Loader KIP command inventories. Modern libnx metadata layouts should
therefore be validated by actual results, not described as independently
verified Switch 2 wire declarations. Public LibHac source paths were unavailable
(404); no conclusions were drawn from those failed requests.

The unresolved dependency is access to executable Chat code or a complete
independent description of its validation. Metadata and syscall hints can
clarify why current interfaces cannot provide it, but do not themselves unlock
that code. No new console binary, HEN extension, or recovery/boot change was
produced by this sub-investigation.
