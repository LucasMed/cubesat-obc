#include <SoftwareSerial.h>

/* =================================================================
 * Binary Telemetry Packet Protocol — Arduino Ground Station
 *
 * The OBC sends 66-byte frames interleaved with text command responses:
 *   Binary  : [0xAA] [0x55] [64-byte telemetry_packet_t]
 *   Text    : Lines ending in \n, prefixed like [CMD], GPS:, etc.
 *
 * State machine detects binary frames by sync word, falls through
 * to line-based text processing for command responses.
 *
 * Note: The SoftwareSerial RX buffer is 64 bytes and the frame is 66
 * bytes over the air. This works because the loop() reads byte-by-byte
 * from HC12 — each byte is consumed before the next one arrives at
 * 9600 baud. There is no risk of buffer overflow in practice.
 * ================================================================= */

/* ---- Binary protocol constants (mirrors OBC) ---- */
#define TLM_SYNC_BYTE_1  0xAA
#define TLM_SYNC_BYTE_2  0x55
#define TLM_PACKET_SIZE  64

/* State machine states */
enum { ST_IDLE, ST_GOT_AA, ST_COLLECT, ST_VERIFY };

/* ---- Binary telemetry packet (packed, 64 bytes, FR-17) ---- */
typedef struct __attribute__((packed)) {
  uint32_t ts;               // [0]  FreeRTOS tick ms
  uint32_t rtc;              // [4]  Unix epoch seconds
  uint8_t  mode;             // [8]  OBC mode
  int16_t  roll;             // [9]  ×10
  int16_t  pitch;            // [11] ×10
  int16_t  yaw;              // [13] ×10
  int16_t  temp;             // [15] ×10 (0.1°C)
  int16_t  humidity;         // [17] ×10 (0.1%)
  uint16_t lux;              // [19] lux
  int32_t  gps_lat;          // [21] ×1e7
  int32_t  gps_lon;          // [25] ×1e7
  int16_t  gps_alt;          // [29] meters
  uint8_t  gps_valid;        // [31] 0=no fix
  uint8_t  gps_sats;         // [32] satellites
  int16_t  bus_mv;           // [33] bus voltage mV
  int16_t  bus_ma;           // [35] bus current mA
  int16_t  bus_mw;           // [37] bus power mW
  int16_t  battery_mv;       // [39] battery mV
  int16_t  solar_mv;         // [41] solar mV
  int16_t  solar_ma;         // [43] solar mA
  int16_t  solar_mw;         // [45] solar mW
  int16_t  sun_x;            // [47] ×100
  int16_t  sun_y;            // [49] ×100
  int16_t  mag_x;            // [51] ×100 µT (FR-17)
  int16_t  mag_y;            // [53] ×100 µT
  int16_t  mag_z;            // [55] ×100 µT
  uint16_t radiation;        // [57] Radiation dose
  uint16_t image_count;      // [59] Images on payload SD
  uint8_t  payload_rail_enabled; // [61] 1=rail on
  uint8_t  flags;            // [62] bitmask
  uint8_t  crc;              // [63] CRC-8/MAXIM over bytes 0..62
} telemetry_packet_t;

/* ---- Pinout ---- */
const byte HC12RxdPin = 2;
const byte HC12TxdPin = 3;
const byte HC12SetPin = 9;

/* ---- Buffers ---- */
#define LINE_BUF_SIZE 256
char line_buf[LINE_BUF_SIZE];
byte frame_buf[TLM_PACKET_SIZE];

/* ---- State ---- */
SoftwareSerial HC12(HC12RxdPin, HC12TxdPin);

#define MODE_NORMAL 0
#define MODE_AT     1
byte currentMode = MODE_NORMAL;

/* Frame reception state machine */
byte rx_state = ST_IDLE;
byte rx_pos   = 0;

/* ================================================================
 * CRC-8/MAXIM (Dallas 1-Wire) — matches OBC tlm_crc8()
 * ================================================================ */
