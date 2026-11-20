# LL21: audio batching and validation

The native libnx Audren driver arms at most four hardware wave buffers per
voice. Software buffers waiting behind those slots require another driver
update before they can play. Six queued 5 ms packets therefore do not imply
that 30 ms is already armed on hardware.

LL21 combines short packets into at least 10 ms blocks. With 5 ms packets,
this adds at most 5 ms of aggregation, while allowing a 30 ms reserve to fit
within the four hardware slots. Existing reserve and bounded-backlog policies
remain in force. No compressed Opus packet is skipped before decoding.

## Automated checks

Run `scripts/test_all_windows.py` with Python and host GCC installed. It runs
all 11 suites, continues after individual failures, and saves complete results
in `build/test-results/host-tests.json`.

The new integration fixture executes the actual Audren renderer implementation
with deterministic SDK and Opus substitutes. It covers:

- 0/10/20/30 ms reserve and 5/10/20 ms decoded packets;
- 10/25/50/100 ms delivery interruptions and catch-up;
- four hardware slots versus a larger software queue;
- 0.5% playback-clock differences over one simulated minute;
- thirty simulated minutes of continuous playback;
- 100 session creation/cleanup cycles;
- decoder errors, concealment, driver update failures and buffer rejection;
- partial initialization failures and repeated cleanup;
- sample accounting, bounded queue and volume clipping.

Existing suites cover video queue ownership, decoder recovery, stream
lifecycle, profile snapshot, NVDEC power, swapchain count and network wrappers.

In the deterministic 25 ms interruption scenario, the previous 5 ms block
configuration produced 72 discard events and 27,479 unavailable samples;
10 ms blocks produced zero discard events and 240 unavailable samples.
This is a model result, not a measurement of the physical console.

## Console validation

Use the same stream settings and 30 ms reserve initially. Enable logging in
Debug for a comparison session, then disable it for normal use. The log is
`moonlight-LL21-log.txt`. Audio summaries include callback gap, driver update
duration/error counters, hardware voice-drop count and sample totals. Discard
events can now represent a coalesced block; compare sample totals across
versions instead of treating event counts as equal-duration packets.

Host tests cannot validate real Wi-Fi, DSP scheduling, audible quality,
HDMI/dock changes, physical end-to-end latency or driver synchronization.
No claim of eliminating every audible interruption is made.

Driver reference: switchbrew/libnx v4.12.0,
`nx/source/audio/driver.c` and `nx/source/audio/voice.c`.
