/**
 * @file gps_task.h
 * @brief GPS polling task — reads GPS fix via UART and updates DLA.
 *
 * The per-iteration logic lives in vGpsTask_Step() so unit tests can
 * exercise it without running the FreeRTOS while(1) loop.
 */
#ifndef GPS_TASK_H
#define GPS_TASK_H

#include <stdint.h>

/**
 * @brief One iteration of the GPS polling logic.
 *
 * @param no_data_count  [in/out] Consecutive polls with no new data.
 * @param prev_timestamp [in/out] Timestamp of the last processed fix.
 */
void vGpsTask_Step(uint32_t *no_data_count, uint32_t *prev_timestamp);

#endif /* GPS_TASK_H */
