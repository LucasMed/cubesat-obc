# Phase 8: Full Component Testing & Pending Tasks — Plan

**Document ID**: PLAN-008  
**Version**: 0.1  
**Last Updated**: 2026-03-17  
**Branch**: `feature/phase8-full-testing`  
**Status**: Planning  
**Depends on**: Phase 7 complete (`feature/phase7-payload` merged to `dev`)

---

## Overview

Phase 8 has two primary objectives:

1. **Maximize Test Coverage**: Achieve the highest possible test coverage across all
   components of the CubeSat OBC flight software, targeting >95% line and function coverage.

2. **Address Pending Tasks**: Complete remaining items from previous phases and prepare
   the system for hardware deployment, including camera integration, external storage,
   and unresolved driver issues.

---

## Objectives

### 1. Comprehensive Test Coverage
- **Unit Tests**: Write tests for all uncovered functions across all modules
- **Integration Tests**: Validate proper interaction between GPS, IMU, and Magnetometer
  drivers with the EKF and telemetry systems
- **Mock Tests**: Implement mocked test suites for hardware dependencies unavailable
  in the host build environment
- **Edge Cases**: Cover input validation, error states, and invalid hardware responses

### 2. Fix and Reactivate Disabled Tests
- **Radiation Test**: Investigate and resolve unsupported APIs in
  `test_radiation_integration.c`
- **API Completion**: Either fix missing dependencies or document necessary future work

### 3. Performance and Stress Testing
- **Stress Tests**: Evaluate system performance under significant load conditions
- **Memory Profiling**: Monitor heap and stack usage during extended operation

### 4. Hardware Testing Preparation
- **Hardware Validation Protocols**: Define clear steps for post-software deployment
  hardware testing
- **Expected Results**: Document expected outcomes for GPS accuracy, IMU orientation
  stability, and magnetometer field measurements

### 5. Coverage Metrics
- Use `gcov`/`lcov` tools to track progress toward >95% line and function coverage
- Capture and report detailed coverage metrics for each CI stage

---

## Pending Tasks from Previous Phases

| Task | Phase | Description | Status |
|------|-------|-------------|--------|
| Camera Driver (OV2640) | 7 | SPI camera interface implementation | Deferred |
| External Storage (SD/W25Q) | 7 | ≥1 GB non-volatile storage for payload data | Deferred |
| FM_PAYLOAD State | 7 | New flight mode for payload operations | Deferred |
| Radiation Driver Test | 7 | Integration test disabled due to unsupported APIs | Deferred |
| Hardware Validation | 7 | Live GPS, IMU, Magnetometer testing on Pico 2W | Pending |
| PAYLOAD-SPEC-001 Compliance | 7 | §13 compliance matrix verification | Pending |
| RTM-OBC-001 Update | 7 | Traceability for FR-13..19, PLD-R-001..005 | Pending |

---

## Work Packages

### WP-8.1 — Test Coverage Expansion

**Objective**: Maximize test coverage across all software modules.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-8.1.1 | Audit current coverage: run `gcovr` and identify uncovered functions | SW | 2 h | ⏳ |
| T-8.1.2 | Write unit tests for uncovered driver functions | SW | 8 h | ⏳ |
| T-8.1.3 | Write unit tests for uncovered service layer functions | SW | 6 h | ⏳ |
| T-8.1.4 | Write unit tests for uncovered task functions | SW | 4 h | ⏳ |
| T-8.1.5 | Add edge case tests for GPS NMEA parser (invalid checksum, malformed sentences) | SW | 2 h | ⏳ |
| T-8.1.6 | Add edge case tests for IMU data validation | SW | 2 h | ⏳ |
| T-8.1.7 | Add edge case tests for magnetometer EKF updates (low field, saturation) | SW | 2 h | ⏳ |
| T-8.1.8 | Target: >95% line coverage, >95% function coverage | SW | — | ⏳ |

**Exit criteria**: Coverage report shows >95% line and function coverage.

---

### WP-8.2 — Fix Radiation Test

**Objective**: Re-enable the radiation detector integration test.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-8.2.1 | Investigate unsupported API calls in `test_radiation_integration.c` | SW | 2 h | ⏳ |
| T-8.2.2 | Implement missing stubs or mock functions | SW | 4 h | ⏳ |
| T-8.2.3 | Re-enable test in CI pipeline | SW | 1 h | ⏳ |
| T-8.2.4 | Verify all radiation driver tests pass | SW | 1 h | ⏳ |

**Exit criteria**: `test_radiation_integration.c` passes on host build and CI.

---

### WP-8.3 — Camera Driver Implementation

**Objective**: Implement SPI camera interface for OV2640/IMX219.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-8.3.1 | Finalize camera module selection (OV2640 or IMX219 SPI) | HW | 1 week | ⏳ |
| T-8.3.2 | Implement `camera_driver.c`: SPI0 shared bus, trigger, readout | SW | 4 h | ⏳ |
| T-8.3.3 | Unit tests for camera driver | SW | 2 h | ⏳ |
| T-8.3.4 | Hardware validation: capture JPEG, verify size 50–500 KB | HW | 3 h | ⏳ |

**Exit criteria**: JPEG image captured and stored on hardware.

---

