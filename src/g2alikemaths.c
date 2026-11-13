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

// g2alikemaths.c - the API over the engine copied from G2Alike. Notes: Docs/code-notes/g2alikemaths.md

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "../include/g2alikemaths.h"

#include "sysIncludes.h"
#include "types.h"
#include "defs.h"
#include "globalVars.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "noteStack.h"
#include "soundEngine.h"
#include "../additions/soundEngineAdditions.h"
#include "moduleNames.h"

#ifndef G2AM_ENGINE_VERSION
#define G2AM_ENGINE_VERSION    "G2Alike unknown"
#endif
#define G2AM_VERSION           "G2AlikeMaths 0.1.0"

#define G2AM_EVENTS            (1024)    // notes §4 - a power of two
#define G2AM_BLOCK_EVENTS      (256)     // the most one render places; the rest wait for the next block
#define G2AM_REBUILD_POLL_US   (4000)    // notes §5
#define G2AM_AREAS             (2)       // voice and fx; settings always exist
#define G2AM_MODULE_MIN        (1)
#define G2AM_MODULE_MAX        (MAX_NUM_MODULES - 1)
#define G2AM_SETTINGS_FIRST    ((int)patchModuleMorph)
#define G2AM_SETTINGS_LAST     ((int)patchModuleSustain)
#define G2AM_VOICES_MAX        (32)
#define G2AM_PLAYED_VARIATIONS (NUM_VARIATIONS)   // 0-8
#define G2AM_MODE_VALUE_MAX    (63)               // six bits on the wire
#define G2AM_CHANNEL_OFF       (16)

#define MORPH_WHEEL            (0u)
#define MORPH_AFTERTOUCH       (3u)
#define MORPH_SUSTAIN          (4u)
#define MORPH_CTRL_PEDAL       (5u)

typedef enum {
    eOpClear,
    eOpModuleAdd,
    eOpModuleRemove,
    eOpUprate,
    eOpCableAdd,
    eOpCableRemove,
    eOpParam,
    eOpMorphRange,
    eOpMode,
    eOpVoices,
    eOpMonoPoly,
    eOpVariation
} tOpKind;

typedef struct {
    tOpKind  kind;
    uint32_t slot;
    uint32_t location;
    uint32_t module;
    int32_t  a, b, c, d, f;
} tOp;

typedef enum {
    eEventMidi,
    eEventKey,
    eEventAllOff
} tEventKind;

typedef struct {
    _Atomic uint32_t sequence;   // claim + 1 once filled
    uint8_t          kind;
    uint8_t          bytes[3];
    int32_t          offset;
} tEvent;

typedef struct {
    uint8_t kind;
    uint8_t bytes[3];
    int32_t offset;
} tBlockEvent;

struct g2am_engine {
    tG2Document *    doc;
    double           sampleRate;
    int              maxFrames;
    int              extensions;
    char             error[256];

    // CONTROL thread: what has been asked for, and the edits waiting for g2am_commit()
    int16_t          shadow[MAX_SLOTS][G2AM_AREAS][MAX_NUM_MODULES];   // type, or -1
    tOp *            ops;
    size_t           opCount;
    size_t           opCapacity;
    int              knobSent[MAX_SLOTS][NUM_MORPHS];   // notes §3 - the knob value last given the engine
    int              variationSeen[MAX_SLOTS];

    // notes §4 - MIDI and keys from any thread, placed by the next render
    tEvent           events[G2AM_EVENTS];
    _Atomic uint32_t eventWrite;
    uint32_t         eventRead;           // audio thread only
    tBlockEvent      block[G2AM_BLOCK_EVENTS];
    float *          scratch;             // interleaved, SOUND_ENGINE_OUT_CHANNELS wide

    // notes §5
    pthread_mutex_t  rebuildMutex;
    pthread_t        rebuildThread;
    bool             rebuildRunning;
    atomic_bool      rebuildStop;
    atomic_bool      morphDirty;
};

static _Thread_local char sCreateError[256];

// ---- Helpers -------------------------------------------------------------------------------------------------

static int fail(g2am_engine * e, const char * format, ...) {
    va_list args;
    char *  into = (e != NULL) ? e->error : sCreateError;
    size_t  size = (e != NULL) ? sizeof(e->error) : sizeof(sCreateError);

    va_start(args, format);
    vsnprintf(into, size, format, args);
    va_end(args);
    return -1;
}

static int ok(g2am_engine * e) {
    e->error[0] = '\0';
    return 0;
}

// EVERY ENTRY STARTS HERE: this engine's document, and with it its engine banks, on the calling thread
static void enter(g2am_engine * e) {
    g2_document_select(e->doc);
}

static int area_location(const char * area) {
    if (area == NULL) {
        return -1;
    }

    if (strcmp(area, "voice") == 0) {
        return (int)locationVa;
    }

    if (strcmp(area, "fx") == 0) {
        return (int)locationFx;
    }

    if (strcmp(area, "settings") == 0) {
        return (int)locationMorph;
    }
    return -1;
}

static int area_row(int location) {
    return (location == (int)locationVa) ? 0 : 1;
}

static uint32_t connector_count(int type, tConnectorDir dir) {
    uint32_t count = 0;

    for (uint32_t i = 0; i < array_size_connector_location_list(); i++) {
        if (((int)connectorLocationList[i].moduleType == type) && (connectorLocationList[i].direction == dir)) {
            count++;
        }
    }

    return count;
}

