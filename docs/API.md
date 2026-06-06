# Purism Core API Reference

Purism Core is a C library for loading and evaluating Live2D Cubism models.
It implements the Cubism Core Native SDK API.

> [!NOTE]
> This reference primarily covers the Cubism Core 6.0 API (v6 ABI), but we'll
> mark the differences from the 5.1 API for your convenience. **The v6 ABI is
> the default.**

## Quick Example

```C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "PurismCore.h" /* or #include "PurismCoreBundle.h" for the bundle */

/* Aligned allocation for MOC3 and model data */
static void *
my_aligned_alloc(size_t align, size_t n)
{
  void *p = malloc(n + align + sizeof(void *));
  if (!p) return NULL;

  void *a = (void *)((((size_t)p + sizeof(p)) + align - 1) & ~(align - 1));
  ((void **)a)[-1] = p;
  return a;
}

static void
my_aligned_free(void *p)
{
  if (p) free(((void **)p)[-1]);
}

int
main(void)
{
  /* 1. Load MOC3 file into aligned buffer */
  FILE *f = fopen("model.moc3", "rb");
  fseek(f, 0, SEEK_END);
  size_t moc_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  void *moc_buf = my_aligned_alloc(csmAlignofMoc, moc_size);
  fread(moc_buf, 1, moc_size, f);
  fclose(f);

  /* 2. Validate and prepare the MOC3 data */
  if (!csmHasMocConsistency(moc_buf, moc_size)) {
    fprintf(stderr, "invalid MOC3 file\n");
    return 1;
  }
  csmMoc *moc = csmReviveMocInPlace(moc_buf, moc_size);

  /* 3. Allocate and initialize the model */
  unsigned int model_size = csmGetSizeofModel(moc);
  void *model_buf = my_aligned_alloc(csmAlignofModel, model_size);
  model = csmInitializeModelInPlace(moc, model_buf, model_size);

  /* 4. Set parameter values */
  int param_count = csmGetParameterCount(model);
  float *param_values = csmGetParameterValues(model);
  const char **param_ids = csmGetParameterIds(model);

  for (int i = 0; i < param_count; i++) {
    if (strcmp(param_ids[i], "ParamAngleX") == 0)
      param_values[i] = 15.0f;
  }

  /* 5. Update the model (runs the full pipeline) */
  csmUpdateModel(model);

  /* 6. Read drawable output for rendering */
  int drawable_count = csmGetDrawableCount(model);
  const int *render_orders = csmGetRenderOrders(model);
  const float *opacities = csmGetDrawableOpacities(model);
  const csmVector2 **positions = csmGetDrawableVertexPositions(model);

  for (int i = 0; i < drawable_count; i++)
    printf("drawable %d: opacity=%.2f order=%d\n", i, opacities[i],
        render_orders[i]);

  /* 7. Free buffers (no library free function) */
  my_aligned_free(model_buf);
  my_aligned_free(moc_buf);
  return 0;
}
```

## Lifecycle

### Ownership Semantics

The caller owns all memory. Purism Core does not perform any heap allocation.
At minimum, two buffers are needed:

- The **MOC3 buffer** which holds the raw file data. It must be aligned to
  `csmAlignofMoc` bytes. `csmReviveMocInPlace` mutates this buffer. It must
  remain valid for as long as any model created from it exists.

- The **model buffer** which holds the model runtime state. It must be aligned
  to `csmAlignofModel` bytes and be at least `csmGetSizeofModel` bytes. The
  model is created in-place.

> [!CAUTION]
> Model operations are not thread safe. A single model must not be accessed
> from multiple threads simultaneously.

### Initialization

```C
csmMoc *csmReviveMocInPlace(moc_buf, size);
unsigned int csmGetSizeofModel(moc);
csmModel *csmInitializeModelInPlace(moc, buf, size);
```

- `csmReviveMocInPlace` validates and prepares the MOC3 data. It modifies the
  buffer in place. Returns NULL on failure.

- `csmGetSizeofModel` returns the number of bytes needed for the model buffer.
  The value may differ between library versions.

- `csmInitializeModelInPlace` creates the model and runs one initial update.
  Returns NULL on failure. After this call, the model is ready for parameter
  manipulation and rendering.

### Updates

```C
float *csmGetParameterValues(model); /* mutable array */
float *csmGetPartOpacities(model);   /* mutable array */
void csmUpdateModel(model);
```

Updating the state of the model requires a few steps:

1. Write parameter values (obtained from `csmGetParameterValues`).
2. Optionally write part opacities (from `csmGetPartOpacities`).
3. Call `csmUpdateModel` to evaluate the pipeline.
4. Read drawable state for rendering.

## Validation

```C
csmMocVersion csmGetMocVersion(data, size);
int csmHasMocConsistency(data, size); /* returns 0 or 1 */
```

