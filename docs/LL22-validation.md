# LL22 audio recovery

Changes relative to LL21:

- Catch-up rate now increases gradually with queue excess, up to 2% faster.
  Each 10 ms block changes the requested rate by no more than 0.05 percentage
  point, avoiding abrupt rate steps. The correction can slightly change pitch.
- After an emergency discard, interpolate the next block's first 2 ms from
  the last accepted sample. On startup or reserve rebuild, fade from zero.
  This masks abrupt waveform jumps; it cannot reconstruct discarded sound.
- Continuous blocks remain bit-exact after the existing volume stage.
- Never modify a buffer already handed to the audio driver. Commit continuity
  state only after the new buffer is accepted.
- Keep LL21's 10 ms hardware blocks, reserve choices and emergency queue bounds.
  There is no additional queue introduced by the waveform transition.
- Add maximum audio callback gap per reporting interval, alongside the lifetime
  maximum, so one pause cannot hide subsequent shorter interruptions.

Validation: `scripts/test_all_windows.py` runs 12 host suites and writes
`build/test-results/host-tests.json`. The new seam test uses warning-as-error
compilation and checks mono/stereo/multichannel helper arithmetic, channel
isolation, memory guards, extreme PCM values, reset and unchanged continuous
audio. Multichannel helper coverage does not add surround output to the app.
Queue policy tests cover correction bounds, monotonicity and rate slew.
Production-renderer integration retains LL21's fault, jitter, drift, repeated
session and thirty-minute simulated playback scenarios.

All hardware, network, DSP timing and audible quality still require a real
console session. The fixtures do not prove that all crackling is eliminated.
For initial comparison retain 30 ms reserve and enable Debug logging. The new
filename is `moonlight-LL22-log.txt`. Disable logging after comparison.
