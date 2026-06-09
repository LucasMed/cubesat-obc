/**
 * @file freertos_mock.h
 * @brief Shared FreeRTOS mock macros for unit tests.
 *
 * Extracts the per-file duplicated `mock_vTaskDelayUntil` / `mock_xTaskGetTickCount`
 * pattern into a single header.
 *
 * Usage in test files:
 *   1. #include "mocks/freertos_mock.h"
 *   2. Call mock_freertos_reset() in setUp() / reset_all().
 *
 * The host FreeRTOS.h already defines vTaskDelayUntil / xTaskGetTickCount
 * with #ifndef guards, so defining the mock macro BEFORE including
 * FreeRTOS.h overrides the stub.  In test files you may need to:
 *
 *   #include "mocks/freertos_mock.h"
 *   // ... then include the source-under-test
 *
 * Or if the source includes FreeRTOS.h internally, redefine after inclusion:
 *
 *   #ifdef mock_vTaskDelayUntil
 *     #undef vTaskDelayUntil
 *     #define vTaskDelayUntil mock_vTaskDelayUntil
 *   #endif
 */

#ifndef FREERTOS_MOCK_H
#define FREERTOS_MOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Mock state — shared across all test files                           */
  /* ------------------------------------------------------------------ */

  /** Tick count returned by mock_xTaskGetTickCount. */
  extern volatile uint32_t mock_freertos_tick_count;

  /** Decrement this counter to make vTaskDelayUntil skip a wake cycle. */
  extern volatile int mock_freertos_delay_until_skip_count;

  /* ------------------------------------------------------------------ */
  /* Mock implementations (defined in .c file or via header-only macro)  */
  /* ------------------------------------------------------------------ */

  /**
   * Overridable xTaskGetTickCount.
   * Test files that #define xTaskGetTickCount mock_xTaskGetTickCount
   * will call this instead of the host stub.
   */
  uint32_t mock_xTaskGetTickCount(void);

  /**
   * Overridable vTaskDelayUntil.
   * Mimics the real FreeRTOS macro: updates *pxPreviousWakeTime
   * by xTimeIncrement, unless mock_freertos_delay_until_skip_count > 0.
   */
  void mock_vTaskDelayUntil(uint32_t *pxPreviousWakeTime, uint32_t xTimeIncrement);

  /* ------------------------------------------------------------------ */
  /* Reset helper                                                        */
  /* ------------------------------------------------------------------ */

  /** Reset all FreeRTOS mock state to defaults. */
  void mock_freertos_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERTOS_MOCK_H */
