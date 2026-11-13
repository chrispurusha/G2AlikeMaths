# The contract

What G2AlikeMaths promises a caller, and what it expects in return. The header (`include/g2alikemaths.h`) is the
detail; this is the whole agreement in one place. A change to anything here is proposed in `exchange.md` first.

## The library promises

**Stability**
- The API changes only after an entry in `exchange.md`. Anything incompatible bumps `G2AM_API_VERSION`, and
  `g2am_create()` refuses a caller built against another version rather than misbehaving.
- Module type names are the G2's short names and stay that way. Where a caller spells one differently, both
  spellings are accepted (`Eq3Band`/`Eq3band`, `Compressor`/`Compress`, `S&H`/`SandH`, `Res`/`Resonator`).
- Indices for connectors, parameters and modes are the `.pch2` file's and never change.

**Behaviour**
- A module plays as the G2's does, as far as `g2am_module_status()` says it has been checked: `"confirmed"` has been
  compared with a G2, `"modelled"` is built but not yet compared, `"approximate"` and `"partial"` are known to differ,
  `"silent"` and `"missing"` pass no sound.
- With `extensions` off, only the G2's own modules and parameter values are accepted.
- Every call returns 0 or a negative number. A failure changes nothing, and `g2am_last_error()` says why in words.
- Nothing reaches the audio side until `g2am_commit()`, and everything since the last commit arrives together.
- `g2am_render()` never blocks, never allocates and never takes a lock, and always fills every buffer it is given.
- Meter and LED values are those the G2 reports to an editor over USB.
- Engines share nothing: one per G2, as many as wanted.

## The caller provides

**Threads**
- Control calls (building, settings, feedback, device listing) from one thread at a time per engine. Several threads
  are fine if the caller serialises them.
- `g2am_render()` from the audio thread only, and never while `g2am_audio_open()` has the device.
- `g2am_midi()` and `g2am_note()` from either.

**Addressing**
- Module indices chosen by the caller, unique within a slot's area, 1-127 as in a patch.
- Variations 0-9 for anything stored per variation; a patch plays 0-8.
- The uprate bit, as the caller would send it to a G2. The library does not work it out.
- Cables that exist on the G2: from an output to an input, or an input to an input. The library refuses others.
- `frames` no larger than `max_frames`; `g2am_prepare()` when the device's rate or block size changes.

**Devices**
- The choice of audio device and MIDI ports, made in the caller's own GUI. The library opens an audio device only
  when asked (`g2am_audio_open()`), and never a MIDI port: the caller forwards MIDI bytes.

## Not promised (yet)

- Loading a whole `.pch2` or `.prf2`: build module by module for now (`design.md` §10).
- Names for connectors and parameters: indices only.
- Sample-for-sample identity with a G2. The engine reproduces the G2's behaviour by measurement; how close each module
  is, is what `g2am_module_status()` reports.
