# LL24: full audio-path diagnostics

This release instruments audio without changing video rendering, queue targets,
codec selection, reserve or playback policy. Enable the existing Debug log
setting before starting a stream. With diagnostics off, the C hook is null and
does not read a clock or allocate/format records.

Two files are written beside the NRO:

- `moonlight-LL24-log.txt`: existing general log and summaries.
- `moonlight-LL24-audio-log.txt`: detailed audio CSV with an inline stage legend.
  Sessions are appended, each with a header and footer, preserving reconnect
  evidence. This file can grow considerably; use a short comparison session.

Each detailed event records client monotonic microseconds, stage, RTP sequence,
and four stage-specific fields. Unknown/concealed sequence is UINT64_MAX. The
first sequence in a coalesced PCM block is retained through driver submission;
the queue event also includes the last source sequence in the block.

Captured stages include socket receive begin/return (packet type/size, RTP
timestamp and SSRC), intentional startup drops, FEC queue input/result and
recovered/failed/out-of-order counters, FEC output, decoder-queue offer/result,
wait/dequeue, decryption begin/end, callback begin/end, audio-mutex wait/acquire,
Opus begin/end, PCM queue/drop, pump begin/end, driver-update begin/end, played
sample counters, precise/coarse queued duration, hardware voice drops, every
wave-buffer state's occupancy, DMA submission result, voice start and exact
underrun context. General summary-log begin/end are measured separately to
expose delays caused by diagnostic output itself.

Every network/audio producer copies a fixed-size record into a separate bounded
ring under a short mutex. No producer formats or writes this CSV. An optional
lower-priority native writer thread formats batches and flushes them. The ring
uses 8192 records (about 448 KiB) and the writer uses a 64 KiB stack; these are
allocated only while detailed logging is enabled. The queue drops diagnostic
records when full, never audio data, and reports the diagnostic loss count in
overflow markers and the session footer. Batch flushing may still affect SD
and OS scheduling; the measurements must be interpreted with this limitation.

`scripts/analyze_audio_trace.py` summarizes receive gaps, receive-to-decoder
queue delay, decoder-queue residence, decryption, callback/lock/Opus/driver
duration, PCM-to-driver delay and preceding event ages for each underrun.
RTP sequence wraps, FEC recovery, trace loss and partial sessions require raw
context. CSV record order can differ slightly from timestamp order because
producers run concurrently; the analyzer sorts each session by timestamp.

Socket-return time is a client observation, not physical Wi-Fi arrival time.
A receive gap alone cannot distinguish server scheduling, network delay or
time spent waiting for the receive thread to run.

## Verification

All 16 host suites passed. Added tests exercise the actual recorder with
15,000 concurrent records, overflow accounting, session append, file/thread
startup failure and cleanup; the production C callback bridge with two threads
and tracing disabled; and analyzer pairing/session/underrun handling. Existing
production-renderer simulations and real-thread lifecycle tests remain active.
The native Switch compiler builds the instrumented C transport and C++ renderer.
Physical console timing and audible behavior remain untested here.

The common-C submodule has its own commit. Distribution archives include a
separate `moonlight-common-c-audio.patch` and base/commit metadata, alongside
the application patch, so the transport instrumentation is reproducible.

For comparison retain 30 ms, enable Debug, reproduce the symptom for one or two
minutes, stop the stream normally so the writer drains, then provide both logs.
Disable logging afterward for everyday use.
