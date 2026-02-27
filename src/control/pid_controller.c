#include "config.h"
#include "pid.h"
#include <stdio.h>

void pid_init(pid_ctrl_t *p, float kp, float ki, float kd) {
    p->kp=kp; p->ki=ki; p->kd=kd; p->integral=0.0f; p->last_error=0.0f;
}

float pid_update(pid_ctrl_t *p, float error, float dt) {
    p->integral += error * dt;
    float derivative = 0.0f;
    if (dt > 0.0f) derivative = (error - p->last_error) / dt;
    float out = p->kp * error + p->ki * p->integral + p->kd * derivative;
    p->last_error = error;
    return out;
}