static int push(g2am_engine * e, tOp op) {
    if (e->opCount == e->opCapacity) {
        size_t capacity = (e->opCapacity == 0) ? 256 : (e->opCapacity * 2);
        tOp *  grown    = (tOp *)realloc(e->ops, capacity * sizeof(tOp));

        if (grown == NULL) {
            return fail(e, "out of memory");
        }
        e->ops        = grown;
        e->opCapacity = capacity;
    }
    e->ops[e->opCount++] = op;
    return ok(e);
}

static int check_slot(g2am_engine * e, int slot) {
    if ((e == NULL) || (slot < 0) || (slot >= MAX_SLOTS)) {
        return (e == NULL) ? -1 : fail(e, "slot %d: slots are 0-3", slot);
    }
    return 0;
}

// The module's type for a voice/fx address, or -1 with the error set
static int check_module(g2am_engine * e, int slot, const char * area, int module, int * location) {
    if (check_slot(e, slot) < 0) {
        return -1;
    }
    *location = area_location(area);

    if (*location < 0) {
        return fail(e, "area \"%s\": areas are \"voice\", \"fx\" and \"settings\"", (area != NULL) ? area : "(null)");
    }

    if (*location == (int)locationMorph) {
        return fail(e, "area \"settings\" holds the patch settings, not modules");
    }

    if ((module < G2AM_MODULE_MIN) || (module > G2AM_MODULE_MAX)) {
        return fail(e, "module %d: indices are %d-%d", module, G2AM_MODULE_MIN, G2AM_MODULE_MAX);
    }
    int type = e->shadow[slot][area_row(*location)][module];

    if (type < 0) {
        return fail(e, "slot %d %s module %d does not exist", slot, area, module);
    }
    return type;
}

// ---- Lifetime --------------------------------------------------------------------------------------------------

static void rebuild(g2am_engine * e) {
    pthread_mutex_lock(&e->rebuildMutex);
    database_read_lock();
    sound_engine_update_from_patch();
    database_read_unlock();
    pthread_mutex_unlock(&e->rebuildMutex);
}

static void * rebuild_worker(void * arg) {
    g2am_engine * e = (g2am_engine *)arg;

    enter(e);

    while (atomic_load(&e->rebuildStop) == false) {
        bool rate = sound_engine_take_rate_changed();

        if ((atomic_exchange(&e->morphDirty, false) == true) || (rate == true) || sound_engine_variation_pending()) {
            rebuild(e);
        }
        usleep(G2AM_REBUILD_POLL_US);
    }

    return NULL;
}

static void reset_shadow(g2am_engine * e, int slot) {
    for (int a = 0; a < G2AM_AREAS; a++) {
        for (int m = 0; m < MAX_NUM_MODULES; m++) {
            e->shadow[slot][a][m] = -1;
        }
    }

    for (int g = 0; g < NUM_MORPHS; g++) {
        e->knobSent[slot][g] = -1;
    }
    e->variationSeen[slot] = 0;
}

g2am_engine * g2am_create(const g2am_config * config) {
    if (config == NULL) {
        fail(NULL, "no config");
        return NULL;
    }

    if (config->api_version != G2AM_API_VERSION) {
        fail(NULL, "built against API version %d, this library is version %d", config->api_version, G2AM_API_VERSION);
        return NULL;
    }

    if ((config->sample_rate < 8000.0) || (config->sample_rate > 384000.0) || (config->max_frames < 1)) {
        fail(NULL, "sample_rate %.0f or max_frames %d out of range", config->sample_rate, config->max_frames);
        return NULL;
    }
    g2am_engine * e = (g2am_engine *)calloc(1, sizeof(g2am_engine));

    if (e == NULL) {
        fail(NULL, "out of memory");
        return NULL;
    }
    e->sampleRate = config->sample_rate;
    e->maxFrames  = config->max_frames;
    e->extensions = config->extensions;
    e->scratch    = (float *)calloc((size_t)config->max_frames * SOUND_ENGINE_OUT_CHANNELS, sizeof(float));
    e->doc        = g2_document_create();

    if ((e->scratch == NULL) || (e->doc == NULL)) {
        g2_document_destroy(e->doc);
        free(e->scratch);
        free(e);
        fail(NULL, "out of memory");
        return NULL;
    }
    enter(e);

    if (sound_engine_attach() == false) {
        g2_document_select(NULL);
        g2_document_destroy(e->doc);
        free(e->scratch);
        free(e);
        fail(NULL, "no engine free: at most %d at once", SOUND_ENGINE_MAX_ENGINES / MAX_SLOTS);
        return NULL;
    }
    pthread_mutex_init(&e->rebuildMutex, NULL);
    note_stack_all_off();
    sound_engine_set_economy(false);           // notes §1 - the graph at 96 kHz, as the G2
    sound_engine_set_split_mode(eSplitSerial);

    for (int slot = 0; slot < MAX_SLOTS; slot++) {
        init_patch((uint32_t)slot);
        reset_shadow(e, slot);
    }
    sound_engine_start_hosted(e->sampleRate);
    rebuild(e);

    atomic_store(&e->rebuildStop, false);

    if (pthread_create(&e->rebuildThread, NULL, rebuild_worker, e) == 0) {
        e->rebuildRunning = true;
    }
    return e;
}

