# Changelog

All notable changes to the CubeSat OBC project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased] — — GPS Integration Complete + BOM Update

### Added
- **BOM-OBC-001.md v1.1** (2026-05-05): Major documentation update
  - Added: SHT31 (I2C 0x44), BH1750 (I2C 0x23), DS3231 (I2C 0x68), INA219 (I2C 0x40), Sun Sensors (GPIO27/28), W25Q64 SPI Flash
  - Updated: UART1 pins → GPIO8/9, I2C pins confirmed GPIO4/5
  - Clarified: Magnetometer as QMC5883L clone (0x0D)
  - Added: Complete pin assignment table (§11) and I2C address table (§11.1)
  - GPS: ✅ Integrated with patch antenna — receiving satellite signals

### Changed
- **ICD-OBC-001.md**: Updated I2C addresses (MPU-6050 → 0x70, full device table)
- **ICD-OBC-001.md §7**: GPS status updated — hardware verified with patch antenna

### Verified
- GPS NEO-7M: Patch antenna receiving satellite signals correctly
- All I2C sensors on shared bus confirmed working

---

## [0.28.0] — 2026-04-25 — Sun Sensor Telemetry + Ground Station

### Added
- **Dual-Axis Photodiode Sun Sensor Driver**: New hardware driver for ADCS sun sensing
  - New files: `include/sun_sensor.h`, `src/drivers/sun_sensor.c`, `src/drivers/sun_sensor_stub.c`
  - Hardware: Photodiodes (e.g., BPW21) connected to ADC1 (GPIO27) and ADC2 (GPIO28)
  - Pull-down: 10 kΩ resistors required for voltage divider circuit
  - API: `sun_sensor_init()`, `sun_sensor_read()`, `sun_sensor_is_sun_visible()`
  - Output: Raw ADC values (0-4095) + normalized intensity (0.0-1.0)
  - Integration: `sensor_read_task.c` reads sun sensor data at 100 Hz
  - Testing: Stub for host-tests, all unit tests pass
  - ICD documentation: Updated ICD-OBC-001 (§11 Sun Sensor Interface)

- **Telemetry Format Switching**: Toggle between JSON and TEXT formats via ground station
  - Command: `TLMFMT=JSON` or `TLMFMT=TEXT`
  - JSON format: `[JSON] {ts:1777141445,m:2,a:0.0,-0.1,2.3,t:22.9,h:59.3,l:25.8,...}`
  - TEXT format: `[TLM] m=2 a=0.0,-0.1,2.3 t=22.9 h=59.3 l=25.8 ...`
  - API: `telemetry_set_format()`, `telemetry_get_format()` in `telemetry_task.h`

- **CRC8 Telemetry Checksum**: End-to-end integrity verification
  - Polynomial: 0x07 (CASPAC standard)
  - Appending: `c:XX` hex in JSON format
  - Unit tests: `test_crc8.c` validates all polynomial cases

- **SHT31 Periodic Mode**: Non-blocking 10Hz sensor reads
  - New API: `sht31_start_periodic(rate_hz)`, `sht31_fetch()`
  - Replaces blocking single-shot mode that caused corrupted FF FF FF humidity data
  - Retry logic with cached value fallback for transient failures

### Fixed
- **Ground Station JSON Parser** (`examples/arduino_ground_station/`): Robust key-based parser
  - Fixed parsing of compact JSON telemetry (key-value format without nesting)
  - All fields parsed: timestamp, mode, attitude, temperature, humidity, GPS, power, sun sensor
  - Removed debug prints for clean serial output

- **MODE Command Error Handling** (`command_task.c`): Show actual transition result
  - Before: Always returned `[CMD] MODE=X OK` even when transition was denied
  - After: Returns `[CMD] MODE=X OK`, `NOT ALLOWED`, or `BLOCKED BY FAULT`
  - Flight mode transitions now correctly reflected in telemetry

### Verified
- Flatsat test: Sun sensor reads ~3700-3800 ADC (0.91 intensity) in direct light
- Flatsat test: Sun sensor reads ~1700-2100 ADC (0.43-0.53 intensity) with ambient light
- Flatsat test: Sun sensor reads ~10-50 ADC (<0.01 intensity) when covered/dark
- Ground station: MODE=1 (SAFE), MODE=2 (DETUMBLE), MODE=3 (NOMINAL after DETUMBLE) all working
- Telemetry: Sun=[0.38,0.38] values reflecting actual sun sensor readings

---

## [0.27.1] — 2026-04-16 — Coverity Bug Fixes

### Fixed
- **Coverity Scan Bug Fixes (CDR-SAF-05)**: Fixed 2 critical bugs identified by Coverity static analysis
  - `event_logger.c` (CID 1654913): Out-of-bounds write in `log_event()` — added bounds check before array write to prevent buffer overflow when ring is full
  - `command_task.c` (CID 1654935/1654921): Buffer overflow in `CMD_TELEMETRY_DUMP` — `telemetry_dump_response_t` (76 bytes) exceeds `payload[32]`; fixed by using `packet->data` directly (256 bytes via `CSP_BUFFER_SIZE`)
  - Remaining 50 defects: 23 third-party (libcsp/fatfs), 18 test false positives (Ceedling), 2 edge cases, 7 pending classification

## [0.27.0] — 2026-04-16 — Safety Analysis Extensions

### Added
- **Fault Injection Test Suite (CDR-SAF-04 / OI-SW-5)**: Comprehensive fault injection for all 25 fault IDs
  - New test: `tests/unit/test_fault_injection.c` (T-FI-01..08, 306 checks)
  - T-FI-01: All 25 fault IDs injectable, trackable, clearable across 10 subsystems
  - T-FI-02: WARNING faults auto-clear after WARN_AUTO_CLEAR_TICKS
  - T-FI-03: ERROR faults persist indefinitely (only explicit clear removes)
  - T-FI-04: CRITICAL faults trigger FM_SAFE from all 5 flight modes (FM_BOOT/SAFE/NOMINAL/DETUMBLE/DIAGNOSTIC)
  - T-FI-05: Level precedence (NONE < WARNING < ERROR < CRITICAL)
  - T-FI-06: Cross-subsystem multi-fault matrix — 10 simultaneous faults
  - T-FI-07: Count increment per fault ID; clear() keeps slot, re-report bumps count
  - T-FI-08: Unknown fault IDs accepted for reporting; get_event=false for unmapped
  - Host-testable: all faults injectable via software (no hardware required)
  - Test suite: 49/49 tests passing
  - RTM: CDR-SAF-04 marked ✅ Implemented; FMEA: OI-SW-5 marked ✅ Implemented

- **StartupTask ALIVE Loop Evaluation (CDR-SAF-06 / CDR-SW-05)**: Evaluation of necessity and safety impact
  - New document: `docs/ecss/safety/startup_alive_loop_eval.md` (STA-OBC-001 v1.0)
  - Finding: ALIVE loop is NOT safety-critical — FDIR handled by HealthMonitor,
    watchdog fed by HealthMonitor, stack overflow detected via separate
    scratch-register mechanism in main()
  - Recommendation: Retain ALIVE loop — provides diagnostic HWM + heap telemetry
    at negligible cost (~8 KB RAM, ~0.1% CPU at 5 s period); avoids SMP
    vTaskDelete() uncertainty; alternative HWM-in-HealthMonitor documented
  - RTM: CDR-SAF-06 marked ✅ Evaluated

