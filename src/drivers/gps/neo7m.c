#ifdef PICO_BUILD
  #if defined(PICO_RP2040)
    #include "hardware/rtc.h"
    #include "pico/util/datetime.h"
  #endif
#endif
// neo7m.c -- GPS NEO-7M/6M NMEA driver implementation
// All comments in English, see WP-7.10

#include "gps_driver.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- Static variables and buffer for NMEA data ---
#define NMEA_RX_BUFFER_SIZE 2048
static uint8_t nmea_rx_buffer[NMEA_RX_BUFFER_SIZE];
static volatile uint16_t nmea_rx_head = 0;
static volatile uint16_t nmea_rx_tail = 0;

static GpsFix_t g_last_fix = {0};
static uint8_t g_satellites_in_view = 0;

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
    xSemaphoreTake(g_gps_mutex, portMAX_DELAY);
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

static bool nmea_buffer_pop(uint8_t *byte)
{
  if (nmea_rx_head == nmea_rx_tail)
  {
    return false;  // empty
  }
  *byte = nmea_rx_buffer[nmea_rx_tail];
  nmea_rx_tail = (nmea_rx_tail + 1) % NMEA_RX_BUFFER_SIZE;
  return true;
}

// --- Driver API implementation ---
bool gps_init(void)
{
#ifdef PICO_BUILD
  g_gps_mutex = xSemaphoreCreateMutex();
#endif
  nmea_buffer_clear();
  memset(&g_last_fix, 0, sizeof(g_last_fix));
  g_satellites_in_view = 0;
  return true;
}

void gps_deinit(void)
{
  // TODO: Release UART, stop interrupts, etc.
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
  if (fields[2] && fields[3] && fields[4] && fields[5] && fields[6] && fields[7] && fields[9])
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
    fix.timestamp_ms = 0;  // TODO: get system time in ms
    // UTC time: convert HHMMSS.00 to seconds since midnight
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
    g_satellites_in_view = (uint8_t)strtoul(fields[7], &endptr, 10);
    if (endptr == fields[7])
    {
      g_satellites_in_view = 0;
    }

    gps_lock();
    g_last_fix = fix;
    gps_unlock();
  }
}

#if defined(PICO_BUILD) && defined(PICO_RP2040)
#include "pico/util/datetime.h"
// $GPRMC parser: sync RTC with GPS time (RP2040 only - datetime_t not available on RP2350)
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
    datetime_t dt = {.year = (int16_t)year,
                     .month = (int8_t)mon,
                     .day = (int8_t)day,
                     .dotw = 0,
                     .hour = (int8_t)h,
                     .min = (int8_t)m,
                     .sec = (int8_t)s};
    rtc_set_datetime(&dt);
  }
}
#endif

#if defined(PICO_BUILD) && !defined(PICO_RP2040)
static void nmea_parse_gprmc_and_sync_rtc(const char *sentence)
{
  (void)sentence;
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
    return NULL;
  }
  if (strncmp(line + 1, "GPGGA", 5) == 0)
  {
    nmea_parse_gga(line);
  }
#ifdef PICO_BUILD
  else if (strncmp(line + 1, "GPRMC", 5) == 0)
  {
    nmea_parse_gprmc_and_sync_rtc(line);
  }
#endif

  gps_lock();
#ifdef PICO_BUILD
  g_last_fix.timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
  g_last_fix.timestamp_ms = 0;
#endif
  GpsFix_t *result = &g_last_fix;
  gps_unlock();

  return result;
}

GpsFix_t *gps_get_last_fix(void)
{
  // Return pointer to last parsed valid fix
  return &g_last_fix;
}

bool gps_is_fix_valid(void)
{
  // TODO: Add stale detection, timestamp check
  return g_last_fix.valid;
}

uint8_t gps_get_satellites_in_view(void)
{
  // TODO: parse from GPGGA sentence
  return g_satellites_in_view;
}