static uint8_t crc8_maxim(const uint8_t *data, uint16_t len)
{
  /* LSB-first CRC-8/MAXIM (Dallas 1-Wire) — matches OBC tlm_crc8()
   *
   * Polynomial 0x31 reflected = 0x8C.
   * RefIn=true, RefOut=true, Init=0x00, XorOut=0x00.
   *
   * An MSB-first implementation with poly 0x31 (no reflection) gives
   * a DIFFERENT result — both sides MUST use the same algorithm.
   */
  uint8_t crc = 0;
  for (uint16_t i = 0; i < len; i++)
  {
    uint8_t extract = data[i];
    for (uint8_t j = 8; j; j--)
    {
      uint8_t sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum)
        crc ^= 0x8C;
      extract >>= 1;
    }
  }
  return crc;
}

/* ================================================================
 * Binary frame parser
 * ================================================================ */
void parseBinaryFrame(const telemetry_packet_t *pkt)
{
  /* Decode fixed-point fields */
  float roll  = pkt->roll  / 10.0f;
  float pitch = pkt->pitch / 10.0f;
  float yaw   = pkt->yaw   / 10.0f;
  float temp  = pkt->temp  / 10.0f;
  float hum   = pkt->humidity / 10.0f;
  float gps_lat = pkt->gps_lat / 10000000.0f;
  float gps_lon = pkt->gps_lon / 10000000.0f;
  float sun_x   = pkt->sun_x  / 100.0f;
  float sun_y   = pkt->sun_y  / 100.0f;

  /* Payload HK (FR-17) */
  float mag_x   = pkt->mag_x / 100.0f;
  float mag_y   = pkt->mag_y / 100.0f;
  float mag_z   = pkt->mag_z / 100.0f;

  /* Decode flags */
  bool imu_ok      = (pkt->flags & 0x01) != 0;
  bool temp_ok     = (pkt->flags & 0x02) != 0;
  bool hum_ok      = (pkt->flags & 0x04) != 0;
  bool lux_ok      = (pkt->flags & 0x08) != 0;
  bool rtc_ok      = (pkt->flags & 0x10) != 0;
  int  energy_state = (pkt->flags >> 5) & 0x07;

  /* ---- Print formatted output ---- */
  Serial.print("Mode=");
  Serial.print(pkt->mode);
  Serial.print(" Roll=");
  Serial.print(roll, 1);
  Serial.print(" Pitch=");
  Serial.print(pitch, 1);
  Serial.print(" Yaw=");
  Serial.print(yaw, 1);
  Serial.print(" IMU=");
  Serial.print(imu_ok ? "OK" : "FAIL");

  if (temp_ok) { Serial.print(" Temp="); Serial.print(temp, 1); Serial.print("C"); }
  if (hum_ok)  { Serial.print(" Hum=");  Serial.print(hum, 1);  Serial.print("%"); }
  if (lux_ok)  { Serial.print(" Lux=");  Serial.print(pkt->lux); }

  if (rtc_ok && pkt->rtc > 0)
  {
    Serial.print(" RTC=");
    Serial.print(pkt->rtc);
    Serial.print(" (");
    Serial.print((pkt->rtc / 86400) % 365);
    Serial.print("d ");
    Serial.print((pkt->rtc / 3600) % 24);
    Serial.print("h ");
    Serial.print((pkt->rtc / 60) % 60);
    Serial.print("m)");
  }

  Serial.print(" Sun=[");
  Serial.print(sun_x, 2);
  Serial.print(",");
  Serial.print(sun_y, 2);
  Serial.print("] Energy=");
  Serial.print(energy_state);

  /* Payload HK (FR-17) */
  Serial.print(" Mag=[");
  Serial.print(mag_x, 2);
  Serial.print(",");
  Serial.print(mag_y, 2);
  Serial.print(",");
  Serial.print(mag_z, 2);
  Serial.print("]uT Rad=");
  Serial.print(pkt->radiation);
  Serial.print(" Img=");
  Serial.print(pkt->image_count);
  Serial.print(" Rail=");
  Serial.print(pkt->payload_rail_enabled ? "ON" : "OFF");

  /* GPS */
  Serial.print(" GPS=");
  Serial.print(pkt->gps_valid ? "OK" : "NO FIX");
  Serial.print(" Sats=");
  Serial.print(pkt->gps_sats);
  if (pkt->gps_valid)
  {
    Serial.print(" Lat=");
    Serial.print(gps_lat, 6);
    Serial.print(" Lon=");
    Serial.print(gps_lon, 6);
    Serial.print(" Alt=");
    Serial.print(pkt->gps_alt);
  }

  /* Bus power */
  Serial.print(" V=");
  Serial.print(pkt->bus_mv);
  Serial.print("mV I=");
  Serial.print(pkt->bus_ma);
  Serial.print("mA P=");
  Serial.print(pkt->bus_mw);
  Serial.print("mW");

  /* Battery */
  Serial.print(" Bat=");
  if (pkt->battery_mv > 0) { Serial.print(pkt->battery_mv); Serial.print("mV"); }
  else                     { Serial.print("N/A"); }

  /* Solar */
  if (pkt->solar_mv > 0 || pkt->solar_ma > 0)
  {
    Serial.print(" SolarV=");
    Serial.print(pkt->solar_mv);
    Serial.print("mV SolarI=");
    Serial.print(pkt->solar_ma);
    Serial.print("mA SolarP=");
    Serial.print(pkt->solar_mw);
    Serial.print("mW");
  }

  Serial.print(" ts=");
  Serial.print(pkt->ts);
  Serial.print(" CRC=");
  Serial.print(pkt->crc, HEX);
  Serial.println();
}