- **Priority Inheritance Analysis (CDR-SAF-02 / OI-SW-3)**: FreeRTOS mutex priority inheritance documentation
  - New document: `docs/ecss/safety/priority_inheritance.md` (PI-OBC-001 v1.0)
  - Covers: priority inversion problem, inheritance mechanism, mutex inventory
    (`g_dl_mutex`, `driver_instance.lock`, `g_gps_mutex`), task priority map,
    potential issues, runtime verification via `uxTaskPriorityGet()`, compliance table
  - RTM: CDR-SAF-02 marked ✅ Implemented; FMEA: OI-SW-3 marked ✅ Implemented

- **Coverity Scan CI Integration (CDR-SAF-05 / OI-SW-4)**: Deep static analysis in CI pipeline
  - New script: `scripts/coverity_scan.sh` — local Coverity scan (configure → cov-build → cov-analyze → cov-format-errors)
  - Added stage `5b coverity` to `scripts/pico_ci.sh` (soft-fail if tool not installed)
  - GitHub Actions: `coverity-scan` job using `vapier/coverity-scan-action@v1` on main/dev push (skips PRs)
  - Requires secrets: `COVERITY_SCAN_EMAIL` + `COVERITY_SCAN_TOKEN` (from scan.coverity.com project settings)
  - FMEA: OI-SW-4 marked ✅ Implemented
  - RTM: Added "CDR Safety & Analysis Requirements" section with CDR-SAF-01..06

- **WCET Profiler (CDR-HW-06 / OI-3)**: DWT cycle counter instrumentation for all FreeRTOS tasks
  - New module: `include/wcet_profiler.h`, `src/services/wcet/wcet_profiler_pico.c` (RP2350 DWT), `src/services/wcet/wcet_profiler_host.c` (no-op stub)
  - `wcet_profiler_init()` enables DWT->CYCCNT (133 MHz on RP2350) via ARM Cortex-M33 DWT unit; `DWT->LAR` unlock sequence for ARMv8-M security
  - `wcet_task_begin/end(task_id)` wrap each task's work section; ISR-safe (single LDR)
  - `wcet_profiler_print_report()` outputs WCET(cycles), WCET(μs), Avg(μs), Samples, CPU Load(‰) table
  - Instrumented: SensorRead, AttitudeCtrl, Telemetry, HealthMon, Command, GPS, Payload
  - Integrated into: `sensor_read_task.c`, `attitude_control_task.c`, `telemetry_task.c`, `health_monitor_task.c`, `gps_task.c`, `payload_task.c`, `command_task.c`, `obc_main.c`
  - On host builds: all functions are no-ops (prints "WCET profiling disabled")

## [0.26.0] — 2026-04-16 — Multi-Sensor Integration

### Added
- **Unit Tests**: Added `test_ds3231.c` (T-DS3231-01..09: init, is_present, read_time, set_time, epoch conversion, data layer) and `test_sht31.c` (T-SHT31-01..11: init, is_present, constants, heater, data layer integration, range validation). All 48 tests passing (was 46).
  - Added FR-13..FR-17 (payload: camera, mag, radiation, power rail, HK telemetry)
  - Added FR-18 (GPS NMEA parsing), FR-19 (GPS UTC synchronisation)
  - Added OR-1..OR-4 (operational monitoring: INA219, DS3231, BH1750, SHT31)
  - Added HW-11..HW-14 (new hardware entries for monitoring components)
  - Updated HW-04 to reflect GPS partial implementation
  - Added traceability gaps for missing tests and SRS integration
- **INA219 Power Monitor**: High-side current/power sensor (I2C 0x40, 0.1 ohm shunt)
  - Driver: `include/ina219.h`, `src/drivers/power/ina219.c`, `src/drivers/power/ina219_stub.c`
  - Integration: Data layer with `bus_voltage_mv`, `current_ua`, `power_uw`, `power_valid`, `power_available`
  - Telemetry: Added `bus_voltage_mv`, `current_ma`, `power_mw` fields to CSP packet, updated flags (bit 6 = power_valid)
  - Debug: Added `POWER_TEST` command to read and display power data
  - Sampling: 1 Hz (every 10 cycles at 10 Hz task rate)
  - Safety: Threshold checks with fault reporting (V<3V critical, V>5.5V warning, I>500mA error, P>2W warning)
  - Hardware verified: V=5724mV, I=5mA, P=28mW
- **DS3231 RTC**: Real-time clock with battery backup (I2C 0x68, ±2 ppm accuracy)
  - Driver: `include/ds3231.h`, `src/drivers/rtc/ds3231.c`, `src/drivers/rtc/ds3231_stub.c`
  - Integration: Data layer, system state with `rtc_timestamp` (Unix epoch), `rtc_valid`, `rtc_available`
  - Telemetry: Added `rtc_timestamp` field to CSP packet, updated flags (bit 4 = rtc_valid)
  - Debug: Added `RTC_TEST` command to read and display RTC time
- **BH1750 Light Sensor**: Digital illuminance sensor driver (I2C 0x23, 0.5 lux resolution, 16ms measurement)
  - Driver: `include/bh1750.h`, `src/drivers/light/bh1750.c`, `src/drivers/light/bh1750_stub.c`
  - Integration: Data layer, system state, telemetry packet with `lux` field
  - Commands: `I2CSCAN` (scan I2C bus), `BH1750_TEST` (test BH1750 at address)
  - Ground station: Arduino now parses and displays `lux` value
- **GPS Driver Enhancements**: Status reporting and telemetry statistics integration in `gps_driver.h` and `neo7m.c`
- **Ground Station Communication**: Enhanced telemetry and GPS status reporting capabilities
- **Command Task Updates**: Extended command parsing for GPS-related operations

### Changed
- **GPS Stub and Tests**: Updated mocks and unit tests for enhanced GPS functionality

---

## [0.25.0] — 2026-03-30 — Hardware Validation Complete

### Added
- **Ground Station Arduino Code**: `examples/arduino_ground_station/ground_station.ino` - 
  Complete ground station implementation using Arduino Nano as HC-12 bridge with telemetry
  parsing and command sending.
- **Text Command Parser**: Implemented in `command_task.c` - Pico now accepts text commands
  via HC-12 radio: REBOOT, STATUS, ECHO, CAPTURE, MODE=1/2/3, HELP.
- **Text Telemetry**: Added plain text telemetry output via UART1 (HC-12) for easy debugging:
  `[TLM] mode=X att=R,P,Y flags=0xXX gps_lat=X gps_lon=X gps_alt=X gps_valid=X`

