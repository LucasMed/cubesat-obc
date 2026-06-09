/**
 * @file freertos_mock.c
 * @brief Shared FreeRTOS mock implementations for unit tests.
 *
 * Provides globally-visible mock implementations of xTaskGetTickCount
 * and vTaskDelayUntil that can be linked into any test executable.
 * See freertos_mock.h for usage.
 */
#include "mocks/freertos_mock.h"

/* ------------------------------------------------------------------ */
/* Mock state                                                          */
/* ------------------------------------------------------------------ */

volatile uint32_t mock_freertos_tick_count = 0;
volatile int mock_freertos_delay_until_skip_count = 0;

/* ------------------------------------------------------------------ */
/* Mock implementations                                                */
/* ------------------------------------------------------------------ */

uint32_t mock_xTaskGetTickCount(void)
{
  return mock_freertos_tick_count;
}

void mock_vTaskDelayUntil(uint32_t *pxPreviousWakeTime, uint32_t xTimeIncrement)
{
  if (mock_freertos_delay_until_skip_count > 0)
  {
    mock_freertos_delay_until_skip_count--;
  }
  else
  {
    *pxPreviousWakeTime += xTimeIncrement;
  }
}

/* ------------------------------------------------------------------ */
/* Reset                                                               */
/* ------------------------------------------------------------------ */

void mock_freertos_reset(void)
{
  mock_freertos_tick_count = 0;
  mock_freertos_delay_until_skip_count = 0;
}
