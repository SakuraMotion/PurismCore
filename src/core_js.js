/*
 * Purism Core: Emscripten/WASM JS API wrapper
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */
var PurismCore;
(function (PurismCore) {
  "use strict";

  let _em = null;

  let _v6 = false;
  const _has = (name) =>
    _em && (typeof _em["_" + name] === "function" ||
      (_em.asm && typeof _em.asm[name] === "function"));

  const _csm = {
    getVersion() {
      return _em.ccall("csmGetVersion", "number", [], []);
    },
    getLatestMocVersion() {
      return _em.ccall("csmGetLatestMocVersion", "number", [], []);
    },
    getMocVersion(moc, mocSize) {
      return _em.ccall("csmGetMocVersion", "number",
        ["number", "number"], [moc, mocSize]);
    },
    getTrueVersion() {
      return _em.ccall("csmGetTrueVersion", "number", [], []);
    },
    getExtendedVersionString() {
      return _em.ccall("csmGetExtendedVersionString", "string", [], []);
    },
    getLogFunction() {
      return _em.ccall("csmGetLogFunction", "number", [], []);
    },
    setLogFunction(handler) {
      _em.ccall("csmSetLogFunction", null, ["number"], [handler]);
    },
    getSizeofModel(moc) {
      return _em.ccall("csmGetSizeofModel", "number", ["number"], [moc]);
    },
    reviveMocInPlace(memory, mocSize) {
      return _em.ccall("csmReviveMocInPlace", "number",
        ["number", "number"], [memory, mocSize]);
    },
    initializeModelInPlace(moc, memory, modelSize) {
      return _em.ccall("csmInitializeModelInPlace", "number",
        ["number", "number", "number"], [moc, memory, modelSize]);
    },
    hasMocConsistency(memory, mocSize) {
      return _em.ccall("csmHasMocConsistency", "number",
        ["number", "number"], [memory, mocSize]);
    },
    updateModel(model) {
      _em.ccall("csmUpdateModel", null, ["number"], [model]);
    },
    readCanvasInfo(model, s, o, p) {
      _em.ccall("csmReadCanvasInfo", null,
        ["number", "number", "number", "number"], [model, s, o, p]);
    },
    getMocError(moc) {
      return _em.ccall("csmGetMocError", "number", ["number"], [moc]);
    },
    getLastError(model) {
      return _em.ccall("csmGetLastError", "number", ["number"], [model]);
    },
    getErrorString(error) {
      return _em.ccall("csmGetErrorString", "string", ["number"], [error]);
    },

    getRenderOrders(model) {
      const sym = _v6 ? "csmGetRenderOrders" : "csmGetDrawableRenderOrders";
      return _em.ccall(sym, "number", ["number"], [model]);
    },

    /* Parameters. */
    getParameterCount(model) {
      return _em.ccall("csmGetParameterCount", "number", ["number"], [model]);
    },
    getParameterIds(model) {
      return _em.ccall("csmGetParameterIds", "number", ["number"], [model]);
    },
    getParameterTypes(model) {
      return _em.ccall("csmGetParameterTypes", "number", ["number"], [model]);
    },
    getParameterMinimumValues(model) {
      return _em.ccall("csmGetParameterMinimumValues", "number", ["number"], [model]);
    },
    getParameterMaximumValues(model) {
      return _em.ccall("csmGetParameterMaximumValues", "number", ["number"], [model]);
    },
    getParameterDefaultValues(model) {
      return _em.ccall("csmGetParameterDefaultValues", "number", ["number"], [model]);
    },
    getParameterValues(model) {
      return _em.ccall("csmGetParameterValues", "number", ["number"], [model]);
    },
    getParameterRepeats(model) {
      /* v6-only. */
      return _em.ccall("csmGetParameterRepeats", "number", ["number"], [model]);
    },
    getParameterKeyCounts(model) {
      return _em.ccall("csmGetParameterKeyCounts", "number", ["number"], [model]);
    },
    getParameterKeyValues(model) {
      return _em.ccall("csmGetParameterKeyValues", "number", ["number"], [model]);
    },

    /* Parts. */
    getPartCount(model) {
      return _em.ccall("csmGetPartCount", "number", ["number"], [model]);
    },
    getPartIds(model) {
      return _em.ccall("csmGetPartIds", "number", ["number"], [model]);
    },
    getPartOpacities(model) {
      return _em.ccall("csmGetPartOpacities", "number", ["number"], [model]);
    },
    getPartParentPartIndices(model) {
      return _em.ccall("csmGetPartParentPartIndices", "number", ["number"], [model]);
    },
    getPartOffscreenIndices(model) {
      /* v6-only. */
      return _em.ccall("csmGetPartOffscreenIndices", "number", ["number"], [model]);
    },

    /* Drawables. */
    getDrawableCount(model) {
      return _em.ccall("csmGetDrawableCount", "number", ["number"], [model]);
    },
    getDrawableIds(model) {
      return _em.ccall("csmGetDrawableIds", "number", ["number"], [model]);
    },
    getDrawableConstantFlags(model) {
      return _em.ccall("csmGetDrawableConstantFlags", "number", ["number"], [model]);
    },
    getDrawableDynamicFlags(model) {
      return _em.ccall("csmGetDrawableDynamicFlags", "number", ["number"], [model]);
    },
    getDrawableTextureIndices(model) {
      return _em.ccall("csmGetDrawableTextureIndices", "number", ["number"], [model]);
    },
    getDrawableDrawOrders(model) {
      return _em.ccall("csmGetDrawableDrawOrders", "number", ["number"], [model]);
    },
    getDrawableOpacities(model) {
      return _em.ccall("csmGetDrawableOpacities", "number", ["number"], [model]);
    },
    getDrawableMaskCounts(model) {
      return _em.ccall("csmGetDrawableMaskCounts", "number", ["number"], [model]);
    },
    getDrawableMasks(model) {
      return _em.ccall("csmGetDrawableMasks", "number", ["number"], [model]);
    },
    getDrawableVertexCounts(model) {
      return _em.ccall("csmGetDrawableVertexCounts", "number", ["number"], [model]);
    },
    getDrawableVertexPositions(model) {
      return _em.ccall("csmGetDrawableVertexPositions", "number", ["number"], [model]);
    },
    getDrawableVertexUvs(model) {
      return _em.ccall("csmGetDrawableVertexUvs", "number", ["number"], [model]);
    },
    getDrawableIndexCounts(model) {
      return _em.ccall("csmGetDrawableIndexCounts", "number", ["number"], [model]);
    },
    getDrawableIndices(model) {
      return _em.ccall("csmGetDrawableIndices", "number", ["number"], [model]);
    },
    getDrawableMultiplyColors(model) {
      return _em.ccall("csmGetDrawableMultiplyColors", "number", ["number"], [model]);
    },
    getDrawableScreenColors(model) {
      return _em.ccall("csmGetDrawableScreenColors", "number", ["number"], [model]);
    },
    getDrawableParentPartIndices(model) {
      return _em.ccall("csmGetDrawableParentPartIndices", "number", ["number"], [model]);
    },
    getDrawableBlendModes(model) {
      /* v6-only. */
      return _em.ccall("csmGetDrawableBlendModes", "number", ["number"], [model]);
    },
    resetDrawableDynamicFlags(model) {
      _em.ccall("csmResetDrawableDynamicFlags", null, ["number"], [model]);
    },

    /* Offscreens (v6-only): the symbols exist only on v6 builds, and these
       shims are only called when _v6, so they invoke the symbol directly. */
    getOffscreenCount(model) {
      return _em.ccall("csmGetOffscreenCount", "number", ["number"], [model]);
    },
    getOffscreenBlendModes(model) {
      return _em.ccall("csmGetOffscreenBlendModes", "number", ["number"], [model]);
    },
    getOffscreenOpacities(model) {
      return _em.ccall("csmGetOffscreenOpacities", "number", ["number"], [model]);
    },
    getOffscreenOwnerIndices(model) {
      return _em.ccall("csmGetOffscreenOwnerIndices", "number", ["number"], [model]);
    },
    getOffscreenMultiplyColors(model) {
      return _em.ccall("csmGetOffscreenMultiplyColors", "number", ["number"], [model]);
    },
    getOffscreenScreenColors(model) {
      return _em.ccall("csmGetOffscreenScreenColors", "number", ["number"], [model]);
    },
    getOffscreenMaskCounts(model) {
      return _em.ccall("csmGetOffscreenMaskCounts", "number", ["number"], [model]);
    },
    getOffscreenMasks(model) {
      return _em.ccall("csmGetOffscreenMasks", "number", ["number"], [model]);
    },
    getOffscreenConstantFlags(model) {
      return _em.ccall("csmGetOffscreenConstantFlags", "number", ["number"], [model]);
    },

    mallocMoc(mocSize) {
      return _em.ccall("csmMallocMoc", "number", ["number"], [mocSize]);
    },
    mallocModelAndInitialize(moc) {
      return _em.ccall("csmMallocModelAndInitialize", "number",
        ["number"], [moc]);
    },
    malloc(size) {
      return _em.ccall("csmMalloc", "number", ["number"], [size]);
    },
    free(memory) {
      _em.ccall("csmFree", null, ["number"], [memory]);
    },
    initializeAmountOfMemory(size) {
      _em.ccall("csmInitializeAmountOfMemory", null, ["number"], [size]);
    },
  };

  /* Alignment constants. */
  PurismCore.AlignofMoc = 64;
  PurismCore.AlignofModel = 16;

  /* .moc3 file versions. */
  PurismCore.MocVersion_Unknown = 0;
  PurismCore.MocVersion_30 = 1;
  PurismCore.MocVersion_33 = 2;
  PurismCore.MocVersion_40 = 3;
  PurismCore.MocVersion_42 = 4;
  PurismCore.MocVersion_50 = 5;
  PurismCore.MocVersion_53 = 6;

  /* Parameter types. */
  PurismCore.ParameterType_Normal = 0;
  PurismCore.ParameterType_BlendShape = 1;

  /* Error codes (Purism extension; from Moc#getError / Model#getLastError). */
  PurismCore.Error_NoError = 0;
  PurismCore.Error_Failed = 1;
  PurismCore.Error_ParameterRange = 2;
  PurismCore.Error_FileUnrecognized = 3;
  PurismCore.Error_FileCorrupt = 4;
  PurismCore.Error_InvalidData = 5;
  PurismCore.Error_InvalidParameter = 6;

  /* Maps a csmError code to a static string (Purism extension). */
  PurismCore.csmGetErrorString = function (error) {
    return _csm.getErrorString(error);
  };

  /* Color blend types. */
  PurismCore.ColorBlendType_Normal = 0;
  PurismCore.ColorBlendType_Add = 3;
  PurismCore.ColorBlendType_AddGlow = 4;
  PurismCore.ColorBlendType_Darken = 5;
  PurismCore.ColorBlendType_Multiply = 6;
  PurismCore.ColorBlendType_ColorBurn = 7;
  PurismCore.ColorBlendType_LinearBurn = 8;
  PurismCore.ColorBlendType_Lighten = 9;
  PurismCore.ColorBlendType_Screen = 10;
  PurismCore.ColorBlendType_ColorDodge = 11;
  PurismCore.ColorBlendType_Overlay = 12;
  PurismCore.ColorBlendType_SoftLight = 13;
  PurismCore.ColorBlendType_HardLight = 14;
  PurismCore.ColorBlendType_LinearLight = 15;
  PurismCore.ColorBlendType_Hue = 16;
  PurismCore.ColorBlendType_Color = 17;
  PurismCore.ColorBlendType_AddCompatible = 1;
  PurismCore.ColorBlendType_MultiplyCompatible = 2;

  /* Alpha blend types. */
  PurismCore.AlphaBlendType_Over = 0;
  PurismCore.AlphaBlendType_Atop = 1;
  PurismCore.AlphaBlendType_Out = 2;
  PurismCore.AlphaBlendType_ConjointOver = 3;
  PurismCore.AlphaBlendType_DisjointOver = 4;

  /* Version. */
  class Version {
    static csmGetVersion() {
      return _csm.getVersion();
    }
    static csmGetTrueVersion() {
      return _csm.getTrueVersion();
    }
    static csmGetExtendedVersionString() {
      return _csm.getExtendedVersionString();
    }
    static csmGetLatestMocVersion() {
      return _csm.getLatestMocVersion();
    }
    static csmGetMocVersion(data, mocBytes) {
      if (data instanceof Moc) {
        return _csm.getMocVersion(data._ptr, mocBytes.byteLength);
      }
      const memory = _csm.mallocMoc(data.byteLength);
      if (!memory) {
        return 0;
      }
      const dst = new Uint8Array(_em.HEAPU8.buffer, memory, data.byteLength);
      dst.set(new Uint8Array(data));
      const v = _csm.getMocVersion(memory, data.byteLength);
      _csm.free(memory);
      return v;
    }
  }
  PurismCore.Version = Version;

  /* Logging. */
  class Logging {
    static csmSetLogFunction(handler) {
      Logging.logFunction = handler;
      const pointer = _em.addFunction(Logging.wrapLogFunction, "vi");
      _csm.setLogFunction(pointer);
    }
    static csmGetLogFunction() {
      return Logging.logFunction;
    }
    static wrapLogFunction(messagePtr) {
      const messageStr = _em.UTF8ToString(messagePtr);
      Logging.logFunction(messageStr);
    }
  }
  PurismCore.Logging = Logging;

  /* Moc. */
  class Moc {
    constructor(mocBytes) {
      const memory = _csm.mallocMoc(mocBytes.byteLength);
      if (!memory) {
        return;
      }
      const dst = new Uint8Array(_em.HEAPU8.buffer, memory, mocBytes.byteLength);
      dst.set(new Uint8Array(mocBytes));
      this._ptr = _csm.reviveMocInPlace(memory, mocBytes.byteLength);
      if (!this._ptr) {
        _csm.free(memory);
      }
    }
    hasMocConsistency(mocBytes) {
      const memory = _csm.mallocMoc(mocBytes.byteLength);
      if (!memory) {
        return;
      }
      const dst = new Uint8Array(_em.HEAPU8.buffer, memory, mocBytes.byteLength);
      dst.set(new Uint8Array(mocBytes));
      const ok = _csm.hasMocConsistency(memory, mocBytes.byteLength);
      _csm.free(memory);
      return ok;
    }
    static fromArrayBuffer(buffer) {
      if (!buffer) {
        return null;
      }
      const moc = new Moc(buffer);
      return (moc._ptr) ? moc : null;
    }
    /* Purism extension: the outcome of this moc's revive/init (a csmError code;
     * map with PurismCore.csmGetErrorString). */
    getError() {
      return _csm.getMocError(this._ptr);
    }
    _release() {
      _csm.free(this._ptr);
      this._ptr = 0;
    }
  }
  PurismCore.Moc = Moc;

  /* Model. */
  class Model {
    constructor(moc) {
      this._ptr = _csm.mallocModelAndInitialize(moc._ptr);
      if (!this._ptr) {
        return;
      }
      /* Construct all heap views AFTER the final allocation: with
       ALLOW_MEMORY_GROWTH the heap ArrayBuffer can be detached and
       replaced on any _malloc, invalidating earlier views. No further
       WASM allocation happens after this point in the constructor. */
      this.parameters = new Parameters(this._ptr);
      this.parts = new Parts(this._ptr);
      this.drawables = new Drawables(this._ptr);
      if (_v6) {
        this.offscreens = new Offscreens(this._ptr);
      }
      this.canvasinfo = new CanvasInfo(this._ptr);
      const length = _csm.getDrawableCount(this._ptr) +
        (_v6 ? _csm.getOffscreenCount(this._ptr) : 0);
      this.renderOrders = new Int32Array(_em.HEAP32.buffer,
        _csm.getRenderOrders(this._ptr), length);
    }
    static fromMoc(moc) {
      const model = new Model(moc);
      return (model._ptr) ? model : null;
    }
    getRenderOrders() {
      return this.renderOrders;
    }
    update() {
      _csm.updateModel(this._ptr);
    }
    /* Purism extension: the error recorded by this model's most recent update
     * (a csmError code; map with PurismCore.csmGetErrorString). */
    getLastError() {
      return _csm.getLastError(this._ptr);
    }
    release() {
      _csm.free(this._ptr);
      this._ptr = 0;
    }
  }
  PurismCore.Model = Model;

  /* CanvasInfo. */
  class CanvasInfo {
    constructor(modelPtr) {
      if (!modelPtr) {
        return;
      }
      const sizePtr = _csm.malloc(2 * 4);
      const originPtr = _csm.malloc(2 * 4);
      const ppuPtr = _csm.malloc(1 * 4);
      _csm.readCanvasInfo(modelPtr, sizePtr, originPtr, ppuPtr);
      /* Re-read the buffer: an intervening malloc may have grown and
       detached it. */
      const f32 = new Float32Array(_em.HEAPF32.buffer);
      this.CanvasWidth = f32[sizePtr >> 2];
      this.CanvasHeight = f32[(sizePtr >> 2) + 1];
      this.CanvasOriginX = f32[originPtr >> 2];
      this.CanvasOriginY = f32[(originPtr >> 2) + 1];
      this.PixelsPerUnit = f32[ppuPtr >> 2];
      _csm.free(sizePtr);
      _csm.free(originPtr);
      _csm.free(ppuPtr);
    }
  }
  PurismCore.CanvasInfo = CanvasInfo;

  /* Parameters. */
  class Parameters {
    constructor(modelPtr) {
      const length = _csm.getParameterCount(modelPtr);
      this.count = length;
      this.ids = new Array(length);
      const idsPtr = _csm.getParameterIds(modelPtr);
      const _ids = new Uint32Array(_em.HEAPU32.buffer, idsPtr, length);
      for (let i = 0; i < _ids.length; i++) {
        this.ids[i] = _em.UTF8ToString(_ids[i]);
      }
      this.types = new Int32Array(_em.HEAP32.buffer,
        _csm.getParameterTypes(modelPtr), length);
      this.minimumValues = new Float32Array(_em.HEAPF32.buffer,
        _csm.getParameterMinimumValues(modelPtr), length);
      this.maximumValues = new Float32Array(_em.HEAPF32.buffer,
        _csm.getParameterMaximumValues(modelPtr), length);
      this.defaultValues = new Float32Array(_em.HEAPF32.buffer,
        _csm.getParameterDefaultValues(modelPtr), length);
      this.values = new Float32Array(_em.HEAPF32.buffer,
        _csm.getParameterValues(modelPtr), length);
      if (_v6) {   /* repeats are a v6-only field */
        this.repeats = new Int32Array(_em.HEAP32.buffer,
          _csm.getParameterRepeats(modelPtr), length);
      }
      this.keyCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getParameterKeyCounts(modelPtr), length);
      const counts = new Int32Array(_em.HEAP32.buffer,
        _csm.getParameterKeyCounts(modelPtr), length);
      this.keyValues = new Array(length);
      const _kv = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getParameterKeyValues(modelPtr), length);
      for (let j = 0; j < _kv.length; j++) {
        this.keyValues[j] = new Float32Array(_em.HEAPF32.buffer,
          _kv[j], counts[j]);
      }
    }
  }
  PurismCore.Parameters = Parameters;

  /* Parts. */
  class Parts {
    constructor(modelPtr) {
      const length = _csm.getPartCount(modelPtr);
      this.count = length;
      this.ids = new Array(length);
      const _ids = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getPartIds(modelPtr), length);
      for (let i = 0; i < _ids.length; i++) {
        this.ids[i] = _em.UTF8ToString(_ids[i]);
      }
      this.opacities = new Float32Array(_em.HEAPF32.buffer,
        _csm.getPartOpacities(modelPtr), length);
      this.parentIndices = new Int32Array(_em.HEAP32.buffer,
        _csm.getPartParentPartIndices(modelPtr), length);
      if (_v6) {   /* offscreenIndices are a v6-only field */
        this.offscreenIndices = new Int32Array(_em.HEAP32.buffer,
          _csm.getPartOffscreenIndices(modelPtr), length);
      }
    }
  }
  PurismCore.Parts = Parts;

  /* Drawables. */
  class Drawables {
    constructor(modelPtr) {
      this._modelPtr = modelPtr;
      const length = _csm.getDrawableCount(modelPtr);
      this.count = length;
      this.ids = new Array(length);
      const _ids = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getDrawableIds(modelPtr), length);
      for (let i = 0; i < _ids.length; i++) {
        this.ids[i] = _em.UTF8ToString(_ids[i]);
      }
      this.constantFlags = new Uint8Array(_em.HEAPU8.buffer,
        _csm.getDrawableConstantFlags(modelPtr), length);
      this.dynamicFlags = new Uint8Array(_em.HEAPU8.buffer,
        _csm.getDrawableDynamicFlags(modelPtr), length);
      this.textureIndices = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableTextureIndices(modelPtr), length);
      this.drawOrders = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableDrawOrders(modelPtr), length);
      this.opacities = new Float32Array(_em.HEAPF32.buffer,
        _csm.getDrawableOpacities(modelPtr), length);
      this.maskCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableMaskCounts(modelPtr), length);
      this.vertexCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableVertexCounts(modelPtr), length);
      this.indexCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableIndexCounts(modelPtr), length);
      this.multiplyColors = new Float32Array(_em.HEAPF32.buffer,
        _csm.getDrawableMultiplyColors(modelPtr), length * 4);
      this.screenColors = new Float32Array(_em.HEAPF32.buffer,
        _csm.getDrawableScreenColors(modelPtr), length * 4);
      this.parentPartIndices = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableParentPartIndices(modelPtr), length);
      if (_v6) {
        this.blendModes = new Int32Array(_em.HEAP32.buffer,
          _csm.getDrawableBlendModes(modelPtr), length);
      }

      const maskCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableMaskCounts(modelPtr), length);
      this.masks = new Array(length);
      const _masks = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getDrawableMasks(modelPtr), length);
      for (let m = 0; m < _masks.length; m++) {
        this.masks[m] = new Int32Array(_em.HEAP32.buffer,
          _masks[m], maskCounts[m]);
      }

      const vCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableVertexCounts(modelPtr), length);
      this.vertexPositions = new Array(length);
      const _pos = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getDrawableVertexPositions(modelPtr), length);
      for (let p = 0; p < _pos.length; p++) {
        this.vertexPositions[p] = new Float32Array(_em.HEAPF32.buffer,
          _pos[p], vCounts[p] * 2);
      }
      this.vertexUvs = new Array(length);
      const _uvs = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getDrawableVertexUvs(modelPtr), length);
      for (let u = 0; u < _uvs.length; u++) {
        this.vertexUvs[u] = new Float32Array(_em.HEAPF32.buffer,
          _uvs[u], vCounts[u] * 2);
      }

      const iCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getDrawableIndexCounts(modelPtr), length);
      this.indices = new Array(length);
      const _idx = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getDrawableIndices(modelPtr), length);
      for (let x = 0; x < _idx.length; x++) {
        this.indices[x] = new Uint16Array(_em.HEAPU16.buffer,
          _idx[x], iCounts[x]);
      }
    }
    resetDynamicFlags() {
      _csm.resetDrawableDynamicFlags(this._modelPtr);
    }
  }
  PurismCore.Drawables = Drawables;

  /* Offscreens (v6-only; constructed only on v6). A v6 model with no
   * offscreens still reaches the empty branch. */
  class Offscreens {
    constructor(modelPtr) {
      const length = _csm.getOffscreenCount(modelPtr);
      this.count = length;
      if (!length) {
        this.blendModes = new Int32Array(0);
        this.opacities = new Float32Array(0);
        this.ownerIndices = new Int32Array(0);
        this.multiplyColors = new Float32Array(0);
        this.screenColors = new Float32Array(0);
        this.maskCounts = new Int32Array(0);
        this.constantFlags = new Uint8Array(0);
        this.masks = [];
        return;
      }
      /* One packed int per offscreen (color | alpha<<8), like drawables. */
      this.blendModes = new Int32Array(_em.HEAP32.buffer,
        _csm.getOffscreenBlendModes(modelPtr), length);
      this.opacities = new Float32Array(_em.HEAPF32.buffer,
        _csm.getOffscreenOpacities(modelPtr), length);
      this.ownerIndices = new Int32Array(_em.HEAP32.buffer,
        _csm.getOffscreenOwnerIndices(modelPtr), length);
      this.multiplyColors = new Float32Array(_em.HEAPF32.buffer,
        _csm.getOffscreenMultiplyColors(modelPtr), length * 4);
      this.screenColors = new Float32Array(_em.HEAPF32.buffer,
        _csm.getOffscreenScreenColors(modelPtr), length * 4);
      this.maskCounts = new Int32Array(_em.HEAP32.buffer,
        _csm.getOffscreenMaskCounts(modelPtr), length);
      this.constantFlags = new Uint8Array(_em.HEAPU8.buffer,
        _csm.getOffscreenConstantFlags(modelPtr), length);
      const counts = new Int32Array(_em.HEAP32.buffer,
        _csm.getOffscreenMaskCounts(modelPtr), length);
      this.masks = new Array(length);
      const _masks = new Uint32Array(_em.HEAPU32.buffer,
        _csm.getOffscreenMasks(modelPtr), length);
      for (let i = 0; i < _masks.length; i++) {
        this.masks[i] = new Int32Array(_em.HEAP32.buffer,
          _masks[i], counts[i]);
      }
    }
  }

  /* Utility flag helpers. */
  class Utils {
    static hasBlendAdditiveBit(b) { return (b & 1) == 1; }
    static hasBlendMultiplicativeBit(b) { return (b & 2) == 2; }
    static hasIsDoubleSidedBit(b) { return (b & 4) == 4; }
    static hasIsInvertedMaskBit(b) { return (b & 8) == 8; }
    static hasIsVisibleBit(b) { return (b & 1) == 1; }
    static hasVisibilityDidChangeBit(b) { return (b & 2) == 2; }
    static hasOpacityDidChangeBit(b) { return (b & 4) == 4; }
    static hasDrawOrderDidChangeBit(b) { return (b & 8) == 8; }
    static hasRenderOrderDidChangeBit(b) { return (b & 16) == 16; }
    static hasVertexPositionsDidChangeBit(b) { return (b & 32) == 32; }
    static hasBlendColorDidChangeBit(b) { return (b & 64) == 64; }
  }
  PurismCore.Utils = Utils;

  /* Memory. */
  class Memory {
    static initializeAmountOfMemory(size) {
      if (size > 16777216) {
        _csm.initializeAmountOfMemory(size);
      }
    }
  }
  PurismCore.Memory = Memory;

  /* Expose the module setter for the bootstrap tail. Detect the build's ABI
     once here (v5 lacks the v6-only symbols) and expose the v6-only Offscreens
     class only when present. */
  PurismCore._setModule = function (m) {
    _em = m;
    _v6 = _has("csmGetRenderOrders");
    if (_v6) {
      PurismCore.Offscreens = Offscreens;
    } else {
      Model.prototype.getDrawableRenderOrders = Model.prototype.getRenderOrders;
      delete Model.prototype.getRenderOrders;
      for (const k of Object.keys(PurismCore)) {
        if (k.startsWith("ColorBlendType_") || k.startsWith("AlphaBlendType_")) {
          delete PurismCore[k];
        }
      }
    }
  };
})(PurismCore || (PurismCore = {}));
