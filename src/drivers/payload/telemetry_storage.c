/**
 * @file telemetry_storage.c
 * @brief Telemetry storage implementation using W25Q64 SPI flash
 */

#include "telemetry_storage.h"
#include "w25q64.h"

#include <string.h>

#ifdef PICO_BUILD
  #include <stdio.h>

  /* Storage layout for 8MB W25Q64 */
  #define TELEMETRY_BASE_ADDR 0x000000     /**< Telemetry log: 0 - 1MB */
  #define TELEMETRY_MAX_ADDR 0x0FFFFF
  #define TELEMETRY_RECORD_SIZE 64        /**< 64 bytes per record */
  #define TELEMETRY_MAX_RECORDS (1024 * 1024 / TELEMETRY_RECORD_SIZE)  /**< ~16384 records */

  static bool s_initialized = false;
  static uint32_t s_current_addr = TELEMETRY_BASE_ADDR;
  static uint32_t s_records_written = 0;
  static uint32_t s_last_sequence = 0;
#endif

bool telemetry_storage_init(void)
{
#ifdef PICO_BUILD
  if (s_initialized)
  {
    return true;
  }

  /* Initialize W25Q64 flash */
  if (w25q64_init() != W25Q64_OK)
  {
    printf("telemetry_storage: Failed to initialize W25Q64\n");
    return false;
  }

  /* Find last written address by scanning for non-0xFF bytes */
  uint8_t check_buf[TELEMETRY_RECORD_SIZE];
  s_current_addr = TELEMETRY_BASE_ADDR;
  
  while (s_current_addr < TELEMETRY_MAX_ADDR)
  {
    if (w25q64_read(s_current_addr, check_buf, TELEMETRY_RECORD_SIZE) != W25Q64_OK)
    {
      break;
    }
    
    /* Check if page is empty (first byte = 0xFF means erased) */
    if (check_buf[0] == 0xFF)
    {
      break;
    }
    
    s_current_addr += TELEMETRY_RECORD_SIZE;
  }
  
  s_records_written = (s_current_addr - TELEMETRY_BASE_ADDR) / TELEMETRY_RECORD_SIZE;
  s_initialized = true;
  
  printf("telemetry_storage: Initialized at 0x%08X, %lu records stored\n", 
         (unsigned int)s_current_addr, (unsigned long)s_records_written);
  
  return true;
#else
  return true;
#endif
}

bool telemetry_storage_store(const telemetry_record_t *record)
{
#ifdef PICO_BUILD
  if (!s_initialized || record == NULL)
  {
    return false;
  }

  /* Check if storage is full */
  if (s_current_addr >= TELEMETRY_MAX_ADDR)
  {
    /* Wrap around to beginning */
    s_current_addr = TELEMETRY_BASE_ADDR;
    s_records_written = 0;
  }

  /* Update sequence number */
  telemetry_record_t rec = *record;
  rec.sequence = ++s_last_sequence;

  /* Erase sector if starting new sector (4KB boundary) */
  uint32_t sector_start = s_current_addr - (s_current_addr % 4096);
  if (s_current_addr % 4096 == 0)
  {
    if (w25q64_erase_sector(sector_start) != W25Q64_OK)
    {
      printf("telemetry_storage: Failed to erase sector at 0x%08X\n", 
             (unsigned int)sector_start);
      return false;
    }
  }

  /* Write record */
  uint8_t buf[TELEMETRY_RECORD_SIZE];
  memcpy(buf, &rec, sizeof(rec));
  
  if (w25q64_write_page(s_current_addr, buf, TELEMETRY_RECORD_SIZE) != W25Q64_OK)
  {
    printf("telemetry_storage: Failed to write at 0x%08X\n", 
           (unsigned int)s_current_addr);
    return false;
  }

  s_current_addr += TELEMETRY_RECORD_SIZE;
  s_records_written++;
  
  return true;
#else
  (void)record;
  return true;
#endif
}

bool telemetry_storage_read(uint32_t address, telemetry_record_t *record)
{
#ifdef PICO_BUILD
  if (!s_initialized || record == NULL)
  {
    return false;
  }
  
  if (address >= TELEMETRY_MAX_ADDR)
  {
    return false;
  }

  uint8_t buf[TELEMETRY_RECORD_SIZE];
  if (w25q64_read(address, buf, TELEMETRY_RECORD_SIZE) != W25Q64_OK)
  {
    return false;
  }

  memcpy(record, buf, sizeof(telemetry_record_t));
  return true;
#else
  (void)address;
  (void)record;
  return true;
#endif
}

void telemetry_storage_get_stats(telemetry_storage_stats_t *stats)
{
#ifdef PICO_BUILD
  if (stats != NULL)
  {
    stats->records_written = s_records_written;
    stats->current_address = s_current_addr;
    stats->last_sequence = s_last_sequence;
    stats->initialized = s_initialized;
  }
#else
  if (stats != NULL)
  {
    memset(stats, 0, sizeof(telemetry_storage_stats_t));
    stats->initialized = true;
  }
#endif
}

uint32_t telemetry_storage_available(void)
{
#ifdef PICO_BUILD
  return TELEMETRY_MAX_RECORDS - s_records_written;
#else
  return 0;
#endif
}

bool telemetry_storage_is_available(void)
{
#ifdef PICO_BUILD
  return s_initialized;
#else
  return true;
#endif
}

bool telemetry_storage_clear(void)
{
#ifdef PICO_BUILD
  if (!s_initialized)
  {
    return false;
  }

  /* Erase entire telemetry region (1MB) in 64KB blocks */
  for (uint32_t addr = TELEMETRY_BASE_ADDR; addr < TELEMETRY_MAX_ADDR; addr += 65536)
  {
    if (w25q64_erase_block64(addr) != W25Q64_OK)
    {
      printf("telemetry_storage: Failed to erase block at 0x%08X\n", (unsigned int)addr);
      return false;
    }
  }

  s_current_addr = TELEMETRY_BASE_ADDR;
  s_records_written = 0;
  s_last_sequence = 0;
  
  printf("telemetry_storage: Storage cleared\n");
  return true;
#else
  return true;
#endif
}

uint32_t telemetry_storage_get_base_addr(void)
{
  return 0x000000;
}
