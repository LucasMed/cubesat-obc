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
 * @brief Main payload task loop.
 */
void vPayloadTask(void *pvParameters);

/**
 * @brief Notification values for payload task.
 */
#define PAYLOAD_NOTIFY_CAPTURE_IMAGE (1 << 0)
#define PAYLOAD_NOTIFY_HEARTBEAT (1 << 1)

#endif  // PAYLOAD_TASK_H
