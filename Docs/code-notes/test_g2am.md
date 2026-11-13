# tests/test_g2am.c - notes

The harness is a caller: it includes only `g2alikemaths.h` and links the static library, so a test that needs
anything else shows the contract is short of something.

- **Pitch** comes from timing rising zero crossings across the window. Autocorrelation was tried first and read
  A5 at 884 Hz (and C7 19 cents flat): the lag was too coarse at short periods. The engine was right.
- **Tolerances**: pitch within 5 cents; "sounding" above 0.01 RMS; "silent" exactly zero before a note and below
  1e-4 after its release.
- Each `play_pitch()` waits 3 s after the release so the next note starts from silence.
