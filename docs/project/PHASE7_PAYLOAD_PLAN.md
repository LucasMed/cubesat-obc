# Phase 7: Scientific Payload Integration — Plan

**Document ID**: PLAN-007  
**Version**: 0.2  
**Last Updated**: 2026-03-10  
**Branch**: `feature/phase7-payload`  
**Status**: Planning  
**Depends on**: Phase 6 complete (`feature/srr-air-resolution` merged to `dev`)

---

## Overview

Phase 7 integrates the scientific payload suite **PLS-001** into the existing
CubeSat OBC flight software. The payload consists of three instruments:
`CAM-001` (Earth observation camera, IMX219 via SPI), `MAG-001` (scientific
magnetometer, RM3100 via I2C), and `RAD-001` (radiation detector, PIN diode
via ADC). Full instrument specifications are in **PAYLOAD-SPEC-001**.

This phase has three parallel tracks:

**Track A — Flight Mode Extension**  
Add `FM_PAYLOAD` to the FMM, update the transition matrix, and validate all
existing FMM tests still pass with the new state.

**Track B — Driver and Task Development**  
Implement the three payload drivers (`camera_driver`, `rm3100`, `radiation_driver`),
the `PayloadTask`, and the `payload_manager` service. Cover each with unit tests.

**Track C — Hardware Integration and Storage**  
Select and integrate external storage (SD card or W25Q128 SPI flash),
assign and resolve the GPIO10 / RW3 conflict, and validate the full payload
pipeline on the Pico 2W hardware.

**Track D — GPS Integration**  
Formally integrate the NEO-7M GPS receiver (GY-NEO6Mv2) on UART0 (GPIO0/1):
NMEA parser driver, FreeRTOS `GpsTask` (1 Hz), UTC clock sync, and telemetry
fields. Resolves AIR-OBC-001 ACT-06 Option A; adds FR-18, FR-19, IR-10 to
SRS-OBC-001 v2.3.

| Area | Phase 6 state | Phase 7 target |
|------|--------------|----------------|
| Flight mode | 5 modes (BOOT..DIAGNOSTIC) | 6 modes (+ FM_PAYLOAD) |
| Payload drivers | None | camera_driver, rm3100, radiation_driver |
| PayloadTask | None | FreeRTOS task, 100 ms period, priority 3 |
| External storage | None (2 MB internal flash only) | ≥ 1 GB external (SD or W25Q) |
| FM_PAYLOAD unit tests | N/A | T-PLD-TSK-01..06 + instrument drivers |
| FM_PAYLOAD integration tests | N/A | T-PLD-INT-01..04 |
| GPIO conflict | None | RW3 reassigned GPIO10 → GPIO3 |
| GPS driver | None | `neo7m.c` NMEA parser, `GpsTask` 1 Hz, UTC sync (FR-18/19) |
| Power budget | Phase 1 only | FM_PAYLOAD scenario added (POWER-BDG-001 §7.4) |

**Duration estimate**: 6–8 weeks  
**Start date**: 2026-04-01 (after Phase 6 merge to `dev`)  
**Target completion**: Late May 2026  
**Hardware dependency**: Pico 2W + RM3100 breakout + IMX219 SPI module + radiation sensor PCB

---

## Architecture Impact

### New modules

```
src/
  drivers/
    payload/
      camera_driver.c        ← SPI1 frame trigger + readout (IMX219 SPI bridge)
      rm3100.c               ← I2C RM3100 driver: init, CMM config, 3-axis read
      radiation_driver.c     ← ADC1 read + TIA reset + dose accumulator
      spi_payload.c          ← SPI1 bus init helper (10 MHz, CPOL/CPHA TBD)
  tasks/
    payload_task.c           ← FreeRTOS task: 10 Hz MAG, 1 Hz RAD, CAM on notification
  services/
    payload/
      payload_manager.c      ← enable/disable rail, status, storage coordinator

include/
  camera_driver.h
  rm3100.h
  radiation_driver.h
  payload_task.h
  payload_manager.h

tests/unit/
  test_rm3100.c              ← T-PLD-MAG-01..04
  test_radiation_driver.c    ← T-PLD-RAD-01..02
  test_payload_task.c        ← T-PLD-TSK-01..06

tests/integration/
  test_payload_fmm.c         ← T-PLD-INT-01..04
```

