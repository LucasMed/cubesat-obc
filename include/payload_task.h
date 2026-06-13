/**
 * @file payload_task.h
 * @brief FreeRTOS task for payload operations.
 */

#ifndef PAYLOAD_TASK_H
#define PAYLOAD_TASK_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief Initialize and start the payload task.
 */
void payload_task_init(void);

/**
 * @brief Main loop of the payload task.
 */
void vPayloadTask(void *pvParameters);

/**
 * @brief Single iteration of the payload task (for testing).
 */
void vPayloadTask_Step(void);

/**
 * @brief Reset internal task state (for testing).
 */
void payload_task_reset(void);

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Notification values for payload task.
 */
#define PAYLOAD_NOTIFY_CAPTURE_IMAGE (1 << 0)
#define PAYLOAD_NOTIFY_HEARTBEAT (1 << 1)
#define PAYLOAD_NOTIFY_DUMP_IMAGE (1 << 2)

#ifdef __cplusplus
}
#endif

#endif  // PAYLOAD_TASK_H