/* ================================================================
 * State machine: feed one byte from HC-12
 * Returns true if the byte was consumed by a binary frame.
 * ================================================================ */
bool feedByte(byte b)
{
  switch (rx_state)
  {
    case ST_IDLE:
      if (b == TLM_SYNC_BYTE_1)
      {
        rx_state = ST_GOT_AA;
        return true;
      }
      return false;  // Not binary — caller should treat as text

    case ST_GOT_AA:
      if (b == TLM_SYNC_BYTE_2)
      {
        rx_state = ST_COLLECT;
        rx_pos = 0;
        return true;
      }
      /* False alarm: 0xAA followed by non-0x55 */
      rx_state = ST_IDLE;
      /* The 0xAA was consumed above and is lost — but 0xAA is non-printable
       * so no text data is lost. The current byte (non-0x55) goes back to
       * text processing. */
      return false;

    case ST_COLLECT:
      frame_buf[rx_pos++] = b;
      if (rx_pos >= TLM_PACKET_SIZE)
      {
        rx_state = ST_VERIFY;
      }
      return true;

    case ST_VERIFY:
      /* This shouldn't be reached with actual bytes — VERIFY is resolved
         immediately. But handle gracefully. */
      rx_state = ST_IDLE;
      return false;

    default:
      rx_state = ST_IDLE;
      return false;
  }
}

/* ================================================================
 * Check and parse a complete binary frame
 * Called when rx_state == ST_VERIFY
 * ================================================================ */
void tryParseFrame(void)
{
  if (rx_state != ST_VERIFY) return;
  rx_state = ST_IDLE;

  telemetry_packet_t *pkt = (telemetry_packet_t *)frame_buf;
  uint8_t expected_crc = crc8_maxim(frame_buf, TLM_PACKET_SIZE - 1);

  if (expected_crc == pkt->crc)
  {
    Serial.print(">");  /* Confirmation: entering binary parser */
    parseBinaryFrame(pkt);
  }
  else
  {
    Serial.print("[CRC FAIL] expected=0x");
    Serial.print(expected_crc, HEX);
    Serial.print(" got=0x");
    Serial.print(pkt->crc, HEX);
    Serial.print(" len=");
    Serial.print(HC12.available());
    Serial.print(" frame=");
    for (uint8_t i = 0; i < 8; i++)
    {
      if (frame_buf[i] < 0x10) Serial.print("0");
      Serial.print(frame_buf[i], HEX);
    }
    Serial.print("..");
    for (uint8_t i = TLM_PACKET_SIZE - 4; i < TLM_PACKET_SIZE; i++)
    {
      if (frame_buf[i] < 0x10) Serial.print("0");
      Serial.print(frame_buf[i], HEX);
    }
    Serial.print(" rtc=");
    uint32_t rtc_val = ((uint32_t)frame_buf[4]) | ((uint32_t)frame_buf[5] << 8) |
                       ((uint32_t)frame_buf[6] << 16) | ((uint32_t)frame_buf[7] << 24);
    Serial.print(rtc_val);
    int16_t temp_val = frame_buf[15] | ((int16_t)frame_buf[16] << 8);
    Serial.print(" temp=");
    Serial.print(temp_val);
    Serial.println();
  }
}

