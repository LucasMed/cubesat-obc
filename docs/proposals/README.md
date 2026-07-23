# Scientific Payload Proposals

**Directory**: `docs/proposals/`
**Purpose**: Future scientific implementations for the CubeSat OBC platform
**Status**: Active

---

## Overview

This directory contains **design proposals** for scientific payloads and
instrumentation that can be implemented on the CubeSat OBC platform. Each
proposal follows a standardized template and includes hardware specifications,
software architecture, command interfaces, and implementation plans.

The CubeSat OBC is designed as a **reusable scientific platform** — any
researcher or team can use these proposals as a starting point for their own
implementations.

---

## Available Proposals

| ID | Title | Status | Effort | Priority |
|----|-------|--------|--------|----------|
| [PROP-001](METEO-STATION-001.md) | Meteorological Station (BME280 + SHT31) | Proposed | 40 h | Medium |

---

## How to Use This Directory

### For Implementers

1. **Choose a proposal** that matches your scientific objectives
2. **Review the hardware requirements** — check if sensors are available
3. **Follow the implementation phases** — each proposal breaks work into phases
4. **Reference the acceptance criteria** — know when you're done

### For Contributors

If you have a new scientific payload idea:

1. **Copy the template** from any existing proposal
2. **Fill in all sections** — hardware, software, commands, risks
3. **Submit a PR** with your proposal
4. **Reference the proposal** in `PENDING_TASKS.md`

---

## Proposal Template Structure

Every proposal follows this structure:

```
1. Executive Summary
2. Motivation (scientific value + platform benefits)
3. Hardware Specification
4. Electrical Interface
5. Software Architecture
6. Data Structures
7. Flash Storage
8. Command Interface
9. Telemetry Integration
10. Algorithms (if applicable)
11. Fault Detection
12. Implementation Phases
13. New/Modified Files
14. Risk Assessment
15. Dependencies
16. Acceptance Criteria
17. Future Enhancements
```

---

## Platform Constraints

When designing a new payload, consider these constraints:

### Hardware

| Resource | Available | Notes |
|----------|-----------|-------|
| **I2C0** (GPIO4/5) | 3 free addresses | 7 devices max (address conflicts possible) |
| **I2C1** (GPIO2/3) | 2 free addresses | Camera uses this bus |
| **SPI0** (GPIO17/18/19) | 1 free CS | Camera, Mag, Flash share bus |
| **ADC** (GPIO28) | 1 channel | Radiation detector uses this |
| **GPIO** | ~5 free | GPIO23–27 available |
| **Power** | 250 mA @ 3.3V | Payload rail budget |

### Software

| Resource | Available | Notes |
|----------|-----------|-------|
| **FreeRTOS tasks** | 2–3 more | Stack budget: 128 KB heap |
| **CSP ports** | 30–39 | Port 20 (commands), 31 (telemetry) |
| **Flash** | ~36 KB free | 0x7F2000 – 0x7FFFFF |
| **Command IDs** | 37+ | Max ID currently: 36 |

### Power Budget

| Component | Power | Notes |
|-----------|-------|-------|
| BME280 | 3.6 µA (1 Hz) | Negligible |
| SHT31 | 800 µA (1 Hz) | Low |
| BH1750 | 120 µA (1 Hz) | Already on I2C0 |
| **Total headroom** | ~248 mA | After existing payload |

---

## Integration Checklist

For any new payload implementation:

- [ ] Hardware specification documented
- [ ] Electrical interface defined (I2C/SPI/UART/ADC pins)
- [ ] Power budget calculated
- [ ] FreeRTOS task designed (priority, stack, period)
- [ ] DLA fields added to `system_state_t`
- [ ] CSP command IDs allocated (port 30+)
- [ ] UART text commands defined
- [ ] Flash storage region allocated (if needed)
- [ ] Fault detection rules defined
- [ ] Unit tests written
- [ ] Integration tests written
- [ ] Hardware validation performed
- [ ] Documentation updated (ICD, RTM, PENDING_TASKS)
- [ ] CI pipeline green

---

## Existing Payload Reference

The existing payload subsystem (Phase 7) provides patterns for:

- **FreeRTOS task design**: `src/tasks/payload_task.c`
- **Driver architecture**: `src/drivers/payload/` (rm3100, camera, radiation)
- **Command interface**: `src/tasks/command_task.c` (CSP + UART)
- **Flash storage**: `src/services/storage/` (W25Q64 ring buffer)
- **DLA integration**: `src/core/data_layer.c`
- **Test patterns**: `tests/unit/test_payload_task.c`

Use these as reference when implementing new payloads.

---

*Last updated: 2026-07-22*
