#ifndef MAGNETORQUER_H
#define MAGNETORQUER_H

typedef struct {
    float moment[3];
} magnetorquer_t;

void magnetorquer_init(magnetorquer_t *mq);
void magnetorquer_set_moment(magnetorquer_t *mq, float mx, float my, float mz);

#endif // MAGNETORQUER_H