void g2am_destroy(g2am_engine * e) {
    if (e == NULL) {
        return;
    }
    enter(e);

    if (e->rebuildRunning == true) {
        atomic_store(&e->rebuildStop, true);
        pthread_join(e->rebuildThread, NULL);
    }
    sound_engine_stop_hosted();
    sound_engine_detach();
    g2_document_select(NULL);
    g2_document_destroy(e->doc);
    pthread_mutex_destroy(&e->rebuildMutex);
    free(e->ops);
    free(e->scratch);
    free(e);
}

// notes §2 - the caller has stopped rendering
int g2am_prepare(g2am_engine * e, double sample_rate, int max_frames) {
    if (e == NULL) {
        return -1;
    }

    if ((sample_rate < 8000.0) || (sample_rate > 384000.0) || (max_frames < 1)) {
        return fail(e, "sample_rate %.0f or max_frames %d out of range", sample_rate, max_frames);
    }
    enter(e);

    if (max_frames > e->maxFrames) {
        float * grown = (float *)calloc((size_t)max_frames * SOUND_ENGINE_OUT_CHANNELS, sizeof(float));

        if (grown == NULL) {
            return fail(e, "out of memory");
        }
        free(e->scratch);
        e->scratch = grown;
    }
    e->maxFrames  = max_frames;
    e->sampleRate = sample_rate;
    sound_engine_stop_hosted();
    sound_engine_start_hosted(sample_rate);
    rebuild(e);
    return ok(e);
}

const char * g2am_last_error(const g2am_engine * e) {
    return (e != NULL) ? e->error : sCreateError;
}

const char * g2am_version(void) {
    return G2AM_VERSION " (" G2AM_ENGINE_VERSION ")";
}

// ---- Building --------------------------------------------------------------------------------------------------

int g2am_clear(g2am_engine * e, int slot) {
    if (check_slot(e, slot) < 0) {
        return -1;
    }
    reset_shadow(e, slot);
    return push(e, (tOp){.kind = eOpClear, .slot = (uint32_t)slot});
}

int g2am_module_add(g2am_engine * e, int slot, const char * area, int module, const char * type) {
    if (check_slot(e, slot) < 0) {
        return -1;
    }
    int location = area_location(area);

    if ((location < 0) || (location == (int)locationMorph)) {
        return fail(e, "area \"%s\": modules go in \"voice\" or \"fx\"", (area != NULL) ? area : "(null)");
    }

    if ((module < G2AM_MODULE_MIN) || (module > G2AM_MODULE_MAX)) {
        return fail(e, "module %d: indices are %d-%d", module, G2AM_MODULE_MIN, G2AM_MODULE_MAX);
    }
    int t = g2am_type_from_name(type);

    if (t < 0) {
        return fail(e, "module type \"%s\" is not one the G2 has", (type != NULL) ? type : "(null)");
    }

    if ((e->extensions == 0) && module_is_engine_only((tModuleType)t)) {
        return fail(e, "module type \"%s\" is a G2Alike addition: turn extensions on to use it", type);
    }

    if (e->shadow[slot][area_row(location)][module] >= 0) {
        return fail(e, "slot %d %s module %d already exists", slot, area, module);
    }
    e->shadow[slot][area_row(location)][module] = (int16_t)t;
    return push(e, (tOp){.kind = eOpModuleAdd, .slot = (uint32_t)slot, .location = (uint32_t)location,
                         .module = (uint32_t)module, .a = t});
}

int g2am_module_remove(g2am_engine * e, int slot, const char * area, int module) {
    int location = 0;

    if (check_module(e, slot, area, module, &location) < 0) {
        return -1;
    }
    e->shadow[slot][area_row(location)][module] = -1;
    return push(e, (tOp){.kind = eOpModuleRemove, .slot = (uint32_t)slot, .location = (uint32_t)location,
                         .module = (uint32_t)module});
}

int g2am_module_uprate(g2am_engine * e, int slot, const char * area, int module, int uprate) {
    int location = 0;

    if (check_module(e, slot, area, module, &location) < 0) {
        return -1;
    }

    if ((uprate != 0) && (uprate != 1)) {
        return fail(e, "uprate %d: 0 or 1", uprate);
    }
    return push(e, (tOp){.kind = eOpUprate, .slot = (uint32_t)slot, .location = (uint32_t)location,
                         .module = (uint32_t)module, .a = uprate});
}

static int cable_op(g2am_engine * e, tOpKind kind, int slot, const char * area, int from_module, int from_connector,
                    int from_is_output, int to_module, int to_connector) {
    int location = 0;
    int fromType = check_module(e, slot, area, from_module, &location);

    if (fromType < 0) {
        return -1;
    }
    int toType = check_module(e, slot, area, to_module, &location);

    if (toType < 0) {
        return -1;
    }
    uint32_t fromCount = connector_count(fromType, (from_is_output != 0) ? connectorDirOut : connectorDirIn);
    uint32_t toCount   = connector_count(toType, connectorDirIn);

    if ((from_connector < 0) || ((uint32_t)from_connector >= fromCount)) {
        return fail(e, "module %d has %u %s; %d is not one", from_module, fromCount,
                    (from_is_output != 0) ? "outputs" : "inputs", from_connector);
    }

    if ((to_connector < 0) || ((uint32_t)to_connector >= toCount)) {
        return fail(e, "module %d has %u inputs; %d is not one", to_module, toCount, to_connector);
    }
    return push(e, (tOp){.kind = kind, .slot = (uint32_t)slot, .location = (uint32_t)location,
                         .module = (uint32_t)from_module, .a = from_connector, .b = (from_is_output != 0) ? 1 : 0,
                         .c = to_module, .d = to_connector});
}

