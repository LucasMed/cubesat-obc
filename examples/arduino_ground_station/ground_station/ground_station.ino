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
  Serial.println(" Commands: AT|STATUS|REBOOT|ECHO|CAPTURE|MODE=0-3|GPS|FAULTS|LOG|RESET|HELP|I2CSCAN|BH1750_TEST|RTC_TEST|POWER_TEST|SHT31_TEST|TLMFMT|TLMFMT=JSON|TLMFMT=TEXT");
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

    // Detect format: JSON starts with '{', TEXT starts with '[TLM]'
    if (recibido.startsWith("{"))
    {
      // JSON format
      parseTelemetryJson(recibido);
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
      else
      {
        Serial.println(recibido);
      }
    }
    else if (recibido.startsWith("SYSTEM:"))
    {
      parseSystemStatus(recibido);
    }
    else if (recibido.startsWith("FAULTS:"))
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
  unsigned long rtc = getValue(msg, "r=").toInt();

  String flagStr = getValue(msg, "f=0x");
  int flags = (int)strtol(("0x" + flagStr).c_str(), NULL, 16);

  bool imu_ok = (flags & 0x01) != 0;
  bool temp_ok = (flags & 0x02) != 0;
  bool humidity_ok = (flags & 0x04) != 0;
  bool lux_ok = (flags & 0x08) != 0;
  bool rtc_ok = (flags & 0x10) != 0;
  bool sun_ok = (flags & 0x20) != 0;
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

  float sun_x = getValue(msg, "sx=").toFloat();
  float sun_y = getValue(msg, "sy=").toFloat();
  
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
  Serial.print(sun_ok ? sun_x : -1, 2);
  Serial.print(",");
  Serial.print(sun_ok ? sun_y : -1, 2);
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
  Serial.print("FAULTS: ");
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

// Parse JSON telemetry format for simulator
void parseTelemetryJson(String msg)
{
  // Extract values using simple string parsing (no external libraries)
  float ts = getJsonValue(msg, "ts").toFloat();
  int mode = getJsonValue(msg, "mode").toInt();
  
  // Attitude: {"r":0.50,"p":-1.20,"y":45.30}
  float roll = getJsonValue(msg, "r").toFloat();
  float pitch = getJsonValue(msg, "p").toFloat();
  float yaw = getJsonValue(msg, "y").toFloat();
  
  // Environment
  float temp = getJsonValue(msg, "temp").toFloat();
  float humidity = getJsonValue(msg, "humidity").toFloat();
  float lux = getJsonValue(msg, "lux").toFloat();
  
  // GPS
  float gps_lat = getJsonValue(msg, "lat").toFloat();
  float gps_lon = getJsonValue(msg, "lon").toFloat();
  float gps_alt = getJsonValue(msg, "alt").toFloat();
  int gps_valid = getJsonValue(msg, "valid").toInt();
  int sats = getJsonValue(msg, "sats").toInt();
  
  // Power
  int volt = getJsonValue(msg, "volt").toInt();
  int curr = getJsonValue(msg, "curr").toInt();
  int pow = getJsonValue(msg, "pow").toInt();
  
  // Sun sensor - get from "sun" object
  float sun_x = getJsonValueIn(msg, "sun", "x").toFloat();
  float sun_y = getJsonValueIn(msg, "sun", "y").toFloat();
  
  // CRC8 (optional)
  String crc_recv = getJsonValue(msg, "crc");
  if (crc_recv.length() > 0)
  {
    crc_recv.replace("\"", "");
  }
  
  // Flags
  int flags = getJsonValue(msg, "flags").toInt();
  bool imu_ok = (flags & 0x01) != 0;
  bool temp_ok = (flags & 0x02) != 0;
  bool humidity_ok = (flags & 0x04) != 0;
  bool lux_ok = (flags & 0x08) != 0;
  bool rtc_ok = (flags & 0x10) != 0;
  bool sun_ok = (flags & 0x20) != 0;
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
  Serial.print(lux_ok ? lux : -1, 0);
  Serial.print(" Sun=[");
  Serial.print(sun_ok ? sun_x : -1, 2);
  Serial.print(",");
  Serial.print(sun_ok ? sun_y : -1, 2);
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
    Serial.print(gps_alt, 0);
  }
  Serial.print(" V=");
  Serial.print(volt);
  Serial.print("mV I=");
  Serial.print(curr);
  Serial.print("mA P=");
  Serial.print(pow);
  Serial.print("mW ts=");
  Serial.print(ts, 0);
  Serial.println();
}

// Simple JSON value extractor - improved to handle duplicates
String getJsonValue(String msg, String key)
{
  // Build search pattern based on key location
  String search = "\"" + key + "\":";
  int pos = msg.indexOf(search);
  if (pos == -1) return "0";
  
  int start = pos + search.length();
  int end = start;
  while (end < msg.length())
  {
    char c = msg.charAt(end);
    if (c == ',' || c == '}' || c == ']')
    {
      break;
    }
    end++;
  }
  
  String val = msg.substring(start, end);
  val.trim();
  return val;
}

// Get JSON value from within a specific parent object (e.g., "sun":{...})
String getJsonValueIn(String msg, String parent, String key)
{
  // Find parent object: "parent":{
  String parentSearch = "\"" + parent + "\":{";
  int parentPos = msg.indexOf(parentSearch);
  if (parentPos == -1) return "0";
  
  // Start after the opening brace
  int searchStart = parentPos + parentSearch.length();
  
  // Find the key within this section
  String keySearch = "\"" + key + "\":";
  int keyPos = msg.indexOf(keySearch, searchStart);
  if (keyPos == -1) return "0";
  
  // Find end of key: value
  int valueStart = keyPos + keySearch.length();
  int valueEnd = valueStart;
  while (valueEnd < msg.length())
  {
    char c = msg.charAt(valueEnd);
    if (c == ',' || c == '}')
    {
      break;
    }
    valueEnd++;
  }
  
  String val = msg.substring(valueStart, valueEnd);
  val.trim();
  return val;
}