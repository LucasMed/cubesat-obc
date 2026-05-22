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

// Parse JSON telemetry format - robust simple parser
void parseTelemetryJson(String msg)
{
  // Find each value by its unique key prefix
  // Keys in order: ts, r, m, a, t, h, l, g, v, s, p, b, sp, sx, sy, f, c
  
  // ts - find "ts:" at start
  int tsPos = msg.indexOf("ts:");
  int tsStart = tsPos + 3;
  int tsEnd = msg.indexOf(',', tsStart);
  float ts = msg.substring(tsStart, tsEnd).toFloat();
  
   // r - RTC: ",r:" to ",m:"
   int rPos = msg.indexOf(",r:");
   int rStart = rPos + 3;
   int rEnd = msg.indexOf(",m:", rStart);
   unsigned long rtc = msg.substring(rStart, rEnd).toInt();
   
   // m - next key after r: ",m:VAL," 
   int mPos = msg.indexOf(",m:");
  int mStart = mPos + 3;
  int mEnd = msg.indexOf(',', mStart);
  int mode = msg.substring(mStart, mEnd).toInt();
  
  // a - attitude: ",a:" to ",t:"
  int aPos = msg.indexOf(",a:");
  int aStart = aPos + 3;
  int aEnd = msg.indexOf(",t:", aStart);
  String attStr = msg.substring(aStart, aEnd);
  
  // Parse attitude: r,p,y
  float roll = 0, pitch = 0, yaw = 0;
  if (attStr.indexOf(',') != -1)
  {
    int c1 = attStr.indexOf(',');
    int c2 = attStr.lastIndexOf(',');
    roll = attStr.substring(0, c1).toFloat();
    pitch = attStr.substring(c1 + 1, c2).toFloat();
    yaw = attStr.substring(c2 + 1).toFloat();
  }
  
  // t - temperature: ",t:" to ",h:"
  int tPos = msg.indexOf(",t:");
  int tStart = tPos + 3;
  int tEnd = msg.indexOf(",h:", tStart);
  float temp = msg.substring(tStart, tEnd).toFloat();
  
  // h - humidity: ",h:" to ",l:"
  int hPos = msg.indexOf(",h:");
  int hStart = hPos + 3;
  int hEnd = msg.indexOf(",l:", hStart);
  float humidity = msg.substring(hStart, hEnd).toFloat();
  
  // l - lux: ",l:" to ",g:"
  int lPos = msg.indexOf(",l:");
  int lStart = lPos + 3;
  int lEnd = msg.indexOf(",g:", lStart);
  float lux_value = msg.substring(lStart, lEnd).toFloat();
  
  // g - GPS: ",g:" to ",v:"
  int gPos = msg.indexOf(",g:");
  int gStart = gPos + 3;
  int gEnd = msg.indexOf(",v:", gStart);
  String gpsStr = msg.substring(gStart, gEnd);
  float gps_lat = 0, gps_lon = 0, gps_alt = 0;
  if (gpsStr.indexOf(',') != -1)
  {
    int c1 = gpsStr.indexOf(',');
    int c2 = gpsStr.lastIndexOf(',');
    gps_lat = gpsStr.substring(0, c1).toFloat();
    gps_lon = gpsStr.substring(c1 + 1, c2).toFloat();
    gps_alt = gpsStr.substring(c2 + 1).toFloat();
  }
  
  // v - valid: ",v:" to ",s:"
  int vPos = msg.indexOf(",v:");
  int vStart = vPos + 3;
  int vEnd = msg.indexOf(",s:", vStart);
  int gps_valid_str = msg.substring(vStart, vEnd).toInt();
  
  // s - sats: ",s:" to ",p:"
  int sPos = msg.indexOf(",s:");
  int sStart = sPos + 3;
  int sEnd = msg.indexOf(",p:", sStart);
  int sats = msg.substring(sStart, sEnd).toInt();
  
   // p - power (bus): ",p:" to ",b:"
   int pPos = msg.indexOf(",p:");
   int pStart = pPos + 3;
   int pEnd = msg.indexOf(",b:", pStart);
   String pwrStr = msg.substring(pStart, pEnd);
   int volt = 0, curr = 0, power = 0;
   if (pwrStr.indexOf(',') != -1)
   {
     int c1 = pwrStr.indexOf(',');
     int c2 = pwrStr.lastIndexOf(',');
     volt = pwrStr.substring(0, c1).toInt();
     curr = pwrStr.substring(c1 + 1, c2).toInt();
     power = pwrStr.substring(c2 + 1).toInt();
   }
   
   // b - battery: ",b:" to ",sp:"
   int bPos = msg.indexOf(",b:");
   int bStart = bPos + 3;
   int bEnd = msg.indexOf(",sp:", bStart);
   int battery_mv = msg.substring(bStart, bEnd).toInt();
   
   // sp - solar panel: ",sp:" to ",sx:"
  int spPos = msg.indexOf(",sp:");
  int spStart = spPos + 4;
  int spEnd = msg.indexOf(",sx:", spStart);
  String solarStr = msg.substring(spStart, spEnd);
  int solar_v = 0, solar_i = 0, solar_p = 0;
  if (solarStr.indexOf(',') != -1)
  {
    int c1 = solarStr.indexOf(',');
    int c2 = solarStr.lastIndexOf(',');
    solar_v = solarStr.substring(0, c1).toInt();
    solar_i = solarStr.substring(c1 + 1, c2).toInt();
    solar_p = solarStr.substring(c2 + 1).toInt();
  }
  
  // sx - sun x: ",sx:" to ",sy:"
  int sxPos = msg.indexOf(",sx:");
  int sxStart = sxPos + 4;
  int sxEnd = msg.indexOf(",sy:", sxStart);
  float sun_x = msg.substring(sxStart, sxEnd).toFloat();
  
  // sy - sun y: ",sy:" to ",f:"
  int syPos = msg.indexOf(",sy:");
  int syStart = syPos + 4;
  int syEnd = msg.indexOf(",f:", syStart);
  float sun_y = msg.substring(syStart, syEnd).toFloat();
  
  // f - flags: ",f:" to ",c:"
  int fPos = msg.indexOf(",f:");
  int fStart = fPos + 3;
  int fEnd = msg.indexOf(",c:", fStart);
  int flags = msg.substring(fStart, fEnd).toInt();
  
  // c - CRC: ",c:" to "}"
  int cPos = msg.indexOf(",c:");
  int cStart = cPos + 3;
  int cEnd = msg.indexOf('}', cStart);
  String crc_recv = msg.substring(cStart, cEnd);
  
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