# GameShare Probe for kaiHEN / Switch 2

**Continuing from another PC:** start with [START_HERE.md](START_HERE.md).
It supersedes the historical setup below and links the portable evidence,
pairing, current installed-runtime profile and standalone build instructions.
Latest outcome: initialization and memory/display setup work; matchmaking
returns 0x2fab1f. No host session or image stream exists yet.

A standalone native diagnostic for investigating this path:

```text
PC game -> Moonlight on Switch 2 -> local GameShare -> guest Switch 1 or 2
              ^                                      |
              +---------- Player 2 input -------------+
```

Version **0.3** is a [persistent native research agent](AGENT.md): launch once,
then compile, execute and inspect new native probes remotely without replacing
the NRO. The app does not automatically start a GameShare session.
It uses the existing libounce SDK, independently of CivHomebrew.

## Current workflow: persistent agent

```powershell
python deploy.py --host 192.168.1.182
# Launch GameShare Probe through kaiHEN HBMenu and leave it open.
python agent.py --host 192.168.1.182 status
python agent.py --host 192.168.1.182 run examples/hello.c
```

The deployer configures the pairing key for the PC client automatically. Commands
can retain service objects and state between runs. Results include native return
values, SDK errors, logs and binary output. See [the protocol and C ABI](AGENT.md).
The automatic one-shot baseline from versions 0.1/0.2 is disabled in 0.3.

## Environment report / earlier baseline versions

1. Copy `dist/gameshare-probe.nro` to
   `sd:/switch/gameshare-probe/gameshare-probe.nro`.
2. Launch it from native kaiHEN HBMenu. A native Switch 2 title override is the
   intended environment. A launch under a different host can have different
   service permissions and loaded modules.
3. Leave the diagnostic open. Note the IP and 16-character pairing key.
4. On the PC, from this directory:

   ```powershell
   python client.py 192.168.1.182 --watch 30
   ```

   Enter the pairing key when prompted. For an agent/script, `--key KEY` is also
   accepted. Captures go to `evidence/` with UTC timestamps. Pairing keys are not
   written to captures and change each launch.

The NRO also writes `sd:/switch/gameshare-probe/report.json`, updating it as
service discovery progresses. This file is useful if networking fails. `+`
returns to HBMenu after the worker has finished. If a request remains pending,
the UI and report server remain available; close the title normally through
HOME to stop it. The NRO does not unload its code with a worker still running.

The PC clients are Python 3.10+ standard library only. The endpoint is HTTP on TCP
39381. `GET /health` identifies the probe; `GET /report` requires an
`X-Probe-Key` header. Version 0.3 also exposes authenticated native execution and
job results, described in [AGENT.md](AGENT.md).

## What the earlier baseline report establishes

- Actual firmware result and value, loader text, and the SDK's HOS/kaiHEN hints.
- One open/close attempt for `stpl:u`, `lrp:u`, `stpl:sys`, `chat:u`, `chat:sp`,
  `ldn:u`, `ldn:s`, `lp2p:app`, and `spl:lrp`, through a private SM session.
- On 0.2, one `stpl:u` CMIF command 0 with no input, requesting one scoped
  interface. That object is closed without invoking its methods. The result is
  separate from successful SM handle acquisition. No alternate input shapes or
  command IDs are tried if it fails.
- Raw result codes with module and description, or an explicit pending/untried
  state. Service probes run once, sequentially; a pending request leaves later
  entries untried. There is no automatic replay or guessed interpretation of
  an SM error as global service absence.
- Own-address-space executable mappings with recognized MOD0 headers, dynamic
  table coverage, and matching dynamic symbol names. Undefined imports and
  defined executable candidates are distinct. No discovered function is called.
- Explicit limits for unrecognized modules, GNU-hash-only/stripped symbol
  tables, long names, table/output caps and incomplete mapping enumeration.

The symbol search terms include `streamplay`, `shareplay`, `lrp`, individual
displays and Npad input. It does not read another process, enumerate its
exports, dump firmware, acquire elevated access, or alter a title policy.
An NRO can lack Nintendo SDK exports even while the system's GameShare services
are available. This stage cannot prove the absence of those APIs system-wide.

Opening a service establishes that this process obtained a handle at that
moment. It does not establish the creation ABI, stream setup, signing privileges,
guest display rendering, input delivery, audio routing, or Wi-Fi coexistence.
The only target-service command in version 0.2 is the documented `stpl:u`
scoped-interface creator. SM client registration and handle acquisition use the
existing SDK's CMIF layout. No commands are sent to a returned scoped object in
that baseline. Version 0.3 reports remote command effects separately.

## Build and validate

```powershell
python build.py
python -m unittest discover -s tests -p test_client.py -v
```

`build.py --sdk PATH` selects an existing devkitPro installation with libounce;
the default is the adjacent `Dev/devkitPro`. It reads the SDK's CPU flags,
builds with warnings treated as errors, and packages the NRO with NACP metadata.
`dist/build.json` records hashes of source, SDK archive, ELF and NRO, the compiler,
and exact commands. `device_executed: false` is deliberate until a separate
console capture establishes execution.

The C parser tests can be built with a native host C compiler:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror source/module_scan.c tests/test_module_scan.c -o build/test_module_scan
./build/test_module_scan
```

They exercise defined/undefined names, missing hash metadata, unreadable tables,
invalid offsets, size limits and address overflow. Client tests use a local HTTP
server to exercise pairing, a real HTTP round trip, schema errors and host
validation. Host tests do not qualify the console's service ABI or renderer.

## Findings and next steps

Public research identifies two useful native service families:

- [`stpl:u` / `nn::streamplay`](https://switchbrew.org/wiki/Switch_2%3A_Chat_services).
- [`lrp:u` / `IUserShareplayHostService`](https://switchbrew.org/wiki/Switch_2%3A_LRP_services).

The command lists are incomplete. In particular, the LRP creator has a PID and
16 bytes of input whose full meaning is not documented. This probe deliberately
does not invent values or send creator commands. The receiver's
[Splay implementation](https://switchbrew.org/wiki/Splay_Applet) uses LDN protocol
3 and verifies signed advertisements. That makes reusing native hosting a better
first research direction than writing a transmitter from scratch, but does not
prove that a homebrew caller can use it.

After capturing this NRO:

1. Determine which host services are accessible under the actual kaiHEN title
   policy, keeping SM setup/access failures separate from service ABI failures.
2. Recover exact creator/startup/display/input layouts from a matching host-side
   SDK module or an observed supported game's calls. A successful open alone
   does not justify calling arbitrary command IDs.
3. Build a minimal host rendering a test image and collecting one guest pad.
4. Test PC LAN connectivity while local GameShare runs, starting with Ethernet
   upstream. Wi-Fi coexistence remains unverified.
5. Integrate Moonlight video/audio and forward the guest as its own controller
   slot. Measure guest latency with a local fighting game before judging Tekken
   playability.

The local Moonlight implementation already has per-controller GameStream input
forwarding (`LiSendMultiControllerEvent`), but no GameShare host integration was
found in the inspected app source or the installed libounce headers. That is a
bounded source search, not proof about all available homebrew projects.

The initial 0.1 build now has console execution evidence; see
[device findings](FINDINGS.md). The 0.2 creator test requires a separate run.
The REA Rizin
provider was unavailable on this host; inspection of the compiled SDK's SM
wrapper used the already installed AArch64 objdump instead. The static wrapper
inspection is not evidence of a successful runtime service open.
