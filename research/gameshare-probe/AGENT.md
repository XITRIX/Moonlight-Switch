# Persistent native research agent

Probe **0.3.1** keeps an authenticated HTTP listener on TCP **39381**. Launch it
once from kaiHEN HBMenu and leave it open. Subsequent probes are freestanding
AArch64 C snippets, compiled on the PC and published through the SDK Jit API.
No NRO replacement is needed for each command.

0.3.1 adds recent native log lines on-screen and **X** to reset this NRO's BSD
connection/listener while idle. A snippet may resolve `probeReconnectNetwork`
as `void (*)(void)` to queue the same reset after its worker has fully exited.
Use it only after finalizing local-wireless operations; it does not terminate
StreamPlay or force infrastructure Wi-Fi while hosting. It changes no HEN/RO.
Session IDs now mix the system tick because 0.3.0's RNG output repeated across
two NRO launches. Never treat the old 0.3.0 session ID alone as a restart guard.
The exact older ELF/NRO were archived in `evidence/probe-builds/0.3.0/` before
rebuilding; old offset-qualified snippets must use their matching binary.
The resolver also exposes aligned_alloc/free, transfer/shared-memory SVCs and
the libounce applet holder/storage/common-arguments functions needed to follow
the reconstructed SDK's matchmaking path. They require their exact SDK header
signatures and remain unqualified on hardware until called and observed.

## Connect and execute

`deploy.py` installs the NRO and creates or retains a random pairing key in
`sd:/switch/gameshare-probe/agent-key.txt`. It saves a matching private local copy
at `evidence/agent-key.txt`; neither the key nor request headers are logged. That
folder is ignored by Git and excluded from distribution ZIPs. A missing/invalid
SD key falls back to a random on-screen key; `agent.py --key KEY` accepts it.

```powershell
python agent.py --host 192.168.1.182 status
python agent.py --host 192.168.1.182 run examples/hello.c
python agent.py --host 192.168.1.182 run examples/hello.c
python agent.py --host 192.168.1.182 run examples/open_streamplay.c
python agent.py --host 192.168.1.182 run examples/create_streamplay.c
python agent.py --host 192.168.1.182 run examples/close_streamplay.c
```

The hello command returns **42**, outputs byte `2a`, and increments a counter in
the app's persistent state. Two successful remote runs establish execution and
state retention for this actual launch. A successful host compile alone does
not establish console JIT support.

This was verified on the console. StreamPlay's factory was also verified on
firmware 23.0.1 using CMIF command 0 with u32 `0x30000`; the older no-input form
closed the session. `create_streamplay.c` now uses that versioned input. The
factory success is from the raw transport example; the updated managed example
has been compiled but has not yet been executed. Neither creates a host session.
See `FINDINGS.md` for evidence, retained state and the remaining ABI gap.

`run` compiles with the adjacent devkitPro/libounce SDK and needs no third-party
Python modules. Use `--sdk PATH` for another matching SDK. Offline compilation
is available with `compile SOURCE --output FOLDER`. `run-bin FILE --entry 0xN`
accepts an already compiled flat image. `run`/`run-bin --input FILE` provides up
to 64 KiB of binary input. `--timeout SECONDS` limits client polling; it does not
cancel native execution.

## Snippet ABI

Include `research_api.h` and define:

```c
uint64_t probe_entry(const ResearchApi *api, ResearchContext *context);
```

The return value, SDK publication/cleanup results, logs and binary output are
separate fields. Set `context->output_size` after writing at most
`context->output_capacity` bytes. `context->state` provides 64 KiB, zeroed on app
launch and retained between commands. Snippets use stack storage and that state;
the linker rejects writable globals, BSS, GOT and unresolved external calls.
Compiler-generated memory helpers are included. Internal code/constant references
are position independent and the RX allocation is page aligned.

Callbacks:

