# StartupTask ALIVE Loop Evaluation

**Document ID**: STA-OBC-001  
**Version**: 1.0  
**Date**: 2026-04-17  
**Classification**: Technical Analysis  
**Owner**: Software Lead  
**Refs**: CDR-SAF-06, CDR-SW-05, FMEA-OBC-002 §12

---

## 1. Purpose

Evaluate whether the `vStartupTask` ALIVE loop (`src/obc_main.c` lines 251–274)
is necessary for safety-critical operation, and determine whether it should be
retained, modified, or removed.

---

## 2. What the ALIVE Loop Does

After all subsystem initialization and task creation, `vStartupTask` lowers its
priority from `configMAX_PRIORITIES - 1` (4) to `tskIDLE_PRIORITY + 1` (1) and
enters an infinite loop:

```c
vTaskPrioritySet(NULL, tskIDLE_PRIORITY + 1);  // Priority 1
for (;;)
{
    // Every 5 seconds:
    printf("[ALIVE] heap=%lu min_ever=%lu tick=%lu\r\n", ...);
    // High Water Mark (HWM) for all tasks:
    printf("  HWM SensorRead  =%4lu  AttitudeCtrl=%4lu\r\n", ...);
    printf("  HWM Telemetry   =%4lu  Command     =%4lu\r\n", ...);
    printf("  HWM HealthMon   =%4lu  LEDBlink    =%4lu  Heartbeat=%4lu\r\n", ...);
    printf("  HWM Payload     =%4lu  GpsTask     =%4lu\r\n", ...);
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(5000));
}
```

---

## 3. Safety Analysis

### 3.1 What It Is NOT

The ALIVE loop is **not a watchdog task**. It does not:

- Feed the hardware watchdog (TPS3431 on GPIO20) — that is done by `HealthMonitorTask`
  via `watchdog_hal_feed()` every health tick
- Monitor task liveness — no task failure detection
- Trigger safe-mode transitions
- Perform any FDIR (Fault Detection, Isolation, and Recovery) action

### 3.2 What It DOES Provide

The ALIVE loop provides **diagnostic visibility**:

| Output | Purpose |
|--------|---------|
| `[ALIVE] heap=` | Heap pressure monitoring — detect memory leaks |
| `min_ever=` | Worst-case heap usage — useful for tuning `configTOTAL_HEAP_SIZE` |
| `tick=` | System uptime counter — cross-reference with telemetry timestamps |
| `HWM SensorRead =` | Stack high-water mark per task — detect approaching stack overflow |

### 3.3 Stack Overflow Detection (Separate Mechanism)

The startup task also participates in stack overflow detection via a **separate
mechanism** in `main()` (lines 287–303):

```c
if (watchdog_hw->scratch[0] == 0xDEAD0001u)
{
    // Rebooted due to stack overflow — scratch holds task name
    printf("*** REBOOTED: Stack overflow in task '%s' ***\r\n", name);
    watchdog_hw->scratch[0] = 0;  // clear
}
```

This uses the hardware watchdog scratch register to persist the reboot cause
across resets. It is **independent** of the ALIVE loop.

---

## 4. Risk Assessment

### 4.1 Removing the ALIVE Loop

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| Loss of heap monitoring | Low | Certain | HealthMonitor already monitors critical memory thresholds; `configTOTAL_HEAP_SIZE` validated at compile time |
| Loss of HWM visibility | Low | Certain | `uxTaskGetStackHighWaterMark()` can be called from other tasks if needed; `T-HIL-STK-01..05` test stack overflow at runtime |
| Task appears "alive" without the loop | Low | — | HealthMonitor handles watchdog; tasks self-report via HWM |
| SMP task deletion issues | Low | Low | If ALIVE loop is removed, `vTaskDelete(NULL)` must be used; SMP port may have issues (documented in code comment) |

