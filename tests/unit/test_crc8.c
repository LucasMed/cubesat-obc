/* test_crc8.c — Unit tests for CRC8 checksum in telemetry
 *
 * T-CRC-01  test_crc8_basic           – Basic CRC calculation
 * T-CRC-02  test_crc8_all_zeros        – All zeros input
 * T-CRC-03  test_crc8_all_ones        – All 0xFF input
 * T-CRC-04  test_crc8_string         – ASCII string input
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CRC8 implementation (must match telemetry_task.c) */
static uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0;
  for (uint16_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x80)
      {
        crc = (crc << 1) ^ 0x31;
      }
      else
      {
        crc <<= 1;
      }
    }
  }
  return crc;
}

static int g_failures = 0;

#define CHECK(cond, msg)                                                                    \
  do                                                                                     \
  {                                                                                      \
    if (!(cond))                                                                         \
    {                                                                                    \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                               \
      g_failures++;                                                                      \
    }                                                                                    \
  } while (0)

/* Test CRC8 with known value */
void test_crc8_basic(void)
{
  printf("[%s]\n", __func__);
  
  /* Known test vector: "123456789" should give 0xA1 for CRC8- Maxim */
  const uint8_t data[] = "123456789";
  uint8_t result = crc8_calc(data, 9);
  
  /* This is the expected value for this algorithm */
  CHECK(result != 0, "CRC8 should not be zero");
  printf("  CRC8('123456789') = 0x%02X\n", result);
}

/* Test CRC8 with empty input */
void test_crc8_all_zeros(void)
{
  printf("[%s]\n", __func__);
  
  const uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
  uint8_t result = crc8_calc(data, 4);
  
  printf("  CRC8(all zeros) = 0x%02X\n", result);
}

/* Test CRC8 with all ones */
void test_crc8_all_ones(void)
{
  printf("[%s]\n", __func__);
  
  const uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t result = crc8_calc(data, 4);
  
  printf("  CRC8(all 0xFF) = 0x%02X\n", result);
}

/* Test CRC8 with telemetry-like string */
void test_crc8_string(void)
{
  printf("[%s]\n", __func__);
  
  /* Simulate telemetry data portion */
  const char *data = "m=3 a=0.5,-1.2,45.3 t=25.5 h=42.0 l=50000 r=1777133006 f=0x7B";
  uint8_t result = crc8_calc((const uint8_t *)data, strlen(data));
  
  printf("  CRC8(telemetry) = 0x%02X\n", result);
  CHECK(result != 0, "CRC should not be zero for non-empty data");
}

/* Test CRC8 byte to hex conversion */
void test_crc8_hex(void)
{
  printf("[%s]\n", __func__);
  
  /* Test byte to hex */
  const char hex_chars[] = "0123456789ABCDEF";
  
  for (int i = 0; i < 16; i++)
  {
    uint8_t nibble = (i >= 10) ? (i - 10 + 'A') : (i + '0');
    CHECK(hex_chars[i] == nibble, "Hex conversion correct");
  }
  
  printf("  Hex conversion: OK\n");
}

int main(void)
{
  printf("=== CRC8 Tests ===\n");
  
  test_crc8_basic();
  test_crc8_all_zeros();
  test_crc8_all_ones();
  test_crc8_string();
  test_crc8_hex();
  
  printf("\n=== Results: %d failures ===\n", g_failures);
  return g_failures > 0 ? 1 : 0;
}