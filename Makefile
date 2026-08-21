# Purism Core: Makefile
#
# Copyright (c) 2026 Sakura Motion Project
# SPDX-License-Identifier: MIT
#
ifeq ($(filter else-if,$(.FEATURES)),)
$(error This build requires GNU Make 3.81 or newer (on *BSD: gmake; or use CMake))
endif

CC      ?= cc
HOSTCC  ?= $(CC)
AR      ?= ar
WINDRES ?= windres
CFLAGS  ?= -O2 -g
CFLAGS  += -Wall -Wextra -I./include -I./src
LDFLAGS ?=

# ABI: v6 (default) or v5. v5 reports the 5.1 compat version and omits the
# v6-only API; the suffix keeps v5/v6 artifacts (and objects) from colliding.
ABI ?= v6
ifeq ($(ABI),v5)
  ABI_CPPFLAGS = -DPSM_COMPAT_VERSION=0x05010000L
  ABI_SUFFIX   = -v5
else
  ABI_CPPFLAGS =
  ABI_SUFFIX   =
endif
CFLAGS += $(ABI_CPPFLAGS)

GIT_HASH ?= $(shell git describe --always --dirty --tags 2>/dev/null || echo unknown)
CFLAGS   += -DPSM_GIT_HASH='"$(GIT_HASH)"'

PSM_TRUE_VER := $(shell sed -n 's/^#define PSM_TRUE_VERSION[[:space:]]*\(0x[0-9A-Fa-f]*\).*/\1/p' include/PurismCore.h)
VERSION      := $(shell printf '%d.%d.%d' $$(($(PSM_TRUE_VER) >> 24 & 255)) $$(($(PSM_TRUE_VER) >> 16 & 255)) $$(($(PSM_TRUE_VER) & 65535)))

SRC = src/core.c src/debug.c src/arena.c src/math2.c src/moc3.c src/verify.c \
      src/model.c src/update.c src/param.c src/part.c src/deformer.c \
      src/artmesh.c src/glue.c src/offscreen.c src/blendshape.c \
      src/interpolate.c src/render.c