int g2am_cable_add(g2am_engine * e, int slot, const char * area, int from_module, int from_connector,
                   int from_is_output, int to_module, int to_connector) {
    return cable_op(e, eOpCableAdd, slot, area, from_module, from_connector, from_is_output, to_module, to_connector);
}

int g2am_cable_remove(g2am_engine * e, int slot, const char * area, int from_module, int from_connector,
                      int from_is_output, int to_module, int to_connector) {
    return cable_op(e, eOpCableRemove, slot, area, from_module, from_connector, from_is_output, to_module, to_connector);
}

// A parameter's address: a module's, or a patch setting's. Its count, or -1 with the error set
static int check_param_address(g2am_engine * e, int slot, const char * area, int module, int param, int variation,
                               int * location, int * type) {
    if (check_slot(e, slot) < 0) {
        return -1;
    }
    *location = area_location(area);

    if (*location < 0) {
        return fail(e, "area \"%s\": areas are \"voice\", \"fx\" and \"settings\"", (area != NULL) ? area : "(null)");
    }

    if ((variation < 0) || (variation >= NUM_VARIATIONS_USB)) {
        return fail(e, "variation %d: variations are 0-%d", variation, NUM_VARIATIONS_USB - 1);
    }
    uint32_t count = 0;

    if (*location == (int)locationMorph) {
        if ((module < G2AM_SETTINGS_FIRST) || (module > G2AM_SETTINGS_LAST)) {
            return fail(e, "settings module %d: the patch settings are modules %d-%d", module,
                        G2AM_SETTINGS_FIRST, G2AM_SETTINGS_LAST);
        }
        *type = -1;
        count = patch_setting_param_count((uint32_t)module);
    } else {
        *type = check_module(e, slot, area, module, location);

        if (*type < 0) {
            return -1;
        }
        count = module_param_count((tModuleType)*type);
    }

    if ((param < 0) || ((uint32_t)param >= count)) {
        return fail(e, "module %d has %u parameters; %d is not one", module, count, param);
    }
    return (int)count;
}

int g2am_param_set(g2am_engine * e, int slot, const char * area, int module, int param, int variation, int value) {
    int location = 0;
    int type     = 0;

    if (check_param_address(e, slot, area, module, param, variation, &location, &type) < 0) {
        return -1;
    }

    if ((value < 0) || (value > 127)) {
        return fail(e, "value %d: values are 0-127", value);
    }

    if ((type >= 0) && (e->extensions == 0) && param_value_is_engine_only((tModuleType)type, (uint32_t)param, (uint32_t)value)) {
        return fail(e, "value %d of parameter %d is a G2Alike addition: turn extensions on to use it", value, param);
    }
    return push(e, (tOp){.kind = eOpParam, .slot = (uint32_t)slot, .location = (uint32_t)location,
                         .module = (uint32_t)module, .a = param, .b = variation, .c = value});
}

int g2am_morph_range(g2am_engine * e, int slot, const char * area, int module, int param, int variation,
                     int group, int range) {
    int location = 0;
    int type     = 0;

    if (check_param_address(e, slot, area, module, param, variation, &location, &type) < 0) {
        return -1;
    }

    if ((group < 0) || (group >= NUM_MORPHS)) {
        return fail(e, "morph group %d: groups are 0-%d", group, NUM_MORPHS - 1);
    }

    if ((range < 0) || (range > 255)) {
        return fail(e, "range %d: the wire's byte, 0-255", range);
    }
    return push(e, (tOp){.kind = eOpMorphRange, .slot = (uint32_t)slot, .location = (uint32_t)location,
                         .module = (uint32_t)module, .a = param, .b = variation, .c = group, .d = range});
}

int g2am_mode_set(g2am_engine * e, int slot, const char * area, int module, int mode, int value) {
    int location = 0;
    int type     = check_module(e, slot, area, module, &location);

    if (type < 0) {
        return -1;
    }
    uint32_t count = module_mode_count((tModuleType)type);

    if ((mode < 0) || ((uint32_t)mode >= count) || (mode >= MAX_NUM_MODES)) {
        return fail(e, "module %d has %u modes; %d is not one", module, count, mode);
    }

    if ((value < 0) || (value > G2AM_MODE_VALUE_MAX)) {
        return fail(e, "mode value %d: 0-%d", value, G2AM_MODE_VALUE_MAX);
    }
    return push(e, (tOp){.kind = eOpMode, .slot = (uint32_t)slot, .location = (uint32_t)location,
                         .module = (uint32_t)module, .a = mode, .b = value});
}

