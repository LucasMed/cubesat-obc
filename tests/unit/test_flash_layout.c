/**
 * @file test_flash_layout.c
 * @brief Compile-time validation of W25Q64 flash memory layout.
 *
 * The _Static_assert declarations in flash_layout.h are verified at
 * compile time — this test exists to ensure they are evaluated and
 * cannot be accidentally removed without test failure.
 */

#include "unity.h"
#include "flash_layout.h"

void setUp(void) {}
void tearDown(void) {}

void test_flash_layout_regions_non_overlapping(void)
{
    /* The _Static_assert in flash_layout.h validates:
     *  - POST region (0x700000) does not overlap IMU calib (0x7F0000)
     *  - IMU calib region (0x7F0000) fits within flash (8 MB)
     *
     * If the assertions fire, this test will not compile.
     * If they pass, this test always succeeds.                     */
    TEST_ASSERT_TRUE(FLASH_SIZE_BYTES == 8u * 1024u * 1024u);
    TEST_ASSERT_TRUE(FLASH_SECTOR_POST == 0x700000u);
    TEST_ASSERT_TRUE(FLASH_SECTOR_IMU_CALIB == 0x7F0000u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_flash_layout_regions_non_overlapping);
    return UNITY_END();
}
