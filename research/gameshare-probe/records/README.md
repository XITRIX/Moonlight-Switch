# Portable research records

- console-jobs.jsonl contains 66 allowlisted console result records and physical
  screen confirmations, indexed by capture timestamp and original filename.
  Framework publication result and native return_value are distinct. Raw native
  output bytes are omitted, including NACP and executable/memory data.
  The final 17 records belong to the November16 autonomous continuation; three
  use recovered-result.json after network reply loss, without POST replay.
- installed-probe-0.3.1-layout.json contains only our own probe's symbol offsets
  and byte guards. qualify.py --installed-031 regenerates the required inputs.
- installed-probe-0.3.1-build.json records original artifact/toolchain hashes;
  hardware confirmation is in HOST_BRINGUP.md and console-jobs.jsonl.
- public-sources/ retains upstream URLs and source hashes. Retrieved Nintendo
  SDK reconstructions and third-party implementation copies are not vendored;
  use the pinned upstream links in the research notes/manifests to inspect them.
- cartridge-inventory/ is metadata only; no cartridge content is included.
- civ7-sdk-inventory.json preserves the symbol/version investigation summary
  with raw function bytes removed. Civ7's inspected SDK was 18.3.1, not a
  suitable native GameShare SDK.
- import-manifest.json hashes the original imported source snapshot, before
  portability edits on this branch. It is provenance, not a current build hash.

No pairing keys, requests, raw memory dumps, Nintendo SDK/game/firmware files,
installed HEN/RO backups, generated binaries or original machine paths are
needed in Git. New local output belongs in ignored evidence/, build/ or dist/.

Historical notes reference the private original evidence tree. Match their
folder timestamps to console-jobs.jsonl for the retained text outcome. Reports
explicitly distinguish console results, user-visible confirmations, source
inferences and host-only tests; do not turn an omitted private artifact into
a claim that the corresponding native experiment succeeded.
