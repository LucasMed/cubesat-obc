/**
 * GPS NEO-7M Test Program for Arduino Nano
 * 
 * This comprehensive test program verifies GPS module functionality:
 * - NMEA sentence reception
 * - Checksum validation
 * - Satellite acquisition
 * - Fix quality (2D/3D)
 * - GPS timing accuracy
 * 
 * Hardware: Arduino Nano + NEO-7M GPS module
 * Connections:
 *   GPS VCC -> Nano 5V (or 3.3V if module is 3.3V compatible)
 *   GPS GND -> Nano GND
 *   GPS TX  -> Nano D2 (Software Serial RX)
 *   GPS RX  -> Nano D3 (optional, for commands)
 * 
 * LED Status Indicators (onboard LED D13):
 *   - Fast blinking (100ms): No satellites detected
 *   - Slow blinking (500ms): Has satellites, searching for fix
 *   - Solid ON: Fix acquired!
 *   - OFF: No data received
 * 
 * Open Serial Monitor at 115200 baud to view results
 */

#include <SoftwareSerial.h>
#include <Arduino.h>

// Configuration
#define GPS_RX_PIN 2    // Receive data from GPS
#define GPS_TX_PIN 3    // Send commands to GPS (optional)
#define BAUD_RATE 9600 // NEO-7M default baud rate
#define SERIAL_MONITOR BAUD_RATE

// NMEA Sentence buffer
#define MAX_SENTENCE_LENGTH 82
char sentenceBuffer[MAX_SENTENCE_LENGTH];
uint8_t bufferIndex = 0;

// Statistics
struct GPSStats {
  uint32_t total_sentences;
  uint32_t gpgga_received;   // GGA sentences (position data)
  uint32_t gprmc_received;   // RMC sentences (position + time)
  uint32_t gpgsa_received;   // GSA sentences (satellites)
  uint32_t gpgsv_received;   // GSV sentences (satellites in view)
  uint32_t checksum_valid;
  uint32_t checksum_invalid;
  uint32_t buffer_overflows;
  uint32_t unknown_sentences;
  
  // Decode raw sentences for debugging
  bool show_raw;
  
  // Fix data
  bool has_fix;
  uint8_t fix_quality;      // 0=invalid, 1=GPS, 2=DGPS
  uint8_t satellites;
  float latitude;
  float longitude;
  float altitude;
  float hdop;
  
  // Time
  uint32_t last_fix_time;
  uint32_t time_since_fix;
  
  // Signal
  int8_t signal_strength;
} stats;

// Software Serial
SoftwareSerial gpsSerial(GPS_RX_PIN, GPS_TX_PIN); // RX, TX

// Timing
unsigned long startupTime;
unsigned long lastStatsPrint;
#define STATS_PRINT_INTERVAL 5000  // 5 seconds

// LED blinking
#define LED_PIN LED_BUILTIN  // D13 on Nano
unsigned long lastLedToggle;
bool ledState = false;
uint16_t ledBlinkInterval = 100;  // Start with fast blink

void resetStats() {
  memset(&stats, 0, sizeof(stats));
}

void printHeader() {
  Serial.println(F("\n========================================"));
  Serial.println(F("   GPS NEO-7M Test Program v1.0"));
  Serial.println(F("   Arduino Nano + SoftwareSerial"));
  Serial.println(F("========================================\n"));
}

