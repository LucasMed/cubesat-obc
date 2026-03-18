/**
 * @file storage_manager.c
 * @brief Storage Manager implementation using FatFs.
 *
 * Handles mounting, directory creation, and file operations
 * for payload logs and images.
 */

#include "storage_manager.h"

#if defined(PICO_BUILD)
  #include "ff.h"
#else
  /* Host Mocks: simulate FatFs with standard C library */
  #include <errno.h>
  #include <stdio.h>
  #include <sys/stat.h>
  #include <sys/types.h>

typedef int FRESULT;
  #define FR_OK 0
  #define FA_OPEN_APPEND 0x30
  #define FA_WRITE 0x02
  #define FA_CREATE_ALWAYS 0x08
#endif

#include <string.h>

#if defined(PICO_BUILD)
static FATFS s_fs;
#endif

storage_status_t storage_init(void)
{
#if defined(PICO_BUILD)
  FRESULT res = f_mount(&s_fs, "", 1);
  if (res != FR_OK)
  {
    return STORAGE_ERR_MOUNT;
  }

  /* Create base directories */
  f_mkdir("/LOGS");
  f_mkdir("/IMAGES");
#else
  /* Host: Create directories if they don't exist */
  if (mkdir("LOGS", 0777) == -1 && errno != EEXIST)
  {
    perror("mkdir LOGS");
  }
  if (mkdir("IMAGES", 0777) == -1 && errno != EEXIST)
  {
    perror("mkdir IMAGES");
  }
#endif
  return STORAGE_OK;
}

storage_status_t storage_append_log(const char *filename, const void *data, size_t size)
{
  if (!filename || !data || size == 0)
  {
    return STORAGE_ERR_WRITE;
  }

#if defined(PICO_BUILD)
  FIL file;
  FRESULT res = f_open(&file, filename, FA_WRITE | FA_OPEN_APPEND);
  if (res != FR_OK)
  {
    return STORAGE_ERR_OPEN;
  }

  UINT written;
  res = f_write(&file, data, (UINT)size, &written);
  f_close(&file);

  return (res == FR_OK && written == size) ? STORAGE_OK : STORAGE_ERR_WRITE;
#else
  /* Host implementation using fopen */
  const char *path = (filename[0] == '/') ? &filename[1] : filename;

  FILE *f = fopen(path, "ab");
  if (!f)
  {
    return STORAGE_ERR_OPEN;
  }
  size_t count = fwrite(data, 1, size, f);
  fclose(f);
  return (count == size) ? STORAGE_OK : STORAGE_ERR_WRITE;
#endif
}

storage_status_t storage_write_image(const char *filename, const uint8_t *data, uint32_t size)
{
  if (!filename || !data || size == 0)
  {
    return STORAGE_ERR_WRITE;
  }

#if defined(PICO_BUILD)
  FIL file;
  FRESULT res = f_open(&file, filename, FA_WRITE | FA_CREATE_ALWAYS);
  if (res != FR_OK)
  {
    return STORAGE_ERR_OPEN;
  }

  UINT written;
  res = f_write(&file, data, (UINT)size, &written);
  f_close(&file);

  return (res == FR_OK && written == size) ? STORAGE_OK : STORAGE_ERR_WRITE;
#else
  /* Host implementation using fopen */
  const char *path = (filename[0] == '/') ? &filename[1] : filename;

  FILE *f = fopen(path, "wb");
  if (!f)
  {
    return STORAGE_ERR_OPEN;
  }
  size_t count = fwrite(data, 1, size, f);
  fclose(f);
  return (count == size) ? STORAGE_OK : STORAGE_ERR_WRITE;
#endif
}

uint64_t storage_get_free_space(void)
{
#if defined(PICO_BUILD)
  FATFS *fs;
  DWORD free_clusters;
  if (f_getfree("", &free_clusters, &fs) != FR_OK)
  {
    return 0;
  }
  return (uint64_t)free_clusters * fs->csize * 512;
#else
  return (uint64_t)1024 * 1024 * 1024; /* 1 GB mock */
#endif
}
