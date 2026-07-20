# PS Vita development loop

Moonlight uses Borealis' GLES2/SDL2 Vita backend. The local helper builds a
debuggable VPK, deploys `Moonlight.self` as the installed app's `eboot.bin`,
launches title `MNTL00000`, captures PrincessLog output, and requires the app to
complete at least one Borealis frame before it reports success.

## One-time Vita setup

1. Enable **Unsafe Homebrew** in HENkaku settings. The required PVR runtime
   modules are already packaged from `app/platforms/psv/module`.
2. Install [vitacompanion](https://github.com/devnoname120/vitacompanion), put
   `vitacompanion.suprx` in `ur0:/tai`, add it under `*main` in
   `ur0:/tai/config.txt`, and reboot. It must expose FTP on port 1337 and its
   command server on port 1338.
3. Install and configure PrincessLog. Point network logging at this Mac's LAN
   address and TCP port 9999. `scripts/psv-dev.sh doctor` prints the address it
   expects. Reboot after changing the logger configuration.
4. Upload the first package and install it once from VitaShell:

   ```sh
   scripts/psv-dev.sh install
   ```

   This places the file at `ux0:/data/Moonlight.vpk`. Open it in VitaShell and
   confirm the install. Later code iterations replace only `eboot.bin` and do
   not reinstall the VPK.

Keep the Vita awake and connected to the same network as the development Mac.
VitaShell's temporary FTP server is not a substitute for vitacompanion because
the loop also needs the persistent command server.

## Build, deploy, launch, verify

Run one complete iteration with:

```sh
scripts/psv-dev.sh cycle
```

Success requires all of the following:

- the `Moonlight.vpk` target builds;
- the installed title directory is reachable by FTP;
- vitacompanion accepts the launch command;
- PrincessLog contains `VITA_HEALTH: READY`, emitted after the first complete
  Borealis frame;
- no error/fatal markers or new Vita crash dumps appear during the stability
  window.

Logs are retained in `.psv-logs/`. When C++ or CMake changes fix a failure, run
the same command again. When XML, images, translations, or PVR modules change,
also sync those files:

```sh
PSV_DEPLOY_RESOURCES=1 scripts/psv-dev.sh cycle
```

If the Vita produced a crash dump, symbolize the newest one against the
unstripped ELF with:

```sh
scripts/psv-dev.sh parse-crash
```

The crash parser uses the `xfangfang/vita_parse` Docker image described by the
Borealis Vita guide.

## Configuration

The defaults target the current development device. Override them with
environment variables when needed:

| Variable | Default | Purpose |
| --- | --- | --- |
| `PSV_IP` | `192.168.1.209` | Vita address |
| `PSV_TITLE_ID` | `MNTL00000` | Installed application title ID |
| `PSV_FTP_PORT` | `1337` | vitacompanion FTP port |
| `PSV_COMMAND_PORT` | `1338` | vitacompanion command port |
| `PSV_LOG_PORT` | `9999` | PrincessLog listener port |
| `PSV_HEALTH_TIMEOUT` | `45` | Seconds allowed to receive the ready marker |
| `PSV_STABILITY_SECONDS` | `8` | Error-free observation time after ready |
| `PSV_DEPLOY_RESOURCES` | `0` | Upload resources/modules when set to `1` |

Use `scripts/psv-dev.sh doctor` for a quick prerequisite and connectivity
check, or `scripts/psv-dev.sh logs` to capture logs without rebuilding.
