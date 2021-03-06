/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_gb18030_2000_intern.h"

typedef struct gb18030_range {
  u_int32_t u_first;
  u_int32_t u_last;
  u_char b_first[4];
  u_char b_last[4];

} gb18030_range_t;

#include "table/ef_gb18030_2000_range.table"

/* --- static functions --- */

static u_int32_t bytes_to_linear(u_char *bytes /* should be 4 bytes. */
                                 ) {
  return ((bytes[0] * 10 + bytes[1]) * 126 + bytes[2]) * 10 + bytes[3];
}

static void linear_to_bytes(u_char *bytes, /* should be 4 bytes. */
                            u_int32_t linear) {
  linear -= bytes_to_linear((u_char *)"\x81\x30\x81\x30");

  bytes[3] = 0x30 + linear % 10;
  linear /= 10;

  bytes[2] = 0x81 + linear % 126;
  linear /= 126;

  bytes[1] = 0x30 + linear % 10;
  linear /= 10;

  bytes[0] = 0x81 + linear;
}

/* --- global functions --- */

int ef_decode_gb18030_2000_to_ucs4(u_char *ucs4,   /* should be 4 bytes. */
                                    u_char *gb18030 /* should be 4 bytes. */
                                    ) {
  int count;
  u_int32_t linear;
  u_int32_t ucs4_code;

  linear = bytes_to_linear(gb18030);

  for (count = 0; count < sizeof(gb18030_ranges) / sizeof(gb18030_ranges[0]); count++) {
    if (bytes_to_linear(gb18030_ranges[count].b_first) <= linear &&
        linear <= bytes_to_linear(gb18030_ranges[count].b_last)) {
      ucs4_code =
          gb18030_ranges[count].u_first + (linear - bytes_to_linear(gb18030_ranges[count].b_first));

      ucs4[0] = (ucs4_code >> 24) & 0xff;
      ucs4[1] = (ucs4_code >> 16) & 0xff;
      ucs4[2] = (ucs4_code >> 8) & 0xff;
      ucs4[3] = ucs4_code & 0xff;

      return 1;
    }
  }

  return 0;
}

int ef_encode_ucs4_to_gb18030_2000(u_char *gb18030, /* should be 4 bytes */
                                    u_char *ucs4     /* should be 4 bytes */
                                    ) {
  int count;
  u_int32_t ucs4_code;

  ucs4_code = ((ucs4[0] << 24) & 0xff000000) + ((ucs4[1] << 16) & 0xff0000) +
              ((ucs4[2] << 8) & 0xff00) + ucs4[3];

  for (count = 0; count < sizeof(gb18030_ranges) / sizeof(gb18030_ranges[0]); count++) {
    if (gb18030_ranges[count].u_first <= ucs4_code && ucs4_code <= gb18030_ranges[count].u_last) {
      linear_to_bytes(gb18030, bytes_to_linear(gb18030_ranges[count].b_first) +
                                   (ucs4_code - gb18030_ranges[count].u_first));

      return 1;
    }
  }

  return 0;
}