- `csmGetMocVersion` returns the MOC3 format version without modifying the
  buffer. Returns `csmMocVersion_Unknown` for invalid data.

- `csmHasMocConsistency` performs structural validation: checks the header,
  section offsets, array bounds, and cross-reference integrity. Returns 1 if
  the file is *likely* safe to load.

> [!WARNING]
> Always call `csmHasMocConsistency` before `csmReviveMocInPlace` when loading
> untrusted data.

## Parameters

```C
int csmGetParameterCount(model);
const char **csmGetParameterIds(model);
const csmParameterType *csmGetParameterTypes(model);
const float *csmGetParameterMinimumValues(model);
const float *csmGetParameterMaximumValues(model);
const float *csmGetParameterDefaultValues(model);
float *csmGetParameterValues(model);
const int *csmGetParameterKeyCounts(model);
const float **csmGetParameterKeyValues(model);
const int *csmGetParameterRepeats(model);
```

All arrays are indexed by parameter index (0 to count-1).

**Types**: `csmParameterType_Normal` (driven by application) or
`csmParameterType_BlendShape` (driven by other parameters via the blend shape
system).

**Values**: write to the array returned by `csmGetParameterValues` before
calling `csmUpdateModel`. Values outside [min, max] are clamped. Repeat
parameters wrap instead of clamping.

**Keys**: the breakpoints along each parameter axis. Used internally for
keyform interpolation. Useful for UI (slider snap points) but not required
for basic usage.

## Parts

```C
int csmGetPartCount(model);
const char **csmGetPartIds(model);
float *csmGetPartOpacities(model);
const int *csmGetPartParentPartIndices(model);
const int *csmGetPartOffscreenIndices(model);
```

Parts are visibility groups. Each drawable belongs to a part. Part opacity
multiplies into all drawables in that part.

**Parent index**: -1 for root parts. Child part opacity is multiplied by
parent opacity during the update.

**Opacities**: write to the array returned by `csmGetPartOpacities` before
calling `csmUpdateModel`. Values are clamped to [0, 1].

## Drawables

Drawables are the renderable art meshes. After `csmUpdateModel`,
read their state for rendering.

### Geometry

```C
int csmGetDrawableCount(model);
const char **csmGetDrawableIds(model);
const int *csmGetDrawableVertexCounts(model);
const csmVector2 **csmGetDrawableVertexPositions(model);
const csmVector2 **csmGetDrawableVertexUvs(model);
const int *csmGetDrawableIndexCounts(model);
const unsigned short **csmGetDrawableIndices(model);
```

Each drawable has a vertex array (positions and UVs) and a triangle index
array. Positions are updated every frame by the pipeline.

### Appearance

```C
const float *csmGetDrawableOpacities(model);
const csmVector4 *csmGetDrawableMultiplyColors(model);
const csmVector4 *csmGetDrawableScreenColors(model);
const int *csmGetDrawableTextureIndices(model);
```

> [!TIP]
> The multiply and screen colors are in RGBA format and should be applied with
> the formula `final = texture * multiply_color + screen_color`.

> [!IMPORTANT]
> The texture index is the ID of the texture atlas used by the drawable. Purism
> Core does not load textures by itself; this is the renderer's responsibility.

### Draw and Render Order

```C
const int *csmGetDrawableDrawOrders(model);
const int *csmGetRenderOrders(model);
```

The **render order** is the final sorted order after draw group processing.
Render in ascending order (earlier entries in the back, later entries in front).

> [!NOTE]
> In the v5 ABI, `csmGetRenderOrders` is `csmGetDrawableRenderOrders`.

### Flags and Blend Modes

```C
const csmFlags *csmGetDrawableConstantFlags(model);
const csmFlags *csmGetDrawableDynamicFlags(model);
const int *csmGetDrawableBlendModes(model); /* v6 only */
void csmResetDrawableDynamicFlags(model);
```

#### Constant Flags

The **constant flags** never change after model initialization.

| Flag                     | Meaning |
|--------------------------|---------|
| `csmBlendAdditive`       | Use additive blending |
| `csmBlendMultiplicative` | Use multiplicative blending |
| `csmIsDoubleSided`       | Disable backface culling |
| `csmIsInvertedMask`      | Invert the clipping mask |

> [!IMPORTANT]
> In v6, use `csmGetDrawableBlendModes` instead of the flag bits for blend mode.

#### Dynamic Flags

The **dynamic flags** are updated each frame by the pipeline.

| Flag                          | Meaning |
|-------------------------------|---------|
| `csmIsVisible`                | Drawable is visible (enabled, non-zero opacity) |
| `csmVisibilityDidChange`      | Visibility changed since last frame |
| `csmOpacityDidChange`         | Opacity changed |
| `csmDrawOrderDidChange`       | Draw order changed |
| `csmRenderOrderDidChange`     | Render order changed |
| `csmVertexPositionsDidChange` | Vertex positions changed |
| `csmBlendColorDidChange`      | Multiply or screen color changed |

