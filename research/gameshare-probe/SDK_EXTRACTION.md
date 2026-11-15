# Cartridge SDK extraction through the existing probe

## Outcome and current constraint

The PC adapter and native snippet are implemented. Real console execution listed
the inserted **Kirby Air Riders** cartridge, program `040077201be50000`, version
0. **No SDK has been extracted.** The loader capability needed to open its CodeFS
is unavailable through the current homebrew interface.

The user explicitly prohibited writing or replacing console HEN/RO, while
authorizing a read-only HEN copy for PC analysis. Preserve this constraint.
No HEN/RO file or runtime code was patched. The isolated HEN extension built on
the PC before that instruction is an inactive research artifact; do not install
it or ask for the same permission again unless the user changes the constraint.

## Implemented adapter

- `sdk_extract.py`: authenticated job orchestration over the existing NRO,
  session/job identity checks, sequential 64 KiB reads, no automatic POST retry,
  metadata journals, and no-clobber output publication after validation.
- `examples/sdk_extract.c`: native metadata listing, CodeFS/file opening, bounded
  reads and explicit child-before-parent cleanup. Uses the existing probe ABI;
  no new NRO was installed. Extractable filenames are `sdk`, or explicitly `main`.
- `nso_verify.py`: bounded raw-LZ4 decompression, NSO segment-bound checks and
  SHA-256 verification where the NSO header supplies segment hashes.
- `examples/cartridge_code_path.c`: alternate read-only mount through the
  ordinary FS proxy, using NCM's actual Program content path and native attribute 1.

The adaptation reuses OunceCake's documented approach: clone a held loader root,
open CodeFS with a 16-byte attributes/storage/program request and a 0x1a4-byte
CodeInfo receive buffer, then read its files. It does not port OunceCake's FS
memory/context manipulation or launch another exploit. Source reference:
`evidence/sources/ouncecake/engine/program.cjs`, revision
`ef81b7f3ef4bb8db6d429b6dddd7a476e6b0ce02`.

## Hardware results

The restarted probe session is `d41edc6a2858099d`.

1. Documented RO/HEN setup succeeded again. HEN PID 210, protocol 6, startup
   result zero. Evidence: `evidence/jobs/20261115T214315.431625Z`.
2. A new `fsp-ldr` request still returned `0x615` (SM OutOfSessions).
   Evidence: `evidence/jobs/20261115T214316.084188Z`.
3. The new adapter's cartridge inventory returned one Air Riders Application
   key, version 0. Evidence:
   `evidence/sdk-extract/20261115T215125.707492Z/inventory.json`.
4. The namespaced bridge capability request returned `0x1759` (Libnx BadInput),
   confirming that extension is not served by the running HEN. Evidence:
   `evidence/sdk-extract/20261115T215126.555806Z/jobs.jsonl`.
5. NCM resolved the actual cartridge Program content ID and GameCard path.
   FS proxy command 10 with native attribute 1 and Code type 0 returned
   `0xc56ee202`; no CodeFS handle was returned. Evidence:
   `evidence/jobs/20261115T215641.029844Z`.

The installed HEN was copied read-only after explicit user authorization and
validated as NSO0 with all three segment hashes checked. It is 78,566 bytes,
SHA-256 `824265c9bb1eb35a47d2542fb60d3030696870407649ea554a43617f134a91e2`,
identical to the local testship HEN. Copy and verification metadata:
`evidence/installed-hen-backup/20261115T215557.962902Z/`.
The first export attempt was rejected by automatic approval review as outside
the cartridge-SDK scope; it was not retried until the user expressly allowed it.

Static Capstone disassembly of that exact copied binary's homebrew dispatcher
(text offsets 0x6820 onward) confirms handlers for commands 16–22 and an unknown
command return path. Command 18 indexes the calling client's retained process
handle; it is not a general HEN/FS-process handle export. Command 19 calls an SM
proxy session creator. The older bundled SDK's comment calling this a direct
master-SM copy is stale for this binary. Existing code still avoids Initialize
and protocol Close on that handle, which is safe for both forms.
The public HEN source was used as a comparison, not as proof of byte identity.
Saved disassembly is `homebrew-dispatch.asm` alongside the copy. This is static
analysis with local tools, not a claim to have read live HEN memory.

## Validation

- Native extraction payload compiles with warnings as errors.
- Four Python tests passed: packet/inventory bounds, LZ4 overlap/truncation,
  NSO corruption detection, and stopping before POST after a probe restart.
- The NSO verifier also validated the real local and copied installed HEN NSOs.
- The inactive backend build passed its native build, a request-policy test,
  and upstream mount/SM host tests. These are offline checks; it was never
  installed or hardware-qualified. Host tests did not use sanitizers on Windows.
- End-to-end SDK file transfer and any GameShare host behavior remain unverified.

## Commands

Run from this directory with the existing paired probe open:

```powershell
python sdk_extract.py inventory
python sdk_extract.py status
```

`status` currently fails with the explanatory missing-bridge result. Extraction
is implemented but requires a functioning CodeFS provider first:

```powershell
python sdk_extract.py extract --program-id 040077201be50000 --file sdk
```

Do not imply that this command currently succeeds. A newly available compatible
read-only provider, or a separate existing OunceCake/ns2jb loader session, is
needed to obtain the SDK without changing the current HEN/RO. No such provider
or held loader session has been identified in the running probe's accessible
interfaces. No Nintendo-server connection is needed by the implemented reads.
