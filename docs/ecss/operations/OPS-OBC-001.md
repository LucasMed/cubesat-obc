# Operations Manual

**Document ID**: OPS-OBC-001  
**Version**: 1.0  
**Last Updated**: 2026-03-21  
**Status**: Active  
**Standard**: Based on ECSS-E-ST-10-06C

---

## Change History

| Version | Date       | Author            | Description                          |
|---------|------------|-------------------|--------------------------------------|
| 1.0     | 2026-03-21 | OBC Systems Team  | Initial release                     |

---

## 1. Purpose and Scope

### 1.1 Purpose

This document defines the flight operations procedures for the CubeSat On-Board Computer (OBC) flight software. It provides ground operators with comprehensive guidance for commanding, monitoring, and controlling the OBC during all phases of mission execution.

### 1.2 Scope

This Operations Manual applies to all flight operations of the CubeSat OBC, including:
- Nominal mission operations
- Commissioning and checkout
- Anomaly detection and recovery
- Payload operations

The procedures defined herein cover all flight modes, command execution, health monitoring, and anomaly handling for the OBC subsystem.

### 1.3 Applicable Documents

| Document ID | Title                              |
|-------------|------------------------------------|
| SRS-OBC-001 | Software Requirements Specification |
| ICD-OBC-001 | OBC Interface Control Document     |
| FSW-SDD-001 | Software Design Description       |
| FMEA-OBC-001 | Failure Mode Effects Analysis     |

---

## 2. Operational Overview

### 2.1 System Description

The CubeSat OBC is based on the Raspberry Pi RP2350 microcontroller and provides the following core functions:
- Attitude determination and control (via EKF and LQR/PID)
- Sensor data acquisition (IMU, magnetometer, GPS)
- Telemetry and telecommand via CSP over LoRa
- Non-volatile data storage
- Payload management (camera, scientific magnetometer, radiation detector)

### 2.2 Flight Mode Summary

| Mode          | Description                                                |
|---------------|------------------------------------------------------------|
| FM_BOOT       | Initial boot state, hardware initialization               |
| FM_SAFE       | Safe mode with reduced functionality, actuators disabled  |
| FM_DETUMBLE   | Detumble mode using B-dot law and momentum dumping        |
| FM_NOMINAL    | Nominal operations with full ADCS control                 |
| FM_DIAGNOSTIC | Diagnostic mode for testing and tuning                   |
| FM_PAYLOAD    | Payload operations (camera, MAG-001, RAD-001)             |

### 2.3 Operational Timeline

1. **Launch → Deployment**: FM_BOOT (transitional)
2. **Post-Deployment**: FM_SAFE → FM_DETUMBLE → FM_NOMINAL
3. **Nominal Operations**: FM_NOMINAL with periodic FM_PAYLOAD windows
4. **Anomaly**: Automatic or commanded transition to FM_SAFE

---

## 3. Command Dictionary

### 3.1 Command Framework

Commands are transmitted via CSP (CubeSat Space Protocol) on port 20. All commands follow the packet format defined in ICD-OBC-001.

### 3.2 Command List

| Command ID | Name                  | Class | Parameters                          | Description                                              |
|------------|-----------------------|-------|-------------------------------------|----------------------------------------------------------|
| CMD_ECHO   | Echo Test             | A     | None                                | Returns received packet for link verification           |
| CMD_REBOOT | System Reboot         | C     | None                                | Reboots the OBC, enters FM_BOOT then transitions to FM_SAFE |
| CMD_SET_MODE | Set Flight Mode     | C     | `mode` (uint8): 0=BOOT, 1=SAFE, 2=DETUMBLE, 3=NOMINAL, 4=DIAGNOSTIC, 5=PAYLOAD | Transitions to specified flight mode |
| CMD_PAYLOAD_CAPTURE | Payload Capture | B     | `type` (uint8): 0=Camera, 1=MAG sample, 2=RAD sample | Initiates payload data acquisition in FM_PAYLOAD mode |

### 3.3 Command Class Definitions

| Class | Description                                    | Authorization Level      |
|-------|------------------------------------------------|-------------------------|
| A     | Public commands, available in all modes       | Any                     |
| B     | Operational commands, available in safe/nominal | Ground Station         |
| C     | Privileged commands, mode control             | Mission Operator        |

---