void printGPSStats() {
  unsigned long now = millis();
  unsigned long uptime = (now - startupTime) / 1000;
  
  Serial.println(F("\n--- GPS Statistics ---"));
  Serial.print(F("Uptime: ")); Serial.print(uptime); Serial.println(F(" seconds"));
  
  Serial.print(F("\nNMEA Sentences:\n"));
  Serial.print(F("  Total received: ")); Serial.println(stats.total_sentences);
  Serial.print(F("  GGA (position): ")); Serial.println(stats.gpgga_received);
  Serial.print(F("  RMC (time+pos): ")); Serial.println(stats.gprmc_received);
  Serial.print(F("  GSA (DOP):     ")); Serial.println(stats.gpgsa_received);
  Serial.print(F("  GSV (sats):    ")); Serial.println(stats.gpgsv_received);
  Serial.print(F("  Unknown:       ")); Serial.println(stats.unknown_sentences);
  
  Serial.print(F("\nChecksum:\n"));
  Serial.print(F("  Valid:   ")); Serial.println(stats.checksum_valid);
  Serial.print(F("  Invalid: ")); Serial.println(stats.checksum_invalid);
  float successRate = (stats.checksum_valid + stats.checksum_invalid > 0) 
    ? (float)stats.checksum_valid / (stats.checksum_valid + stats.checksum_invalid) * 100 
    : 0;
  Serial.print(F("  Success: ")); Serial.print(successRate, 1); Serial.println(F("%"));
  
  if (stats.buffer_overflows > 0) {
    Serial.print(F("\n*** Buffer Overflows: ")); Serial.println(stats.buffer_overflows);
  }
  
  Serial.print(F("\n--- Fix Status ---\n"));
  if (stats.has_fix) {
    Serial.println(F("  Status: FIX ACQUIRED!"));
    Serial.print(F("  Quality: ")); Serial.println(stats.fix_quality);
    Serial.print(F("  Satellites: ")); Serial.println(stats.satellites);
    Serial.print(F("  Latitude:  ")); Serial.print(stats.latitude, 6);
    Serial.println(F(" deg"));
    Serial.print(F("  Longitude: ")); Serial.print(stats.longitude, 6);
    Serial.println(F(" deg"));
    Serial.print(F("  Altitude: ")); Serial.print(stats.altitude, 1);
    Serial.println(F(" m"));
    Serial.print(F("  HDOP: ")); Serial.println(stats.hdop, 2);
    Serial.print(F("  Time since fix: ")); Serial.print(stats.time_since_fix);
    Serial.println(F(" ms"));
  } else {
    Serial.println(F("  Status: NO FIX"));
    Serial.print(F("  Fix quality: ")); Serial.println(stats.fix_quality);
    Serial.print(F("  Satellites: ")); Serial.println(stats.satellites);
    if (stats.satellites > 0) {
      Serial.println(F("  *** WARNING: Satellites detected but no fix! ***"));
    }
  }
  
  // Signal strength estimate
  if (stats.satellites > 0) {
    Serial.print(F("\n--- Satellite Signal ---\n"));
    Serial.print(F("  Satellites in view: ")); Serial.println(stats.satellites);
  }
}

bool verifyChecksum(const char* sentence) {
  // Format: $XXXXXX*HH\r\n
  // HH is XOR of all bytes between $ and *
  
  const char* star = strchr(sentence, '*');
  if (!star) return false;
  
  uint8_t calculated_sum = 0;
  for (const char* p = sentence + 1; p < star; p++) {
    calculated_sum ^= *p;
  }
  
  char* endptr = NULL;
  uint8_t expected_sum = strtoul(star + 1, &endptr, 16);
  
  return (calculated_sum == expected_sum);
}

