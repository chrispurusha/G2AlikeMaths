# G2AlikeMaths - design

Living design note, started 2026-11-06. Nothing built yet.

G2AlikeMaths is G2Alike's sound engine as a stand-alone C library, in its own repo, for other programs to
play G2 patches with - first among them Jan den Besten's NordModularG2-Editor (Electron + TypeScript, with a C
daemon for the USB side). G2Alike itself does not use it at first; moving G2Alike onto it is a later update (§9).

## 1. What it has to do

- Build a patch: add and remove modules by name, connect and disconnect them, set parameters and modes.
- Play it: notes and MIDI in, audio out, the G2's audio inputs in.
- Report back: the meters and LEDs the G2 would show, voice counts, CPU load.
- Let the caller choose the audio device and MIDI ports from its own GUI.
- Only the modules the G2 has. G2Alike's own additions (the Sampler, engine-only parameter values) only when the
  caller turns extensions on.

## 2. Shape of the API

- **Plain C, one header** (`g2alikemaths.h`), an opaque engine handle, no globals visible. C is what the editor's
  daemon is written in, and what a Node-API addon wraps without ceremony.
- **No enums or macro constants for modules.** A module type is its G2 short name as a string: `"OscB"`,
  `"FltClassic"`, `"2-Out"`. 166 of the 170 names in the editor's own module table (`nmg2mods.ts`) are already
  spelled as G2Alike spells them; the other four are accepted either way (`Eq3Band`/`Eq3band`,
  `Compressor`/`Compress`, `S&H`/`SandH`, `Res`/`Resonator`).
- **Connectors, parameters and modes by index** - the indices the `.pch2` file and the USB protocol carry, which the
  editor already holds for every module. G2Alike has no canonical names for these (its tables hold the labels drawn
  on the module face, often none), so names would mean a new table for both sides to keep in step; indices need
  none. A name lookup can be added later if wanted (§10).
- **Values are the G2's own 0-127 (or 0..n-1 for a list)**, as on the wire. The library owns what a value means.
- **Status codes, not exceptions**: every call returns `G2AM_OK` or a negative code; `g2am_last_error()` says why
  in words.
- **Edits are batched.** Changes collect until `g2am_commit()`, which rebuilds the engine's snapshot once - the
  same cost whether one dial moved or a whole patch arrived. A dial turned continuously just commits each time.

## 3. The header

The draft is `include/g2alikemaths.h`. It merges two proposals: an earlier one (`g2SoundEngineLib.h`, 2026-11-02,
`g2se_` names) and this note's first sketch.

Kept from the earlier one:
- the version handshake (`api_version` in the config);
- `max_frames`, and `g2am_prepare()` when the device changes;
- MIDI and notes with a frame offset, so the audio thread can place them within a block;
- performances;
- `g2am_variation_changed()`, for a CtrlSend or program change inside a patch;
- the stats struct;
- non-interleaved Out 1-4 and In 1-4.

Changed from it:
- **Structure is built, not only loaded.** The earlier draft changed modules and cables only by sending a whole
  new `.pch2`. Now modules and cables are added and removed one at a time, which is how an editor works anyway, and
  which keeps the `.pch2` reader (and the SynthLib code it brings, §6) out of v1.
- **No enums.** The location enum became an `area` string. The module-status enum became a string. The result enum
  became 0 or a negative number with `g2am_last_error()`. The module type id became its short name.
- **Fewer constants**: only `G2AM_API_VERSION` is left; slot, variation and channel counts are stated in the comments.
- **Settings by name** (`g2am_patch_setting()`, `g2am_performance_setting()`) instead of a call each.
- **The optional device layer** (§4) is new.

Threads: building, playing and feedback calls from any one non-audio thread (or several, serialised by the
caller); `g2am_render()` from the audio thread only, never blocking. That is how the engine already works in
G2Alike: edits publish a snapshot the audio thread picks up.

## 4. Audio and MIDI devices

The caller owns the GUI, so the library never decides a device. Two layers:

- **Core: the caller drives `g2am_render()`** from whatever audio callback it has. Nothing in the core opens a
  device. This is what a plug-in host or a test harness uses, and it keeps the core free of platform audio code.
- **Optional: `g2am_io`**, a small device layer for callers with no native audio of their own (an Electron app
  has none it can use in real time - Web Audio runs in the renderer, at the mercy of the UI thread):

```c
int          g2am_audio_device_count(void);
const char * g2am_audio_device_name(int i);       // and its channel counts, default rate
int          g2am_audio_open(g2am_engine * e, int device, int out_first, int in_first, int frames);
void         g2am_audio_close(g2am_engine * e);
```

  On miniaudio (one MIT-licensed C file): CoreAudio, WASAPI and ALSA/PulseAudio from the same code. G2Alike's
  Windows build already plays through it.

