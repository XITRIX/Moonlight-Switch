# Public research located, 2026-11-15

## Follow-up: reconstructed SDK and exact-firmware IPC

Public code search found vexcited/fefw's reconstructed StreamPlay SDK code,
Dittobyte/Recompia's proxy analysis and borntohonk/Switch-Ghidra-Guides' Switch 2
23.0.1 Chat IPC inventory. No Nintendo SDK binary was obtained. These provide
concrete ABI leads beyond earlier stubs. See PUBLIC_SDK_RESEARCH.md for pinned
sources, candidate layouts, version differences and the Civ7 runtime audit.
Earlier search conclusions below predate this follow-up.

## NS2GameShare: public implementation of the receiver

Repository: https://github.com/mizuyoukanao/NS2GameShare

Inspected revision: `a19809f893ce03b4958fe09fc0b1be2436c82242`.
Selected MIT-licensed source and documentation are saved in
`evidence/sources/ns2gameshare-a19809f893ce/`, with per-file SHA-256 hashes and
pinned URLs in `source-manifest.json`. Only source/docs were retrieved. No
executables were run; no keys, passphrases or console/game files were obtained.

This is a Windows **client**, using an ESP32 Wi-Fi bridge. Its README describes
video/audio/input from a real Switch 2 host. It does not implement a host or the
native `nn::streamplay` IPC startup ABI.

The protocol reference exposes these useful boundaries:

- LDN discovery/authentication and association precede LRP session admission.
- The host sends compressed matching information; the receiver requests a
  session, then the user approves it on the host.
- LRP carries control messages; LSP carries H.264 Annex B and wrapped Opus audio.
- UDP feedback includes Npad button/stick records and clock synchronization.
- LRP's host stream IDs differ from LSP stream indexes.
- The reference explicitly calls itself incomplete. Some fields are inferred.
- Its PC client does **not** verify host signatures. Success against that client
  would not establish acceptance by Nintendo's built-in receiver.

The PC client's prerequisites include the user's own keys and a GameShare
passphrase. They are not supplied. Its extractor targets a particular original
Switch `splay` build; Switch 2 SYSTEM is explicitly unsupported. This does not
establish a requirement to export keys when using the Switch 2's own native
hosting services; that remains the preferred approach for this investigation.

**Implication:** the wire protocol and media/input shape have considerably more
public documentation than previously located. This provides a reference for
validation and a possible independent implementation. It does not yet supply
the factory-child initialization, session-open or display submission calls
needed by the current probe. No hardware interoperability test of this project's
code was performed in our session.

Primary references:

- https://github.com/mizuyoukanao/NS2GameShare/blob/a19809f893ce03b4958fe09fc0b1be2436c82242/README.md
- https://github.com/mizuyoukanao/NS2GameShare/blob/a19809f893ce03b4958fe09fc0b1be2436c82242/docs/PROTOCOL.md
- https://github.com/mizuyoukanao/NS2GameShare/blob/a19809f893ce03b4958fe09fc0b1be2436c82242/docs/PASSPHRASE.md

## OunceCake: tool for extracting owned cartridges

Repository: https://github.com/misshexa66/OunceCake

Inspected revision: `ef81b7f3ef4bb8db6d429b6dddd7a476e6b0ce02`.
Selected source/docs are saved in `evidence/sources/ouncecake/`.

Its README documents decrypted ExeFS/RomFS access for game cards, digital games,
updates, DLC and Switch 2 Editions. The OS mounts the content; the PC receives
plain files, without offline decryption keys. The repository supplies the tool,
not game executables or Nintendo SDK binaries.

The documented entry route requires kurumuz's ns2jb with `--rpc` and a retained
run folder. Its CodeFS implementation clones the already-held loader root from
that run's `*-loader-root-copy.json`. It therefore does not overcome our current
probe's `fsp-ldr` OutOfSessions result by simply requesting a new connection.
Other paths also temporarily manipulate the FS inspection context; the whole
tool should not be described as a drop-in read-only native NRO.

An HTTP/WebDAV fork also exists at https://github.com/ariankordi/OunceCake ; its
tree was inspected at `ab743fdd248641539f57d787e09d7b822ac29012`. It remains a
retained-ns2jb route, not a demonstrated replacement for our current HEN API.
No dump tool or exploit was launched, and the running agent was left intact.

## Search outcome and next dependency

No verifiable public dump of either user's Kirby SDK was obtained. Public game
analysis exists in nxnative, but the inspected StreamPlay implementations are
offline emulator stubs and do not recreate a host session.

The executable-access dependency is still unresolved. The useful next options
are exposing a bounded CodeFS read through the HEN-held loader root, or a
separate retained acquisition that can extract only the owned SDK. The latter
would interrupt the current research setup and must not be launched silently.
Neither requires connecting this Switch to Nintendo servers.

The probe has created a scoped StreamPlay interface. It has **not** opened a
discoverable host session, streamed an image or accepted a Nintendo receiver.