Call `csmResetDrawableDynamicFlags` before `csmUpdateModel` to detect changes.
The reset preserves `csmIsVisible`.

#### Blend Modes

> [!IMPORTANT]
> This section only applies to v6.

`csmGetDrawableBlendModes` returns the blend modes for each drawable. The color
blend is stored in lowest 8 bits, and the alpha blend is stored in the next
lowest 8 bits.

##### Color Blend Modes

- `csmColorBlendType_Normal`
- `csmColorBlendType_AddCompatible` (v5 compatible additive blending)
- `csmColorBlendType_MultiplyCompatible` (v5 compatible multiplicative blending)
- `csmColorBlendType_Add`
- `csmColorBlendType_AddGlow`
- `csmColorBlendType_Darken`
- `csmColorBlendType_Multiply`
- `csmColorBlendType_ColorBurn`
- `csmColorBlendType_LinearBurn`
- `csmColorBlendType_Lighten`
- `csmColorBlendType_Screen`
- `csmColorBlendType_ColorDodge`
- `csmColorBlendType_Overlay`
- `csmColorBlendType_SoftLight`
- `csmColorBlendType_HardLight`
- `csmColorBlendType_LinearLight`
- `csmColorBlendType_Hue`
- `csmColorBlendType_Color`

##### Alpha Blend Modes

- `csmAlphaBlendType_Over`
- `csmAlphaBlendType_Atop`
- `csmAlphaBlendType_Out`
- `csmAlphaBlendType_ConjointOver`
- `csmAlphaBlendType_DisjointOver`

### Masks (Clipping)

```C
const int *csmGetDrawableMaskCounts(model);
const int **csmGetDrawableMasks(model);
const int *csmGetDrawableParentPartIndices(model);
```

The **masks** are an array of drawable indices that clip the main drawable.
Render the mask drawables to a stencil buffer, and then render the main
drawable clipped to that region.

The **parent part index** identifiers which part the drawable belongs to
(or -1 if none).

## Offscreen Surfaces (v6 only)

```C
int csmGetOffscreenCount(model);
const int *csmGetOffscreenBlendModes(model);
const float *csmGetOffscreenOpacities(model);
const int *csmGetOffscreenOwnerIndices(model);
const csmVector4 *csmGetOffscreenMultiplyColors(model);
const csmVector4 *csmGetOffscreenScreenColors(model);
const int *csmGetOffscreenMaskCounts(model);
const int **csmGetOffscreenMasks(model);
const csmFlags *csmGetOffscreenConstantFlags(model);
```

Offscreen surfaces are render-to-texture targets for advanced compositing
effects. Each surface is owned by a part (**owner index**).

> [!NOTE]
> Offscreen surfaces are only present in MOC3 v5.3+ files and only properly
> supported in v6 ABI builds.

## Canvas Metadata

```C
void csmReadCanvasInfo(model, &size, &origin, &ppu);
```

To set up the coordinate system for rendering, read the canvas size, origin,
and pixels-per-unit from the model.

## Version Information

```C
csmVersion csmGetVersion();
csmVersion csmGetTrueVersion();
csmMocVersion csmGetLatestMocVersion();
```

- `csmGetVersion` returns the ABI compatibility version (e.g., 0x05010000 for
  v5, 0x06000001 for v6).

- `csmGetTrueVersion` returns the actual Purism Core implementation version.

- `csmGetLatestMocVersion` returns the highest MOC3 format version the library
  can load (always `csmMocVersion_53`).

## Logging

```C
void csmSetLogFunction(handler)
csmLogFunction csmGetLogFunction();
void csmSetLogLevel(level);
int csmGetLogLevel();
```

- `csmSetLogFunction` is used to set a callback to receive log messages from
  the library; `csmGetLogFunction` returns the previously configured callback.
  Logging is disabled if NULL is passed.

> [!TIP]
> Log levels range from 0 (verbose) to 3 (errors only).

## Appendix

### Additional Configuration

Defining `PSM_FAST_AND_DANGEROUS` at build time disables safety and bounds
checking. This is not recommended.

Define `PSM_COMPAT_VERSION` to the desired ABI compatibility version if you
require a different ABI than the default v6.

### Types

```C
typedef unsigned int   csmVersion;
typedef unsigned int   csmMocVersion;
typedef unsigned char  csmFlags;
typedef int            csmParameterType;

typedef struct { float X, Y; }          csmVector2;
typedef struct { float X, Y, Z, W; }    csmVector4;

typedef void (*csmLogFunction)(const char *message);
```

### Constants

```C
csmAlignofMoc    /* Required alignment for MOC3 buffer (64) */
csmAlignofModel  /* Required alignment for model buffer (16) */
```
