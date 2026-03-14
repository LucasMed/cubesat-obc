// Minimal stub for Unity test framework for host builds
#ifndef UNITY_H
#define UNITY_H
#define UNITY_BEGIN() 0
#define UNITY_END() 0
#define RUN_TEST(func) func()
#define TEST_ASSERT_TRUE(x)
#define TEST_ASSERT_FALSE(x)
#define TEST_ASSERT_FLOAT_WITHIN(delta, expect, actual)
#define TEST_ASSERT_GREATER_THAN(a, b)
#define TEST_ASSERT_EQUAL_HEX8(expect, actual)
#define TEST_ASSERT_EQUAL_UINT32(expect, actual)
#define TEST_ASSERT_EQUAL_INT(expect, actual)
#endif  // UNITY_H