### Changed
- **UART1 pins**: Moved from GPIO4/5 to GPIO8/9 to avoid conflict with I2C0 (MPU-6050).
- **I2C pins**: Changed from I2C1 to I2C0 for MPU-6050 (GPIO4/5).
- **Baud rates**: HC-12 now operates at 9600 baud (was 115200).

### Hardware Verified
| Component | Status | Notes |
|-----------|--------|-------|
| MPU-6050/6500 IMU | ✅ OK | Detected ID 0x70 (MPU-6500) |
| Temperature | ✅ OK | Integrated in MPU-6050 |
| GPS NEO-6M/7M | ✅ OK | 9600 baud, fix obtained |
| HC-12 Radio | ✅ OK | 9600 baud, bidirectional |
| Telemetry (RF) | ✅ OK | Text format working |
| Commands (RF) | ✅ OK | REBOOT, MODE, ECHO, CAPTURE |
| Magnetometer | ❌ Not connected | HMC5883L pending |
| Camera | ❌ Not connected | OV2640 pending |

### Testing
- CI pipeline: **6/6 stages passing** (host-test, pico-build, static analysis, coverage)
- All commands tested and working via HC-12 radio link

---

## [0.24.0] — 2026-03-20 — phase7-payload integration

### Added
- **GPS Integration (NEO-7M)**: Full driver implementation (`src/drivers/gps/neo7m.c`) with NMEA
  parser for `$GPGGA`/`$GPRMC` sentences, UART0 @ 9600 baud, 1 Hz GpsTask with UTC sync,
  `gps_fix_t` extended to Data Layer, telemetry fields (lat/lon/alt/utc/valid). Resolves
  AIR-OBC-001 ACT-06 Option A.
- **IMU Integration**: `test_imu_integration.c` validates IMU driver and telemetry data flow.
  Fixed `temp_sensor_available` → `temp_available` naming in test assertions.
- **Magnetometer Integration**: `test_mag_integration.c` validates EKF operations using
  magnetometer data. Fixed incorrect function calls to `ekf_init`, `ekf_predict`,
  `ekf_update_mag`, `ekf_get_quaternion`, and `data_layer_write_ekf` with correct parameters.

### Changed
- **Radiation Test Disabled**: `test_radiation_integration.c` uses unsupported APIs and has
  been disabled in CI. Tracked for future implementation in Phase 8.

### Testing
- CI pipeline: **6/6 stages passing** (host-test, pico-build, static analysis, coverage)
- Line coverage: **93.0%**
- Function coverage: **92.4%**

---

## [0.23.0] — 2026-03-09 — feature/cdr-budgets-fmea (#26)

### Added
- **FMEA-OBC-001 v0.1** (`docs/ecss/design/FMEA-OBC-001.md`): CDR Failure Mode and Effects
  Analysis — all 25 fault IDs from `fault_ids.h` (10 subsystems); S/O/D/RPN scoring; FDIR
  response mapping (LOG/HK/FS/WDT/AUTO-CLR); MRD §6.2.1 hazard coverage (H-1..H-5); 9-item
  RPN priority list; SPF analysis (EPS emergency, I²C bus-stuck, over-temperature); mitigation
  status with Phase 2/3 tracking (M-01..M-10); 4 open items.
- **POWER-BDG-001 v0.2** (`docs/ecss/design/POWER-BDG-001.md`): CDR Power Budget — orbital
  parameters (600 km SSO, 37 min max eclipse); solar array model (EOL 14.1 W, 14.0 Wh/orbit);
  battery model (2S 2000 mAh, 40% DoD, 4.74 Wh usable EOL); Phase 1 load breakdown (FM_SAFE
  351 mW / FM_NOMINAL 421 mW / FM_DETUMBLE 621 mW / peak 3.92 W); eclipse survival analysis
  (458 min @ FM_DETUMBLE — 12× 37-min requirement); MIS-PB-001 PASS (+34 min margin);
  MIS-PB-002 PASS (LDO works to 3 V input); GPS load corrected to 120 mW (NEO-7M datasheet);
  TX PA efficiency concern flagged (OI-7); CDR readiness Q&A §15; power rail shedding order
  table; 7 open items.
- **LINK-BDG-001 v0.2** (`docs/ecss/design/LINK-BDG-001.md`): CDR Link Budget — 433 MHz LoRa
  E22-400M30S; SF9 BW 125 kHz CR 4/5 (3 906 bps raw / ~1 758 bps payload); SC antenna revised
  to −3 dBi tumble-averaged (§5.3 radiation pattern analysis); TX uprated to +30 dBm maintaining
  EIRP +26.5 dBm; DL worst-case (5°, 2300 km) margin **+9.6 dB** (MIS-C-003 ≥ +8 dB ✅);
  UL margin +9.6 dB; HK 10.7% of capacity (MIS-DB-001 ✅); Doppler ±11 kHz << BW (no AFC);
  MIS-DB-002 shortfall: Phase 3 W25Qxx resolves to ≥ 131 072 events; CDR readiness Q&A §15;
  6 open items.

---

## [0.22.0] — 2026-03-09 — feature/fsw-sdd-001 (#25)

### Added
- **FSW-SDD-001 v0.2** (`docs/ecss/design/FSW-SDD-001.md`): Revision addressing CDR review
  observations — §7.5 HK telemetry packet table (13 fields, ~42 B, CSP port 10, little-endian);
  §7.6 Command ACK format (`result`/`echo_seq`/`reason` fields); §8.1 FMM transition conditions
  table (6 arcs with explicit triggers and `fmm_force_safe()` FDIR path); §8.3 EPS execution
  context clarification (`eps_monitor_tick()` called by `vHealthMonitorTask` at 1 Hz, WDT
  liveness guarantee); §8.4 logger flash driver target named (W25Qxx SPI NOR, Phase 3);
  §13.2 RP2350 physical SRAM bank map (SRAM0–SRAM5 with base addresses, sizes, aliases, and
  linker-level region breakdown); §13.3 heap budget split by host vs Pico build, revealing
  ~82 KB needed vs 60 KB configured (OI-8 HIGH); §14 CPU Budget Estimate (8-task load table,
  ~13% Core 0 total, DWT profiling planned OI-3); OI-2 updated with W25Qxx target; OI-4
  updated with explicit CDR single-core baseline; OI-8 added (heap sizing — HIGH priority);
  §17 traceability note referencing RTM-OBC-001; §15–19 section renumbering.

- **FSW-SDD-001 v0.1** (`docs/ecss/design/FSW-SDD-001.md`): Flight Software Design Description —
  CDR-level consolidated FSW design document with 18 sections: software architecture overview
  (4-layer: Task/Service/DLA/Driver), module inventory (full `src/` directory tree + call graph),
  task design (9 tasks with priority rationale, period, HWM, stack budget), service layer design
  (FMM state machine, Fault Manager escalation path, EPS Schmidt trigger, Event Logger 3-class
  storage, EKF 6-state filter, LQR gain scheduling, Momentum Dump, CommInit), DLA API and
  staleness detection, Driver/HAL layer (I²C, MPU6050, HMC5883L, temperature, UART, watchdog),
  boot sequence call graph with SMP spinlock rationale, inter-task communication (DLA-only,
  no queue sharing), memory budget (heap + per-task HWM table), FDIR integration (fault severity
  mapping, watchdog crash recovery, ISR safety table), build system portability table, 12-row
  traceability to SRS requirements, 7 open items. Cross-references all 8 `*-DES-001` docs.

