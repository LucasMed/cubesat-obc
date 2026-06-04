/**
 * @file flash_layout.h
 * @brief W25Q64 Flash Memory Map — single source of truth.
 *
 * All persistent storage regions in the W25Q64 external flash MUST
 * be documented here to prevent overlapping allocations.
 *
 * W25Q64: 64 Mbit = 8 388 608 bytes (8 MB)
 * Erase sector size: 4096 bytes (4 KB)
 *
 * Spec ref: FMM-SPEC-093 (deploy-automation)
 */

#ifndef FLASH_LAYOUT_H
#define FLASH_LAYOUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Flash geometry                                                      */
  /* ------------------------------------------------------------------ */

#define FLASH_SIZE_BYTES (8u * 1024u * 1024u) /* 8 MB */
#define FLASH_SECTOR_SIZE 4096u               /* 4 KB erase sector */

  /* ------------------------------------------------------------------ */
  /* Region assignments                                                  */
  /*                                                                     */
  /* All offsets are byte addresses from the flash base (0x000000).      */
  /* Regions MUST NOT overlap — add compile-time assertions if needed.   */
  /* ------------------------------------------------------------------ */

  /* 0x000000 – 0x6FFFFF: application image, telemetry storage,          */
  /*                       configuration, calibration, fault logs        */
  /*                       (reserved for existing / future consumers)    */

  /* 0x700000 – 0x700FFF: POST ring buffer (4 KB)                       */
#define FLASH_SECTOR_POST 0x700000u

  /* 0x7F0000 – 0x7F0FFF: IMU calibration data (4 KB)                  */
#define FLASH_SECTOR_IMU_CALIB 0x7F0000u

  /* 0x7F1000 – 0x7FFFFF: unallocated (reserved)                        */

  /* ------------------------------------------------------------------ */
  /* Compile-time overlap assertions                                     */
  /* ------------------------------------------------------------------ */

  _Static_assert(FLASH_SECTOR_POST + 0x1000u <= FLASH_SECTOR_IMU_CALIB,
                 "flash_layout: POST region overlaps IMU calibration region");
  _Static_assert(FLASH_SECTOR_IMU_CALIB + 0x1000u <= FLASH_SIZE_BYTES,
                 "flash_layout: IMU calibration region exceeds flash size");

#ifdef __cplusplus
}
#endif

#endif /* FLASH_LAYOUT_H */
