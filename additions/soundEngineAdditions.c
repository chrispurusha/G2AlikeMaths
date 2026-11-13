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

// notes §1 - one slot's morph group, from that slot's morph knob, whatever slots the keyboard plays
bool g2am_engine_set_slot_morph(uint32_t slot, uint32_t group, double amount) {
    int32_t was     = sEngineSlot;
    bool    changed = false;

    sEngineSlot = (int32_t)(slot % MAX_SLOTS);
    changed     = set_morph_one(group, amount);
    sEngineSlot = was;
    return changed;
}

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
