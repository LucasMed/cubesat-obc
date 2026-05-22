#include <SoftwareSerial.h>

const byte HC12RxdPin = 2;
const byte HC12TxdPin = 3;
const byte HC12SetPin = 9;

SoftwareSerial HC12(HC12RxdPin, HC12TxdPin);

#define MODE_NORMAL 0
#define MODE_AT 1
byte currentMode = MODE_NORMAL;

// Telemetry format: 0=TEXT, 1=JSON
byte tlmFormat = 0;

void setup()
{
  pinMode(HC12SetPin, OUTPUT);
  digitalWrite(HC12SetPin, HIGH);

  Serial.begin(9600);
  HC12.begin(9600);

  Serial.println("=== Ground Station Ready ===");
  Serial.println(" Commands: AT|STATUS|REBOOT|ECHO|CAPTURE|MODE=0-3|GPS|FAULTS|LOG|RESET|HELP|I2CSCAN|BH1750_TEST|RTC_TEST|POWER_TEST|SOLAR_TEST|SHT31_TEST|TLMFMT|TLMFMT=JSON|TLMFMT=TEXT");
  Serial.println(" Para modo AT: escribe 'AT' y presiona Enter");
  Serial.println(" Para formato JSON: escribe 'TLMFMT=JSON'");
}

void loop()
{
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

  if (HC12.available())
  {
    String recibido = HC12.readStringUntil('\n');
    recibido.trim();

    if (recibido.length() == 0) return;

    // Detect format: JSON starts with '[', TEXT starts with '['
    if (recibido.startsWith("[JSON]"))
    {
      // JSON format - remove prefix and parse
      String jsonMsg = recibido.substring(7);
      parseTelemetryJson(jsonMsg);
    }
    else if (recibido.startsWith("[TLM]"))
    {
      parseTelemetry(recibido);
    }
    else if (recibido.startsWith("[CMD]"))
    {
      // Check if it's a power test response
      if (recibido.indexOf("POWER:") != -1)
      {
        parsePowerTest(recibido);
      }
      else if (recibido.indexOf("SOLAR:") != -1)
      {
        parseSolarTest(recibido);
      }
      else
      {
        Serial.println(recibido);
      }
    }
    else if (recibido.startsWith("SYSTEM:"))
    {
      parseSystemStatus(recibido);
    }
    else if (recibido.startsWith("ULTS:"))
    {
      parseFaults(recibido);
    }
    else if (recibido.startsWith("GPS:"))
    {
      parseGpsStatus(recibido);
    }
    else if (recibido.startsWith("GPS STATS:"))
    {
      parseGpsStats(recibido);
    }
    else
    {
      Serial.println(recibido);
    }
  }
}

void enterAtMode()
{
  Serial.println("=== Modo AT ===");
  digitalWrite(HC12SetPin, LOW);
  currentMode = MODE_AT;
  delay(100);
}

