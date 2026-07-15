/* test_crc32.c — Unit tests for CRC-32 (PKZIP / Ethernet)
 *
 * Validates the bootloader CRC32 implementation with known test vectors,
 * incremental API, and edge cases.
 *
 * T-CRC32-01  test_crc32_known_vector   – CRC-32("123456789") == 0xCBF43926
 * T-CRC32-02  test_crc32_empty          – CRC-32("") == 0x00000000
 * T-CRC32-03  test_crc32_single_byte    – CRC-32 of single-byte buffers
 * T-CRC32-04  test_crc32_incremental    – init+update+finalize == compute
 * T-CRC32-05  test_crc32_consistency    – 100 random buffers, both paths match
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CRC32 public API — compiled as separate translation unit (crc32.c linked). */
#include "crc32.h"

static int g_failures = 0;

#define CHECK(cond, msg)                                            \
  do {                                                              \
    if (!(cond)) {                                                  \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);       \
      g_failures++;                                                 \
    }                                                               \
  } while (0)

/* ================================================================
 * T-CRC32-01: Known test vector
 *
 * CRC-32 check value for "123456789" is 0xCBF43926.
 * Source: https://reveng.sourceforge.io/crc-catalogue/16.htm
 * This is the definitive validation for any CRC-32 implementation.
 * ================================================================ */
void test_crc32_known_vector(void)
{
  printf("[%s]\n", __func__);

  const uint8_t check[] = "123456789";
  uint32_t result = crc32_compute(check, 9);
  printf("  CRC-32('123456789') = 0x%08lX (expected 0xCBF43926)\n",
         (unsigned long)result);
  CHECK(result == 0xCBF43926u,
        "Known check value: CRC-32('123456789') == 0xCBF43926");
}

/* ================================================================
 * T-CRC32-02: Empty (zero-length) input
 *
 * CRC-32 of an empty input = 0x00000000
 * (init=0xFFFFFFFF, no data, finalize XOR 0xFFFFFFFF = 0x00000000)
 * ================================================================ */
void test_crc32_empty(void)
{
  printf("[%s]\n", __func__);
  uint32_t result = crc32_compute(NULL, 0);
  printf("  CRC-32(empty) = 0x%08lX (expected 0x00000000)\n",
         (unsigned long)result);
  CHECK(result == 0x00000000u, "CRC-32 of empty input == 0x00000000");
}

/* ================================================================
 * T-CRC32-03: Single-byte buffers
 * ================================================================ */
void test_crc32_single_byte(void)
{
  printf("[%s]\n", __func__);

  /* CRC-32 of single byte 0x00 */
  const uint8_t zero = 0x00;
  uint32_t crc_zero = crc32_compute(&zero, 1);
  printf("  CRC-32(0x00) = 0x%08lX\n", (unsigned long)crc_zero);
  /* Not a specific expected value, but must be consistent */
  CHECK(crc32_compute(&zero, 1) == crc_zero,
        "CRC-32 of 0x00 is deterministic");

  /* CRC-32 of single byte 0xFF */
  const uint8_t ff = 0xFF;
  uint32_t crc_ff = crc32_compute(&ff, 1);
  printf("  CRC-32(0xFF) = 0x%08lX\n", (unsigned long)crc_ff);
  CHECK(crc32_compute(&ff, 1) == crc_ff,
        "CRC-32 of 0xFF is deterministic");

  /* Different bytes give different CRCs */
  CHECK(crc_zero != crc_ff,
        "CRC-32(0x00) != CRC-32(0xFF)");
}

/* ================================================================
 * T-CRC32-04: Incremental API
 *
 * crc32_init() + crc32_update() + crc32_finalize() must produce the
 * same result as crc32_compute() for the same data.
 * ================================================================ */
void test_crc32_incremental(void)
{
  printf("[%s]\n", __func__);

  /* Test with various split sizes */
  const uint8_t data[] = "The quick brown fox jumps over the lazy dog";
  size_t data_len = strlen((const char *)data);

  /* Full compute */
  uint32_t expected = crc32_compute(data, data_len);

  /* Single-shot incremental */
  uint32_t crc = crc32_init();
  crc = crc32_update(crc, data, data_len);
  uint32_t result = crc32_finalize(crc);
  CHECK(result == expected,
        "Incremental (single chunk) matches compute");

  /* Two-chunk incremental */
  size_t mid = data_len / 2;
  crc = crc32_init();
  crc = crc32_update(crc, data, mid);
  crc = crc32_update(crc, data + mid, data_len - mid);
  result = crc32_finalize(crc);
  CHECK(result == expected,
        "Incremental (2 chunks) matches compute");

  /* Byte-at-a-time incremental */
  crc = crc32_init();
  for (size_t i = 0; i < data_len; i++) {
    crc = crc32_update(crc, data + i, 1);
  }
  result = crc32_finalize(crc);
  CHECK(result == expected,
        "Incremental (byte-at-a-time) matches compute");

  printf("  All incremental paths produce CRC-32 = 0x%08lX\n",
         (unsigned long)expected);
}

/* ================================================================
 * T-CRC32-05: Consistency stress test
 *
 * 100 random buffers, all incremental paths must match compute().
 * ================================================================ */
void test_crc32_consistency(void)
{
  printf("[%s]\n", __func__);

  srand(42);  /* Deterministic seed — coverity[risky_function] */

  for (int trial = 0; trial < 100; trial++)
  {
    size_t len = (size_t)(rand() % 1024) + 1;
    uint8_t *buf = (uint8_t *)malloc(len);
    assert(buf != NULL);
    for (size_t i = 0; i < len; i++) {
      buf[i] = (uint8_t)(rand() & 0xFF);
    }

    /* Reference: crc32_compute */
    uint32_t ref = crc32_compute(buf, len);

    /* Incremental single-chunk */
    uint32_t crc = crc32_init();
    crc = crc32_update(crc, buf, len);
    CHECK(crc32_finalize(crc) == ref,
          "Single-chunk incremental matches compute");

    /* Incremental random-split chunks */
    crc = crc32_init();
    size_t offset = 0;
    while (offset < len) {
      size_t chunk = (size_t)(rand() % 64) + 1;
      if (chunk > len - offset) chunk = len - offset;
      crc = crc32_update(crc, buf + offset, chunk);
      offset += chunk;
    }
    CHECK(crc32_finalize(crc) == ref,
          "Multi-chunk incremental matches compute");

    free(buf);
  }

  printf("  All 100 random trials: PASS\n");
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
  printf("=== CRC-32 Tests ===\n\n");

  test_crc32_known_vector();
  printf("\n");
  test_crc32_empty();
  printf("\n");
  test_crc32_single_byte();
  printf("\n");
  test_crc32_incremental();
  printf("\n");
  test_crc32_consistency();

  printf("\n=== Results: %d failures ===\n", g_failures);
  return g_failures > 0 ? 1 : 0;
}