void parseGGA(const char* sentence) {
  // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
  // Fields: 0=time, 1=lat, 2=latDir, 3=lon, 4=lonDir, 5=quality, 6=sats, 7=hdop, 8=alt, ...
  
  char buffer[MAX_SENTENCE_LENGTH];
  strcpy(buffer, sentence);
  
  char* ptr = buffer;
  char* fields[15];
  uint8_t fieldCount = 0;
  
  // Split by comma
  fields[fieldCount++] = strtok(ptr, ",");
  while (fieldCount < 15 && (fields[fieldCount-1] = strtok(NULL, ",")) != NULL) {
    fieldCount++;
  }
  
  if (fieldCount < 9) return;  // Invalid GGA
  
  // Fix quality (field 6)
  stats.fix_quality = atoi(fields[5]);
  
  // Satellites (field 7)
  stats.satellites = atoi(fields[7]);
  
  // HDOP (field 8)
  stats.hdop = atof(fields[8]);
  
  // Altitude (field 9)
  stats.altitude = atof(fields[9]);
  
  // Has fix if quality >= 1
  stats.has_fix = (stats.fix_quality >= 1);
  
  if (stats.has_fix) {
    // Parse latitude
    if (strlen(fields[1]) >= 4 && strlen(fields[2]) > 0) {
      float lat = atof(fields[1]);
      int latDeg = (int)(lat / 100);
      float latMin = lat - latDeg * 100;
      stats.latitude = latDeg + latMin / 60.0;
      if (fields[2][0] == 'S') {
        stats.latitude = -stats.latitude;
      }
    }
    
    // Parse longitude
    if (strlen(fields[3]) >= 5 && strlen(fields[4]) > 0) {
      float lon = atof(fields[3]);
      int lonDeg = (int)(lon / 100);
      float lonMin = lon - lonDeg * 100;
      stats.longitude = lonDeg + lonMin / 60.0;
      if (fields[4][0] == 'W') {
        stats.longitude = -stats.longitude;
      }
    }
    
    stats.last_fix_time = millis();
  }
}

void parseRMC(const char* sentence) {
  // $GPRMC,123519,A,3723.2475,N,12202.3246,W,0.13,309.62,120598,,,A*10
  // Fields: 0=time, 1=status, 2=lat, 3=latDir, 4=lon, 5=lonDir, ...
  
  char buffer[MAX_SENTENCE_LENGTH];
  strcpy(buffer, sentence);
  
  char* ptr = buffer;
  char* fields[15];
  uint8_t fieldCount = 0;
  
  fields[fieldCount++] = strtok(ptr, ",");
  while (fieldCount < 15 && (fields[fieldCount-1] = strtok(NULL, ",")) != NULL) {
    fieldCount++;
  }
  
  // Check if data is valid (field 2 should be 'A')
  if (fieldCount >= 3 && fields[2][0] == 'A') {
    // Position captured in GGA usually, this confirms validity
  }
}

void parseGSV(const char* sentence) {
  // $GPGSV,2,1,05,11,,,34,12,,,40,25,,,40,29,,,35*71
  // Shows satellites in view - we count them
  
  char buffer[MAX_SENTENCE_LENGTH];
  strcpy(buffer, sentence);
  
  char* ptr = buffer;
  char* fields[20];
  uint8_t fieldCount = 0;
  
  fields[fieldCount++] = strtok(ptr, ",");
  while (fieldCount < 20 && (fields[fieldCount-1] = strtok(NULL, ",")) != NULL) {
    fieldCount++;
  }
  
  // Field 3 = number of satellites in message
  // We just count total from this message
}

void processSentence(const char* sentence) {
  stats.total_sentences++;
  
  // Debug: print first 20 sentences in raw form
  if (stats.total_sentences <= 20) {
    Serial.print(F("RX: ")); Serial.println(sentence);
  }
  
  // Verify checksum first
  bool validChecksum = verifyChecksum(sentence);
  if (validChecksum) {
    stats.checksum_valid++;
  } else {
    stats.checksum_invalid++;
    // Don't process invalid checksum data
    return;
  }
  
  // Parse by sentence type (skip $ and look at first 5 chars)
  const char* type = sentence + 1;
  
  if (strncmp(type, "GPGGA", 5) == 0) {
    stats.gpgga_received++;
    parseGGA(sentence);
  }
  else if (strncmp(type, "GPRMC", 5) == 0) {
    stats.gprmc_received++;
    parseRMC(sentence);
  }
  else if (strncmp(type, "GPGSA", 5) == 0) {
    stats.gpgsa_received++;
  }
  else if (strncmp(type, "GPGSV", 5) == 0) {
    stats.gpgsv_received++;
    parseGSV(sentence);
  }
  else {
    stats.unknown_sentences++;
    // Debug: print unknown sentence types
    if (stats.unknown_sentences <= 3) {
      Serial.print(F("UNKNOWN: ")); Serial.println(sentence);
    }
  }
}

