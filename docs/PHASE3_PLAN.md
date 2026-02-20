# Phase 3: Communication & Telemetry Plan

## Overview

**Phase 3** focuses on establishing reliable, packet-based communication between the OBC and the external world (Ground Station or other subsystems) using the **CubeSat Space Protocol (libcsp)**. This phase transitions the system from simple debug logs (printf) to structured, binary telemetry and remote command handling (C&DH).

**Duration Estimate**: 2 weeks  
**Start Date**: 2026-02-21  
**Target Completion**: Early March 2026

---

## Goals

1. **CSP Protocol Integration**
   - Integrate `libcsp` into the FreeRTOS-SMP environment.
   - Configure memory pools and router settings for RP2350.
   - Implement the UART interface as the physical layer for CSP.

2. **Structured Telemetry**
   - Define binary telemetry frames (attitude, sensor health, power metrics).
   - Refactor `telemetry_task` to periodicically emit CSP packets.
   - Verify reception on a host-side CSP client (simulation).

3. **Remote Command Uplink (C&DH)**
   - Implement a new `command_task` to listen for incoming CSP packets.
   - Create a Command Dictionary for remote control (e.g., reset, safe mode, calibration).
   - Execute and acknowledge commands (ACK/NACK).

4. **Integration Testing**
   - End-to-end "Ground Station to OBC" loop.
   - Validate packet integrity and error recovery (CRC).

---

## Deliverables

### Code Changes
- [ ] `third_party/libcsp` — Added as a dependency.
- [ ] `src/core/csp_manager.c` — CSP stack initialization and routing.
- [ ] `src/drivers/uart/csp_uart.c` — UART driver for CSP interface.
- [ ] `src/tasks/command_task.c` — Command processing mission logic.
- [ ] `src/tasks/telemetry_task.c` (Update) — Switch to CSP-based telemetry frames.

### Documentation
- [ ] `docs/PHASE3_COMM_SPEC.md` — Packet formats and command dictionary.
- [ ] `docs/GS_INTERFACE_GUIDE.md` — How to talk to the OBC from a PC.
- [ ] Updated `docs/ARCHITECTURE.md` — Reflect the new communication stack.

### Tests
- [ ] Integration test: CSP Ping from host to Pico hardware.
- [ ] Integration test: Remote command execution (e.g., toggle LED).
- [ ] Integration test: Telemetry decoding validation.

---

## Work Breakdown Structure (WBS)

### Task 3.1: libcsp Integration (Est. 3 days)
1. **Source Integration**: Add `libcsp` to the project build system.
2. **OS Abstraction**: Link CSP to FreeRTOS (mutexes, tasks).
3. **Pico Hardware Interface**: Implement the UART interface using `pico_stdlib`.

**Acceptance Criteria**:
- [ ] `csp_init()` completes successfully on hardware.
- [ ] OBC responds to a "ping" on its CSP address.

### Task 3.2: Telemetry Refactor (Est. 3 days)
1. **Frame Definition**: Design the binary structure of the telemetry packet.
2. **Task Update**: Modify `telemetry_task` to build and send packets every 1 second.
3. **CRC Validation**: Enable CSP level checksums for data integrity.

**Acceptance Criteria**:
- [ ] Ground station receives valid binary packets.
- [ ] Telemetry contains correct data from `system_state`.

### Task 3.3: Command Uplink & CDH (Est. 4 days)
1. **Command Listener**: Implement a blocking listener in `command_task`.
2. **Command Dispatcher**: Implement logic to map command IDs to system actions.
3. **Safe State Logic**: Implement "Enter Safe Mode" command.

**Acceptance Criteria**:
- [ ] Command received → LED toggles / State changes.
- [ ] System sends a command acknowledgment back to sender.

---

## Success Criteria

**Phase 3 is COMPLETE when**:
1. ✅ `libcsp` is fully integrated and stable on the RTOS.
2. ✅ Binary telemetry is streaming reliably over UART.
3. ✅ Remote commands are correctly parsed and executed.
4. ✅ Telemetry and command dictionary are documented.
5. ✅ End-to-end communication verified with 0% packet loss in stable conditions.

---

**Last Updated**: 2026-02-20  
**Plan Version**: 1.0  
**Status**: Ready for Implementation
