# Phase 3 Communication Specification

This document defines the interface for communicating with the CubeSat On-Board Computer (OBC) over the CubeSat Space Protocol (CSP).

## 1. Network Topology

- **OBC Address**: 10
- **Ground Station (GN) Address**: 1
- **Physical Layer**: UART1 (GPIO 4: TX, GPIO 5: RX) at 115200 baud
- **Framing**: KISS (Keep It Simple, Stupid) protocol via `csp_if_kiss`

## 2. Telemetry Interface (Downlink)

The OBC continuously broadcasts telemetry packets at 1 Hz.

### Connection Details
- **Source Address**: 10 (OBC)
- **Source Port**: 10
- **Destination Address**: 1 (Ground Station)
- **Destination Port**: 10
- **Type**: Connection-less (UDP style)
- **Priority**: Normal (`CSP_PRIO_NORM`)

### Packet Structure
`csp_telemetry_packet_t` (29 bytes, packed)
```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp_ms;      // 4 bytes: Time since boot [ms]
    float attitude[3];          // 12 bytes: Roll, Pitch, Yaw [deg]
    float rates[3];             // 12 bytes: Gyro rates (Roll, Pitch, Yaw) [deg/s]
    float temp;                 // 4 bytes: Internal temperature [C]
    uint8_t flags;              // 1 byte: Bit 0: imu_valid, Bit 1: temp_valid
} csp_telemetry_packet_t;
```
*Note: Floats are standard IEEE-754 32-bit single precision.*

## 3. Command Interface (Uplink)

The OBC listens for incoming commands from the Ground Station.

### Connection Details
- **Source Address**: 1 (Ground Station)
- **Destination Address**: 10 (OBC)
- **Destination Port**: 20
- **Type**: Connection-oriented

### Packet Structure
`csp_command_packet_t` (variable length)
```c
typedef struct __attribute__((packed)) {
    uint8_t cmd_id;             // 1 byte: Command ID
    uint8_t payload[32];        // Variable length depending on cmd_id
} csp_command_packet_t;
```

### Command Dictionary

| `cmd_id` | Name       | Payload | Description | Response |
|----------|------------|---------|-------------|----------|
| `1`      | `CMD_ECHO` | `N` bytes | Echoes back the payload. Used to test latency and packet integrity. | Returns exact packet |
| `2`      | `CMD_REBOOT`| `0` bytes | Initiates a system hardware watchdog reboot. | None |
| `3`      | `CMD_SET_MODE`| `1` byte | Requests a mode change (e.g. 0=Safe, 1=AttitudeControl, 2=Idle). *Not fully implemented yet.* | None |

## 4. Built-in CSP Services

The `libcsp` stack handles built-in services automatically on reserved ports:
- **Port 0**: CSP Ping (responds to pings transparently)
- **Port 1-6**: CSP management services (Buffer status, uptime, free memory)