## 4. Flight Mode Operations

### 4.1 FM_BOOT (Boot Mode)

#### 4.1.1 Description

FM_BOOT is the initial state entered upon power-on or reboot. In this mode, the OBC performs hardware initialization, loads firmware, and prepares for transition to FM_SAFE.

#### 4.1.2 Entry Conditions
- Power-on reset
- Watchdog reset
- Software reboot command (CMD_REBOOT)

#### 4.1.3 Exit Conditions
- Automatic transition to FM_SAFE after initialization complete

#### 4.1.4 Behavior
- Hardware peripheral initialization (I2C, SPI, UART, GPIO)
- Sensor driver initialization
- Data layer initialization
- Task scheduler startup
- Memory initialization and sanity checks

#### 4.1.5 Constraints
- No actuator commands issued
- No payload operations
- Telemetry limited to boot progress messages

#### 4.1.6 Transition Time
- Maximum 10 seconds from power-on to FM_SAFE entry

---

### 4.2 FM_SAFE (Safe Mode)

#### 4.2.1 Description

FM_SAFE is the degraded but stable operational mode. All actuators are disabled to prevent uncontrolled torques. The OBC maintains health monitoring and telemetry while awaiting ground commands.

#### 4.2.2 Entry Conditions
- Automatic from FM_BOOT
- Automatic on CRITICAL fault detection
- Commanded via CMD_SET_MODE

#### 4.2.3 Exit Conditions
- Commanded transition to FM_NOMINAL, FM_DETUMBLE, or FM_DIAGNOSTIC

#### 4.2.4 Behavior
- Actuator outputs disabled (reaction wheels, magnetorquers)
- Health monitoring active
- Telemetry transmission active (housekeeping data)
- Command processing enabled (limited to safe-mode commands)
- EKF maintains last valid state estimate

#### 4.2.5 Constraints
- No attitude control outputs
- No momentum dumping
- Limited to Class A and Class C commands

#### 4.2.6 Telemetry
- Bus voltage and current
- Temperature readings
- Flight mode state
- Fault status
- Memory usage

---

### 4.3 FM_DETUMBLE (Detumble Mode)

#### 4.3.1 Description

FM_DETUMBLE uses the B-dot control law to reduce spacecraft angular rates. This mode is entered when angular rates exceed safe limits for FM_NOMINAL operation.

#### 4.3.2 Entry Conditions
- Automatic when angular rates exceed DETUMBLE_RATE_THRESHOLD (5 deg/s)
- Commanded via CMD_SET_MODE

#### 4.3.3 Exit Conditions
- Automatic when angular rates fall below DETUMBLE_RATE_THRESHOLD
- Commanded via CMD_SET_MODE to FM_NOMINAL

#### 4.3.4 Behavior
- B-dot control law active
- Magnetic dipole commands generated for detumbling
- Reaction wheel momentum dumped via magnetorquers
- High-bandwidth LQR gains applied (ωn = 30 rad/s)
- Health monitoring active

#### 4.3.5 Constraints
- No reaction wheel torque commands
- No payload operations

#### 4.3.6 Transition Criteria
- Auto-return to FM_NOMINAL: angular rates < 5 deg/s for 10 seconds
- Duration limit: 30 minutes max before forced FM_SAFE

---

### 4.4 FM_NOMINAL (Nominal Mode)

#### 4.4.1 Description

FM_NOMINAL is the primary operational mode for the mission. Full attitude control is active using LQR (with EKF) or PID fallback.

#### 4.4.2 Entry Conditions
- Commanded via CMD_SET_MODE
- Automatic from FM_DETUMBLE when rates normalized

#### 4.4.3 Exit Conditions
- Commanded transition to FM_PAYLOAD, FM_DIAGNOSTIC, or FM_SAFE
- Automatic on CRITICAL fault detection
- Automatic to FM_DETUMBLE if rates exceed threshold

#### 4.4.4 Behavior
- LQR attitude control with EKF state estimation
- PID fallback when EKF invalid or converging
- Reaction wheel torque commands active
- Magnetorquer desaturation on momentum threshold
- Full telemetry transmission
- GPS navigation data acquisition

#### 4.4.5 Constraints
- Payload power rail disabled by default

#### 4.4.6 Control Parameters
- LQR natural frequency: ωn = 10 rad/s
- EKF update rate: 10 Hz
- Control loop rate: 20 Hz

