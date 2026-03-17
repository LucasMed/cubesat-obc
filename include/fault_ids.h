#define FAULT_GPS_TIMEOUT (FAULT_SUBSYS_SENSOR | 0x10u) /**< GPS UART0 silent > 10s */
#define FAULT_GPS_PARSE_ERR (FAULT_SUBSYS_SENSOR | 0x11u) /**< GPS NMEA parse error */
/**
 * @file fault_ids.h
 * @brief Fault identifier catalogue.
 *
 * Every fault that the Fault Manager can track is assigned a unique
 * 16-bit ID.  The upper byte encodes the subsystem; the lower byte
 * encodes the specific fault within that subsystem.
 *
 *   Bits [15:8] — subsystem code
 *   Bits  [7:0] — fault index within subsystem
 *
 * Spec ref: SPEC-2-FMM v1.1 §5, SPEC-2 v2.0 §4.2
 */

#ifndef FAULT_IDS_H
#define FAULT_IDS_H

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Subsystem base addresses                                            */
  /* ------------------------------------------------------------------ */

#define FAULT_SUBSYS_ESTIMATOR 0x0100u
#define FAULT_SUBSYS_CONTROLLER 0x0200u
#define FAULT_SUBSYS_ACTUATOR 0x0300u
#define FAULT_SUBSYS_SENSOR 0x0400u
#define FAULT_SUBSYS_TIMING 0x0500u
#define FAULT_SUBSYS_WATCHDOG 0x0600u
#define FAULT_SUBSYS_COMMAND 0x0700u
#define FAULT_SUBSYS_THERMAL 0x0800u
#define FAULT_SUBSYS_EPS 0x0900u
#define FAULT_SUBSYS_TELEMETRY 0x0A00u

  /* ------------------------------------------------------------------ */
  /* Estimator faults (0x0101 – 0x01FF)                                 */
  /* ------------------------------------------------------------------ */

#define FAULT_EST_GYRO_TIMEOUT                                                                     \
  (FAULT_SUBSYS_ESTIMATOR | 0x01u) /**< IMU gyro read timeout          */
#define FAULT_EST_MAG_TIMEOUT                                                                      \
  (FAULT_SUBSYS_ESTIMATOR | 0x02u) /**< Magnetometer read timeout       */
#define FAULT_EST_DIVERGENCE                                                                       \
  (FAULT_SUBSYS_ESTIMATOR | 0x03u)                           /**< Estimator state diverged        */
#define FAULT_EST_QUAT_NORM (FAULT_SUBSYS_ESTIMATOR | 0x04u) /**< Quaternion norm out of range */

  /* ------------------------------------------------------------------ */
  /* Controller faults (0x0201 – 0x02FF)                                */
  /* ------------------------------------------------------------------ */

#define FAULT_CTRL_OUTPUT_SATURATED                                                                \
  (FAULT_SUBSYS_CONTROLLER | 0x01u) /**< Control output clipped          */
#define FAULT_CTRL_RATE_LIMIT                                                                      \
  (FAULT_SUBSYS_CONTROLLER | 0x02u) /**< Rate-limit violation            */
#define FAULT_CTRL_DEADLINE_MISS                                                                   \
  (FAULT_SUBSYS_CONTROLLER | 0x03u) /**< Control loop deadline missed    */

  /* ------------------------------------------------------------------ */
  /* Actuator faults (0x0301 – 0x03FF)                                  */
  /* ------------------------------------------------------------------ */

#define FAULT_ACT_RW_OVERCURRENT                                                                   \
  (FAULT_SUBSYS_ACTUATOR | 0x01u) /**< Reaction wheel overcurrent       */
#define FAULT_ACT_RW_SPEED_LIMIT                                                                   \
  (FAULT_SUBSYS_ACTUATOR | 0x02u)                           /**< Reaction wheel speed exceeded    */
#define FAULT_ACT_MTQ_FAULT (FAULT_SUBSYS_ACTUATOR | 0x03u) /**< Magnetorquer driver fault */

  /* ------------------------------------------------------------------ */
  /* Sensor faults (0x0401 – 0x04FF)                                    */
  /* ------------------------------------------------------------------ */

