/*
 * Purism Core: binary to C header build tool
 *
 * Usage: bin2h <input> <symbol> [output - default: stdout]
 *
 * Copyright (c) 2026 Sakura Motion Project
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>

int
main(int argc, char **argv)
{
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "usage: %s <input> <symbol> [output]\n", argv[0]);
    return 2;
  }
  const char *inpath = argv[1], *sym = argv[2];

  FILE *in = fopen(inpath, "rb");
  if (!in) {
    fprintf(stderr, "bin2h: cannot open %s\n", inpath);
    return 1;
  }
  FILE *out = stdout;
  if (argc == 4) {
    out = fopen(argv[3], "wb");
    if (!out) {
      fprintf(stderr, "bin2h: cannot write %s\n", argv[3]);
      fclose(in);
      return 1;
    }
  }

  fprintf(out, "static const char %s[] = {\n", sym);
  unsigned long size = 0;
  int           c;
  while ((c = fgetc(in)) != EOF) {
    fprintf(out, "0x%02x,", (unsigned char)c);
    if (++size % 16 == 0)
      fputc('\n', out);
  }
  fprintf(out, "0x00\n};\n");
  fprintf(out, "#define %s_size %lu\n", sym, size);

  if (out != stdout)
    fclose(out);
  fclose(in);
  return 0;
}
