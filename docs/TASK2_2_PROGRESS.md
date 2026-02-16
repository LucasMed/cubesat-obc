# Task 2.2 Progress: FreeRTOS Real Kernel Integration

**Date**: 2026-02-16  
**Status**: ✅ **PARTIAL - Foundation Ready, FreeRTOS Kernel Integration Pending**  
**Completion**: ~60% (structure ready, kernel linking needed)

---

## Completed ✅

### 1. OBC Main Entry Point (obc_main_pico.c)
- ✅ Created structured firmware for Pico 2W
- ✅ CYW43 initialization (WiFi chip for LED)
- ✅ Task creation framework (sensor, control, telemetry, health monitor)
- ✅ LED diagnostic blink task
- ✅ FreeRTOS hooks for error handling (malloc failure, stack overflow)
- ✅ Compiles to 68 KB UF2 (Pico 2W format)

### 2. FreeRTOSConfig.h Updated for RP2040
- ✅ Heap size: 32 KB (tuned for RP2040's 264 KB total SRAM)
- ✅ Tick rate: 1000 Hz (1 ms resolution, standard for RTOS)
- ✅ Max priorities: 5 levels (0=idle, 4=highest)
- ✅ Timer task support enabled
- ✅ Stack overflow detection (method 2)
- ✅ Malloc failure hooks

### 3. Build System Integration
- ✅ Dual-mode CMake (host vs Pico SDK)
- ✅ Conditional compilation of examples
- ✅ Both blink_test.uf2 and obc_firmware_pico.uf2 generate successfully

---

## Pending for Full Integration ⏳

### 1. **Integrate Real FreeRTOS Kernel**
The current obc_firmware_pico.c uses `FreeRTOS.h` headers but is **not linked against the real kernel**.

**Why pending?**
- Pico SDK doesn't include FreeRTOS kernel by default
- Options to resolve:
  - **A**: Manually download FreeRTOS kernel and add to SDK/lib
  - **B**: Use a pre-built FreeRTOS package for RP2040
  - **C**: Create a minimal FreeRTOS scheduler stub for now (suitable for testing task structure)

**Next step**: Choose approach A or B, then update CMakeLists.txt with FreeRTOS target_link_libraries

### 2. **Task Scheduler Verification**
Once FreeRTOS kernel is linked:
- [ ] Verify all 5 tasks spawn correctly
- [ ] Measure task jitter (expect <10 ms per design spec)
- [ ] Monitor heap usage
- [ ] Test LED blink diagnostic pattern

### 3. **Real I2C Drivers** (Task 2.3)
Not started yet - depends on FreeRTOS integration:
- [ ] I2C master driver
- [ ] MPU6050 sensor readout
- [ ] Temperature sensor

---

## Files Created/Modified

| File | Changes |
|------|---------|
| `src/obc_main_pico.c` | ✅ NEW - Pico firmware with task structure |
| `config/FreeRTOSConfig.h` | ✅ UPDATED - RP2040 tuned parameters |
| `examples/CMakeLists.txt` | ✅ UPDATED - Added obc_firmware_pico target |

---

## Build Artifacts

```
build/examples/blink_test.uf2          (536 KB) ✅ Tested & working
build/examples/obc_firmware_pico.uf2   (68 KB)  ⏳ Ready for testing (kernel pending)
```

---

## Testing Strategy (When FreeRTOS Linked)

### Hardware: Pico 2W

1. **Flash obc_firmware_pico.uf2**
   ```bash
   # Hold BOOTSEL, connect USB, drag to RPI-RP2 drive
   cp build/examples/obc_firmware_pico.uf2 /mnt/pico/
   ```

2. **Monitor Serial Output** (USB CDC)
   ```bash
   screen /dev/ttyACM0 115200
   # or minicom, picocom, etc.
   ```

3. **Expected Output**:
   ```
   =====================================
     CubeSat OBC - Pico 2W Firmware
     FreeRTOS Real Kernel (Phase 2.2)
   =====================================

   Initializing CYW43 (WiFi chip)...
   ✓ CYW43 initialized

   Creating FreeRTOS tasks...
   ✓ LED Blink task created
   ✓ Sensor Read task created
   ✓ Attitude Control task created
   ✓ Telemetry task created
   ✓ Health Monitor task created

   Starting FreeRTOS scheduler...
   LED will blink to indicate system is running.
   Task output will appear below:
   -------------------------------------

   LED ON
   Sensor Read: IMU polling...
   LED OFF
   AttitudeCtrl: Computing control law...
   ...
   ```

4. **LED Pattern**: Blink diagnostic (200ms on, 800ms off)

---

## Decision Points

### **Which approach for FreeRTOS?**

#### Option A: Download & Link Real FreeRTOS Kernel ⭐ **Recommended**
- **Pros**: Full real-time guarantees, production-ready
- **Cons**: Adds ~200-300 KB to firmware
- **Action**: 
  1. Download FreeRTOS kernel (aws/amazon-freertos or official repo)
  2. Add to Pico SDK via CMakeLists.txt
  3. Link to obc_firmware_pico target

#### Option B: Use Pre-compiled FreeRTOS Library
- **Pros**: Minimal setup
- **Cons**: Less control, may not be optimized for RP2040
- **Action**: Install via package manager or fetch from Raspberry Pi ecosystem

#### Option C: Stub Approach (Temporary)
- **Pros**: Fast validation of task structure
- **Cons**: No real scheduling, not production-ready
- **Action**: Create simple task queue for Phase 2 testing only

**Recommended**: **Option A** (Real kernel) gives us production-ready code by Phase 2 end.

---

## Next Steps (Priority Order)

1. **[ Task 2.2a ]** Decide & implement FreeRTOS kernel linking (Option A/B/C)
2. **[ Task 2.2b ]** Test on Pico 2W: Verify tasks spawn, jitter <10 ms
3. **[ Task 2.2c ]** Document results in TESTING_REPORT_PHASE2.md
4. **[ Task 2.3 ]** I2C driver implementation (sensor readout)
5. **[ Task 2.4 ]** Control loop integration with real data
6. **[ Task 2.5 ]** Final hardware validation

---

## Estimated Effort

| Sub-task | Effort | Status |
|----------|--------|--------|
| Task 2.2a (FreeRTOS kernel) | 2-3 hours | ⏳ TODO |
| Task 2.2b (Verify on hardware) | 1-2 hours | ⏳ TODO |
| Task 2.2c (Documentation) | 30 min | ⏳ TODO |
| **Task 2.2 Total** | **4-6 hours** | **60% done** |

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|-----------|
| FreeRTOS kernel fails to link | High | Use Option C stub temporarily; full kernel later |
| Task jitter exceeds 10 ms | Medium | Profile hot paths; consider OS tick rate tuning |
| Memory exhaustion | Medium | Monitor heap; adjust task stack sizes if needed |
| LED blink not visible | Low | Test blink_test.uf2 first; same LED control |

---

## Acceptance Criteria for Task 2.2 Complete

- [ ] OBC firmware compiles with real FreeRTOS kernel
- [ ] All 5 tasks successfully spawn
- [ ] Task jitter measured and <10 ms
- [ ] LED blink diagnostic operational
- [ ] Serial output shows expected task messages
- [ ] No stack overflow or malloc failures detected
- [ ] Documentation updated (TESTING_REPORT_PHASE2.md)

---

**Last Updated**: 2026-02-16  
**Prepared By**: OBC Team  
**Status**: Ready for FreeRTOS kernel integration decision