HDR      = include/PurismCore.h $(wildcard src/*.h)
COMMON_H = src/samples/common.h

OBJDIR = build/obj$(ABI_SUFFIX)
OBJ    = $(SRC:src/%.c=$(OBJDIR)/%.o)

OS ?=
LIBSUF = .a
EXESUF =
ifeq ($(OS),windows)
  DLLPRE =
  DLLSUF = .dll
  EXESUF = .exe
  CFLAGS += -DPURISM_CORE_DLL
  RC_OBJ = $(OBJDIR)/version.o
else ifneq ($(filter macos darwin,$(OS)),)
  DLLPRE = lib
  DLLSUF = .dylib
else
  DLLPRE = lib
  DLLSUF = .so
endif

LIB_A      = build/libPurismCore$(ABI_SUFFIX)$(LIBSUF)
LIB_SHARED = build/$(DLLPRE)PurismCore$(ABI_SUFFIX)$(DLLSUF)

ifeq ($(OS),wasm)
all: web-lib
else
all: static-lib shared-lib
endif

static-lib: $(LIB_A)
$(LIB_A): $(OBJ) | build
	$(AR) rcs $@ $^

$(OBJDIR)/%.o: src/%.c $(HDR) | $(OBJDIR)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

shared-lib: $(LIB_SHARED)
build/lib%.so: $(OBJ) | build              # ELF: embed the soname
	$(CC) -shared -Wl,-soname,$(@F) -o $@ $(OBJ) $(LDFLAGS) -lm
build/lib%.dylib: $(OBJ) | build           # Mach-O: rpath-relative install name
	$(CC) -dynamiclib -install_name @rpath/$(@F) -o $@ $(OBJ) $(LDFLAGS) -lm
build/%.dll: $(OBJ) $(RC_OBJ) | build      # PE: no lib prefix, + version resource
	$(CC) -shared -o $@ $(OBJ) $(RC_OBJ) $(LDFLAGS) -lm

# Windows version resource (linked only into the .dll).
$(OBJDIR)/version.rc: src/version.rc.in include/PurismCore.h | $(OBJDIR)
	./scripts/gen-version-rc.sh > $@
$(OBJDIR)/version.o: $(OBJDIR)/version.rc | $(OBJDIR)
	$(WINDRES) $< -o $@

moc3info:  build/moc3info$(EXESUF)
benchmark: build/benchmark$(EXESUF)

build/moc3info$(EXESUF): src/samples/moc3info.c $(COMMON_H) $(LIB_A) | build
	$(CC) $(CFLAGS) $< -o $@ $(LIB_A) -lm
build/benchmark$(EXESUF): src/samples/benchmark.c $(COMMON_H) $(LIB_A) | build
	$(CC) $(CFLAGS) -O2 -Wno-unused-function $< -o $@ $(LIB_A) -lm

vpath %.frag src/samples/viewer/shaders
vpath %.fnt  src/samples
vpath %.png  src/samples
VIEWER_SHADERS = $(wildcard src/samples/viewer/shaders/*.frag)
VIEWER_EMBEDS  = $(patsubst src/samples/viewer/shaders/%,build/embed/%.h,$(VIEWER_SHADERS)) \
                 build/embed/cascadia.fnt.h build/embed/cascadia_0.png.h

build/bin2h: scripts/bin2h.c | build
	$(HOSTCC) -O2 -o $@ $<
build/embed/%.h: % build/bin2h | build/embed
	@./build/bin2h $< $(subst .,_,$*) $@

RAYLIB_PREFIX ?= /usr/local
RAYLIB_DIR    ?=
ifeq ($(RAYLIB_DIR),)
  RAYLIB_CFLAGS ?= -I$(RAYLIB_PREFIX)/include
  RAYLIB_LIBS   ?= -L$(RAYLIB_PREFIX)/lib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
else
  RAYLIB_CFLAGS ?= -I$(RAYLIB_DIR)
  RAYLIB_LIBS   ?= -L$(RAYLIB_DIR) -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

VIEWER_SRC = src/samples/viewer/io.c src/samples/viewer/blend.c \
             src/samples/viewer/graphics.c src/samples/viewer/render.c \
             src/samples/viewer/panel.c src/samples/viewer/viewer.c

viewer: build/viewer$(ABI_SUFFIX)$(EXESUF)
build/viewer$(ABI_SUFFIX)$(EXESUF): $(VIEWER_SRC) src/samples/viewer/viewer.h \
    $(VIEWER_EMBEDS) src/samples/vendor/raygui.h $(LIB_A) | build
	$(CC) -O2 -g -std=gnu11 -I./include -I./src -I./src/samples/viewer -Ibuild \
	    $(RAYLIB_CFLAGS) $(ABI_CPPFLAGS) -Wall -Wno-unused-function \
	    $(VIEWER_SRC) -o $@ $(LIB_A) $(RAYLIB_LIBS)

EMCC           ?= emcc
RAYLIB_WEB_DIR ?= third_party/local/raylib/src
RAYLIB_WEB_LIB ?= $(RAYLIB_WEB_DIR)/libraylib.web.a

viewer-web: build/viewer.html
build/viewer.html: $(VIEWER_SRC) src/samples/viewer/viewer.h \
    src/samples/viewer/shell.html $(VIEWER_EMBEDS) \
    src/samples/vendor/raygui.h $(SRC) $(HDR) | build
	$(EMCC) -O2 -DPSM_GIT_HASH='"$(GIT_HASH)"' -std=gnu11 $(ABI_CPPFLAGS) \
	    -I./include -I./src -I./src/samples/viewer -Ibuild -I$(RAYLIB_WEB_DIR) \
	    -Wall -Wno-unused-function \
	    $(SRC) $(VIEWER_SRC) $(RAYLIB_WEB_LIB) -o $@ \
	    -s USE_GLFW=3 -s FULL_ES3=1 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 \
	    -s ALLOW_MEMORY_GROWTH=1 -s FORCE_FILESYSTEM=1 \
	    -s EXPORTED_FUNCTIONS=_main,_ViewerRequestLoad \
	    -s EXPORTED_RUNTIME_METHODS=ccall,FS \
	    --shell-file src/samples/viewer/shell.html

WASM_SRC        = $(SRC) src/core_js.c
WASM_RT_METHODS := ccall,cwrap,addFunction,removeFunction,UTF8ToString
WASM_RT_METHODS := $(WASM_RT_METHODS),HEAP8,HEAPU8,HEAPU16,HEAP32,HEAPU32,HEAPF32

web-lib: build/purismcore$(ABI_SUFFIX).js
build/purismcore$(ABI_SUFFIX).js: $(WASM_SRC) src/core_js.js src/core_js_tail.js \
    scripts/assemble-core-js.sh $(HDR) | build build/wasm
	$(EMCC) $(ABI_CPPFLAGS) -O3 -DPSM_GIT_HASH='"$(GIT_HASH)"' \
	    -I./include -I./src $(WASM_SRC) \
	    -o build/wasm/em-module$(ABI_SUFFIX).js \
	    -sSINGLE_FILE=1 -sMODULARIZE=1 -sEXPORT_NAME=_em_module \
	    -sWASM_ASYNC_COMPILATION=0 -sALLOW_TABLE_GROWTH=1 -sALLOW_MEMORY_GROWTH=1 \
	    -sENVIRONMENT=web,worker,node -sFILESYSTEM=0 --closure 0 \
	    -sEXPORTED_RUNTIME_METHODS=$(WASM_RT_METHODS)
	@./scripts/assemble-core-js.sh \
	    src/core_js.js build/wasm/em-module$(ABI_SUFFIX).js src/core_js_tail.js > $@
	@echo "Wrote $@ ($(ABI))"

wasm:
	$(MAKE) OS=wasm ABI=v6
wasm-v5:
	$(MAKE) OS=wasm ABI=v5
wasm-all: wasm wasm-v5

stageplay: build/stageplay
build/stageplay: src/tests/stageplay.c src/tests/partcl.h $(COMMON_H) $(LIB_A) | build
	$(CC) $(CFLAGS) $< -o $@ $(LIB_A) -lm

test: build/stageplay
	@./scripts/run-tests.sh
quick-test: build/stageplay
	@./scripts/run-tests.sh -q

unit: build/unit
	./build/unit
build/unit: src/tests/unit.c $(SRC) $(HDR) $(COMMON_H) | build
	$(CC) $(CFLAGS) -Wno-unused-function $< -o $@ -lm

endian-test: build/endian-test
	./build/endian-test $(shell find "$(TEST_DATA)" -iname '*.moc3')
build/endian-test: src/tests/test_endian.c $(SRC) $(HDR) $(COMMON_H) | build
	$(CC) $(CFLAGS) -Wno-unused-function $< -o $@ -lm

verify-negctl: build/negctl-triidx
	./build/negctl-triidx $(shell find "$(TEST_DATA)" -iname '*.moc3')
build/negctl-triidx: src/tests/negctl_triidx.c $(SRC) $(HDR) $(COMMON_H) | build
	$(CC) $(CFLAGS) -Wno-unused-function $< -o $@ -lm

FUZZ_CC     ?= clang
FUZZ_CFLAGS ?= -g -O1 -fsanitize=fuzzer,address,undefined \
               -fno-sanitize-recover=undefined
fuzzer: build/fuzzer
build/fuzzer: src/tests/fuzzer.c $(COMMON_H) $(LIB_A) | build
	$(FUZZ_CC) $(FUZZ_CFLAGS) -I./include $< -o $@ $(LIB_A) -lm
fuzz: build/fuzzer | build/corpus
	./build/fuzzer build/corpus/

# Fuzz with one malloc per model field (PSM_DEBUG_MALLOC) so ASan puts redzones
# between adjacent model arrays, catching intra-model overflows the single arena
# buffer would otherwise hide. Rebuilds from clean.
MALLOC_CFLAGS = -g -O1 -I./include -I./src -DPSM_DEBUG_MALLOC \
                -fsanitize=address,undefined,fuzzer-no-link -fno-sanitize-recover=undefined
fuzz-malloc: | build/corpus
	@mkdir -p build/corpus
	$(MAKE) static-lib CC=$(FUZZ_CC) CFLAGS="$(MALLOC_CFLAGS)"
	$(FUZZ_CC) $(FUZZ_CFLAGS) -DPSM_DEBUG_MALLOC -I./include \
		src/tests/fuzzer.c $(LIB_A) -lm -o build/fuzzer
	./build/fuzzer build/corpus/

PREFIX       ?= /usr/local
LIBDIR       ?= $(PREFIX)/lib
INCLUDEDIR   ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
PC            = build/purismcore$(ABI_SUFFIX).pc

install: $(LIB_A) $(LIB_SHARED) $(PC)
	install -d $(DESTDIR)$(LIBDIR) $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(PKGCONFIGDIR)
	install -m 644 $(LIB_A)        $(DESTDIR)$(LIBDIR)/
	install -m 755 $(LIB_SHARED)   $(DESTDIR)$(LIBDIR)/
	install -m 644 include/PurismCore.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 $(PC)           $(DESTDIR)$(PKGCONFIGDIR)/

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/$(notdir $(LIB_A))
	rm -f $(DESTDIR)$(LIBDIR)/$(notdir $(LIB_SHARED))
	rm -f $(DESTDIR)$(INCLUDEDIR)/PurismCore.h
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/$(notdir $(PC))

.PHONY: $(PC)
$(PC): scripts/purismcore.pc.in | build
	@sed -e 's|@PREFIX@|$(PREFIX)|' -e 's|@ABI@|$(ABI)|' \
	     -e 's|@ABI_SUFFIX@|$(ABI_SUFFIX)|' -e 's|@VERSION@|$(VERSION)|' $< > $@

bundle: dist/PurismCoreBundle.h
dist/PurismCoreBundle.h: src/bundle.c.in scripts/bundle.sh $(HDR) $(SRC) | dist
	./scripts/bundle.sh > $@

print-%: ; @echo '$($*)'

CLANG_FORMAT ?= clang-format

# AlignConsecutiveDeclarations isn't always idempotent, so reformat to a fixed
# point (loop until --dry-run is clean) rather than a single pass.
format:
	@files=`git ls-files '*.c' '*.h'`; \
	for i in 1 2 3 4 5 6; do \
	  $(CLANG_FORMAT) -i $$files; \
	  if $(CLANG_FORMAT) --dry-run --Werror $$files >/dev/null 2>&1; then \
	    echo "formatted ($$i pass(es))"; exit 0; fi; \
	done; echo "clang-format did not converge after 6 passes" >&2; exit 1

# Verify-only: fails (non-zero) if any tracked C source isn't formatted.
format-check:
	@files=`git ls-files '*.c' '*.h'`; \
	$(CLANG_FORMAT) --dry-run --Werror $$files && echo "format OK"

# One-time: point git at the committed hooks (runs format-check pre-commit).
hooks:
	git config core.hooksPath .githooks
	@echo "git hooks enabled (.githooks)"

build dist:
	@mkdir -p $@
$(OBJDIR) build/embed build/wasm build/corpus: | build
	@mkdir -p $@

clean:
	rm -rf build/
dist-clean:
	rm -rf dist/

.PHONY: all static-lib shared-lib moc3info benchmark viewer viewer-web \
	wasm wasm-v5 wasm-all validate-wasm \
	stageplay test quick-test unit endian-test verify-negctl \
	fuzzer fuzz fuzz-malloc \
	format format-check hooks \
	bundle install uninstall clean dist-clean
