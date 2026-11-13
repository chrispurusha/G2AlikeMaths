/*
 * G2AlikeMaths - the G2Alike sound engine as a library.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

// g2alikemaths.h - the API. DRAFT 2026-11-13, being built: see Docs/status.md for what works.
//
// An unofficial reproduction of the Nord G2's sound engine. Not affiliated with or endorsed by Clavia.
//
// Plain C. One engine is one G2: four slots, performance mode, its own voices. Engines share nothing.
//
// Addressed as the G2's own USB protocol addresses it, so an editor can send the engine what it sends a G2.
// A module type is the G2's short name ("OscB", "FltClassic", "2-Out"). An area is "voice", "fx" or "settings"
// (the patch settings, location 2 on the wire). Connectors, parameters and modes are the indices the .pch2 file
// and the USB protocol carry. Values are the G2's own: 0-127, or 0..n-1 for a list. No enums, no constants to
// keep in step.
//
// Variations: 0-9, as on the wire. 0-7 are the eight the panel shows, 8 the ninth the G2 keeps, 9 the
// Mutator's hidden one. A patch plays one of 0-8.
//
// Results: 0 is success, a negative number a failure; g2am_last_error() says what went wrong, in words.
//
// Threads. CONTROL calls from any one thread at a time per engine (an editor's UI or comms thread). AUDIO calls
// from the thread that renders: real time, never blocking, never allocating. Control calls may run while the
// audio thread renders; the engine hands changes across itself.

#ifndef G2ALIKEMATHS_H
#define G2ALIKEMATHS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define G2AM_API_VERSION    (1)   // pass to g2am_create(); bumped on any incompatible change

typedef struct g2am_engine g2am_engine;   // opaque

// ---- Lifetime (CONTROL) ---------------------------------------------------------------------------------------

typedef struct {
    int    api_version;   // G2AM_API_VERSION: a library and a header from different releases refuse each other
    double sample_rate;   // the audio device's; the engine converts to and from its own internal rate
    int    max_frames;    // the largest block g2am_render() will be asked for
    int    extensions;    // 0: the G2's own modules only. 1: also G2Alike's additions (the Sampler, ...)
} g2am_config;

g2am_engine * g2am_create(const g2am_config * config);   // NULL on failure, with the reason in g2am_last_error(NULL)
void          g2am_destroy(g2am_engine * e);
int           g2am_prepare(g2am_engine * e, double sample_rate, int max_frames);   // the device changed; silences
const char *  g2am_last_error(const g2am_engine * e);
const char *  g2am_version(void);                         // e.g. "G2AlikeMaths 0.1.0 (G2Alike V0.6.0-beta.3)"

// ---- Building a patch (CONTROL) - slot 0-3 (A-D), module index 1-127 as stored in the patch ----------------
//
// Changes collect until g2am_commit(), which hands them to the audio thread in one go. Each call checks its
// arguments at once and changes nothing on failure, so a commit never fails part way.

int g2am_clear(g2am_engine * e, int slot);   // a new empty patch, settings at the G2's new-patch values
int g2am_module_add(g2am_engine * e, int slot, const char * area, int module, const char * type);
int g2am_module_remove(g2am_engine * e, int slot, const char * area, int module);   // and its cables
// Uprate, as the editor works it out and sends it to a G2: 1 runs a control-rate module at audio rate
int g2am_module_uprate(g2am_engine * e, int slot, const char * area, int module, int uprate);
int g2am_cable_add(g2am_engine * e, int slot, const char * area, int from_module, int from_connector,
                   int from_is_output, int to_module, int to_connector);   // to is always an input
int g2am_cable_remove(g2am_engine * e, int slot, const char * area, int from_module, int from_connector,
                      int from_is_output, int to_module, int to_connector);

// A parameter in one variation. Area "settings" reaches the patch settings by their wire module and parameter:
// module 1 is the morphs (parameters 0-7 the eight morph knobs, 8-15 their modes), 2 Volume, 3 Glide, 4 Bend,
// 5 Vibrato, 6 Arpeggiator, 7 Octave shift and Sustain - every one stored per variation, as on the G2.
int g2am_param_set(g2am_engine * e, int slot, const char * area, int module, int param, int variation, int value);
// How far morph group 0-7 moves a parameter in one variation: the wire's byte, a signed 8-bit offset
// (1-127 up, 128-255 down from 256), 0 for none.
int g2am_morph_range(g2am_engine * e, int slot, const char * area, int module, int param, int variation,
                     int group, int range);
int g2am_mode_set(g2am_engine * e, int slot, const char * area, int module, int mode, int value);   // not per variation

// The patch's own settings that are not per variation: "voices" (1-32), "mono_poly" (0 poly, 1 mono,
// 2 legato), "variation" (the one that plays, 0-8).
int g2am_patch_setting(g2am_engine * e, int slot, const char * name, int value);
int g2am_commit(g2am_engine * e);

// Performance settings by name (applied at once): "mode" (0 = the selected slot plays, 1 = performance),
// "slot" (selected, 0-3), "keyboard_range" (0/1: ranges on), "enabled_a".."enabled_d", "keyboard_a".."keyboard_d",
// "hold_a".."hold_d", "range_low_a".."range_low_d", "range_high_a".."range_high_d" (0-127), "clock_bpm" (30-240),
// "clock_run" (0/1), "channel_a".."channel_d" (0-15, 16 off), "global_channel" (0-15, 16 off).
int g2am_performance_setting(g2am_engine * e, const char * name, int value);

// ---- What it plays (any thread) ------------------------------------------------------------------------------

int          g2am_module_count(const g2am_engine * e);      // every type this engine accepts (extensions as set)
const char * g2am_module_name(const g2am_engine * e, int i);
// "confirmed" (compared with a G2), "modelled", "approximate", "partial", "silent", "missing", or NULL if unknown
const char * g2am_module_status(const char * type);

// ---- Playing (AUDIO or CONTROL) -------------------------------------------------------------------------------
//
// From the audio thread these land at frame_offset within the next g2am_render() block; from a control thread at
// the start of the next block, and frame_offset is ignored.

// Raw MIDI, as the G2 would receive it: one channel message (notes, CCs, bend, aftertouch). The selected slot's
// channel (or in a performance the global channel) plays as the keyboard does; another slot's channel plays that
// slot; CtrlRcv / NoteRcv modules hear what they listen for. CC 1, 4 and 64 and aftertouch move their morphs.
int  g2am_midi(g2am_engine * e, const uint8_t * bytes, size_t length, int frame_offset);
// A key, as the G2's own keyboard (or an editor's PLAY_NOTE) plays it: the selected slot, or in a performance
// every slot with Keyboard on, within its range. Velocity 0 is a note-off.
int  g2am_note(g2am_engine * e, int note, int velocity, int frame_offset);
void g2am_all_notes_off(g2am_engine * e);

// ---- Rendering (AUDIO) ----------------------------------------------------------------------------------------
//
// in:  the G2's In 1-4, in_count buffers (0-4), any of them NULL for silence.
// out: Out 1-4 in that order, out_count buffers (1-4). Not interleaved. Always filled, with silence if nothing plays.
void g2am_render(g2am_engine * e, const float * const * in, int in_count, float * const * out, int out_count, int frames);

// How many frames g2am_render()'s output trails its input: the resampling to and from the engine's 96 kHz, so
// none at 88.2 kHz and above, or with economy on. Any thread; changes with g2am_prepare() and g2am_set_economy().
int g2am_latency(const g2am_engine * e);

// Economy (CONTROL): 1 runs the graph at the device's own rate below 88.2 kHz - no resampling, so no latency and
// about half the work, but not the G2's rate: the parts with 96 kHz internals step twice a sample to stay close.
// 0, the default, is the G2's 96 kHz. Applied between blocks; the patch's state is cleared, a moment's silence.
int g2am_set_economy(g2am_engine * e, int on);

// Fixed point (CONTROL): 1 stores the Phaser's, Chorus's and Flanger's words on the G2's own 24-bit step, so
// their tails and feedback end as words do; 0, the default, keeps them in floating point. Other modules not yet.
int g2am_set_fixed_point(g2am_engine * e, int on);

// The G2's output stage (CONTROL): 1, the default, rolls the outputs off at the top as the G2's converter does (a
// two-pole low-pass at 33.5 kHz); 0 leaves them flat. At a graph rate below 88.2 kHz (economy) it cannot be
// placed right and cuts lower - turn it off there.
int g2am_set_output_stage(g2am_engine * e, int on);

// ---- Beyond the G2 (CONTROL) - only with extensions on -----------------------------------------------------------

// Sets one module's internal LFO to the same module's in `from_slot`, plus `offset` of a cycle (0..1), so two
// slots' copies run a fixed distance apart. A Phaser or Flanger, in the FX area. Made by the audio thread at the
// start of the next blocks, once both have started; one link at a time - a second call replaces the first. The
// sweep jumps when it is made.
int g2am_lfo_link(g2am_engine * e, int slot, const char * area, int module, int from_slot, double offset);

// ---- What the panel shows (CONTROL, polled) -------------------------------------------------------------------
//
// Values as the G2 reports them to an editor over USB, so an editor's existing meter and LED drawing serves.

int g2am_meter(g2am_engine * e, int slot, const char * area, int module, int leg, int * value);   // < 0: none
int g2am_led(g2am_engine * e, int slot, const char * area, int module, int led, int * value);
int g2am_meters_changed(g2am_engine * e);   // 1 once after any meter or LED moved: worth a redraw
int g2am_variation_changed(g2am_engine * e, int slot);   // a CtrlSend or program change picked one: it, or -1

typedef struct {
    int voices_allowed[4];   // per slot
    int voices_sounding[4];
    int load_percent;        // worst block since the last call, % of real time
    int late_blocks;         // blocks that overran, since create
} g2am_stats;

int g2am_get_stats(g2am_engine * e, g2am_stats * out);

// ---- Audio device, optional (CONTROL) - for a caller with no real-time audio of its own --------------------
//
// The caller lists, the user picks, the caller opens by index. Open, the library drives g2am_render() itself.

int          g2am_audio_device_count(void);
const char * g2am_audio_device_name(int device);
int          g2am_audio_device_channels(int device, int * inputs, int * outputs);
int          g2am_audio_open(g2am_engine * e, int device, int first_output, int first_input, int frames);
void         g2am_audio_close(g2am_engine * e);

#ifdef __cplusplus
}
#endif

#endif