int g2am_patch_setting(g2am_engine * e, int slot, const char * name, int value) {
    if (check_slot(e, slot) < 0) {
        return -1;
    }

    if (name == NULL) {
        return fail(e, "no setting named");
    }

    if (strcmp(name, "voices") == 0) {
        if ((value < 1) || (value > G2AM_VOICES_MAX)) {
            return fail(e, "voices %d: 1-%d", value, G2AM_VOICES_MAX);
        }
        return push(e, (tOp){.kind = eOpVoices, .slot = (uint32_t)slot, .a = value});
    }

    if (strcmp(name, "mono_poly") == 0) {
        if ((value < 0) || (value >= (int)monoPolyMax)) {
            return fail(e, "mono_poly %d: 0 poly, 1 mono, 2 legato", value);
        }
        return push(e, (tOp){.kind = eOpMonoPoly, .slot = (uint32_t)slot, .a = value});
    }

    if (strcmp(name, "variation") == 0) {
        if ((value < 0) || (value >= G2AM_PLAYED_VARIATIONS)) {
            return fail(e, "variation %d: a patch plays 0-%d", value, G2AM_PLAYED_VARIATIONS - 1);
        }
        return push(e, (tOp){.kind = eOpVariation, .slot = (uint32_t)slot, .a = value});
    }
    return fail(e, "patch setting \"%s\": the settings are \"voices\", \"mono_poly\" and \"variation\"", name);
}

static void apply_module_add(const tOp * op) {
    tModule *  module = (tModule *)calloc(1, sizeof(tModule));
    tModuleKey key    = {op->slot, op->location, op->module};

    if (module == NULL) {
        return;
    }
    tModuleType type  = (tModuleType)op->a;
    uint32_t    modes = module_mode_count(type);

    module->type             = type;
    module->actualParamCount = module_param_count(type);
    module->modeCount        = (modes < MAX_NUM_MODES) ? modes : MAX_NUM_MODES;

    for (uint32_t v = 0; v < NUM_VARIATIONS_USB; v++) {
        for (uint32_t p = 0; (p < module->actualParamCount) && (p < MAX_NUM_PARAMETERS); p++) {
            module->param[v][p].value = (uint8_t)module_param_default(type, p);
        }
    }

    for (uint32_t m = 0; m < module->modeCount; m++) {
        module->mode[m].value = module_mode_default(type, m);
    }
    write_module(key, module);
    free(module);
}

static void apply_module_remove(const tOp * op) {
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(op->slot, op->location, i);

        if ((cable != NULL) && cable->active && ((cable->key.moduleFromIndex == op->module) || (cable->key.moduleToIndex == op->module))) {
            memset(cable, 0, sizeof(tCable));
        }
    }

    delete_module((tModuleKey){op->slot, op->location, op->module});
}

static tCableKey cable_key(const tOp * op) {
    return (tCableKey){
        .slot                 = op->slot,
        .location             = op->location,
        .moduleFromIndex      = op->module,
        .connectorFromIoCount = (uint32_t)op->a,
        .linkType             = (uint32_t)op->b,   // 1 output to input, 0 input to input, as on the wire
        .moduleToIndex        = (uint32_t)op->c,
        .connectorToIoCount   = (uint32_t)op->d,
    };
}

static void apply(g2am_engine * e, const tOp * op) {
    tModule * module = (op->location < (uint32_t)locationMax) ? get_module_slot(op->slot, op->location, op->module) : NULL;

    switch (op->kind) {
        case eOpClear:
            init_patch(op->slot);
            break;
        case eOpModuleAdd:
            apply_module_add(op);
            break;
        case eOpModuleRemove:
            apply_module_remove(op);
            break;
        case eOpUprate:
            module->upRate = (uint32_t)op->a;
            break;
        case eOpCableAdd:
        {
            tCable cable = {0};

            write_cable(cable_key(op), &cable);
            break;
        }
        case eOpCableRemove:
            delete_cable(cable_key(op));
            break;
        case eOpParam:
            set_param_value(module, (uint32_t)op->b, (uint32_t)op->a, (uint32_t)op->c);
            break;
        case eOpMorphRange:
            module->param[op->b][op->a].morphRange[op->c] = (uint8_t)op->d;
            break;
        case eOpMode:
            module->mode[op->a].value = (uint32_t)op->b;
            break;
        case eOpVoices:
            gPatchDescr[op->slot].voiceCount = (uint32_t)(op->a - 1);   // the wire holds one fewer
            break;
        case eOpMonoPoly:
            gPatchDescr[op->slot].monoPoly = (uint32_t)op->a;
            break;
        case eOpVariation:
            gPatchDescr[op->slot].activeVariation = (uint8_t)op->a;
            e->variationSeen[op->slot]            = op->a;
            break;
    }
}

// notes §3 - each slot's morph knobs, in the variation it plays, where they moved since the last commit
static void apply_morph_knobs(g2am_engine * e) {
    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        tModule * morphs    = get_module_slot(slot, (uint32_t)locationMorph, (uint32_t)patchModuleMorph);
        uint32_t  variation = gPatchDescr[slot].activeVariation % NUM_VARIATIONS_USB;

        for (uint32_t group = 0; group < NUM_MORPHS; group++) {
            int knob = morphs->param[variation][group].value;

            if (knob != e->knobSent[slot][group]) {
                g2am_engine_set_slot_morph(slot, group, (double)knob / 127.0);
                e->knobSent[slot][group] = knob;
            }
        }
    }
}

