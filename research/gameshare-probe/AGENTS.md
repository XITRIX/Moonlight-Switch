# GameShare research handoff

Read START_HERE.md and the latest section of HOST_BRINGUP.md before runtime work.
This is the standalone kaiHEN native probe, not a Moonlight hosting feature yet.
Initialization/memory/display/communication checks succeed; matchmaking returns
0x2fab1f with unknown precise meaning. No host or image stream is established.

Session constraints:

- Do not modify HEN or RO files/runtime code. JIT code and buffers belong to
  the probe's own process. Existing HEN/RO client interfaces may be used.
- Keep the Switch offline from Nintendo servers. Do not update/launch a fresh
  jailbreak pipeline, overwrite boot files or modify saves to obtain an SDK.
- research-backend/kaihen is an inactive, never-deployed historical experiment;
  its presence and PC builder are not authorization to install a HEN extension.
- Respect the actual native communication/parental checks. The pctl service
  fallback succeeds normally; no setting change or check bypass is required.
- Keep pairing keys, raw console outputs, NACP, game/firmware/SDK dumps and
  binaries out of Git. Local output goes in ignored evidence/, build/ or dist/.
  records/ contains deliberately allowlisted text evidence and our own profiles.
- Treat framework result and native return_value separately. A compiled snippet
  or successful publication is not evidence that its target command succeeded.
- Use the profile matching the exact deployed NRO. The checked-in VI2 header
  matches the original installed 0.3.1 hash; new builds require requalification.
  Old 0.3.0 session IDs repeated across launches and are not sufficient guards.
- Never replay a POST after a connection error. Inspect status/the physical
  screen first. A worker may have executed even when the PC lost the reply.
- Close only owned service/handle resources. Do not close borrowed RO/master
  handles or free transfer-memory allocations while services retain them.
- Preserve the distinction between measured console results, physical-screen
  confirmation, source-derived hypotheses and host-only validation. Update the
  current findings when new evidence supersedes historical sections.

No CivHomebrew environment is needed for the native probe. archive/civ7 holds
historical finite JS inspections with old runtime addresses, not live probes.
