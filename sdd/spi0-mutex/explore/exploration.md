## Exploration: SPI0 Mutex Addition

### Current State
SPI0 (`spi0` peripheral) is a shared bus with **zero** protection against concurrent access. Four devices share the bus via individual chip-select GPIOs, and two FreeRTOS tasks can preempt each other mid-transaction. This causes corrupted flash writes, camera FIFO read failures, and intermittent payload hangs.

The FreeRTOSConfig already has `configUSE_MUTEXES 1` and `configUSE_SEMAPHORES 1` — the infrastructure is ready, just not wired in.

### Affected Areas
- `src/drivers/payload/spi_payload.c` / `include/spi_payload.h` — SPI bus init, cs_select/cs_deselect. **Central mutex insertion point.** Also need `lock()`/`unlock()` API.
- `src/drivers/payload/w25q64.c` — flash driver. 10 public functions each doing cs_select → SPI → cs_deselect. Recursive page-boundary splitting in `w25q64_write_page`.
- `src/drivers/payload/camera_driver.c` — camera SPI. `cam_spi_write()`, `cam_spi_read()`, `camera_read_fifo_burst()` (holds CS for entire FIFO read).
- `src/drivers/payload/rm3100.c` — magnetometer SPI. `rm3100_spi_write()`, `rm3100_spi_read()`.
- `src/drivers/payload/sd_spi.c` — SD card. Uses `SPI_CS_FLASH_PIN` (GPIO7 — same pin as W25Q64 flash). Hardware sharing issue to note.
- `config/FreeRTOSConfig.h` — already has `configUSE_MUTEXES 1`.

### SPI Users

| File | Function(s) | Task | Priority | CS Pin | Pattern |
|------|-------------|------|----------|--------|---------|
| w25q64.c | read_id, read_status, wait_ready, read, write_page, erase_sector, erase_block32/64, erase_chip, is_present, write_imu_calib, read_imu_calib | **telemetry_task** (via telemetry_storage_store/read) + **POST** (startup context) | 2 | GPIO7 | cs_select → spi_write_read_blocking → cs_deselect |
| camera_driver.c | cam_spi_write, cam_spi_read, camera_read_fifo_burst | **payload_task** (camera_capture, camera_read_fifo_burst) | 2 | GPIO14 | cs_select → spi_write_blocking / spi_read_blocking → cs_deselect |
| rm3100.c | rm3100_spi_write, rm3100_spi_read | **payload_task** (rm3100_read_vector) | 2 | GPIO6 | cs_select → spi_write_blocking / spi_read_blocking → cs_deselect |
| sd_spi.c | sd_spi_init, sd_spi_read_sector | POST (startup) | — | GPIO7* | cs_select → spi_write_read_blocking / sd_spi_transfer_byte → cs_deselect |

\* GPIO7 is shared between W25Q64 flash and SD card — hardware limitation.

### Approaches

1. **Mutex in cs_select/cs_deselect** (recommended — minimal change, maximum coverage)
   - Add `SemaphoreHandle_t` mutex in `spi_payload.c`. Create in `spi_payload_init()` (first call only).
   - `spi_payload_cs_select()`: `xSemaphoreTake(mutex, portMAX_DELAY)` before asserting CS.
   - `spi_payload_cs_deselect()`: `xSemaphoreGive(mutex)` after deasserting CS.
   - Also expose `spi_payload_lock()`/`spi_payload_unlock()` for callers that need multi-transaction atomicity.
   - **Pros:** Zero changes to any driver. Every SPI transaction automatically protected. Works for all existing and future SPI0 users.
   - **Cons:** Tiny gap between `write_enable()` cs_deselect and page-program cs_select in w25q64 (≈30ns at 150MHz). Both tasks at same priority (2) — context switch only on 1ms tick. Negligible risk.
   - **Effort:** Low (~30 lines in 2 files: spi_payload.c + spi_payload.h)

2. **Mutex at driver public-API level**
   - Add `spi_payload_lock()`/`unlock()` and call at entry/exit of every public function in w25q64.c, camera_driver.c, rm3100.c, sd_spi.c.
   - **Pros:** No gap between multi-step operations. Each public API call is atomic.
   - **Cons:** Touches 4 drivers + header. Error-prone (forgetting lock/unlock on a new function). Driver changes add review surface. Recursive flash page-split needs handling.
   - **Effort:** Medium (~80 lines across 6 files)

3. **Dual approach: cs_select/deselect mutex + `spi_payload_lock_bus()` for multi-step**
   - Approach 1 as base, plus a `lock_bus()`/`unlock_bus()` for the flash driver's write_enable → page_program → wait_ready sequence.
   - **Pros:** No gaps whatsoever.
   - **Cons:** Slightly more complex than approach 1. Extra API surface.
   - **Effort:** Low-Medium

### Recommendation

**Approach 1** — mutex in `cs_select/cs_deselect`. Rationale:
- Zero driver changes — no risk of introducing bugs in existing code.
- Both SPI-using tasks run at **identical priority (2)**, so no priority inversion risk.
- The write_enable → page_program gap is ~30ns at 150MHz vs 1ms FreeRTOS tick — effectively zero collision probability.
- `w25q64_wait_ready()` polling already uses per-read cs_select/cs_deselect, so it correctly releases the bus between status polls (flash is busy internally during this time).
- The `camera_read_fifo_burst()` function holds CS asserted for the entire FIFO read — the mutex is held for the full duration, correctly blocking flash writes.
- Add `spi_payload_lock()`/`spi_payload_unlock()` as exposed API for any future multi-transaction needs.

### Risks
- **Deadlock if cs_select/cs_deselect unbalanced**: If a driver function asserts CS via `spi_payload_cs_select()` but returns early without calling `cs_deselect()`, the mutex is never released. All drivers currently maintain balanced select/deselect pairs (verified in code review).
- **Mutex creation failure**: `xSemaphoreCreateMutex()` in `spi_payload_init()` could return NULL if heap exhausted. Add `configASSERT()` or handle gracefully.
- **SD card / flash share CS=7**: This is a hardware issue (separate from mutex). Both devices on GPIO7 means they CANNOT coexist on the bus. The mutex doesn't solve this — it's a hardware constraint.
- **Host build coupling**: Mutex code must be `#if defined(PICO_BUILD)` guarded, since host stubs don't use FreeRTOS.
- **Startup race**: POST and sensor init run from `vStartupTask` before any SPI-using tasks are created — no race possible during init.

### ISR Safety Needed
**No.** No interrupt service routine accesses SPI0. All ISRs in the system are for UART, GPIO interrupts, and fault handling — none touch the SPI bus. Regular `xSemaphoreTake` (not `FromISR`) is sufficient.

### Test Coverage
Existing tests use **host stubs** that bypass real SPI:
- `tests/unit/test_w25q64.c` — 14 tests, all using host-stub w25q64 (no real SPI)
- `tests/unit/test_rm3100.c` — uses weak-symbol SPI stubs
- `tests/integration/host_spi_gpio_stubs.c` — stubs for spi_write_blocking, spi_read_blocking, gpio_get
- No integration test exercises real SPI0 contention between tasks

### Ready for Proposal
Yes. The orchestrator should inform the user that this is a low-risk, high-reward change with clear scope: add a FreeRTOS mutex to `spi_payload_cs_select()`/`cs_deselect()` in `spi_payload.c`, guard with `#if defined(PICO_BUILD)`, and optionally expose `lock()`/`unlock()` for future use. The analysis confirms no ISR interference, no priority inversion (both SPI tasks at priority 2), and zero driver changes needed.