void parseTelemetry(String msg)
{
  // Compact format: m=mode a=r,p,y t=temp h=humidity l=lux r=rtc f=flags g=lat,lon,alt v=valid s=sats
  int mode = getValue(msg, "m=").toInt();
  
  String attStr = getValue(msg, "a=");
  int comma1 = attStr.indexOf(',');
  int comma2 = attStr.lastIndexOf(',');
  float roll = attStr.substring(0, comma1).toFloat();
  float pitch = attStr.substring(comma1 + 1, comma2).toFloat();
  float yaw = attStr.substring(comma2 + 1).toFloat();

  float temp = getValue(msg, "t=").toFloat();
  float humidity = getValue(msg, "h=").toFloat();
  float lux = getValue(msg, "l=").toFloat();
  float sun_x = getValue(msg, "sx=").toFloat();
  float sun_y = getValue(msg, "sy=").toFloat();
  unsigned long rtc = getValue(msg, "r=").toInt();

  String flagStr = getValue(msg, "f=0x");
  int flags = (int)strtol(("0x" + flagStr).c_str(), NULL, 16);

  bool imu_ok = (flags & 0x01) != 0;
  bool temp_ok = (flags & 0x02) != 0;
  bool humidity_ok = (flags & 0x04) != 0;
  bool lux_ok = (flags & 0x08) != 0;
  bool rtc_ok = (flags & 0x10) != 0;
  int energy_state = (flags >> 5) & 0x07;

  // GPS: g=lat,lon,alt
  String gpsStr = getValue(msg, "g=");
  int g_comma1 = gpsStr.indexOf(',');
  int g_comma2 = gpsStr.lastIndexOf(',');
  float gps_lat = gpsStr.substring(0, g_comma1).toFloat();
  float gps_lon = gpsStr.substring(g_comma1 + 1, g_comma2).toFloat();
  float gps_alt = gpsStr.substring(g_comma2 + 1).toFloat();
  int gps_valid = getValue(msg, "v=").toInt();
  int sats = getValue(msg, "s=").toInt();
  
  // Power (bus): p=V,I,P
  String pwrStr = getValue(msg, "p=");
  int bus_v = 0, bus_i = 0, bus_p = 0;
  if (pwrStr.length() > 0 && pwrStr != "0")
  {
    int c1 = pwrStr.indexOf(',');
    int c2 = pwrStr.lastIndexOf(',');
    if (c1 != -1 && c2 != -1)
    {
      bus_v = pwrStr.substring(0, c1).toInt();
      bus_i = pwrStr.substring(c1 + 1, c2).toInt();
      bus_p = pwrStr.substring(c2 + 1).toInt();
    }
  }
  
  // Battery: b=BAT_mV
  int battery_mv = getValue(msg, "b=").toInt();
  
  // Solar panel: sp=V,I,P
  String solarStr = getValue(msg, "sp=");
  int solar_v = 0, solar_i = 0, solar_p = 0;
  if (solarStr.length() > 0 && solarStr != "0")
  {
    int c1 = solarStr.indexOf(',');
    int c2 = solarStr.lastIndexOf(',');
    if (c1 != -1 && c2 != -1)
    {
      solar_v = solarStr.substring(0, c1).toInt();
      solar_i = solarStr.substring(c1 + 1, c2).toInt();
      solar_p = solarStr.substring(c2 + 1).toInt();
    }
  }
  
  // CRC (optional, format: c=XX)
  String crc_recv = getValue(msg, "c=");
  
  Serial.print("Mode=");
  Serial.print(mode);
  Serial.print(" Roll=");
  Serial.print(roll);
  Serial.print(" Pitch=");
  Serial.print(pitch);
  Serial.print(" Yaw=");
  Serial.print(yaw);
  Serial.print(" IMU=");
  Serial.print(imu_ok ? "OK" : "FAIL");
  Serial.print(" Temp=");
  Serial.print(temp_ok ? temp : -1, 1);
  Serial.print("C");
  Serial.print(" Hum=");
  Serial.print(humidity_ok ? humidity : -1, 1);
  Serial.print("%");
  Serial.print(" Lux=");
  Serial.print(lux_ok ? lux : -1, 1);
  Serial.print(" RTC=");
  Serial.print(rtc_ok ? rtc : 0);
  Serial.print(" Sun=[");
  Serial.print(sun_x, 2);
  Serial.print(",");
  Serial.print(sun_y, 2);
  Serial.print("]");
  Serial.print(" Energy=");
  Serial.print(energy_state);
  Serial.print(" GPS=");
  Serial.print(gps_valid ? "OK" : "NO FIX");
  Serial.print(" Sats=");
  Serial.print(sats);
  if (gps_valid)
  {
    Serial.print(" Lat=");
    Serial.print(gps_lat, 6);
    Serial.print(" Lon=");
    Serial.print(gps_lon, 6);
    Serial.print(" Alt=");
    Serial.print(gps_alt, 1);
  }
  Serial.print(" V=");
  Serial.print(bus_v);
  Serial.print("mV I=");
  Serial.print(bus_i);
  Serial.print("mA P=");
  Serial.print(bus_p);
  Serial.print("mW");
  Serial.print(" Bat=");
  Serial.print(battery_mv);
  Serial.print("mV");
  if (solar_v > 0 || solar_i > 0)
  {
    Serial.print(" SolarV=");
    Serial.print(solar_v);
    Serial.print("mV SolarI=");
    Serial.print(solar_i);
    Serial.print("mA SolarP=");
    Serial.print(solar_p);
    Serial.print("mW");
  }
  Serial.println();
}

