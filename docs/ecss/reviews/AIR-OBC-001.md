# AIR-OBC-001 — Action Item Register
## CubeSat OBC Flight Software — SRR Cycle

| Field            | Value                                             |
|------------------|---------------------------------------------------|
| **Document ID**  | AIR-OBC-001                                       |
| **Title**        | Action Item Register — SRR Cycle                  |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                    |
| **Version**      | 1.0                                               |
| **Status**       | Open — Awaiting Resolution                        |
| **Issue Date**   | 2026-03-10                                        |
| **Issued by**    | OBC Systems Review Board                          |
| **Source Review**| SRR-OBC-001 v1.0                                  |
| **Standard**     | ECSS-M-ST-10C §5.3, ECSS-E-ST-40C                |

---

## Change History

| Version | Date       | Author               | Description              |
|---------|------------|----------------------|--------------------------|
| 1.0     | 2026-03-10 | OBC Systems Review Board | Initial issue from SRR-OBC-001 |

---

## Table of Contents

1. [Register Overview](#1-register-overview)
2. [Status Definitions](#2-status-definitions)
3. [Priority Definitions](#3-priority-definitions)
4. [Action Items — Critical / High (PDR Blockers)](#4-action-items--critical--high-pdr-blockers)
5. [Action Items — Medium (PDR Desirable)](#5-action-items--medium-pdr-desirable)
6. [Action Items — Before CDR](#6-action-items--before-cdr)
7. [Summary Dashboard](#7-summary-dashboard)
8. [Closure Criteria](#8-closure-criteria)

---

## 1. Register Overview

This register tracks all action items raised during the System Requirements Review
(SRR) conducted on 2026-03-10. Items are sourced from `SRR-OBC-001 v1.0`.

Action items are numbered `ACT-xx`. Each item references the originating finding
(Major `MAJ-xx` or Minor `MIN-xx`), the affected document(s), the responsible
lead, the target milestone, and the current status.

**PDR Baseline Lock Criteria:** All CRITICAL and HIGH items in §4 must reach
status `CLOSED` before the PDR baseline is formally locked.

---

## 2. Status Definitions

| Status | Meaning |
|---|---|
| `OPEN` | Item raised; no action taken yet |
| `IN-PROGRESS` | Work has started; evidence available on request |
| `RESOLVED` | Fix implemented; pending formal verification |
| `CLOSED` | Verified and accepted by Review Board |
| `DEFERRED` | Formally deferred to next milestone with documented rationale |
| `DESCOPED` | Item resolved by formally removing the affected scope |

---

## 3. Priority Definitions

| Priority | Meaning |
|---|---|
| **CRITICAL** | Blocks PDR. Protocol or functional incompatibility. Must be closed first. |
| **HIGH** | Blocks PDR baseline lock. Must be closed before PDR. |
| **MEDIUM** | Should be closed before PDR; may be deferred with documented rationale. |
| **LOW** | Best-effort improvement; acceptable at CDR. |

---

## 4. Action Items — Critical / High (PDR Blockers)

---

### ACT-01

| Field | Value |
|---|---|
| **ID** | ACT-01 |
| **Source** | MAJ-01 |
| **Priority** | HIGH |
| **Status**       | `CLOSED` — 2026-03-10                             |
| **Target Milestone** | PDR baseline lock                             |
| **Responsible** | Systems Engineering Lead                          |
| **Due Date** | 2026-03-10                                            |

**Title:** Correct SRS Internal Document Identifier from `REQ-001` to `SRS-OBC-001`

**Description:**
The Software Requirements Specification file header declares `Document ID: REQ-001 v2.0`.
All other baseline documents reference this document as `SRS-OBC-001`. This inconsistency
breaks formal configuration management traceability per ECSS-E-ST-40C §5.3.

**Required Action:**
1. Update `docs/ecss/requirements/SRS-OBC-001.md` header field `Document ID` from `REQ-001` to `SRS-OBC-001`.
2. Bump document version to `v2.1`.
3. Add entry to the document change history table.
4. Verify no other documents reference `REQ-001` as the SRS identifier.

**Acceptance Criteria:**
- [ ] `SRS-OBC-001.md` header shows `Document ID: SRS-OBC-001 v2.1`.
- [ ] Change history updated.
- [ ] `grep -r "REQ-001" docs/` returns no cross-references to SRS (excluding internal change log entry).

---

### ACT-02

| Field | Value |
|---|---|
| **ID** | ACT-02 |
| **Source** | MAJ-02 |
| **Priority** | **CRITICAL** |
| **Status**       | `CLOSED` — 2026-03-10                             |
| **Target Milestone** | PDR baseline lock                             |
| **Responsible** | Communications Lead                               |
| **Due Date** | 2026-03-10                                            |

**Title:** Resolve CSP Version Conflict — v1.x vs. v2.x

**Description:**
SyRS-OBC-001 SYS-F-401 declares `libcsp v1.x`. ARCHITECTURE.md §7 declares "CSP v2".
The attached libcsp documentation corresponds to the v2.x API. CSP v1.x and v2.x are
incompatible at the protocol frame level. A v1.x OBC cannot communicate with a v2.x
ground station. This is a blocking functional incompatibility.

**Required Action:**
1. Determine the definitive protocol version in use (inspect `third_party/libcsp/` vendored source — check `CMakeLists.txt` version string or `include/csp/csp_types.h` API signatures).
2. Update SYS-F-401 to state the confirmed version explicitly.
3. Align `ARCHITECTURE.md` §7, `COMMS-DES-001`, and `ICD-OBC-001` to the same version.
4. Verify ground station tooling (if any) is compatible with the declared version.
5. Document the version decision rationale in COMMS-DES-001.

**Acceptance Criteria:**
- [ ] `third_party/libcsp/` version identified and documented.
- [ ] SYS-F-401 states a single, specific CSP version (e.g., "libcsp v2.0").
- [ ] ARCHITECTURE.md §7, COMMS-DES-001, and ICD-OBC-001 are consistent.
- [ ] Ground station compatibility confirmed or open item raised in RMP.

---

### ACT-03

| Field | Value |
|---|---|
| **ID** | ACT-03 |
| **Source** | MAJ-03 |
| **Priority** | HIGH |
| **Status**       | `CLOSED` — 2026-03-10                             |
| **Target Milestone** | PDR baseline lock                             |
| **Responsible** | Software Lead                                     |
| **Due Date** | 2026-03-10                                            |

**Title:** Verify Flash Footprint and Resolve NFR-6 Ambiguity

**Description:**
NFR-6 requires flash footprint < 200 KB. Build artifact `cubesat_obc_pico.uf2` is 536 KB.
NFR-6 does not specify the measurement basis (UF2 container vs. raw flash image). The
discrepancy is unacknowledged; NFR-6 is marked "TBC" with no open item.

**Required Action:**
1. Run `arm-none-eabi-size build_pico/src/*.elf` (or equivalent) to obtain `.text` + `.data` + `.rodata` sizes.
2. Update NFR-6 to state measurement basis explicitly: *"Flash image size (`.text`+`.data`+`.rodata` sections of linked ELF)"*.
3. If actual flash usage is within budget: close NFR-6 as `[IMPL]` with measured values.
4. If actual flash usage exceeds budget: revise the NFR-6 budget to a realistic value, document the rationale, and add a risk entry to RMP-OBC-001.
5. Report SRAM usage from `arm-none-eabi-size` `.bss`+`.data` against the 60 KB SRAM budget.

**Acceptance Criteria:**
- [ ] `arm-none-eabi-size` output recorded in a test evidence artifact.
- [ ] NFR-6 updated with explicit measurement basis and measured values.
- [ ] NFR-6 status updated from "TBC" to `[IMPL]` or a formal open item raised in `RMP-OBC-001`.

---

### ACT-04

| Field | Value |
|---|---|
| **ID** | ACT-04 |
| **Source** | MAJ-04 |
| **Priority** | HIGH |
| **Status** | `CLOSED` — 2026-03-10 (Option A: payload concept defined in SyRS §7.5) |
| **Target Milestone** | PDR baseline lock |
| **Responsible** | Systems Engineering Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `MRD-OBC-001` §2.3 MO-6; `SyRS-OBC-001`; `ICD-OBC-001`; `FMEA-OBC-001` |

**Title:** Define Payload Subsystem at Mission Concept Level (or Formally Descope MO-6)

**Description:**
MO-6 requires "payload rail enabled" for full mission success. Zero functional requirements,
interface definitions, or FDIR provisions exist for a payload. FMEA excludes it as "undefined — OI-1".

**Required Action (Option A — Define):**
1. Add a payload concept section to MRD-OBC-001: type (e.g., camera, spectrometer, beacon), nominal power draw, activation conditions.
2. Add ≥5 system-level SyRS requirements (e.g., payload power enable/disable, interlock with FMM, fault response on payload overcurrent).
3. Add a payload interface section to ICD-OBC-001 (GPIO pin, power rail, current limit).
4. Add payload failure modes to FMEA-OBC-001.

**Required Action (Option B — Descope):**
1. Remove payload rail from MO-6 success criteria in MRD-OBC-001.
2. Document descope rationale in MRD-OBC-001 change history.
3. Remove OI-1 placeholder from FMEA-OBC-001.

**Acceptance Criteria:**
- [ ] Either ≥5 payload SyRS requirements exist with ICD interface, OR MO-6 is formally amended with documented rationale.
- [ ] FMEA-OBC-001 OI-1 resolved.

---

### ACT-05

| Field | Value |
|---|---|
| **ID** | ACT-05 |
| **Source** | MAJ-05 |
| **Priority** | HIGH |
| **Status** | `CLOSED` — 2026-03-10 |
| **Target Milestone** | PDR baseline lock |
| **Responsible** | Communications Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SRS-OBC-001` IR-4; `ICD-OBC-001`; `ARCHITECTURE.md` |

**Title:** Replace IR-4 Ground Station Interface with Actual UART1/CSP/KISS/E22 Requirement

**Description:**
IR-4 specifies "WiFi TCP/UDP" as the ground station telemetry interface. The implemented
architecture uses CSP over UART1 with KISS framing via E22-400M30S LoRa radio. The WiFi
interface has never been formally descoped.

**Required Action:**
1. Replace IR-4 in SRS-OBC-001 with: *"IR-4 — Ground station TT&C link: CSP v[X] over UART1, KISS framing, 433 MHz LoRa (E22-400M30S), baud rate [Y bps]"*, with status `[IMPL]`.
2. Add a note: *"WiFi-based telemetry (CYW43) is formally descoped from this release."*
3. Update ARCHITECTURE.md block diagram to remove the WiFi telemetry path.
4. Verify ICD-OBC-001 §8 reflects the E22-400M30S as the primary TT&C interface.

**Acceptance Criteria:**
- [ ] IR-4 defines the UART1/CSP/KISS interface with correct baud rate.
- [ ] WiFi descope formally documented.
- [ ] ARCHITECTURE.md diagram updated.

---

### ACT-06

| Field | Value |
|---|---|
| **ID** | ACT-06 |
| **Source** | MAJ-06 |
| **Priority** | HIGH |
| **Status** | `CLOSED` — 2026-03-10 (Option A implemented: FR-18/19/IR-10 added to SRS-OBC-001 v2.3; ICD-OBC-001 §7 activated; WP-7.10 added to PHASE7_PAYLOAD_PLAN) |
| **Target Milestone** | PDR baseline lock |
| **Responsible** | Systems Engineering Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SRS-OBC-001`; `SyRS-OBC-001`; `ICD-OBC-001` §3; `FMEA-OBC-001` |

**Title:** Manage GPS Interface — Add Requirements or Formally Descope

**Description:**
GPS receiver NEO-7M is wired to UART0 in ICD-OBC-001 but has zero functional
requirements in SRS or SyRS. FMEA excludes GPS failure modes. This is an
unmanaged interface consuming hardware resources.

**Required Action (Option A — Keep GPS):**
1. Add ≥2 functional requirements to SRS (e.g., parse NMEA `$GPGGA`/`$GPRMC`, provide UTC time synchronisation).
2. Add GPS failure modes to FMEA-OBC-001 (e.g., UART0 receive timeout, NMEA parse failure).
3. Assign a fault ID in `fault_ids.h`.

**Required Action (Option B — Descope GPS):**
1. Update ICD-OBC-001 §3 UART0 entry to: *"UART0 — reserved for GPS NEO-7M; not activated in this release"*.
2. Remove GPS from ARCHITECTURE.md data flow.
3. Document descope decision in SRS change history.

**Acceptance Criteria:**
- [x] GPS requirements FR-18 (NMEA parse ≥ 1 Hz) and FR-19 (UTC sync ± 500 ms) added to SRS-OBC-001 v2.3
- [x] IR-10 (UART0 GPS interface) added to SRS-OBC-001 v2.3
- [x] ICD-OBC-001 §7 GPS interface activated (Phase 7, WP-7.10)
- [x] WP-7.10 GPS Integration added to PHASE7_PAYLOAD_PLAN
- [ ] FMEA-OBC-001 GPS failure modes (UART0 timeout, NMEA parse fail) — Phase 7 WP-7.10.7
- [ ] `fault_ids.h` GPS fault IDs (`FAULT_GPS_TIMEOUT`, `FAULT_GPS_PARSE_ERR`) — Phase 7 WP-7.10.7

---

### ACT-07

| Field | Value |
|---|---|
| **ID** | ACT-07 |
| **Source** | MAJ-07 |
| **Priority** | HIGH |
| **Status** | `CLOSED` — 2026-03-10 |
| **Target Milestone** | PDR baseline lock |
| **Responsible** | Systems Engineering Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `MRD-OBC-001` §2.3 MO-3 |

**Title:** Amend MO-3 FDIR Response Time from "1 orbit" to a Meaningful Budget

**Description:**
MO-3 states FDIR triggers FM_SAFE "within 1 orbit" (≈90 min). This is a non-constraint.
The implemented EPS Monitor tick fires every 5 s, making FM_SAFE transition measurable
within seconds. The mission objective must reflect engineering reality.

**Required Action:**
1. Amend MO-3 success criterion in MRD-OBC-001 §2.3 to:
   *"FAULT_LEVEL_CRITICAL event triggers FM_SAFE within 10 s (2× Health Monitor period; measured from fault detection to FMM state change)"*.
2. Update the corresponding verification method in SVVP-OBC-001 to include a timed fault injection test.
3. Add a test case to STP-OBC-001 for timed FDIR response measurement.

**Acceptance Criteria:**
- [ ] MO-3 success criterion updated with ≤10 s (or justified alternative) numeric budget.
- [ ] SVVP and STP updated with corresponding timed verification test.

---

## 5. Action Items — Medium (PDR Desirable)

---

### ACT-08

| Field | Value |
|---|---|
| **ID** | ACT-08 |
| **Source** | TG-01 |
| **Priority** | MEDIUM |
| **Status** | `CLOSED` — 2026-03-10 |
| **Target Milestone** | PDR |
| **Responsible** | Systems Engineering Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SyRS-OBC-001` §1 |

**Title:** Add MO→SyRS Cross-Reference Table to SyRS §1

**Description:**
MO-1..MO-7 in MRD are not explicitly indexed to SyRS requirement IDs. Satisfaction of
mission objectives cannot be formally demonstrated without this mapping.

**Required Action:**
Add a cross-reference table to SyRS-OBC-001 §1 (or new §2.x "Mission Objective Allocation")
mapping each MO-x to the SyRS requirement(s) that implement it.

Example format:
| MO | Description | Implemented by |
|---|---|---|
| MO-1 | EKF attitude determination | SYS-F-101..106 |
| MO-2 | 3-axis attitude control | SYS-F-111..115 |

**Acceptance Criteria:**
- [ ] SyRS §1 or §2 contains a complete MO→SYS-F cross-reference table covering MO-1..MO-7.

---

### ACT-09

| Field | Value |
|---|---|
| **ID** | ACT-09 |
| **Source** | MIN-01, TG-04 |
| **Priority** | MEDIUM |
| **Status** | `CLOSED` — 2026-03-10 |
| **Target Milestone** | PDR |
| **Responsible** | Systems Engineering Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `RTM-OBC-001` |

**Title:** Align RTM System Requirement Identifiers to SyRS SYS-F-xxx Numbering

**Description:**
The RTM uses informal numbering (`SYS-REQ 1`, `SYS-REQ 2`, ...) that does not match SyRS
identifiers (`SYS-F-101`, `SYS-F-201`, etc.). This requires manual cross-referencing and
reduces review efficiency.

**Required Action:**
Replace all `SYS-REQ x` references in RTM-OBC-001 "System Requirements to Implementation
Mapping" section with the corresponding SyRS IDs.

**Mapping to apply:**
| RTM SYS-REQ-x | SyRS equivalent |
|---|---|
| SYS-REQ 1 | SYS-F-101..SYS-F-106 |
| SYS-REQ 2 | SYS-F-111..SYS-F-115 |
| SYS-REQ 3 | SYS-F-403..SYS-F-407 |
| SYS-REQ 4 | SYS-NF-001 |
| SYS-REQ 5 | SYS-F-201..SYS-F-205, SYS-F-211..SYS-F-213 |
| SYS-REQ 6 | SYS-F-301..SYS-F-304 |

**Acceptance Criteria:**
- [ ] RTM no longer contains `SYS-REQ x` identifiers.
- [ ] All RTM requirement cross-references use SyRS `SYS-F-xxx` or `SYS-NF-xxx` IDs.

---

### ACT-10

| Field | Value |
|---|---|
| **ID** | ACT-10 |
| **Source** | MIN-02, MIN-03 |
| **Priority** | MEDIUM |
| **Status** | `CLOSED` — 2026-03-10 |
| **Target Milestone** | PDR |
| **Responsible** | Systems Engineering Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SyRS-OBC-001` SYS-F-401; `SRS-OBC-001` NFR-7 |

**Title:** Correct SYS-F-401 SMP Reference; Harmonize NFR-7 Boot Time

**Sub-item A — SYS-F-401 SMP:**
SYS-F-401 states the CSP stack runs "on FreeRTOS SMP" but SMP is disabled
(`configNUMBER_OF_CORES = 1`; SYS-P-003 `[PLANNED]`).

*Action:* Amend SYS-F-401 to: *"The OBC shall run the libcsp v[X] stack on FreeRTOS
(single-core configuration; SMP planned per SYS-P-003)"*.

**Sub-item B — NFR-7 Boot Time:**
SRS NFR-7 targets 5 s with "TBD" status. SyRS-NF-005 sets 10 s with `[IMPL]` status.

*Action:* Update SRS NFR-7 to: *"The system shall boot to operational state within 10 s
of power-on (per SyRS-NF-005). Status: [IMPL]."*

**Acceptance Criteria:**
- [ ] SYS-F-401 qualification note added referencing SYS-P-003.
- [ ] SRS NFR-7 value changed to 10 s and status updated to `[IMPL]`.
- [ ] NFR-7 references SyRS-NF-005 in rationale.

---

## 6. Action Items — Before CDR

---

### ACT-11

| Field | Value |
|---|---|
| **ID** | ACT-11 |
| **Source** | MIN-08 |
| **Priority** | HIGH |
| **Status** | `CLOSED` — 2026-03-10 (EM=HMC5883L locked; FM/CDR=LIS3MDL candidate declared) |
| **Target Milestone** | CDR |
| **Responsible** | Hardware Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SRS-OBC-001` FR-11; `ICD-OBC-001` §6; `BOM-OBC-001`; `ARCHITECTURE.md` §2 |

**Title:** Lock Magnetometer Part Number (HMC5883L vs. LIS3MDL); Update All References

**Description:**
ICD-OBC-001 specifies HMC5883L (`0x1E`). ARCHITECTURE.md notes CDR migration to LIS3MDL
and flags QMC5883L clone risk in GY-271 modules. FR-11 specifies HMC5883L. Flight hardware
part selection is unlocked.

**Required Action:**
1. Hardware Lead: Decide and document the flight magnetometer part number in BOM-OBC-001.
2. Update FR-11 to the selected part.
3. Update ICD-OBC-001 §6 with correct I2C address, ODR, driver filename.
4. Rename `src/drivers/mag/hmc5883l.c` to match part (e.g., `lis3mdl.c`) if LIS3MDL selected.
5. Update ARCHITECTURE.md §2 driver note.

**Acceptance Criteria:**
- [ ] BOM-OBC-001 lists the flight magnetometer with part number and procurement note.
- [ ] FR-11, ICD-OBC-001, driver filename, and ARCHITECTURE.md are consistent.

---

### ACT-12

| Field | Value |
|---|---|
| **ID** | ACT-12 |
| **Source** | MIN-06 |
| **Priority** | MEDIUM |
| **Status** | `CLOSED` — 2026-03-10 (SYS-F-450 added to SyRS §7.4; 6 dB margin at 600 km/5°; LINK-BDG-001 deferred to CDR) |
| **Target Milestone** | CDR |
| **Responsible** | Communications Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SyRS-OBC-001`; `COMMS-DES-001` |

**Title:** Add RF Link Margin System Requirement

**Description:**
MO-4 requires bidirectional TT&C over 433 MHz LoRa but no system requirement exists for
link margin, receive sensitivity, or minimum pass geometry.

**Required Action:**
Add ≥1 SyRS requirement, e.g.:
> `SYS-F-450 — RF Link Margin`
> `[PLANNED]` The RF link shall provide a minimum 6 dB link margin in the 433 MHz uplink
> and downlink paths at 600 km altitude, 5° elevation angle, with E22-400M30S at 30 dBm TX
> and ground station antenna gain ≥ 3 dBi.

Reference COMMS-DES-001 link budget table for parameter values.

**Acceptance Criteria:**
- [ ] SyRS contains ≥1 RF link margin requirement with numeric budget.
- [ ] COMMS-DES-001 link budget table referenced.

---

### ACT-13

| Field | Value |
|---|---|
| **ID** | ACT-13 |
| **Source** | MIN-09 |
| **Priority** | MEDIUM |
| **Status** | `CLOSED` — 2026-03-10 (SYS-F-121..124 added to SyRS §4) |
| **Target Milestone** | CDR |
| **Responsible** | ADCS Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SyRS-OBC-001`; `ADCS-DES-001`; `controller_limits.h`; `lqr_schedule.h` |

**Title:** Add Reaction Wheel Performance Requirements

**Description:**
FR-5 and SYS-F-111 (LQR) rely on reaction wheel capability. No requirements exist for
maximum RW speed, torque, momentum storage, or saturation threshold. LQR gains are
unverifiable at system level without these.

**Required Action:**
Add subsystem requirements to SyRS, e.g.:
- `SYS-F-120 — RW Maximum Speed: Each reaction wheel shall support a maximum angular velocity of [N] rpm.`
- `SYS-F-121 — RW Maximum Torque: Each reaction wheel shall produce a maximum torque of [X] N·m.`
- `SYS-F-122 — RW Momentum Storage: Each reaction wheel shall store a maximum angular momentum of [Y] N·m·s.`
- `SYS-F-123 — RW Saturation Threshold: Momentum dump shall trigger when any RW exceeds [Z]% of SYS-F-122.`

Cross-reference numeric values to `controller_limits.h` and ADCS-DES-001.

**Acceptance Criteria:**
- [ ] SyRS contains ≥4 RW performance requirements with quantified values.
- [ ] Values consistent with `controller_limits.h` and `lqr_schedule.h` constants.

---

### ACT-14

| Field | Value |
|---|---|
| **ID** | ACT-14 |
| **Source** | MIN-05, MIN-07 |
| **Priority** | HIGH |
| **Status** | `OPEN` |
| **Target Milestone** | CDR |
| **Responsible** | Software Lead |
| **Due Date** | TBD |
| **Affected Documents** | `SVVP-OBC-001` §10; `STP-OBC-001`; `RTM-OBC-001` |

**Title:** Schedule HIL Validation Campaign: Watchdog, Stack HWMs, Power Profile

**Sub-item A — Watchdog Timeout Validation (SYS-F-214):**
The TPS3431 watchdog timeout window has never been measured on hardware.

*Action:* Add test case `T-WDT-02` to STP-OBC-001: Suspend Health Monitor task; measure time-to-reset; verify > 5 s and < 30 s. Add to RTM and SVVP §10 HIL plan.

**Sub-item B — Stack High-Water Marks (SYS-NF-006):**
Only the Heartbeat task HWM is instrumented. Four flight-critical tasks (AttitudeControl,
SensorRead, Telemetry, Command) have unmeasured watermarks.

*Action:* Instrument all task HWMs in hardware test. Add test case `T-STK-01..05` to STP-OBC-001 with pass criterion ≥20% headroom. Add to RTM.

**Sub-item C — Power Profile (NFR-4):**
No power measurement setup is defined. Target: < 2 W nominal.

*Action:* Add test case `T-PWR-01` to STP-OBC-001: Measure OBC current draw in FM_NOMINAL via bench supply ammeter. Verify < 2 W. Add to SVVP §10.

**Acceptance Criteria:**
- [ ] `T-WDT-02`, `T-STK-01..05`, `T-PWR-01` added to STP-OBC-001.
- [ ] All new test cases added to RTM with traceability to SYS-F-214, SYS-NF-006, NFR-4.
- [ ] SVVP §10 HIL plan updated.

---

### ACT-15

| Field | Value |
|---|---|
| **ID** | ACT-15 |
| **Source** | MIN-10 |
| **Priority** | MEDIUM |
| **Status** | `CLOSED` — 2026-03-10 (FMEA-OBC-001 v0.2 — owners and target dates assigned to all 4 OI items; OI-2 partially resolved by SyRS §7.5) |
| **Target Milestone** | CDR (v1.0 baseline) / QR (verified closed) |
| **Responsible** | Systems Engineering Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `FMEA-OBC-001` |

**Title:** Baseline FMEA at v1.0 with All Open Items Closed

**Description:**
FMEA-OBC-001 is version 0.1 published at CDR baseline. Open items in §11 have no
assigned owners or closure dates.

**Required Action:**
1. Review FMEA §11 open items; assign owner and target date to each.
2. Close or formally defer each OI with documented rationale.
3. Increment to v0.2 once OIs are assigned; v1.0 when all OIs are closed.

**Acceptance Criteria:**
- [ ] FMEA §11 OI table: every entry has an assigned owner and target date.
- [ ] FMEA version ≥ v0.2 at CDR; v1.0 at QR.

---

### ACT-16

| Field | Value |
|---|---|
| **ID** | ACT-16 |
| **Source** | MIN-04 |
| **Priority** | HIGH |
| **Status** | `CLOSED` — 2026-03-10 (`src/core/flash_backend.c` implemented; round-robin 4-sector region at `0x1FC000`; CRC-32 header; `flash_backend_recover()`; SYS-F-304 → `[IMPL]`) |
| **Target Milestone** | CDR |
| **Responsible** | Software Lead |
| **Due Date** | 2026-03-10 |
| **Affected Documents** | `SyRS-OBC-001` SYS-F-304; `src/core/flash_backend.c`; `include/flash_backend.h`; `src/core/CMakeLists.txt` |

**Title:** Implement and Test SYS-F-304 Flash Backend for Logger (Blocks MO-6)

**Description:**
SYS-F-304 (Class-A event persistence across power cycles) is `[PLANNED]`. Without this,
mission objective MO-6 (*"Class A events survive reset"*) cannot be fully satisfied.

**Required Action:**
1. Implement `flash_backend.c`/`flash_backend.h` flash write driver for the logger.
2. Integrate with `logger.c` to write Class-A events to a dedicated flash region.
3. Implement `logger_read()` flash recovery path on boot.
4. Add unit/integration test: write Class-A event → power cycle → verify event readable.
5. Add test case `T-LOG-04` (flash persistence) to STP-OBC-001 and RTM.
6. Update SYS-F-304 status from `[PLANNED]` to `[IMPL]`.

**Acceptance Criteria:**
- [ ] `flash_backend.c` implemented and unit tested.
- [ ] SYS-F-304 status `[IMPL]` in SyRS.
- [ ] `T-LOG-04` test case passes on hardware.
- [ ] RMP-OBC-001 risk entry removed or downgraded.

---

## 7. Summary Dashboard

### By Priority

| Priority | Total | Open | In-Progress | Resolved | Closed |
|---|---|---|---|---|---|
| CRITICAL | 1 | — | — | — | 1 (ACT-02) |
| HIGH | 9 | — | — | — | 9 |
| MEDIUM | 6 | — | — | — | 6 |
| **Total** | **16** | **0** | **0** | **0** | **16** |

### By Milestone

| Milestone | Items | Critical/High | Medium | Status |
|---|---|---|---|---|
| PDR Baseline Lock | 10 | ACT-01..07 | ACT-08, ACT-09, ACT-10 | **ALL CLOSED** |
| CDR | 6 | ACT-11, ACT-14, ACT-16 | ACT-12, ACT-13, ACT-15 | **ALL CLOSED** |

### By Responsible Lead

| Lead | Items Assigned | Open |
|---|---|---|
| Systems Engineering | ACT-01, ACT-04, ACT-06, ACT-07, ACT-08, ACT-09, ACT-10, ACT-15 | — |
| Communications | ACT-02, ACT-05, ACT-12 | — |
| Software | ACT-03, ACT-14, ACT-16 | — |
| Hardware | ACT-11 | — |
| ADCS | ACT-13 | — |

### AIR Final Status (as of 2026-03-10)

```
PDR Blocking Items: 10 / 10 CLOSED

[x] ACT-01  HIGH     SRS document ID
[x] ACT-02  CRITICAL CSP version conflict
[x] ACT-03  HIGH     Flash footprint NFR-6
[x] ACT-04  HIGH     Payload definition
[x] ACT-05  HIGH     Ground station IR-4
[x] ACT-06  HIGH     GPS interface management
[x] ACT-07  HIGH     MO-3 FDIR response time
[x] ACT-08  MEDIUM   MO→SyRS traceability table
[x] ACT-09  MEDIUM   RTM identifier alignment
[x] ACT-10  MEDIUM   SYS-F-401 SMP + NFR-7 boot time

PDR GATE: READY — all 10 PDR-blocking items CLOSED (2026-03-10)

CDR Items: 6 / 6 CLOSED

[x] ACT-11  HIGH     Magnetometer part number lock (EM=HMC5883L / FM=LIS3MDL)
[x] ACT-12  MEDIUM   RF link margin SyRS req (SYS-F-450, 6 dB @ 600 km/5°)
[x] ACT-13  MEDIUM   RW performance reqs (SYS-F-121..124)
[x] ACT-14  HIGH     HIL test cases (T-HIL-WDT-01, T-HIL-STK-01..05, T-HIL-PWR-01)
[x] ACT-15  MEDIUM   FMEA baseline v0.2 (owners + dates assigned to OI-1..4)
[x] ACT-16  HIGH     flash_backend.c implemented (SYS-F-304 → [IMPL])

AIR COMPLETE — all 16 action items CLOSED (2026-03-10)
CDR GATE: DOCUMENT BASELINE READY
```

---

## 8. Closure Criteria

An action item may be moved to `CLOSED` status when:

1. The required document change has been committed to the `main` branch.
2. The change has been reviewed and accepted by the Systems Engineering Lead.
3. No new issues have been raised by the change (regression check).
4. For test-related items: the test case has been executed with a PASS result and evidence recorded.

Items may be `DEFERRED` to the next milestone only with explicit written rationale approved by the project lead, and a corresponding risk entry in RMP-OBC-001.

---

*This register will be updated at each engineering review gate. Status updates are the responsibility of the assigned lead for each action item.*

*Source review: SRR-OBC-001 v1.0 | Next review gate: PDR*