/* ================================================================
 * Text line processing (command responses from OBC)
 * ================================================================ */
void processTextLine(const String &line)
{
  if (line.length() == 0) return;

  if (line.startsWith("[CMD]"))
  {
    if (line.indexOf("POWER:") != -1)    { parsePowerTest(line); }
    else if (line.indexOf("SOLAR:") != -1) { parseSolarTest(line); }
    else                                   { Serial.println(line); }
  }
  else if (line.startsWith("SYSTEM:"))   { parseSystemStatus(line); }
  else if (line.startsWith("GPS STATS:")) { parseGpsStats(line); }
  else if (line.startsWith("GPS:"))      { parseGpsStatus(line); }
  else if (line.startsWith("ULTS:"))     { parseFaults(line); }
  else                                   { Serial.println(line); }
}

/* ================================================================
 * Arduino setup / loop
 * ================================================================ */
void setup()
{
  pinMode(HC12SetPin, OUTPUT);
  digitalWrite(HC12SetPin, HIGH);

  Serial.begin(9600);
  HC12.begin(9600);

  Serial.println("=== Ground Station Ready (Binary Protocol) ===");
  Serial.println(" Commands: HELP|AT|STATUS|REBOOT|ECHO|CAPTURE|MODE=<0-5|name>|DEPLOY|DEPLOYCLEAR|GPS|GPSSTATS|RESETGPS|RESETGPS COLD");
  Serial.println("           FAULTS|LOG|I2CSCAN|BH1750_TEST|RTC_TEST|POWER_TEST|SOLAR_TEST|SHT31_TEST|SETTIME");
  Serial.println("           MAG-CAL-START|MAG-CAL-STOP|MAG-CAL-STATUS|IMU-CAL-START|IMU-CAL-STOP|IMU-CAL-STATUS|IMU-CAL-SAVE|IMU-CAL-LOAD");
  Serial.println(" Binary telemetry auto-detected from OBC.");
}

void loop()
{
  /* ---- Serial input: user commands → HC-12 ---- */
  if (currentMode == MODE_AT)
  {
    if (Serial.available())
    {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd.length() > 0)
      {
        HC12.println(cmd);
        Serial.println("-> " + cmd);
      }
    }
    if (HC12.available())
    {
      Serial.write(HC12.read());
    }
    return;
  }

  if (Serial.available())
  {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.length() > 0)
    {
      if (comando == "AT")
      {
        enterAtMode();
      }
      else
      {
        HC12.println(comando);
        Serial.println("-> Enviado: " + comando);
      }
    }
  }

  /* ---- HC-12 data: binary frames + text lines ---- */
  while (HC12.available())
  {
    byte b = HC12.read();

    /* Feed into state machine — if it matches a binary frame, done */
    if (feedByte(b))
    {
      /* If we just completed a frame, parse it */
      if (rx_state == ST_VERIFY)
      {
        tryParseFrame();
      }
      continue;
    }

    /* Not binary — treat as text byte */
    static int line_pos = 0;

    if (b == '\n' || line_pos >= LINE_BUF_SIZE - 1)
    {
      line_buf[line_pos] = '\0';
      String line = String(line_buf);
      line.trim();
      line_pos = 0;
      processTextLine(line);
    }
    else if (b != '\r')
    {
      line_buf[line_pos++] = (char)b;
    }
    /* On '\r', just skip */
  }

  /* Check for pending frame in case all bytes arrived in one burst */
  if (rx_state == ST_VERIFY)
  {
    tryParseFrame();
  }
}

/* ================================================================
 * AT mode
 * ================================================================ */
void enterAtMode()
{
  Serial.println("=== Modo AT ===");
  digitalWrite(HC12SetPin, LOW);
  currentMode = MODE_AT;
  delay(100);
}

/* ================================================================
 * Text response parsers (unchanged from original)
 * ================================================================ */

