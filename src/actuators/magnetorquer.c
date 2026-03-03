
#include "magnetorquer.h"

#include "config.h"

void magnetorquer_init(magnetorquer_t *mq)
{
  mq->moment[0] = 0.0f;
  mq->moment[1] = 0.0f;
  mq->moment[2] = 0.0f;
}

void magnetorquer_set_moment(magnetorquer_t *mq, float mx, float my, float mz)
{
  mq->moment[0] = mx;
  mq->moment[1] = my;
  mq->moment[2] = mz;
}
