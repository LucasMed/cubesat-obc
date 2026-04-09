/**
 * @file test_w25q64.c
 * @brief Unit tests for W25Q64 flash driver
 */

#include "w25q64.h"
#include "unity.h"
#include <string.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_w25q64_init(void)
{
    w25q64_status_t status = w25q64_init();
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_read_id(void)
{
    w25q64_id_t id;
    w25q64_status_t status = w25q64_read_id(&id);
    
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    TEST_ASSERT_TRUE(id.valid);
    TEST_ASSERT_EQUAL_INT(0xEF, id.manufacturer_id);  /* Winbond */
    TEST_ASSERT_EQUAL_INT(0x40, id.memory_type);
    TEST_ASSERT_EQUAL_INT(0x17, id.capacity);  /* W25Q64 */
}

void test_w25q64_capacity(void)
{
    uint32_t capacity = w25q64_get_capacity();
    TEST_ASSERT_EQUAL_UINT32(8 * 1024 * 1024, capacity);  /* 8MB */
}

void test_w25q64_is_present(void)
{
    bool present = w25q64_is_present();
    TEST_ASSERT_TRUE(present);
}

void test_w25q64_read_write(void)
{
    /* Test reading - should succeed with dummy data in host mode */
    uint8_t read_buf[16];
    memset(read_buf, 0xAA, sizeof(read_buf));
    
    w25q64_status_t status = w25q64_read(0, read_buf, 16);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    
    /* Test write - should succeed in host mode */
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                               0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    
    status = w25q64_write_page(0, write_data, 16);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_erase(void)
{
    w25q64_status_t status;
    
    /* Test sector erase */
    status = w25q64_erase_sector(0);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    
    /* Test block erase 32KB */
    status = w25q64_erase_block32(0);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    
    /* Test block erase 64KB */
    status = w25q64_erase_block64(0);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_erase_alignment(void)
{
    w25q64_status_t status;
    
    /* Sector erase must be 4KB-aligned - test with misaligned address */
    status = w25q64_erase_sector(100);  /* Not aligned */
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_ERASE, status);
    
    /* Block 32KB must be 32KB-aligned */
    status = w25q64_erase_block32(100);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_ERASE, status);
    
    /* Block 64KB must be 64KB-aligned */
    status = w25q64_erase_block64(100);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_ERASE, status);
}

void test_w25q64_status(void)
{
    uint8_t status = w25q64_read_status();
    /* In host mode, returns 0 (idle) */
    TEST_ASSERT_EQUAL_INT(0, status);
}

void test_w25q64_wait_ready(void)
{
    w25q64_status_t status = w25q64_wait_ready(100);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_w25q64_init);
    RUN_TEST(test_w25q64_read_id);
    RUN_TEST(test_w25q64_capacity);
    RUN_TEST(test_w25q64_is_present);
    RUN_TEST(test_w25q64_read_write);
    RUN_TEST(test_w25q64_erase);
    RUN_TEST(test_w25q64_erase_alignment);
    RUN_TEST(test_w25q64_status);
    RUN_TEST(test_w25q64_wait_ready);
    
    return UNITY_END();
}
