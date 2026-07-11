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
#include <errno.h>
#include <sys/stat.h>

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

void test_storage_append_log_null_filename(void) {
    /* Should return STORAGE_ERR_WRITE when filename is NULL */
    const char data[] = "test_data";
    TEST_ASSERT_EQUAL_INT(STORAGE_ERR_WRITE, storage_append_log(NULL, data, sizeof(data)));
}

void test_storage_write_image_null_filename(void) {
    /* Should return STORAGE_ERR_WRITE when filename is NULL */
    uint8_t data[4] = {0x01, 0x02, 0x03, 0x04};
    TEST_ASSERT_EQUAL_INT(STORAGE_ERR_WRITE, storage_write_image(NULL, data, sizeof(data)));
}

void test_storage_append_log_fopen_failure(void) {
    /* Should return STORAGE_ERR_OPEN when parent directory doesn't exist */
    setUp();
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, storage_init());
    const char data[] = "test_data";
    /* Path with non-existent parent directory -> fopen fails (host) */
    TEST_ASSERT_EQUAL_INT(STORAGE_ERR_OPEN,
        storage_append_log("/NONEXISTENT_DIR/test.dat", data, sizeof(data)));
}

void test_storage_write_image_fopen_failure(void) {
    /* Should return STORAGE_ERR_OPEN when parent directory doesn't exist */
    setUp();
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, storage_init());
    uint8_t img_data[4] = {0xFF, 0xD8, 0xFF, 0xD9};
    /* Path with non-existent parent directory -> fopen fails (host) */
    TEST_ASSERT_EQUAL_INT(STORAGE_ERR_OPEN,
        storage_write_image("/NONEXISTENT_DIR/test.jpg", img_data, sizeof(img_data)));
}

void test_mkdir_logs_failure(void) {
    /* Force mkdir("LOGS") to fail with EACCES by removing write permission
     * from the current directory. After the call, restore permission. */
    setUp();
    chmod(".", 0555);
    /* mkdir("LOGS") will fail with EACCES, errno != EEXIST -> perror called */
    storage_status_t st = storage_init();
    chmod(".", 0755);
    /* storage_init still returns OK (the perror is non-fatal) */
    TEST_ASSERT_EQUAL_INT(STORAGE_OK, st);
    /* Clean up LOGS/IMAGES for subsequent tests */
    system("rm -rf LOGS IMAGES");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_storage_lifecycle);
    RUN_TEST(test_storage_append_log_null_filename);
    RUN_TEST(test_storage_write_image_null_filename);
    RUN_TEST(test_storage_append_log_fopen_failure);
    RUN_TEST(test_storage_append_log_fopen_failure);
    RUN_TEST(test_storage_write_image_fopen_failure);
    RUN_TEST(test_mkdir_logs_failure);
    return UNITY_END();
}
