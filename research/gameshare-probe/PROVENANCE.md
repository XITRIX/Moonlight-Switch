# Source provenance and scope

This folder consolidates the standalone GameShare probe and investigation from
the kaiHEN research workspace. Its original file hashes are in
records/import-manifest.json. Portability changes remove dependencies on that
workspace, preserve the existing runtime profile and add fresh-PC instructions.
It is not integrated into Moonlight's streaming pipeline.

Existing SPDX notices are preserved. The imported kaiHEN snapshot is pinned to
ouncebrew/kaihen 20261114d, commit 5ac772560698b325551db014478a5a43e941474c;
research-backend/kaihen/UPSTREAM.json lists original upstream file hashes.
The modified standalone HEN experiment retains its GPL-2.0-only source/license
and remains an inactive historical build, never a default console dependency.
The normal probe build uses only libkaihen's ISC client jit.c and headers;
their ISC notice is in research-backend/kaihen/libkaihen/LICENSE. It invokes
existing RO JIT interfaces for this NRO and installs no HEN/RO code.

Other upstream research is identified by pinned URLs/hashes in the findings
and records/public-sources/. Their full source mirrors, proprietary Nintendo
SDK binaries and console/game dumps are not included. The root Moonlight
license and existing per-file notices remain in place.

Do not deploy build/research-hen/main or modify HEN/RO under this investigation.
That archived source was retained to explain the abandoned CodeFS approach,
not as permission or a requirement to install it.