---

### 4.5 FM_DIAGNOSTIC (Diagnostic Mode)

#### 4.5.1 Description

FM_DIAGNOSTIC provides a test environment for hardware checkout, algorithm tuning, and troubleshooting. PID control is used regardless of EKF state.

#### 4.5.2 Entry Conditions
- Commanded via CMD_SET_MODE

#### 4.5.3 Exit Conditions
- Commanded via CMD_SET_MODE to any other mode

#### 4.5.4 Behavior
- PID controller active (bypasses LQR)
- EKF continues but not used for control
- Extended diagnostic telemetry
- Sensor raw data output enabled
- Actuation commands active

#### 4.5.5 Constraints
- Not for nominal mission operations

#### 4.5.6 Telemetry
- Standard housekeeping plus:
- Raw sensor data (IMU, magnetometer)
- Controller intermediate values
- Task execution timing

---

### 4.6 FM_PAYLOAD (Payload Mode)

#### 4.6.1 Description

FM_PAYLOAD activates the scientific payload suite including the camera (CAM-001), scientific magnetometer (MAG-001), and radiation detector (RAD-001).

#### 4.6.2 Entry Conditions
- Commanded via CMD_SET_MODE
- Must be in FM_NOMINAL first (implicit transition)

#### 4.6.3 Exit Conditions
- Commanded via CMD_SET_MODE to FM_NOMINAL
- Automatic on CRITICAL fault detection

#### 4.6.4 Behavior
- 5V payload power rail enabled (GPIO21)
- Camera image capture (IMX219 via SPI1)
- MAG-001 sampling (RM3100 via I2C0) at ≥10 Hz
- RAD-001 dose accumulation (PIN diode via ADC1) at ≥1 Hz
- GPS position/time acquisition continues
- Data stored to non-volatile storage

#### 4.6.5 Constraints
- Requires FM_NOMINAL entry state

#### 4.6.6 Payload Telemetry
- Image count
- Magnetometer field vector
- Radiation dose rate and accumulated dose
- GPS position/time

---

## 5. Health Monitoring Procedures

### 5.1 Health Monitoring Overview

The Health Monitor task (HK) runs continuously at 1 Hz, monitoring system health and generating telemetry. The HK also kicks the hardware watchdog (TPS3431) every 500 ms.

### 5.2 Monitored Parameters

| Parameter          | Source          | Threshold                           | Action          |
|--------------------|-----------------|-------------------------------------|-----------------|
| Bus Voltage        | EPS ADC         | <6.5 V or >8.5 V                   | Warning/Fault   |
| Bus Current        | EPS INA219      | >500 mA                             | Warning         |
| MCU Temperature    | Internal sensor | >85°C                               | Warning         |
| IMU Health         | MPU6050         | Data timeout >2 s                  | Warning         |
| Magnetometer Health| HMC5883L       | Data timeout >2 s                   | Warning         |
| GPS Health         | NEO-7M          | Fix timeout >60 s                  | Warning         |
| EKF Health         | Internal        | Covariance divergence              | Warning         |
| Memory Usage       | FreeRTOS        | Heap <10%                           | Warning         |
| Task Health        | Heartbeat       | Task stuck >5 s                     | Fault           |

### 5.3 Telemetry Schedule

| Telemetry Type | Rate   | Contents                                    |
|----------------|--------|---------------------------------------------|
| HK_SHORT       | 1 Hz   | Mode, voltage, current, temperature, faults |
| HK_FULL        | 0.1 Hz | HK_SHORT + sensor data, GPS, memory        |
| DIAGNOSTIC    | 1 Hz   | Full raw sensor data, debug info            |

### 5.4 Watchdog Behavior

The TPS3431 hardware watchdog triggers a reset if not kicked within 8 seconds. On watchdog reset:
1. System reboots to FM_BOOT
2. Automatically transitions to FM_SAFE
3. Fault logged (FAULT_WDT_KICK_MISSED)
4. Ground station notified via telemetry

---

## 6. Anomaly Handling

### 6.1 Fault Classification