int g2am_commit(g2am_engine * e) {
    if (e == NULL) {
        return -1;
    }
    enter(e);
    pthread_mutex_lock(&e->rebuildMutex);
    database_write_lock();

    for (size_t i = 0; i < e->opCount; i++) {
        if (e->ops[i].kind == eOpClear) {
            for (uint32_t g = 0; g < NUM_MORPHS; g++) {
                e->knobSent[e->ops[i].slot][g] = -1;
            }
        }
        apply(e, &e->ops[i]);
    }
    e->opCount = 0;
    apply_morph_knobs(e);
    database_write_unlock();
    database_read_lock();
    sound_engine_update_from_patch();
    database_read_unlock();
    pthread_mutex_unlock(&e->rebuildMutex);
    return ok(e);
}

// ---- Performance -----------------------------------------------------------------------------------------------

static int slot_suffix(const char * name, const char * stem) {
    size_t n = strlen(stem);

    if ((strncmp(name, stem, n) != 0) || (name[n] < 'a') || (name[n] > 'd') || (name[n + 1] != '\0')) {
        return -1;
    }
    return name[n] - 'a';
}

static int in_range(g2am_engine * e, const char * name, int value, int low, int high) {
    if ((value < low) || (value > high)) {
        return fail(e, "%s %d: %d-%d", name, value, low, high);
    }
    return 0;
}

int g2am_performance_setting(g2am_engine * e, const char * name, int value) {
    int slot = -1;

    if (e == NULL) {
        return -1;
    }

    if (name == NULL) {
        return fail(e, "no setting named");
    }
    enter(e);

    if (strcmp(name, "mode") == 0) {
        if (in_range(e, name, value, 0, 1) < 0) {
            return -1;
        }
        gGlobalSettings.perfMode = (uint8_t)value;
    } else if (strcmp(name, "slot") == 0) {
        if (in_range(e, name, value, 0, MAX_SLOTS - 1) < 0) {
            return -1;
        }
        gSlot                        = (uint32_t)value;
        gGlobalSettings.selectedSlot = (uint8_t)value;
    } else if (strcmp(name, "keyboard_range") == 0) {
        if (in_range(e, name, value, 0, 1) < 0) {
            return -1;
        }
        gPerfSettings.keyboardRange = (uint8_t)value;
    } else if (strcmp(name, "clock_bpm") == 0) {
        if (in_range(e, name, value, 30, 240) < 0) {
            return -1;
        }
        gGlobalSettings.masterClock = (uint8_t)value;
    } else if (strcmp(name, "clock_run") == 0) {
        if (in_range(e, name, value, 0, 1) < 0) {
            return -1;
        }
        gGlobalSettings.masterClockRunning = (uint8_t)value;
    } else if (strcmp(name, "global_channel") == 0) {
        if (in_range(e, name, value, 0, G2AM_CHANNEL_OFF) < 0) {
            return -1;
        }
        gSynthSettings.globalChan = (uint8_t)value;
    } else if ((slot = slot_suffix(name, "enabled_")) >= 0) {
        if (in_range(e, name, value, 0, 1) < 0) {
            return -1;
        }
        gGlobalSettings.slot[slot].enabled = (uint8_t)value;
    } else if ((slot = slot_suffix(name, "keyboard_")) >= 0) {
        if (in_range(e, name, value, 0, 1) < 0) {
            return -1;
        }
        gPerfSettings.slot[slot].keyboardEnabled = (uint8_t)value;
    } else if ((slot = slot_suffix(name, "hold_")) >= 0) {
        if (in_range(e, name, value, 0, 1) < 0) {
            return -1;
        }
        gPerfSettings.slot[slot].holdEnabled = (uint8_t)value;
    } else if ((slot = slot_suffix(name, "range_low_")) >= 0) {
        if (in_range(e, name, value, 0, 127) < 0) {
            return -1;
        }
        gPerfSettings.slot[slot].rangeLower = (uint8_t)value;
    } else if ((slot = slot_suffix(name, "range_high_")) >= 0) {
        if (in_range(e, name, value, 0, 127) < 0) {
            return -1;
        }
        gPerfSettings.slot[slot].rangeUpper = (uint8_t)value;
    } else if ((slot = slot_suffix(name, "channel_")) >= 0) {
        if (in_range(e, name, value, 0, G2AM_CHANNEL_OFF) < 0) {
            return -1;
        }
        gSynthSettings.midiChanSlot[slot] = (uint8_t)value;
    } else {
        return fail(e, "performance setting \"%s\" is not one this library knows", name);
    }
    atomic_store(&e->morphDirty, true);   // the next rebuild picks up which slots play
    return ok(e);
}

// ---- What it plays ---------------------------------------------------------------------------------------------

int g2am_module_count(const g2am_engine * e) {
    int extensions = (e != NULL) ? e->extensions : 0;
    int n          = 0;

    while (g2am_type_at(n, extensions) >= 0) {
        n++;
    }
    return n;
}

const char * g2am_module_name(const g2am_engine * e, int i) {
    return g2am_type_name(g2am_type_at(i, (e != NULL) ? e->extensions : 0));
}

const char * g2am_module_status(const char * type) {
    return g2am_type_status(g2am_type_from_name(type));
}

// ---- Playing ---------------------------------------------------------------------------------------------------

