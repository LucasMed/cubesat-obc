#ifndef PICO_COMPAT_ENDIAN_H
#define PICO_COMPAT_ENDIAN_H

#include <stdint.h>

/* Setup basic endian defines if they don't exist */
#ifndef __LITTLE_ENDIAN
  #define __LITTLE_ENDIAN 1234
#endif
#ifndef __BIG_ENDIAN
  #define __BIG_ENDIAN 4321
#endif

#ifndef __BYTE_ORDER
  #ifdef __BYTE_ORDER__
    #define __BYTE_ORDER __BYTE_ORDER__
  #else
    /* Fallback to little endian for ARM Cortex-M */
    #define __BYTE_ORDER __LITTLE_ENDIAN
  #endif
#endif

/* Byte swap functions */
#if __BYTE_ORDER == __LITTLE_ENDIAN
  #ifndef htobe16
    #define htobe16(x) __builtin_bswap16(x)
  #endif
  #ifndef htole16
    #define htole16(x) (x)
  #endif
  #ifndef be16toh
    #define be16toh(x) __builtin_bswap16(x)
  #endif
  #ifndef le16toh
    #define le16toh(x) (x)
  #endif

  #ifndef htobe32
    #define htobe32(x) __builtin_bswap32(x)
  #endif
  #ifndef htole32
    #define htole32(x) (x)
  #endif
  #ifndef be32toh
    #define be32toh(x) __builtin_bswap32(x)
  #endif
  #ifndef le32toh
    #define le32toh(x) (x)
  #endif

  #ifndef htobe64
    #define htobe64(x) __builtin_bswap64(x)
  #endif
  #ifndef htole64
    #define htole64(x) (x)
  #endif
  #ifndef be64toh
    #define be64toh(x) __builtin_bswap64(x)
  #endif
  #ifndef le64toh
    #define le64toh(x) (x)
  #endif

#else
  /* Big Endian */
  #ifndef htobe16
    #define htobe16(x) (x)
  #endif
  #ifndef htole16
    #define htole16(x) __builtin_bswap16(x)
  #endif
  #ifndef be16toh
    #define be16toh(x) (x)
  #endif
  #ifndef le16toh
    #define le16toh(x) __builtin_bswap16(x)
  #endif

  #ifndef htobe32
    #define htobe32(x) (x)
  #endif
  #ifndef htole32
    #define htole32(x) __builtin_bswap32(x)
  #endif
  #ifndef be32toh
    #define be32toh(x) (x)
  #endif
  #ifndef le32toh
    #define le32toh(x) __builtin_bswap32(x)
  #endif

  #ifndef htobe64
    #define htobe64(x) (x)
  #endif
  #ifndef htole64
    #define htole64(x) __builtin_bswap64(x)
  #endif
  #ifndef be64toh
    #define be64toh(x) (x)
  #endif
  #ifndef le64toh
    #define le64toh(x) __builtin_bswap64(x)
  #endif
#endif

#endif