### WP-8.4 — External Storage Integration

**Objective**: Add ≥1 GB non-volatile storage for payload science data.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-8.4.1 | Select storage solution (microSD or W25Q128 SPI flash) | HW/SW | 2 h | ⏳ |
| T-8.4.2 | Implement storage driver (FATFS or SPI flash) | SW | 8 h | ⏳ |
| T-8.4.3 | Integrate with `payload_manager.c`: store mag/rad/image data | SW | 3 h | ⏳ |
| T-8.4.4 | Unit tests: write/read round-trip, storage-full handling | SW | 2 h | ⏳ |
| T-8.4.5 | Hardware validation: write 1 MB, read back without error | HW | 2 h | ⏳ |

**Exit criteria**: 1 MB payload data written and read back correctly.

---

### WP-8.5 — FM_PAYLOAD Implementation

**Objective**: Add payload flight mode to FMM and integrate with payload manager.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-8.5.1 | Implement `payload_manager.c`: enable/disable PAYLOAD rail | SW | 2 h | ⏳ |
| T-8.5.2 | Implement `payload_task.c`: 10 Hz MAG, 1 Hz RAD, CAM on notification | SW | 4 h | ⏳ |
| T-8.5.3 | Update FMM with FM_PAYLOAD transitions | SW | 1 h | ⏳ |
| T-8.5.4 | Integration test: FM_NOMINAL → FM_PAYLOAD → FM_NOMINAL | SW | 2 h | ⏳ |
| T-8.5.5 | Update telemetry: payload HK packet | SW | 1 h | ⏳ |

**Exit criteria**: FM_PAYLOAD state works, payload HK visible in telemetry.

---

### WP-8.6 — Hardware Validation Protocol

**Objective**: Define and document hardware testing procedures.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-8.6.1 | Document GPS hardware test procedure (connection, cold-start, NMEA monitoring) | DOCS | 1 h | ⏳ |
| T-8.6.2 | Document IMU hardware test procedure (data validation, orientation) | DOCS | 1 h | ⏳ |
| T-8.6.3 | Document magnetometer hardware test procedure (field measurement) | DOCS | 1 h | ⏳ |
| T-8.6.4 | Execute hardware validation: GPS fix ≤5 min | HW | 3 h | ⏳ |
| T-8.6.5 | Execute hardware validation: IMU data stable | HW | 1 h | ⏳ |
| T-8.6.6 | Execute hardware validation: Mag field within Earth-range | HW | 1 h | ⏳ |

**Exit criteria**: All hardware tests pass with expected results.

---

### WP-8.7 — Documentation Updates

**Objective**: Finalize documentation for Phase 8 and update traceability.

| Task | Description | Owner | Estimate | Status |
|------|-------------|-------|----------|--------|
| T-8.7.1 | Update RTM-OBC-001: trace FR-13..19, PLD-R-001..005 | DOCS | 1 h | ⏳ |
| T-8.7.2 | Update PAYLOAD-SPEC-001 §13 compliance matrix | DOCS | 1 h | ⏳ |
| T-8.7.3 | Update CHANGELOG.md with Phase 8 entry | DOCS | 15 min | ⏳ |
| T-8.7.4 | Update PROJECT_PROGRESS.md | DOCS | 15 min | ⏳ |

**Exit criteria**: All documentation updated and consistent.

---

## Schedule

```
2026-03-17  Phase 8 planning complete
2026-03-24  WP-8.1 start (coverage expansion)
2026-03-31  WP-8.2 start (radiation test fix)
2026-04-07  WP-8.3 start (camera driver)
2026-04-14  WP-8.4 start (external storage)
2026-04-21  WP-8.5 start (FM_PAYLOAD)
2026-04-28  WP-8.6 start (hardware validation)
2026-05-05  WP-8.7 docs closure
2026-05-12  Phase 8 PR review; merge to dev
```

Total calendar time: ~8 weeks.

---

## Definition of Done

Phase 8 is **complete** when:

- [ ] Line coverage >95%
- [ ] Function coverage >95%
- [ ] All disabled tests re-enabled and passing
- [ ] Camera driver implemented and tested
- [ ] External storage implemented and tested
- [ ] FM_PAYLOAD state fully operational
- [ ] Hardware validation complete for GPS, IMU, Magnetometer
- [ ] Documentation updated (RTM, PAYLOAD-SPEC, CHANGELOG, PROJECT_PROGRESS)
- [ ] CI pipeline 6/6 stages green
- [ ] Phase 8 PR reviewed and merged to `dev`

---

## Risk Register

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Coverage target >95% not achievable | Low | Medium | Prioritize uncovered safety-critical functions; document exceptions |
| Camera module unavailable | Medium | High | Use OV2640 fallback; document swap procedure |
| External storage SPI conflicts with camera | Low | High | Use separate SPI buses; test with oscilloscope |
| Hardware validation delays (weather/location) | Medium | Medium | Schedule clear-sky window for GPS; lab-based IMU/mag tests |

---

## Future Considerations

- **Phase 9**: Advanced features (reaction wheel control, B-dot detumbling, image processing)
- **Radiation Hardening**: Future work for space-grade components
- **HIL Testing**: Hardware-in-the-loop validation for complete system testing
