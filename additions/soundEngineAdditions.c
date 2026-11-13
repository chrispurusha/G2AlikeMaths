/*
 * G2AlikeMaths - additions to the engine copied from G2Alike.
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

// Included at the END of engine/soundEngine.c by ./sync-from-g2alike, so it reaches the engine's statics.
// G2Alike's own copy of the engine knows nothing of it. Notes: Docs/code-notes/additions.md

// notes §2 - the engine's own readouts, for one named slot rather than the selected one
#define G2AM_ON_SLOT(slot, type, call) \
    int32_t was = sEngineSlot;          \
    type    result;                     \
    sEngineSlot = (int32_t)((slot) % MAX_SLOTS); \
    result      = (call);               \
    sEngineSlot = was;                  \
    return result

bool g2am_engine_module_meter(uint32_t slot, uint32_t location, uint32_t index, uint32_t leg, uint32_t * value) {
    G2AM_ON_SLOT(slot, bool, sound_engine_module_meter(location, index, leg, value));
}

bool g2am_engine_module_led(uint32_t slot, uint32_t location, uint32_t index, uint32_t led, uint32_t * value) {
    G2AM_ON_SLOT(slot, bool, sound_engine_module_led(location, index, led, value));
}

uint32_t g2am_engine_voice_count(uint32_t slot) {
    G2AM_ON_SLOT(slot, uint32_t, sound_engine_voice_count());
}

uint32_t g2am_engine_voices_sounding(uint32_t slot) {
    G2AM_ON_SLOT(slot, uint32_t, sound_engine_voices_sounding());
}

// notes §4 - where one module's internal LFO keeps its phase, in one slot's engine: a Phaser's counter (-1..1
// a cycle) or a Flanger's signed 24-bit word. False if the module has no node, or its LFO has not started.
typedef struct {
    double *  counter;   // Phaser
    int32_t * word;      // Flanger
} tLfoPhase;

static bool lfo_phase_of(uint32_t slot, uint32_t location, uint32_t index, tLfoPhase * out) {
    int32_t was   = sEngineSlot;
    bool    found = false;

    sEngineSlot = (int32_t)(slot % MAX_SLOTS);
    {
        SE_LOCAL;

        for (uint32_t n = 0; (n < gParams.nodeCount) && (n < MAX_ENGINE_NODES) && (found == false); n++) {
            const tEngineNode * spec = &gParams.node[n];

            if ((spec->moduleIndex != index) || (spec->location != location) || (spec->postMix == false)) {
                continue;
            }

            if (spec->kind == eNodePhaser) {
                out->counter = &gLadder[0][n][7];   // the FX pass runs on voice 0
                out->word    = NULL;
                found        = true;
            } else if (spec->kind == eNodeFlanger) {
                uint32_t l = fx_buf_instance(spec, 0);

                if ((l < FXBUF_INSTANCES) && (gFxState[l].ready == true)) {
                    out->counter = NULL;
                    out->word    = &gFxState[l].phase;
                    found        = true;
                }
            }
        }
    }
    sEngineSlot = was;
    return found;
}

// notes §4 - sets `toSlot`'s module's LFO to `fromSlot`'s same module plus `offset` of a cycle
bool g2am_engine_lfo_link(uint32_t toSlot, uint32_t fromSlot, uint32_t location, uint32_t index, double offset) {
    tLfoPhase from = {0};
    tLfoPhase to   = {0};

    offset -= floor(offset);

    if (!lfo_phase_of(fromSlot, location, index, &from) || !lfo_phase_of(toSlot, location, index, &to)) {
        return false;
    }

    if ((from.counter != NULL) && (to.counter != NULL)) {
        double p = *from.counter + (2.0 * offset);

        *to.counter = (p >= 1.0) ? (p - 2.0) : p;
        return true;
    }

    if ((from.word != NULL) && (to.word != NULL)) {
        *to.word = chorus_sign24((uint32_t)*from.word + (uint32_t)lround(offset * 16777216.0));
        return true;
    }
    return false;
}