### Modified modules

| File | Change |
|------|--------|
| `include/flight_mode.h` | `FM_PAYLOAD = 5` added before `FM_COUNT` ✅ **done** |
| `src/services/fmm/flight_mode_manager.c` | Add `FM_PAYLOAD` row/column to `g_allowed[][]`; add `"PAYLOAD"` to `fmm_mode_name()` |
| `src/obc_main.c` | Create `PayloadTask` in task-init block |
| `include/data_layer.h` | Add `payload_status_t` field to `obc_snapshot_t` |
| `src/core/data_layer.c` | Implement `data_layer_get/set_payload_status()` |
| `include/config.h` | Add `PAYLOAD_SPI_HZ`, `CAM_TRIGGER_PIN`, `PAYLOAD_ENABLE_PIN`, `RAD_ADC_CHANNEL` |
| `config/pico_pins.h` | Add payload GPIO defs; reassign `RW_MOTOR3_PIN` from GPIO10 to GPIO3 |
| `include/log_event_ids.h` | Add `LOG_EVT_PAYLOAD_ENABLE (0x0201)`, `LOG_EVT_PAYLOAD_DISABLE (0x0202)`, `LOG_EVT_CAM_CAPTURE (0x0203)` |
| `include/fault_ids.h` | Add `FAULT_PAYLOAD_CAM_FAIL`, `FAULT_PAYLOAD_MAG_FAIL`, `FAULT_PAYLOAD_STORAGE_FULL` |
| `src/CMakeLists.txt` | Add `drivers/payload/`, `services/payload/`, `tasks/payload_task.c` |
| `tests/CMakeLists.txt` | Add new unit and integration test targets |

---

## Work Packages

### WP-7.1 — FMM Extension (Track A)

**Objective**: Add `FM_PAYLOAD` to the FMM state machine and validate.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.1.1 | Update `flight_mode.h`: add `FM_PAYLOAD = 5` | SW | 15 min | ✅ Done |
| T-7.1.2 | Update `flight_mode_manager.c`: expand `g_allowed[][]` to 6×6; add `FM_PAYLOAD` entries (NOMINAL↔PAYLOAD); update `fmm_mode_name()` | SW | 1 h | ⏳ |
| T-7.1.3 | Update FMM unit tests: add T-FMM-02/03 cases for new row/column | SW | 1 h | ⏳ |
| T-7.1.4 | Regression: all 29 existing tests pass after matrix change | SW | 15 min | ⏳ |
| T-7.1.5 | Update FMM-DES-001 v0.4: transition matrix, subsystem table, HK encoding | DOCS | 1 h | ✅ Done |

**Exit criteria**: `bash scripts/pico_ci.sh host-test` passes 29+N/29+N (N = new FMM tests).

---

### WP-7.2 — RM3100 Magnetometer Driver (Track B)

**Objective**: Implement and unit-test the PNI RM3100 I2C driver.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.2.1 | Create `include/rm3100.h`: API `rm3100_init()`, `rm3100_read()`, `rm3100_configCMM()` | SW | 1 h | ⏳ |
| T-7.2.2 | Implement `src/drivers/payload/rm3100.c`: CMM config (CMXYZ register), DRDY poll, 3-axis read, nT conversion (×13 nT/count) | SW | 3 h | ⏳ |
| T-7.2.3 | Unit tests `test_rm3100.c` (T-PLD-MAG-01..04): mock I2C; init correctness; read scaling; NACK handling | SW | 2 h | ⏳ |
| T-7.2.4 | Hardware validation: read RM3100 on real Pico 2W; mag vector |measured| ≈ 40–60 µT at Bs.As. | HW | 2 h | ⏳ |

