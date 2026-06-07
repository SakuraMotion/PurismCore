/*
 * Purism Core: public API
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#ifndef PURISM_CORE_H
#define PURISM_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* PSM_COMPAT_VERSION is the version reported by csmGetVersion for
   compatibility and determines what public API functions are available. */
#ifndef PSM_COMPAT_VERSION
#define PSM_COMPAT_VERSION 0x06000001L
#endif

#if PSM_COMPAT_VERSION != 0x06000001L && PSM_COMPAT_VERSION != 0x05010000L
# error Unsupported Cubism compatibility level
#endif

/* PSM_TRUE_VERSION is the actual Purism Core implementation version. */
#define PSM_TRUE_VERSION 0x01000001L

/* CSM_CORE_WIN32_DLL is an alias for PURISM_CORE_DLL. */
#ifdef CSM_CORE_WIN32_DLL
# define PURISM_CORE_DLL
#endif

/* PSMDEF specifies the linkage and attributes of public API functions. */
#ifndef PSMDEF
#  if defined(PURISM_CORE_STATIC)
#    define PSMDEF static
#  elif defined(_WIN32) && defined(PURISM_CORE_DLL)
#    define PSMDEF __declspec(dllexport) __stdcall
#  else
#    define PSMDEF
#  endif
#endif

/* PSM_HAS_STDINT determines whether C99 <stdint.h> is available. */
#ifdef PSM_HAS_STDINT
# if PSM_HAS_STDINT
#   if defined(PSM_STDINT_HEADER)
#     include PSM_STDINT_HEADER
#   else
#     include <stdint.h>
#   endif
# endif
#elif defined(__has_include)
# if __has_include(<stdint.h>)
#   include <stdint.h>
#   define PSM_HAS_STDINT 1
# endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# include <stdint.h>
# define PSM_HAS_STDINT 1
#elif defined(_MSC_VER) && _MSC_VER >= 1600
# include <stdint.h>
# define PSM_HAS_STDINT 1
#elif defined(__cplusplus) && __cplusplus >= 201103L
# include <cstdint>
# define PSM_HAS_STDINT 1
#endif

#ifndef PSM_HAS_STDINT
# define PSM_HAS_STDINT 0
#endif

#if defined(PSM_HAS_STDINT) && PSM_HAS_STDINT
typedef int8_t   psm__int8;
typedef uint8_t  psm__u8;
typedef int16_t  psm__i16;
typedef uint16_t psm__u16;
typedef int32_t  psm__i32;
typedef uint32_t psm__u32;
#else
typedef signed char    psm__int8;
typedef unsigned char  psm__u8;
typedef signed short   psm__i16;
typedef unsigned short psm__u16;
typedef signed int     psm__i32;
typedef unsigned int   psm__u32;
#endif

typedef float psm__f32;
typedef psm__u32 psm_size;

#ifndef psm__static_assert
# if defined(PSM_HAS_STATIC_ASSERT) && PSM_HAS_STATIC_ASSERT
#   define psm__static_assert(cond, msg) _Static_assert(cond, msg)
# elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#   define psm__static_assert(cond, msg) _Static_assert(cond, msg)
# elif defined(__cplusplus) && __cplusplus >= 201103L
#   define psm__static_assert(cond, msg) static_assert(cond, msg)
# else
#   ifndef PSM__JOIN
#     define PSM__JOIN_(a, b) a##b
#     define PSM__JOIN(a, b)  PSM__JOIN_(a, b)
#   endif
#   define psm__static_assert(cond, msg) \
      typedef char PSM__JOIN(psm__static_assertion_, __LINE__)[(cond) ? 1 : -1]
# endif
#endif

/* csmMoc is an opaque handle to a revived MOC3 file. */
typedef struct csmMoc csmMoc;

/* csmModel is an opaque handle to a model instance created from a csmMoc. */
typedef struct csmModel csmModel;

/* csmVersion is a version number. */
typedef psm__u32 csmVersion;

/* csmAlignofMoc is the required alignment for MOC memory (64 bytes).
   csmAlignofModel is the required alignment for model memory (16 bytes). */
enum {
  csmAlignofMoc = 64,
  csmAlignofModel = 16
};

/* csmFlags holds bit flags for drawable properties. */
typedef psm__u8 csmFlags;

/* Constant flags for drawables, obtained via csmGetDrawableConstantFlags.
   csmBlendAdditive and csmBlendMultiplicative are mutually exclusive. */
enum {
  csmBlendAdditive = 1 << 0,       // use additive blending
  csmBlendMultiplicative = 1 << 1, // use multiplicative blending
  csmIsDoubleSided = 1 << 2,       // disable backface culling
  csmIsInvertedMask = 1 << 3       // invert the clipping mask
};

/* Dynamic flags for drawables, obtained via csmGetDrawableDynamicFlags.
   These indicate what changed since the last csmResetDrawableDynamicFlags
   call. */
enum {
  csmIsVisible = 1 << 0,
  csmVisibilityDidChange = 1 << 1,
  csmOpacityDidChange = 1 << 2,
  csmDrawOrderDidChange = 1 << 3,
  csmRenderOrderDidChange = 1 << 4,
  csmVertexPositionsDidChange = 1 << 5,
  csmBlendColorDidChange = 1 << 6
};

