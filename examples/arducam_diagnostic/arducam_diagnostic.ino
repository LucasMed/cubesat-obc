/**
 * @file arducam_diagnostic.ino
 * @brief Standalone Arducam OV2640 2MP Plus diagnostic for Arduino Uno.
 *
 * Tests the camera module IN ISOLATION from the OBC CPLD to determine
 * whether write/readback failures are caused by:
 *   (A) The camera module itself (sensor dead, PWDN stuck, etc.)
 *   (B) The OBC CPLD/bridge (SCCB translation bug)
 *
 * WIRING (⚠ 3.3V LOGIC — Arduino Uno is 5V!)
 *
 *   ⚠ The Arducam module is 3.3V ONLY.  Arduino Uno pins are 5V.
 *   You MUST use a 3.3V Arduino (Pro Mini 3.3V, Nano 3.3V) OR
 *   bi-directional level shifters on ALL I2C + SPI lines.
 *
 *   Camera Module ←→ Arduino (via level shifter):
 *     VCC     → 3.3V
 *     GND     → GND
 *     SDA     → A4 (I2C) + 4.7kΩ pull-up to 3.3V
 *     SCL     → A5 (I2C) + 4.7kΩ pull-up to 3.3V
 *     MOSI    → pin 11 (SPI)
 *     MISO    → pin 12 (SPI)
 *     SCK     → pin 13 (SPI)
 *     CS      → pin 10 (SPI chip select)
 *
 * OUTPUT:
 *   USB Serial (115200 baud) — connect PC directly only
 *
 * PROTOCOL REFERENCE:
 *   OV2640 I2C address: 0x30 (7-bit)
 *   Arducam SPI write:   addr | 0x80, data
 *   Arducam SPI read:    addr & 0x7F  → read byte
 *
 * REGISTERS:
 *   OV2640:
 *     0x0A  → PID_HIGH (expected 0x26)
 *     0x0B  → PID_LOW  (expected 0x42 or 0x41)
 *     0x12  → COM7     (post-reset 0x00, after config 0x40-0x43)
 *     0xFF  → BANK select (0x00 = DSP, 0x01 = SENSOR)
 *     0xDA  → IMAGE_MODE (DSP bank)
 *     0xE0  → IMAGE_MODE change enable (DSP)
 *
 *   Arducam CPLD (SPI):
 *     0x00  → TEST1 (R/W — write 0x55 read back to verify SPI)
 *     0x04  → FIFO (0x01=clear, 0x02=start capture)
 *     0x05  → GPIO_DIR (bit0=RST, bit1=PD, bit2=PWR_EN direction)
 *     0x06  → GPIO_WR  (bit0=RST, bit1=PD, bit2=PWR_EN value)
 *     0x07  → STATUS   (bit0=FIFO_RDY, bit2=FIFO_EMPTY)
 *     0x41  → TRIG     (bit3=CAP_DONE)
 *     0x42-44 → FIFO_SIZE (24-bit LE)
 *     0x3C  → BURST_READ_FIFO
 */

#include <stdio.h>
#include <SPI.h>
#include <Wire.h>

/* ====================================================================
 * Pin Definitions
 * ==================================================================== */
/* HC-12 pins removed — not enough RAM on Uno for SoftwareSerial */
#define PIN_CAM_CS    10  /* SPI chip select */

/* ====================================================================
 * Arducam / OV2640 Constants
 * ==================================================================== */
#define OV2640_ADDR    0x30  /* 7-bit I2C address */

/* Arducam SPI registers */
#define REG_TEST1       0x00
#define REG_FIFO        0x04
#define REG_GPIO_DIR    0x05
#define REG_GPIO_WR     0x06
#define REG_STATUS      0x07
#define REG_TRIG        0x41
#define REG_FIFO_SIZE1  0x42
#define REG_FIFO_SIZE2  0x43
#define REG_FIFO_SIZE3  0x44
#define CMD_BURST_READ  0x3C
#define SPI_WRITE_BIT   0x80
#define CAP_DONE_MASK   0x08

/* OV2640 I2C registers */
#define REG_PID_HIGH    0x0A
#define REG_PID_LOW     0x0B
#define REG_COM7        0x12
#define REG_BANK        0xFF
#define REG_OUTPUT_CTRL 0x04

/* Global */
static bool s_spi_ok = false;

/* ====================================================================
 * Serial helpers — USB Serial only (HC-12 removed for RAM)
 * Use F() to keep strings in flash, not RAM.
 * ==================================================================== */
