# Telemetry Protocol Specification

## Overview

This document describes the telemetry protocol used for communication between the CubeSat OBC and the Ground Station (via HC-12 radio module).

## Transport

- **UART**: UART1 (GPIO8=TX, GPIO9=RX) at 9600 baud
- **Radio**: HC-12 module (433 MHz)
- **Rate**: 1 Hz (once per second)

## Output Formats

### TEXT Format (Default)

```
[TLM] m={mode} a={roll},{pitch},{yaw} t={temp} h={humidity} l={lux} r={rtc_ts} f={flags} g={lat},{lon},{alt} v={gps_valid} s={satellites} sx={sun_x} sy={sun_y} c={crc}\r\n
```

**Example:**
```
[TLM] m=3 a=0.5,-1.2,45.3 t=25.5 h=42.0 l=50000 r=1777133006 f=0x7B g=-34.901,-56.164,520000 v=1 s=8 sx=0.21 sy=0.29 c=A3\r\n
```

### JSON Format

**Compact flattened format** (used by OBC):

```
[JSON] {ts:1777133006,m:3,a:0.5,-1.2,45.3,t:25.5,h:42.0,l:50000,g:-34.901,-56.164,520000,v:1,s:8,p:5000,100,500,sx:0.21,sy:0.29,f:123,c:A3}
```

**Parsed output** (displayed by ground station):

```
JSON Mode=3 Roll=0.5 Pitch=-1.2 Yaw=45.3 IMU=OK Temp=25.5C Hum=42.0% Lux=50000 
Sun=[0.21,0.29] Energy=3 GPS=OK Lat=-34.901 Lon=-56.164 Alt=520000 
V=5000mV I=100mA P=500mW ts=1777133006 CRC=A3
```

## Field Definitions

| Field | Type | Description | Range |
|-------|------|-------------|-------|
| `m` | uint8 | Flight mode (0=BOOT,1=SAFE,2=DETUMBLE,3=NOMINAL,4=DIAG,5=PAYLOAD) | 0-5 |
| `a` | float | Attitude roll,pitch,yaw [°] | See below |
| `t` | float | Temperature [°C] | -40..+85 |
| `h` | float | Relative humidity [%], -1 if N/A | 0..100, -1 |
| `l` | float | Illuminance [lux], -1 if N/A | 0..65535, -1 |
| `r` | uint32 | RTC Unix timestamp | 0..2^32-1 |
| `f` | hex | Flags byte (validity bits + energy state) | 0x00..0xFF |
| `g` | float | GPS lat,lon,alt [°,°,m] | Valid ranges |
| `v` | uint8 | GPS fix valid (0/1) | 0-1 |
| `s` | uint8 | Satellites in view | 0-20 |
| `sx` | float | Sun sensor X (0.0-1.0), -1 if N/A | 0..1, -1 |
| `sy` | float | Sun sensor Y (0.0-1.0), -1 if N/A | 0..1, -1 |
| `c` | hex | CRC8 checksum (2 hex digits) | 00..FF |

### Attitude Ranges

- Roll (`a[0]`): -180° to +180°
- Pitch (`a[1]`): -90° to +90°
- Yaw/Heading (`a[2]`): 0° to 360°

### GPS Ranges

- Latitude: -90° to +90°
- Longitude: -180° to +180°
- Altitude: 0 to 1,000,000 m (LEO orbit ~400km = 400,000m)

## Flags Byte

| Bit | Name | Description |
|-----|------|------------|
| 0 | imu_valid | IMU/attitude data valid |
| 1 | temp_valid | Temperature valid |
| 2 | humidity_valid | Humidity valid |
| 3 | lux_valid | Illuminance valid |
| 4 | rtc_valid | RTC timestamp valid |
| 5 | sun_valid | Sun sensor valid |
| 6 | power_valid | Power monitoring valid |
| 7:5 | energy_state | Energy state (0=NOMINAL, 1=LOW, 2=HIGH, 3=EMERGENCY) |

## CRC8 Checksum

The CRC8 is calculated using the **CASPAC polynomial (0x07)**:

```
CRC8 = CRC8-CASPAC (data)
```

- For TEXT: CRC calculated on data after "[TLM] " (5 bytes), excluding trailing " c=  \r\n"
- For JSON: CRC calculated on the JSON string after "[JSON] " prefix (7 bytes), excluding `,c:XX}\r\n`
- Checksum is appended as 2 uppercase hex digits

### CRC8 Algorithm (C)

```c
uint8_t crc8_calc(const uint8_t *data, uint16_t len)
{
  uint8_t crc = 0;
  for (uint16_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++)
    {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;  // CASPAC polynomial
      else
        crc <<= 1;
    }
  }
  return crc;
}
```

## Commands

| Command | Description |
|---------|------------|
| `TLMFMT=JSON` | Switch to JSON output format |
| `TLMFMT=TEXT` | Switch to TEXT output format |
| `TLMFMT` | Query current format |

### Response

```
[CMD] TLMFMT=JSON OK
[CMD] TLMFMT=TEXT OK
[CMD] TLMFMT: JSON
```

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-04-25 | Initial protocol with CRC8, sun sensor, JSON/TEXT switching |

## Test Cases

| ID | Description |
|----|------------|
| T-TLM-01 | Full fields sent in FM_NOMINAL |
| T-TLM-02 | Only HK in FM_SAFE (attitude zeroed) |
| T-TLM-03 | Sends in all flight modes |
| T-TLM-04 | Energy state encoded in flags |
| T-TLM-05 | Validity flags track sensor status |
| T-CRC-01 | CRC8 basic calculation |
| T-CRC-02 | CRC8 all zeros |
| T-CRC-03 | CRC8 all ones |
| T-CRC-04 | CRC8 telemetry string |
| T-SUN-01 | Sun sensor write to data layer |
| T-SUN-02 | Sun validity false when N/A |
| T-SUN-03 | Sun availability set |
| T-SUN-04 | Sequence increments on write |