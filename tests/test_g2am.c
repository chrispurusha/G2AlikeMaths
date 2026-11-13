/*
 * G2AlikeMaths - test harness.
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

// test_g2am.c - `make test`. Uses the public header only, as a caller would: builds patches through the API,
// renders offline and checks what comes out. Notes: Docs/code-notes/test_g2am.md

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "g2alikemaths.h"

#define RATE      (48000.0)
#define BLOCK     (256)
#define SECONDS(s) ((int)((s) * RATE))

static int sFailures;
static int sChecks;

#define CHECK(cond, ...)                                  \
    do {                                                  \
        sChecks++;                                        \
        if (!(cond)) {                                    \
            sFailures++;                                  \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                          \
            printf("\n");                                 \
        }                                                 \
    } while (0)

// Every build call must succeed; a failure says why
#define OK(e, call)                                                          \
    do {                                                                     \
        int r_ = (call);                                                     \
        CHECK(r_ == 0, "%s -> %d: %s", #call, r_, g2am_last_error(e));        \
    } while (0)

#define REFUSED(e, call)                                                     \
    do {                                                                     \
        int r_ = (call);                                                     \
        CHECK(r_ < 0, "%s was accepted", #call);                             \
        CHECK((r_ >= 0) || (g2am_last_error(e)[0] != '\0'), "%s gave no reason", #call); \
    } while (0)

static g2am_engine * make_engine(int extensions) {
    g2am_config config = {G2AM_API_VERSION, RATE, BLOCK, extensions};
    g2am_engine * e    = g2am_create(&config);

    CHECK(e != NULL, "g2am_create: %s", g2am_last_error(NULL));
    return e;
}

// Renders `frames` of Out 1 into buf (Out 2 discarded), in device-sized blocks
static void render(g2am_engine * e, float * buf, int frames) {
    float right[BLOCK];

    for (int done = 0; done < frames; done += BLOCK) {
        int     n      = (frames - done < BLOCK) ? (frames - done) : BLOCK;
        float * out[2] = {buf + done, right};

        g2am_render(e, NULL, 0, out, 2, n);
    }
}

static double rms(const float * buf, int frames) {
    double sum = 0.0;

    for (int i = 0; i < frames; i++) {
        sum += (double)buf[i] * buf[i];
    }
    return (frames > 0) ? sqrt(sum / frames) : 0.0;
}

// Fundamental from rising zero crossings, timed to a fraction of a sample, first to last over the window.
// A saw or pulse crosses rising once a cycle; the autocorrelation tried first lost cents at short lags.
static double pitch_hz(const float * buf, int frames) {
    double mean  = 0.0;
    double first = -1.0;
    double last  = -1.0;
    int    count = 0;

    for (int i = 0; i < frames; i++) {
        mean += buf[i];
    }
    mean /= (frames > 0) ? frames : 1;

    for (int i = 1; i < frames; i++) {
        double a = buf[i - 1] - mean;
        double b = buf[i] - mean;

        if ((a < 0.0) && (b >= 0.0)) {
            double at = (double)(i - 1) + (a / (a - b));

            if (first < 0.0) {
                first = at;
            } else {
                count++;
            }
            last = at;
        }
    }

    return (count > 0) ? (RATE * count / (last - first)) : 0.0;
}

static double cents(double hz, double want) {
    return (hz > 0.0) ? (1200.0 * log2(hz / want)) : 9999.0;
}

// Keyboard -> EnvADSR gate, OscB -> EnvADSR -> 2-Out L and R: the voice from Docs/api.md
static void build_voice(g2am_engine * e, int slot) {
    OK(e, g2am_clear(e, slot));
    OK(e, g2am_module_add(e, slot, "voice", 1, "Keyboard"));
    OK(e, g2am_module_add(e, slot, "voice", 2, "OscB"));
    OK(e, g2am_module_add(e, slot, "voice", 3, "EnvADSR"));
    OK(e, g2am_module_add(e, slot, "voice", 4, "2-Out"));
    OK(e, g2am_cable_add(e, slot, "voice", 1, 1, 1, 3, 1));   // Keyboard Gate -> EnvADSR Gate
    OK(e, g2am_cable_add(e, slot, "voice", 2, 0, 1, 3, 0));   // OscB Out      -> EnvADSR In
    OK(e, g2am_cable_add(e, slot, "voice", 3, 1, 1, 4, 0));   // EnvADSR Out   -> 2-Out L
    OK(e, g2am_cable_add(e, slot, "voice", 3, 1, 1, 4, 1));   // EnvADSR Out   -> 2-Out R
}

// Plays `note` for `held` seconds after `settle`, and gives the pitch heard over the held part
static double play_pitch(g2am_engine * e, int note, double * level) {
    int     frames = SECONDS(0.5);
    float * buf    = calloc((size_t)frames, sizeof(float));

    g2am_note(e, note, 100, 0);
    render(e, buf, frames);
    g2am_note(e, note, 0, 0);

    double hz = pitch_hz(buf + SECONDS(0.2), SECONDS(0.25));

    if (level != NULL) {
        *level = rms(buf + SECONDS(0.2), SECONDS(0.25));
    }
    float tail[BLOCK * 4];

    for (int i = 0; i < SECONDS(3.0) / (BLOCK * 4); i++) {   // let the release finish
        render(e, tail, BLOCK * 4);
    }
    free(buf);
    return hz;
}

// ---- The tests ---------------------------------------------------------------------------------------------------

static void test_lifetime(void) {
    printf("lifetime\n");
    g2am_config bad = {G2AM_API_VERSION + 1, RATE, BLOCK, 0};

    CHECK(g2am_create(&bad) == NULL, "a wrong api_version was accepted");
    CHECK(strstr(g2am_last_error(NULL), "version") != NULL, "no reason for the version refusal");
    CHECK(strstr(g2am_version(), "G2AlikeMaths") != NULL, "version string: %s", g2am_version());

    g2am_engine * e[3];

    for (int i = 0; i < 3; i++) {
        e[i] = make_engine(0);
    }

    for (int i = 0; i < 3; i++) {
        g2am_destroy(e[i]);
    }
}

static void test_silence(void) {
    printf("an empty engine is silent\n");
    g2am_engine * e   = make_engine(0);
    float *       buf = calloc((size_t)SECONDS(0.5), sizeof(float));

    g2am_note(e, 60, 100, 0);
    render(e, buf, SECONDS(0.5));
    CHECK(rms(buf, SECONDS(0.5)) == 0.0, "rms %g from no patch", rms(buf, SECONDS(0.5)));
    free(buf);
    g2am_destroy(e);
}

static void test_refusals(void) {
    printf("refusals\n");
    g2am_engine * e = make_engine(0);

    build_voice(e, 0);
    REFUSED(e, g2am_module_add(e, 4, "voice", 5, "OscB"));          // slot
    REFUSED(e, g2am_module_add(e, 0, "middle", 5, "OscB"));         // area
    REFUSED(e, g2am_module_add(e, 0, "voice", 0, "OscB"));          // index
    REFUSED(e, g2am_module_add(e, 0, "voice", 2, "OscB"));          // taken
    REFUSED(e, g2am_module_add(e, 0, "voice", 5, "OscZ"));          // unknown type
    REFUSED(e, g2am_module_add(e, 0, "voice", 5, "Sampler"));       // an extension, extensions off
    REFUSED(e, g2am_module_add(e, 0, "settings", 5, "OscB"));       // settings hold no modules
    REFUSED(e, g2am_cable_add(e, 0, "voice", 2, 7, 1, 3, 0));       // OscB has one output
    REFUSED(e, g2am_cable_add(e, 0, "voice", 2, 0, 1, 9, 0));       // no module 9
    REFUSED(e, g2am_param_set(e, 0, "voice", 2, 99, 0, 64));        // parameter
    REFUSED(e, g2am_param_set(e, 0, "voice", 2, 0, 10, 64));        // variation 10
    REFUSED(e, g2am_param_set(e, 0, "voice", 2, 0, 0, 128));        // value
    REFUSED(e, g2am_param_set(e, 0, "settings", 8, 0, 0, 64));      // settings modules are 1-7
    REFUSED(e, g2am_morph_range(e, 0, "voice", 2, 0, 0, 8, 12));    // group
    REFUSED(e, g2am_morph_range(e, 0, "voice", 2, 0, 0, 0, 256));   // range
    REFUSED(e, g2am_module_uprate(e, 0, "voice", 2, 2));
    REFUSED(e, g2am_patch_setting(e, 0, "variation", 9));           // a patch plays 0-8
    REFUSED(e, g2am_patch_setting(e, 0, "voices", 0));
    REFUSED(e, g2am_patch_setting(e, 0, "colour", 1));
    REFUSED(e, g2am_performance_setting(e, "range_low_e", 1));
    REFUSED(e, g2am_performance_setting(e, "clock_bpm", 300));
    const uint8_t sysex[] = {0xF0, 0x7E, 0xF7};

    REFUSED(e, g2am_midi(e, sysex, sizeof sysex, 0));
    OK(e, g2am_param_set(e, 0, "voice", 2, 0, 9, 64));              // the Mutator's variation is addressable
    OK(e, g2am_module_add(e, 0, "voice", 5, "Eq3Band"));            // an alias
    OK(e, g2am_commit(e));
    g2am_destroy(e);

    g2am_engine * x = make_engine(1);

    OK(x, g2am_module_add(x, 0, "voice", 5, "Sampler"));            // extensions on
    g2am_destroy(x);
}

static void test_note(void) {
    printf("a note sounds, at its pitch, and stops\n");
    g2am_engine * e = make_engine(0);

    build_voice(e, 0);
    OK(e, g2am_commit(e));

    double level = 0.0;
    double hz    = play_pitch(e, 69, &level);

    CHECK(level > 0.01, "A4 level %g", level);
    CHECK(fabs(cents(hz, 440.0)) < 5.0, "A4 at %.2f Hz (%.1f cents)", hz, cents(hz, 440.0));

    hz = play_pitch(e, 57, NULL);
    CHECK(fabs(cents(hz, 220.0)) < 5.0, "A3 at %.2f Hz", hz);

    float after[BLOCK];

    render(e, after, BLOCK);
    CHECK(rms(after, BLOCK) < 1e-4, "still sounding after release: rms %g", rms(after, BLOCK));
    g2am_destroy(e);
}

static void test_midi_and_offset(void) {
    printf("MIDI on the slot's channel, placed within the block\n");
    g2am_engine * e = make_engine(0);

    build_voice(e, 0);
    OK(e, g2am_commit(e));

    float         buf[BLOCK];
    float         right[BLOCK];
    float *       out[2] = {buf, right};
    const uint8_t on[]   = {0x90, 69, 100};   // channel 1: slot A's

    OK(e, g2am_midi(e, on, sizeof on, 100));
    g2am_render(e, NULL, 0, out, 2, BLOCK);
    CHECK(rms(buf, 100) == 0.0, "sound before the note's offset: rms %g", rms(buf, 100));

    float * more = calloc((size_t)SECONDS(0.3), sizeof(float));

    render(e, more, SECONDS(0.3));
    CHECK(rms(more, SECONDS(0.3)) > 0.01, "the MIDI note is silent");

    const uint8_t off[] = {0x80, 69, 0};

    OK(e, g2am_midi(e, off, sizeof off, 0));
    free(more);
    g2am_destroy(e);
}

static void test_morph_range(void) {
    printf("a morph range moves a parameter as its knob turns\n");
    g2am_engine * e = make_engine(0);

    build_voice(e, 0);
    // OscB Coarse (parameter 0) up 12 units under morph group 7 (the eighth knob), in variation 0
    OK(e, g2am_morph_range(e, 0, "voice", 2, 0, 0, 7, 12));
    OK(e, g2am_commit(e));
    double hz = play_pitch(e, 69, NULL);

    CHECK(fabs(cents(hz, 440.0)) < 5.0, "knob at 0: %.2f Hz, want 440", hz);

    OK(e, g2am_param_set(e, 0, "settings", 1, 7, 0, 127));   // the eighth knob, full
    OK(e, g2am_commit(e));
    hz = play_pitch(e, 69, NULL);
    CHECK(fabs(cents(hz, 880.0)) < 5.0, "knob at 127: %.2f Hz, want 880", hz);
    g2am_destroy(e);
}

static void test_variations(void) {
    printf("variations hold their own values, settings included\n");
    g2am_engine * e = make_engine(0);

    build_voice(e, 0);
    OK(e, g2am_param_set(e, 0, "voice", 2, 0, 1, 76));        // Coarse +12 in variation 1 only
    OK(e, g2am_commit(e));
    CHECK(fabs(cents(play_pitch(e, 69, NULL), 440.0)) < 5.0, "variation 0 moved");

    OK(e, g2am_patch_setting(e, 0, "variation", 1));
    OK(e, g2am_commit(e));
    double hz = play_pitch(e, 69, NULL);

    CHECK(fabs(cents(hz, 880.0)) < 5.0, "variation 1: %.2f Hz, want 880", hz);
    CHECK(g2am_variation_changed(e, 0) == -1, "a variation the caller chose was reported as the patch's own");

    // Patch volume (settings module 2, parameter 0) at 0 in variation 1: silent there
    OK(e, g2am_param_set(e, 0, "settings", 2, 0, 1, 0));
    OK(e, g2am_commit(e));
    double level = 1.0;

    play_pitch(e, 69, &level);
    CHECK(level < 1e-3, "patch volume 0 in variation 1 still plays at %g", level);
    g2am_destroy(e);
}

static void test_performance(void) {
    printf("a performance splits the keyboard across slots\n");
    g2am_engine * e = make_engine(0);

    build_voice(e, 0);
    build_voice(e, 1);
    OK(e, g2am_param_set(e, 1, "voice", 2, 0, 0, 76));        // slot B an octave up
    OK(e, g2am_commit(e));

    OK(e, g2am_performance_setting(e, "mode", 1));
    OK(e, g2am_performance_setting(e, "keyboard_range", 1));
    OK(e, g2am_performance_setting(e, "enabled_b", 1));
    OK(e, g2am_performance_setting(e, "keyboard_b", 1));
    OK(e, g2am_performance_setting(e, "range_high_a", 59));
    OK(e, g2am_performance_setting(e, "range_low_b", 60));
    OK(e, g2am_commit(e));

    double hz = play_pitch(e, 57, NULL);                       // A3, slot A's half: 220

    CHECK(fabs(cents(hz, 220.0)) < 5.0, "A3 in slot A: %.2f Hz, want 220", hz);
    hz = play_pitch(e, 69, NULL);                              // A4, slot B's half: up an octave, 880
    CHECK(fabs(cents(hz, 880.0)) < 5.0, "A4 in slot B: %.2f Hz, want 880", hz);

    g2am_stats stats;

    OK(e, g2am_get_stats(e, &stats));
    CHECK(stats.voices_allowed[0] >= 1, "slot A allows %d voices", stats.voices_allowed[0]);
    g2am_destroy(e);
}

// Renders with a 440 Hz sine of amplitude 0.25 on In 1 and In 2; Out 1 into buf
static void render_with_input(g2am_engine * e, float * buf, int frames) {
    float right[BLOCK];
    float sine[BLOCK];
    static long phase;

    for (int done = 0; done < frames; done += BLOCK) {
        int           n      = (frames - done < BLOCK) ? (frames - done) : BLOCK;
        float *       out[2] = {buf + done, right};
        const float * in[2]  = {sine, sine};

        for (int i = 0; i < n; i++, phase++) {
            sine[i] = 0.25f * (float)sin(2.0 * M_PI * 440.0 * (double)phase / RATE);
        }
        g2am_render(e, in, 2, out, 2, n);
    }
}

static void test_fx_input(void) {
    printf("the G2's In 1/2 through the FX area\n");
    g2am_engine * e      = make_engine(0);
    int           frames = SECONDS(1.0);
    float *       buf    = calloc((size_t)frames, sizeof(float));

    OK(e, g2am_module_add(e, 0, "fx", 1, "2-In"));
    OK(e, g2am_module_add(e, 0, "fx", 3, "2-Out"));
    OK(e, g2am_cable_add(e, 0, "fx", 1, 0, 1, 3, 0));
    OK(e, g2am_cable_add(e, 0, "fx", 1, 1, 1, 3, 1));

    double gain[4] = {0};

    for (int pad = 0; pad < 4; pad++) {
        OK(e, g2am_param_set(e, 0, "fx", 1, 2, 0, pad));
        OK(e, g2am_commit(e));
        render_with_input(e, buf, frames);
        gain[pad] = rms(buf + SECONDS(0.5), SECONDS(0.5)) / (0.25 / sqrt(2.0));
        printf("  2-In Pad %d: gain %.3f (%+.1f dB)\n", pad, gain[pad], 20.0 * log10(gain[pad] + 1e-12));
    }

    CHECK(gain[0] > 0.1, "the input does not reach the output: gain %g", gain[0]);

    // A Phaser between them: the input changes
    OK(e, g2am_module_add(e, 0, "fx", 2, "Phaser"));
    OK(e, g2am_cable_remove(e, 0, "fx", 1, 0, 1, 3, 0));
    OK(e, g2am_cable_add(e, 0, "fx", 1, 0, 1, 2, 0));
    OK(e, g2am_cable_add(e, 0, "fx", 2, 0, 1, 3, 0));
    OK(e, g2am_param_set(e, 0, "fx", 2, 2, 0, 100));   // FB
    OK(e, g2am_commit(e));
    render_with_input(e, buf, frames);

    double lo = 1e9, hi = 0.0;

    for (int w = 0; w < 10; w++) {
        double r = rms(buf + SECONDS(0.5) + (w * SECONDS(0.05)), SECONDS(0.05));

        lo = (r < lo) ? r : lo;
        hi = (r > hi) ? r : hi;
    }
    printf("  Phaser: level over 0.5 s from %.4f to %.4f\n", lo, hi);
    CHECK(hi > 0.01, "the Phaser passes nothing");
    free(buf);
    g2am_destroy(e);
}

static void test_module_list(void) {
    printf("the module list and verdicts\n");
    g2am_engine * e = make_engine(0);
    int           n = g2am_module_count(e);

    CHECK(n >= 160, "only %d module types", n);

    for (int i = 0; i < n; i++) {
        const char * name = g2am_module_name(e, i);

        CHECK((name != NULL) && (g2am_module_status(name) != NULL), "type %d (%s) has no status", i, name);
        CHECK((name == NULL) || (strcmp(name, "Sampler") != 0), "the Sampler listed with extensions off");
    }

    CHECK(strcmp(g2am_module_status("OscB"), "confirmed") == 0, "OscB is %s", g2am_module_status("OscB"));
    CHECK(g2am_module_status("OscZ") == NULL, "an unknown type has a status");
    g2am_destroy(e);
}

int main(void) {
    printf("%s\n", g2am_version());
    test_lifetime();
    test_silence();
    test_refusals();
    test_note();
    test_midi_and_offset();
    test_morph_range();
    test_variations();
    test_performance();
    test_fx_input();
    test_module_list();
    printf("%d checks, %d failed\n", sChecks, sFailures);
    return (sFailures == 0) ? 0 : 1;
}