static int post(g2am_engine * e, tEventKind kind, const uint8_t * bytes, size_t length, int frame_offset) {
    uint32_t claim = atomic_fetch_add(&e->eventWrite, 1u);
    tEvent * event = &e->events[claim % G2AM_EVENTS];

    event->kind   = (uint8_t)kind;
    event->offset = (frame_offset > 0) ? frame_offset : 0;
    memset(event->bytes, 0, sizeof(event->bytes));

    for (size_t i = 0; (i < length) && (i < sizeof(event->bytes)); i++) {
        event->bytes[i] = bytes[i];
    }
    atomic_store(&event->sequence, claim + 1u);   // published last
    return 0;
}

int g2am_midi(g2am_engine * e, const uint8_t * bytes, size_t length, int frame_offset) {
    if ((e == NULL) || (bytes == NULL) || (length == 0) || ((bytes[0] & 0x80u) == 0u)) {
        return (e == NULL) ? -1 : fail(e, "not a MIDI message: it starts with a status byte");
    }
    uint8_t kind = bytes[0] & 0xF0u;
    size_t  need = ((kind == 0xC0u) || (kind == 0xD0u)) ? 2u : 3u;

    if ((kind == 0xF0u) || (length < need)) {
        return fail(e, "only channel messages, whole: 0x%02X needs %zu bytes", bytes[0], need);
    }
    return post(e, eEventMidi, bytes, need, frame_offset);
}

int g2am_note(g2am_engine * e, int note, int velocity, int frame_offset) {
    if ((e == NULL) || (note < 0) || (note > 127) || (velocity < 0) || (velocity > 127)) {
        return (e == NULL) ? -1 : fail(e, "note %d velocity %d: both 0-127", note, velocity);
    }
    uint8_t bytes[2] = {(uint8_t)note, (uint8_t)velocity};

    return post(e, eEventKey, bytes, 2, frame_offset);
}

void g2am_all_notes_off(g2am_engine * e) {
    if (e != NULL) {
        post(e, eEventAllOff, NULL, 0, 0);
    }
}

// notes §6 - the channel the G2 plays as its keyboard
static bool keyboard_channel(uint32_t channel) {
    uint32_t own = (gGlobalSettings.perfMode != 0u) ? gSynthSettings.globalChan : gSynthSettings.midiChanSlot[gSlot % MAX_SLOTS];

    return own == channel;
}

static void morph(g2am_engine * e, uint32_t group, uint32_t value) {
    if (sound_engine_set_morph(group, (double)value / 127.0) == true) {
        atomic_store(&e->morphDirty, true);
    }
}

// AUDIO thread
static void play(g2am_engine * e, const tBlockEvent * ev) {
    if (ev->kind == eEventAllOff) {
        note_stack_all_off();
        return;
    }

    if (ev->kind == eEventKey) {
        if (ev->bytes[1] > 0u) {
            note_stack_note_on(ev->bytes[0], ev->bytes[1]);
        } else {
            note_stack_note_off(ev->bytes[0], 0);
        }
        return;
    }
    uint8_t  kind     = ev->bytes[0] & 0xF0u;
    uint32_t channel  = ev->bytes[0] & 0x0Fu;
    uint8_t  d1       = ev->bytes[1] & 0x7Fu;
    uint8_t  d2       = ev->bytes[2] & 0x7Fu;
    bool     listened = keyboard_channel(channel);

    switch (kind) {
        case 0x80:
        case 0x90:
        {
            bool on = (kind == 0x90u) && (d2 > 0u);

            sound_engine_midi_note(channel, d1, on ? d2 : 0u, on, listened);

            if (listened == true) {
                if (on == true) {
                    note_stack_note_on(d1, d2);
                } else {
                    note_stack_note_off(d1, (kind == 0x80u) ? d2 : 0u);
                }
            }
            break;
        }
        case 0xB0:
            sound_engine_midi_cc(channel, d1, d2, listened);

            if (listened == false) {
                break;
            }

            if ((d1 == 120u) || (d1 == 123u)) {
                note_stack_all_off();
            } else if (d1 == 1u) {
                morph(e, MORPH_WHEEL, d2);
            } else if (d1 == 4u) {
                morph(e, MORPH_CTRL_PEDAL, d2);
            } else if (d1 == 64u) {
                morph(e, MORPH_SUSTAIN, d2);
            }
            break;
        case 0xE0:
            if (listened == true) {
                sound_engine_pitch_bend((double)((int32_t)(((uint32_t)d2 << 7) | d1) - 8192) / 8192.0);
            }
            break;
        case 0xA0:
            if ((listened == true) && sound_engine_note_sounding((int32_t)d1)) {
                morph(e, MORPH_AFTERTOUCH, d2);
            }
            break;
        case 0xD0:
            if (listened == true) {
                morph(e, MORPH_AFTERTOUCH, d1);
            }
            break;
        default:
            break;
    }
}

// ---- Rendering -------------------------------------------------------------------------------------------------

