# Tasks: DS3231 RTC Integration

## Phase 1: Infrastructure

- [ ] 1.1 Create driver header: `include/ds3231.h` (I2C address, register defines, API prototypes)
- [ ] 1.2 Create rtc/ subdirectory: `src/drivers/rtc/`

## Phase 2: Driver Implementation

- [ ] 2.1 Write real driver: `src/drivers/rtc/ds3231.c` (PICO_BUILD only, BCD conversions, I2C read/write, plausibility checks)
- [ ] 2.2 Write stub: `src/drivers/rtc/ds3231_stub.c` (host only, mock time 2024-01-01 12:00:00)
- [ ] 2.3 Add rtc/ to CMakeLists.txt (PICO_BUILD: ds3231.c, non-PICO_BUILD: ds3231_stub.c)

## Phase 3: System State & Data Layer

- [ ] 3.1 Update `include/system_state.h`: add `rtc_timestamp`, `rtc_valid`, `rtc_available` to `system_state_t`
- [ ] 3.2 Update `include/data_layer.h`: add `data_layer_write_rtc()`, `data_layer_set_rtc_avail()`
- [ ] 3.3 Implement data layer functions in `src/core/data_layer.c`

## Phase 4: Task Integration

- [ ] 4.1 Update `include/telemetry_task.h`: add `rtc_timestamp` to `csp_telemetry_packet_t`, update flags comment (bit 4: rtc_valid)
- [ ] 4.2 Update `src/tasks/telemetry_task.c`: add rtc to UART output `[TLM]`, populate `tlm->rtc_timestamp`, update flags
- [ ] 4.3 Update `src/tasks/sensor_read_task.c`: poll RTC at 1 Hz (every 10 cycles of 10 Hz task), call `ds3231_read_time()`, convert to epoch, call `data_layer_write_rtc()`, add `#include "ds3231.h"`
- [ ] 4.4 Update `src/obc_main.c`: call `ds3231_init()` at startup, call `data_layer_set_rtc_avail()`, add RTC to startup printf

## Phase 5: Debug Command

- [ ] 5.1 Add `RTC_TEST` command to `src/tasks/command_task.c` (read and print RTC time)

## Phase 6: Testing

- [ ] 6.1 Create unit tests: `tests/unit/test_ds3231.c` (init, is_present, read_time, set_time, error handling)
- [ ] 6.2 Update `tests/unit/CMakeLists.txt` to include `test_ds3231.c`

## Phase 7: Documentation

- [x] 7.1 Update `CHANGELOG.md` (DS3231 RTC driver, telemetry, debug command)
- [x] 7.2 Update `docs/architecture/ARCHITECTURE.md` (RTC subsystem documentation)