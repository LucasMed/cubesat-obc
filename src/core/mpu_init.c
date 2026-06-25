/**
 * @file mpu_init.c
 * @brief Cortex-M33 MPU initialization for the CubeSat OBC.
 *
 * Configures 6 MPU regions (3 active + 3 reserved) before the FreeRTOS
 * scheduler starts.  Provides:
 *   - Flash read-only (no accidental corruption)
 *   - SRAM full access (note: XN=0 required on RP2350 — see region 1)
 *   - Peripherals privileged-only (user tasks can't escape)
 *
 * This is the *bare-metal* MPU, not the FreeRTOS MPU port
 * (configENABLE_MPU=0).  PRIVDEFENA is enabled so privileged code
 * retains default R/W access to any unmapped region.
 *
 * Hardware: RP2350 (Cortex-M33), ARMv8-M MPU, 8 hardware regions.
 *
 * Spec ref: mpu-config/spec.md, OBC-DES-001 §4.5.2
 * CDR ref:  CDR-SW-09, CDR-SW-12
 */

#include "mpu_init.h"

#ifdef PICO_BUILD
  #include "hardware/structs/mpu.h"

  /* ------------------------------------------------------------------ */
  /* Barrier helpers (GCC/Clang builtins for ARM)                        */
  /* ------------------------------------------------------------------ */
  #define MPU_DMB() __asm volatile("dmb" ::: "memory")
  #define MPU_DSB() __asm volatile("dsb" ::: "memory")
  #define MPU_ISB() __asm volatile("isb" ::: "memory")

  /* ------------------------------------------------------------------ */
  /* MAIR attribute encodings for ARMv8-M                                */
  /* ------------------------------------------------------------------ */
  /*
   * AttrIdx 0 — Normal memory, Outer+Inner Write-Back, Read+Write Allocate
   * 0xFF = 0b1111_1111: outer=0xF, inner=0xF
   */
  #define MAIR_ATTR_NORMAL_WBWA 0xFFu

  /*
   * AttrIdx 1 — Device memory nGnRE
   * 0x04 = Device, non-Gathering, non-Reordering, Early Write Ack
   */
  #define MAIR_ATTR_DEVICE_nGnRE 0x04u

  /* ------------------------------------------------------------------ */
  /* Region base / limit helpers                                         */
  /* ------------------------------------------------------------------ */
  #define MPU_ALIGN_32B(x) ((x) & ~0x1Fu)
  #define MPU_RBAR_VAL(base, sh, ap, xn)                                                           \
    (((base)&M33_MPU_RBAR_BASE_BITS) |                                                             \
     (((uint32_t)(sh) << M33_MPU_RBAR_SH_LSB) | ((uint32_t)(ap) << M33_MPU_RBAR_AP_LSB) |          \
      ((uint32_t)(xn) << M33_MPU_RBAR_XN_LSB)))

  #define MPU_RLAR_VAL(limit, attr_idx)                                                            \
    ((MPU_ALIGN_32B(limit) & M33_MPU_RLAR_LIMIT_BITS) |                                            \
     (((uint32_t)(attr_idx) << M33_MPU_RLAR_ATTRINDX_LSB) | M33_MPU_RLAR_EN_BITS))

/* Static region configuration table */
typedef struct
{
  uint32_t base;
  uint32_t limit;
  uint32_t sh;      /* shareability: 0=non, 2=outer, 3=inner */
  uint8_t ap;       /* access permissions: 0=full, 1=priv, 2=RO, 3=RO-priv */
  uint8_t xn;       /* execute-never: 0=exec, 1=no-exec */
  uint8_t attr_idx; /* MAIR attribute index */
} mpu_region_cfg_t;

static const mpu_region_cfg_t s_region_cfg[6] = {
    /* 0: Flash — RO for all, executable */
    {.base = 0x10000000u, .limit = 0x103FFFFFu, .sh = 0, .ap = 2, .xn = 0, .attr_idx = 0},
    /* 1: SRAM — R/W full, exec (XN=0 required on RP2350 — speculative
       prefetch causes MemManage fault when XN=1 on SRAM) */
    {.base = 0x20000000u, .limit = 0x2007FFFFu, .sh = 0, .ap = 0, .xn = 0, .attr_idx = 0},
    /* 2: Peripherals — privileged-only, no-exec */
    {.base = 0x40000000u, .limit = 0x400FFFFFu, .sh = 0, .ap = 1, .xn = 1, .attr_idx = 1},
    /* 3–5: Reserved — not yet configured */
};

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void mpu_init(void)
{
  /* ── Disable MPU before reconfiguration ────────────────────── */
  MPU_DMB();
  hw_clear_bits(&mpu_hw->ctrl, M33_MPU_CTRL_ENABLE_BITS);
  MPU_DSB();
  MPU_ISB();

  /* ── Configure MAIR0: two attribute indices ──────────────────── */
  mpu_hw->mair[0] = (uint32_t)MAIR_ATTR_NORMAL_WBWA << 0u | /* AttrIdx 0 */
                    (uint32_t)MAIR_ATTR_DEVICE_nGnRE << 8u; /* AttrIdx 1 */

  /* ── Clear all 6 regions ─────────────────────────────────────── */
  for (uint32_t i = 0; i < 6u; i++)
  {
    mpu_hw->rnr = i;
    mpu_hw->rlar = 0u;
  }
  MPU_DSB();
  MPU_ISB();

  /* ── Configure up to 6 regions from the table ──────────────────── */
  for (uint32_t i = 0; i < 6u; i++)
  {
    if (s_region_cfg[i].limit == 0u)
      continue; /* reserved slot */

    mpu_hw->rnr = i;
    mpu_hw->rbar = MPU_RBAR_VAL(s_region_cfg[i].base, s_region_cfg[i].sh, s_region_cfg[i].ap,
                                s_region_cfg[i].xn);
    mpu_hw->rlar = MPU_RLAR_VAL(s_region_cfg[i].limit, s_region_cfg[i].attr_idx);
  }

  /* ── Enable MPU ──────────────────────────────────────────────── */
  /*
   * PRIVDEFENA=1: privileged code gets default access to regions not
   * covered by any MPU region.  Non-privileged (user) tasks will fault
   * on access to unmapped regions.
   *
   * HFNMIENA=0 (default): MPU is NOT active during NMI/hard-fault
   * handler execution — essential for flash-programming routines.
   */
  MPU_DMB();
  mpu_hw->ctrl = M33_MPU_CTRL_PRIVDEFENA_BITS | M33_MPU_CTRL_ENABLE_BITS;
  MPU_DSB();
  MPU_ISB();
}

#else /* !PICO_BUILD — host simulation stub */

void mpu_init(void)
{
  /* No MPU on the host — implementation is a no-op. */
}

#endif /* PICO_BUILD */
