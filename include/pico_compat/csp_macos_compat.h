#ifndef CSP_MACOS_COMPAT_H
#define CSP_MACOS_COMPAT_H

#include <stdint.h>

#ifndef PICO_COMPAT
  #define PICO_COMPAT 1
#endif

#ifndef PICO_RP2040
  #define PICO_RP2040 0
#endif

#ifndef PICO_RP2350
  #define PICO_RP2350 0
#endif

#ifndef PICO_ON_DEVICE
  #define PICO_ON_DEVICE 0
#endif

#ifndef PICO_NO_HARDWARE
  #define PICO_NO_HARDWARE 0
#endif

#ifndef PICO_BUILD
  #define PICO_BUILD 0
#endif

#ifndef __weak
  #define __weak __attribute__((weak))
#endif

#ifndef PICO_STATIC_ASSERT
  #define PICO_STATIC_ASSERT(x, msg) _Static_assert(x, msg)
#endif

#ifndef __noinit
  #define __noinit
#endif

/* macOS does not provide sem_timedwait() (POSIX option).
 * Provide a simple poll-based fallback so libcsp's POSIX arch compiles. */
#include <time.h>
#include <semaphore.h>
#include <errno.h>

static inline int csp_macos_sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
  struct timespec now;
  for (;;)
  {
    int ret = sem_trywait(sem);
    if (ret == 0)
    {
      return 0;
    }
    if (errno != EAGAIN)
    {
      return -1;
    }
    clock_gettime(CLOCK_REALTIME, &now);
    if (now.tv_sec > abs_timeout->tv_sec
        || (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec))
    {
      errno = ETIMEDOUT;
      return -1;
    }
    struct timespec sleep = {0, 1000000}; /* 1 ms */
    nanosleep(&sleep, NULL);
  }
}
#define sem_timedwait(sem, ts) csp_macos_sem_timedwait((sem), (ts))

#endif  // CSP_MACOS_COMPAT_H
