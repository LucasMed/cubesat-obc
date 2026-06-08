#ifndef TELEMETRY_PACKET_H
#define TELEMETRY_PACKET_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ================================================================
 * Binary Telemetry Packet Protocol
 *
 * Sent over UART1 (HC-12) with 2-byte sync prefix:
 *   [0xAA] [0x55] [64 bytes telemetry_packet_t]
 *
 * Total over-the-air: 66 bytes per frame (~69ms @ 9600 baud).
 * The ground station reads byte-by-byte from the SoftwareSerial
 * buffer in its main loop, so the 64-byte RX buffer never overflows
 * despite the 66-byte frame — each byte is consumed before the next
 * serial character arrives at 9600 baud.
 * ================================================================ */

/* Sync word bytes */
#define TLM_SYNC_BYTE_1 0xAA
#define TLM_SYNC_BYTE_2 0x55

/* Packet size (without sync) */
#define TLM_PACKET_SIZE 64

  /**
   * telemetry_packet_t — Binary telemetry frame (packed, 64 bytes)
   *
   * All multi-byte values are little-endian (native for both
   * ARM Cortex-M0+ and AVR).
   *
   * Precision notes:
   *   attitude : int16_t ×10   →  -180.0 .. 180.0°  (0.1° resolution)
   *   temp/hum : int16_t ×10   →  -3276.8 .. 3276.7°C  (0.1°C)
   *   lux      : uint16_t      →  0 .. 65535 lux
   *   GPS lat/lon : int32_t ×1e7 →  ±180°  (1cm precision)
   *   GPS alt  : int16_t       →  -32768 .. 32767 m
   *   voltage  : int16_t       →  mV
   *   current  : int16_t       →  mA
   *   power    : int16_t       →  mW
   *   sun_x/y  : int16_t ×100  →  -327.68 .. 327.67
   *   mag_x/y/z: int16_t ×100  →  -327.68 .. 327.67 µT
   */
  typedef struct __attribute__((packed))
  {
    /* Timestamps */
    uint32_t ts;   // [0]  Freertos tick in ms
    uint32_t rtc;  // [4]  Unix epoch seconds

    /* Mode / state */
    uint8_t mode;  // [8]  OBC mode (0=INIT .. 3=NOMINAL)

    /* Attitude (×10) */
    int16_t roll;   // [9]
    int16_t pitch;  // [11]
    int16_t yaw;    // [13]

    /* Environment (×10) */
    int16_t temp;      // [15] Temperature in 0.1°C
    int16_t humidity;  // [17] Humidity in 0.1%
    uint16_t lux;      // [19] Illuminance in lux

    /* GPS */
    int32_t gps_lat;    // [21] Latitude  × 1e7
    int32_t gps_lon;    // [25] Longitude × 1e7
    int16_t gps_alt;    // [29] Altitude in meters
    uint8_t gps_valid;  // [31] 0=no fix, 1=fix
    uint8_t gps_sats;   // [32] Satellites in view

    /* Bus power */
    int16_t bus_mv;  // [33] Bus voltage mV
    int16_t bus_ma;  // [35] Bus current mA
    int16_t bus_mw;  // [37] Bus power mW

    /* Battery */
    int16_t battery_mv;  // [39] Battery mV

    /* Solar panel */
    int16_t solar_mv;  // [41] Solar voltage mV
    int16_t solar_ma;  // [43] Solar current mA
    int16_t solar_mw;  // [45] Solar power mW

    /* Sun sensor (×100) */
    int16_t sun_x;  // [47]
    int16_t sun_y;  // [49]

    /* Payload housekeeping (FR-17) */
    int16_t mag_x;                 // [51] Mag X ×100 [µT]
    int16_t mag_y;                 // [53] Mag Y ×100 [µT]
    int16_t mag_z;                 // [55] Mag Z ×100 [µT]
    uint16_t radiation;            // [57] Radiation dose
    uint16_t image_count;          // [59] Images on payload SD
    uint8_t payload_rail_enabled;  // [61] 1=rail on

    /* Status */
    uint8_t flags;  // [62] Bitmask (see TLM_FLAG_*)
    uint8_t crc;    // [63] CRC-8/MAXIM over bytes [0..62]
  } telemetry_packet_t;

  _Static_assert(sizeof(telemetry_packet_t) == TLM_PACKET_SIZE,
                 "telemetry_packet_t must be exactly 64 bytes (FR-17)");

/* Flag bits (matches telemetry_task.c) */
#define TLM_FLAG_IMU_OK (1u << 0)
#define TLM_FLAG_TEMP_OK (1u << 1)
#define TLM_FLAG_HUMIDITY_OK (1u << 2)
#define TLM_FLAG_LUX_OK (1u << 3)
#define TLM_FLAG_RTC_OK (1u << 4)
#define TLM_FLAG_ENERGY_SHIFT 5u

  /* ================================================================
   * CRC-8/MAXIM (Dallas 1-Wire)
   * ================================================================ */
  static inline uint8_t tlm_crc8(const uint8_t *data, uint16_t len)
  {
    /* LSB-first CRC-8/MAXIM (Dallas 1-Wire) — polynomial 0x31, reflected 0x8C
     *
     * This must match crc8_maxim() in the Arduino ground station.  An MSB-first
     * implementation with poly 0x31 but NO bit-reflection gives a *different*
     * result — CRC-8/MAXIM specifies RefIn=true + RefOut=true.
     */
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++)
    {
      uint8_t extract = data[i];
      for (uint8_t j = 8; j; j--)
      {
        uint8_t sum = (crc ^ extract) & 0x01u;
        crc >>= 1;
        if (sum)
          crc ^= 0x8C;
        extract >>= 1;
      }
    }
    return crc;
  }

  /* Verify packet CRC — returns 1 if valid, 0 if corrupted */
  static inline int tlm_packet_ok(const telemetry_packet_t *pkt)
  {
    uint8_t expected = tlm_crc8((const uint8_t *)pkt, sizeof(*pkt) - 1);
    return (expected == pkt->crc);
  }

  /* Check if a byte pair matches the sync word */
  static inline int tlm_is_sync(uint8_t b1, uint8_t b2)
  {
    return (b1 == TLM_SYNC_BYTE_1 && b2 == TLM_SYNC_BYTE_2);
  }

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_PACKET_H */
