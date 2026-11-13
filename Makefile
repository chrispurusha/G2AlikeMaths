# G2AlikeMaths - builds $(BUILD)/libg2alikemaths.a and .dylib/.so, the example and the tests.
#   make            the libraries
#   make test       the test harness (tests/), offline: renders and checks what it hears
#   make example    examples/play_note

CC      ?= cc
BUILD   ?= build
UNAME   := $(shell uname -s)
ENGINE_VERSION := $(shell head -1 engine/VERSION 2>/dev/null || echo unknown)

CFLAGS  ?= -O2
CFLAGS  += -std=gnu11 -fPIC -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
CFLAGS  += -DSYNTHLIB_PLUGIN_BUILD -DG2AM_ENGINE_VERSION='"$(ENGINE_VERSION)"'
CFLAGS  += -Iinclude -Iengine -Iengine/synthlib
LDLIBS  := -lm -lpthread

# ARCHS="-arch arm64 -arch x86_64" for a universal build (AllAlike's plug-ins want one)
ARCHS   ?=
CFLAGS  += $(ARCHS)

ifeq ($(UNAME),Darwin)
    CFLAGS  += -mmacosx-version-min=12.0
    SHARED  := $(BUILD)/libg2alikemaths.dylib
    SHFLAGS := -dynamiclib -install_name @rpath/libg2alikemaths.dylib
else
    SHARED  := $(BUILD)/libg2alikemaths.so
    SHFLAGS := -shared
endif

ENGINE_SRC := $(wildcard engine/*.c)
LIB_SRC    := $(wildcard src/*.c)
OBJ        := $(patsubst %.c,$(BUILD)/obj/%.o,$(ENGINE_SRC) $(LIB_SRC))
STATIC     := $(BUILD)/libg2alikemaths.a

all: $(STATIC) $(SHARED)

$(BUILD)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/obj/engine/soundEngine.o: additions/soundEngineAdditions.c additions/soundEngineAdditions.h

# notes: Docs/code-notes/build.md - one object, only g2am_* left global, so the engine's names cannot collide
# with a caller's (SynthLib's log globals did)
PUBLIC := $(BUILD)/obj/g2alikemaths-public.o

ifeq ($(UNAME),Darwin)
$(PUBLIC): $(OBJ)
	$(CC) $(ARCHS) -r -nostdlib -Wl,-exported_symbol,'_g2am_*' -o $@ $^
else
$(PUBLIC): $(OBJ)
	$(LD) -r -o $@.all $^
	objcopy --wildcard --keep-global-symbol='g2am_*' $@.all $@
	@rm -f $@.all
endif

$(STATIC): $(PUBLIC)
	@rm -f $@
	ar rcs $@ $^

$(SHARED): $(PUBLIC)
	$(CC) $(ARCHS) $(SHFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/test_g2am: tests/test_g2am.c $(STATIC)
	$(CC) $(CFLAGS) -o $@ tests/test_g2am.c $(STATIC) $(LDLIBS)

$(BUILD)/play_note: examples/play_note.c $(STATIC)
	$(CC) $(CFLAGS) -o $@ examples/play_note.c $(STATIC) $(LDLIBS)

test: $(BUILD)/test_g2am
	./$(BUILD)/test_g2am

example: $(BUILD)/play_note

clean:
	rm -rf $(BUILD)

.PHONY: all test example clean
