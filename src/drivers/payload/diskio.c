/**
 * @file diskio.c
 * @brief Bridge between FatFs and sd_spi driver.
 */

#include "ff.h"
#include "diskio.h"

#include "sd_spi.h"

#include <string.h>

/* Physical drive number to identify the drive */
#define DEV_SD 0

DSTATUS disk_status(BYTE pdrv)
{
  if (pdrv != DEV_SD)
  {
    return STA_NOINIT;
  }
  return (sd_spi_get_type() == SD_TYPE_UNKNOWN) ? STA_NOINIT : 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{
  if (pdrv != DEV_SD)
  {
    return STA_NOINIT;
  }
  return sd_spi_init() ? 0 : STA_NOINIT;
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
  if (pdrv != DEV_SD || !count)
  {
    return RES_PARERR;
  }

  for (UINT i = 0; i < count; i++)
  {
    if (!sd_spi_read_sector((uint32_t)sector + i, buff + ((size_t)i * 512)))
    {
      return RES_ERROR;
    }
  }
  return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
  if (pdrv != DEV_SD || !count)
  {
    return RES_PARERR;
  }

  for (UINT i = 0; i < count; i++)
  {
    if (!sd_spi_write_sector((uint32_t)sector + i, buff + ((size_t)i * 512)))
    {
      return RES_ERROR;
    }
  }
  return RES_OK;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  if (pdrv != DEV_SD)
  {
    return RES_PARERR;
  }

  switch (cmd)
  {
  case CTRL_SYNC:
    return RES_OK;
  case GET_SECTOR_COUNT:
    /* Placeholder: return a fixed size or query card (CSD) */
    *(LBA_t *)buff = 2000000; /* ~1GB */
    return RES_OK;
  case GET_SECTOR_SIZE:
    *(WORD *)buff = 512;
    return RES_OK;
  case GET_BLOCK_SIZE:
    *(DWORD *)buff = 1;
    return RES_OK;
  }
  return RES_PARERR;
}

/**
 * @brief Provide system time for FatFs file timestamps.
 */
DWORD get_fattime(void)
{
  /* Placeholder: Return 2026-03-14 15:00:00 */
  return ((DWORD)(2026 - 1980) << 25) | ((DWORD)3 << 21) | ((DWORD)14 << 16) | ((DWORD)15 << 11) |
         ((DWORD)0 << 5) | ((DWORD)0 >> 1);
}
