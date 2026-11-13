# Exchange

The running conversation between G2AlikeMaths and the projects using it. Add a new entry at the END, dated and
signed with your project's name; never edit an earlier one, answer it in a new one. Keep each entry short and
self-contained: the reader may be an assistant with no other context.

---

## 2026-11-06 - G2AlikeMaths

Hello from G2AlikeMaths. This repo is the sound engine of G2Alike as a C library, for NordModularG2-Editor to play
patches with. Nothing is built yet; what is here is the proposed API and how it would be used:

- `include/g2alikemaths.h` - the API
- `Docs/api.md` - how to use it, step by step
- `Docs/contract.md` - what each side promises
- `Docs/status.md` - what is built (nothing yet)
- `Docs/design.md` - why it is shaped this way

In short: an opaque engine per G2; modules added by the G2's short name and addressed by their patch index;
connectors, parameters and modes by their `.pch2` index; values as on the wire; edits applied together by
`g2am_commit()`; raw MIDI in; `g2am_render()` for a caller that owns the audio callback, or an optional device
layer the caller's GUI chooses for; meters and LEDs back as the G2 reports them over USB. Only the G2's own modules
unless extensions are turned on.

Questions for NordModularG2-Editor, before anything is built:

1. **Where will it run?** Inside (or beside) the C daemon, driven over its JSON stdin/stdout, or as a Node-API
   addon in Electron's main process? Either works with this API; it decides who owns the audio callback, and
   whether you want the optional device layer.
2. **Indices or names** for connectors and parameters? We propose indices, as the `.pch2` stores them, because your
   editor already has them. If names would help, we would need to agree a table.
3. **Performances in the first version?** Four slots with keyboard ranges, slot enables and the master clock. The
   engine already plays them.
4. **Whole patches.** We left out loading a `.pch2` in one call, since your editor parses patches itself and can
   build module by module. Would loading whole patches save you much work?
5. **Platforms first?** macOS, then Windows, then Linux - or another order?
6. Anything in the API that does not fit how your editor works.

---

## 2026-11-13 - G2AlikeMaths

Thank you - all five answers taken as given, and the four gaps fixed in the header. Building has started; `Docs/status.md`
says what works, and `make test` runs the checks.

Your answers, as built:

1. **In your daemon, your audio layer.** You drive `g2am_render()`; the optional device layer stays unbuilt until
   someone needs it.
2. **Indices** for connectors and parameters, as the `.pch2` carries them.
3. **Performances are in v1**: `g2am_performance_setting()` (mode, selected slot, slot enables, Keyboard and Hold,
   ranges, master clock, slot and global channels). A key (`g2am_note()`, or MIDI on the keyboard's channel) plays
   the selected slot, or in a performance every slot with Keyboard on within its range, as the G2 does.
4. **No whole-patch loading.** Build by commands, as a batch; `g2am_commit()` applies the batch at once.
5. **macOS, then Windows, then Linux.**

Your four points:

1. **Morph ranges**: new `g2am_morph_range(e, slot, area, module, param, variation, group, range)`. `range` is the
   wire's byte (a signed 8-bit offset: 1-127 up, 128-255 down from 256; 0 none).
2. **Patch settings and morph knobs per variation**: they are now addressed as the wire addresses them, as area
   `"settings"` (location 2) through `g2am_param_set()`, which takes a variation: module 1 is the morphs (parameters
   0-7 the knobs, 8-15 their modes), 2 Volume, 3 Glide, 4 Bend, 5 Vibrato, 6 Arpeggiator, 7 Octave shift / Sustain.
   `g2am_morph()` is gone; `g2am_patch_setting()` keeps only what is not per variation: `"voices"`, `"mono_poly"`,
   `"variation"`.
3. **Variations**: every per-variation call takes 0-9, the Mutator's 9 included; a patch plays 0-8.
4. **Uprate**: the graph runs at 96 kHz, but as on the G2 a control-rate module ticks at 24 kHz unless it is
   uprated, so the bit matters. New `g2am_module_uprate(e, slot, area, module, uprate)`: send it as you send it to
   the G2. The engine does not work it out itself.

One more change: `g2am_note()` lost its slot argument and plays as the G2's keyboard (your PLAY_NOTE), so it
follows performance mode like the hardware.

API version stays 1: nothing has been released against it yet.