static uint32_t take_events(g2am_engine * e, int frames) {
    uint32_t count = 0;

    while (count < G2AM_BLOCK_EVENTS) {
        tEvent * event = &e->events[e->eventRead % G2AM_EVENTS];

        if (atomic_load(&event->sequence) != (e->eventRead + 1u)) {
            break;
        }
        tBlockEvent * to = &e->block[count];

        to->kind   = event->kind;
        to->offset = (event->offset < frames) ? event->offset : frames;
        memcpy(to->bytes, event->bytes, sizeof(to->bytes));

        // Arrival order within one offset; insertion keeps the sort stable
        for (uint32_t i = count; (i > 0) && (e->block[i - 1].offset > e->block[i].offset); i--) {
            tBlockEvent swap = e->block[i];

            e->block[i]     = e->block[i - 1];
            e->block[i - 1] = swap;
        }
        e->eventRead++;
        count++;
    }

    return count;
}

static void render_span(g2am_engine * e, const float * const * in, int in_count, float * const * out, int out_count,
                        int from, int frames) {
    int done = 0;

    while (done < frames) {
        int           chunk = frames - done;
        const float * jack[SOUND_ENGINE_INPUT_CHANNELS] = {NULL, NULL, NULL, NULL};

        if (chunk > e->maxFrames) {
            chunk = e->maxFrames;
        }

        for (int c = 0; (in != NULL) && (c < in_count) && (c < SOUND_ENGINE_INPUT_CHANNELS); c++) {
            jack[c] = (in[c] != NULL) ? (in[c] + from + done) : NULL;
        }
        sound_engine_set_input(jack, SOUND_ENGINE_INPUT_CHANNELS);
        sound_engine_render(e->scratch, (uint32_t)chunk, SOUND_ENGINE_OUT_CHANNELS);

        for (int c = 0; c < out_count; c++) {
            if (out[c] == NULL) {
                continue;
            }

            for (int i = 0; i < chunk; i++) {
                out[c][from + done + i] = e->scratch[(i * SOUND_ENGINE_OUT_CHANNELS) + c];
            }
        }

        done += chunk;
    }
}

void g2am_render(g2am_engine * e, const float * const * in, int in_count, float * const * out, int out_count, int frames) {
    if ((e == NULL) || (out == NULL) || (frames <= 0)) {
        return;
    }
    enter(e);
    out_count = (out_count < 0) ? 0 : ((out_count > 4) ? 4 : out_count);

    uint32_t count = take_events(e, frames);
    int      pos   = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (e->block[i].offset > pos) {
            render_span(e, in, in_count, out, out_count, pos, e->block[i].offset - pos);
            pos = e->block[i].offset;
        }
        play(e, &e->block[i]);
    }

    if (pos < frames) {
        render_span(e, in, in_count, out, out_count, pos, frames - pos);
    }
}

// ---- What the panel shows --------------------------------------------------------------------------------------

static int readout(g2am_engine * e, int slot, const char * area, int module, int which, int * value, bool led) {
    int location = 0;

    if ((value == NULL) || (check_module(e, slot, area, module, &location) < 0)) {
        return -1;
    }
    enter(e);
    uint32_t v = 0;
    bool     got = led ? g2am_engine_module_led((uint32_t)slot, (uint32_t)location, (uint32_t)module, (uint32_t)which, &v)
                   : g2am_engine_module_meter((uint32_t)slot, (uint32_t)location, (uint32_t)module, (uint32_t)which, &v);

    if (got == false) {
        return fail(e, "module %d has no %s %d the engine drives", module, led ? "LED" : "meter", which);
    }
    *value = (int)v;
    return ok(e);
}

int g2am_meter(g2am_engine * e, int slot, const char * area, int module, int leg, int * value) {
    return readout(e, slot, area, module, leg, value, false);
}

int g2am_led(g2am_engine * e, int slot, const char * area, int module, int led, int * value) {
    return readout(e, slot, area, module, led, value, true);
}

int g2am_meters_changed(g2am_engine * e) {
    if (e == NULL) {
        return 0;
    }
    enter(e);
    return sound_engine_meters_dirty() ? 1 : 0;
}

int g2am_variation_changed(g2am_engine * e, int slot) {
    if (check_slot(e, slot) < 0) {
        return -1;
    }
    enter(e);
    int now = (int)gPatchDescr[slot].activeVariation;

    if (now == e->variationSeen[slot]) {
        return -1;
    }
    e->variationSeen[slot] = now;
    return now;
}

int g2am_get_stats(g2am_engine * e, g2am_stats * out) {
    if ((e == NULL) || (out == NULL)) {
        return -1;
    }
    enter(e);

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        out->voices_allowed[slot]  = (int)g2am_engine_voice_count(slot);
        out->voices_sounding[slot] = (int)g2am_engine_voices_sounding(slot);
    }

    out->load_percent = (int)sound_engine_load_percent();
    out->late_blocks  = (int)sound_engine_late_blocks();
    return ok(e);
}

// ---- Audio device, optional: not built yet (Docs/status.md) ----------------------------------------------------

int g2am_audio_device_count(void) {
    return 0;
}

const char * g2am_audio_device_name(int device) {
    (void)device;
    return NULL;
}

int g2am_audio_device_channels(int device, int * inputs, int * outputs) {
    (void)device;
    (void)inputs;
    (void)outputs;
    return -1;
}

int g2am_audio_open(g2am_engine * e, int device, int first_output, int first_input, int frames) {
    (void)device;
    (void)first_output;
    (void)first_input;
    (void)frames;
    return (e == NULL) ? -1 : fail(e, "the optional audio device layer is not built yet");
}

void g2am_audio_close(g2am_engine * e) {
    (void)e;
}
