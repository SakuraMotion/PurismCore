# Building Purism Core

Purism Core is written in standard C99, but it also compiles properly with a
C++ compiler.

If you intend to work on the library itself or simply want maximum flexibility,
the primary way to build is using the supplied Makefile. This requires a Unix-
like environment with GCC or Clang, GNU make, and standard POSIX utilities;
MSYS, Cygwin, or busybox-w32 *may* work on Windows.

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
| `make test` | Run integration tests (requires MOC3 test data) |
| `make quick-test` | Run tests with fewer scenarios |
| `make fuzzer` | Build the libFuzzer harness |
| `make fuzz` | Run the fuzzer |
| `make bundle` | Generate single-file header (dist/PurismCoreBundle.h) |
| `make install` | Install library, header, and pkg-config file |
| `make uninstall` | Remove installed files |
| `make clean` | Remove build directory |
| `make dist-clean` | Remove dist directory |

### Supported Variable Reference

| Variable | Default | Description |
|----------|---------|-------------|
| `CC` | `cc` | C compiler |
| `AR` | `ar` | Archiver |
| `CFLAGS` | `-O2 -g` | Compiler flags (appended to) |
| `LDFLAGS` | (empty) | Linker flags |
| `ABI` | `v6` | Target ABI: `v5` or `v6` |
| `OS` | (empty) | Set to `windows` for DLL build or `macos`/`darwin` for dylib build |
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

On Windows, `__declspec(dllexport)` visbility and `__stdcall` calling
convention are used for compatibility reasons.

If `windres` is available and `src/version.rc.in` exists, a version resource is
compiled and linked into the DLL.

## Cross-Compiling

We rely on `zig` for easy cross-compiling to our supported targets.

Read `scripts/zig-build.sh` for more information.

On non-macOS platforms, you need to install `https://github.com/konoui/lipo` to
cross-build the macOS dylib!

### Distribution

The full SDK distribution is built using `scripts/build-dist.sh`.
Static and shared libraries for supported platforms, headers, the single-file
bundle, and documentation are all output to `dist/sdk/`.

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
