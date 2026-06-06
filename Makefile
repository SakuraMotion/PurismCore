# Purism Core - Makefile
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT

CC ?= cc
AR ?= ar
CFLAGS ?= -O2 -g
CFLAGS += -Wall -Wextra -I./include -I./src
LDFLAGS ?=

# ABI version: v5 or v6 (default)
ABI ?= v6
ifeq ($(ABI),v5)
CFLAGS += -DPSM_COMPAT_VERSION=0x05010000L
ABI_SUFFIX = -v5
else
ABI_SUFFIX =
endif

SRC = src/core.c src/debug.c src/arena.c src/math2.c \
      src/moc3.c src/model.c src/update.c src/param.c \
      src/part.c src/deformer.c src/artmesh.c src/glue.c \
      src/offscreen.c src/blendshape.c src/interpolate.c \
      src/render.c
OBJ = $(SRC:src/%.c=build/%.o)

HDR = include/PurismCore.h $(wildcard src/*.h)
COMMON_H = src/samples/common.h

LIB_A = build/libPurismCore$(ABI_SUFFIX).a

OS ?=
ifeq ($(OS),windows)
WINDRES ?= windres
CFLAGS += -DPURISM_CORE_DLL
LIB_SHARED = build/PurismCore$(ABI_SUFFIX).dll
RC_OBJ = build/version.o
else ifneq ($(filter macos darwin,$(OS)),)
LIB_SHARED = build/libPurismCore$(ABI_SUFFIX).dylib
RC_OBJ =
else
LIB_SHARED = build/libPurismCore$(ABI_SUFFIX).so
RC_OBJ =
endif

all: $(LIB_A) $(LIB_SHARED)

$(LIB_A): $(OBJ) | build
	$(AR) rcs $@ $^

$(LIB_SHARED): $(OBJ) $(RC_OBJ) | build
	$(CC) -shared -o $@ $^ $(LDFLAGS) -lm

build/version.rc: src/version.rc.in include/PurismCore.h | build
	./scripts/gen-version-rc.sh > $@

build/version.o: build/version.rc | build
	$(WINDRES) $< -o $@

build/%.o: src/%.c $(HDR) | build
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

stageplay: build/stageplay
moc3info: build/moc3info

build/stageplay: src/tests/stageplay.c \
    src/tests/partcl.h $(COMMON_H) $(LIB_A) | build
	$(CC) $(CFLAGS) $< -o $@ $(LIB_A) -lm

build/moc3info: src/samples/moc3info.c \
    $(COMMON_H) $(LIB_A) | build
	$(CC) $(CFLAGS) $< -o $@ $(LIB_A) -lm

unit: build/unit
	./build/unit

build/unit: src/tests/unit.c $(SRC) $(HDR) $(COMMON_H) | build
	$(CC) $(CFLAGS) -Wno-unused-function $< -o $@ -lm

test: build/stageplay
	@./scripts/run-tests.sh

quick-test: build/stageplay
	@./scripts/run-tests.sh -q

FUZZ_CC ?= clang
FUZZ_CFLAGS ?= -g -O1 -fsanitize=fuzzer,address,undefined \
	-fno-sanitize-recover=undefined

fuzzer: build/fuzzer

build/fuzzer: src/tests/fuzzer.c $(COMMON_H) $(LIB_A) | build
	$(FUZZ_CC) $(FUZZ_CFLAGS) -I./include \
		$< -o $@ $(LIB_A) -lm

fuzz: build/fuzzer | build/corpus
	./build/fuzzer build/corpus/

build/corpus: | build
	@mkdir -p build/corpus

bundle: dist/PurismCoreBundle.h

dist/PurismCoreBundle.h: $(HDR) $(SRC) | dist
	./scripts/bundle.sh > $@

PREFIX ?= /usr/local
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig

PC = build/purismcore$(ABI_SUFFIX).pc

install: $(LIB_A) $(LIB_SHARED) $(PC)
	install -d $(DESTDIR)$(LIBDIR)
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -m 644 $(LIB_A) $(DESTDIR)$(LIBDIR)/
	install -m 755 $(LIB_SHARED) $(DESTDIR)$(LIBDIR)/
	install -m 644 include/PurismCore.h \
		$(DESTDIR)$(INCLUDEDIR)/
	install -m 644 $(PC) $(DESTDIR)$(PKGCONFIGDIR)/

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(notdir $(LIB_A))
	rm -f $(DESTDIR)$(LIBDIR)/$(notdir $(LIB_SHARED))
	rm -f $(DESTDIR)$(INCLUDEDIR)/PurismCore.h
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(notdir $(PC))

.PHONY: $(PC)
$(PC): scripts/purismcore.pc.in | build
	@sed -e 's|@PREFIX@|$(PREFIX)|' \
	     -e 's|@ABI@|$(ABI)|' \
	     -e 's|@ABI_SUFFIX@|$(ABI_SUFFIX)|' \
	     $< > $@

build:
	@mkdir -p build

dist:
	@mkdir -p dist

clean:
	rm -rf build/

dist-clean:
	rm -rf dist/

.PHONY: all unit stageplay moc3info fuzzer \
	test quick-test fuzz bundle \
	install uninstall clean dist-clean
