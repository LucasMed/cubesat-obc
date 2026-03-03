/**
 * @file flash_backend_stub.c
 * @brief Host-build stub for flash_backend_flush().
 *
 * When building for the host (PICO_ENABLED=OFF / PICO_BUILD not defined)
 * this stub satisfies the flash_backend_flush() symbol by appending raw
 * log records to /tmp/obc_log.bin.
 *
 * This file must NOT be compiled for Pico targets; the real Pico
 * implementation is expected in a future phase and uses
 * flash_range_program() from the Pico SDK.
 *
 * Spec ref: SPEC-2-PLG v1.16 §5.3
 */

#ifndef PICO_BUILD

  #include "flash_backend.h"

/* cppcheck-suppress misra-c2012-21.6 -- MISRA deviation: stdio fopen/fwrite
 * are used exclusively in this host-only stub; not compiled for flight targets
 * (guarded by #ifndef PICO_BUILD). See MISRA_DEVIATIONS.md §21.6-D2. */
  #include <stdio.h>

  #define FLASH_LOG_PATH "/tmp/obc_log.bin"

/**
 * Append @p len bytes of @p buf to the host log file.
 * Creates the file on first write; subsequent calls append.
 */
void flash_backend_flush(const uint8_t *buf, size_t len)
{
  if ((buf == NULL) || (len == 0u))
  {
    return;
  }

  FILE *fp = fopen(FLASH_LOG_PATH, "ab");
  if (fp != NULL)
  {
    (void)fwrite(buf, 1u, len, fp);
    (void)fclose(fp);
  }
}

#endif /* !PICO_BUILD */