void parsePowerTest(String msg)
{
  // Format: [CMD] POWER: V=5728 mV, I=5 mA, P=28 mW
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

void parseSolarTest(String msg)
{
  // Format: [CMD] SOLAR: V=896 mV, I=0 mA, P=0 mW
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

void parseGpsStatus(String msg)
{
  int valid = getValue(msg, "v=").toInt();
  float lat = getValue(msg, "lat=").toFloat();
  float lon = getValue(msg, "lon=").toFloat();
  float alt = getValue(msg, "alt=").toFloat();
  int sats = getValue(msg, "s=").toInt();
  float hdop = getValue(msg, "hdop=").toFloat();

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

void parseGpsStats(String msg)
{
  unsigned long rx = getValue(msg, "rx=").toInt();
  unsigned long chk_err = getValue(msg, "chk_err=").toInt();
  unsigned long inv = getValue(msg, "inv=").toInt();
  unsigned long valid = getValue(msg, "valid=").toInt();
  unsigned long overflow = getValue(msg, "overflow=").toInt();

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

void parseSystemStatus(String msg)
{
  String modeStr = getValue(msg, "mode=");
  String energyStr = getValue(msg, "energy=");
  String imuStr = getValue(msg, "imu=");
  String tempStr = getValue(msg, "temp=");
  String magStr = getValue(msg, "mag=");

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

void parseFaults(String msg)
{
  String levelStr = getValue(msg, "ULTS: ");
  Serial.print("ULTS: ");
  Serial.println(levelStr);
}

String getValue(String msg, String key)
{
  int pos = msg.indexOf(key);
  if (pos == -1) return "0";
  int start = pos + key.length();
  int end = msg.indexOf(' ', start);
  if (end == -1) end = msg.length();
  return msg.substring(start, end);
}

// Get JSON value by Nth colon-separated value (0-indexed)
// The keys are in FIXED order: ts,r,m,a,t,h,l,g,v,s,p,b,sp,sx,sy,f,c
String getJsonValueByNth(String msg, int n)
{
  // Find the nth colon
  int colonPos = -1;
  for (int i = 0; i <= n; i++)
  {
    colonPos = (i == 0) ? msg.indexOf(':') : msg.indexOf(':', colonPos + 1);
    if (colonPos == -1) return "0";
  }
  
  // colonPos now at the nth colon
  int start = colonPos + 1;
  while (start < msg.length() && msg.charAt(start) == ' ') start++;
  
  // Find end: use nth+1 colon position minus 1
  // Or if no next colon, find closing brace
  int searchStart = colonPos + 1;
  int nextColonPos = msg.indexOf(':', searchStart);
  int end;
  
  if (nextColonPos != -1)
  {
    // Back up to find comma before next colon
    end = nextColonPos;
    while (end > start && (msg.charAt(end - 1) == ' ' || msg.charAt(end - 1) == ',')) end--;
  }
  else
  {
    // No next colon - go to closing brace
    end = start;
    while (end < msg.length() && msg.charAt(end) != '}') end++;
  }
  
  return msg.substring(start, end);
}

// Helper: safely extract value between two delimiters, returns default if not found
String safeExtract(String msg, String key, String endDelim, String def)
{
  int pos = msg.indexOf(key);
  if (pos == -1) return def;
  int start = pos + key.length();
  int end = msg.indexOf(endDelim, start);
  if (end == -1) return def;
  return msg.substring(start, end);
}

// Parse JSON telemetry format — handles corrupted/concatenated frames gracefully
void parseTelemetryJson(String msg)
{
  // Validate basic frame structure
  if (msg.length() < 20 || msg.charAt(0) != '{') {
    // Corrupted or empty frame — skip silently
    return;
  }
  // Detect concatenated frames (two JSONs glued together)
  if (msg.indexOf('{', 1) != -1) {
    return;  // Skip — frame boundary issue
  }
  
  // All values default to 0/false — only overwrite when key is found cleanly
  float ts = 0;
  unsigned long rtc = 0;
  int mode = 0;
  float roll = 0, pitch = 0, yaw = 0;
  float temp = 0, humidity = 0, lux_value = 0;
  float gps_lat = 0, gps_lon = 0, gps_alt = 0;
  int gps_valid_str = 0, sats = 0;
  int volt = 0, curr = 0, power = 0;
  int battery_mv = 0;
  int solar_v = 0, solar_i = 0, solar_p = 0;
  float sun_x = 0, sun_y = 0;
  int flags = 0;
  String crc_recv = "";
  
  // ts — first key
  String tsStr = safeExtract(msg, "ts:", ",", "0");
  ts = tsStr.toFloat();
  
  // r — RTC
  String rStr = safeExtract(msg, ",r:", ",m:", "0");
  rtc = (unsigned long)rStr.toInt();
  
  // m — mode
  String mStr = safeExtract(msg, ",m:", ",a:", "0");
  mode = mStr.toInt();
  
  // a — attitude (multi-value: roll,pitch,yaw separated by commas)
  String attStr = safeExtract(msg, ",a:", ",t:", "");
  if (attStr.length() > 0)
  {
    int c1 = attStr.indexOf(',');
    int c2 = attStr.lastIndexOf(',');
    if (c1 != -1 && c2 != -1 && c2 > c1)
    {
      roll = attStr.substring(0, c1).toFloat();
      pitch = attStr.substring(c1 + 1, c2).toFloat();
      yaw = attStr.substring(c2 + 1).toFloat();
    }
  }
  
  // t — temperature
  String tStr = safeExtract(msg, ",t:", ",h:", "0");
  temp = tStr.toFloat();
  
  // h — humidity
  String hStr = safeExtract(msg, ",h:", ",l:", "0");
  humidity = hStr.toFloat();
  
  // l — lux
  String lStr = safeExtract(msg, ",l:", ",g:", "0");
  lux_value = lStr.toFloat();
  
  // g — GPS (multi-value: lat,lon,alt)
  String gpsStr = safeExtract(msg, ",g:", ",v:", "");
  if (gpsStr.length() > 0)
  {
    int c1 = gpsStr.indexOf(',');
    int c2 = gpsStr.lastIndexOf(',');
    if (c1 != -1 && c2 != -1 && c2 > c1)
    {
      gps_lat = gpsStr.substring(0, c1).toFloat();
      gps_lon = gpsStr.substring(c1 + 1, c2).toFloat();
      gps_alt = gpsStr.substring(c2 + 1).toFloat();
    }
  }
  
  // v — GPS valid
  String vStr = safeExtract(msg, ",v:", ",s:", "0");
  gps_valid_str = vStr.toInt();
  
  // s — GPS satellites
  String sStr = safeExtract(msg, ",s:", ",p:", "0");
  sats = sStr.toInt();
  
  // p — bus power (multi-value: V,I,P)
  String pwrStr = safeExtract(msg, ",p:", ",b:", "");
  if (pwrStr.length() > 0)
  {
    int c1 = pwrStr.indexOf(',');
    int c2 = pwrStr.lastIndexOf(',');
    if (c1 != -1 && c2 != -1 && c2 > c1)
    {
      volt = pwrStr.substring(0, c1).toInt();
      curr = pwrStr.substring(c1 + 1, c2).toInt();
      power = pwrStr.substring(c2 + 1).toInt();
    }
  }
  
  // b — battery
  String bStr = safeExtract(msg, ",b:", ",sp:", "0");
  battery_mv = bStr.toInt();
  
  // sp — solar power (multi-value: V,I,P)
  String solarStr = safeExtract(msg, ",sp:", ",sx:", "");
  if (solarStr.length() > 0)
  {
    int c1 = solarStr.indexOf(',');
    int c2 = solarStr.lastIndexOf(',');
    if (c1 != -1 && c2 != -1 && c2 > c1)
    {
      solar_v = solarStr.substring(0, c1).toInt();
      solar_i = solarStr.substring(c1 + 1, c2).toInt();
      solar_p = solarStr.substring(c2 + 1).toInt();
    }
  }
  
  // sx — sun X
  String sxStr = safeExtract(msg, ",sx:", ",sy:", "0");
  sun_x = sxStr.toFloat();
  
  // sy — sun Y
  String syStr = safeExtract(msg, ",sy:", ",f:", "0");
  sun_y = syStr.toFloat();
  
  // f — flags
  String fStr = safeExtract(msg, ",f:", ",c:", "0");
  flags = fStr.toInt();
  
  // c — CRC (2 hex chars before closing brace)
  int cPos = msg.indexOf(",c:");
  if (cPos != -1)
  {
    int cStart = cPos + 3;
    // CRC value is 2 hex chars, then '}'
    if (cStart + 2 <= msg.length())
    {
      crc_recv = msg.substring(cStart, cStart + 2);
    }
  }
  
  // Decode flags
  bool imu_ok = (flags & 0x01) != 0;
  bool temp_ok = (flags & 0x02) != 0;
  bool humidity_ok = (flags & 0x04) != 0;
  bool lux_ok = (flags & 0x08) != 0;
  bool rtc_ok = (flags & 0x10) != 0;
  int energy_state = (flags >> 5) & 0x07;
  
  // Print formatted output
  Serial.print("JSON Mode=");
  Serial.print(mode);
  Serial.print(" Roll=");
  Serial.print(roll, 1);
  Serial.print(" Pitch=");
  Serial.print(pitch, 1);
  Serial.print(" Yaw=");
  Serial.print(yaw, 1);
  Serial.print(" IMU=");
  Serial.print(imu_ok ? "OK" : "FAIL");
  Serial.print(" Temp=");
  Serial.print(temp_ok ? temp : -1, 1);
  Serial.print("C");
  Serial.print(" Hum=");
  Serial.print(humidity_ok ? humidity : -1, 1);
  Serial.print("%");
  Serial.print(" Lux=");
  Serial.print(lux_ok ? lux_value : -1, 0);
  Serial.print(" Sun=[");
  Serial.print(sun_x, 2);
  Serial.print(",");
  Serial.print(sun_y, 2);
  Serial.print("]");
  Serial.print(" Energy=");
  Serial.print(energy_state);
  Serial.print(" GPS=");
  Serial.print(gps_valid_str ? "OK" : "NO FIX");
  Serial.print(" Sats=");
  Serial.print(sats);
  if (gps_valid_str)
  {
    Serial.print(" Lat=");
    Serial.print(gps_lat, 6);
    Serial.print(" Lon=");
    Serial.print(gps_lon, 6);
    Serial.print(" Alt=");
    Serial.print(gps_alt, 0);
  }
  Serial.print(" V=");
  Serial.print(volt);
  Serial.print("mV I=");
  Serial.print(curr);
  Serial.print("mA P=");
  Serial.print(power);
  Serial.print("mW");
  Serial.print(" Bat=");
  Serial.print(battery_mv);
  Serial.print("mV");
  if (solar_v > 0 || solar_i > 0)
  {
    Serial.print(" SolarV=");
    Serial.print(solar_v);
    Serial.print("mV SolarI=");
    Serial.print(solar_i);
    Serial.print("mA SolarP=");
    Serial.print(solar_p);
    Serial.print("mW");
  }
  Serial.print(" RTC=");
  if (rtc > 0)
  {
    Serial.print(rtc);
    Serial.print(" (");
    Serial.print((rtc / 86400) % 365);
    Serial.print("d ");
    Serial.print((rtc / 3600) % 24);
    Serial.print("h ");
    Serial.print((rtc / 60) % 60);
    Serial.print("m)");
  }
  else
  {
    Serial.print("N/A");
  }
  Serial.print(" ts=");
  Serial.print(ts, 0);
  Serial.print(" CRC=");
  Serial.print(crc_recv);
  Serial.println();
}