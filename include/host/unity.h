// Minimal stub for Unity test framework for host builds
#ifndef UNITY_H
#define UNITY_H
#define UNITY_BEGIN() 0
#define UNITY_END() 0
extern void setUp(void);
extern void tearDown(void);
#define RUN_TEST(func)                                                                             \
  {                                                                                                \
    setUp();                                                                                       \
    func();                                                                                        \
    tearDown();                                                                                    \
  }
#include <stdio.h>
#include <stdlib.h>

#define TEST_ASSERT_TRUE(x)                                                                        \
  if (!(x))                                                                                        \
  {                                                                                                \
    fprintf(stderr, "ASSERT TRUE FAIL at %s:%d\n", __FILE__, __LINE__);                            \
    exit(1);                                                                                       \
  }
#define TEST_ASSERT_FALSE(x)                                                                       \
  if (x)                                                                                           \
  {                                                                                                \
    fprintf(stderr, "ASSERT FALSE FAIL at %s:%d\n", __FILE__, __LINE__);                           \
    exit(1);                                                                                       \
  }
#define TEST_ASSERT_FLOAT_WITHIN(d, e, a)
#define TEST_ASSERT_GREATER_THAN(a, b)                                                             \
  if (!((a) > (b)))                                                                                \
  {                                                                                                \
    fprintf(stderr, "ASSERT GREATER FAIL at %s:%d\n", __FILE__, __LINE__);                         \
    exit(1);                                                                                       \
  }
#define TEST_ASSERT_EQUAL_HEX8(e, a)                                                               \
  if ((e) != (a))                                                                                  \
  {                                                                                                \
    fprintf(stderr, "ASSERT HEX8 FAIL: expected 0x%02X, got 0x%02X at %s:%d\n", (e), (a),          \
            __FILE__, __LINE__);                                                                   \
    exit(1);                                                                                       \
  }
#define TEST_ASSERT_EQUAL_UINT32(e, a)                                                             \
  if ((e) != (a))                                                                                  \
  {                                                                                                \
    fprintf(stderr, "ASSERT UINT32 FAIL: expected %u, got %u at %s:%d\n", (uint32_t)(e),           \
            (uint32_t)(a), __FILE__, __LINE__);                                                    \
    exit(1);                                                                                       \
  }
#define TEST_ASSERT_EQUAL_INT(e, a)                                                                \
  if ((e) != (a))                                                                                  \
  {                                                                                                \
    fprintf(stderr, "ASSERT INT FAIL: expected %d, got %d at %s:%d\n", (int)(e), (int)(a),         \
            __FILE__, __LINE__);                                                                   \
    exit(1);                                                                                       \
  }
#define TEST_ASSERT_NOT_NULL(x)                                                                    \
  if ((x) == NULL)                                                                                 \
  {                                                                                                \
    fprintf(stderr, "ASSERT NOT NULL FAIL at %s:%d\n", __FILE__, __LINE__);                        \
    exit(1);                                                                                       \
  }
#define TEST_ASSERT_EQUAL_PTR(a, b)                                                                \
  if ((a) != (b))                                                                                  \
  {                                                                                                \
    fprintf(stderr, "ASSERT PTR FAIL at %s:%d\n", __FILE__, __LINE__);                             \
    exit(1);                                                                                       \
  }
#endif  // UNITY_H
