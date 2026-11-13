/*
 * G2AlikeMaths - example: builds a voice, plays A4 for a second and writes play_note.wav (Out 1/2, 48 kHz).
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

#include <stdint.h>
#include <stdio.h>

#include "g2alikemaths.h"

#define RATE      (48000)
#define BLOCK     (256)
#define SECONDS   (2)

static void check(g2am_engine * e, int result) {
    if (result < 0) {
        fprintf(stderr, "g2am: %s\n", g2am_last_error(e));
    }
}

static void put(FILE * f, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; i++) {
        fputc((int)((v >> (8 * i)) & 0xFF), f);
    }
}

int main(void) {
    g2am_config   config = {G2AM_API_VERSION, RATE, BLOCK, 0};
    g2am_engine * e      = g2am_create(&config);

    if (e == NULL) {
        fprintf(stderr, "g2am: %s\n", g2am_last_error(NULL));
        return 1;
    }
    check(e, g2am_module_add(e, 0, "voice", 1, "Keyboard"));
    check(e, g2am_module_add(e, 0, "voice", 2, "OscB"));
    check(e, g2am_module_add(e, 0, "voice", 3, "EnvADSR"));
    check(e, g2am_module_add(e, 0, "voice", 4, "2-Out"));
    check(e, g2am_cable_add(e, 0, "voice", 1, 1, 1, 3, 1));   // Keyboard Gate -> EnvADSR Gate
    check(e, g2am_cable_add(e, 0, "voice", 2, 0, 1, 3, 0));   // OscB Out      -> EnvADSR In
    check(e, g2am_cable_add(e, 0, "voice", 3, 1, 1, 4, 0));   // EnvADSR Out   -> 2-Out L
    check(e, g2am_cable_add(e, 0, "voice", 3, 1, 1, 4, 1));   // EnvADSR Out   -> 2-Out R
    check(e, g2am_commit(e));

    FILE * f = fopen("play_note.wav", "wb");

    if (f == NULL) {
        return 1;
    }
    uint32_t bytes = (uint32_t)(RATE * SECONDS * 2 * 2);

    fwrite("RIFF", 1, 4, f); put(f, 36 + bytes, 4); fwrite("WAVEfmt ", 1, 8, f);
    put(f, 16, 4); put(f, 1, 2); put(f, 2, 2); put(f, RATE, 4); put(f, RATE * 4, 4); put(f, 4, 2); put(f, 16, 2);
    fwrite("data", 1, 4, f); put(f, bytes, 4);

    float   left[BLOCK], right[BLOCK];
    float * out[2] = {left, right};

    g2am_note(e, 69, 100, 0);

    for (int done = 0; done < RATE * SECONDS; done += BLOCK) {
        if (done == RATE) {
            g2am_note(e, 69, 0, 0);
        }
        g2am_render(e, NULL, 0, out, 2, BLOCK);

        for (int i = 0; i < BLOCK; i++) {
            float l = (left[i] > 1.0f) ? 1.0f : ((left[i] < -1.0f) ? -1.0f : left[i]);
            float r = (right[i] > 1.0f) ? 1.0f : ((right[i] < -1.0f) ? -1.0f : right[i]);

            put(f, (uint32_t)(int16_t)(l * 32767.0f), 2);
            put(f, (uint32_t)(int16_t)(r * 32767.0f), 2);
        }
    }

    fclose(f);
    g2am_destroy(e);
    printf("wrote play_note.wav\n");
    return 0;
}