| Callback | Purpose |
| --- | --- |
| `log`, `log_value` | Record bounded text and named hexadecimal values |
| `service_open` | Acquire a service under the NRO's existing SM identity |
| `service_close` | Close one retained agent resource ID |
| `service_domain` | Convert that resource to a domain using the SDK wrapper |
| `ipc` | Dispatch explicit CMIF input/output, buffers, PID, objects and handles |
| `handle_close` | Close an owned raw handle returned by `ipc` |
| `query_memory`, `read_memory` | Query/read mapped memory in this process |
| `sleep_ms`, `time_ms` | Bounded-by-caller waits and monotonic timing |
| `resolve` | Resolve a named, linked SDK function; returns zero when unavailable |

`ResearchIpc` supports four descriptors, four input objects/handles and four
output objects/handles. Inline input/output is capped at 128 bytes, with a
conservative combined 0x100-byte TLS request bound; individual descriptors are
capped at 64 KiB. Buffer attribute bitmasks use the matching SDK's
`SfBufferAttr_*` definitions. Target-service protocol layouts and descriptor
types must come from evidence; this agent does not infer them.

Returned service objects are agent IDs, retained across snippets in a table of
32 entries. Domain parents cannot be closed while a retained child references
their session. Up to 64 raw output handles are retained and closed through
`handle_close` or ordinary app shutdown. Do not close managed handles through a
raw SVC and leave their registry entries stale. App exit closes child objects
before parents. The StreamPlay examples use state offsets 8 and 12 for root and
scoped object IDs; the hello counter occupies offsets 0 through 7.

The resolver currently exposes `svcQueryMemory`, `svcSendSyncRequest`,
`svcCloseHandle`, `svcWaitSynchronization`, `svcGetProcessId`, `svcGetInfo`,
`ldnInitialize`, `ldnExit`, `ldnSetProtocol`, `ldnGetState`, `ldnGetNetworkInfo`,
`appletGetAppletResourceUserId`, `hidGetNpadStatesFullKey`, and
`hidGetNpadStatesHandheld`. Check for zero before calling, and use the matching
SDK signature. A resolved address does not establish that a particular call is
permitted or that a proposed ABI is correct.

## Execution and lifetime

One command runs at a time on its own worker. The main thread continues serving
status and rendering the UI while a command blocks. Publication and execution
are serialized; code is made writable, copied, published through kaiHEN's RO
provider, and the executing thread invalidates its instruction cache before
entry. The RX allocation is closed after return. Cleanup failure poisons the
agent until restart instead of silently admitting another publication.

Native snippets are trusted code with the app's permissions, not a sandbox.
A bad native pointer/instruction can terminate the title; there is no claimed
recoverable exception handler or safe forced cancellation. A stuck command is
reported as running, remains queryable, and requires normal title closure if it
does not return. `+` queues exit until a worker finishes. The NRO never returns
to HBMenu while a worker is still executing its code.

## Evidence and protocol

- `GET /health`: public app/version metadata.
- `GET /report`: authenticated environment/module report. In agent mode the
  old automatic service baseline is disabled; remote effects belong to `/agent`.
- `GET /agent`: authenticated latest job, session ID, publication stage, results,
  return value, logs, output hex and idle-only resource snapshot.
- `POST /run`: authenticated `application/octet-stream`, explicit Content-Length,
  no transfer encoding; returns 202 for an accepted asynchronous job or 409 if
  busy/poisoned. No command is automatically replayed.

The run body is `KJIT` followed by five little-endian u32 values: ABI=1, entry
offset, code size, input size, reserved=0. Code follows the 24-byte header, then
input. Code is at most 256 KiB and both its size and entry must be four-byte
aligned. Entry must lie within the image. The HTTP receiver reads complete
bounded bodies before dispatch. The latest result remains available until the
next command; the session ID distinguishes app restarts.

Before sending, the PC saves the exact source, compiled image, packet, compiler
commands and hashes under `evidence/jobs/<UTC timestamp>/`. It records replies
as JSONL plus final result/output files. Connection loss after submission is
reported as uncertain and is never handled by resending. Query `status` first.
`sd:/switch/gameshare-probe/agent-last.json` independently journals job stage/log
changes while the app is running. Keys are omitted from both journals.

Host validation covers packet bounds/framing, actual AArch64 snippet compilation,
the NRO build/parser and local HTTP client tests. Actual console qualification
requires successful remote command responses.