---

## [0.21.0] — feature/sdp-rev1-mrr-observations

### Changed
- **SDP-OBC-001 v1.1** (`docs/ecss/requirements/SDP-OBC-001.md`): Revision addressing MRR review
  observations — added §6.1 Programme Review Schedule (MRR/SRR/PDR/CDR/TRR/AR/Launch milestone
  dates), §6.3 Verification Strategy Summary (unit/integration/HIL/environmental table),
  §10.4 Bus-Factor Mitigation (documentation completeness, repository redundancy, reproducible
  build, mentor review). New OI-4 for schedule date confirmation post-SRR gate.

---

## [0.20.0] — feature/mrd-rev1-mrr-observations

### Changed
- **MRD-OBC-001 v1.1** (`docs/ecss/requirements/MRD-OBC-001.md`): Revision addressing MRR review
  board observations — added §2.4 Mission Success Criteria table (Minimum/Nominal/Full levels),
  §2.5 lifetime justification (battery degradation, radiation, thermal cycling rationale),
  §3.3 top-level system architecture ASCII diagram (GS↔Radio↔OBC↔ADCS/Actuators/EPS),
  §6.2.1 Top Mission Hazards table (H-1..H-5 with likelihood and linked requirements),
  §6.6 Power Budget Requirements (subsystem-level power estimates, MIS-PB-001/002),
  §6.7 Data Budget Requirements (HK/TC/event-log/attitude volumes, MIS-DB-001/002).

---

## [0.19.0] — feature/obc-des-001

### Added
- **OBC-DES-001 v0.1** (`docs/ecss/design/OBC-DES-001.md`): OBC Hardware & CDH Design Document —
  RP2350/Pico 2W hardware platform (Cortex-M33, 520 KB SRAM, 2 MB flash, FPv5-SP FPU), full
  GPIO pin assignment table (UART0/1, I2C0/1, PWM×6, ADC×3, WDT kick, LED), bus architecture
  (I2C0 sensor bus 400 kHz, UART1 TT&C, PWM actuators, ADC power monitoring), FreeRTOS
  configuration (ARM_CM33_NTZ, tick=1 kHz, heap=60 KB, stack overflow detection),
  task model with HWM measurements (all tasks ≥91% headroom), boot sequence
  (`main→vStartupTask→subsystems→tasks`), power architecture (VBATT/5V/3.3V rails,
  battery voltage sense via ADC0, external watchdog TPS3431 on GPIO20), host vs target
  build differences table, performance budget (CPU < 5%, heap free 60 KB). 8 open items.

---

## [0.18.0] — feature/srr-plans

### Added
- **SDP-OBC-001 v1.0** (`docs/ecss/requirements/SDP-OBC-001.md`): Software Development Plan —
  development environment (Ubuntu 22.04, GCC, ARM GCC, Pico SDK submodule), coding standards
  (C11, no dynamic allocation, `-Wall -Wextra -pedantic`), phased lifecycle (Phase 1–7), branching
  strategy, CMake build system, CI pipeline (ubuntu-22.04 pinned), code review process, release
  management (SemVer, ECSS baseline tags), documentation management. 3 open items.
- **SVVP-OBC-001 v1.0** (`docs/ecss/verification/SVVP-OBC-001.md`): Software Verification &
  Validation Plan — 4 verification levels (Unit/Integration/System/HIL), Unity test framework,
  existing 29/29 passing test suites catalogued, integration scenarios INT-01..05, system scenarios
  SYS-01..07, static analysis plan (clang-format-14/clang-tidy-14/cppcheck), coverage targets
  (≥80% line / ≥70% branch, ≥90% for safety-critical), regression strategy, entry/exit criteria
  per review gate. 5 open items.
- **CMP-OBC-001 v1.0** (`docs/ecss/requirements/CMP-OBC-001.md`): Configuration Management Plan —
  full CI inventory (SW-001..010, DOC-001..006, TP-001..004), VCS policy (protected branches,
  PR-gated merges), baseline management (MRR/SRR/PDR/CDR tags on `main`), change control
  (Class A/B/C), build reproducibility (pinned submodule SHAs, pinned clang-format-14),
  submodule update procedure, release artefact management (UF2, coverage, test XML),
  FCA/PCA audit schedule. 4 open items.
- **RMP-OBC-001 v1.0** (`docs/ecss/requirements/RMP-OBC-001.md`): Risk Management Plan —
  5×5 likelihood/consequence matrix, 22-entry risk register across SW/HW/Schedule/Resource/
  Operational categories; critical risks: RISK-SW-001 (FreeRTOS SMP), RISK-SW-010 (watchdog
  not connected), RISK-SC-001..003 (CDR backlog / HIL / integration tests); high-priority
  mitigations with owners and target phases; risk dashboard (2 critical, 5 high, 11 medium,
  3 low, 1 closed). 4 open items.

---

## [Unreleased — merged] — feature/comms-des-001

### Added
- **COMMS-DES-001 v0.1** (`docs/ecss/design/COMMS-DES-001.md`): Communications Subsystem Design Document —
  CSP/KISS/UART1 protocol stack, OBC address 10, GS address 1, TELEMETRY_PORT=10 (1 Hz downlink),
  COMMAND_PORT=20 (uplink echo/reboot/set-mode), FreeRTOS task model (4 tasks), host vs Pico build
  behaviour, error handling, and full test mapping (T-COM-01..02, T-TLM-01..06, T-CMD-01..05,
  13/13 passing). 6 open items tracked (OI-1..OI-6).

---

## [0.17.0] — feature/fault-oi1-eps-emergency

### Changed
- **`src/services/eps/eps_monitor.c`**: Close FAULT-DES-001 OI-1 — `ENERGY_EMERGENCY` branch now
  calls `fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_CRITICAL)` (separate case, no fall-through).
  `ENERGY_NOMINAL` and `ENERGY_LOW` recovery paths also clear `FAULT_EPS_VBATT_EMERGENCY`.
- **`tests/unit/test_eps_monitor.c`**: Test 6 updated to assert `FAULT_EPS_VBATT_EMERGENCY` active
  and `FAULT_EPS_VBATT_CRITICAL` not active in EMERGENCY state. Test 3 adds EMERGENCY not-active check.
- **`docs/ecss/design/FAULT-DES-001.md`**: Bump to v0.2 — closes OI-1 and OI-2; updates Section 15
  integration table and Open Items table.

---

## [0.16.0] — feature/fault-des-001

### Added
- **FAULT-DES-001 v0.1** (`docs/ecss/design/FAULT-DES-001.md`): Fault Manager Design Document —
  32-slot flat fault table, 4-level severity model (NONE/WARNING/ERROR/CRITICAL), CRITICAL→`fmm_force_safe()`
  FDIR chain, WARNING auto-clear ageing policy (30 s), ISR-safe `taskENTER_CRITICAL` locking, fault ID
  catalogue (10 subsystems, 25 IDs), and full test mapping (T-FM-01..12, 12/12 passing).

