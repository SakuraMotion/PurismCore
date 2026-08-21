/* Miscellaneous tests: blend mode, MOC3 header, glob, version */

TEST(blend_mode_remap)
{
#if PSM_COMPAT_VERSION >= 0x06000000L
  CHECK(psm__remap_blend_mode(
      csmColorBlendType_Normal) ==
      csmColorBlendType_Normal);
  CHECK(psm__remap_blend_mode(
      csmColorBlendType_AddGlow) ==
      csmColorBlendType_AddGlow);
#else
  CHECK(psm__remap_blend_mode(
      csmColorBlendType_Normal) ==
      csmColorBlendType_Normal);
  CHECK(psm__remap_blend_mode(
      csmColorBlendType_Add) ==
      csmColorBlendType_AddCompatible);
  CHECK(psm__remap_blend_mode(
      csmColorBlendType_Multiply) ==
      csmColorBlendType_MultiplyCompatible);
  CHECK(psm__remap_blend_mode(
      csmColorBlendType_Darken) ==
      csmColorBlendType_MultiplyCompatible);
  CHECK(psm__remap_blend_mode(99) ==
      csmColorBlendType_Normal);
#endif
}

TEST(moc3_version)
{
  psm__u8 buf[64];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, "MOC3", 4);
  buf[4] = 1;

  csmMocVersion ver = csmGetMocVersion(buf, sizeof(buf));
  CHECK(ver == csmMocVersion_30);
}

TEST(moc3_bad_magic)
{
  psm__u8 buf[64];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, "NOPE", 4);

  csmMocVersion ver = csmGetMocVersion(buf, sizeof(buf));
  CHECK(ver == csmMocVersion_Unknown);
}

TEST(moc3_too_small)
{
  psm__u8 buf[4] = {'M', 'O', 'C', '3'};
  csmMocVersion ver = csmGetMocVersion(buf, 4);
  CHECK(ver == csmMocVersion_Unknown);
}

TEST(glob_exact)
{
  CHECK(psm_glob_match("hello", "hello"));
  CHECK(!psm_glob_match("hello", "world"));
  CHECK(!psm_glob_match("", "x"));
  CHECK(!psm_glob_match("x", ""));
  CHECK(psm_glob_match("", ""));
}

TEST(glob_star)
{
  CHECK(psm_glob_match("*", "anything"));
  CHECK(psm_glob_match("*", ""));
  CHECK(psm_glob_match("Param*", "ParamAngleX"));
  CHECK(psm_glob_match("*Angle*", "ParamAngleX"));
  CHECK(!psm_glob_match("*Angle*", "ParamMouth"));
  CHECK(psm_glob_match("**", "test"));
}

TEST(glob_question)
{
  CHECK(psm_glob_match("h?llo", "hello"));
  CHECK(psm_glob_match("h?llo", "hallo"));
  CHECK(!psm_glob_match("h?llo", "hllo"));
  CHECK(!psm_glob_match("?", ""));
}

TEST(glob_case_insensitive)
{
  CHECK(psm_glob_match("HELLO", "hello"));
  CHECK(psm_glob_match("hello", "HELLO"));
  CHECK(psm_glob_match("*angle*", "ParamAngleX"));
}

TEST(version_api)
{
  csmVersion v = csmGetVersion();
  CHECK(v != 0);

  csmVersion tv = csmGetTrueVersion();
  CHECK(tv != 0);

  csmMocVersion mv = csmGetLatestMocVersion();
  CHECK(mv == csmMocVersion_53);
}

/* psm__resolve_params flags non-repeat inputs outside [min,max] and clamps. */
TEST(resolve_params_range)
{
  struct psm__param items[2];
  psm__f32 input[2];
  struct psm__params params;

  memset(items, 0, sizeof(items));
  /* param 0: non-repeat, range [-1, 1] */
  items[0].repeat = 0;
  items[0].range[0] = -1.0f;
  items[0].range[1] = 1.0f;
  items[0].range_length = 2.0f;
  /* param 1: repeat, range [0, 1] (wraps, never a range error) */
  items[1].repeat = 1;
  items[1].range[0] = 0.0f;
  items[1].range[1] = 1.0f;
  items[1].range_length = 1.0f;

  params.count = 2;
  params.items = items;
  params.input_value = input;
  params.type = NULL;

  /* all in range: no error, repeat param wraps without flagging */
  input[0] = 0.5f;
  input[1] = 3.25f;
  CHECK(psm__resolve_params(&params) == PSM__OK);
  CHECK_FLOAT(items[0].value, 0.5f, 0.0001f);
  CHECK_FLOAT(items[1].value, 0.25f, 0.0001f);

  /* non-repeat above max: error, value clamped, input rewritten to clamp */
  input[0] = 5.0f;
  CHECK(psm__resolve_params(&params) == PSM__ERR_PARAMETER_RANGE_ERROR);
  CHECK_FLOAT(items[0].value, 1.0f, 0.0001f);
  CHECK_FLOAT(input[0], 1.0f, 0.0001f);

  /* non-repeat below min: error, value clamped */
  input[0] = -9.0f;
  CHECK(psm__resolve_params(&params) == PSM__ERR_PARAMETER_RANGE_ERROR);
  CHECK_FLOAT(items[0].value, -1.0f, 0.0001f);

  /* empty parameter set: no error */
  params.count = 0;
  CHECK(psm__resolve_params(&params) == PSM__OK);
}

/* csmGetLastError on NULL is benign; csmGetErrorString covers every code. */
TEST(error_api)
{
  CHECK(csmGetLastError(NULL) == csmError_NoError);

  CHECK(strcmp(csmGetErrorString(csmError_NoError), "no error") == 0);
  CHECK(strcmp(csmGetErrorString(csmError_ParameterRange),
      "parameter out of range") == 0);
  CHECK(strcmp(csmGetErrorString(csmError_FileUnrecognized),
      "unrecognized MOC3 file") == 0);
  CHECK(strcmp(csmGetErrorString(csmError_FileCorrupt),
      "corrupt MOC3 file") == 0);
  /* out-of-enum code maps to the catch-all string, never NULL */
  CHECK(strcmp(csmGetErrorString((csmError)999), "unknown error") == 0);
}

/* csmReviveMocInPlace stashes its failure reason in the moc header. */
TEST(moc_error_api)
{
  CHECK(csmGetMocError(NULL) == csmError_NoError);

  /* Both cases are rejected before any aligned layout cast, so a plain
   * (4-byte-aligned) buffer is enough to exercise the error stash. */
  _Alignas(8) psm__u8 buf[256];
  memset(buf, 0, sizeof(buf));

  /* not a MOC3 at all -> unrecognized */
  memcpy(buf, "XXXX", 4);
  CHECK(csmReviveMocInPlace(buf, sizeof(buf)) == NULL);
  CHECK(csmGetMocError((const csmMoc *)buf) == csmError_FileUnrecognized);

  /* MOC3 magic but an impossible version -> corrupt */
  memcpy(buf, "MOC3", 4);
  buf[4] = 200;
  CHECK(csmReviveMocInPlace(buf, sizeof(buf)) == NULL);
  CHECK(csmGetMocError((const csmMoc *)buf) == csmError_FileCorrupt);
}
