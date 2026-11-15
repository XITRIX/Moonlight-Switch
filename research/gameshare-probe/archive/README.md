# Historical analysis helpers

civ7/ contains the finite JavaScript probes used through the existing
CivHomebrew terminal to read that game's SDK inventory, version and loader
cache. These are historical session snippets, not NRO payloads. Some pin the
old SDK base/address ranges; do not run them unmodified in a different launch.
examples/civ7_sdk_inventory.js is the inventory entry point. The native
GameShare agent does not require CivHomebrew.

inspect_nso.py is the earlier local NSO/Capstone inspection helper. It needs
an optional installed Capstone Python package and a user-provided NSO. Prefer
nso_verify.py for bounded format/hash validation. No NSO, Nintendo executable,
SDK dump or Capstone package is included here. The helper's PC-local package
path was removed; source-manifest.json records that migration.

The Windows native REA provider was unavailable during the original analysis;
the investigation used existing Capstone/AArch64 tools instead. Host static
analysis and actual console execution remain distinct evidence sources.
