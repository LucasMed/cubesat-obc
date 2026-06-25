# CubeSat OBC - Pending Tasks Document

**Document ID:** PENDING_TASKS.md  
**Version:** 2.4  
**Last Updated:** 2026-06-24  
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
| Camera Driver | Implement camera driver for payload capture | High | 16h | Camera hardware selection |
| LIS3MDL Migration | Migrate from HMC5883L (discontinued) to LIS3MDL | High | 12h | PR-18 |
| FM_PAYLOAD Mode | Payload mode state implementation | High | 8h | Camera driver |
| W25Qxx Integration | External flash storage (W25Qxx) integration | High | 8h | ✅ Done (PR-39) |
| PWM HAL (Wheels) | PWM HAL for reaction wheels (GPIO6/7/8) | Medium | 6h | ✅ Done |
| PWM HAL (Torquers) | PWM HAL for magnetorquers (GPIO14/15/16) | Medium | 6h | ✅ Done |
| RP2350 Flash Backend | Full RP2350 flash backend implementation | High | 8h | flash_backend_stub.c |
| MC/DC Coverage | MC/DC coverage analysis for certification | High | 20h | Test completion |
| **Sun Sensor Driver** | Dual-axis photodiode sun sensor on GPIO27/28 | **Done** | **4h** | ✅ Implemented in feat/sun-sensor-driver |
| ISR-safe mode_entry_tick | Add mode_entry_tick update in fmm_force_safe() from ISR context | High | 4h | ✅ Done (2c75ac6) |
| Fix BASEPRI mask in dl_lock_from_isr | Pass saved BASEPRI mask through from_isr lock/unlock | High | 2h | ✅ Done (59ddcaa) |
| Host test coverage expansion | diskio, eps_hal, spi_payload, watchdog_hal + camera/radiation/rm3100/w25q64 | Medium | 8h | ✅ Done (56c7eec) |

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
| Camera Driver | Deferred | Awaiting camera hardware selection |
| External Storage | Pending | W25Qxx integration pending |
| HMC5883L Driver | Partial | Stub implementation, I2C not implemented |
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

### 9.1 Supervised Watchdog Before Jump

**Priority**: HIGH | **Effort**: XS (~30 min) | **Status**: ❌ Not implemented

The bootloader must arm the hardware watchdog before jumping to the FSW. If the FSW fails to kick the watchdog within the timeout, the MCU resets and the bootloader increments the failure counter for that slot.

**Files**:
- `bootloader/bootloader.c` — add `watchdog_enable()` before `jump_to_image()`
- `src/services/watchdog/watchdog_hal_pico.c` — FSW watchdog kick in `vStartupTask` (already exists)

**Acceptance**:
- Bootloader arms WDT with 30 s timeout
- FSW kicks WDT in `vStartupTask` before timeout expiry
- If FSW hangs, WDT fires → bootloader sees slot failure counter increment

### 9.2 FSW Boot Confirmation (`fsw_confirmed`)

**Priority**: HIGH | **Effort**: M (~2 h) | **Status**: ❌ Not implemented

The FSW must mark a boot as successful in the Boot Config Block after completing its initialization. The bootloader resets the slot failure counter when it sees `fsw_confirmed == 1` on the next boot.

**Current state**:
- ✅ Failure counters exist (`slot_a_failures`, `slot_b_failures`, `MAX_FAILURES = 3`)
- ❌ FSW never writes `fsw_confirmed`
- ❌ Failure counters never reset → all slots eventually exhaust retries after `MAX_FAILURES × number_of_slots` boot cycles

**Files**:
- `bootloader/bootloader.c` — read `fsw_confirmed` from `boot_meta_t`, reset failure counters when set
- `include/internal_flash_layout.h` — extend `boot_meta_t` with `fsw_confirmed` field
- `src/obc_main.c` — write `fsw_confirmed = 1` after successful POST + task creation

**Acceptance**:
- After first successful boot, `fsw_confirmed = 1` persists in flash
- On subsequent boots, the bootloader resets `slot_a_failures = 0` when `fsw_confirmed == 1`
- A slot with simulated CRC failure exhausts `MAX_FAILURES` attempts before fallback

### 9.3 Reset Cause Detection

**Priority**: MEDIUM | **Effort**: S (~30 min) | **Status**: ❌ Not implemented

