# LL23: independent Audren feeding

Audio reception decodes Opus and copies ready 10 ms PCM blocks into a
preallocated ring. It never calls the audio driver during playback. One native
libnx thread owns all playback driver calls, serializes access with a mutex,
and updates active buffers independently of packet arrival.

The thread sleeps on a condition variable when idle. A ready block wakes it
immediately; active playback uses a 5 ms timed wait to refresh hardware slots.
Scheduling can exceed that interval; telemetry reports observed pump gaps.
There is no busy spin or per-packet heap allocation. The new ring is included
in the existing total-queue limit, rather than introducing a second reserve.
The producer uses a conservative cached hardware count, at most one refresh
old under normal scheduling, to bound total queued audio.

Default 48 kHz stereo/5 ms packets use an additional 46,080-byte PCM pool
and a 64 KiB thread stack, plus small thread/TLS/synchronization bookkeeping.
Actual CPU usage and scheduling latency require console measurement.

Startup and recovery still respect the user's reserve. Known discontinuities
retain LL22's 2 ms PCM transition and bounded catch-up correction. Hardware
buffer rejection and temporary IPC failures are handled without overwriting
an active buffer. Failed updates retain pending PCM and retry on a later wake.

Audio diagnostics use preallocated snapshots. The pump never formats or
writes log lines; the receiver flushes snapshots after releasing the audio
mutex. This lets driver updates continue during a slow diagnostic write, as
long as there is queued audio. Logging off is still preferable for normal use.

Cleanup first disables acceptance and wakes the worker. It joins the worker
without holding the mutex, closes the thread, and only then closes Audren and
frees the PCM/DMA pools and decoder. Thread creation/start errors use the same
cleanup, without joining a thread that never started.

## Verification

`scripts/test_all_windows.py` runs 13 host suites, including:

- Production-renderer simulation: reserve 0/10/20/30 ms, packet 5/10/20 ms,
  burst delivery, 10/25/50/100 ms gaps, drift, thirty simulated minutes,
  repeated sessions, Opus errors/concealment, driver errors, rejected buffers,
  service/decoder/thread initialization failures and repeated cleanup.
- A specific driver-slot scenario primes 60 ms of decoded audio, then pauses
  callbacks for 50 ms. Packet-coupled servicing produces 480 unavailable
  samples; independent servicing produces zero in this deterministic model.
- Real host threads with the production pump: idle blocking, periodic updates
  without callbacks, SDK updates restricted to the pump thread, no logging
  from the pump, 80 ms diagnostic-output delay, 60 restarts, reception during
  shutdown, temporary update failures, join-before-close and handle cleanup.
- Existing video queue, recovery, stream lifecycle, profiles, power, swapchain,
  network wrapper, audio policy/sample accounting and PCM seam suites.

SDK and Opus are substitutes in host tests. These tests do not measure actual
DSP/Wi-Fi behavior, CPU load, battery life, audible quality or end-to-end
latency, and cannot guarantee elimination of all audio interruptions.

For initial console comparison retain 30 ms reserve. Enable Debug logging for
one comparison session; the filename is `moonlight-LL23-log.txt`. New `Audren
pump` summaries expose update cadence and separate software/hardware backlog.
Disable logging for normal use.