#if PSM_COMPAT_VERSION >= 0x06000000L || defined(PSM__BLENDTYPE_V6)
/* Color blend mode values returned by csmGetDrawableBlendModes and
   csmGetOffscreenBlendModes. These define how color channels are combined. */
enum {
  csmColorBlendType_Normal = 0,
  csmColorBlendType_AddCompatible = 1,
  csmColorBlendType_MultiplyCompatible = 2,
  csmColorBlendType_Add = 3,
  csmColorBlendType_AddGlow = 4,
  csmColorBlendType_Darken = 5,
  csmColorBlendType_Multiply = 6,
  csmColorBlendType_ColorBurn = 7,
  csmColorBlendType_LinearBurn = 8,
  csmColorBlendType_Lighten = 9,
  csmColorBlendType_Screen = 10,
  csmColorBlendType_ColorDodge = 11,
  csmColorBlendType_Overlay = 12,
  csmColorBlendType_SoftLight = 13,
  csmColorBlendType_HardLight = 14,
  csmColorBlendType_LinearLight = 15,
  csmColorBlendType_Hue = 16,
  csmColorBlendType_Color = 17
};

/* Alpha blend mode values. These define how alpha channels are combined. */
enum {
  csmAlphaBlendType_Over = 0,
  csmAlphaBlendType_Atop = 1,
  csmAlphaBlendType_Out = 2,
  csmAlphaBlendType_ConjointOver = 3,
  csmAlphaBlendType_DisjointOver = 4
};
#else
/* v5 blend types: Additive and Multiplicative only (no extended blend modes).
   AddCompatible maps to Add, MultiplyCompatible maps to Multiply. */
enum {
  csmColorBlendType_Normal = 0,
  csmColorBlendType_Add = 1,
  csmColorBlendType_Multiply = 2,
  csmColorBlendType_AddCompatible = csmColorBlendType_Add,
  csmColorBlendType_MultiplyCompatible = csmColorBlendType_Multiply
};
#endif

/* csmMocVersion identifies the MOC3 file format version. */
typedef psm__u32 csmMocVersion;

/* MOC3 file format versions returned by csmGetMocVersion. */
enum {
  csmMocVersion_Unknown = 0,
  csmMocVersion_30 = 1,  /* 3.0.00 - 3.2.07 */
  csmMocVersion_33 = 2,  /* 3.3.00 - 3.3.03 */
  csmMocVersion_40 = 3,  /* 4.0.00 - 4.1.05 */
  csmMocVersion_42 = 4,  /* 4.2.00 - 4.2.04 */
  csmMocVersion_50 = 5,  /* 5.0.00 - 5.2.03 */
  csmMocVersion_53 = 6   /* 5.3.00+ */
};

/* csmParameterType distinguishes normal parameters from blend shape
   parameters. */
typedef psm__i32 csmParameterType;

/* Parameter types returned by csmGetParameterTypes. */
enum {
  csmParameterType_Normal = 0,
  csmParameterType_BlendShape = 1
};

/* csmVector2 is a 2D vector with X and Y components. */
typedef struct csmVector2 {
  float X, Y;
} csmVector2;

/* csmVector4 is a 4D vector used for colors (X=R, Y=G, Z=B, W=A). */
typedef struct csmVector4 {
  float X, Y, Z, W;
} csmVector4;

/* csmLogFunction is a callback for receiving log messages from the library. */
typedef void (*csmLogFunction)(const char *message);

/*
 * Version and logging
 */
PSMDEF csmVersion     csmGetVersion(void);
PSMDEF csmVersion     csmGetTrueVersion(void);
PSMDEF csmMocVersion  csmGetLatestMocVersion(void);
PSMDEF csmMocVersion  csmGetMocVersion(const void *, unsigned int);
PSMDEF int            csmHasMocConsistency(void *, unsigned int);
PSMDEF csmLogFunction csmGetLogFunction(void);
PSMDEF void           csmSetLogFunction(csmLogFunction);
PSMDEF int            csmGetLogLevel(void);
PSMDEF void           csmSetLogLevel(int);

/*
 * Lifecycle
 *
 * csmReviveMocInPlace: address must be 64-byte aligned (csmAlignofMoc).
 * Memory is modified in place and must outlive all model instances.
 *
 * csmInitializeModelInPlace: address must be 16-byte aligned
 * (csmAlignofModel). The MOC must outlive all its model instances.
 *
 * csmUpdateModel: recalculates vertices after parameter/opacity changes.
 * Call csmResetDrawableDynamicFlags first to track what changed.
 */
PSMDEF csmMoc      *csmReviveMocInPlace(void *, unsigned int);
PSMDEF unsigned int csmGetSizeofModel(const csmMoc *);
PSMDEF csmModel    *csmInitializeModelInPlace(const csmMoc *,
                        void *, unsigned int);
