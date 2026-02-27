#ifndef ATTITUDE_CONTROL_H
#define ATTITUDE_CONTROL_H

#include "pid.h"

typedef struct {
    pid_ctrl_t pid_roll, pid_pitch, pid_yaw;
} attitude_ctrl_t;

void attitude_ctrl_init(attitude_ctrl_t *ac);
void attitude_ctrl_update(attitude_ctrl_t *ac, const float target[3], const float current[3], const float rates[3], float outputs[3], float dt);

#endif // ATTITUDE_CONTROL_H
