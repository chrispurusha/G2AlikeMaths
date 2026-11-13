# G2AlikeMaths - builds build/libg2alikemaths.a and .dylib/.so, the example and the tests.
#   make            the libraries
#   make test       the test harness (tests/), offline: renders and checks what it hears
#   make example    examples/play_note

CC      ?= cc
UNAME   := $(shell uname -s)
ENGINE_VERSION := $(shell head -1 engine/VERSION 2>/dev/null || echo unknown)

CFLAGS  ?= -O2
CFLAGS  += -std=gnu11 -fPIC -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers
CFLAGS  += -DSYNTHLIB_PLUGIN_BUILD -DG2AM_ENGINE_VERSION='"$(ENGINE_VERSION)"'
CFLAGS  += -Iinclude -Iengine -Iengine/synthlib
LDLIBS  := -lm -lpthread

ifeq ($(UNAME),Darwin)
    CFLAGS  += -mmacosx-version-min=12.0
    SHARED  := build/libg2alikemaths.dylib
    SHFLAGS := -dynamiclib -install_name @rpath/libg2alikemaths.dylib
else
    SHARED  := build/libg2alikemaths.so
    SHFLAGS := -shared
endif

ENGINE_SRC := $(wildcard engine/*.c)
LIB_SRC    := $(wildcard src/*.c)
OBJ        := $(patsubst %.c,build/obj/%.o,$(ENGINE_SRC) $(LIB_SRC))
STATIC     := build/libg2alikemaths.a

all: $(STATIC) $(SHARED)

build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

build/obj/engine/soundEngine.o: additions/soundEngineAdditions.c additions/soundEngineAdditions.h

$(STATIC): $(OBJ)
	@rm -f $@
	ar rcs $@ $^

$(SHARED): $(OBJ)
	$(CC) $(SHFLAGS) -o $@ $^ $(LDLIBS)

build/test_g2am: tests/test_g2am.c $(STATIC)
	$(CC) $(CFLAGS) -o $@ tests/test_g2am.c $(STATIC) $(LDLIBS)

build/play_note: examples/play_note.c $(STATIC)
	$(CC) $(CFLAGS) -o $@ examples/play_note.c $(STATIC) $(LDLIBS)

test: build/test_g2am
	./build/test_g2am

example: build/play_note

clean:
	rm -rf build

.PHONY: all test example clean
