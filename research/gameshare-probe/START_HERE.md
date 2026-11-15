# Continue the GameShare investigation on another PC

## Current result

The native Switch 2 probe initializes StreamPlay, registers real work memory,
creates a player1 guest display/layer, obtains guest-input shared memory and
passes the normal native communication check. Matchmaking preparation commands
504 and 511 still return **0x2fab1f** (module 287, description 6101). Its exact
meaning is unknown. There is no working host advertisement, guest connection
or image stream. Moonlight itself has not been modified to host GameShare.

The intended pipeline is PC game -> Moonlight on Switch 2 -> local GameShare
guest on Switch 1/2, with guest input sent back as PC player 2.

**Keep HEN and RO unchanged.** Use their existing interfaces only. Keep the
console disconnected from Nintendo servers. The experimental HEN backend in
research-backend/ was built only on PC, never deployed, and is not an authorized
route. Do not run a fresh jailbreak pipeline, update HEN/RO, alter saves, or
install console SDK/game/firmware files as part of continuing these tests.

## Clone and prerequisites

```sh
git clone --single-branch --branch codex/gameshare-research https://github.com/XITRIX/Moonlight-Switch.git
cd Moonlight-Switch/research/gameshare-probe
```

Python 3.10+ is sufficient for pairing, status, recorded-result inspection and
the installed-runtime qualification inputs. No pip packages are needed.
Compiling snippets/NROs also needs devkitA64 and the public **ounce-devkit
20261115b** (libounce d1718bb7076f). See [the existing Switch 2 toolchain
instructions](../../docs/SWITCH2.md) and its pinned CI archive/hash. This is the
open-source homebrew SDK, not Nintendo's proprietary SDK. Moonlight's submodules
and application dependencies are unnecessary for this standalone probe.

Set DEVKITPRO to that installation or pass --sdk to the build/client commands:

```powershell
$env:DEVKITPRO = 'C:\devkitPro'
```

On Linux/macOS, use export DEVKITPRO=/opt/devkitpro (or your installation).
The scripts select .exe tools only on Windows and honor DEVKITPRO. The optional
offline historical backend builder also accepts --sdk; it does not deploy.

## Continue with the already-installed probe

The hardware-qualified NRO is **0.3.1**, SHA-256
79ff7da764e2d3de8416912b98fd6644141029f034639d73b305bdf43c2f0392.
It last ran on firmware 23.0.1. The last known IP is 192.168.1.182, port 39381;
use the IP actually shown on the console. It was left idle with no retained
test services or work allocations.

```powershell
python pair.py --host 192.168.1.182
# Enter the probe's on-screen pairing key in the hidden prompt.
python qualify.py --installed-031
python agent.py --host 192.168.1.182 status
python agent.py --host 192.168.1.182 run examples/hello.c
```

Pairing verifies a read-only status response before saving the key locally in
ignored evidence/agent-key.txt. No key is in this branch. You can alternatively
copy your existing key file privately between PCs. Do not commit it.

qualify.py --installed-031 uses the recorded profile; it needs no original ELF
and makes no console request. The checked-in examples/own_vi2_symbols.h matches
that exact installed NRO. The runtime checks its function-byte guards before
calling the probe's existing VI2 functions. Rebuilding creates a different NRO;
do not use a newly generated profile against the older installed binary.

## Reproduce the current native preparation

Read [HOST_BRINGUP.md](HOST_BRINGUP.md) first. Submit one command at a time and
inspect native return_value as well as the framework result: agent.py can exit
zero even when the native experiment returns a nonzero service result.

```powershell
python agent.py run examples/open_streamplay.c
python agent.py run examples/create_streamplay.c
python agent.py run examples/streamplay_memory.c
python agent.py run examples/streamplay_initialize_cycle.c
python -c "from pathlib import Path; Path('build/matchmake-511-display.bin').write_bytes(bytes([0,0,1,1]))"
python agent.py run examples/streamplay_prepare.c --input build/matchmake-511-display.bin
python agent.py run examples/streamplay_cleanup.c
```

These commands default to 192.168.1.182; place --host IP before run if different.
Preparation allocates the actual 43.8 MiB work memory, follows the native pctl
communication check, creates only its own guest display, requests matchmaking,
then finalizes and releases resources. The 64-byte matching configuration is
still a reconstruction; its semantic fields are not fully known. Do not treat
the command's failure as a decoded Nintendo error name or title restriction.

Initialization can interrupt the PC listener. The cycle/preparation snippets
queue a local network reset after finalization. Probe 0.3.1 also shows recent
logs and provides X to reconnect while idle. Never automatically resend POST
after a connection error: execution may have happened. Check status/the physical
screen first. Do not close an NRO with a worker still executing native code.

## Build a replacement NRO without the old workspace

```powershell
python build.py
python qualify.py --header examples/own_vi2_symbols.h
python -m unittest discover -s tests -p 'test_*.py'
```

build.py compiles the bundled ISC kaiHEN JIT client hooks; no sibling kaiHEN
checkout or prebuilt ouncehen-jit.o is required. Outputs stay in ignored build/
and dist/. qualify.py checks ELF/NRO code agreement, writes guarded binary
inputs and regenerates the VI2 header for the new binary. **Install that exact
NRO before using its new profile.** Keep the historical installed profile for
the old runtime. Generated builds start as device_executed=false.

To install just the new probe, open FTP homebrew and use:

```powershell
python deploy.py --host 192.168.1.182 --port 5000
```

The installer changes only /switch/gameshare-probe/, stages and verifies the
NRO, retains the key and backs up the previous probe. It does not install HEN
or RO. Native behavior of the relocated source-built JIT hook must be checked
again on hardware; the migration only qualified its build on PC.

The generated ZIP contains a VI2 header and records/packaged-probe-layout.json
matching its included NRO. When using that packaged runtime, regenerate its
binary inputs with python qualify.py --profile records/packaged-probe-layout.json
instead of --installed-031. Never mix profiles from different probe builds.

## Evidence and next step

- [HOST_BRINGUP.md](HOST_BRINGUP.md): latest hardware outcomes, hypotheses,
  cleanup, and exact timestamps; read this before older reports.
- [FINDINGS.md](FINDINGS.md), [PUBLIC_SDK_RESEARCH.md](PUBLIC_SDK_RESEARCH.md),
  [ONLINE_RESEARCH.md](ONLINE_RESEARCH.md): accumulated native/public research.
- [SDK_EXTRACTION.md](SDK_EXTRACTION.md): unsuccessful cartridge SDK access;
  the current HEN lacks the archived CodeFS bridge. Extraction is not working.
- [AGENT.md](AGENT.md): transport, C ABI, state ownership and runtime limits.
- [records/README.md](records/README.md): portable evidence index. Historical
  evidence/... paths in the notes refer to the original private workspace;
  corresponding text results are indexed here by the same capture timestamp.

The next useful evidence is a real host's MatchmakeConfig/call trace, a suitable
SDK module, or the Chat service's validation of 0x2fab1f. Public reconstructions
leave that contract incomplete. Do not fabricate applet arguments or ask the
guest to connect until an actual host advertisement is established.
