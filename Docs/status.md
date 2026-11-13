# Status

What is built and working. Updated with every change; when this and `api.md` disagree, this is right.

**2026-11-13: the core is built and passes `make test`** (679 checks) on macOS. First user: AllAlike's ModAlike
(an effect plug-in: In 1/2 through the FX area). The engine is G2Alike V1.0.5's,
copied by `./sync-from-g2alike` (`engine/VERSION`).

| Part | State |
|---|---|
| Header and contract | Revised 11-13 after the first answers (`exchange.md`) |
| Engine copied from G2Alike (`engine/`, `./sync-from-g2alike`) | Done; additions in `additions/` |
| Lifetime: create, destroy, prepare, version, errors | Built, tested (prepare untested) |
| Building: modules, cables, parameters, morph ranges, modes, uprate, settings, commit | Built, tested (modes and uprate accepted, not yet heard) |
| Performance settings | Built; keyboard split tested |
| Playing: MIDI, notes, frame offsets, render, In 1-4 | Built, tested |
| Feedback: meters, LEDs, variation changes, stats | Built; meters and LEDs untested |
| What it plays: module list and status | Built, tested |
| Latency (`g2am_latency`) | Built, tested at 44.1-192 kHz |
| LFO link (`g2am_lfo_link`, extension) | Built, tested (Phaser, Flanger) |
| Economy, fixed point, output stage (`g2am_set_*`) | Built, tested; fixed point covers the Phaser, Chorus and Flanger only |
| Optional audio device layer (miniaudio) | Not started; calls return failure |
| Build: Makefile, static and shared library | Done (`make`, `make test`, `make example`); only `g2am_*` exported; `ARCHS=` universal, `BUILD=` folder |
| Build: CMakeLists.txt | Not started |
| Examples | `examples/play_note.c` |
| macOS | Builds and passes |
| Windows | Not started |
| Linux | Not started |

Known limits:

- MIDI program change and CC 70 (variation select) are not acted on yet.
- Morph groups follow their modes (settings parameters 8-15): Knob uses the knob, otherwise the controller. G.Wh1 and
  G.Wh2 (CC 96/97) take effect at the next rebuild rather than at once.
- `g2am_prepare()` must not run while the caller renders.