PSMDEF void         csmUpdateModel(csmModel *);
PSMDEF void         csmReadCanvasInfo(const csmModel *,
                        csmVector2 *, csmVector2 *, float *);

/*
 * Parameters
 *
 * csmGetParameterValues returns a writable array. Modify then call
 * csmUpdateModel. Values are clamped to [min, max] unless repeat is set.
 */
PSMDEF int                      csmGetParameterCount(const csmModel *);
PSMDEF const char             **csmGetParameterIds(const csmModel *);
PSMDEF const csmParameterType  *csmGetParameterTypes(const csmModel *);
PSMDEF const float             *csmGetParameterMinimumValues(const csmModel *);
PSMDEF const float             *csmGetParameterMaximumValues(const csmModel *);
PSMDEF const float             *csmGetParameterDefaultValues(const csmModel *);
PSMDEF float                   *csmGetParameterValues(csmModel *);
PSMDEF const int               *csmGetParameterKeyCounts(const csmModel *);
PSMDEF const float            **csmGetParameterKeyValues(const csmModel *);
#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF const int               *csmGetParameterRepeats(const csmModel *);
#endif

/*
 * Parts
 *
 * csmGetPartOpacities returns a writable array. Values are clamped
 * to [0, 1]. Part opacity affects all drawables in that part.
 * Parent index of -1 means no parent.
 */
PSMDEF int          csmGetPartCount(const csmModel *);
PSMDEF const char **csmGetPartIds(const csmModel *);
PSMDEF float       *csmGetPartOpacities(csmModel *);
PSMDEF const int   *csmGetPartParentPartIndices(const csmModel *);
#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF const int   *csmGetPartOffscreenIndices(const csmModel *);
#endif

/*
 * Drawables (art meshes)
 *
 * Constant flags: csmBlendAdditive, csmBlendMultiplicative,
 * csmIsDoubleSided, csmIsInvertedMask.
 *
 * Dynamic flags: csmIsVisible, csmVisibilityDidChange, etc.
 * Call csmResetDrawableDynamicFlags before csmUpdateModel to track.
 *
 * Vertex pos are in model space, origin at bottom-left.
 * Triangle indices use counter-clockwise winding.
 * Mask index of -1 means the mask drawable is hidden.
 */
PSMDEF int                    csmGetDrawableCount(const csmModel *);
PSMDEF const char           **csmGetDrawableIds(const csmModel *);
PSMDEF const csmFlags        *csmGetDrawableConstantFlags(const csmModel *);
PSMDEF const csmFlags        *csmGetDrawableDynamicFlags(const csmModel *);
PSMDEF const int             *csmGetDrawableTextureIndices(const csmModel *);
PSMDEF const int             *csmGetDrawableDrawOrders(const csmModel *);
PSMDEF const float           *csmGetDrawableOpacities(const csmModel *);
PSMDEF const int             *csmGetDrawableMaskCounts(const csmModel *);
PSMDEF const int            **csmGetDrawableMasks(const csmModel *);
PSMDEF const int             *csmGetDrawableVertexCounts(const csmModel *);
PSMDEF const csmVector2     **csmGetDrawableVertexPositions(const csmModel *);
PSMDEF const csmVector2     **csmGetDrawableVertexUvs(const csmModel *);
PSMDEF const int             *csmGetDrawableIndexCounts(const csmModel *);
PSMDEF const unsigned short **csmGetDrawableIndices(const csmModel *);
PSMDEF const csmVector4      *csmGetDrawableMultiplyColors(const csmModel *);
PSMDEF const csmVector4      *csmGetDrawableScreenColors(const csmModel *);
PSMDEF const int             *csmGetDrawableParentPartIndices(const csmModel *);
PSMDEF void                   csmResetDrawableDynamicFlags(csmModel *);
#if PSM_COMPAT_VERSION >= 0x06000000L
PSMDEF const int             *csmGetDrawableBlendModes(const csmModel *);
PSMDEF const int             *csmGetRenderOrders(const csmModel *);
#else
PSMDEF const int             *csmGetDrawableRenderOrders(const csmModel *);
#endif

#if PSM_COMPAT_VERSION >= 0x06000000L
/*
 * Offscreen surfaces (MOC3 v6+)
 *
 * Render-to-texture surfaces for advanced effects.
 * Owner index maps each offscreen to its parent part.
 */
PSMDEF int                csmGetOffscreenCount(const csmModel *);
PSMDEF const int         *csmGetOffscreenBlendModes(const csmModel *);
PSMDEF const float       *csmGetOffscreenOpacities(const csmModel *);
PSMDEF const int         *csmGetOffscreenOwnerIndices(const csmModel *);
PSMDEF const csmVector4  *csmGetOffscreenMultiplyColors(const csmModel *);
PSMDEF const csmVector4  *csmGetOffscreenScreenColors(const csmModel *);
PSMDEF const int         *csmGetOffscreenMaskCounts(const csmModel *);
PSMDEF const int        **csmGetOffscreenMasks(const csmModel *);
PSMDEF const csmFlags    *csmGetOffscreenConstantFlags(const csmModel *);
#endif

#ifdef __cplusplus
}
#endif

#endif