**Exit criteria**: T-PLD-MAG-01..04 pass on host build; hardware read within expected Earth-field range.

---

### WP-7.3 — Radiation Driver (Track B)

**Objective**: Implement and unit-test the PIN diode ADC radiation detector driver.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.3.1 | Create `include/radiation_driver.h`: API `radiation_init()`, `radiation_read()`, `radiation_reset()` | SW | 30 min | ⏳ |
| T-7.3.2 | Implement `src/drivers/payload/radiation_driver.c`: ADC1 single channel read; dose accumulator struct; GPIO2 reset; basic calibration constant in `config.h` | SW | 2 h | ⏳ |
| T-7.3.3 | Unit tests `test_radiation_driver.c` (T-PLD-RAD-01..02): mock ADC; dose conversion; saturation handling | SW | 1.5 h | ⏳ |
| T-7.3.4 | Hardware validation: inject known voltage reference at GPIO27; verify reading within ±1% | HW | 1 h | ⏳ |

**Exit criteria**: T-PLD-RAD-01..02 pass on host build.

---

### WP-7.4 — Camera Driver (Track B)

**Objective**: Implement SPI camera interface for IMX219 via SPI bridge board.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.4.1 | Select and receive Arducam IMX219 SPI module; confirm SPI protocol and register map | HW | 1 week lead | ⏳ |
| T-7.4.2 | Create `include/camera_driver.h`: API `camera_init()`, `camera_trigger_capture()`, `camera_readout_spi()` | SW | 1 h | ⏳ |
| T-7.4.3 | Implement `src/drivers/payload/camera_driver.c`: SPI1 init (GPIO10/11/12/13); trigger pulse GPIO22; SPI frame read into RAM buffer | SW | 4 h | ⏳ |
| T-7.4.4 | Unit tests (host mock SPI): trigger sequence; frame length bounds check | SW | 2 h | ⏳ |
| T-7.4.5 | Hardware validation: capture JPEG on Pico 2W; verify size 50–500 KB; no SPI errors | HW | 3 h | ⏳ |

**Exit criteria**: JPEG image captured, stored, size within spec, no SPI errors on hardware.

---

### WP-7.5 — GPIO10 / RW3 Conflict Resolution (Track C)

**Objective**: Free GPIO10 for SPI1 SCK by moving RW3 PWM to GPIO3.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.5.1 | Update `config/pico_pins.h`: `RW_MOTOR3_PIN` GPIO10 → GPIO3; `SPI1_SCK_PIN = 10` | SW | 30 min | ⏳ |
| T-7.5.2 | Update `src/actuators/reaction_wheel.c` / CMakeLists if hardcoded GPIO | SW | 30 min | ⏳ |
| T-7.5.3 | Verify PWM slice: GPIO3 → PWM1B (RP2350 datasheet confirmation) | HW | 30 min | ⏳ |
| T-7.5.4 | PCB trace change on proto board or flying-wire on breadboard | HW | 1 h | ⏳ |
| T-7.5.5 | Regression: RW3 PWM waveform correct on oscilloscope after move | HW | 30 min | ⏳ |

**Exit criteria**: SPI1 on GPIO10/11/12/13 usable; RW3 PWM verified on new GPIO3.

---

### WP-7.6 — External Storage (Track C)

**Objective**: Add ≥ 1 GB non-volatile storage for payload science data.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.6.1 | Select storage: microSD over SPI0 (FATFS) vs W25Q128 SPI flash | HW/SW | 2 h | ⏳ |
| T-7.6.2 | Implement or port FATFS / SPIFFS driver for selected storage | SW | 8 h | ⏳ |
| T-7.6.3 | Unit tests: file write/read round-trip; storage-full handling | SW | 3 h | ⏳ |
| T-7.6.4 | Hardware validation: write 1 MB of mock data; read back without error | HW | 2 h | ⏳ |
| T-7.6.5 | Integrate with `payload_manager.c`: `flash_store_mag()`, `flash_store_rad()`, `flash_store_image()` | SW | 3 h | ⏳ |
| T-7.6.6 | Ground downlink: add CSP file-transfer service for payload data chunks | SW | 6 h | ⏳ |

