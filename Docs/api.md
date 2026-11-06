# Using G2AlikeMaths

Step by step, against the API in `include/g2alikemaths.h`. **Nothing is built yet** (see `status.md`); this is
written against the draft API, so the calls are exact but untested. Every call returns 0 on success and a negative
number on failure, with the reason in `g2am_last_error()`; the examples check that once, in `check()`.

## 1. Addressing - the same as the patch file

| What | How it is given | Example |
|---|---|---|
| Slot | 0-3 for A-D | `0` |
| Area | `"voice"` or `"fx"` | `"voice"` |
| Module | its index in the patch, 1-255, chosen by the caller | `3` |
| Module type | the G2's short name | `"OscB"`, `"EnvADSR"`, `"2-Out"` |
| Connector | its index among the module's inputs, or among its outputs, as the `.pch2` stores it | OscB's `Out` is output 0 |
| Parameter | its index in the module, as the `.pch2` stores it | OscB's `FreqCoarse` is 0 |
| Mode | its index among the module's modes | |
| Variation | 0-7 | `0` |
| Value | the G2's own: 0-127, or 0..n-1 for a list | `64` |

A cable runs from an output to an input, or from an input to another input (a chain, as the G2 allows). The
library works out what feeds what, as the G2 does.

## 2. Create an engine

```c
#include "g2alikemaths.h"

static void check(g2am_engine * e, int result) {
    if (result < 0) {
        fprintf(stderr, "g2am: %s\n", g2am_last_error(e));
    }
}

g2am_config config = {
    .api_version = G2AM_API_VERSION,
    .sample_rate = 48000.0,
    .max_frames  = 512,
    .extensions  = 0,   // the G2's own modules only
};
g2am_engine * e = g2am_create(&config);

if (e == NULL) {
    fprintf(stderr, "g2am: %s\n", g2am_last_error(NULL));
}
```

One engine is one G2: four slots, its own voices. Several engines share nothing.

## 3. Build a patch

A playable voice: the keyboard's gate opens an envelope, which shapes an oscillator into both outputs.

```c
check(e, g2am_clear(e, 0));

check(e, g2am_module_add(e, 0, "voice", 1, "Keyboard"));
check(e, g2am_module_add(e, 0, "voice", 2, "OscB"));
check(e, g2am_module_add(e, 0, "voice", 3, "EnvADSR"));
check(e, g2am_module_add(e, 0, "voice", 4, "2-Out"));

//                             from module, connector, is_output -> to module, input
check(e, g2am_cable_add(e, 0, "voice", 1, 1, 1, 3, 1));   // Keyboard Gate  -> EnvADSR Gate
check(e, g2am_cable_add(e, 0, "voice", 2, 0, 1, 3, 0));   // OscB Out       -> EnvADSR In
check(e, g2am_cable_add(e, 0, "voice", 3, 1, 1, 4, 0));   // EnvADSR Out    -> 2-Out InL
check(e, g2am_cable_add(e, 0, "voice", 3, 1, 1, 4, 1));   // EnvADSR Out    -> 2-Out InR

check(e, g2am_commit(e));   // everything above reaches the audio thread together
```

A module starts with the G2's default for every parameter and mode, in every variation. Nothing sounds, and nothing
changes on the audio side, until `g2am_commit()`.

Taking a module away takes its cables with it: `g2am_module_remove(e, 0, "voice", 2)`.

## 4. Turn a dial

```c
check(e, g2am_param_set(e, 0, "voice", 2, 0, 0, 76));   // OscB FreqCoarse, variation 0, an octave up
check(e, g2am_commit(e));
```

The value is the G2's 0-127, exactly what the editor sends the G2. Committing after every movement is fine: a
commit costs the same for one change as for a hundred.

Variations: set a parameter in any of the eight; which one plays is a patch setting.

```c
check(e, g2am_patch_setting(e, 0, "variation", 3));
```

Morphs: `g2am_morph(e, slot, group, value)` moves a morph group's knob, 0-127.

## 5. Play it

```c
// Raw MIDI, as the G2 would receive it; channels reach slots by the patch's own settings
const uint8_t note_on[] = {0x90, 60, 100};
check(e, g2am_midi(e, note_on, sizeof note_on, 0));

// Or straight to a slot, whatever its channel; velocity 0 is the note-off
check(e, g2am_note(e, 0, 60, 0, 0));
```

From the audio thread, the last argument places the event within the next block, in frames. From any other thread
it is ignored and the event lands at the start of the next block.

## 6. Render - if the caller owns the audio device

In the caller's audio callback:

```c
float * out[2] = {left, right};                    // Out 1 and 2, not interleaved
g2am_render(e, NULL, 0, out, 2, frames);          // no inputs: the G2's In 1-4 are silent
```

`frames` is never more than `max_frames`. When the device's rate or block size changes, call `g2am_prepare()` from
the control thread first. `g2am_render()` never blocks and never allocates.

## 6a. Or let the library open the device

For a caller with no real-time audio of its own. The caller's GUI lists the devices and the user chooses:

```c
for (int d = 0; d < g2am_audio_device_count(); d++) {
    int ins = 0, outs = 0;

    g2am_audio_device_channels(d, &ins, &outs);
    printf("%d: %s (%d in, %d out)\n", d, g2am_audio_device_name(d), ins, outs);
}
check(e, g2am_audio_open(e, chosen, 0, 0, 256));   // Out 1 on the device's first output, In 1 on its first input
...
g2am_audio_close(e);
```

While it is open, the library calls `g2am_render()` itself; the caller does not.

MIDI ports stay with the caller (an Electron app has Web MIDI built in): forward whatever arrives to `g2am_midi()`.

## 7. Read what the panel shows

Poll from the control thread, at the panel's own redraw rate:

```c
if (g2am_meters_changed(e)) {
    int left = 0, right = 0;

    if ((g2am_meter(e, 0, "voice", 4, 0, &left) == 0) && (g2am_meter(e, 0, "voice", 4, 1, &right) == 0)) {
        draw_meters(left, right);   // values as the G2 reports them over USB
    }
}

int v = g2am_variation_changed(e, 0);   // a patch picked a variation itself (CtrlSend, program change)

g2am_stats stats;
g2am_get_stats(e, &stats);               // voices per slot, CPU load, overruns
```

## 8. What the engine plays

```c
for (int i = 0; i < g2am_module_count(e); i++) {
    const char * name = g2am_module_name(e, i);
    printf("%s: %s\n", name, g2am_module_status(name));   // "confirmed", "modelled", ...
}
```

`g2am_module_add()` refuses a type the engine does not accept: an unknown name, or one of G2Alike's additions with
`extensions` off. A module that is accepted but `"silent"` or `"missing"` passes no sound, so an editor may want to
grey those out.

## 9. Finish

```c
g2am_destroy(e);   // closes the audio device too, if the library opened it
```
