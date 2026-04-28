// Sensor Read Task - reads IMU and other sensors
#ifndef SENSOR_READ_TASK_H
#define SENSOR_READ_TASK_H

#include "ekf.h"

#include <stdbool.h>

void vSensorReadTask(void *pvParameters);
void vSensorReadTask_Step(void);

extern ekf_t s_ekf;
extern bool s_ekf_initialised;

#endif  // SENSOR_READ_TASK_H