### Changed
- **`include/fault_ids.h`**: Implements EPS-DES-001 OI-6 — adds `FAULT_EPS_VBATT_EMERGENCY` (0x0903)
  for independent emergency battery event visibility; `FAULT_EPS_OVERCURRENT` renumbered to 0x0904,
  `FAULT_EPS_READ_ERROR` to 0x0905; `FAULT_ID_COUNT` updated from 22 to 25. All 29/29 tests pass.

---

## [0.15.0] — feature/dl-des-001

### Added
- **DL-DES-001 v0.1** (`docs/ecss/design/DL-DES-001.md`): Data Layer Abstraction Design Document —
  single-mutex shared state bus (`dl_snapshot_t`), 12-function public API, seq-counter stale-copy
  detection, ISR-safety constraints, `system_state` legacy shim, units convention (rad/rad·s⁻¹/°C/V/µT),
  initialisation sequence, FreeRTOS threading model, and full test mapping (T-DL-01..09, 9/9 passing).

---

## [0.14.0] — feature/mrr-docs

### Added
- **MRD-OBC-001 v1.0** (`docs/ecss/requirements/MRD-OBC-001.md`): Mission Requirements Document /
  ConOps — mission objectives (MO-1..7), orbital parameters (SSO 600 km), 5 flight phases,
  mission requirements (MIS-P/D/C/O/E), constraints, assumptions, ground segment description.
- **SMP-OBC-001 v1.0** (`docs/ecss/requirements/SMP-OBC-001.md`): System Management Plan —
  project organisation, ECSS lifecycle model, review gate schedule (MRR→AR/QR), branching
  policy, CI/CD pipeline, risk register (R-1..6), documentation hierarchy.
- **PAP-OBC-001 v1.0** (`docs/ecss/requirements/PAP-OBC-001.md`): Product Assurance Plan —
  ECSS-Q-ST-80C Class B classification, MISRA compliance policy (0 required/mandatory violations),
  test and coverage requirements (≥90% line), non-conformance process, fail-safe analysis.

---

## [0.13.0] — feature/eps-des-001

### Added
- **EPS-DES-001 v0.2** (`docs/ecss/design/EPS-DES-001.md`): Electrical Power System Monitor Design
  Document — Schmidt-trigger hysteresis energy state model, four-rail management, FDIR authority
  chain (EPS → Fault Manager → FMM), HAL abstraction, ISR-safe critical-section locking, and
  full test mapping (T-01..12, 12/12 passing).

### Changed
- **EPS-DES-001 §9**: Split `FAULT_EPS_VBATT_EMERGENCY` (0x0903) from `FAULT_EPS_VBATT_CRITICAL`
  for independent mission-log visibility; OVERCURRENT → 0x0904, READ_ERROR → 0x0905.
- **EPS-DES-001 §11.1**: Fixed initial state — `eps_monitor_init()` now derives `prev` from raw
  classification (not assumed NOMINAL), preventing spurious transitions at boot on low battery.
- **EPS-DES-001 §8.4**: Added `eps_apply_load_policy()` definition — formalises per-state
  autonomous rail shedding (PAYLOAD off at LOW; PAYLOAD+COMMS+ADCS off at CRITICAL/EMERGENCY).
- **EPS-DES-001 §4**: Added 5 s polling period justification (battery time constant >> 5 s).

---

## [0.12.0] — feature/fmm-des-001

### Added
- **FMM-DES-001 v0.1** (`docs/ecss/design/FMM-DES-001.md`): Flight Mode Manager Design Document — formal
  specification of the five-mode state machine (`FM_BOOT`, `FM_SAFE`, `FM_DETUMBLE`, `FM_NOMINAL`,
  `FM_DIAGNOSTIC`). Covers: allowed-transition matrix, `fmm_request_transition()` / `fmm_force_safe()`
  API, fault integration (CRITICAL → FM_SAFE), EPS energy-state mapping, per-mode subsystem
  behaviour table, threading model, and verification test mapping (T-FMM-01..09, T-FMM-INT-01..04).

---

## [0.11.0] — feature/docs-reorg

### Changed
- **Documentation restructured** under ECSS-aligned hierarchy:
  - `docs/ecss/{requirements,design,verification,standards}/` — formal ECSS review documents
  - `docs/architecture/` — system architecture references
  - `docs/dev/` — developer guides
  - `docs/project/` — project management and phase plans
- Existing ECSS documents renamed to formal document IDs:
  `BOM.md` → `BOM-OBC-001.md`, `SAD_v1.0.md` → `SAD-OBC-001.md`,
  `SyRS_v1.0.md` → `SyRS-OBC-001.md`, `SOFTWARE_REQUIREMENTS.md` → `SRS-OBC-001.md`,
  `TRACEABILITY_MATRIX.md` → `RTM-OBC-001.md`, `TEST_PLANS.md` → `STP-OBC-001.md`
- Removed duplicate `docs/CODING_STANDARDS.md` (canonical: `docs/ecss/standards/CODING_STANDARDS.md`)
- Added `docs/README.md` — master documentation index organised by ECSS milestone

---

## [0.10.0] — feature/adcs-des-001

### Added
- **ADCS-DES-001 v0.1** (`docs/ecss/design/ADCS-DES-001.md`): ADCS Design Document — formal
  specification of the complete attitude determination and control system. Covers:
  EKF 6-state estimator (roll/pitch/yaw + gyro bias), RK2 midpoint integration,
  LQR full-state controller with mode-scheduled gains (FM_NOMINAL ωn=10 rad/s,
  FM_DETUMBLE ωn=30 rad/s), PID fallback, B×L cross-product momentum dump
  (k_dump=0.01), magnetorquer actuation, FreeRTOS task rates (sensor 10 Hz /
  control 20 Hz), controller dispatch table, fault IDs, and verification test
  mapping (T-EKF-01..06, T-EKFM-01..07, T-LQR-01..07, T-MTM-01..05).

---

## [0.9.0] — feature/hardware-bom

### Added
- **Hardware BOM v1.0** (`docs/ecss/design/BOM-OBC-001.md`): complete Bill of Materials for the CubeSat OBC reaching
  **PDR PASS** status. Covers all subsystems: OBC (RP2350/Pico 2W), ADCS sensors (MPU-6050 +
  LIS3MDL), GPS (NEO-7M), TT&C (E22-400M30S / HC-12), EPS (LiPo 18650 + MT3608 + TP4056),
  actuators (TB6612FNG RW + DRV8833 MTQ), ground station, and orbital parameters (SSO 500–700 km).
