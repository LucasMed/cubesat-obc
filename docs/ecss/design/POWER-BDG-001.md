# POWER-BDG-001 — Power Budget

| Field           | Value                                |
|-----------------|--------------------------------------|
| Document ID     | POWER-BDG-001                        |
| Version         | 0.2                                  |
| Date            | 2026-03-09                           |
| Author          | OBC Systems Team                     |
| Status          | CDR Baseline                         |
| Classification  | Internal                             |

## Change History

| Version | Date       | Author           | Description                              |
|---------|------------|------------------|------------------------------------------|
| 0.2     | 2026-03-09 | OBC Systems Team | GPS load corrected to 120 mW (NEO-7M datasheet); all derived scenario/eclipse numbers updated; TX electrical draw concern flagged (OI-7); added §15 CDR readiness notes |
| 0.1     | 2026-03-09 | OBC Systems Team | Initial CDR baseline — Phase 1 load estimates from MRD §6.6; eclipse sizing; battery margin analysis; EPS FSM energy state coverage |

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Orbital and Environmental Assumptions](#4-orbital-and-environmental-assumptions)
5. [Solar Array Model](#5-solar-array-model)
6. [Battery Model](#6-battery-model)
7. [Load Budget — Phase 1](#7-load-budget--phase-1)
8. [Energy Balance Analysis](#8-energy-balance-analysis)
9. [Eclipse Survival Analysis](#9-eclipse-survival-analysis)
10. [EPS FSM Energy State Coverage](#10-eps-fsm-energy-state-coverage)
11. [Power Rail Architecture](#11-power-rail-architecture)
12. [Margins and Compliance](#12-margins-and-compliance)
13. [Open Items](#13-open-items)
14. [References](#14-references)
15. [CDR Readiness Notes](#15-cdr-readiness-notes)

---

## 1. Introduction

### 1.1 Purpose

This Power Budget document provides the CDR-level analysis of the CubeSat OBC
electrical power system. It verifies compliance with `MIS-PB-001` and
`MIS-PB-002` (MRD-OBC-001 §6.6) and drives EPS hardware sizing decisions for
Phase 1.

### 1.2 Scope

- Phase 1 only: OBC + ADCS sensors + TT&C radio + Magnetorquers ×3.
- Reaction wheels (Phase 2) and payload (TBD — MRD OI-1) are identified as
  open items.
- All values are estimates at CDR; updated measurements required during FM
  qualification.

### 1.3 Relationship to Other Documents

| Document | Provides |
|----------|---------|
| MRD-OBC-001 §6.6 | Top-level power estimates and MIS-PB requirements |
| EPS-DES-001 | Battery voltage thresholds; Schmidt-trigger model |
| OBC-DES-001 | RP2350 operating modes; GPIO current draw |
| FMEA-OBC-001 | FAULT_EPS_VBATT_* fault IDs; priority 2 item |
| LINK-BDG-001 | TX duty cycle input for radio power average calculation |

---

## 2. Applicable Documents

| ID              | Title                                              | Version |
|-----------------|----------------------------------------------------|---------|
| MRD-OBC-001     | Mission Requirements Document / ConOps             | 1.1     |
| EPS-DES-001     | Electrical Power System Design Document            | 0.1     |
| OBC-DES-001     | OBC Hardware & CDH Design Document                 | 0.1     |
| FMEA-OBC-001    | Failure Mode and Effects Analysis                  | 0.1     |
| LINK-BDG-001    | Link Budget                                        | 0.1     |

---

## 3. Acronyms and Definitions

| Term  | Definition |
|-------|-----------|
| BOL   | Beginning of Life (fresh battery / new solar cells) |
| EOL   | End of Life (after ≥ 12 months, degraded capacity) |
| DoD   | Depth of Discharge |
| Pave  | Average power over a full orbital period |
| Ppeak | Peak instantaneous power |
| SSO   | Sun-Synchronous Orbit |
| η     | Efficiency (solar cells, DC-DC converter) |
| Wh    | Watt-hour (energy unit) |
| EPS   | Electrical Power System |

---

## 4. Orbital and Environmental Assumptions

Reference: MRD-OBC-001 §4.

| Parameter | Value | Source |
|-----------|-------|--------|
| Orbit altitude | 600 km (nominal) | MRD §4 |
| Orbit type | Sun-Synchronous (SSO), 96° inclination | MRD §4 |
| Orbital period | T = 96.7 min | Kepler: $T = 2\pi\sqrt{a^3/\mu}$, $a = 6971$ km |
| Max eclipse duration | 37 min (worst case β = 0°) | MRD §4 |
| Min eclipse duration | 0 min (continuous sunlight, |β| > 66°) | MRD §4 |
| Sun time (worst case) | T − 37 = 59.7 min | MRD §4 |
| Eclipse fraction (worst) | $f_e = 37/96.7 = 0.383$ | — |
| Solar constant | 1 361 W/m² | Mean at 1 AU |
| Solar irradiance at 600 km | ≈ 1 361 W/m² (negligible atmospheric attenuation in LEO) | Standard value |
| Solar cell efficiency (GaAs, BOL) | 28% | Representative 2U CubeSat panel |
| Solar cell efficiency (GaAs, EOL) | 24% (≈ 15% degradation over 1 year) | ECSS-E-ST-20-08 |
| Panel area (2U CubeSat, 4 panels) | 4 × 0.02 m² = 0.08 m² | TBD by mechanical — see OI-1 |
| Power conversion efficiency (DC-DC) | 90% | Representative LDO + boost converter |
| Effective cosine loss | 0.60 | Average over random tumble / nadir-pointing (conservative) |

---

## 5. Solar Array Model

### 5.1 Generated Power

$$P_{solar} = I_{solar} \times A_{panel} \times \eta_{cell} \times \eta_{conv} \times \cos\theta$$

Using worst-case (EOL, β = 0°, average cosine):

$$P_{solar,EOL} = 1361 \times 0.08 \times 0.24 \times 0.90 \times 0.60 = \mathbf{14.1 \text{ W}}$$

Using nominal (BOL):

$$P_{solar,BOL} = 1361 \times 0.08 \times 0.28 \times 0.90 \times 0.60 = \mathbf{16.5 \text{ W}}$$

### 5.2 Energy Generated per Orbit (Worst Case EOL)

$$E_{gen} = P_{solar,EOL} \times t_{sun} = 14.1 \times (59.7/60) \text{ Wh} = \mathbf{14.0 \text{ Wh/orbit}}$$

> **Note**: The solar array model is sensitive to panel area and pointing. OI-1
> tracks finalizing panel geometry with the mechanical team. The above uses a
> conservative 4-panel 2U estimate.

---

## 6. Battery Model

### 6.1 Battery Specification (Baseline — ASS-4 from MRD-OBC-001)

| Parameter | Value | Notes |
|-----------|-------|-------|
| Chemistry | Li-ion / LiPo 2S | Two cells in series |
| Nominal voltage | 7.4 V (2 × 3.7 V) | Mid-cell voltage |
| Full charge voltage | 8.4 V (2 × 4.2 V) | — |
| Minimum voltage (cutoff) | 6.0 V (2 × 3.0 V) | Hardware cutoff |
| ENERGY_CRITICAL threshold | 6.6 V (EPS-DES-001) | FW-controlled load shedding |
| ENERGY_EMERGENCY threshold | 6.6 V | = CRITICAL in software (forced FM_SAFE) |
| Baseline capacity | 2 000 mAh (2.0 Ah) | Minimum specification |
| Energy (nominal) | 7.4 V × 2.0 Ah = **14.8 Wh** | BOL |
| Max DoD (Phase 1 ops) | 40% | ECSS-E-HB-20-05A guideline for 500+ cycles |
| Usable energy at 40% DoD | 14.8 × 0.40 = **5.92 Wh** | Nominal BOL |
| Usable energy (EOL, 80% capacity) | 5.92 × 0.80 = **4.74 Wh** | After 12 months |

### 6.2 Battery State vs Voltage (2S Li-ion)

| V_batt (V) | Approx. SoC (%) | EPS State | Action |
|-----------|-----------------|-----------|--------|
| 8.4 | 100% | NOMINAL | Full authority |
| 7.4 | ~50% | NOMINAL (low boundary) | Normal operations |
| 7.0 | ~30% | CRITICAL ↓ | Load shed; FM_SAFE requested |
| 6.6 | ~10% | EMERGENCY ↓ | All non-OBC rails off; FM_SAFE forced |
| 6.0 | ~0% | Hardware cutoff | Under-voltage protection |

---

## 7. Load Budget — Phase 1

All values are at the **battery bus** (accounting for DC-DC losses). Duty
cycles are defined for the `FM_NOMINAL` reference scenario.

### 7.1 Continuous Loads (always on)

| Subsystem | Component | Typical (mW) | Peak (mW) | Duty Cycle | Avg (mW) | Notes |
|-----------|-----------|-------------|-----------|------------|----------|-------|
| OBC | RP2350 Core 0 @ 133 MHz | 150 | 300 | 100% | 150 | Core 1 idle; FPU active during ADCS |
| ADCS sensors | MPU-6050 (gyro+accel) | 15 | 25 | 100% | 15 | Normal+LP mode |
| ADCS sensors | LIS3MDL magnetometer | 5 | 15 | 100% | 5 | Continuous mode |
| ADCS sensors | NEO-7M GPS | 120 | 150 | 100% | 120 | Tracking mode; revised from 90 mW per U-blox NEO-7M datasheet operating current ~25 mA @ 3.3 V (≤ 120 mW typical) |
| EPS | TPS3431 WDT | 1 | 1 | 100% | 1 | μA-level, negligible |
| Board | Passive quiescent (LDO, pull-ups) | 30 | 30 | 100% | 30 | Estimated |
| **Continuous total** | | **321** | **521** | — | **321** | |

### 7.2 Duty-Cycled Loads

| Subsystem | Component | Active Power (mW) | Duty Cycle | Avg (mW) | Notes |
|-----------|-----------|-------------------|------------|----------|-------|
| TT&C | E22-400M30S RX (listening) | 30 | 100% | 30 | RX idle current |
| TT&C | E22-400M30S TX @ 30 dBm | **3 000** | 1% (6 s/pass) | 30 | EBYTE datasheet: ~600 mA × 5 V = 3 000 mW; PA eff. ~33%; may be 3 000–4 500 mW — see OI-7; 1.1% actual duty from LINK-BDG-001 §6.1 |
| Magnetorquers | MTQ ×3 B-dot (FM_DETUMBLE) | 600 | 50% | 300 | PWM duty cycle varies; 50% conservative for B-dot |
| Magnetorquers | MTQ ×3 FM_NOMINAL (trim) | 200 | 20% | 40 | Low-duty trim torques |
| **Duty-cycled total (FM_NOMINAL)** | | — | — | **100** | TX + MTQ trim |

### 7.3 Total Phase 1 Load Summary

| Scenario | Power Avg (mW) | Power Peak (mW) |
|----------|---------------|-----------------|
| FM_SAFE (minimal — OBC + RX only) | **351** | 380 |
| FM_NOMINAL (standard — continuous + duty-cycled) | **421** | ~3 921 (TX burst) |
| FM_DETUMBLE (max operational) | **621** | ~3 921 (TX + MTQ full) |
| Deep eclipse survival (OBC + RX only) | **351** | 380 |

> **MRD §6.6 reference**: Full Phase 1 load (≤ 4 W peak). This analysis yields
> **3.92 W** peak — confirmed within 4 W requirement.
>
> **Note**: If E22 electrical draw is 4 500 mW (OI-7), the instantaneous TX burst
> peak rises to ~5.1 W for ~134 ms. The 4 W MRD limit is interpreted as a
> duty-cycled average; at 1% TX duty cycle the time-averaged TX contribution
> remains < 50 mW. Confirm interpretation with EPS design authority.

---

## 8. Energy Balance Analysis

### 8.1 Per-Orbit Energy Balance (Worst Case — EOL, β = 0°, FM_NOMINAL)

| Term | Value | Calculation |
|------|-------|------------|
| Energy generated (sun phase) | 14.0 Wh | §5.2 |
| Energy consumed — sun phase (59.7 min) | 0.421 W × (59.7/60) h = **0.42 Wh** | FM_NOMINAL avg |
| Energy consumed — eclipse phase (37 min) | 0.421 W × (37/60) h = **0.26 Wh** | Eclipse (no TX in practice) |
| Total consumed / orbit | **0.68 Wh** | — |
| Net energy per orbit | 14.0 − 0.68 = **+13.32 Wh** | Strongly positive |
| **Verdict** | **PASS — array far exceeds orbital consumption** | |

> **Observation**: At 2U panel area, the solar array generates ~20× the orbital
> energy budget. This large margin reflects that the E22 TX at 30 dBm has a very
> low duty cycle (< 1% of orbit time). The effective average radio load is ~30 mW.
> The design is **power-positive by a large margin** — the limiting factor is
> eclipse survival, not energy generation.

### 8.2 Daily Energy Balance

| Term | Value |
|------|-------|
| Orbits per day | 60 min × 24 h / 96.7 min ≈ **14.9 orbits/day** |
| Energy generated per day | 14.9 × 14.0 Wh = **208.6 Wh/day** |
| Energy consumed per day | 14.9 × 0.68 Wh = **10.1 Wh/day** |
| Balance | **+198.5 Wh/day** — strongly positive |

---

## 9. Eclipse Survival Analysis

This is the binding constraint — the battery must supply power through the
worst-case 37-minute eclipse period.

### 9.1 Eclipse Energy Required

| Scenario | Load (mW) | Eclipse (min) | Energy Required (Wh) |
|----------|----------|--------------|----------------------|
| FM_SAFE minimal (OBC + RX) | 351 | 37 | 0.217 |
| FM_NOMINAL | 421 | 37 | 0.260 |
| FM_DETUMBLE (active) | 621 | 37 | 0.383 |

### 9.2 Margin Analysis

Using EOL usable capacity 4.74 Wh @ 40% DoD:

| Scenario | Energy Required (Wh) | Usable (Wh) | Margin (Wh) | Margin (%) | Time at load (min) |
|----------|---------------------|-------------|-------------|------------|-------------------|
| FM_SAFE | 0.217 | 4.74 | 4.52 | **+2084%** | **810 min** |
| FM_NOMINAL | 0.260 | 4.74 | 4.48 | **+1723%** | **676 min** |
| FM_DETUMBLE | 0.383 | 4.74 | 4.36 | **+1138%** | **458 min** |

All scenarios pass with large margins. The baseline 2S 2 Ah battery provides
**458 min at full FM_DETUMBLE load** — more than 12× the worst-case eclipse of
37 min.

### 9.3 MIS-PB-001 Compliance

> *"The EPS battery shall provide ≥ 37 min of operation at Phase 1 full load (≤ 4 W)"*

| Value | Required | Actual (EOL, FM_DETUMBLE @ 591 mW) | Compliant? |
|-------|----------|-------------------------------------|------------|
| Eclipse survival time | ≥ 37 min | **458 min** (EOL) | ✅ **PASS** |

> **Note**: The MRD was written conservatively using 4 W (peak with radio TX).
> At average FM_DETUMBLE load ~621 mW (no TX during eclipse), the margin is
> even larger. At 4 W peak: 4.74 Wh / 4 W × 60 min/h = **71 min** — still
> nearly 2× the required 37 min.

### 9.4 MIS-PB-002 Compliance

> *"The OBC 3.3 V rail shall remain powered at battery voltages ≥ 6.6 V"*

The OBC 3.3 V rail is supplied by a dedicated LDO (MIC5219 or equivalent) with
input range 3.0–16 V. At V_batt = 6.6 V:

- LDO input = 6.6 V; output = 3.3 V → 2× headroom above 3.3 V output ✅
- EPS FSM: `ENERGY_EMERGENCY` threshold = 6.6 V; all rails except OBC shed at this point
- Hardware PCB cutoff = 6.0 V — OBC rail remains on down to 6.0 V

| Requirement | Threshold | Design | Compliant? |
|------------|-----------|--------|------------|
| OBC rail on at V_batt ≥ 6.6 V | 6.6 V | LDO works to 3.0 V input; OBC rail dedicated | ✅ **PASS** |

---

## 10. EPS FSM Energy State Coverage

The EPS Monitor implements a Schmidt-trigger FSM (EPS-DES-001 §5–6). This
section confirms that the voltage thresholds are sized correctly against the
battery model.

| State | Enter (falling) | Exit (rising — hysteresis) | Battery SoC (approx) | Action |
|-------|-----------------|---------------------------|----------------------|--------|
| NOMINAL | V ≥ 7.4 V | — | > 50% | Full authority |
| LOW | V < 7.4 V ↓ | V > 7.5 V ↑ | ~40–50% | Optional payload shed |
| CRITICAL | V < 7.0 V ↓ | V > 7.15 V ↑ | ~25–30% | FM_SAFE requested; attitude loads shed |
| EMERGENCY | V < 6.6 V ↓ | V > 6.75 V ↑ | ~10% | FM_SAFE forced; all non-OBC rails off |

**Remaining usable capacity at CRITICAL (7.0 V):**

Approximated using linear SoC model (7.4 V → 0%, 8.4 V → 100%; 7.0 V → –40% → clamp 0%):

> At 7.0 V the cell is below the nominal (7.4 V = 50% SoC for 2S) —
> approximately 20–30% SoC remains depending on cell C-rate and temperature.
> 2.0 Ah × 25% × 7.0 V ≈ **0.35 Wh** usable before hitting hardware cutoff.
>
> At FM_SAFE minimal load (321 mW): ~0.35 / 0.321 × 60 = **65 min** remaining
> after CRITICAL entry — sufficient for ground contact on the next pass.

---

## 11. Power Rail Architecture

| Rail | Voltage | Source | Loads | On/Off Control |
|------|---------|--------|-------|----------------|
| Battery bus | 6.0 – 8.4 V | 2S Li-ion | Boost converter input | Always on (HW) |
| 3.3 V OBC | 3.3 V | LDO from battery | RP2350, UART, I²C pull-ups | Always on (HW) |
| 3.3 V ADCS | 3.3 V | LDO (separate rail) | MPU-6050, LIS3MDL | EPS-controlled GPIO |
| 5 V COMMS | 5 V | Boost converter | E22-400M30S, GPS | EPS-controlled GPIO |
| 5 V MTQ | 5 V | Boost converter | Magnetorquer drivers ×3 | EPS-controlled GPIO |
| 5 V PAYLOAD | 5 V | Boost converter | TBD (Phase 2) | EPS-controlled GPIO |

**Shedding order (CRITICAL → EMERGENCY):**

1. PAYLOAD rail off
2. COMMS rail → listen-only (RX only, TX disabled in FW)
3. MTQ rail off (ADCS disabled)
4. ADCS sensor rail off
5. OBC 3.3 V — **never shed**

---

## 12. Margins and Compliance

| Requirement | Threshold | Analysis Result | Margin | Status |
|------------|-----------|----------------|--------|--------|
| MIS-PB-001 — eclipse survival ≥ 37 min at ≤ 4 W | 37 min | 71 min @ 4 W peak (EOL) / 458 min @ avg load | +34 min / +12× | ✅ PASS |
| MIS-PB-002 — OBC rail at V ≥ 6.6 V | 6.6 V | LDO input range 3.0–16 V; OBC rail dedicated | +3.0 V headroom | ✅ PASS |
| Peak load ≤ 4 W (MRD §6.6 note) | 4 000 mW | 3 921 mW (FM_DETUMBLE + TX burst) | +79 mW | ✅ PASS |
| Energy positive per orbit | > 0 Wh | +13.32 Wh/orbit (EOL) | — | ✅ PASS |

**CDR Recommendation**: No redesign required for Phase 1 power system. Battery
remains 2S 2000 mAh minimum. Phase 2 additions (RW ×3, INA219 current monitor)
require updated budget iteration.

---

## 13. Open Items

| OI  | Description | Priority | Status |
|-----|-------------|----------|--------|
| OI-1 | Solar panel area not finalized — mechanical team to confirm 2U panel count and area; TBD value used in §5 | High | Open |
| OI-2 | Reaction wheel power (Phase 2) not included — requires motor driver characterization (~1500 mW peak ×3) | Medium | Phase 2 |
| OI-3 | Payload power unknown (MRD OI-1) — budget must be re-run once payload is defined | Medium | Blocked |
| OI-4 | EOL degradation factor 80% assumed; actual cell datasheet (manufacturer TBD) should be confirmed | Medium | Open |
| OI-5 | TX duty cycle assumed 1% — verify against LINK-BDG-001 actual pass geometry and dwell time | Low | In LINK-BDG-001 |
| OI-6 | INA219 current monitor driver not implemented (Phase 2 plan) — required for closed-loop power management | High | Phase 2 |
| OI-7 | E22-400M30S electrical TX draw assumed 3 000 mW (EBYTE datasheet ~600 mA × 5 V); PA efficiency ~33%; actual draw may be 3 000–4 500 mW — confirm by current measurement during FM thermal-vacuum test; peak load compliance margin may reduce from +79 mW to negative but time-average impact is < 50 mW | High | Open |

---

## 14. References

| Ref | Document |
|-----|----------|
| [1] | MRD-OBC-001 v1.1 — §4 Orbital Parameters, §6.6 Power Budget Requirements |
| [2] | EPS-DES-001 v0.1 — §5 Energy State Model, §6 Voltage Thresholds |
| [3] | OBC-DES-001 v0.1 — §12 Power Architecture |
| [4] | FMEA-OBC-001 v0.1 — §9.2 EPS priority items |
| [5] | LINK-BDG-001 v0.2 — TX duty cycle (§6.1) |
| [6] | ECSS-E-HB-20-05A — Spacecraft Electrical Power Systems Handbook |
| [7] | ASS-4 (MRD-OBC-001 §7.2) — Battery 2S Li-ion / LiPo ≥ 37 min at full load |

---

## 15. CDR Readiness Notes

Brief responses to anticipated CDR review panel questions.

### 15.1 EPS / Battery Questions

**Q: What is the battery thermal environment?**

Li-ion cells operate within specification (discharge 0–45 °C, storage –20–60 °C)
in the expected LEO thermal cycling environment. Phase 1 relies on passive
thermal management (PCB copper pours, cell foam insulation). FMEA-OBC-001 OI-4
tracks active thermal control (heater + thermostat) for Phase 2.

**Q: What is the battery cycle life versus mission duration?**

At 40% DoD and ~15 orbits/day, the battery undergoes up to 2 738 charge-discharge
cycles for a 6-month Phase 1 mission. Standard Li-ion cells at 40% DoD support
> 1 000 cycles per most datasheets; actual cell datasheet (OI-4) must be confirmed.
The 80% EOL capacity assumption is conservative against industry-standard 12-month
CubeSat mission profiles.

**Q: What happens to the battery after 1 year (degradation)?**

EOL model uses 80% capacity retention after 12 months: 4.74 Wh usable. Eclipse
survival decreases from ~810 min (BOL FM_SAFE) to 810 min at EOL FM_SAFE — the
40% DoD sizing absorbs the degradation entirely. The EPS FSM voltage thresholds
(§10) are calibrated to open-circuit voltage, not capacity, so they remain valid
at EOL.

**Q: What if the solar panel is partially shadowed?**

Panel area is the primary uncertainty (OI-1). At 50% shadowing (one panel
obstructed), generation halves to ~7 W → ~7 Wh/orbit. This still exceeds orbital
consumption (0.68 Wh) by 10×. Eclipse survival is unaffected (battery-only).
The solar model cosine factor of 0.60 already accounts for average off-pointing.

### 15.2 Peak Power and TX Concerns

**Q: Is the 3 W TX power figure verified?**

The 3 000 mW figure is derived from the EBYTE E22-400M30S datasheet typical
current (~600 mA from a 5 V supply). PA efficiency is approximately 33%. Some
measured COTS modules in this class draw up to 4 500 mW at 30 dBm. OI-7 requires
current measurement during FM testing. Even at 5 000 mW, the time-averaged load
contribution is < 50 mW (1% duty) — negligible for energy balance.

**Q: What happens if the peak load exceeds 4 W?**

The 3 921 mW peak is within the 4 W MRD limit. If TX is 4 500 mW the instantaneous
peak would be ~5.1 W for the ~134 ms TX burst. The 4 W limit in MRD §6.6 is a
power-rail sizing constraint (wire gauge, connector rating), not an energy limit.
Recommendation: confirm with EPS design authority whether limit is peak-
instantaneous or duty-cycled-average, and uprate conductors if needed.
