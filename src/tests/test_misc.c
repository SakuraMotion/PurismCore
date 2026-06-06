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
