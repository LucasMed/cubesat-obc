/**
 * @file test_storage_manager.c
 * @brief Unit tests for the Storage Manager.
 *
 * Verifies that the high-level API correctly creates directories,
 * appends log data, and writes images on the host system.
 */

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif

#include "storage_manager.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

void setUp(void) {
    /* Robust cleanup of host test directories */
    system("rm -rf LOGS IMAGES");
}

void tearDown(void) {
    unlink("LOGS/test.dat");
    unlink("IMAGES/test.jpg");
    rmdir("LOGS");
    rmdir("IMAGES");
}

void test_storage_lifecycle(void) {
    /* 1. Init (should create directories) */
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, storage_init());
    
    /* 2. Append log */
    const char *log_data = "SENSOR_PACKET_001";
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, storage_append_log("/LOGS/test.dat", log_data, strlen(log_data)));
    
    /* 3. Verify file content on host */
    const char *path = "LOGS/test.dat";
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    char buffer[32];
    size_t n = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);
    TEST_ASSERT_EQUAL_INT((int)strlen(log_data), (int)n);
    TEST_ASSERT_EQUAL_INT(0, memcmp(log_data, buffer, strlen(log_data)));
    
    /* 4. Write image */
    uint8_t img_data[10] = {0xFF, 0xD8, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0xFF, 0xD9};
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, storage_write_image("/IMAGES/test.jpg", img_data, 10));
    
    /* 5. Check free space (mock) */
    uint64_t free_sp = storage_get_free_space();
    TEST_ASSERT_TRUE(free_sp > 0);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_storage_lifecycle);
    return UNITY_END();
}
