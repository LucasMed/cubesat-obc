
#include "magnetorquer.h"

#include "config.h"

#include <stdio.h>

void magnetorquer_init(magnetorquer_t *mq)
{
  mq->moment[0] = mq->moment[1] = mq->moment[2] = 0.0f;
}

void magnetorquer_set_moment(magnetorquer_t *mq, float mx, float my, float mz)
{
  mq->moment[0] = mx;
  mq->moment[1] = my;
  mq->moment[2] = mz;
}