| Level    | Description                                    | Response                          |
|----------|------------------------------------------------|-----------------------------------|
| WARNING  | Non-critical anomaly, mission continuation    | Log, notify, continue operations |
| FAULT    | Significant anomaly, degraded operation       | Log, notify, may trigger mode change |
| CRITICAL | Mission-critical anomaly, uncontrolled state   | Immediate FM_SAFE transition      |

### 6.2 Anomaly Response Matrix

| Anomaly                    | Detection          | Automatic Action         | Ground Action              |
|----------------------------|--------------------|--------------------------|----------------------------|
| High angular rates         | IMU                | FM_DETUMBLE              | Monitor, wait for recovery |
| Sensor failure             | HK timeout         | Disable affected sensor  | Command FM_SAFE if needed |
| Actuator failure           | PWM fault flag     | Disable actuators        | Command FM_SAFE           |
| Watchdog timeout           | TPS3431            | Reboot → FM_SAFE         | Investigate root cause    |
| Bus voltage anomaly        | EPS ADC            | Warning/Fault           | Reduce load, FM_SAFE      |
| Temperature extreme        | MCU internal       | Warning/Fault           | Reduce activity, FM_SAFE  |
| Memory exhaustion          | FreeRTOS           | Fault → FM_SAFE          | Reboot if needed          |
| Comm failure               | CSP timeout        | Switch to listen-only    | Wait, attempt recovery    |

### 6.3 Recovery Procedures

#### 6.3.1 Recovery from FM_SAFE

1. Analyze telemetry to identify cause
2. Verify sensor health
3. Issue CMD_SET_MODE to FM_NOMINAL
4. Monitor transition
5. Verify nominal operations resume

#### 6.3.2 Recovery from Watchdog Reset

1. Receive boot telemetry
2. Review fault log (prior to reset)
3. Issue CMD_SET_MODE to FM_NOMINAL when ready
4. Monitor system behavior
5. If recurring, investigate hardware/firmware

#### 6.3.3 Recovery from Automatic FM_DETUMBLE

1. Wait for angular rates to decrease
2. Verify FM_NOMINAL transition occurs
3. If stuck >30 minutes, command FM_SAFE manually
4. Investigate cause (actuator, sensor)

### 6.4 Emergency Procedures

#### 6.4.1 Total Comm Loss

If communication is lost for >30 minutes:
1. OBC continues in current mode
2. HK continues watchdog kicks
3. Attempt recovery on next pass
4. If persistent, wait for autonomous recovery window

#### 6.4.2 Reboot Procedure

To force a reboot when operating normally:
1. Send CMD_REBOOT
2. Wait for boot sequence (~10 seconds)
3. System enters FM_SAFE automatically
4. Resume normal operations

---

## Appendix A: Flight Mode State Diagram

```
                    +----------+
                    |  FM_BOOT |
                    +----+-----+
                         | (init complete)
                         v
                    +----------+        (CRITICAL fault)
                    |  FM_SAFE | <-----------------------------+
                    +----+-----+                               |
                         | (CMD_SET_MODE)                      |
           +-------------+-------------+                       |
           |             |             |                       |
           v             v             v                       |
    +----------+  +-----------+  +------------+                |
    |FM_DETUMBLE|  |FM_NOMINAL|  |FM_DIAGNOSTIC|               |
    +----+------+  +----+------+  +------+------+              |
         | (rates<threshold)   | (CMD_SET_MODE) |              |
         |------------------+  |                |              |
         v                  v  v                | (CRITICAL)   |
    +----------+ <--------+    |                +-------------+
    |          |  (rates>threshold)  +----------+-------------+
    |          +------------------> |           |              |
    |          |          +-------->|FM_PAYLOAD+-------------+
    +----------+          |         |           |              |
                          |         +----------+               |
                          | (CMD_SET_MODE)                     | 
                          +----------------------------------+
```

---

## Appendix B: Command Quick Reference

| Command          | Usage                                      |
|------------------|--------------------------------------------|
| `CMD_ECHO`       | Verify link: `echo`                        |
| `CMD_REBOOT`     | Reboot system: `reboot`                    |
| `CMD_SET_MODE n` | Change mode: `setmode 1` (1=SAFE, 2=DETUMBLE, 3=NOMINAL, 4=DIAG, 5=PAYLOAD) |
| `CMD_PAYLOAD_CAPTURE t` | Trigger payload: `capture 0` (0=Camera, 1=MAG, 2=RAD) |

---

*End of Document*