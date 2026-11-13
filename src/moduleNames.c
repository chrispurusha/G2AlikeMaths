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

// moduleNames.c - module short names <-> types, and each type's verdict. Notes: Docs/code-notes/moduleNames.md

#include <string.h>
#include <strings.h>

#include "constData.h"
#include "moduleResourcesAccess.h"
#include "soundEngine.h"
#include "moduleNames.h"

typedef struct {
    const char * name;
    int          state;
} tVerdict;

static const tVerdict kVerdict[] = {
#include "moduleStatus.inc"
};

// notes §1 - the spellings another editor uses, beside the G2's own
static const struct {
    const char * alias;
    const char * name;
} kAlias[] = {
    {"Eq3Band",    "Eq3band"  },
    {"Compressor", "Compress" },
    {"S&H",        "SandH"    },
    {"Res",        "Resonator"},
};

#define SAMPLER_NAME    "Sampler"   // notes §2 - G2Alike's own, not in the G2's table

static int def_count(void) {
    return (int)(sizeof(constModules) / sizeof(constModules[0]));
}

static bool type_is_real(int type) {
    if ((type <= 0) || (type >= (int)moduleTypeMax)) {
        return false;
    }

    if (type == (int)moduleTypeSampler) {
        return true;
    }
    return (type < def_count()) && (constModules[type].id == type) && (strcmp(constModules[type].name, "Unknown") != 0);
}

const char * g2am_type_name(int type) {
    if (type_is_real(type) == false) {
        return NULL;
    }
    return (type == (int)moduleTypeSampler) ? SAMPLER_NAME : constModules[type].name;
}

int g2am_type_from_name(const char * name) {
    if (name == NULL) {
        return -1;
    }

    for (size_t i = 0; i < (sizeof(kAlias) / sizeof(kAlias[0])); i++) {
        if (strcmp(name, kAlias[i].alias) == 0) {
            name = kAlias[i].name;
            break;
        }
    }

    for (int type = 1; type < (int)moduleTypeMax; type++) {
        const char * own = g2am_type_name(type);

        if ((own != NULL) && (strcmp(own, name) == 0)) {
            return type;
        }
    }

    return -1;
}

// Types in id order, skipping the gaps - with extensions, G2Alike's additions at the end
int g2am_type_at(int i, int extensions) {
    int n = 0;

    for (int type = 1; type < (int)moduleTypeMax; type++) {
        if ((type_is_real(type) == false) || ((extensions == 0) && module_is_engine_only((tModuleType)type))) {
            continue;
        }

        if (n == i) {
            return type;
        }
        n++;
    }

    return -1;
}

static const char * palette_label(int type) {
    for (uint32_t i = 0; i < array_size_palette_list(); i++) {
        if ((int)gPaletteList[i].moduleType == type) {
            return gPaletteList[i].menuLabel;
        }
    }

    return NULL;
}

// notes §3
const char * g2am_type_status(int type) {
    static const char * kState[] = {"confirmed", "modelled", "approximate", "partial", "silent"};
    static tModule      probe;     // only .type is read; static because a tModule is large

    if (type_is_real(type) == false) {
        return NULL;
    }
    const char * label = palette_label(type);

    if (label == NULL) {
        return "missing";
    }
    probe.type = (tModuleType)type;

    if (sound_engine_models_module(&probe) == false) {
        return "silent";
    }

    for (size_t i = 0; i < (sizeof(kVerdict) / sizeof(kVerdict[0])); i++) {
        if ((strcmp(kVerdict[i].name, label) == 0) && (kVerdict[i].state >= 0) && (kVerdict[i].state <= 4)) {
            return kState[kVerdict[i].state];
        }
    }

    return "modelled";
}
