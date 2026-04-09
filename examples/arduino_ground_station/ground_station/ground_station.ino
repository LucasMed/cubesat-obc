#include <SoftwareSerial.h>

// Pines en el Nano
const byte HC12RxdPin = 2;
const byte HC12TxdPin = 3;
const byte HC12SetPin = 9;

SoftwareSerial HC12(HC12RxdPin, HC12TxdPin);

void setup()
{
  pinMode(HC12SetPin, OUTPUT);
  digitalWrite(HC12SetPin, HIGH);

  Serial.begin(9600);
  HC12.begin(9600);

  Serial.println("=== Ground Station Ready ===");
  Serial.println("Commands: REBOOT|STATUS|ECHO|CAPTURE|MODE=1|2|3|GPS|FAULTS|HELP");
}

void loop()
{
  // Enviar comandos desde PC a OBC
  if (Serial.available())
  {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.length() > 0)
    {
      HC12.println(comando);
      Serial.println("-> Enviado: " + comando);
    }
  }

  // Recibir datos desde OBC
  if (HC12.available())
  {
    String recibido = HC12.readStringUntil('\n');
    recibido.trim();

    if (recibido.startsWith("[TLM]"))
    {
      parseTelemetry(recibido);
    }
    else if (recibido.startsWith("[CMD]"))
    {
      Serial.println(recibido);
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

void parseTelemetry(String msg)
{
  // Extraer mode
  int posMode = msg.indexOf("mode=");
  int posAtt = msg.indexOf("att=");
  int mode = msg.substring(posMode + 5, msg.indexOf(' ', posMode + 5)).toInt();

  // Extraer actitud
  String attStr = msg.substring(posAtt + 4, msg.indexOf("flags=") - 1);
  float roll = attStr.substring(0, attStr.indexOf(',')).toFloat();
  float pitch = attStr.substring(attStr.indexOf(',') + 1, attStr.lastIndexOf(',')).toFloat();
  float yaw = attStr.substring(attStr.lastIndexOf(',') + 1).toFloat();

  // Extraer temperatura y humedad
  float temp = getValue(msg, "temp=").toFloat();
  float humidity = getValue(msg, "humidity=").toFloat();

  // Extraer flags
  int posFlags = msg.indexOf("flags=");
  int posGps = msg.indexOf("gps_lat=");
  String flagStr = msg.substring(posFlags + 6, posGps - 1);
  int flags = (int)strtol(flagStr.c_str(), NULL, 16);

  bool imu_ok = (flags & 0x01) != 0;
  bool temp_ok = (flags & 0x02) != 0;
  bool humidity_ok = (flags & 0x04) != 0;
  int energy_state = (flags >> 3) & 0x07;

  // Extraer GPS
  float gps_lat = getValue(msg, "gps_lat=").toFloat();
  float gps_lon = getValue(msg, "gps_lon=").toFloat();
  float gps_alt = getValue(msg, "gps_alt=").toFloat();
  int gps_valid = getValue(msg, "gps_valid=").toInt();
  int sats = getValue(msg, "sats=").toInt();

  // Mostrar
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
  Serial.print(temp, 1);
  Serial.print("C");
  Serial.print(" Hum=");
  Serial.print(humidity_ok ? humidity : -1, 1);
  Serial.print("%");
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

void parseGpsStatus(String msg)
{
  // Formato: GPS: v=1 lat=-31.43210 lon=-64.18123 alt=431.5 s=6 hdop=1.2
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
  // Formato: GPS STATS: rx=1234 chk_err=0 inv=2 valid=45 overflow=0
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
  // Formato: SYSTEM: mode=NOMINAL energy=NOMINAL imu=OK temp=OK mag=FAIL
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
  // Formato: FAULTS: OK
  String levelStr = getValue(msg, "FAULTS: ");
  Serial.print("FAULTS: ");
  Serial.println(levelStr);
}

String getValue(String msg, String key)
{
  int pos = msg.indexOf(key);
  if (pos == -1)
    return "0";
  int start = pos + key.length();
  int end = msg.indexOf(' ', start);
  if (end == -1)
    end = msg.length();
  return msg.substring(start, end);
}