**Recommendation**: microSD (≥ 2 GB, SPI0 on GPIO2/3 or separate SPI bus) provides the
simplest path to PLD-R-003 (≥ 1 GB). W25Q128 (16 MB) is cheaper but only supports
~2 days of buffering.

**Exit criteria**: 1 MB payload data set written and read back correctly; CSP file-transfer
service delivers file in chunks over UART1.

---

### WP-7.7 — PayloadTask and payload_manager (Track B)

**Objective**: Integrate all drivers into a FreeRTOS task with proper mode-aware control.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.7.1 | Create `include/payload_task.h` + `include/payload_manager.h` | SW | 30 min | ⏳ |
| T-7.7.2 | Implement `src/services/payload/payload_manager.c`: PAYLOAD_ENABLE GPIO21; idempotent enable/disable; status fields in data_layer | SW | 2 h | ⏳ |
| T-7.7.3 | Implement `src/tasks/payload_task.c`: 100 ms `vTaskDelayUntil` loop; MAG@10 Hz; RAD@1 Hz; CAM on `xTaskNotifyWait`; FM_PAYLOAD gate; rail enable/disable | SW | 4 h | ⏳ |
| T-7.7.4 | Unit tests `test_payload_task.c` (T-PLD-TSK-01..06): mode gate; timing; CAM notification; rail GPIO; storage-full fault | SW | 3 h | ⏳ |
| T-7.7.5 | Integration with `obc_main.c`: add `xTaskCreate(payload_task, …, priority=3)` | SW | 30 min | ⏳ |
| T-7.7.6 | Update telemetry: add payload HK packet (mag field, rad dose, cam image count) in `telemetry_task.c` | SW | 2 h | ⏳ |
| T-7.7.7 | Update command decoder: add `CMD_CAM_CAPTURE`, `CMD_ENABLE_PAYLOAD`, `CMD_DISABLE_PAYLOAD` in `command_task.c` | SW | 2 h | ⏳ |

**Exit criteria**: T-PLD-TSK-01..06 pass; payload HK visible in telemetry stream in emulator.

---

### WP-7.8 — Integration Tests (Track B)

**Objective**: Validate end-to-end payload pipeline including FMM transitions.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.8.1 | `test_payload_fmm.c` T-PLD-INT-01: FM_NOMINAL → FM_PAYLOAD → FM_NOMINAL; PAYLOAD_ENABLE assert/deassert | SW | 2 h | ⏳ |
| T-7.8.2 | T-PLD-INT-02: FAULT_LEVEL_CRITICAL forces FM_SAFE from FM_PAYLOAD; rail off | SW | 1 h | ⏳ |
| T-7.8.3 | T-PLD-INT-03: Telemetry contains non-zero MAG/RAD fields in FM_PAYLOAD | SW | 1 h | ⏳ |
| T-7.8.4 | T-PLD-INT-04: Ground command triggers image capture; storage count increments | SW | 1.5 h | ⏳ |
| T-7.8.5 | Run full CI (all 6 stages) after all WPs complete | SW | — | ⏳ |

**Exit criteria**: All T-PLD-INT-01..04 pass; `bash scripts/pico_ci.sh all` green on all stages.

---

### WP-7.9 — Documentation and Verification Closure (Cross-cutting)

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.9.1 | PAYLOAD-SPEC-001 §13 compliance matrix: fill verification results | DOCS | 1 h | ⏳ |
| T-7.9.2 | Update RTM-OBC-001: trace FR-13..17, PLD-R-001..005 to test IDs | DOCS | 1 h | ⏳ |
| T-7.9.3 | Update BOM-OBC-001: add RM3100, Arducam module, PIN diode, storage (SD/W25Q) | DOCS | 1 h | ⏳ |
| T-7.9.4 | Update CHANGELOG.md with Phase 7 entry | DOCS | 15 min | ⏳ |
| T-7.9.5 | Update PROJECT_PROGRESS.md: mark Phase 7 in-progress | DOCS | 15 min | ⏳ |

