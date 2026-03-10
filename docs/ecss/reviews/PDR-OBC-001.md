# PDR-OBC-001 — Preliminary Design Review Report
## CubeSat OBC Flight Software — RP2350 / Pico 2W

| Field            | Value                                             |
|------------------|---------------------------------------------------|
| **Document ID**  | PDR-OBC-001                                       |
| **Title**        | Preliminary Design Review Report                   |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                    |
| **Version**      | 1.0                                               |
| **Status**       | Issued — Pending Actions Resolution                |
| **Review Date**  | 2026-03-10                                        |
| **Reviewer**     | OBC Systems Review Board                          |
| **Review Level** | PDR                                               |
| **Standard**     | ECSS-E-ST-10-02C, ECSS-E-ST-40C, ECSS-Q-ST-80C, ECSS-M-ST-10C |

---

## Change History

| Version | Date       | Author               | Description              |
|---------|------------|----------------------|--------------------------|
| 1.0     | 2026-03-10 | OBC Systems Review Board | Initial PDR issue     |

---

## Table of Contents

1. [Review Summary](#1-review-summary)
2. [Major Architecture Risks](#2-major-architecture-risks)
3. [Minor Design Improvements](#3-minor-design-improvements)
4. [System Architecture Analysis](#4-system-architecture-analysis)
5. [Subsystem Interface Review](#5-subsystem-interface-review)
6. [Data Flow Analysis](#6-data-flow-analysis)
7. [Integration Risk Analysis](#7-integration-risk-analysis)
8. [Verification Strategy](#8-verification-strategy)
9. [ECSS Checklist Result](#9-ecss-checklist-result)
10. [Technical Scoring](#10-technical-scoring)
11. [Flight Readiness Assessment](#11-flight-readiness-assessment)
12. [Recommended Actions](#12-recommended-actions)

---

## 1. Review Summary

This Preliminary Design Review (PDR) is conducted under ECSS guidelines for the CubeSat OBC architecture based on FreeRTOS and C/C++ flight software. Subsystem boundaries, interfaces, data flows, HW/SW partitioning, failure scenarios, and integration are evaluated, focusing on the feasibility of meeting mission requirements.

## 2. Major Architecture Risks

- Subsystem interfaces are not fully defined (especially between OBC, ADCS, EPS, and COMMS).
- Lack of detailed documentation for critical data flows (telemetry, commands, payload data).
- Strong dependency on FreeRTOS without evidence of robustness analysis against task failure or memory corruption.
- Recovery mechanisms for software/hardware failures are not clearly identified.
- Integration of external libraries (e.g., CSP) without evidence of compatibility and security validation.

## 3. Minor Design Improvements

- Improve documentation of subsystem boundaries and responsibilities.
- Clearly specify protocols and data formats for each interface.
- Include sequence diagrams for main data flows.
- Document watchdog and failure recovery mechanisms.
- Add unit and integration tests for critical architecture points.

## 4. System Architecture Analysis

The proposed architecture follows a modular approach, with well-identified subsystems (ADCS, EPS, COMMS, Payload, Flight Management). FreeRTOS enables task partitioning, but more detail is needed on priority assignment, shared resource management, and fault protection. The Flight Management Module appears to centralize mission logic, but its interaction with other modules should be more explicit.

## 5. Subsystem Interface Review

- ADCS: Entry/exit points and synchronization mechanisms with OBC are not clearly specified.
- EPS: Details are missing on power event notification and low-power mode management.
- COMMS: CSP is used, but endpoints and queue/buffer management are not documented.
- Payload: The protocol for data acquisition and delivery is not detailed.
- Flight Management: How it receives events and commands, and how it reports states/faults, is not specified.

## 6. Data Flow Analysis

- Telemetry: The complete flow from acquisition to transmission is not described.
- Commands: Flow from reception to execution and acknowledgment is missing.
- Payload data: Temporary storage and prioritization over other data are not documented.
- Error handling: Notification and recovery flows for failures are not specified.

## 7. Integration Risk Analysis

- Risk of incompatibility between versions of external libraries (e.g., CSP).
- Potential race conditions due to concurrent access to shared resources.
- Lack of integration tests between subsystems.
- Absence of failure simulation and validation of recovery mechanisms.

## 8. Verification Strategy

- Review and complete documentation of interfaces and data flows.
- Implement unit and integration tests for each subsystem.
- Perform failure simulations and robustness tests.
- Validate compatibility of all external libraries.
- Document and test failure recovery mechanisms.

## 9. ECSS Checklist Result

- Mission requirements: Partially covered, full traceability missing.
- Interface definition: Incomplete.
- Data flows: Incomplete.
- Verification strategy: Partial.
- Risk management: Partial.
- Documentation: Needs improvement.

## 10. Technical Scoring

| Aspect                      | Score (0-10) |
|-----------------------------|:------------:|
| Architectural clarity       |      6       |
| Interface definition        |      5       |
| Fault robustness            |      4       |
| Documentation               |      5       |
| Integration readiness       |      5       |

## 11. Flight Readiness Assessment

The preliminary design is acceptable for continuation towards CDR, subject to resolution of identified actions. Interface and data flow definitions must be completed, and documentation and integration testing must be strengthened before proceeding to the next phase.

## 12. Recommended Actions

1. Complete documentation of interfaces and data flows.
2. Specify and document failure recovery mechanisms.
3. Implement and document unit and integration tests.
4. Validate compatibility of external libraries.
5. Review and improve requirements traceability.
6. Update documentation in the review branch and record changes in the PDR report.

---