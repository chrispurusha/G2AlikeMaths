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

// g2alikemaths.h - the API. DRAFT 2026-11-06, nothing built yet: see Docs/status.md.
//
// An unofficial reproduction of the Nord G2's sound engine. Not affiliated with or endorsed by Clavia.
//
// Plain C. One engine is one G2: four slots, performance mode, its own voices. Engines share nothing.
//
// Names, not numbers. A module type is the G2's short name ("OscB", "FltClassic", "2-Out"); an area is "voice"
// or "fx". Connectors, parameters and modes are the indices the .pch2 file and the USB protocol carry. Values are
// the G2's own: 0-127, or 0..n-1 for a list. No enums and no constants to keep in step.
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

// ---- Building a patch (CONTROL) - slot 0-3 (A-D), module index as stored in the patch ------------------------
//
// Changes collect until g2am_commit(), which hands them to the audio thread in one go.

int g2am_clear(g2am_engine * e, int slot);
int g2am_module_add(g2am_engine * e, int slot, const char * area, int module, const char * type);
int g2am_module_remove(g2am_engine * e, int slot, const char * area, int module);
int g2am_cable_add(g2am_engine * e, int slot, const char * area, int from_module, int from_connector,
                   int from_is_output, int to_module, int to_connector);   // to is always an input
int g2am_cable_remove(g2am_engine * e, int slot, const char * area, int from_module, int from_connector,
                      int from_is_output, int to_module, int to_connector);
int g2am_param_set(g2am_engine * e, int slot, const char * area, int module, int param, int variation, int value);
int g2am_mode_set(g2am_engine * e, int slot, const char * area, int module, int mode, int value);   // not per variation

// Patch settings by name: "voices", "variation", "glide", "glide_time", "bend", "bend_range", "vibrato", ...
int g2am_patch_setting(g2am_engine * e, int slot, const char * name, int value);
int g2am_morph(g2am_engine * e, int slot, int group, int value);   // a morph group's knob, 0-127
int g2am_commit(g2am_engine * e);

// Performance settings by name: "mode" (0 = the selected slot plays, 1 = performance), "slot" (selected),
// "keyboard_a".."keyboard_d", "range_low_a", "range_high_a", ..., "clock_bpm", ...
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

// Raw MIDI, as the G2 would receive it: notes, CCs, bend, aftertouch, program changes, sustain. Channels reach slots
// by the patches' and performance's own settings, and CtrlRcv / NoteRcv modules hear what they listen for.
int  g2am_midi(g2am_engine * e, const uint8_t * bytes, size_t length, int frame_offset);
// The common cases without building MIDI: straight to one slot, whatever its channel. Velocity 0 is a note-off.
int  g2am_note(g2am_engine * e, int slot, int note, int velocity, int frame_offset);
void g2am_all_notes_off(g2am_engine * e);

// ---- Rendering (AUDIO) ----------------------------------------------------------------------------------------
//
// in:  the G2's In 1-4, in_count buffers (0-4), any of them NULL for silence.
// out: Out 1-4 in that order, out_count buffers (1-4). Not interleaved. Always filled, with silence if nothing plays.
void g2am_render(g2am_engine * e, const float * const * in, int in_count, float * const * out, int out_count, int frames);

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