---

### WP-7.10 — GPS NEO-7M Integration (Track D)

**Objective**: Formally integrate the GPS module (GY-NEO6Mv2 / NEO-7M) on UART0 (GPIO0/1),
implementing the NMEA driver, FreeRTOS task, UTC time synchronization, and telemetry fields.
Resolves AIR-OBC-001 ACT-06 (Option A).

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-7.10.1 | Create `include/gps_driver.h`: API `gps_init()`, `gps_read_fix()`, `gps_get_last_fix()` | SW | 30 min | ⏳ |
| T-7.10.2 | Implement `src/drivers/gps/neo7m.c`: UART0 init (9600 baud), NMEA sentence tokenizer, `$GPGGA`/`$GPRMC` parser, fix validity check (field count + checksum) | SW | 3 h | ⏳ |
| T-7.10.3 | Implement `src/tasks/gps_task.c`: 1 Hz `vTaskDelayUntil` loop; call `gps_read_fix()`; write to `data_layer_set_gps_fix()` | SW | 2 h | ⏳ |
| T-7.10.4 | Data Layer extension: add `gps_fix_t` (lat, lon, alt\_m, utc\_s, fix\_valid) to `obc_snapshot_t`; implement `data_layer_set/get_gps_fix()` | SW | 1 h | ⏳ |
| T-7.10.5 | Unit tests `tests/unit/test_gps.c` (T-GPS-01..04): mock UART buffer; parse `$GPGGA` valid; parse `$GPRMC` valid; invalid checksum rejected; stale-fix flag after 5 s | SW | 2 h | ⏳ |
| T-7.10.6 | UTC sync: on valid `$GPRMC` fix call `rtc_set_datetime()`; verify accuracy ≤ ± 500 ms (FR-19) | SW | 1 h | ⏳ |
| T-7.10.7 | Add `FAULT_GPS_TIMEOUT` (UART0 silent > 10 s) and `FAULT_GPS_PARSE_ERR` to `fault_ids.h`; add fault reports in driver | SW | 1 h | ⏳ |
| T-7.10.8 | Update telemetry: add `lat`, `lon`, `alt_m`, `utc_s`, `gps_valid` fields to HK packet in `telemetry_task.c` | SW | 1 h | ⏳ |
| T-7.10.9 | Hardware validation: connect GY-NEO6Mv2 to GPIO0/1; verify cold-start fix ≤ 5 min (clear sky); NMEA sentences visible in USB CDC monitor | HW | 2 h | ⏳ |

**Exit criteria**: T-GPS-01..04 pass on host build; GPS fix visible in telemetry HK with non-zero lat/lon; hardware cold-start fix confirmed.

---

## Schedule

```
2026-Apr-01  WP-7.1 start (FMM extension — depends on Phase 6 merge)
2026-Apr-01  WP-7.2, 7.3, 7.10 start (drivers — host build, no HW required)
2026-Apr-08  Hardware procurement complete (RM3100, Arducam, PIN diode, GY-NEO6Mv2)
2026-Apr-08  WP-7.5 start (GPIO conflict — first HW task)
2026-Apr-15  WP-7.2, 7.3 complete; WP-7.4 camera driver start
2026-Apr-22  WP-7.6 storage selection and driver complete
2026-Apr-29  WP-7.7 payload task + manager complete
2026-May-06  WP-7.8 integration tests complete
2026-May-13  WP-7.9 docs + RTM closure
2026-May-20  Phase 7 PR review; merge to dev
```

Total calendar time: ~7 weeks.

---

## Hardware Procurement List