- **MIDI: the caller sends bytes** (`g2am_midi()`). Electron has Web MIDI built in, so the editor can list ports,
  let the user pick, and forward what arrives; MIDI is not real-time critical the way audio is. A native MIDI
  layer can follow if a caller needs it, but nothing needs it yet.

## 5. Modules: the G2's, and extensions

`module_is_engine_only()` and `param_value_is_engine_only()` already mark G2Alike's additions. With extensions
off, `g2am_module_add()` refuses the Sampler (and any later addition), `g2am_param_set()` refuses an engine-only
value, and `g2am_load_patch()` refuses a patch using either. `g2am_module_status()` comes from the module-status
generator (`tools/modulestatus.c`), so the editor can grey out what the engine does not play.

## 6. What it is built from

The engine is already platform-free, and `G2Alike/tools/do-render` proves it: the engine links headless from
eleven sources -

`soundEngine.c`, `paramCurves.c`, `waveModels.c`, `nordSample.c`, `dataBase.c`, `globalVars.c`, `cableChain.c`,
`moduleResourcesAccess.c`, `patchParamsResources.c`, `noteStack.c`, and `plugin/g2HostIo.c` (the null audio/MIDI
layer the plug-in uses).

`g2am_load_patch()` is the exception. G2Alike's `.pch2` reader (`protocol.c`) also brings in SynthLib's
`utils.c` (bit helpers), the USB message queue (`msgQueue.c`) and undo (`undo.c`). The reader would need those
stubbed, or a small reader of its own, so it is left out of v1 (§10).

**From SynthLib, without `g2am_load_patch()`: no code.** Six headers only, pulled in through G2Alike's `types.h` and `globalVars.h`:
`synthlibDefs.h`, `synthlibTypes.h`, `synthlibGlobals.h`, `synthlibQueue.h`, `geometry.h`, `floatingPanel.h` -
mostly for GUI state the engine never touches. So there is no cut-down SynthLib build to make: v1 carries copies
of those six headers, and a later step splits the engine's types from the editor's GUI state so they go.

Portability: every Apple-only include in the engine already has a fallback (pthreads, semaphores), because
G2Alike builds for Windows. Linux has not been tried.

Inside, the façade owns one G2Alike document per engine (`tG2Document`, selected per thread exactly as a plug-in
instance does), fills its patch database from the API calls, and calls `sound_engine_update_from_patch()` on
commit. So v1 needs no change to the engine at all.

## 7. The repo

- `G2AlikeMaths/` (public, GPLv3): `include/g2alikemaths.h`, `src/` (the façade and `g2am_io`), `engine/`
  (the sources above), `Docs/`, a `Makefile` and a `CMakeLists.txt` (the editor's daemon builds with make),
  static and shared library, and `examples/` - a few lines that build an OscB into a 2-Out and play a note.
- **Licence**: GPLv3, as G2Alike. The editor is AGPLv3; GPLv3 §13 allows the two to be combined.
- **Same rules as G2Alike's public repo**: it says what the engine does and how it was measured. The engine
  sources point their comments at `Docs/sound-engine-reference.md` and `sound-engine-notes.md` (`// §n`); those
  docs either come along or the pointers go.

## 8. Keeping it in step with G2Alike (v1)

The engine stays developed in G2Alike for now. G2AlikeMaths takes a copy: `./sync-from-g2alike` copies the
sources and headers in `engine/`, and records G2Alike's commit in `engine/VERSION`, so every release says which
engine it carries. Nothing is edited in `engine/` by hand.

## 9. Later: G2Alike on G2AlikeMaths

The engine moves into G2AlikeMaths for good, and G2Alike - application and plug-in - links it like any other
caller. That is when the engine's types are split from G2Alike's GUI state (§6) and the SynthLib headers go.
Not part of v1.

## 10. Open questions

- **How the editor wants to call it**: a Node-API addon in the Electron main process, or inside (or beside) its C
  daemon, driven over the same JSON stdin/stdout. The C API serves both; the choice decides who owns the audio
  callback.
- **Connector and parameter names** as well as indices: worth a shared table, or are indices enough?
- **Performances** (four slots, keyboard ranges, the master clock): v1 or later? The engine already plays them.
- **`g2am_load_patch()`: proposed for later.** The editor already parses `.pch2` itself and can build module by
  module, and the reader brings SynthLib code with it (§6). Worth adding if building patch by patch proves slow
  or awkward on the editor's side.