### 4.2 Keeping the ALIVE Loop

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| Wastes 8 KB RAM + CPU | Low | Certain | Runs at priority 1 (idle+1) — preempted by all real tasks; CPU usage is negligible (~0.1% at 5 s period) |
| UART TX congestion | Low | Low | `fflush(stdout)` after each printf; in flight, UART TX may be disabled |
| Security (task enumeration) | Low | Low | Task names exposed via UART; acceptable for a spacecraft (no external attack surface) |

---

## 5. Evaluation Criteria

| Criterion | Result | Notes |
|-----------|--------|-------|
| Safety-critical function | ❌ No | FDIR is handled by HealthMonitor + FaultManager |
| Required by hardware watchdog | ❌ No | TPS3431 fed by HealthMonitorTask |
| Detects stack overflow | ❌ No | Separate scratch-register mechanism in `main()` |
| Provides useful operational telemetry | ✅ Yes | HWM + heap monitoring every 5 s |
| Resource cost acceptable | ✅ Yes | 8 KB stack, ~0.1% CPU at 5 s period |
| SMP task deletion safe | ❌ Uncertain | `vTaskDelete()` has known issues on RP2350 SMP port |

---

## 6. Recommendation

**Keep the ALIVE loop** for diagnostic purposes, with the following rationale:

1. **Not safety-critical**, but provides valuable operational telemetry
2. **Resource cost is negligible** — 8 KB stack, priority 1 (preempted by everything)
3. **Removes the only remaining uncertainty** around `vTaskDelete()` on the RP2350 SMP port
4. **HWM output is required** for verifying stack sizing during integration testing
5. The separate **stack overflow detection mechanism** (watchdog scratch register) operates independently

### 6.1 In-Flight Operation

The ALIVE loop can be **disabled at compile time** by undefining `ALIVE_LOOP_ENABLED`:

```c
// Add to config/FreeRTOSConfig.h or build flags:
#ifdef ALIVE_LOOP_ENABLED
    for (;;) { ... }
#else
    vTaskDelete(NULL);  // safe if SMP issues are resolved
#endif
```

This provides a path to disable it without code changes before flight if desired.

### 6.2 Alternative: HWM in HealthMonitor

If the ALIVE loop is ever removed, stack HWM monitoring should be added to
`health_monitor_task.c`. The HealthMonitor already monitors system health and
could log HWM via the event logger:

```c
// In health_monitor_tick():
uint32_t hwm = uxTaskGetStackHighWaterMark(h_sensor);
if (hwm < HWM_MIN_THRESHOLD) {
    fault_manager_report(FAULT_ID_STACK_LOW, WARNING);
}
```

This is **not implemented** — tracked as a future improvement if needed.

---

## 7. Conclusion

The ALIVE loop is **not required for safety-critical operation**. Its functions
are:

- ✅ Diagnostic (heap, HWM) — valuable for integration and operations
- ❌ Safety-critical — handled by separate mechanisms (HealthMonitor, FaultManager, scratch register)

**Recommendation**: Retain the ALIVE loop as-is. It consumes negligible resources
(~8 KB RAM, ~0.1% CPU) and provides useful operational visibility. The SMP
task-deletion uncertainty is avoided by keeping the loop. The separate
stack-overflow detection mechanism (watchdog scratch register) operates independently
and does not depend on the ALIVE loop.

---

## 8. Compliance

| Requirement | Status | Evidence |
|-------------|--------|----------|
| CDR-SAF-06 | ✅ Evaluated | This document — ALIVE loop retained, rationale documented |
| CDR-SW-05 | ✅ Evaluated | Same as CDR-SAF-06 |
| Safety: watchdog not fed by ALIVE loop | ✅ Compliant | HealthMonitor feeds TPS3431 watchdog independently |
| FDIR not dependent on ALIVE loop | ✅ Compliant | FaultManager + FlightModeManager handle FDIR |

---

**Author**: Software Lead  
**Reviewed by**: —  
**Approved**: —