| Item | Model | Qty | Est. Cost | Lead time | Source |
|------|-------|-----|-----------|-----------|--------|
| Scientific Magnetometer | PNI RM3100 breakout | 1 | ~$170 | 4–6 weeks | Mouser / DigiKey |
| Camera module | Arducam Mini 2MP SPI (OV2640) or Arducam IMX219 SPI | 1 | ~$30 | 1–2 weeks | Arducam directly |
| PIN diode | Hamamatsu S1223-01 (or equivalent 1 cm² Si PIN) | 1 | ~$20 | 1–2 weeks | Digi-Key |
| TIA front-end | OPAx134 or similar low-bias JFET op-amp + passives | 1 kit | ~$10 | 1 week | Digi-Key |
| Storage | SPI microSD breakout (OTRONIC or Adafruit) | 1 | ~$5 | 1 week | local / Amazon |
| microSD card | 2 GB Class 10 (SDHC) | 1 | ~$5 | local | local |
| GPS Module | GY-NEO6Mv2 / NEO-7M + patch antenna | 2 | ~$15 | 1 week | Online / AliExpress |
| **Total** | | | **~$270** | | |

> **Note**: If Arducam IMX219 SPI module is unavailable, the OV2640-based
> Arducam Mini 2MP (2 MP, 20 fps, native SPI) is a compatible drop-in with
> a simpler driver, at ~8 MP → 2 MP image quality trade-off.

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| IMX219 SPI bridge module unavailable | Medium | High | Use OV2640 Arducam SPI module as fallback; lower resolution (2 MP) but identical interface |
| RM3100 4–6 week lead time delays Phase 7 | Medium | Medium | Begin SW driver development with I2C mock; start HW validation later |
| GPIO10 / RW3 PCB conflict on proto board | Low | Medium | Use flying-wire hack on breadboard for Phase 7; fix on next PCB spin |
| SPI frame rate insufficient for 8 MP at 10 MHz | Medium | Medium | Reduce resolution to 960×720 (lower JPEG, ~100 KB); acceptable for LEO observation |
| External storage (SD/flash) SPI conflicts with SPI1 camera bus | Low | High | Use separate SPI0 for storage (GPIO2/3) while SPI1 dedicates to camera |
| FreeRTOS task count increase causes stack overflow | Low | Medium | Increase `configTOTAL_HEAP_SIZE` in FreeRTOSConfig.h; monitor with `uxTaskGetStackHighWaterMark()` |
| Ground downlink insufficient for all payload data | High | Medium | Implement per-instrument decimation selectable by ground command (already architected in PAYLOAD-SPEC-001 §10.2) |
| GPS cold-start time > 5 min in lab (no clear-sky view) | Medium | Low | Pre-load almanac via UBX `CFG-AOP`; hot-start < 1 s; use external clear-sky window for first fix |
| UART0 / debug pin conflict | Resolved | — | Debug fully routed to USB CDC (`pico_enable_stdio_usb=1`); UART0 exclusively available for GPS |

---

## Definition of Done

Phase 7 is **complete** when:

- [ ] All WP-7.1 through WP-7.9 tasks are ✅ Done
- [ ] `bash scripts/pico_ci.sh all` exits 0 with all 6 stages green
- [ ] Unit test count ≥ 43 (29 existing + 10 payload + 4 GPS tests)
- [ ] `cppcheck` reports 0 errors / 0 warnings on new `src/drivers/payload/` and `src/tasks/payload_task.c`
- [ ] FM_PAYLOAD → FM_SAFE transition confirmed on hardware (LED indicator)
- [ ] At least one JPEG image captured and stored on external media in flight-software context
- [ ] PAYLOAD-SPEC-001 §13 compliance matrix fully populated
- [ ] RTM-OBC-001 traces updated for FR-13..19 and PLD-R-001..005
- [ ] GPS fix visible in telemetry HK packet on hardware (lat/lon/UTC non-zero after ≤ 5 min with clear sky)
- [ ] Phase 7 PR reviewed and merged to `dev`
