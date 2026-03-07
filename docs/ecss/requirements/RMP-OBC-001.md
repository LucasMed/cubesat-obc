# Risk Management Plan

**Document ID**: RMP-OBC-001
**Version**: 1.0
**Date**: 2026-03-07
**Status**: Approved — SRR Baseline
**Standard**: ECSS-M-ST-80C (Risk Management), ECSS-E-ST-40C §5
**Project**: CubeSat OBC Flight Software (RP2350 / Pico 2W)

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Risk Management Framework](#4-risk-management-framework)
5. [Risk Identification and Classification](#5-risk-identification-and-classification)
6. [Risk Register](#6-risk-register)
7. [Risk Response Strategies](#7-risk-response-strategies)
8. [Risk Monitoring and Review](#8-risk-monitoring-and-review)
9. [Open Items](#9-open-items)

---

## 1. Introduction

### 1.1 Purpose

This Risk Management Plan (RMP) defines the process for identifying, assessing,
responding to, and monitoring risks throughout the development and operation of
the CubeSat OBC flight software. Its goal is to prevent risks from becoming
issues and to minimize their impact when they do.

### 1.2 Scope

This plan covers risks in the following categories:

- **Technical risks**: software defects, design errors, integration failures
- **Schedule risks**: delays in development phases, milestone slippage
- **Resource risks**: toolchain dependency, personnel, hardware availability
- **Operational risks**: flight anomalies, on-orbit software failures

It does **not** cover launch vehicle, ground segment infrastructure, or RF link
risks to the extent they are independent of the OBC software.

### 1.3 Risk Management Objectives

1. Identify risks early — before they become issues
2. Quantify likelihood and impact to prioritize mitigation effort
3. Define clear mitigation actions with owners and target dates
4. Re-evaluate risks at each ECSS review gate
5. Generate lessons-learned at project close

---

## 2. Applicable Documents

| ID | Title | Version |
|---|---|---|
| SDP-OBC-001 | Software Development Plan | 1.0 |
| SyRS-OBC-001 | System Requirements Specification | 1.0 |
| SRS-OBC-001 | Software Requirements Specification | 2.0 |
| SVVP-OBC-001 | Software Verification & Validation Plan | 1.0 |
| PAP-OBC-001 | Product Assurance Plan | 1.0 |
| ECSS-M-ST-80C | Risk Management | — |
| ECSS-E-ST-40C | Software Engineering Standard | — |

---

## 3. Acronyms and Definitions

| Acronym / Term | Definition |
|---|---|
| RMP | Risk Management Plan |
| RL | Risk Likelihood (1–5 scale) |
| RC | Risk Consequence (1–5 scale) |
| RPN | Risk Priority Number = RL × RC |
| FDIR | Fault Detection, Isolation and Recovery |
| FMM | Flight Mode Manager |
| EPS | Electrical Power System |
| BV | Bus Voltage |
| WDT | Hardware Watchdog Timer |
| SMP | Symmetric Multi-Processing |
| HIL | Hardware-in-the-Loop |
| CI | Continuous Integration |

### 3.1 Risk Likelihood Scale

| Level | Label | Probability |
|---|---|---|
| 1 | Rare | < 5 % |
| 2 | Unlikely | 5–20 % |
| 3 | Possible | 20–45 % |
| 4 | Likely | 45–75 % |
| 5 | Almost Certain | > 75 % |

### 3.2 Risk Consequence Scale

| Level | Label | Impact |
|---|---|---|
| 1 | Negligible | No impact on schedule or functionality |
| 2 | Minor | Manageable with extra effort; no milestone slip |
| 3 | Moderate | Milestone slip < 2 weeks; partial feature impact |
| 4 | Major | Milestone slip > 2 weeks; significant feature loss |
| 5 | Critical | Mission failure or unrecoverable software state |

### 3.3 Risk Priority Thresholds

| RPN | Priority | Action Required |
|---|---|---|
| 1–4 | Low | Monitor only |
| 5–9 | Medium | Mitigation plan required |
| 10–14 | High | Immediate mitigation action + owner assigned |
| 15–25 | Critical | Escalate to project lead; stop / redesign if needed |

---

## 4. Risk Management Framework

### 4.1 Risk Lifecycle

```
Identify → Analyze → Evaluate → Plan Response → Implement → Monitor → Close
```

### 4.2 Review Cadence

| Event | Risk Review Activity |
|---|---|
| Each PR merge | Assess whether any new technical risk was introduced |
| Monthly | Review risk register; update likelihoods and mitigations |
| ECSS review gate | Full risk register re-assessment; update status |
| Post-anomaly | Conduct 5-Why analysis; add / update risk register entry |

### 4.3 Risk Register Ownership

| Role | Responsibility |
|---|---|
| Project Lead | Overall risk register ownership; escalation decisions |
| Software Lead | Technical risk identification and mitigation |
| HW/SW Lead | HIL and hardware-related risk management |
| Each engineer | Identify and report risks within their domain |

---

## 5. Risk Identification and Classification

### 5.1 Risk ID Convention

`RISK-<category>-<NN>` where category:
- `SW` — Software / firmware
- `HW` — Hardware
- `SC` — Schedule
- `RES` — Resource / toolchain
- `OPS` — Operational / on-orbit

---

## 6. Risk Register

### 6.1 Software / Firmware Risks

| ID | Title | Description | RL | RC | RPN | Status |
|---|---|---|---|---|---|---|
| RISK-SW-001 | FreeRTOS SMP instability | SMP (`configNUMBER_OF_CORES=2`) disabled due to boot anomalies observed on hardware. If enabled prematurely, could cause task corruption or deadlock. | 3 | 5 | 15 | **Open — mitigation active** |
| RISK-SW-002 | Stack overflow in task | Insufficient task stack size causes `configCHECK_FOR_STACK_OVERFLOW` trigger → FMM forced to SAFE with no recovery path. | 2 | 4 | 8 | Open |
| RISK-SW-003 | EKF numerical divergence | EKF diverges on large/sudden attitude change (e.g., tumble), producing NaN/inf states that propagate to control output. | 2 | 4 | 8 | Open |
| RISK-SW-004 | CSP buffer exhaustion | `csp_buffer_get()` returns NULL under high command rate; telemetry task silently drops packets with no fault raised. | 3 | 3 | 9 | Open |
| RISK-SW-005 | CMD_REBOOT bypasses FMM | `CMD_REBOOT` (COMMS-DES-001 OI-4) does not call `fmm_request_transition(FM_SAFE)` before rebooting, leaving actuators in unknown state. | 4 | 3 | 12 | **Open — OI active** |
| RISK-SW-006 | CMD_SET_MODE unconstrained | `CMD_SET_MODE` (COMMS-DES-001 OI-3) does not validate that the requested mode is reachable from the current state via FMM FSM. | 3 | 3 | 9 | **Open — OI active** |
| RISK-SW-007 | Fault cascade under ISR | `fault_report_from_isr()` not implemented (FAULT-DES-001 OI-3); ISR-origin faults silently dropped. | 3 | 3 | 9 | Open |
| RISK-SW-008 | Logger ring buffer race | Logger uses `taskENTER_CRITICAL`; ISR-context calls use a different spinlock path. A high-rate ISR may corrupt the tail pointer. | 2 | 3 | 6 | Open |
| RISK-SW-009 | Integer overflow in EPS hysteresis | Energy state thresholds are `float` comparisons; if bus voltage ADC produces out-of-range value, state machine may cycle rapidly. | 2 | 2 | 4 | Open |
| RISK-SW-010 | Watchdog not yet connected | `FR-12` hardware watchdog task HAL not finalized; no watchdog kick in production firmware means no protection against task hangs. | 4 | 4 | 16 | **Open — critical** |

### 6.2 Hardware Risks

| ID | Title | Description | RL | RC | RPN | Status |
|---|---|---|---|---|---|---|
| RISK-HW-001 | IMU I2C bus lockup | MPU-6050 can lock the I2C bus on power-cycle glitch; recovery requires bus reset. No I2C bus reset implemented. | 3 | 3 | 9 | Open |
| RISK-HW-002 | UART1 cable reliability | KISS framing over UART1 (GPIO8/9) is tested in-lab; connector reliability on assembled CubeSat structure not validated. | 3 | 3 | 9 | Open |
| RISK-HW-003 | RP2350 thermal throttling | RP2350 does not have thermal throttling; sustained motor drive at high ambient may affect timing. | 1 | 2 | 2 | Low |

### 6.3 Schedule Risks

| ID | Title | Description | RL | RC | RPN | Status |
|---|---|---|---|---|---|---|
| RISK-SC-001 | CDR documentation backlog | Multiple CDR documents (OBC-DES-001, FSW-SDD-001, FMEA-OBC-001/002, POWER-BDG-001, LINK-BDG-001) still to be produced; concurrent with Phase 5 development. | 4 | 3 | 12 | **Open** |
| RISK-SC-002 | Hardware availability for HIL | Physical Pico 2W hardware and assembled PCBs needed for Phase 6 HIL testing; procurement or assembly delay slips TRR. | 3 | 4 | 12 | Open |
| RISK-SC-003 | Integration test phase not started | Integration test suite (`tests/integration/`) and SIL emulation scenarios not yet implemented; may require significant effort before TRR. | 4 | 3 | 12 | Open |

### 6.4 Resource / Toolchain Risks

| ID | Title | Description | RL | RC | RPN | Status |
|---|---|---|---|---|---|---|
| RISK-RES-001 | CI tool version drift | clang-format version mismatch between CI runner and local environment was root cause of a CI failure (Phase 7). Mitigated by pinning to `ubuntu-22.04` and `clang-format-14`. | 1 | 2 | 2 | **Closed — mitigated** |
| RISK-RES-002 | Pico SDK breaking changes | Future Pico SDK submodule update may change HAL APIs (e.g., UART, I2C) incompatibly. | 2 | 3 | 6 | Open |
| RISK-RES-003 | FreeRTOS ARM_CM33_NTZ port defect | FreeRTOS ARM_CM33_NTZ port was selected to avoid PendSV stack corruption observed with ARM_CM0 port. If the CM33 port has unresolved defects, the RTOS startup may fail. | 1 | 5 | 5 | Open |
| RISK-RES-004 | libcsp API instability | libcsp is under active development; API changes in the submodule may break `comm_init.c`, `telemetry_task.c`, `command_task.c`. | 2 | 3 | 6 | Open |

### 6.5 Operational / On-Orbit Risks

| ID | Title | Description | RL | RC | RPN | Status |
|---|---|---|---|---|---|---|
| RISK-OPS-001 | Unrecoverable SAFE state | FMM transition to SAFE disables attitude control; if SAFE cannot self-recover (e.g., EPS stuck below threshold), satellite becomes uncontrollable. | 2 | 5 | 10 | Open |
| RISK-OPS-002 | Flash wear | Repeated in-orbit firmware updates via UF2 could lead to flash sector wear on RP2350. No wear levelling implemented. | 1 | 3 | 3 | Low |
| RISK-OPS-003 | Single-event upsets (SEU) | RP2350 has no ECC RAM; SEUs in space environment may corrupt task stacks or control state. | 2 | 4 | 8 | Open |
| RISK-OPS-004 | Ground contact loss | If CSP router or UART driver hangs, there is no autonomous recovery of comms (watchdog covers core hangs but not UART driver freeze). | 2 | 4 | 8 | Open |

---

## 7. Risk Response Strategies

### 7.1 High/Critical Priority Mitigations

**RISK-SW-001 — FreeRTOS SMP instability**
- *Mitigation*: Keep `configNUMBER_OF_CORES = 1` until Phase 6 HIL boot testing
  on hardware confirms stability. SMP enablement gated on dedicated HIL regression.
- *Owner*: SW Lead | *Target*: Phase 6 entry

**RISK-SW-005 — CMD_REBOOT bypasses FMM**
- *Mitigation*: COMMS-DES-001 OI-4 — implement `fmm_request_transition(FM_SAFE)`
  call with minimum 500 ms dwell before `watchdog_reboot()` in `command_task.c`.
- *Owner*: SW Lead | *Target*: CDR

**RISK-SW-010 — Watchdog not connected**
- *Mitigation*: Implement `watchdog_hal.c` with `health_monitor_task` kick.
  Required before any HIL testing. Blocking for TRR entry.
- *Owner*: SW Lead | *Target*: Phase 6

**RISK-SC-001 — CDR documentation backlog**
- *Mitigation*: Dedicate one sprint per CDR document. Priority order: OBC-DES-001 →
  FSW-SDD-001 → FMEA-OBC-001 → POWER-BDG-001 → LINK-BDG-001.
- *Owner*: Project Lead | *Target*: CDR gate

**RISK-SC-002 / SC-003 — Hardware and integration test delays**
- *Mitigation*: Parallel-track integration test infrastructure development with
  CDR documentation. SIL emulator (`build_emu/`) development begins in Phase 5.
- *Owner*: SW Lead | *Target*: TRR entry

**RISK-OPS-001 — Unrecoverable SAFE state**
- *Mitigation*: Define autonomous SAFE→NOMINAL re-arming criteria (EPS recovery
  threshold + minimum dwell time). Implement in `eps_monitor.c` + FMM. Requires
  `SAFE_REARM_VBATT_THRESHOLD` constant (planned OI).
- *Owner*: SW/EPS Lead | *Target*: CDR

### 7.2 Medium Priority Mitigations

**RISK-SW-002 — Stack overflow**
- *Mitigation*: `configCHECK_FOR_STACK_OVERFLOW = 2` enabled. HIL stack high-water
  mark monitoring during stress test.

**RISK-SW-003 — EKF divergence**
- *Mitigation*: Bound check on EKF state vector; if any element exceeds 2π (rad)
  or 10 rad/s, reset to last known good state and raise `FAULT_ADCS_EKF_DIVERGE`.

**RISK-SW-004 — CSP buffer exhaustion**
- *Mitigation*: Add `fault_report(FAULT_COMMS_CSP_BUFFER_EMPTY, FAULT_LEVEL_WARNING)`
  on `csp_buffer_get()` returning NULL. Monitor via logger.

**RISK-SW-006 — CMD_SET_MODE unconstrained**
- *Mitigation*: COMMS-DES-001 OI-3 — validate requested mode against
  `fmm_is_transition_valid()` before calling `fmm_request_transition()`.

**RISK-HW-001 — I2C bus lockup**
- *Mitigation*: Implement I2C bus reset (`gpio_set_function` toggle) in
  `mpu6050_init()` recovery path. Log to fault manager on repeated failure.

---

## 8. Risk Monitoring and Review

### 8.1 Risk Register Updates

The risk register in this document is updated:
- At each ECSS review gate (SRR, PDR, CDR, TRR, AR/QR)
- When a risk materializes and becomes an issue
- When a mitigation is completed and the risk can be downgraded or closed

### 8.2 Summary Dashboard

Current risk snapshot at SRR baseline (`a9cb3b0`, 2026-03-07):

| Priority | Count | Items |
|---|---|---|
| Critical (RPN 15–25) | 2 | RISK-SW-001, RISK-SW-010 |
| High (RPN 10–14) | 5 | RISK-SW-005, RISK-SC-001, RISK-SC-002, RISK-SC-003, RISK-OPS-001 |
| Medium (RPN 5–9) | 11 | SW-002..004, SW-006..008, HW-001..002, RES-002..004, OPS-003..004 |
| Low (RPN 1–4) | 3 | SW-009, HW-003, OPS-002 |
| Closed | 1 | RISK-RES-001 |

### 8.3 Issues Derived from Risks

When a risk materializes, an Issue is created in the GitHub issue tracker and
cross-referenced back to the risk ID (e.g., `RISK-SW-005, see #42`).

---

## 9. Open Items

| ID | Description | Priority | Owner | Status |
|---|---|---|---|---|
| OI-1 | SAFE→NOMINAL re-arming criteria not yet defined — required for RISK-OPS-001 mitigation | High | SW/EPS Lead | Open |
| OI-2 | `fault_report_from_isr()` implementation deferred (closes RISK-SW-007) — target CDR | High | SW Lead | Open |
| OI-3 | Watchdog HAL implementation not complete (RISK-SW-010) — blocking for TRR | Critical | SW Lead | Open |
| OI-4 | SEU mitigation strategy not defined for flight qualification (RISK-OPS-003) | Medium | Project Lead | Open |
