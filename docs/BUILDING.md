# Building Purism Core

Purism Core is written in standard C99, but it also compiles properly with a
C++ compiler.

There are two main ways to build Purism Core:

- **Make** for development, tests, the WASM libraries, and the single-file
  bundle. Requires a Unix-like environment with GCC or Clang, GNU make, and
  standard POSIX utilities; MSYS, Cygwin, or busybox-w32 *may* work on
  Windows, but we haven't tested.
- **CMake** for IDE integration, `find_package` consumers, install/packaging,
  and all cross-compilation (via `zig cc` and the bundled configure presets).

If you do not intend to modify the library itself, the easiest method is to use
the single-file bundle, `PurismCoreBundle.h`. It is included with the standard
binary distribution, but you can also prepare it using `make bundle`. No C
compiler is required at the time of bundle generation.

> [!TIP]
> Most developers will want to use the prebuilt libraries or the single-file
> bundle whose use is described below.

Except for the unit test binary, Purism Core should build without any warnings
(`-Wall -Wextra`).

## Using Make

### Supported Target Reference

| Target | Description |
|--------|-------------|
| `make` | Build static and shared libraries (default) |
| `make unit` | Build and run unit tests |
| `make stageplay` | Build the Tcl-based test runner |
| `make moc3info` | Build the MOC3 info tool |
| `make viewer` | Build the raylib model viewer |
| `make viewer-web` | Build the web viewer (Emscripten; dist/viewer.*) |
| `make test` | Run integration tests (requires MOC3 test data) |
| `make quick-test` | Run tests with fewer scenarios |
| `make fuzzer` | Build the libFuzzer harness |
| `make fuzz` | Run the fuzzer |
| `make bundle` | Generate single-file header (dist/PurismCoreBundle.h) |
| `make wasm-all` | Build both WASM drop-ins (see "Web (WASM)" below) |
| `make install` | Install library, header, and pkg-config file |
| `make uninstall` | Remove installed files |
| `make clean` | Remove build directory |
| `make dist-clean` | Remove dist directory |

### Supported Variable Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `CC` | `cc` | C compiler |
| `HOSTCC` | `$(CC)` | Host C compiler for build-time codegen tools (set when `CC` is a cross compiler) |
| `AR` | `ar` | Archiver |
| `CFLAGS` | `-O2 -g` | Compiler flags (appended to) |
| `LDFLAGS` | (empty) | Linker flags |
| `ABI` | `v6` | Target ABI: `v5` or `v6` |
| `OS` | (empty) | `windows` for DLL, `macos`/`darwin` for dylib, `wasm` for the web drop-in |
| `RAYLIB_DIR` | (empty) | Link the viewer against an extracted raylib tree instead of the system install |
| `PREFIX` | `/usr/local` | Install prefix |
| `DESTDIR` | (empty) | Staging directory for packaging |

#### ABI Selection

```sh
make        # v6 ABI (default)
make ABI=v5 # v5 ABI (Cubism Core 5.x compatible)
```

The ABI controls which public API functions are available and the suffixes of
the built libraries and pkg-config metadata. For more information, see
COMPAT.md.

#### Windows DLL

```sh
make OS=windows        # builds PurismCore.dll (v6)
make OS=windows ABI=v5 # builds PurismCore-v5.dll
```

On Windows, `__declspec(dllexport)` visibility and `__stdcall` calling
convention are used for compatibility reasons.

If `windres` is available and `src/version.rc.in` exists, a version resource is
compiled and linked into the DLL.

## Using CMake

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build   # optional; respects CMAKE_INSTALL_PREFIX
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `PURISM_CORE_ABI` | `v6` | Target ABI: `v5` or `v6` |
| `BUILD_SHARED_LIBS` | `OFF` | Build the shared library instead of the static one |
| `PURISM_CORE_BUILD_SAMPLES` | `OFF` | Build the moc3info/benchmark tools |
| `PURISM_CORE_BUILD_VIEWER` | `OFF` | Build the raylib viewer (see below) |
| `PURISM_CORE_BUILD_TESTS` | `OFF` | Build the unit/conformance tests |
| `PURISM_CORE_VERSION_RC` | `ON` | Embed a version resource in Windows DLLs |

With `PURISM_CORE_BUILD_TESTS=ON`, run the suite via ctest (`unit` and
`conformance`; the latter needs MOC3 test data, like `make test`):

```sh
ctest --test-dir build
```

### Consumers

Installation provides the header, a pkg-config file, and a CMake package
config, so a downstream project can simply:

```cmake
find_package(PurismCore REQUIRED)
target_link_libraries(my_app PRIVATE PurismCore::PurismCore)
```

## Cross-Compiling

Cross-compilation is handled by CMake configure presets using `zig cc` via
`cmake/zig-toolchain.cmake`:

```sh
cmake --preset zig-cross-linux-x86_64
cmake --build build/_cmake/zig-cross-linux-x86_64
```

