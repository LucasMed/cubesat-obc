#include <SoftwareSerial.h>

// Pines en el Nano
const byte HC12RxdPin = 2;
const byte HC12TxdPin = 3;
const byte HC12SetPin = 9;

SoftwareSerial HC12(HC12RxdPin, HC12TxdPin);

void setup() {
  pinMode(HC12SetPin, OUTPUT);
  digitalWrite(HC12SetPin, HIGH);

  Serial.begin(9600);
  HC12.begin(9600);

  Serial.println("=== Ground Station Ready ===");
  Serial.println("Comandos: REBOOT|STATUS|ECHO|CAPTURE|MODE=1|2|3|HELP");
}

void loop() {
  // Enviar comandos desde PC a OBC
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.length() > 0) {
      HC12.println(comando);
      Serial.println("-> Enviado: " + comando);
    }
  }

  // Recibir datos desde OBC
  if (HC12.available()) {
    String recibido = HC12.readStringUntil('\n');
    recibido.trim();
    
    if (recibido.startsWith("[TLM]")) {
      parseTelemetry(recibido);
    }
    else if (recibido.startsWith("[CMD]")) {
      Serial.println(recibido);
    }
    else {
      Serial.println(recibido);
    }
  }
}

void parseTelemetry(String msg) {
  // Extraer mode
  int posMode = msg.indexOf("mode=");
  int posAtt  = msg.indexOf("att=");
  int mode = msg.substring(posMode + 5, msg.indexOf(' ', posMode + 5)).toInt();

  // Extraer actitud
  String attStr = msg.substring(posAtt + 4, msg.indexOf("flags=") - 1);
  float roll = attStr.substring(0, attStr.indexOf(',')).toFloat();
  float pitch = attStr.substring(attStr.indexOf(',') + 1, attStr.lastIndexOf(',')).toFloat();
  float yaw = attStr.substring(attStr.lastIndexOf(',') + 1).toFloat();

  // Extraer flags
  int posFlags = msg.indexOf("flags=");
  int posGps = msg.indexOf("gps_lat=");
  String flagStr = msg.substring(posFlags + 6, posGps - 1);
  int flags = (int)strtol(flagStr.c_str(), NULL, 16);

  bool imu_ok = (flags & 0x01) != 0;
  bool temp_ok = (flags & 0x02) != 0;
  int energy_state = (flags >> 2) & 0x03;

  // Extraer GPS
  float gps_lat = getValue(msg, "gps_lat=").toFloat();
  float gps_lon = getValue(msg, "gps_lon=").toFloat();
  float gps_alt = getValue(msg, "gps_alt=").toFloat();
  int gps_valid = getValue(msg, "gps_valid=").toInt();

  // Mostrar
  Serial.print("Mode="); Serial.print(mode);
  Serial.print(" Roll="); Serial.print(roll);
  Serial.print(" Pitch="); Serial.print(pitch);
  Serial.print(" Yaw="); Serial.print(yaw);
  Serial.print(" IMU="); Serial.print(imu_ok ? "OK" : "FAIL");
  Serial.print(" Temp="); Serial.print(temp_ok ? "OK" : "FAIL");
  Serial.print(" Energy="); Serial.print(energy_state);
  Serial.print(" GPS="); Serial.print(gps_valid ? "OK" : "NO FIX");
  if (gps_valid) {
    Serial.print(" Lat="); Serial.print(gps_lat, 6);
    Serial.print(" Lon="); Serial.print(gps_lon, 6);
    Serial.print(" Alt="); Serial.print(gps_alt, 1);
  }
  Serial.println();
}

String getValue(String msg, String key) {
  int pos = msg.indexOf(key);
  if (pos == -1) return "0";
  int start = pos + key.length();
  int end = msg.indexOf(' ', start);
  if (end == -1) end = msg.length();
  return msg.substring(start, end);
}