#define both_print(s)      Serial.print(F(s))
#define both_println(s)    Serial.println(F(s))

/* both_printf — vsnprintf + Serial.print (format in RAM, 64-byte stack buf).
 * Stack OK now (340 B free vs 164 B before the consolidation). */
void both_printf(const char *fmt, ...)
{
  char buf[64];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  Serial.print(buf);
}

/* ====================================================================
 * SPI Helpers
 * ==================================================================== */
void cam_spi_write(uint8_t addr, uint8_t data)
{
  digitalWrite(PIN_CAM_CS, LOW);
  SPI.transfer(addr | SPI_WRITE_BIT);
  SPI.transfer(data);
  digitalWrite(PIN_CAM_CS, HIGH);
  delay(1);
}

uint8_t cam_spi_read(uint8_t addr)
{
  digitalWrite(PIN_CAM_CS, LOW);
  SPI.transfer(addr & 0x7F);
  uint8_t val = SPI.transfer(0x00);
  digitalWrite(PIN_CAM_CS, HIGH);
  return val;
}

/* ====================================================================
 * I2C Helpers
 * ==================================================================== */
bool ov_write(uint8_t reg, uint8_t val)
{
  Wire.beginTransmission(OV2640_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

bool ov_read(uint8_t reg, uint8_t *val)
{
  if (!val) return false;
  Wire.beginTransmission(OV2640_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(OV2640_ADDR, (uint8_t)1);
  if (Wire.available() < 1) return false;
  *val = Wire.read();
  return true;
}

/* ====================================================================
 * Diagnostics
 * ==================================================================== */

/* --- I2C Scan (targeted) --- */
void test_i2c_scan(void)
{
  both_println("\n============================================");
  both_println("TEST 1: I2C Bus Scan");
  both_println("============================================");

  /* Probe address 0x30 (OV2640) directly — only address we expect */
  Wire.beginTransmission(OV2640_ADDR);
  if (Wire.endTransmission() == 0)
  {
    both_println("  Found: 0x30 ← OV2640/SCCB");
    both_println("  Total: 1 device(s)");
  }
  else
  {
    both_println("  ✗ No response at 0x30");
    both_println("  Total: 0 device(s)");
    /* Quick sanity: try a few nearby addresses to rule out bus issues */
    for (uint8_t addr = 0x08; addr < 0x10; addr++)
    {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0)
        both_printf("  Unexpected device at 0x%02X\n", addr);
    }
  }
}

/* --- PID/VER Read --- */
void test_pid_ver(void)
{
  both_println("\n============================================");
  both_println("TEST 2: OV2640 Chip ID (PID/VER)");
  both_println("============================================");
  uint8_t pidh = 0xFF, pidl = 0xFF;
  bool r1 = ov_read(REG_PID_HIGH, &pidh);
  bool r2 = ov_read(REG_PID_LOW,  &pidl);
  both_printf("  PID_HIGH(0x0A) = 0x%02X  read=%s  (expected 0x26)\n",
              pidh, r1 ? "OK" : "FAIL");
  both_printf("  PID_LOW (0x0B) = 0x%02X  read=%s  (expected 0x42 or 0x41)\n",
              pidl, r2 ? "OK" : "FAIL");
  if (r1 && r2 && pidh == 0x26 && (pidl == 0x42 || pidl == 0x41))
    both_println("  ✓ VERDICT: OV2640 sensor identified");
  else
    both_println("  ✗ VERDICT: Sensor ID mismatch — not an OV2640 or I2C issue");
}

/* --- Write + Readback Test --- */
void test_write_readback(void)
{
  both_println("\n============================================");
  both_println("TEST 3: OV2640 Write + Readback");
  both_println("============================================");

  /* First, ensure we are in SENSOR bank */
  both_println("  Ensuring SENSOR bank (0xFF=0x01)...");
  ov_write(REG_BANK, 0x01);
  delay(10);
  {
    uint8_t bank = 0xFF;
    ov_read(REG_BANK, &bank);
    both_printf("  Bank=0x%02X  %s\n", bank,
                bank == 0x01 ? "✓ SENSOR bank" :
                bank == 0x00 ? "✗ Still DSP" : "✗ Unexpected");
  }

  /* 3a: Read COM7 without writing (baseline) */
  uint8_t com7 = 0xFF;
  ov_read(REG_COM7, &com7);
  both_printf("  Baseline COM7(0x12) = 0x%02X  (post-reset should be 0x00)\n", com7);

  /* 3b-3e: Write + readback helper — one format string in RAM, not four */
  int passed = 0;
  {
    struct { uint8_t reg, val; const char *desc; } wr[] = {
      {REG_COM7,        0x41, "COM7=0x41 (UXGA+JPEG)"},
      {REG_COM7,        0x42, "COM7=0x42 (test pattern)"},
      {REG_OUTPUT_CTRL, 0x08, "OUT=0x08 (JPEG en)"},
      {0x11,            0x80, "CLKRC=0x80 (PLL x2)"},
    };
    both_println("  Writing registers...");
    for (int i = 0; i < 4; i++)
    {
      Serial.print("  ");
      Serial.print(wr[i].desc);
      Serial.print(" → 0x");
      bool w = ov_write(wr[i].reg, wr[i].val);
      delay(5);
      uint8_t r = 0xFF;
      bool rd = ov_read(wr[i].reg, &r);
      if (rd) both_printf("%02X", r);
      else    both_print("??");
      both_printf("  w=%s r=%s m=%s\n",
                  w  ? "OK" : "FAIL",
                  rd ? "OK" : "FAIL",
                  (rd && r == wr[i].val) ? "YES" : "NO");
      if (rd && r == wr[i].val) passed++;
    }
  }
  both_print("\n  Write/readback: ");
  both_printf("%d", passed);
  both_println("/4 registers survived");
  if (passed == 4)
    both_println("  ✓ VERDICT: Writes reach the sensor and survive readback");
  else if (passed == 0)
    both_println("  ✗ VERDICT: NO writes survive — sensor not accepting config");
  else
    both_println("  ⚠ VERDICT: Partial write survival — intermittent or bank issue");
}

/* --- SPI CPLD Test --- */
void test_spi_cpld(void)
{
  both_println("\n============================================");
  both_println("TEST 4: Arducam CPLD SPI Communication");
  both_println("============================================");

  /* 4a: SPI test via TEST1 register */
  both_println("  TEST1 R/W test...");
  cam_spi_write(REG_TEST1, 0x55);
  delay(2);
  uint8_t rb1 = cam_spi_read(REG_TEST1);

  cam_spi_write(REG_TEST1, 0xAA);
  delay(2);
  uint8_t rb2 = cam_spi_read(REG_TEST1);

  cam_spi_write(REG_TEST1, 0x55); /* restore */
  both_printf("  Write 0x55, readback: 0x%02X\n", rb1);
  both_printf("  Write 0xAA, readback: 0x%02X\n", rb2);
  s_spi_ok = (rb1 == 0x55 && rb2 == 0xAA);
  both_printf("  CPLD: %s\n", s_spi_ok ? "GENUINE (R/W)" : "CLONE/FAKE (read-only)");

  /* If SPI is dead, skip CPLD-dependent tests */
  cam_spi_write(REG_TEST1, 0x55);
}

/* --- CPLD GPIO + Capture Test --- */
void test_cpld_capture(void)
{
  if (!s_spi_ok)
  {
    both_println("\n  SKIP: SPI CPLD not responding, cannot test capture");
    return;
  }

  both_println("\n============================================");
  both_println("TEST 5: CPLD GPIO + Capture Sequence");
  both_println("============================================");

  /* 5a: Read current STATUS */
  uint8_t status = cam_spi_read(REG_STATUS);
  both_printf("  STATUS (before): 0x%02X  (FIFO_RDY=%d FIFO_FULL=%d FIFO_EMPTY=%d)\n",
              status,
              (status & 0x01) ? 1 : 0,
              (status & 0x02) ? 1 : 0,
              (status & 0x04) ? 1 : 0);

  /* 5b: Configure CPLD GPIO for sensor (same as OBC camera_init) */
  both_println("  Configuring CPLD GPIO...");
  cam_spi_write(REG_GPIO_DIR, 0x07); /* RST+PD+PWR_EN as outputs */
  delay(10);
  cam_spi_write(REG_GPIO_WR,  0x05); /* RST=1, PD=0, PWR_EN=1 */
  delay(10);
  both_println("  GPIO_DIR=0x07, GPIO_WR=0x05 (RST=1 PD=0 PWR_EN=1)");

  /* 5c: Read status after GPIO config */
  status = cam_spi_read(REG_STATUS);
  both_printf("  STATUS (after GPIO): 0x%02X\n", status);

  /* 5d: Read GPIO_RD to verify GPIO_WR took effect */
  uint8_t gpio_rd = cam_spi_read(0x45); /* GPIO readback */
  both_printf("  GPIO_RD (0x45): 0x%02X  (should match GPIO_WR=0x05)\n", gpio_rd);

  /* 5e: SW reset OV2640 via I2C */
  both_println("  Sending SW reset (COM7=0x80)...");
  ov_write(0x12, 0x80);
  delay(100);
  uint8_t pidh = 0xFF, pidl = 0xFF;
  ov_read(REG_PID_HIGH, &pidh);
  ov_read(REG_PID_LOW,  &pidl);
  both_printf("  Post-reset: PIDH=0x%02X PIDL=0x%02X\n", pidh, pidl);

  /* 5f: Minimal config — JPEG mode + test pattern */
  both_println("  Minimal config: JPEG + color bars...");
  ov_write(REG_BANK, 0x01); delay(2); /* SENSOR bank */
  ov_write(REG_COM7, 0x42); delay(2); /* gray + test pattern */
  uint8_t com7_check = 0xFF;
  ov_read(REG_COM7, &com7_check);
  both_printf("  COM7 after config: 0x%02X  (0x42 = test pattern)\n", com7_check);

  /* 5g: Clear FIFO */
  both_println("  Clearing FIFO...");
  cam_spi_write(REG_FIFO, 0x01);
  delay(5);
  cam_spi_write(REG_FIFO, 0x01); /* second clear (SDK quirk) */
  delay(5);

  /* 5h: Read FIFO size before capture */
  status = cam_spi_read(REG_STATUS);
  both_printf("  STATUS (before capture): 0x%02X\n", status);

  /* 5i: Start capture */
  both_println("  Starting capture...");
  cam_spi_write(REG_FIFO, 0x02);
  delay(5);

  uint8_t trig   = cam_spi_read(REG_TRIG);
  uint8_t fifo_st = cam_spi_read(REG_FIFO);
  status = cam_spi_read(REG_STATUS);
  both_printf("  After START_CAPTURE: TRIG=0x%02X FIFO_REG=0x%02X STATUS=0x%02X\n",
              trig, fifo_st, status);

  /* 5j: Poll for CAP_DONE */
  both_println("  Polling CAP_DONE (5s timeout)...");
  uint32_t timeout = millis() + 5000;
  bool cap_done = false;
  while (millis() < timeout)
  {
    trig = cam_spi_read(REG_TRIG);
    status = cam_spi_read(REG_STATUS);
    if (trig & CAP_DONE_MASK)
    {
      both_printf("  CAP_DONE at %lu ms: TRIG=0x%02X STATUS=0x%02X\n",
                  millis() - (timeout - 5000), trig, status);
      cap_done = true;
      break;
    }
    delay(10);
  }

  if (!cap_done)
    both_println("  TIMEOUT — CAP_DONE never set");

  /* 5k: Final status + FIFO size */
  trig = cam_spi_read(REG_TRIG);
  status = cam_spi_read(REG_STATUS);
  uint32_t fifo_len = 0;
  if (cap_done)
  {
    uint32_t b1 = cam_spi_read(REG_FIFO_SIZE1);
    uint32_t b2 = cam_spi_read(REG_FIFO_SIZE2);
    uint32_t b3 = cam_spi_read(REG_FIFO_SIZE3) & 0x7F;
    fifo_len = (b3 << 16) | (b2 << 8) | b1;
  }
  both_printf("  FINAL: TRIG=0x%02X STATUS=0x%02X FIFO_SIZE=%lu\n",
              trig, status, (unsigned long)fifo_len);

  /* 5l: Read FIFO header (first 64 bytes) if data available */
  if (fifo_len > 0)
  {
    uint16_t to_read = (fifo_len < 64) ? (uint16_t)fifo_len : 64;
    both_printf("  Reading %u bytes from FIFO...\n", to_read);
    uint8_t buf[64];
    digitalWrite(PIN_CAM_CS, LOW);
    /* Phase 1: send BURST_READ command + dummy, first FIFO byte comes back with dummy */
    SPI.transfer(CMD_BURST_READ);                 /* command, response ignored */
    buf[0] = SPI.transfer(0x00);                 /* dummy tx → first FIFO byte rx */
    /* Phase 2: read remaining bytes */
    for (uint16_t i = 1; i < to_read; i++)
      buf[i] = SPI.transfer(0x00);
    digitalWrite(PIN_CAM_CS, HIGH);

    both_print("  FIFO header:");
    uint16_t show = (fifo_len < 64) ? fifo_len : 64;
    for (uint16_t i = 0; i < show; i++)
    {
      if (i % 16 == 0) both_print("\n    ");
      both_printf("%02X ", buf[i]);
    }
    both_println("");

    /* JPEG SOI check */
    if (buf[0] == 0xFF && buf[1] == 0xD8)
      both_println("  SOI found at offset 0 — JPEG header present");
    else
      both_printf("  No SOI at offset 0 (first bytes: %02X %02X)\n", buf[0], buf[1]);
  }
  else
  {
    both_println("  FIFO empty — no data to read");
  }
}

/* ====================================================================
 * Test Pattern Experiment
 * ==================================================================== */
void test_test_pattern(void)
{
  if (!s_spi_ok) return;

  both_println("\n============================================");
  both_println("TEST 6: Test Pattern + Capture (if writes stuck)");
  both_println("============================================");

  /* Read what COM7 actually is */
  uint8_t com7 = 0xFF;
  ov_read(REG_COM7, &com7);
  both_printf("  Current COM7: 0x%02X\n", com7);

  if (com7 != 0x42)
  {
    both_println("  COM7 is not 0x42 — writes may not stick.");
    both_println("  Attempting forced write anyway...");
    ov_write(REG_BANK, 0x01);
    delay(3);
    ov_write(REG_COM7, 0x42);
    delay(3);
    uint8_t check = 0xFF;
    ov_read(REG_COM7, &check);
    both_printf("  COM7 after forced write: 0x%02X\n", check);
  }

  /* Try a capture with current state */
  cam_spi_write(REG_FIFO, 0x01);
  delay(3);
  cam_spi_write(REG_FIFO, 0x01);
  delay(3);
  cam_spi_write(REG_FIFO, 0x02);

  both_println("  Polling (2s)...");
  uint32_t t0 = millis();
  bool done = false;
  while (millis() - t0 < 2000)
  {
    uint8_t t = cam_spi_read(REG_TRIG);
    if (t & CAP_DONE_MASK)
    {
      both_printf("  CAP_DONE at %lu ms (TRIG=0x%02X)\n", millis() - t0, t);
      done = true;
      break;
    }
    delay(10);
  }
  if (!done) both_println("  TIMEOUT (2s)");

  uint32_t b1 = cam_spi_read(REG_FIFO_SIZE1);
  uint32_t b2 = cam_spi_read(REG_FIFO_SIZE2);
  uint32_t b3 = cam_spi_read(REG_FIFO_SIZE3) & 0x7F;
  uint32_t sz = (b3 << 16) | (b2 << 8) | b1;
  both_printf("  FIFO_SIZE = %lu (0x%06lX)\n", (unsigned long)sz, (unsigned long)sz);
}

/* ====================================================================
 * Summary
 * ==================================================================== */
void print_summary(bool i2c_ok, bool pid_ok, int write_pass, bool spi_ok)
{
  both_println("\n============================================");
  both_println("DIAGNOSTIC SUMMARY");
  both_println("============================================");
  both_printf("  I2C communication:      %s\n", i2c_ok    ? "✓ OK" : "✗ FAIL");
  both_printf("  OV2640 ID (PID/VER):    %s\n", pid_ok    ? "✓ OK" : "✗ FAIL");
  both_printf("  Write+readback (3 regs): %d/3\n", write_pass);
  both_printf("  SPI CPLD communication: %s\n", spi_ok    ? "✓ OK" : "✗ FAIL");

  both_println("");
  if (!i2c_ok)
    both_println("  ➤ RECOMMENDATION: Check wiring, pull-ups, level shifters");
  else if (!pid_ok)
    both_println("  ➤ RECOMMENDATION: Sensor ID mismatch — wrong module or dead sensor");
  else if (write_pass == 0)
    both_println("  ➤ RECOMMENDATION: Writes don't stick — OBC CPLD bridge is likely the culprit");
  else if (write_pass == 3 && spi_ok)
    both_println("  ➤ RECOMMENDATION: Module fully functional — OBC CPLD bridge is the root cause");
  else
    both_println("  ➤ RECOMMENDATION: Partial functionality — see test details");
  both_println("============================================\n");
}

/* ====================================================================
 * Setup
 * ==================================================================== */
void setup()
{
  /* USB Serial */
  Serial.begin(115200);
  /* HC-12 not available — removed for RAM (see issue #4255) */

  delay(2000);
  both_println("\n\n");
  both_println("╔═══════════════════════════════════════════════╗");
  both_println("║  Arducam OV2640 2MP Plus Standalone Test     ║");
  both_println("║  CubeSat OBC Camera Diagnostic               ║");
  both_println("╚═══════════════════════════════════════════════╝");
  both_printf("Built: %s %s\n\n", __DATE__, __TIME__);

  /* I2C */
  Wire.begin();
  Wire.setClock(100000);

  /* SPI */
  pinMode(PIN_CAM_CS, OUTPUT);
  digitalWrite(PIN_CAM_CS, HIGH);
  SPI.begin();
  SPI.setDataMode(SPI_MODE0);
  SPI.setClockDivider(SPI_CLOCK_DIV8); /* ~2 MHz */

  delay(100);

  /* Reset CPLD — required by ArduCAM example for proper GPIO/sensor init */
  both_println("  Resetting CPLD...");
  cam_spi_write(REG_STATUS, 0x80); /* CPLD reset */
  delay(100);
  cam_spi_write(REG_STATUS, 0x00); /* Release reset */
  delay(100);

  /* ================================================================
   * Run Tests
   * ================================================================ */

  /* TEST 1: I2C scan — just check presence */
  test_i2c_scan();

  /* Check if OV2640 is on the bus */
  Wire.beginTransmission(OV2640_ADDR);
  bool i2c_ok = (Wire.endTransmission() == 0);

  /* If sensor present, initialize CPLD GPIO before I2C register ops:
   * CPLD's default GPIO is high-Z, may leave sensor in PWDN/reset */
  if (i2c_ok)
  {
    both_println("\n  Initializing CPLD GPIO for sensor...");
    cam_spi_write(REG_GPIO_DIR, 0x07); /* RST+PD+PWR_EN as outputs */
    cam_spi_write(REG_GPIO_WR,  0x05); /* RST=1, PD=0, PWR_EN=1 */
    delay(20); /* let sensor power up */
    s_spi_ok = true; /* mark for later tests */
    both_println("  GPIO_DIR=0x07, GPIO_WR=0x05 (RST=1 PD=0 PWR_EN=1)");

    /* Try a sensor register to verify GPIO release helped */
    uint8_t check = 0xFF;
    ov_write(REG_BANK, 0x01);
    delay(10);
    ov_read(REG_BANK, &check);
    both_printf("  REG_BANK after CPLD-init: 0x%02X\n", check);
  }

  /* TEST 2: PID/VER */
  bool pid_ok = false;
  if (i2c_ok)
  {
    test_pid_ver();
    uint8_t pidh = 0xFF, pidl = 0xFF;
    ov_read(REG_PID_HIGH, &pidh);
    ov_read(REG_PID_LOW,  &pidl);
    pid_ok = (pidh == 0x26 && (pidl == 0x42 || pidl == 0x41));
  }
  else
  {
    both_println("\n  SKIP: No I2C response from OV2640 address");
  }

  /* TEST 3: Write + readback */
  int write_pass = 0;
  if (i2c_ok)
  {
    test_write_readback();
    /* Count survivors from output — we'll use a simple heuristic */
    uint8_t com7 = 0xFF, out = 0xFF, clkrc = 0xFF;
    ov_read(REG_COM7, &com7);
    ov_read(REG_OUTPUT_CTRL, &out);
    ov_read(0x11, &clkrc);
    if (com7 == 0x42 || com7 == 0x41) write_pass++;
    if (out  == 0x08) write_pass++;
    if (clkrc == 0x80) write_pass++;
    /* Only 3 counts here since COM7 had 2 writes; approximate */
  }
  else
  {
    both_println("  SKIP: I2C not responding");
  }

  /* TEST 4: SPI CPLD */
  test_spi_cpld();

  /* TEST 5: Capture (only if SPI works) */
  if (s_spi_ok)
  {
    test_cpld_capture();
    test_test_pattern();
  }

  /* Summary */
  print_summary(i2c_ok, pid_ok, write_pass, s_spi_ok);

  both_println("Tests complete. Reset to re-run.");
}

/* ====================================================================
 * Loop — idle (all diagnostics run once in setup)
 * ==================================================================== */
void loop()
{
  /* Nothing to do — all diagnostics run once in setup */
}
