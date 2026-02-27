#ifndef PID_H
#define PID_H

typedef struct {
    float kp, ki, kd;
    float integral;
    float last_error;
} pid_ctrl_t;

void pid_init(pid_ctrl_t *p, float kp, float ki, float kd);
float pid_update(pid_ctrl_t *p, float error, float dt);

#endif // PID_H
