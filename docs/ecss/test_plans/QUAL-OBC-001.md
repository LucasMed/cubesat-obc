# Qualification Test Procedure — QUAL-OBC-001

**Version**: 1.0
**Date**: 2026-07-23
**Status**: Released
**Project**: CubeSat OBC Flight Software

---

## 1. Purpose

This procedure defines the environmental qualification requirements for the CubeSat OBC flight software. It specifies what tests must be performed, acceptance criteria, and reporting requirements for the AR (Acceptance Review) gate.

---

## 2. Scope

### Critical Subset (Mandatory for AR)

The following tests are **mandatory** for AR gate evaluation:

| Test ID | Test Name | Standard | Priority |
|---------|-----------|----------|----------|
| ATP-ENV-01 | Thermal cycling | ECSS-E-ST-10-02C | **MANDATORY** |
| ATP-ENV-02 | Vibration (random) | ECSS-E-ST-10-02C | **MANDATORY** |
| ATP-ENV-03 | Power supply variation | ECSS-E-ST-10-02C | **MANDATORY** |
| ATP-ENV-04 | Post-env functional verification | ECSS-E-ST-10-02C | **MANDATORY** |

### Nice-to-Have (Not Blocking AR)

| Test ID | Test Name | Notes |
|---------|-----------|-------|
| ATP-ENV-05..14 | EMI/EMC, radiation, mission-profile | If lab available |

---

## 3. References

- ATP-OBC-001 §9 — Environmental test cases
- ECSS-E-ST-10-02C — Environmental testing
- ECSS-Q-ST-30-02C — Qualification and approval of spacecraft components

---

## 4. Environmental Test Requirements

### 4.1 Thermal Cycling (ATP-ENV-01)

**Objective**: Verify software survives thermal stress.

**Setup**:
- OBC powered on with flight software running
- Telemetry active (temperature, battery, IMU)
- Data logging enabled

**Procedure**:
1. Start thermal cycling: -20°C to +50°C
2. Cycle count: 5 cycles
3. Dwell time: 30 minutes at each extreme
4. Transition time: ≤ 15 minutes

**Acceptance Criteria**:
- Software remains operational throughout cycling
- No crashes, resets, or watchdog entries
- Telemetry continues flowing
- Temperature readings remain valid

**Data to Record**:
- Cycle number
- Temperature at start/end of each dwell
- Software status (operational/crashed/reset)
- Any anomalies observed

### 4.2 Vibration (ATP-ENV-02)

**Objective**: Verify software survives mechanical stress.

**Setup**:
- OBC powered on with flight software running
- Telemetry active
- Mounted on vibration table

**Procedure**:
1. Random vibration: ≥ 14.1 g_rms
2. Frequency range: 20-2000 Hz
3. Duration: 3 minutes per axis (X, Y, Z)

**Acceptance Criteria**:
- Software remains operational during vibration
- No crashes, resets, or watchdog entries
- Post-vibration functional tests pass
- No mechanical damage observed

**Data to Record**:
- Vibration profile (g_rms, frequency range)
- Software status during vibration
- Post-vibration test results
- Physical inspection results

### 4.3 Power Supply Variation (ATP-ENV-03)

**Objective**: Verify software operates at voltage extremes.

**Setup**:
- OBC connected to programmable power supply
- Flight software running
- Telemetry active

**Procedure**:
1. Set supply to 3.0V (minimum)
2. Run functional tests for 5 minutes
3. Set supply to 5.5V (maximum)
4. Run functional tests for 5 minutes
5. Set supply to nominal (3.3V or 5V)

**Acceptance Criteria**:
- Software operates at 3.0V without crashes
- Software operates at 5.5V without crashes
- Functional tests pass at both extremes
- No data corruption observed

**Data to Record**:
- Voltage applied
- Current consumption
- Software status
- Test results at each voltage

### 4.4 Post-Environmental Functional Verification (ATP-ENV-04)

**Objective**: Verify all functional tests pass after environmental qualification.

**Setup**:
- OBC powered on (post-environmental)
- All peripherals connected

**Procedure**:
1. Run all ATP-FUNC tests (01..12)
2. Run all ATP-PERF tests (13..15)
3. Record results

**Acceptance Criteria**:
- All ATP-FUNC tests pass
- All ATP-PERF tests pass
- No degradation observed

**Data to Record**:
- Test ID
- Pass/Fail status
- Any anomalies

---

## 5. Reporting Requirements

For each test, the HIL lab must provide:

### 5.1 Test Execution Report

| Field | Required |
|-------|----------|
| Test ID | Yes |
| Test Name | Yes |
| Date/Time | Yes |
| Operator | Yes |
| Hardware Config | Yes (OBC serial, peripherals) |
| Software Version | Yes (from SCI-OBC-001) |
| Result (Pass/Fail) | Yes |
| Evidence (logs, screenshots) | Yes |
| Anomalies | Yes (if any) |

### 5.2 Summary Report

After all tests:
- Total tests executed
- Pass/Fail counts
- Deviations from expected results
- Recommendations

---

## 6. Deviation Handling

### 6.1 Minor Deviation

If a test fails but the failure is non-critical:
1. Document the deviation
2. Perform root cause analysis
3. Implement corrective action
4. Re-run the test
5. Update STR with results

### 6.2 Major Deviation

If a test fails and the failure is critical:
1. Stop testing
2. Notify project lead
3. Perform failure analysis
4. Implement corrective action
5. Re-qualify from the beginning

### 6.3 Deviation Report

For any deviation, create a deviation report with:
- Test ID and name
- Expected vs actual result
- Root cause
- Corrective action
- Re-test results

---

## 7. AR Gate Evaluation

After all critical tests are complete:
1. Compile results into AR-GATE-001 report
2. Evaluate against AR-CRITERIA-001
3. Make GO/NO-GO decision
4. If GO: proceed to FRR
5. If NO-GO: identify blocking items and re-test