| Preset | Target |
|--------|--------|
| `zig-cross-linux-x86_64` | Linux x86_64 (ELF, gnu) |
| `zig-cross-linux-arm64` | Linux arm64 (ELF, gnu) |
| `zig-cross-macos-x86_64` | macOS x86_64 (Mach-O) |
| `zig-cross-macos-arm64` | macOS arm64 (Mach-O) |
| `zig-cross-windows-x86_64` | Windows x86_64 (PE, mingw) |
| `zig-cross-windows-x86` | Windows x86 (PE, mingw) |
| `zig-cross-windows-arm64` | Windows arm64 (PE, mingw) |

Add `-DPURISM_CORE_ABI=v5` for a v5 build, and `-DBUILD_SHARED_LIBS=ON` for a
shared library (the default is static).

### Distribution

The full SDK distribution is built using `scripts/build-dist.sh`, which drives
the presets above. Static and shared libraries for supported platforms,
headers, the single-file bundle, the model viewer, and documentation are all
output to `dist/sdk/` (see `docs/SDKINFO.txt` for the layout).

```sh
./scripts/build-dist.sh                   # full matrix
./scripts/build-dist.sh linux             # one OS family (linux/macos/windows)
./scripts/build-dist.sh zig-cross-linux-x86_64  # a single target
```

Windows ships both v6 and v5 ABIs while Linux/macOS ship v6 only.

To build the viewers, set the appropriate environment variables to unpacked
[raylib release archives](https://github.com/raysan5/raylib/releases/tag/6.0):
`RAYLIB_DIR_LINUX_AMD64`, `RAYLIB_DIR_LINUX_ARM64`, `RAYLIB_DIR_MACOS`,
`RAYLIB_DIR_WINDOWS_AMD64`, and/or `RAYLIB_DIR_WINDOWS_X86`.

On non-macOS platforms, you need to install `https://github.com/konoui/lipo`
to produce the universal macOS binaries!

## Viewer

The raylib model viewer (`src/samples/viewer/`) builds three ways:

- **Native, Make:** `make viewer` against the system raylib, or
  `make viewer RAYLIB_DIR=/path/to/raylib` for an extracted/source tree.
- **Native, CMake:** `-DPURISM_CORE_BUILD_VIEWER=ON` (uses
  `find_package(raylib)`, falling back to fetching the raylib 6.0 source).
- **Cross / dist:** via `scripts/build-dist.sh` with the `RAYLIB_DIR_*`
  variables described above; binaries land in `dist/sdk/bin/`.
- **Web:** `make viewer-web` (Emscripten; needs a WebGL2/GLES3 raylib build).
  The shell page accepts `?model=<zip-url>`.

> [!IMPORTANT]
> The model viewer requires WebGL 2, and the prebuilt Raylib 6.0 for WASM
> only supports WebGL 1. You'll need to build specifically for WebGL 2/GLES3:
>
> `cd raylib && make PLATFORM=PLATFORM_WEB GRAPHICS=GRAPHICS_API_OPENGL_ES3 RAYLIB_BUILD_MODE=RELEASE`

## Web (Emscripten/WASM)

The Emscripten build is the most compatible way of using Purism Core in
existing web-based projects:

```sh
make wasm-all # build/purismcore.js (v6) + build/purismcore-v5.js
```

## Single-file bundle

The single-file bundle is the recommended way of integrating Purism Core into
new projects.

If you would like to generate it yourself:

```sh
make bundle # generates dist/PurismCoreBundle.h
```

### Usage

```C
#include "PurismCoreBundle.h" /* instead of "PurismCore.h" */

/* In exactly ONE .c file: */
#define PURISM_CORE_IMPLEMENTATION
#include "PurismCoreBundle.h"
```

For the v5 ABI, define before inclusion:

```C
#define PSM_COMPAT_VERSION 0x05010000L
#define PURISM_CORE_IMPLEMENTATION
#include "PurismCoreBundle.h"
```

You'll also need to `#define PSM_COMPAT_VERSION 0x05010000L` whenever you
include the header for function and constant definitions.

You can also use the bundle to build your own shared or static libraries
if you desire.

## Testing

### Unit and Integration Tests

Unit tests are included and can be run using the Makefile:

```sh
make unit
```

Integration tests require sample MOC3 files (most of which are not provided).
They use our `stageplay` test runner.

```sh
make test # Use included test data
TEST_DATA=/path/to/moc3/files make test # Custom test data path
```

There are 24 test scenarios. For more information, read `scripts/run-tests.sh`
and `scripts/scenarios/*.tcl`.

### Fuzzing

Fuzzing requires clang and depends on libFuzzer; ASAN and UBSAN are enabled.

```sh
make fuzz
```

Seed the corpus with real MOC3 files for best results:

```sh
cp /path/to/moc3/files/* build/corpus/
make fuzz
```
