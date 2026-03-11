# ECSS PDR Checklist — OBC SOFTWARE (CubeSat)

## 1. Requirements Baseline

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-REQ-01   | Complete SRS exists, versioned, under config control                 | ✔      | SRS-OBC-001 v2.x, under Git, change history tracked |
| PDR-REQ-02   | Requirements are verifiable (shall, quantified, testable)            | ✔      | Most requirements use "shall", are quantified and testable (see SRS, SyRS) |
| PDR-REQ-03   | Traceability: REQ → subsystem → SW modules                           | ✔      | Complete: RTM-OBC-001 maps all SRS/SyRS requirements to modules and tests |
| PDR-REQ-04   | Fault detection & recovery requirements defined                      | ✔      | SyRS §5, SRS, and FDIR matrix cover detection/recovery (e.g., watchdog, comms loss) |
| PDR-REQ-05   | Traceability: requirement → verification method                      | ✔      | RTM-OBC-001 and SVVP-OBC-001 provide mapping to test/analysis/inspection |

## 2. Software Architecture

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-ARCH-01  | Software architecture defined (diagram, responsibilities, boundaries)| ✔      | SAD-OBC-001, FSW-SDD-001, and architecture diagrams exist |
| PDR-ARCH-02  | Modular architecture (Flight Manager, ADCS, EPS, etc.)               | ✔      | Modules and boundaries defined in SAD, SDD, and code structure |
| PDR-ARCH-03  | FreeRTOS task model defined (list, priorities, scheduling)           | ✔      | Task list, priorities, and scheduling in SDD §7, config/FreeRTOSConfig.h |
| PDR-ARCH-04  | Shared resource control (mutex, queue, semaphore)                    | ✔      | Use of FreeRTOS mutexes, queues, semaphores documented in SDD and code |
| PDR-ARCH-05  | Deadlock/race condition protection                                   | ✔      | Locking order, timeouts, and watchdog reset documented (see SDD, code) |

## 3. Interface Definition

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-INT-01   | Interfaces with ADCS, EPS, COMMS, Payload defined (table)            | ✔      | ICD-OBC-001, PAYLOAD-SPEC-001, and SAD-OBC-001 provide tables and diagrams |
| PDR-INT-02   | Communication protocols defined (UART, I2C, SPI, CAN, CSP)           | ✔      | ICD-OBC-001, COMMS-DES-001, and code document all protocols used |
| PDR-INT-03   | Packet formats defined (telemetry, telecommand, payload)             | ✔      | Packet formats in ICD-OBC-001, COMMS-DES-001, and code comments |
| PDR-INT-04   | CSP endpoints defined (port allocation, routing, services)           | ✔      | CSP port allocation and routing in COMMS-DES-001, ICD-OBC-001, and code |

## 4. Data Flow Definition

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-DATA-01  | Telemetry acquisition → storage → transmission flow defined          | ✔      | Data flow diagrams in SAD-OBC-001, SDD, and PAYLOAD-SPEC-001 |
| PDR-DATA-02  | Command reception → validation → execution flow defined              | ✔      | Command flow in SDD, COMMS-DES-001, and code |
| PDR-DATA-03  | Data prioritization (critical, housekeeping, payload)                | ✔      | Prioritization in SDD, code, and telemetry/command task logic |

## 5. Resource Budgets

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-RES-01   | CPU budget exists (per task)                                         | ✔      | Formal CPU budget table available in SDD §14.1 |
| PDR-RES-02   | RAM budget exists (stack, heap, buffers, margin)                     | ✔      | Stack/heap sizes in FreeRTOSConfig.h, SDD, and code comments |
| PDR-RES-03   | Flash memory budget exists (.text, .data, .rodata)                   | ✔      | Documented in SDD, build artifacts, and CHANGELOG.md |
| PDR-RES-04   | Stack analysis performed (watermark)                                 | ✔      | Stack watermarking implemented and reported in SDD, code, and test logs |

## 6. Fault Detection Isolation and Recovery (FDIR)

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-FDIR-01  | FDIR strategy exists (detection, isolation, recovery)                | ✔      | FDIR matrix in SyRS, SDD, and FAULT-DES-001 |
| PDR-FDIR-02  | Watchdog system defined (timeout, reset behavior)                    | ✔      | Hardware watchdog (TPS3431), timeout, and reset chain in ICD-OBC-001, SDD |
| PDR-FDIR-03  | Recovery for task crash, comms loss, memory corruption               | ✔      | Recovery paths in SDD, code, and FMEA-OBC-001 |

## 7. External Libraries

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-LIB-01   | External libraries identified (FreeRTOS, libcsp, drivers)            | ✔      | All libraries listed in SDD, CMake, and third_party/ |
| PDR-LIB-02   | Versions frozen (version, repo, commit)                              | ✔      | Version and commit pinned in CMakeLists.txt, submodules, and docs |
| PDR-LIB-03   | Compatibility with architecture analyzed                             | ✔      | Compatibility checked and documented in SDD, COMMS-DES-001 |

## 8. Verification Strategy

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-VER-01   | Verification plan exists (unit, integration, system test)            | ✔      | SVVP-OBC-001, STP-OBC-001, and test suites implemented |
| PDR-VER-02   | Hardware-in-the-Loop strategy exists                                 | ✔      | HIL plan in SVVP-OBC-001, test cases in STP-OBC-001 |
| PDR-VER-03   | Fault simulation exists (sensor timeout, task crash, power drop)     | ✔      | Fault injection and simulation in test plans and code |

## 9. Configuration Management

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-CM-01    | Configuration control (git, branch policy, tagging)                  | ✔      | Git, protected branches, PR policy, tags, documented in CMP-OBC-001, SDP-OBC-001 |
| PDR-CM-02    | Documents are versioned                                              | ✔      | All docs versioned, tracked in Git, with change history tables |

## 10. Documentation

| ID           | Item                                                                 | Status | Comments/Findings |
|--------------|----------------------------------------------------------------------|--------|-------------------|
| PDR-DOC-01   | Key documents exist (SRS, ICD, Architecture, Verification plan)      | ✔      | All required docs present and versioned in docs/ecss/ |

---

## 11. Scoring Model

| Score | Meaning         |
|-------|----------------|
| 0     | Not addressed  |
| 1     | Major gaps     |
| 2     | Partial        |
| 3     | Acceptable     |
| 4     | Good           |
| 5     | Flight ready   |

---

## 12. PDR Decision Logic

- ≥ 80% checks PASS → PDR PASS
- 60–80% → PASS WITH ACTIONS
- < 60% → PDR FAIL

---

## 13. Output

- **PDR Findings:** All major ECSS PDR checklist items are fully addressed. Requirements traceability is complete and CPU budget is documented.
- **Technical Score:** 5/5 (Flight ready design)
- **Risk Level:** Low
- **Action Items:** 
  - (Resolved) Complete and formalize requirements traceability matrix
  - (Resolved) Add formal CPU budget table per task
  - Continue to update and cross-reference all documents for consistency
- **PDR Decision:** PASS

---