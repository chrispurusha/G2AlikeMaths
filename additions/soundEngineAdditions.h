// G2AlikeMaths - the entry points additions/soundEngineAdditions.c adds to the engine. GPLv3, see LICENSE.
#ifndef G2AM_SOUND_ENGINE_ADDITIONS_H
#define G2AM_SOUND_ENGINE_ADDITIONS_H

#include <stdbool.h>
#include <stdint.h>

bool     g2am_engine_set_slot_morph(uint32_t slot, uint32_t group, double amount);
bool     g2am_engine_module_meter(uint32_t slot, uint32_t location, uint32_t index, uint32_t leg, uint32_t * value);
bool     g2am_engine_module_led(uint32_t slot, uint32_t location, uint32_t index, uint32_t led, uint32_t * value);
uint32_t g2am_engine_voice_count(uint32_t slot);
uint32_t g2am_engine_voices_sounding(uint32_t slot);

#endif