#define FAULT_SENS_IMU_I2C_ERROR                                                                   \
  (FAULT_SUBSYS_SENSOR | 0x01u) /**< IMU I2C bus error                 */
#define FAULT_SENS_IMU_DATA_STALE                                                                  \
  (FAULT_SUBSYS_SENSOR | 0x02u) /**< IMU data not refreshed in time    */
#define FAULT_SENS_TEMP_OUT_RANGE                                                                  \
  (FAULT_SUBSYS_SENSOR | 0x03u) /**< Temperature sensor out of range   */

  /* ------------------------------------------------------------------ */
  /* Timing faults (0x0501 – 0x05FF)                                    */
  /* ------------------------------------------------------------------ */

#define FAULT_TIMING_DEADLINE_MISS                                                                 \
  (FAULT_SUBSYS_TIMING | 0x01u) /**< Generic task deadline missed      */

  /* ------------------------------------------------------------------ */
  /* Watchdog faults (0x0601 – 0x06FF)                                  */
  /* ------------------------------------------------------------------ */

#define FAULT_WDT_KICK_MISSED                                                                      \
  (FAULT_SUBSYS_WATCHDOG | 0x01u) /**< Watchdog kick not received in time */

  /* ------------------------------------------------------------------ */
  /* Command faults (0x0701 – 0x07FF)                                   */
  /* ------------------------------------------------------------------ */

#define FAULT_CMD_UNKNOWN (FAULT_SUBSYS_COMMAND | 0x01u) /**< Unrecognised command received     */
#define FAULT_CMD_QUEUE_FULL                                                                       \
  (FAULT_SUBSYS_COMMAND | 0x02u) /**< Command queue overflow            */

  /* ------------------------------------------------------------------ */
  /* Thermal faults (0x0801 – 0x08FF)                                   */
  /* ------------------------------------------------------------------ */

#define FAULT_THERM_OVER_TEMP                                                                      \
  (FAULT_SUBSYS_THERMAL | 0x01u) /**< Over-temperature threshold hit    */
#define FAULT_THERM_UNDER_TEMP                                                                     \
  (FAULT_SUBSYS_THERMAL | 0x02u) /**< Under-temperature threshold hit   */

  /* ------------------------------------------------------------------ */
  /* EPS faults (0x0901 – 0x09FF)                                       */
  /* ------------------------------------------------------------------ */

#define FAULT_EPS_VBATT_LOW (FAULT_SUBSYS_EPS | 0x01u) /**< Battery voltage below LOW threshold */
#define FAULT_EPS_VBATT_CRITICAL                                                                   \
  (FAULT_SUBSYS_EPS | 0x02u) /**< Battery voltage below CRITICAL threshold (7.0 V) */
#define FAULT_EPS_VBATT_EMERGENCY                                                                  \
  (FAULT_SUBSYS_EPS | 0x03u) /**< Battery voltage below EMERGENCY threshold (6.6 V) */
#define FAULT_EPS_OVERCURRENT                                                                      \
  (FAULT_SUBSYS_EPS | 0x04u) /**< Bus overcurrent detected              */
#define FAULT_EPS_READ_ERROR                                                                       \
  (FAULT_SUBSYS_EPS | 0x05u) /**< EPS telemetry read failure            */

  /* ------------------------------------------------------------------ */
  /* Telemetry faults (0x0A01 – 0x0AFF)                                 */
  /* ------------------------------------------------------------------ */

#define FAULT_TLM_QUEUE_OVERFLOW                                                                   \
  (FAULT_SUBSYS_TELEMETRY | 0x01u) /**< Telemetry transmit queue full    */

  /* ------------------------------------------------------------------ */
  /* Total number of tracked fault IDs                                   */
  /* ------------------------------------------------------------------ */

#define FAULT_ID_COUNT 25u /**< Must equal the total number of FAULT_* macros above */

#ifdef __cplusplus
}
#endif

#endif /* FAULT_IDS_H */
