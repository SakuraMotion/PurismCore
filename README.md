# Sakura2D Purism Core

Purism Core is a free, open reimplementation of Live2D Cubism Core, the central
component of the Live2D Cubism SDK. For this reason, you can use, modify, and
distribute this library without needing a special license from Live2D Inc.

**Do not contact Live2D Inc. for support as this project is not developed by
them.**

## Supported Platforms

Purism Core is written in plain C99 and depends only on the C standard library.

Please see the table below for official support:

| CPU/OS| Windows | macOS | Linux | iOS | Android | Emscripten  |
|:-----:|:-------:|:-----:|:-----:|:---:|:-------:|:-----------:|
| x86   | ✓       | ?     | ✓     | -   | ?       | -           |
| x86_64| ✓       | ✓     | ✓     | ✓ * | ✓       | -           |
| armv7l| ?       | -     | ✓     | ?   | ✓       | -           |
| arm64 | ✓       | ✓     | ✓     | ✓   | ✓       | -           |
| wasm32| -       | -     | -     | -   | -       | ?           |
| wasm64| -       | -     | -     | -   | -       | ?           |
<sup>\*The iOS simulator is fully supported.</sup>

A `?` indicates that we have not tested on that platform. Purism Core should,
in theory, function on all "sane" platforms. A `-` indicates that the platform
is not supported, usually due to an impossible OS/CPU architecture combination.

We provide prebuilt shared and static libraries for the following platforms:

- Windows x86/x86_64/arm64 (MinGW only)
- Linux x86_64/arm64 (glibc only)
- macOS x86_64/arm64

We also provide a single-file bundle of the entire library in a header file
(`PurismCoreBundle.h`) if you would prefer to avoid both using prebuilt
binaries and building them manually. This is the easiest way to use Purism
Core, and it's the way we recommend.

### Application Compatibility

Purism Core should be compatible with existing frameworks that use Live2D
Cubism Core, such as [Ren'Py](https://www.renpy.org/doc/html/live2d.html).

## Build Instructions

For build instructions, see [BUILDING.md](docs/BUILDING.md).

## Usage Instructions

For API usage instructions, see [API.md](docs/API.md) and
[COMPAT.md](docs/COMPAT.md). Additionally, since Purism Core is compatible with
Live2D Cubism Core, you may refer to the official Live2D Cubism Core
documentation as well.

## Bug Reports

If you encounter a mismatch between the behavior of Purism Core and Live2D
Cubism Core, please open an issue. If the mismatch is due to security
improvements in Purism Core, we may not "fix" the mismatch.

## Common Questions

#### Is Purism Core fully compatible with Live2D Cubism Core?

Purism Core is a drop-in replacement for the Live2D Cubism Core header and
library with complete API and ABI compatibility. There should be no difference
in behavior.

#### Can I use it with [game engine] or [application]?

Yes, you can. If the software is intended for developers, you simply need to
point it at the Purism Core library. If the software bundles its own copy of
Cubism Core, you will need to overwrite `Live2DCubismCore.dll` or similar with
the appropriate Purism Core library. When doing so, remember to match the ABI.
If you're unsure of which ABI the software uses, it is probably v5; see below
for more information.

#### What features does Purism Core support?

Purism Core currently supports all the features from Live2D Cubism Core.
It can be built to be compatible with the Cubism Core 5 ABI (as used by VTube
Studio and other applications) or the Cubism Core 6 ABI (default). For more
information, see [COMPAT.md](docs/COMPAT.md).

MOC3 files exported for Cubism 5.3 SDK can be loaded regardless of ABI
version, although loading such models will likely result in bugs or glitches
if the host application has not been updated.

#### How are you so sure about that?

We have an internal test suite with more than 10,000 test cases to verify
correctness.

#### Is Purism Core secure?

While this *is* a C codebase, we are reasonably sure of its security. We have
performed hundreds of hours of fuzz testing (with ASAN and UBSAN enabled) and
implemented proper bounds checking. That said, we cannot guarantee absolute
memory safety.

Purism Core may *not* be secure depending on the build options you select.

#### Then what about those ugly 500 line functions?

Given the constraints of the Cubism Core ABI, that was the least terrible way
of implementing model initialization. We have explored other options. Trust me.

Pull requests *are* welcome.

#### Why didn't you rewrite it in Rust then?

We tried that. It was uglier. As previously said, the ABI is awkward.

#### Then why not change the ABI?

The goal is for Purism Core to be compatible with all software that already
works with Cubism Core. A Rust implementation with a more ergonomic API is
forthcoming.

#### Then C++ for now?

No, but Purism Core does build with a C++ compiler.

#### Do I need a license from Live2D Inc. to use this?

No. Because Purism Core is a reimplementation of Live2D Cubism Core without any
Live2D code, you can use it freely without an "SDK Release License" or other
type of license from Live2D Inc.

#### Do I need a license from you to use this?

Purism Core is free for both commercial and non-commercial projects. You do not
need to request anything from us or notify us; however, we would appreciate if
you did the latter so we can gauge the popularity of this project.

#### Will Live2D Inc. come after me for using this?

We currently do not think so. If you have any problems with Live2D Inc., please
contact us by email at <sakura2d@pm.me>.

## License

Copyright © 2025, 2026 Sakura Motion Project. <https://sakura2d.org/>

Use of Purism Core is subject to the terms of the [MIT License](LICENSE).
Purism Core is provided "AS IS" without any express or implied warranty.

"Sakura Motion", "Sakura2D", and "Purism Core" are trademarks of Sakura Motion
Project. "Live2D" and "Cubism" are trademarks of Live2D Inc. and all references
thereto are purely nominative.