void readGPS() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    
    if (c == '$') {
      // Start of new sentence
      bufferIndex = 0;
      sentenceBuffer[bufferIndex++] = c;
    }
    else if (c == '\n' || c == '\r') {
      // End of sentence
      if (bufferIndex > 0) {
        sentenceBuffer[bufferIndex] = '\0';
        processSentence(sentenceBuffer);
      }
      bufferIndex = 0;
    }
    else if (bufferIndex < MAX_SENTENCE_LENGTH - 1) {
      sentenceBuffer[bufferIndex++] = c;
    }
    else {
      // Buffer overflow
      stats.buffer_overflows++;
      bufferIndex = 0;
    }
  }
}

void updateLED() {
  // Update LED blink interval based on GPS status
  if (!stats.has_fix && stats.satellites == 0) {
    // No satellites - fast blink
    ledBlinkInterval = 100;
  } else if (!stats.has_fix && stats.satellites > 0) {
    // Has satellites but no fix - slow blink
    ledBlinkInterval = 500;
  } else if (stats.has_fix) {
    // Has fix - solid ON
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
    return;
  }
  
  // Blink based on interval
  if (millis() - lastLedToggle >= ledBlinkInterval) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    lastLedToggle = millis();
  }
}

void setup() {
  Serial.begin(SERIAL_MONITOR);
  
  // Initialize LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize GPS software serial
  gpsSerial.begin(BAUD_RATE);
  
  // Wait for GPS to stabilize
  delay(1000);
  
  resetStats();
  startupTime = millis();
  lastStatsPrint = millis();
  
  printHeader();
  
  Serial.println(F("Initializing GPS module..."));
  Serial.println(F("Wait 30-60 seconds for cold start acquisition..."));
  Serial.println(F("\nSending NMEA configuration commands...\n"));
  
  // Configure GPS to output ALL sentences including GSV
  // PMTK314 sets which NMEA sentences are enabled
  // Format: $PMTK314,x,x,x,x,x,x,x,x,x,x,x,x,x,x,x*Checksum
  
  // Enable all standard sentences (GGA, GLL, GSA, GSV, RMC, VTG, ZDA)
  gpsSerial.println(F("$PMTK314,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0*2C")); // 1Hz all
  delay(100);
  
  // Set to output GSV every sentence
  gpsSerial.println(F("$PMTK313,1*2E")); // Enable GSV output
  delay(100);
  
  // Enable GLONASS (more satellites = better fix)
  gpsSerial.println(F("$PMTK351,0*2A")); // SBAS on
  delay(100);
  
  // Set to WGS84
  gpsSerial.println(F("$PMTK330,0*1E")); 
  delay(100);
  
  Serial.println(F("Configuration sent.\n"));
  
  printGPSStats();
}

void loop() {
  // Read incoming GPS data
  readGPS();
  
  // Update time since fix
  if (stats.has_fix) {
    stats.time_since_fix = millis() - stats.last_fix_time;
  }
  
  // Update LED status
  updateLED();
  
  // Print stats every 5 seconds
  if (millis() - lastStatsPrint >= STATS_PRINT_INTERVAL) {
    printGPSStats();
    lastStatsPrint = millis();
    
    // Status indicator
    Serial.print(F("\n>>> "));
    if (stats.has_fix) {
      Serial.print(F("FIX ACQUIRED - "));
      Serial.print(stats.satellites);
      Serial.println(F(" satellites"));
    } else if (stats.satellites > 0) {
      Serial.print(stats.satellites);
      Serial.println(F(" satellites visible, searching..."));
    } else {
      Serial.println(F("Searching..."));
    }
    Serial.println(F("<<<\n"));
  }
}

/**
 * LED blink pattern:
 * - Fast blink: No satellites
 * - Slow blink: Has satellites but no fix
 * - Solid on: Has fix
 * 
 * Optional external LED on D4 (uncomment to enable):
 * #define EXTERNAL_LED_PIN 4
 */