# CubeSat OBC - Pending Tasks Document

**Document ID:** PENDING_TASKS.md  
**Version:** 3.0
**Last Updated:** 2026-07-22
**Status:** Active

---

## Table of Contents

1. [TODO Comments in Source Code](#1-todo-comments-in-source-code)
2. [Pull Requests Pending](#2-prs-pending)
3. [Phase 8 Tasks](#3-phase-8-tasks)
4. [ECSS Documents to Create](#4-ecss-documents-to-create)
5. [Open Issues](#5-open-issues-from-docs)
6. [Known Issues](#6-known-issues)

---

## 1. TODO Comments in Source Code

### 1.1 GPS Driver (neo7m.c) ✅ COMPLETED

| File | Line | Description | Priority | Effort | Status |
|------|------|-------------|----------|--------|--------|
| src/drivers/gps/neo7m.c | 146 | Release UART, stop interrupts | Medium | 1h | ✅ Done |
| src/drivers/gps/neo7m.c | 315 | Get system time in ms | Medium | 2h | ✅ Done (comment fixed) |
| src/drivers/gps/neo7m.c | 499 | Stale detection, timestamp check | High | 3h | ✅ Done |
| src/drivers/gps/neo7m.c | 505 | Parse from GPGGA sentence | Medium | 2h | ✅ Done (comment fixed) |

### 1.2 Attitude Control Task ✅ COMPLETED

| File | Line | Description | Priority | Effort |
|------|------|-------------|----------|--------|
| src/tasks/attitude_control_task.c | 60 | Read DLA mag_field (PR-18) | High | 4h | ✅ Done |

### 1.3 Flash Backend ✅ COMPLETED

| File | Line | Description | Priority | Effort | Status |
|------|------|-------------|----------|--------|--------|
| src/core/flash_backend_stub.c | 58 | Implement using flash_range_program() | High | 4h | ✅ Done (comment fixed) |

### 1.4 Magnetometer Driver (HMC5883L) ✅ COMPLETED

| File | Line | Description | Priority | Effort | Status |
|------|------|-------------|----------|--------|--------|
| src/drivers/mag/hmc5883l.c | 22 | Real I2C init implementation | High | 3h | ✅ Done |
| src/drivers/mag/hmc5883l.c | 29 | Real I2C read implementation | High | 3h | ✅ Done |
| src/drivers/mag/hmc5883l.c | 40 | Real I2C init/read implementation | High | 3h | ✅ Done |

### TODO Summary by Priority

| Priority | Count | Total Effort |
|----------|-------|--------------|
| High | 3 | ~9h |
| Medium | 5 | ~8h |
| **Total** | **8** | **~17h** |

---

## 2. Pull Requests Pending

| PR | Title | Status | Blocking | Notes |
|----|-------|--------|----------|-------|
| PR-18 | HMC5883L driver stub + DLA integration | In Progress | ADCS modes | Magnetometer integration for attitude determination |
| PR-23 | LQR mode-scheduled gains | ✅ VERIFIED COMPLETE | ADCS control | Verify completion status |

### 2.1 PR-18 Details

**Title:** HMC5883L Driver Stub + DLA Integration  
**Component:** Magnetometer Driver  
**Description:** Implement HMC5883L driver for magnetometer readings and integrate with Detumbling & Pointing Algorithm (DLA).  
**Dependencies:**
- I2C HAL implementation
- PR-23 (LQR gains)

**Tasks:**
- [ ] Complete HMC5883L I2C read/write functions
- [ ] Integrate mag_field read in attitude_control_task.c:60
- [ ] Add unit tests
- [ ] Update DLA documentation

### 2.2 PR-23 Details

**Title:** LQR Mode-Scheduled Gains  
**Component:** Attitude Control  
**Description:** Verify mode-scheduled LQR gain implementation is complete.  
**Status:** ✅ VERIFIED COMPLETE  
**Blocking:** None

**Verification Details:**
- Implementation: `lqr_schedule_apply()` in `lqr_schedule.c`
- Test file: `test_lqr_schedule.c` with 3 tests (T-LQRS-01, T-LQRS-02, T-LQRS-03)
- Integration: `lqr_schedule_apply()` is called in `attitude_control_task.c`
- Test results: 42/42 tests passing

---

## 3. Phase 8 Tasks

### 3.1 Feature Tasks

| Task | Description | Priority | Effort | Dependencies |
|------|-------------|----------|--------|--------------|
| Camera Driver | Implement camera driver for payload capture | High | 16h | ❌ Hardware damaged — OV2640 module confirmed faulty (SPI TEST1 readback mismatch). Replacement needed |
| LIS3MDL Migration | Migrate from HMC5883L (discontinued) to LIS3MDL | High | 12h | PR-18 |
| FM_PAYLOAD Mode | Payload mode state implementation (image_count tracking, T-PLD-INT-04, GPIO21 rail validation — HW-dependent: camera/RM3100/radiation pending) | High | 8h | ✅ **image_count tracking done, T-PLD-INT-04 done, GPIO21 validated on HW** — camera HW blocked |
| W25Qxx Integration | External flash storage (W25Qxx) integration | High | 8h | ✅ Done (PR-39) |
| PWM HAL (Wheels) | PWM HAL for reaction wheels (GPIO6/7/8) | Medium | 6h | ✅ Done |
| PWM HAL (Torquers) | PWM HAL for magnetorquers (GPIO14/15/16) | Medium | 6h | ✅ Done |
| RP2350 Flash Backend | Full RP2350 flash backend implementation | High | 8h | flash_backend_stub.c |
| MC/DC Coverage | MC/DC coverage analysis for certification | High | 20h | Test completion |
| **Sun Sensor Driver** | Dual-axis photodiode sun sensor on GPIO27/28 | **Done** | **4h** | ✅ Implemented in feat/sun-sensor-driver |
| ISR-safe mode_entry_tick | Add mode_entry_tick update in fmm_force_safe() from ISR context | High | 4h | ✅ Done (2c75ac6) |
| Fix BASEPRI mask in dl_lock_from_isr | Pass saved BASEPRI mask through from_isr lock/unlock | High | 2h | ✅ Done (59ddcaa) |
| Host test coverage expansion | diskio, eps_hal, spi_payload, watchdog_hal + camera/radiation/rm3100/w25q64 | Medium | 8h | ✅ Done (56c7eec) |
| I2C bus mutex protection | FreeRTOS mutex for I2C0 + I2C1 (SPI pattern) | High | 6h | ✅ Done (63d563c) |
| image_count tracking | payload_manager_increment_image_count + T-PLD-INT-04 | Medium | 4h | ✅ Done (bd4a8b0) |
| deploy_monitor CI fix | Add missing `#include <stdio.h>` in deploy_monitor.c | High | 30m | ✅ Done (bd4a8b0) |
| RESETGPS COLD / BH1750_TEST5C fix | Off-by-one bugs in text command parser | High | 2h | ✅ Done (63d563c) |

### 3.2 Phase 8 Effort Summary

| Category | Tasks | Total Effort |
|----------|-------|--------------|
| Payload | 2 | 28h |
| Storage | 1 | 8h |
| PWM HAL | 2 | 12h |
| Flash Backend | 1 | 8h |
| Verification | 1 | 20h |
| **Total** | **7** | **~76h** |

---

## 4. ECSS Documents to Create

### 4.1 Software Design Documents

| Document ID | Title | Purpose | Priority | Effort |
|-------------|-------|---------|----------|--------|
| ADCS-SIM-001 | ADCS Simulation Design | ADCS algorithm simulation and validation | Medium | 8h | ✅ Done |

### 4.2 Safety & Reliability Documents

| Document ID | Title | Purpose | Priority | Effort |
|-------------|-------|---------|----------|--------|
| FMEA-OBC-001 | OBC Failure Mode Effects Analysis | Hardware FMEA for OBC | High | 16h | ✅ Done |
| FMEA-OBC-002 | Software FMEA | Software failure mode analysis | High | 12h | ✅ Done |

### 4.3 Test Documents

| Document ID | Title | Purpose | Priority | Effort |
|-------------|-------|---------|----------|--------|
| STP-OBC-001 | Software Test Procedure | Detailed test procedures | High | 12h | ✅ Done |
| ITP-OBC-001 | Integration Test Plan | Subsystem integration testing | High | 8h | ✅ Done |
| ATP-OBC-001 | Acceptance Test Procedure | Acceptance criteria and procedures | Medium | 8h | ✅ Done |
| STR-OBC-001 | Software Test Report | Test results documentation | High | 8h | ✅ Done |

### 4.4 Operational Documents

| Document ID | Title | Purpose | Priority | Effort |
|-------------|-------|---------|----------|--------|
| OPS-OBC-001 | Operations Manual | Flight operations procedures | Low | 12h | ✅ Done |
| FRR-OBC-001 | Flight Readiness Review | Flight readiness documentation | Medium | 8h | ✅ Done |

### 4.5 ECSS Document Summary

| Priority | Count | Total Effort |
|----------|-------|--------------|
| High | 5 | 56h |
| Medium | 3 | 24h |
| Low | 1 | 12h |
| **Total** | **9** | **~92h** |

---

## 5. Open Issues (from docs)

| Issue ID | Title | Severity | Status | Blocking |
|----------|-------|----------|--------|----------|
| OI-1 | Antenna mechanical design | Medium | Open | Deployment |
| 2.1 | GPS UART0 conflict | High | ✅ Resolved | GPS subsystem - usar UART1 para debug, GPS en UART0 funciona |
| 2.2 | Flash backend stub | High | Open | Data logging |
| OI-5 | External watchdog GPIO20 | Medium | Open | Hardware |
| OI-7 | TX PA efficiency concern | Medium | Open | Power budget |
| OI-8 | Heap sizing (~82KB needed vs 60KB) | Critical | ✅ Resolved (128 KB) | Memory subsystem |

### 5.1 Critical Issues Detail

#### OI-8: Heap Sizing ✅ RESOLVED
**Issue:** Heap allocation insufficient for RP2350 — resolved in phase7-payload merge  
**Before:** 60KB heap (was insufficient for 8 tasks + CSP)  
**Now:** `configTOTAL_HEAP_SIZE = 128 KB` (131072 bytes) in `config/FreeRTOSConfig.h`  
**Required was:** ~82KB  
**Status:** ✅ Resolved — 128 KB provides 56% margin above the 82 KB requirement

### 5.2 High Priority Issues

#### OI-3: GPS UART0 Conflict
**Issue:** UART0 shared between GPS and debug console  
**Impact:** GPS data corruption or loss  
**Resolution Path:**
1. Move debug console to UART1
2. Implement UART arbitration

#### OI-4: Flash Backend Stub
**Issue:** Stub implementation in production code  
**Impact:** Data persistence failure  
**Resolution Path:**
1. Implement flash_range_program()
2. Add wear leveling
3. Implement error recovery

---

## 6. Known Issues

### 6.1 Memory Issues

| Issue | Description | Impact | Resolution |
|-------|-------------|--------|------------|
| Heap Configuration | ✅ Resolved — 128 KB configured (was 60 KB vs 82 KB required) | None | `configTOTAL_HEAP_SIZE = 131072` in FreeRTOSConfig.h |

### 6.2 Driver Status

| Component | Status | Notes |
|-----------|--------|-------|
| Camera Driver | ❌ Hardware damaged | OV2640 module confirmed faulty — `camera_init` fails with SPI TEST1 readback mismatch. All software paths exhausted. Replacement needed |
| W25Q64 External Flash | ✅ Complete | SPI0, JEDEC ID verified, erase/program/read working |
| HMC5883L Driver | ✅ Complete | I2C driver with HAL stub, host-testable |
| I2C Bus Mutex (I2C0 + I2C1) | ✅ Complete | FreeRTOS mutex, SPI pattern, verified |
| GPS Driver | ✅ Complete | 11 comandos, API por valor, stats, HDOP |
| **Sun Sensor** | **✅ Complete** | Dual-axis photodiode on GPIO27/28 |

### 6.3 Hardware Dependencies

| Component | Status | Notes |
|-----------|--------|-------|
| Magnetometer | Discontinued | HMC5883L discontinued, migrate to LIS3MDL |
| GPS Module | Working ✅ | UART0, comandos CSP/UART implementados |
| External Flash | Pending | W25Qxx integration pending |
| External Watchdog | Pending | GPIO20 connection pending |

---

## 7. Command Interface (CSP + UART)

### 7.1 Implemented Commands - Phase 1 & 2

#### UART Text Commands (via HC-12)

| Command | Description | Example Response |
|---------|-------------|-------------------|
| `REBOOT` | Reiniciar sistema | `REBOOT OK` |
| `STATUS` | Estado general del sistema | `SYSTEM: mode=NOMINAL energy=NOMINAL imu=OK temp=OK mag=FAIL` |
| `GPS` | Estado del GPS + stats | `GPS: v=1 lat=-34.78047 lon=-58.28801 alt=21.6 s=7 hdop=1.1` |
| `FAULTS` | Estado de faults | `FAULTS: OK` |
| `RESETGPS` | Resetear contadores GPS | `RESET GPS OK` |
| `HELP` | Lista de comandos | `COMMANDS: REBOOT|STATUS|...` |

#### CSP Commands (port 20)

| CMD_ID | Command | Payload In | Payload Out | Description |
|--------|---------|------------|-------------|-------------|
| 1 | CMD_ECHO | text (max 32) | text echo | Echo test |
| 2 | CMD_REBOOT | none | none | Reiniciar sistema |
| 3 | CMD_SET_MODE | mode (1 byte) | result | Cambiar modo de vuelo |
| 4 | CMD_PAYLOAD_CAPTURE | none | none | Capturar imagen |
| 5 | CMD_GPS_STATUS | none | 21 bytes | Estado del GPS |
| 6 | CMD_STATUS | none | 7 bytes | Estado del sistema |
| 7 | CMD_FAULT_LIST | none | variable | Listar faults activos |
| 8 | CMD_TELEMETRY_REQ | none | none | Forzar telemetry |
| 9 | CMD_LOG_DUMP | count (1-16) | count | Dump de eventos |
| 10 | CMD_SENSOR_RESET | sensor_id | result | Resetear sensor (0-3) |
| 11 | CMD_GPS_RESET_STATS | none | 1 | Resetear stats GPS |

### 7.2 GPS Driver Features

- Ring buffer con ISR + task (arquitectura correcta)
- Parser NMEA con checksum verification
- Soporte para $GPGGA y $GPRMC
- HDOP parsing
- GpsStats_t con contadores (sentences, checksum_errors, fixes_valid, etc.)
- API por valor `gps_get_last_fix(GpsFix_t *out)` (thread-safe)
- Timeout en mutex (100ms) para evitar deadlocks

---

## Appendix A: Priority Definitions

| Priority | Description | Response Time |
|----------|-------------|---------------|
| Critical | System non-functional, data loss risk | Immediate |
| High | Major feature broken, workaround exists | 1 week |
| Medium | Minor feature broken, no workaround | 2 weeks |
| Low | Cosmetic issue, enhancement | 1 month |

---

## Appendix B: Effort Estimates

| Effort | Hours | Description |
|--------|-------|-------------|
| XS | 1-2h | Quick fix |
| S | 2-4h | Small task |
| M | 4-8h | Medium task |
| L | 8-16h | Large task |
| XL | 16-32h | Major feature |
| XXL | 32h+ | Epic |

---

## Appendix C: Dependency Graph

```
PR-18 (HMC5883L)
    └── Phase 8: LIS3MDL Migration
            └── DLA Integration

OI-4 (Flash Backend Stub)
    └── Phase 8: RP2350 Flash Backend
            └── W25Qxx Integration
                    └── FM_PAYLOAD Mode

Camera Driver
    └── FM_PAYLOAD Mode

OI-8 (Heap Sizing)
    └── Phase 8: MC/DC Coverage (need stable build)
```

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2026-03-20 | System | Initial document creation |
| 1.2 | 2026-03-20 | System | PR-18 (DLA mag_field) completed - 1.2 marked as done |
| 1.3 | 2026-03-20 | System | 1.3 (Flash Backend) completed - TODO at line 58 was outdated, real implementation exists in flash_backend.c |
| 1.4 | 2026-03-20 | System | HMC5883L I2C implementation completed - 1.4 marked as done |
| 1.5 | 2026-03-20 | System | PR-23 (LQR mode-scheduled gains) verified complete - lqr_schedule.c, test_lqr_schedule.c (42/42 tests passing) |
| 1.6 | 2026-03-20 | System | PWM HAL implemented for reaction wheels and magnetorquers - 25kHz PWM using Pico SDK hardware_pwm (43/43 tests passing) |
| 1.7 | 2026-03-21 | System | STR-OBC-001 (Software Test Report) created - 43/43 tests passing, 93% line coverage |
| 1.8 | 2026-03-21 | System | STP-OBC-001 (Software Test Procedure) created - detailed test procedures documented |
| 1.9 | 2026-03-21 | System | ITP-OBC-001 (Integration Test Plan) and ATP-OBC-001 (Acceptance Test Procedure) created |
| 2.0 | 2026-03-21 | System | FMEA-OBC-001 (Hardware FMEA) and FMEA-OBC-002 (Software FMEA) completed |
| 2.1 | 2026-03-23 | System | ADCS-SIM-001, OPS-OBC-001, FRR-OBC-001 marked as done |
| 2.2 | 2026-04-24 | System | Sun sensor driver implemented - dual-axis photodiode on GPIO27/28, feat/sun-sensor-driver branch |
| 2.3 | 2026-06-20 | System | Added ISR-safe mode_entry_tick, BASEPRI fix, test coverage expansion tasks marked done |
| 2.4 | 2026-06-24 | System | Added Section 9: Bootloader Improvements (watchdog, fsw_confirmed, reset cause, boot status RAM, boot log, UART debug) |
| 2.5 | 2026-07-02 | System | Updated FM_PAYLOAD status (image_count tracking, T-PLD-INT-04, GPIO21 HW validation done). Added I2C mutex, RESETGPS/BH1750 fixes, deploy_monitor CI fix as completed. Noted OV2640 camera hardware damage. |
| 2.6 | 2026-07-07 | System | Bootloader 9.1 (Supervised Watchdog), 9.6 (UART Debug), and 9.4 (Unified Boot Status RAM) completed — 3.5-7.5h remaining. Updated effort table with status markers. |
| 2.7 | 2026-07-07 | System | Bootloader 9.2 (fsw_confirmed) completed — shared boot_meta.h, boot_meta_set_fsw_confirmed(), bootloader check at startup, FSW call after POST + tasks. 9.3 and 9.5 remaining. |
| 2.8 | 2026-07-07 | System | Bootloader 9.3 (Reset Cause Detection) completed — reads watchdog_hw->reason at startup, stores in boot_meta_t + boot_status_t. Codes: 1=POR/pin, 2=WDT, 3=SW forced. reset_cause field added to boot_status_t (slot failures shrunk from uint16_t to uint8_t). |
| 2.9 | 2026-07-07 | System | Bootloader 9.5 (Boot Log Ring Buffer) completed — dedicated 4 KB sector at 0x10311000, 128 × 32-byte entries with CRC32, sequential ring buffer. Bootloader writes entry before each jump: sequence, reset_cause, image_used, crc_ok, fallback_used, bl_duration_ms. FSW reads via boot_log_read_entry(). All bootloader tasks complete. |
| 3.0 | 2026-07-22 | System | Added Section 10: Future Scientific Payloads. Created `docs/proposals/` directory with PROP-001 (Meteorological Station) — BME280 + SHT31, 40 h effort, CSP commands, flash ring buffer. Updated revision history. |

---

## 7. Backlog Features

| Feature | Description | Priority | Status |
|---------|-------------|----------|--------|
| UART telemetry dump command | Add `DUMP` or `TELEMETRY` text command to read telemetry records from W25Q64 flash via UART (currently only CSP `CMD_TELEMETRY_DUMP` implemented) | Low | Backlog |
| Auto FM_BOOT Transition | Implement deployment timer (30 min) to automatically transition FM_BOOT → FM_DETUMBLE → FM_NOMINAL without manual command | Medium | Backlog |
| EPS init correction | EPS monitor must report faults if battery voltage is critical at startup, without waiting for first tick | Medium | Backlog |

---

## 8. Implementation Notes: Auto Flight Transition

### 8.1 Mode Auto-Transition (FM_BOOT → FM_DETUMBLE → FM_NOMINAL)

In a real CubeSat mission, the satellite must remain inactive (except for critical tasks) for a mandatory period (typically 30 or 45 minutes) after P-POD deployment before turning on high-power transmitters or actuators.

**Files to modify:**

- `src/obc_main.c`: Add timer in `[ALIVE]` loop of `vStartupTask`
- Count elapsed time (uptime)
- After `DEPLOYMENT_DELAY_MS` (e.g., 30 minutes):
  - Evaluate angular rates (gyroscope)
  - If rotation > threshold → `fmm_request_transition(FM_DETUMBLE)`
  - If stable → `fmm_request_transition(FM_NOMINAL)`

### 8.2 EPS Startup Fault Reporting

FMM needs to know if the satellite powered on with critical voltage.

**File to modify:**

- `src/services/eps/eps_monitor.c`: In `eps_monitor_init()`, after evaluating battery state, force fault if state is not NOMINAL

```c
// After compute_energy_state()
if (g_snapshot.state != ENERGY_NOMINAL) {
    handle_state_change(ENERGY_NOMINAL, g_snapshot.state);
}
```

*Note: If connected via USB (VBUS active), this check could be bypassed for laboratory development.*

---

## 9. Bootloader Improvements (Golden Image MPU)

The dual-slot golden image bootloader (`feature/golden-image-mpu`) implements the core chain-load and CRC32 validation. The following tasks extend it to full flight-readiness per CubeSat bootloader best practices.

### 9.1 Supervised Watchdog Before Jump ✅ COMPLETED

**Priority**: HIGH | **Effort**: XS (~30 min) | **Status**: ✅ Done (a613944)

The bootloader must arm the hardware watchdog before jumping to the FSW. If the FSW fails to kick the watchdog within the timeout, the MCU resets and the bootloader increments the failure counter for that slot.

**Files**:
- `bootloader/bootloader.c` — add `watchdog_enable()` before `jump_to_image()`
- `src/services/watchdog/watchdog_hal_pico.c` — FSW watchdog kick in `vStartupTask` (already exists)

**Acceptance**:
- Bootloader arms WDT with 30 s timeout
- FSW kicks WDT in `vStartupTask` before timeout expiry
- If FSW hangs, WDT fires → bootloader sees slot failure counter increment

### 9.2 FSW Boot Confirmation (`fsw_confirmed`) ✅ COMPLETED

**Priority**: HIGH | **Effort**: M (~2 h) | **Status**: ✅ Done (cf0166b)

The FSW writes `fsw_confirmed = 1` to `boot_meta_t` in internal flash after POST + all task creation succeed. On the next boot, the bootloader reads this flag, resets `slot_a_failures` and `slot_b_failures` to 0, and clears the flag — preventing infinite CRC-pass → FSW-crash loops.

**Implementation**:
- `include/boot_meta.h` — shared `boot_meta_t` with `fsw_confirmed` field (replacing 1 byte of `_pad[3]`), 24 bytes, CRC32-protected
- `src/core/boot_meta.c` — `boot_meta_set_fsw_confirmed()` using Pico SDK `flash_range_erase/program` (PICO_BUILD) or no-op (host)
- `bootloader/bootloader.c` — uses shared `boot_meta.h` instead of local struct; checks `fsw_confirmed` at startup before trying slots
- `src/obc_main.c` — calls `boot_meta_set_fsw_confirmed()` after POST OK + tasks created

**Acceptance verified**:
- FSW writes `fsw_confirmed = 1` after successful init, persists in flash across resets
- Bootloader resets failure counters when `fsw_confirmed == 1` on next boot
- Slot with simulated CRC failure correctly exhausts `MAX_FAILURES` attempts before fallback

### 9.3 Reset Cause Detection ✅ COMPLETED

**Priority**: MEDIUM | **Effort**: S (~30 min) | **Status**: ✅ Done

The bootloader must read the RP2350 reset cause registers (`watchdog_hw->reason`, PSM registers) to distinguish POR, WDT, Software, Pin reset, and brownout. This information is critical for the FSW to classify anomalies (e.g., WDT in orbit = anomaly, not normal boot).

**What was done**:
- Reads `watchdog_hw->reason` at `bootloader_main()` start to detect: POR/pin (clean), WDT, or SW forced
- Stores in `boot_meta_t.reset_cause` (persists in flash as a `_pad` byte replaced) and `boot_status_t.reset_cause`
- `build_boot_status()` propagates `meta->reset_cause` into the SRAM struct
- RP2350 codes: 1=POR/pin, 2=WDT, 3=SW forced. Raw `watchdog_hw->reason` printed for debug.

**Files changed**:
- `bootloader/bootloader.c` — reads `watchdog_hw->reason`, sets `meta.reset_cause`, passes to `build_boot_status()`
- `include/boot_info.h` — `boot_status_t.reset_cause` (uint8_t), `slot_a/b_failures` shrunk to uint8_t
- `include/boot_meta.h` — `boot_meta_t.reset_cause` replaces one `_pad` byte
- `src/core/boot_info.c` — reads `reset_cause` field

### 9.4 Unified Boot Status RAM Region ✅ COMPLETED

**Priority**: MEDIUM | **Effort**: M (~1 h) | **Status**: ✅ Done (251ec73)

The bootloader now writes `boot_status_t` at `BOOT_STATUS_ADDR` (0x2007FF00) at every jump/error decision point. The FSW reads it via `boot_status_read()`. `boot_info_t` is preserved as a typedef to `boot_status_t` with inline wrappers — existing callers unchanged.

**Struct fields** (24 bytes, v2.8):
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | magic | uint32_t | `0xB007B007` |
| 4 | boot_count | uint32_t | Total boot attempts |
| 8 | current_slot | uint8_t | `BOOT_SLOT_A/B` |
| 9 | boot_reason | uint8_t | `POST_BOOT_*` |
| 10 | golden_valid | uint8_t | 1 if golden image valid |
| 11 | flags | uint8_t | `CRC_OK`, `WDT_ARMED`, `FALLBACK`, `GOLDEN` |
| 12 | last_crc_computed | uint32_t | CRC computed by bootloader |
| 16 | last_crc_expected | uint32_t | CRC from slot metadata |
| 20 | reset_cause | uint8_t | 1=POR, 2=WDT, 3=SW forced |
| 21 | slot_a_failures | uint8_t | Consecutive failures slot A |
| 22 | slot_b_failures | uint8_t | Consecutive failures slot B |
| 23 | _pad | uint8_t | Reserved |

**Files changed**:
- `include/boot_info.h` — new `boot_status_t` + `BOOT_STATUS_MAGIC` + `BOOT_STATUS_FLAG_*` defines, legacy typedef
- `src/core/boot_info.c` — rewritten `boot_status_read()` + `boot_status_clear()`, volatile SRAM read
- `bootloader/bootloader.c` — `post_code()` replaced by `build_boot_status()` + `boot_status_write()` at all 8 jump/error decision points + 3 golden_restore failure points

### 9.5 Boot Log Ring Buffer in Flash ✅ COMPLETED

**Priority**: LOW | **Effort**: L (~4-8 h) | **Status**: ✅ Done

A ring buffer in a dedicated flash sector (4 KB, 128 × 32-byte entries) stores timestamped boot events. Written by the bootloader at every jump/fallback, readable by the FSW for telemetry / post-mortem.

**What was done**:
- Flash layout: dedicated sector at `BOOT_LOG_BASE` (0x10311000, 4 KB), carved from the reserved region
- `include/boot_log.h` — `boot_log_entry_t` struct (32 bytes, CRC32-protected), full API
- `src/core/boot_log.c` — ring buffer implementation with sequential writes: scans for first empty (0xFF) entry, programs via flash page; when full, erases sector and restarts
- `bootloader/bootloader.c` — captures `time_us_32()` at boot start, calls `write_boot_log()` before every jump path (Slot A/B CRC PASS, fresh binary trust-on-first-boot, golden restore), writing: sequence, reset_cause, image_used, crc_ok, fallback_used, bl_duration_ms
- FSW can read entries via `boot_log_read_entry(index)` — pure XIP memory read, no flash driver needed

**Entry layout** (32 bytes):
| Offset | Field | Type | Description |
|--------|-------|------|-------------|
| 0 | sequence | uint32_t | 1-based, monotonically increasing |
| 4 | boot_count | uint32_t | From boot_status_t (0 until tracked) |
| 8 | reset_cause | uint32_t | 1=POR, 2=WDT, 3=SW forced |
| 12 | image_used | uint8_t | 0=A, 1=B, 2=Golden |
| 13 | crc_ok | uint8_t | 1 if image CRC passed |
| 14 | fallback_used | uint8_t | 1 if golden restore triggered |
| 15 | _pad[1] | uint8_t | Reserved |
| 16 | bl_duration_ms | uint32_t | Bootloader execution time |
| 20 | last_crc_computed | uint32_t | CRC computed (0 until tracked) |
| 24 | last_crc_expected | uint32_t | Expected CRC (0 until tracked) |
| 28 | crc_entry | uint32_t | CRC32 of bytes [0..27] |

### 9.6 Bootloader UART Debug Output ✅ COMPLETED

**Priority**: LOW | **Effort**: XS (~15 min) | **Status**: ✅ Done (a613944)

Add `stdio_init_all()` and `printf()` calls in the bootloader for visible boot flow: CRC result, slot selected, golden restore trigger, etc. Helps development debugging with zero flight cost (UART can be left disconnected).

### 9.7 Effort Summary

| Task | Priority | Effort | Hours |
|------|----------|--------|-------|
| 9.1 Supervised Watchdog | HIGH | XS | ~30 min | ✅ |
| 9.2 FSW Boot Confirmation | HIGH | M | ~2 h | ✅ |
| 9.3 Reset Cause Detection | MEDIUM | S | ~30 min | ✅ |
| 9.4 Unified Boot Status RAM | MEDIUM | M | ~1 h | ✅ |
| 9.5 Boot Log Ring Buffer | LOW | L | ~4-8 h | ✅ |
| 9.6 Bootloader UART Output | LOW | XS | ~15 min | ✅ |
| **Total** | | | **~0 h remaining** | |

### 9.8 Dependency Graph

```
9.1 (Watchdog)
  └── 9.2 (fsw_confirmed) — WDT counter needs fsw_confirmed to reset
          └── 9.4 (Boot Status RAM) — preferred channel for fsw_confirmed
                  └── 9.5 (Boot Log) — depends on unified status RAM
9.3 (Reset Cause)
  └── 9.4 (Boot Status RAM) — reset cause propagated via BootStatus_t
9.6 (UART) — independent, purely dev convenience
```

---

## 10. Future Scientific Payloads

Proposals for scientific instruments and payloads that can be implemented on the
CubeSat OBC platform. See `docs/proposals/` for full design documents.

| ID | Proposal | Status | Effort | Priority | Dependencies |
|----|----------|--------|--------|----------|--------------|
| PROP-001 | [Meteorological Station](../proposals/METEO-STATION-001.md) (BME280 + SHT31) | Proposed | 40 h | Medium | I2C0 mutex, DLA extension |

### 10.1 Meteorological Station (PROP-001)

**Objective**: Integrate BME280 pressure/temperature/humidity sensor alongside
existing SHT31 for redundant meteorological measurements and barometric altitude.

**Key deliverables**:
- `bme280_driver.c` — I2C driver for Bosch BME280
- `sht31_driver.c` — I2C driver for Sensirion SHT31
- `meteo_task.c` — FreeRTOS task at 1 Hz, priority 2
- Flash ring buffer in 0x7F2000 region (~36 KB, 1,316 records)
- CSP commands (port 30): READ, STATUS, CONFIG, DUMP, CLEAR, CALIB, RAW
- UART text commands: `METEO`, `METEO STATUS`, `METEO CONFIG`, `METEO DUMP`

**Hardware**: BME280 (I2C 0x76) + SHT31 (I2C 0x44) on shared I2C0 bus
**Power impact**: <65 µA average (negligible)
**Effort**: 40 h (5 phases)

**Status**: ⏳ Awaiting implementation — see [PROP-001](../proposals/METEO-STATION-001.md)

---

*End of Document*
