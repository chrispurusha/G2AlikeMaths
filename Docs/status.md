# Status

What is built and working. Updated with every change; when this and `api.md` disagree, this is right.

**2026-11-13: the core is built and passes `make test`** (509 checks) on macOS. The engine is G2Alike V1.0.5's,
copied by `./sync-from-g2alike` (`engine/VERSION`).

| Part | State |
|---|---|
| Header and contract | Revised 11-13 after the first answers (`exchange.md`) |
| Engine copied from G2Alike (`engine/`, `./sync-from-g2alike`) | Done; additions in `additions/` |
| Lifetime: create, destroy, prepare, version, errors | Built, tested (prepare untested) |
| Building: modules, cables, parameters, morph ranges, modes, uprate, settings, commit | Built, tested (modes and uprate accepted, not yet heard) |
| Performance settings | Built; keyboard split tested |
| Playing: MIDI, notes, frame offsets, render | Built, tested; In 1-4 untested |
| Feedback: meters, LEDs, variation changes, stats | Built; meters and LEDs untested |
| What it plays: module list and status | Built, tested |
| Optional audio device layer (miniaudio) | Not started; calls return failure |
| Build: Makefile, static and shared library | Done (`make`, `make test`, `make example`) |
| Build: CMakeLists.txt | Not started |
| Examples | `examples/play_note.c` |
| macOS | Builds and passes |
| Windows | Not started |
| Linux | Not started |

Known limits:

- MIDI program change and CC 70 (variation select) are not acted on yet.
- A turned morph knob and the controller for the same group (wheel, aftertouch, pedals) share one amount: the
  last to move sets it, and a commit re-applies only the knobs that changed.
- `g2am_prepare()` must not run while the caller renders.