void parsePowerTest(const String &msg)
{
  int posV = msg.indexOf("V=");
  int posI = msg.indexOf("I=");
  int posP = msg.indexOf("P=");
  if (posV != -1 && posI != -1 && posP != -1)
  {
    int v = msg.substring(posV + 2, msg.indexOf(' ', posV + 2)).toInt();
    int i = msg.substring(posI + 2, msg.indexOf(' ', posI + 2)).toInt();
    int p = msg.substring(posP + 2, msg.indexOf(' ', posP + 2)).toInt();
    Serial.print("POWER: V=");
    Serial.print(v);
    Serial.print(" mV, I=");
    Serial.print(i);
    Serial.print(" mA, P=");
    Serial.print(p);
    Serial.println(" mW");
  }
  else
  {
    Serial.println(msg);
  }
}

void parseSolarTest(const String &msg)
{
  int posV = msg.indexOf("V=");
  int posI = msg.indexOf("I=");
  int posP = msg.indexOf("P=");
  if (posV != -1 && posI != -1 && posP != -1)
  {
    int v = msg.substring(posV + 2, msg.indexOf(' ', posV + 2)).toInt();
    int i = msg.substring(posI + 2, msg.indexOf(' ', posI + 2)).toInt();
    int p = msg.substring(posP + 2, msg.indexOf(' ', posP + 2)).toInt();
    Serial.print("SOLAR PANEL: V=");
    Serial.print(v);
    Serial.print(" mV, I=");
    Serial.print(i);
    Serial.print(" mA, P=");
    Serial.print(p);
    Serial.println(" mW");
  }
  else
  {
    Serial.println(msg);
  }
}

void parseGpsStatus(const String &msg)
{
  int valid    = getValue(msg, "v=").toInt();
  float lat    = getValue(msg, "lat=").toFloat();
  float lon    = getValue(msg, "lon=").toFloat();
  float alt    = getValue(msg, "alt=").toFloat();
  int sats     = getValue(msg, "s=").toInt();
  float hdop   = getValue(msg, "hdop=").toFloat();

  Serial.print("GPS FIX: ");
  Serial.print(valid ? "VALID" : "NO FIX");
  if (valid)
  {
    Serial.print(" Lat=");
    Serial.print(lat, 5);
    Serial.print(" Lon=");
    Serial.print(lon, 5);
    Serial.print(" Alt=");
    Serial.print(alt, 1);
    Serial.print(" Sats=");
    Serial.print(sats);
    Serial.print(" HDOP=");
    Serial.print(hdop, 1);
  }
  Serial.println();
}

void parseGpsStats(const String &msg)
{
  unsigned long rx       = (unsigned long)getValue(msg, "rx=").toInt();
  unsigned long chk_err  = (unsigned long)getValue(msg, "chk_err=").toInt();
  unsigned long inv      = (unsigned long)getValue(msg, "inv=").toInt();
  unsigned long valid    = (unsigned long)getValue(msg, "valid=").toInt();
  unsigned long overflow = (unsigned long)getValue(msg, "overflow=").toInt();

  Serial.print("GPS STATS: rx=");
  Serial.print(rx);
  Serial.print(" err=");
  Serial.print(chk_err);
  Serial.print(" inv=");
  Serial.print(inv);
  Serial.print(" valid=");
  Serial.print(valid);
  Serial.print(" overflow=");
  Serial.print(overflow);
  Serial.println();
}

void parseSystemStatus(const String &msg)
{
  String modeStr   = getValue(msg, "mode=");
  String energyStr = getValue(msg, "energy=");
  String imuStr    = getValue(msg, "imu=");
  String tempStr   = getValue(msg, "temp=");
  String magStr    = getValue(msg, "mag=");

  Serial.print("SYSTEM: mode=");
  Serial.print(modeStr);
  Serial.print(" energy=");
  Serial.print(energyStr);
  Serial.print(" IMU=");
  Serial.print(imuStr);
  Serial.print(" Temp=");
  Serial.print(tempStr);
  Serial.print(" Mag=");
  Serial.print(magStr);
  Serial.println();
}

void parseFaults(const String &msg)
{
  String levelStr = getValue(msg, "ULTS: ");
  Serial.print("ULTS: ");
  Serial.println(levelStr);
}

/* ================================================================
 * Helper: extract value by key ("key=value ...")
 * ================================================================ */
String getValue(const String &msg, const String &key)
{
  int pos = msg.indexOf(key);
  if (pos == -1) return "0";
  int start = pos + key.length();
  int end = msg.indexOf(' ', start);
  if (end == -1) end = msg.length();
  return msg.substring(start, end);
}