- **External hardware watchdog TPS3431** (BOM §10 #12): WDI → GPIO20 (dedicated pin), timeout = 3 s,
  kick source `HealthMonitorTask → watchdog_hal_feed()`. Closes the recovery chain:
  `HealthMonitorTask → internal MCU watchdog → TPS3431 → full system reset`.
- **LIS3MDL** identified as flight-grade magnetometer (BOM §3.1): replaces discontinued HMC5883L;
  continuous mode, ODR = 80 Hz, I2C addr `0x1C`. QMC5883L clone risk in GY-271 modules documented
  with mitigation steps and driver guidance.
- **GPS NEO-7M** pin assignment finalised (BOM §4): UART0 @ 9600 baud, NMEA 0183 (`$GPGGA`/`$GPRMC`).
  Debug output migrated from UART0 to USB CDC (`pico_enable_stdio_usb = 1`).
- **SAW filter 433 MHz** added to RF chain (BOM §6e): TDK B39431 or equivalent placed between
  E22-400M30S PA output and antenna; attenuates harmonics and out-of-band EMI interference.
- **Magnetorquers-first ADCS strategy** documented (BOM §8): B-dot detumbling and safe-mode attitude
  hold are achievable with MTQ-only (no reaction wheels). Phase 1 ADCS = MTQ-only; Phase 2 = RW + MTQ.
  Firmware implication: implement and validate `b_dot_control.c` before `lqr_control.c`.
- **Ground station design finalised** (BOM §7.2): E22-400M30S + CP2102/CH340 USB-UART adapter as
  recommended final GS (symmetric 433 MHz pair, transparent UART mode, no custom firmware required).
  LORA32U4 II demoted to bench-only secondary option.
- **Link budget verified** for SSO horizon case (BOM §6.1): 2300 km slant range at 5° elevation;
  E22-400M30S margin = +8.5 dB ✅; HC-12 margin = −9.5 dB ❌ (lab only confirmed).
- **Lab purchase list** (BOM §14): 22 items, ~$125–155 USD, priority-ordered by firmware readiness.

### Documentation
- BOM §2 OBC: DWT cycle counter (`DWT->CYCCNT`) noted for per-task WCET measurement before CDR;
  COTS non-space-grade risk (TID/SEE) and conformal coating recommendation documented.
- BOM §10 #9: I2C pull-up 4.7 kΩ on SDA/SCL confirmed required for MPU-6050 and LIS3MDL at 400 kHz;
  status updated from `❓ To evaluate` → `🔄 Planned`.
- BOM §0: SSO orbital parameters documented (500–700 km, 96–98°, eclipse ≤ 35.5 min).
- CDR pending items identified: flight EPS, reaction wheel final spec, OBC PCB design, LIS3MDL driver
  migration (`src/drivers/mag/`), radiation tolerance qualification.

---

## [0.8.0] — feature/pre-hw-integration-docs

### Added
- **SyRS v1.0** (`docs/ecss/requirements/SyRS-OBC-001.md`): System Requirements Specification
  covering functional, performance, interface, and FDIR requirements — validated against
  the implemented codebase (v0.7.0 baseline). Fields marked `[IMPL]` are implemented and
  hardware-verified; `[TBD-HW]` require physical sensor connection.
- **SAD v1.0** (`docs/ecss/design/SAD-OBC-001.md`): System Architecture Document describing the
  as-built software stack, FreeRTOS task map (with measured HWM values), data flow,
  FDIR authority chain, EKF/control architecture, CSP stack, HAL pattern, memory map,
  and boot sequence. Reflects Pico 2W hardware measurements.
- **Full per-task HWM instrumentation**: all 7 task stack high-water marks printed every
  5 s in the ALIVE loop (`obc_main.c`). Max usage measured on hardware: Telemetry
  180/2048 words (8.8%); all tasks ≥91% headroom.
- **Heap watermark output**: `xPortGetMinimumEverFreeHeapSize()` added to Heartbeat and
  ALIVE diagnostic prints. Host stub added to `include/host/FreeRTOS.h`.

### Fixed
- **FDIR dual-authority removed** (ARCH-02 closed): `eps_monitor.c` was invoking
  `fmm_request_transition(FM_SAFE)` directly *in addition to* `fault_report()`, creating
  two concurrent code paths to `FM_SAFE`. The direct call and `#include "flight_mode.h"`
  are removed. Single canonical chain enforced: EPS → `fault_report(FAULT_LEVEL_CRITICAL)`
  → FaultManager → `fmm_force_safe()`. Both `ENERGY_CRITICAL` and `ENERGY_EMERGENCY`
  cases merged into one fall-through (resolves `bugprone-branch-clone` clang-tidy finding).
- **SensorRead priority P3 → P4**: raised above AttitudeCtrl (P3) to guarantee the EKF
  always has fresh sensor data before the control tick runs on every 100 ms cycle.
- **`vTaskSuspend` removed from FSW**: the IMU-not-found path in `attitude_control_task`
  replaced with a 5 s polling loop. No `vTaskSuspend` calls remain in flight software.
  Task self-resumes when IMU becomes available (supports future hot-plug).
- **`TaskHandle_t` captures**: all 7 task handles captured at `xTaskCreate` time (were
  `NULL`). Handles gated under `#ifdef PICO_BUILD`; host build receives `NULL` via
  `HPTR()` macro to avoid unused-variable errors.

### Documentation
- SyRS SYS-F-205 `[PLANNED]` → `[IMPL]`: single FDIR authority chain implemented.
- SAD §4 task table: HWM column filled with measured hardware values; added "Used" column;
  clarification note that HWM = remaining free words (high = good).
- SAD §5.3 FDIR: replaced dual-path ASCII diagram with single canonical chain diagram;
  added design rationale (audit log, no concurrent paths, single inhibit point).
- SAD §5.3: added note that `vTaskSuspend` is avoided in FSW; `AttitudeCtrl` FM_SAFE
  path uses early return, not task suspension.
- SAD §9 memory map: added `xPortGetMinimumEverFreeHeapSize` line (~58 KB measured).
- SAD §11: ARCH-02 and ARCH-03 closed.

### Testing
- **29/29 tests pass**. `test_eps_monitor.c` test 5 updated: expects `FAULT_LEVEL_CRITICAL`
  (was `FAULT_LEVEL_ERROR`) matching the new single-path FDIR behaviour.
- CI pipeline (6 stages): host-test ✅, pico-build ✅, emu-build ✅, emulate ✅,
  static ✅, coverage ✅.
- Hardware-validated on Pico 2W: 7 tasks, LED blink, telemetry @ 2 Hz, heap stable at
  60,408 bytes, sensor timing avg=99,999 µs jitter <40 µs (100-sample measurement).

---

## [0.7.0] - 2026-03-20

### Added (Phase 6 — Closed-Loop Stability & Architectural Consolidation)
- **Quaternion library** (`src/control/quaternion.c`, `include/quaternion.h`): unit-quaternion
  multiply, rotate, normalize, spherical linear interpolation (slerp), and to-Euler conversion;
  replaces ad-hoc Euler math throughout the attitude pipeline (PR-21).
  Five unit tests: T-QAT-01..05.
- **Configurable magnetic declination** (`OBC_MAG_DECLINATION_RAD` in `include/config.h`):
  compile-time declination offset applied inside `ekf_update_mag()`; default 0.0 rad; keeps
  yaw estimate referenced to true north (PR-22). One unit test: T-EKFM-07.
- **LQR gain scheduling** (`src/control/lqr_schedule.c`, `include/lqr_schedule.h`):
  table-driven gain selection by combined energy state and angular momentum magnitude;
  three discrete gain sets (HIGH_ENERGY, NOMINAL, LOW_ENERGY); `lqr_schedule_select()`
  called by `vAttitudeControlTask_Step()` before each control tick (PR-23).
  Three unit tests: T-LQRS-01..03.
- **Closed-loop simulation harness** (`src/control/closed_loop_sim.c`,
  `include/closed_loop_sim.h`): full EKF → LQR → RK2-dynamics simulation loop with
  configurable initial attitude error, noise injection, and stability acceptance criterion
  (settling within 30 s, residual ω < 0.05 rad/s) (PR-24). Six unit tests: T-CLS-01..06.
- **Integration test — fault-to-safe sequence** (`tests/integration/test_fault_safe.c`):
  end-to-end verification that `fault_report(FAULT_ANY, FAULT_LEVEL_CRITICAL)` causes
  FMM to enter FM_SAFE within 100 ms simulation ticks (T-FMS-01a..d, 4 sub-tests) (PR-25).
- **Integration test — watchdog safe-mode trigger** (`tests/integration/test_safe_trigger.c`):
  verifies that `health_monitor_task.c` `watchdog_hal_triggered()` path calls
  `fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL)` → FM_SAFE
  (T-SAFE-01a..c, 3 sub-tests) (PR-25).
- **Flash-backend stub** (`src/core/flash_backend_stub.c`, `include/flash_backend.h`):
  host-build stub that opens `/tmp/obc_log.bin` and writes packed records via
  `flash_backend_flush()`; hardware path is a `__attribute__((weak))` override (PR-26).
- **Event logger flush hook** (`src/core/event_logger.c`): ring-buffer (64 records × 40 B)
  now calls `flash_backend_flush()` automatically when capacity is reached; constant
  `LOG_RING_CAPACITY 64u` exported in `include/logger.h` (PR-26).
  Four sub-tests: T-LOG-01a..d (one CTest target `event_logger_test`).
- **MISRA C deviations log** (`docs/ecss/standards/MISRA_DEVIATIONS.md`): all advisory
  deviations documented with rationale; zero required or mandatory violations (PR-27).

### Changed
- `src/tasks/health_monitor_task.c`: added `watchdog_hal_triggered()` poll →
  `fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL)` on every task tick (PR-25).
- `include/eps.h`: added `eps_hal_read()` declaration for host-HAL abstraction.
- Multiple files: MISRA C fixes — unused `<stdio.h>` removed, void casts on ignored
  return values, braced single-statement bodies, NULL pointer comparisons (PR-27).

### Testing
- Test suite: **29** CTest executables.  All **29/29** passing.
- New test targets (Phase 6): `quaternion_test`, `lqr_schedule_test`,
  `closed_loop_test`, `ekf_mag_test` (T-EKFM-07 added), `test_fault_safe`,
  `test_safe_trigger`, `event_logger_test`.
- gcovr line coverage: **91.8%** on `src/control/` + `src/core/` + `src/services/`
  combined (target was ≥90%).
- cppcheck, clang-format-14, clang-tidy-14 all clean.

---

## [0.6.0] - 2026-03-12

### Added (Phase 5 — Flight Readiness)
- **Watchdog HAL** (`src/core/watchdog.c`, `include/watchdog.h`): hardware watchdog
  kick/init/enable via `__attribute__((weak))` HAL stubs; host build issues periodic
  log messages; `xTaskGetTickCount()`-gated kick called from `vHealthMonitorTask_Step()`
  (PR-16). Five unit tests: T-WDT-01..05.
- **Momentum Dump** (`src/control/momentum_dump.c`, `include/momentum_dump.h`): angular
  momentum magnitude check, FM_DETUMBLE guard, duty-cycle B-dot dump algorithm.  Dump
  result published to DLA (`data_layer_write_momentum_dump()`); called from
  `vAttitudeControlTask_Step()` (PR-17). Five unit tests: T-MDT-01..05.
- **HMC5883L Magnetometer driver** (`src/drivers/hmc5883l.c`, `include/drivers/hmc5883l.h`):
  I²C 3-axis mag driver with `__attribute__((weak))` HAL stub returning `{25, 0, 42}` µT;
  `vSensorReadTask_Step()` reads mag, converts to µT, writes to DLA via
  `data_layer_write_mag()`.  `system_state_t` extended with `mag_field_uT[3]`,
  `mag_valid`, `mag_timestamp_ms` (PR-18). Four unit tests: T-MAG-01..04.
- **EKF yaw update via magnetometer** (`src/control/ekf.c` / `include/ekf.h`): scalar
  yaw measurement update `ekf_update_mag()` using tilt-compensated H=[0,0,1,0,0,0]
  observation; degenerate-field guard (`|Bh| < 1 µT`); innovation wrapped to `[-π, +π]`;
  `ekf_t` extended with `float r_mag`; `vSensorReadTask_Step()` calls update after each
  mag read (PR-19). Six unit tests: T-EKFM-01..06.

### Testing
- Test suite: **23** CTest executables.  All **23/23** passing.
- New test targets: `watchdog_test`, `momentum_dump_test`, `hmc5883l_test`,
  `ekf_mag_test`.
- cppcheck, clang-format-14 clean on all PR-16..19 files.

---

## [0.5.0] - 2026-03-08

### Added (Phase 4 — Advanced Control)
- **RK2 Dynamics integrator** (`src/dynamics/attitude_dynamics.c`): Euler → midpoint
  (RK2) integration for attitude propagation; reduces attitude error to <0.1 mrad at
  10 Hz (PR-11).  Five unit tests: T-DYN-01..05.
- **EKF attitude estimator** (`src/control/ekf.c`, `include/ekf.h`): 6-state EKF
  `x = [roll, pitch, yaw, bx, by, bz]` with analytic 2×2 S⁻¹ inversion, gyro-bias
  estimation, and accelerometer measurement update (PR-12).
  Six unit tests: T-EKF-01..06.
- **LQR controller** (`src/control/lqr.c`, `include/lqr.h`): 3×6 full-state gain
  matrix `u = -K·x` with analytic default gains for ωn = 10 rad/s, ζ = 1, derived
  for Isat = diag(0.01, 0.01, 0.005) kg·m² (PR-13).
  Seven unit tests: T-LQR-01..07.
- **Sensor fusion integration** (`src/tasks/sensor_read_task.c`): static `ekf_t g_ekf`
  runs EKF predict + update every 10 Hz tick; EKF attitude, gyro bias, and diagonal
  covariance published to DLA via `data_layer_write_ekf()` (PR-14).
  Three new tests: T-SRF-08..10 (10 total for sensor read task).
- **LQR/PID mode dispatch** (`src/tasks/attitude_control_task.c`): FM_NOMINAL +
  `imu_ekf_valid` → LQR; FM_DIAGNOSTIC or EKF converging → PID fallback (PR-15).
  Three new tests: T-ACT-09..11 (11 total for attitude control task).
- **EKF fields in system_state_t**: `gyro_bias[3]`, `att_uncertainty[3]`,
  `imu_ekf_valid` (PR-14).
- **`data_layer_write_ekf()`** DLA write function for EKF outputs (PR-14).

### Testing
- Test suite: 19 CTest executables.  All 19/19 passing.
- clang-format-14, clang-tidy-14, cppcheck all clean.

---

## [0.4.0] - 2026-03-02

### Added
- **Foundation Types** (`include/foundation_types.h`): `FlightMode_t`, `FaultCode_t`, `EpsState_t`, `SubsystemId_t`, `OBCResult_t` — shared enums and status codes used across all subsystems (PR-1).
- **Data Layer Abstraction (DLA)** (`src/core/data_layer.c`): thread-safe `SystemStateStore` with getter/setter API and FreeRTOS mutex protection; replaces direct `system_state_t` struct accesses (PR-2).
- **Flight Mode Manager (FMM)** (`src/core/flight_mode_manager.c`): state machine with 6 operational modes (`SAFE`, `NOMINAL`, `DETUMBLE`, `SCIENCE`, `COMMS`, `LOW_POWER`), priority-based transitions, and inhibit flags (PR-3).
- **Fault Manager (FM)** (`src/core/fault_manager.c`): fault table with severity levels (`INFO`, `WARNING`, `CRITICAL`), auto-escalation to safe mode on critical faults, fault persistence counter, and `fault_manager_tick()` (PR-4).
- **EPS Monitor** (`src/core/eps_monitor.c`): voltage/current threshold checks, energy state calculation (`FULL`, `NOMINAL`, `LOW`, `CRITICAL`), and `eps_monitor_tick()` for periodic health updates (PR-5).
- **Persistent Event Logger** (`src/core/event_logger.c`): ring-buffer event store with severity tagging, query-by-severity API, and `event_logger_flush()` hook for future flash backend (PR-6).
- **Sensor Read Task — DLA migration**: rewired `sensor_read_task.c` to write via DLA setters, removing direct struct coupling (PR-7).
- **Attitude Control Task — DLA migration**: rewired `attitude_control_task.c` to consume flight mode and fault state from DLA, gating actuators on FM guard (PR-8).
- **Telemetry Task — DLA migration**: rewired `telemetry_task.c` to read all fields via DLA getters and embed energy state flags in telemetry packets (PR-9).
- **Health Monitor Task wiring**: integrated `fault_manager_tick()` and `eps_monitor_tick()` calls into `health_monitor_task.c` periodic loop (PR-10).

### Changed
- Architecture aligned with SPEC-2 v2.0 (Data Flow), SPEC-3 (Fault Management), and SPEC-5 (Command & Telemetry Interface).
- All tasks now communicate exclusively through the DLA — no direct `SystemState` struct access in task code.
- Flight-mode-aware actuator inhibit logic added to attitude and sensor tasks.

### Testing
- Test suite extended to 17 CTest executables covering DLA, FMM, FM, EPS Monitor, Event Logger, and task integration.
- 100% tests passing (17/17).

---

## [0.3.0] - 2026-02-22

### Added
- Integrated `libcsp` via a git submodule for cross-platform POSIX and FreeRTOS SMP communication.
- Implemented `pico_usart.c` UART driver mapping libcsp's KISS protocol to the RP2350 hardware UART.
- Rewrote `telemetry_task.c` to accurately pack and send `csp_telemetry_packet_t` payloads over UART.
- Added `command_task.c` to listen for remote commands (`CMD_ECHO`, `CMD_REBOOT`, `CMD_SET_MODE`) and act on system states.
- Reached extensive unit testing suite totaling 8 CTest executables checking parsing logic, packet packing, and initialization routing.
- Increased overall GCC line test coverage from 38% to 64%.

### Changed
- Shifted default debugging output from target UART to UART0 while allocating UART1 explicitly for the libcsp protocol.

## [0.2.0] - 2026-02-20

### Added
- MPU6050 IMU driver and onboard Temperature sensor driver (Pico ADC4)
- Thread-safe system state management using FreeRTOS mutexes
- High-resolution task jitter telemetry (µs precision)
- Integrated sensor-to-control loop in `obc_main.c`

### Changed
- Increased `configMINIMAL_STACK_SIZE` to 4KB and `configTOTAL_HEAP_SIZE` to 128KB for SMP/USB stability
- Forced `-mfloat-abi=soft` to ensure FreeRTOS SMP compatibility on RP2350
- Enabled UART0 and USB CDC stdio with FreeRTOS-aware initialization
- Fixed `pico_flash` header shadowing issues

### Fixed
- Resolved `FATAL: Stack overflow in task 'IDLE0'` caused by FPU context switching mismatch
- Resolved USB CDC enumeration issues on boot

### Known Issues
- Power consumption baseline measurement pending final hardware sign-off

## [0.1.0] - 2026-02-12

### Added
- Initial skeleton implementation
  - Modular architecture following ECSS-Q-ST-80C standards
  - Core subsystems: drivers, actuators, control, dynamics, tasks
  - FreeRTOS task framework with stub implementation for host builds
  - Configuration system (config/FreeRTOSConfig.h)
- Development environment
  - CMake build system with per-module organization
  - Unit testing framework (3 basic tests)
  - CI workflow via GitHub Actions
  - Build automation scripts
- Documentation
  - README with project overview
  - Coding standards (MISRA-like guidelines)
  - Build guide
  - Next steps and verification plan
- Project governance
  - MIT License
  - Contributing guidelines
  - `.gitignore` for embedded projects

### Project Status
**✅ Complete:** Architecture, FreeRTOS integration, testing framework, documentation.

**🔄 In Progress:** N/A

**⏳ TODO (Priority Order):**
1. Pico SDK integration (Pico 2W hardware support)
2. I2C driver for MPU6050 (real sensor readout)
3. WiFi and lwIP integration (telemetry transmission)
4. Kalman filter for attitude estimation
5. Advanced control algorithms (LQR, MPC)
6. Flight-ready hardening (watchdog, safe states, logging)

---

## Development Notes

### Versions 0.1.x Series
- **0.1.0**: Skeleton with FreeRTOS and basic control logic
- **0.2.0**: Pico SDK + hardware driver support
- **0.3.0**: libcsp and Telemetry/Command integration
- **0.4.0**: (Planned) Advanced Control filters and algorithms

### Estimated Timeline
- Phase 2 (Pico SDK): Q1 2026
- Phase 3 (Communication): Feb 2026 (Completed)
- Phase 4 (Advanced Control): Q2-Q3 2026
- Phase 5 (Flight Ready): Q3 2026

---

## Unreleased Commits

View unreleased changes with:
```bash
git log $(git describe --tags --abbrev=0)..HEAD --oneline
```

---

**Last Updated:** 2026-02-22