The bootloader must read the RP2350 reset cause registers (`watchdog_hw->reason`, PSM registers) to distinguish POR, WDT, Software, Pin reset, and brownout. This information is critical for the FSW to classify anomalies (e.g., WDT in orbit = anomaly, not normal boot).

**Current state**:
- ✅ FSW has `post_detect_boot_reason()` reading `watchdog_hw->scratch[0]`
- ❌ Bootloader does not read or propagate reset cause

**Implementation**:
- Read `watchdog_hw->reason` and PSM registers in `bootloader_main()`
- Propagate via `BootStatus_t` in SRAM (see 9.4) or via `boot_meta_t`

### 9.4 Unified Boot Status RAM Region

**Priority**: MEDIUM | **Effort**: M (~1 h) | **Status**: ❌ Not implemented — address mismatch

The bootloader writes to `POST_CODE_ADDR` (0x20040000) and `BOOT_META_BASE` (0x10221000, flash). The FSW reads from `BOOT_INFO_ADDR` (0x2007FF00) via `boot_info_read()`. These are **different addresses with different structs** — the bootloader and FSW speak different protocols.

**Fix**: Define a single `BootStatus_t` struct in a reserved SRAM region:

| Field | Type | Description |
|-------|------|-------------|
| `magic` | `uint32_t` | `0xB007B007` — validates BL wrote this |
| `boot_count` | `uint32_t` | Total boot attempts |
| `active_image` | `uint8_t` | `0=A, 1=B` |
| `reset_cause` | `uint8_t` | POR / WDT / SW / PIN |
| `boot_attempts` | `uint8_t` | Attempts for this image |
| `flags` | `uint8_t` | Bits: crc_ok, wdt_armed, fallback_used |
| `fsw_crc_computed` | `uint32_t` | CRC computed by bootloader |
| `fsw_crc_expected` | `uint32_t` | CRC from slot metadata |
| `timestamp_ms` | `uint32_t` | Bootloader execution time |
| `crc_self` | `uint32_t` | CRC32 of this struct |

**Files**:
- `bootloader/bootloader.c` — replace `post_code()` with `BootStatus_t` write
- `include/boot_info.h` — replace `boot_info_t` with `BootStatus_t` (or alias)
- `src/core/boot_info.c` — update read to use new struct and address
- `include/internal_flash_layout.h` — add `BOOT_STATUS_ADDR` constant

### 9.5 Boot Log Ring Buffer in Flash

**Priority**: LOW | **Effort**: L (~4-8 h) | **Status**: ❌ Not implemented

A ring buffer in a dedicated flash sector (4 KB) stores timestamped boot events. The FSW can download the log via telemetry for post-mortem analysis of anomalies (long eclipses, SEU, etc.).

**Structure** (per entry, 32 bytes):

```c
typedef struct {
    uint32_t sequence;
    uint32_t boot_count;
    uint32_t reset_cause;
    uint8_t  image_used;
    uint8_t  crc_ok;
    uint8_t  fallback_used;
    uint8_t  pad;
    uint32_t bl_duration_ms;
    uint32_t crc_computed;
    uint32_t crc_expected;
    uint32_t crc_entry;
} __attribute__((packed)) BootLogEntry_t;
```

### 9.6 Bootloader UART Debug Output

**Priority**: LOW | **Effort**: XS (~15 min) | **Status**: ❌ Not implemented

Add `stdio_init_all()` and `printf()` calls in the bootloader for visible boot flow: CRC result, slot selected, golden restore trigger, etc. Helps development debugging with zero flight cost (UART can be left disconnected).

### 9.7 Effort Summary

| Task | Priority | Effort | Hours |
|------|----------|--------|-------|
| 9.1 Supervised Watchdog | HIGH | XS | ~30 min |
| 9.2 FSW Boot Confirmation | HIGH | M | ~2 h |
| 9.3 Reset Cause Detection | MEDIUM | S | ~30 min |
| 9.4 Unified Boot Status RAM | MEDIUM | M | ~1 h |
| 9.5 Boot Log Ring Buffer | LOW | L | ~4-8 h |
| 9.6 Bootloader UART Output | LOW | XS | ~15 min |
| **Total** | | | **~8.5-12.5 h** |

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

*End of Document*
