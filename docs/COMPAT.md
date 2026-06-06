# Compatibility

Purism Core is completely compatible with Cubism Core 5.1 and Cubism Core 6.0,
modulo some bug fixes and extended features. It can be built to expose an ABI
matching either version of Cubism Core.

## ABI Versions

> [!IMPORTANT]
> The v6 ABI is the default build configuration.

| ABI | Cubism SDK Name | Cubism Core Version | Notes |
|:---:|:---------------:|:-------------------:|:------|
| v5  | Cubism 5.1 SDK  | 5.1                 | Introduced with Cubism Editor 5.1; most widely supported. |
| v6  | Cubism 5.3 SDK  | 6.0                 | Introduced with Cubism Editor 5.3 |

In general, the v5 ABI has the widest support among third party applications,
due to the fact that the v6 ABI introduced multiple breaking changes.

Examples of software using the v5 ABI includes:

- VTube Studio
- older versions of Ren'Py
- gd_cubism for Godot

Examples of software using the v6 ABI includes:

- Live2D's first party applications and plugins
- the latest versions of Ren'Py

For greenfield projects, we recommend using the v6 ABI unless you have specific
constraints that prevent you from doing so; however, you may need v5 ABI builds
if you want to integrate Purism Core with an existing application or project.

When building, it is possible to choose the v5 ABI by defining
`PSM_COMPAT_VERSION` to `0x05010000L` or, if using the supplied Makefile,
setting `ABI=v5` (e.g. `make ABI=v5 OS=windows`).

### Differences

The following functions are only available in the v6 ABI.

- `csmGetParameterRepeats` - per-parameter repeat flag
- `csmGetPartOffscreenIndices` — part-to-offscreen mapping
- `csmGetDrawableBlendModes` — extended blend mode enum per drawable
- `csmGetRenderOrders` — replaces `csmGetDrawableRenderOrders`
- `csmGetOffscreenCount` — number of offscreen surfaces
- `csmGetOffscreenBlendModes` — blend mode per offscreen
- `csmGetOffscreenOpacities` — opacity per offscreen
- `csmGetOffscreenOwnerIndices` — owner part per offscreen
- `csmGetOffscreenMultiplyColors` — multiply color per offscreen
- `csmGetOffscreenScreenColors` — screen color per offscreen
- `csmGetOffscreenMaskCounts` — mask count per offscreen
- `csmGetOffscreenMasks` — mask drawable indices per offscreen
- `csmGetOffscreenConstantFlags` — constant flags per offscreen

#### Blend Modes

In v5, blend modes are encoded in drawable constant flags (`csmBlendAdditive`,
`csmBlendMultiplicative`) and there is no separate `csmGetDrawableBlendModes`
function.

In v6, blend modes use the `csmColorBlendType` enum with extended values:

| Value | v6 name                                   | v5 equivalent            |
|-------|-------------------------------------------|--------------------------|
| 0     | `csmColorBlendType_Normal`                | no flag set              |
| 1     | `csmColorBlendType_AddCompatible`         | `csmBlendAdditive`       |
| 2     | `csmColorBlendType_MultiplyCompatible`    | `csmBlendMultiplicative` |
| 3-17  | `csmColorBlendType_Add` to `csmColorBlendType_Color` | not available |

In v6, there are also new alpha blend modes accessible through the same
function.

### MOC3 Compatibility and v5 ABI

Regardless of which ABI is chosen, Purism Core still supports the latest MOC3
files exported from Cubism Editor 5.3. **If those files use new features of
Cubism 5.3, they may render incorrectly,** since the v5 ABI does not expose
functionality to integrate those new features (offscreen rendering, additional
blend modes, etc.) and existing host applications would not use those functions
even if they were exposed.

## General Notes

If you notice minor divergences in output between Purism Core and Cubism Core,
there is a good chance that it is not a bug. For extremely small differences,
they are likely the result of [floating point arithmetic quirks](https://0.30000000000000004.com).
In other cases, they may be the result of a bug in Cubism Core which is not
present in Purism Core. Please make sure the output is obviously incorrect
before filing a bug report.
