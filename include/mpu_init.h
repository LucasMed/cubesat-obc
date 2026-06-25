/**
 * @file mpu_init.h
 * @brief MPU initialization for critical data protection.
 *
 * Configures the Cortex-M33 MPU before the FreeRTOS scheduler starts.
 * Region indices and attribute definitions for 6-region layout.
 *
 * Spec ref: Golden Image SDD, mpu-config/spec.md
 */

#ifndef MPU_INIT_H
#define MPU_INIT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ------------------------------------------------------------------ */
/* MPU Region indices                                                   */
/* ------------------------------------------------------------------ */
#define MPU_REGION_FLASH 0      /**< Flash text: read-only, executable       */
#define MPU_REGION_SRAM 1       /**< SRAM: read/write, no-exec               */
#define MPU_REGION_PERIPH 2     /**< Peripherals: privileged-only, no-exec   */
#define MPU_REGION_RESERVED_3 3 /**< Reserved for g_snapshot protection      */
#define MPU_REGION_RESERVED_4 4 /**< Reserved for g_table protection         */
#define MPU_REGION_RESERVED_5 5 /**< Reserved for FMM globals protection     */

  /* ------------------------------------------------------------------ */
  /* Public API                                                           */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialize the MPU with static region configuration.
   *
   * Must be called from obc_main() BEFORE vTaskStartScheduler().
   * Configures 6 regions (3 active + 3 reserved) with memory attributes.
   * Once enabled, MPU regions are not changed at runtime.
   */
  void mpu_init(void);

#ifdef __cplusplus
}
#endif

#endif /* MPU_INIT_H */
