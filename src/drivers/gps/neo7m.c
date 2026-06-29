#ifdef PICO_BUILD
  #include "ds3231.h"
#endif
// neo7m.c -- GPS NEO-7M/6M NMEA driver implementation

#include "gps_driver.h"
#include "pico_pins.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PICO_BUILD
  #include "hardware/irq.h"
  #include "hardware/uart.h"
  #include "pico/stdlib.h"

static void gps_uart_isr(void);
#endif

// --- Configuration ---
#ifndef GPS_STALE_THRESHOLD_MS
  #define GPS_STALE_THRESHOLD_MS 5000U  // 5 seconds - consider fix stale if older
#endif

// --- Static variables and buffer for NMEA data ---
#define NMEA_RX_BUFFER_SIZE 2048
static uint8_t nmea_rx_buffer[NMEA_RX_BUFFER_SIZE];
static volatile uint16_t nmea_rx_head = 0;
static volatile uint16_t nmea_rx_tail = 0;

static GpsFix_t g_last_fix = {0};
static uint8_t g_satellites_in_view = 0;
static GpsStats_t g_stats = {0};

#ifdef PICO_BUILD
  #include "FreeRTOS.h"
  #include "semphr.h"
static SemaphoreHandle_t g_gps_mutex = NULL;
#endif

// --- Mutex helpers ---
static void gps_lock(void)
{
#ifdef PICO_BUILD
  if (g_gps_mutex)
  {
    if (xSemaphoreTake(g_gps_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
      return;
    }
  }
#endif
}

static void gps_unlock(void)
{
#ifdef PICO_BUILD
  if (g_gps_mutex)
  {
    xSemaphoreGive(g_gps_mutex);
  }
#endif
}

// --- UART and buffer helpers ---
// These should be connected to real UART driver/ISR for the target

static void nmea_buffer_clear(void)
{
  nmea_rx_head = nmea_rx_tail = 0;
  memset(nmea_rx_buffer, 0, NMEA_RX_BUFFER_SIZE);
}

// Expose for unit test injection only
#ifdef GPS_TEST
bool nmea_buffer_push(uint8_t byte)
{
  uint16_t next = (nmea_rx_head + 1) % NMEA_RX_BUFFER_SIZE;
  if (next == nmea_rx_tail)
  {
    // Buffer full, drop byte
    return false;
  }
  nmea_rx_buffer[nmea_rx_head] = byte;
  nmea_rx_head = next;
  return true;
}
#endif

// Thread-safe buffer push (used by UART ISR on Pico)
void nmea_buffer_push_isr(uint8_t byte)
{
  uint16_t next = (nmea_rx_head + 1) % NMEA_RX_BUFFER_SIZE;
  if (next == nmea_rx_tail)
  {
    return;  // Buffer full, drop byte
  }
  nmea_rx_buffer[nmea_rx_head] = byte;
  nmea_rx_head = next;
}

#ifdef PICO_BUILD
static void gps_uart_isr(void)
{
  while (uart_is_readable(uart0))
  {
    uint8_t byte = uart_getc(uart0);
    nmea_buffer_push_isr(byte);
  }
}
#endif

static bool nmea_buffer_pop(uint8_t *byte)
{
  uint16_t head;
#ifdef PICO_BUILD
  taskENTER_CRITICAL();
#endif
  head = nmea_rx_head;
#ifdef PICO_BUILD
  taskEXIT_CRITICAL();
#endif
  if (head == nmea_rx_tail)
  {
    return false;  // empty
  }
  *byte = nmea_rx_buffer[nmea_rx_tail];
  nmea_rx_tail = (nmea_rx_tail + 1) % NMEA_RX_BUFFER_SIZE;
  return true;
}

// --- UBX Protocol helpers ---
// UBX message structure: $UBX, CLASS, ID, LENGTH(LE), LENGTH(BE), PAYLOAD, CK_A, CK_B

/**
 * @brief Calculate UBX checksum (Fletcher)
 */
static void ubx_calculate_checksum(const uint8_t *payload, uint16_t len, uint8_t *ck_a,
                                   uint8_t *ck_b)
{
  *ck_a = 0;
  *ck_b = 0;
  for (uint16_t i = 0; i < len; i++)
  {
    *ck_a += payload[i];
    *ck_b += *ck_a;
  }
}

/**
 * @brief Send a UBX message to the GPS module
 */
static void gps_send_ubx(const uint8_t *payload, uint16_t len)
{
#ifdef PICO_BUILD
  uint8_t ck_a, ck_b;
  ubx_calculate_checksum(payload, len, &ck_a, &ck_b);

  // Sync char
  uart_putc(uart0, 0xB5);

  // Header and class/ID
  uart_putc(uart0, 0x62);
  for (uint16_t i = 0; i < len; i++)
  {
    uart_putc(uart0, payload[i]);
  }

  // Checksum
  uart_putc(uart0, ck_a);
  uart_putc(uart0, ck_b);
#endif
}

/**
 * @brief Send UBX-CFG-RST command for controlled reset
 *
 * @param reset_mode 0 = hardware reset, 1 = software reset, 4 = controlled GNSS stop, 5 =
 * controlled GNSS start
 * @param clear_mask Which data to clear (bitmask: 0x0001 = ephemeris, 0x0002 = almanac, etc.)
 */
static void gps_send_ubx_reset(uint16_t reset_mode, uint16_t clear_mask)
{
#ifdef PICO_BUILD
  // UBX-CFG-RST: Class=06, ID=04
  const uint8_t payload[4] = {
      (uint8_t)(clear_mask & 0xFF),         // clearMask low byte
      (uint8_t)((clear_mask >> 8) & 0xFF),  // clearMask high byte
      (uint8_t)(reset_mode & 0xFF),         // resetMode low byte
      (uint8_t)((reset_mode >> 8) & 0xFF)   // resetMode high byte (reserved)
  };
  const uint8_t msg[] = {0x06, 0x04};  // UBX class=CFG, ID=RST
  const uint8_t full_payload[8] = {msg[0],     msg[1],     0x04,       0x00,
                                   payload[0], payload[1], payload[2], payload[3]};

  gps_send_ubx(full_payload, 8);
#endif
}

/**
 * @brief Perform a GPS cold start internally (clear all data and restart search)
 */
static void gps_perform_cold_start(void)
{
#ifdef PICO_BUILD
  // Send controlled stop and start to force fresh satellite search
  gps_send_ubx_reset(4, 0x0000);  // Controlled GNSS stop
  sleep_ms(100);
  gps_send_ubx_reset(5, 0x0000);  // Controlled GNSS start
#endif
}

/**
 * @brief Perform a GPS factory reset (clear all stored data)
 */
static void gps_factory_reset(void)
{
#ifdef PICO_BUILD
  // Send controlled reset with clear of all data
  // clearMask: 0x0001=ephemeris, 0x0002=almanac, 0x0004=health, 0x0008=position,
  //            0x0010=time, 0x0020=osc, 0x0040=sbas, 0x0080=rtcm
  uint16_t clear_mask = 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010;  // Clear all position/time data

  // Stop GNSS
  gps_send_ubx_reset(4, 0x0000);
  sleep_ms(100);

  // Reset with clear
  gps_send_ubx_reset(0, clear_mask);  // Immediate controlled reset
  sleep_ms(500);

  // Start GNSS
  gps_send_ubx_reset(5, 0x0000);
#endif
}

// --- Driver API implementation ---
bool gps_init(void)
{
#ifdef PICO_BUILD
  if (g_gps_mutex == NULL)
  {
    g_gps_mutex = xSemaphoreCreateMutex();
  }

  // Reset ring buffer and last fix
  nmea_rx_head = nmea_rx_tail = 0;
  memset(&g_last_fix, 0, sizeof(g_last_fix));
  g_satellites_in_view = 0;
  memset(&g_stats, 0, sizeof(g_stats));

  uart_init(uart0, 9600);
  gpio_set_function(UART0_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART0_RX_PIN, GPIO_FUNC_UART);
  uart_set_hw_flow(uart0, false, false);
  uart_set_fifo_enabled(uart0, true);

  irq_set_exclusive_handler(UART0_IRQ, gps_uart_isr);
  irq_set_enabled(UART0_IRQ, true);
  uart_set_irq_enables(uart0, true, false);

  /* Perform GPS cold start (GNSS stop + restart without clearing ephemeris).
   * Cold start is gentler than factory reset: it clears in-memory state but keeps
   * stored ephemeris, allowing faster TTFF (time-to-first-fix) on next startup.
   * Factory reset would wipe everything, requiring 20-30 min to reacquire sats. */
  sleep_ms(100);  // Wait for GPS module to be ready
  gps_perform_cold_start();
  sleep_ms(500);  // Allow GNSS restart sequence to complete
  printf("GPS: Cold start sent (ephemeris preserved)\n");
#endif

  nmea_buffer_clear();
  memset(&g_last_fix, 0, sizeof(g_last_fix));
  g_satellites_in_view = 0;
  return true;
}

void gps_deinit(void)
{
#ifdef PICO_BUILD
  uart_set_irq_enables(uart0, false, false);
  irq_set_enabled(UART0_IRQ, false);
  irq_remove_handler(UART0_IRQ, gps_uart_isr);
  uart_deinit(uart0);
  if (g_gps_mutex)
  {
    vSemaphoreDelete(g_gps_mutex);
    g_gps_mutex = NULL;
  }
#endif
  nmea_buffer_clear();
}

// Helper: extract a full NMEA sentence from buffer (returns sentence in tmp, or false if none)
static bool nmea_get_sentence(char *dest, size_t maxlen)
{
  uint16_t start = nmea_rx_tail;
  bool found_dollar = false;
  size_t i = 0;
  uint8_t byte;
  // Find '$'
  while (nmea_buffer_pop(&byte))
  {
    if (!found_dollar)
    {
      if (byte == '$')
      {
        found_dollar = true;
        if (i < maxlen - 1)
        {
          dest[i++] = '$';
        }
      }
    }
    else
    {
      if (i < maxlen - 1)
      {
        dest[i++] = byte;
      }
      if (byte == '\n')
      {
        dest[i] = '\0';
        return true;
      }
      // Framing error: sentence too long
      if (i >= maxlen - 1)
      {
        break;
      }
    }
  }
  // Not enough for complete sentence, restore buffer pointer
  nmea_rx_tail = start;
  return false;
}

// Validate and parse NMEA checksum
static bool nmea_verify_checksum(const char *sentence)
{
  if (!sentence || sentence[0] != '$')
  {
    return false;
  }
  const char *star = strchr(sentence, '*');
  if (!star)
  {
    return false;
  }
  uint8_t sum = 0;
  for (const char *p = sentence + 1; *p && *p != '*'; ++p)
  {
    sum ^= (uint8_t)*p;
  }
  char *endptr = NULL;
  unsigned long chk = strtoul(star + 1, &endptr, 16);
  if (endptr == star + 1 || (*endptr != '\0' && *endptr != '\r' && *endptr != '\n'))
  {
    return false;
  }
  return (sum == (chk & 0xFF));
}

// Convert NMEA lat/lon floats to decimal degrees
double nmea_deg_min_to_dec(const char *str, char hemisphere)
{
  if (!str || !*str)
  {
    return 0.0;
  }
  double deg = 0.0;
  double min = 0.0;
  char *endptr = NULL;
  if (hemisphere == 'N' || hemisphere == 'S')
  {
    // Latitude: 2 digits degrees
    char deg_str[3] = {0};
    strncpy(deg_str, str, 2);
    long d = strtol(deg_str, &endptr, 10);
    if (endptr == deg_str || *endptr != '\0')
    {
      return 0.0;
    }
    deg = (double)d;
    min = strtod(str + 2, &endptr);
    if (endptr == str + 2)
    {
      return 0.0;
    }
  }
  else if (hemisphere == 'E' || hemisphere == 'W')
  {
    // Longitude: 3 digits degrees
    char deg_str[4] = {0};
    strncpy(deg_str, str, 3);
    long d = strtol(deg_str, &endptr, 10);
    if (endptr == deg_str || *endptr != '\0')
    {
      return 0.0;
    }
    deg = (double)d;
    min = strtod(str + 3, &endptr);
    if (endptr == str + 3)
    {
      return 0.0;
    }
  }
  double val = deg + (min / 60.0);
  if (hemisphere == 'S' || hemisphere == 'W')
  {
    val = -val;
  }
  return val;
}

// $GPGGA parser: update g_last_fix
static void nmea_parse_gga(const char *sentence)
{
  // Example:
  // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n
  char buf[128];
  strncpy(buf, sentence, sizeof(buf));
  buf[sizeof(buf) - 1] = 0;
  char *tok = buf;
  char *fields[15] = {0};
  int field = 0;
  for (field = 0; field < 15; field++)
  {
    fields[field] = strsep(&tok, ",");
    if (!fields[field])
    {
      break;
    }
  }

  if (field < 9)
  {
    return;
  }
  // UTC time: fields[1]
  // Latitude: fields[2] + [3]
  // Longitude: fields[4] + [5]
  // Fix quality: fields[6] (1=valid)
  // Num satellites: fields[7]
  // Altitude (meters): fields[9]

  GpsFix_t fix = {0};
  if (fields[2] && fields[3] && fields[3][0] && fields[4] && fields[5] && fields[5][0] &&
      fields[6] && fields[7] && fields[9])
  {
    fix.lat = (float)nmea_deg_min_to_dec(fields[2], fields[3][0]);
    fix.lon = (float)nmea_deg_min_to_dec(fields[4], fields[5][0]);
    char *endptr = NULL;
    fix.alt_m = (float)strtod(fields[9], &endptr);
    if (endptr == fields[9])
    {
      fix.alt_m = 0.0f;
    }
    fix.valid = (fields[6][0] == '1');
    fix.timestamp_ms = 0;
    {
      char *endptr_sat = NULL;
      fix.satellites = (uint8_t)strtoul(fields[7], &endptr_sat, 10);
      if (endptr_sat == fields[7])
      {
        fix.satellites = 0;
      }
    }
    if (fields[8])
    {
      char *endptr_hdop = NULL;
      fix.hdop = (float)strtod(fields[8], &endptr_hdop);
      if (endptr_hdop == fields[8])
      {
        fix.hdop = 99.0f;
      }
    }
    else
    {
      fix.hdop = 99.0f;
    }
    if (fields[1])
    {
      int h = 0;
      int m = 0;
      int s = 0;
      char hh[3] = {0};
      char mm[3] = {0};
      char ss[3] = {0};
      strncpy(hh, fields[1], 2);
      strncpy(mm, fields[1] + 2, 2);
      strncpy(ss, fields[1] + 4, 2);
      h = (int)strtol(hh, &endptr, 10);
      if (endptr == hh || *endptr != '\0')
      {
        h = 0;
      }
      m = (int)strtol(mm, &endptr, 10);
      if (endptr == mm || *endptr != '\0')
      {
        m = 0;
      }
      s = (int)strtol(ss, &endptr, 10);
      if (endptr == ss || *endptr != '\0')
      {
        s = 0;
      }
      fix.utc_time = h * 3600 + m * 60 + s;
    }
    g_satellites_in_view = fix.satellites;

    gps_lock();
    g_last_fix = fix;
    gps_unlock();
  }
}

#if defined(PICO_BUILD)
  #include "pico/util/datetime.h"
// $GPRMC parser: sync RTC with GPS time
static void nmea_parse_gprmc_and_sync_rtc(const char *sentence)
{
  // Example: $GPRMC,235947.00,A,3723.2475,N,12202.3246,W,0.13,309.62,120598,,,A*10
  char buf[128];
  strncpy(buf, sentence, sizeof(buf));
  buf[sizeof(buf) - 1] = 0;
  char *tok = buf;
  char *fields[13] = {0};
  int field = 0;
  for (field = 0; field < 13; field++)
  {
    fields[field] = strsep(&tok, ",");
    if (!fields[field])
    {
      break;
    }
  }
  if (field < 10)
  {
    return;
  }
  // fields[2] = 'A' (data valid)
  if (fields[2] && fields[2][0] == 'A')
  {
    // fields[1]: UTC time (hhmmss.sss)
    // fields[9]: date (ddmmyy)
    int h = 0, m = 0, s = 0, day = 1, mon = 1, year = 2000;
    char *endptr = NULL;
    if (fields[1] && strlen(fields[1]) >= 6)
    {
      char hh[3] = {0}, mm[3] = {0}, ss[3] = {0};
      strncpy(hh, fields[1], 2);
      strncpy(mm, fields[1] + 2, 2);
      strncpy(ss, fields[1] + 4, 2);
      h = (int)strtol(hh, &endptr, 10);
      if (endptr == hh || *endptr != '\0')
      {
        h = 0;
      }
      m = (int)strtol(mm, &endptr, 10);
      if (endptr == mm || *endptr != '\0')
      {
        m = 0;
      }
      s = (int)strtol(ss, &endptr, 10);
      if (endptr == ss || *endptr != '\0')
      {
        s = 0;
      }
    }
    if (fields[9] && strlen(fields[9]) >= 6)
    {
      char dd[3] = {0}, mo[3] = {0}, yy[3] = {0};
      strncpy(dd, fields[9], 2);
      strncpy(mo, fields[9] + 2, 2);
      strncpy(yy, fields[9] + 4, 2);
      day = (int)strtol(dd, &endptr, 10);
      if (endptr == dd || *endptr != '\0')
      {
        day = 1;
      }
      mon = (int)strtol(mo, &endptr, 10);
      if (endptr == mo || *endptr != '\0')
      {
        mon = 1;
      }
      year = (int)strtol(yy, &endptr, 10);
      if (endptr == yy || *endptr != '\0')
      {
        year = 2000;
      }
      else
      {
        year += 2000;
      }
    }
    ds3231_set_time((uint16_t)year, (uint8_t)mon, (uint8_t)day, (uint8_t)h, (uint8_t)m, (uint8_t)s);
  }
}
#endif

GpsFix_t *gps_read_fix(void)
{
  char line[128];

  if (!nmea_get_sentence(line, sizeof(line)))
  {
    return NULL;
  }

  if (!nmea_verify_checksum(line))
  {
    g_stats.checksum_errors++;
    return NULL;
  }

  g_stats.sentences_received++;

#ifdef PICO_BUILD
  (void)printf("GPS: %s\n", line);
#endif

  if (strncmp(line + 1, "GPGGA", 5) == 0)
  {
    nmea_parse_gga(line);
    if (g_last_fix.valid)
    {
      g_stats.fixes_valid++;
    }
    else
    {
      g_stats.fixes_invalid++;
    }
    gps_lock();
#ifdef PICO_BUILD
    g_last_fix.timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    g_last_fix.timestamp_ms = 0;
#endif
    gps_unlock();
  }
#ifdef PICO_BUILD
  else if (strncmp(line + 1, "GPRMC", 5) == 0)
  {
    nmea_parse_gprmc_and_sync_rtc(line);
  }
#endif

  GpsFix_t *result = &g_last_fix;

  return result;
}

bool gps_get_last_fix(GpsFix_t *out)
{
  if (out == NULL)
  {
    return false;
  }
  gps_lock();
  *out = g_last_fix;
  gps_unlock();
  return out->valid;
}

bool gps_is_fix_valid(void)
{
#ifdef PICO_BUILD
  uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
  uint32_t age_ms = now_ms - g_last_fix.timestamp_ms;
  if (age_ms > GPS_STALE_THRESHOLD_MS)
  {
    return false;
  }
#endif
  return g_last_fix.valid;
}

uint8_t gps_get_satellites_in_view(void)
{
  // Updated by nmea_parse_gga() when a valid GPGGA sentence is received
  return g_satellites_in_view;
}

const GpsStats_t *gps_get_stats(void)
{
  return &g_stats;
}

void gps_reset_stats(void)
{
  memset(&g_stats, 0, sizeof(g_stats));
}

void gps_cold_start(void)
{
#ifdef PICO_BUILD
  printf("GPS: Sending cold start command...\n");
  gps_factory_reset();
  printf("GPS: Cold start complete\n");
#else
  (void)gps_factory_reset;
#endif
}
