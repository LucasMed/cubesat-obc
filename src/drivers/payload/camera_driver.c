/**
 * @file camera_driver.c
 * @brief Arducam OV2640 Camera Driver implementation.
 *
 * Register configuration via I2C1 (GPIO2/3 SDA/SCL) and data readout
 * via the shared SPI0 bus (GPIO14 CS, GPIO17 MISO, GPIO18 SCK, GPIO19 MOSI).
 *
 * 8-pin Arducam Mini module (no RESET, TRIG, or FIFO_RDY pins):
 *   - SW reset via SCCB register 0x12 = 0x80
 *   - Capture trigger via SPI register ARDUCHIP_FIFO
 *   - Status polling via SPI register ARDUCHIP_TRIG (bit 3 = CAP_DONE_MASK)
 *
 * The OV2640 sensor is configured over SCCB (I2C-compatible at address 0x30).
 * The Arducam FIFO bridge (AL422B) is controlled via SPI registers.
 *
 * Register tables sourced from ArduCAM SDK (ov2640_regs.h) and adapted
 * for RP2350 / Pico 2W.
 *
 * Spec ref: ICD-PAYLOAD-001 §11.4, §11.16
 */

#include "camera_driver.h"

#include "pico_pins.h"
#include "spi_payload.h"

#if defined(PICO_BUILD)
  #include "hardware/clocks.h"
  #include "hardware/gpio.h"
  #include "hardware/i2c.h"
  #include "hardware/spi.h"
  #include "pico/time.h"
#endif

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* OV2640 Register Tables                                             */
/* ------------------------------------------------------------------ */

/**
 * Terminator for register tables.
 * Uses a {0xFF, 0xFF} sentinel pair because 0xFF alone is a valid
 * OV2640 register address (bank select DSP=0x00 / SENS=0x01).
 * A write of {0xFF, 0xFF} is never used in any init sequence.
 */
#define REG_TABLE_END 0xFF
#define REG_END_PAIR(r, v) ((r) == REG_TABLE_END && (v) == REG_TABLE_END)

/** OV2640 SCCB register pair */
typedef struct
{
  uint8_t reg;
  uint8_t val;
} ov2640_reg_t;

/*
 * OV2640_JPEG_INIT — Full sensor initialisation for JPEG output.
 * Selects sensor bank, DSP bank, PLL, clock dividers, AEC/AGC, AWB,
 * colour matrix, and gamma/LENC correction tables.
 */
static const ov2640_reg_t OV2640_JPEG_INIT[] = {
    /* ----------------------------------------------------------------- */
    /* Full init sequence from official ArduCAM Arduino SDK              */
    /* (ArduCAM/ov2640_regs.h — OV2640_JPEG_INIT)                        */
    /* ----------------------------------------------------------------- */

    /* DSP bank — reserved init */
    {0xFF, 0x00},
    {0x2C, 0xFF},
    {0x2E, 0xDF},

    /* Sensor bank — PLL, clock, output format */
    {0xFF, 0x01},
    {0x3C, 0x32}, /* PLL control */
    {0x11, 0x00}, /* Clock divider */
    {0x09, 0x02}, /* Reserved */
    {0x04, 0x28}, /* Output control */
    {0x13, 0xE5}, /* AEC/AGC */
    {0x14, 0x48}, /* AGC */
    {0x2C, 0x0C}, /* Vsync */
    {0x33, 0x78}, /* Reserved */
    {0x3A, 0x33}, /* Reserved */
    {0x3B, 0xFB}, /* Reserved */
    {0x3E, 0x00}, /* Reserved */
    {0x43, 0x11}, /* Reserved */
    {0x16, 0x10}, /* Output format — DVP */
    {0x39, 0x92}, /* PLL / clock */
    {0x35, 0xDA}, /* Clock control */

    /* ISP and sensor misc */
    {0x22, 0x1A}, /* ISP control (AEC, AWB) */
    {0x37, 0xC3}, /* Sensor misc */
    {0x23, 0x00}, /* AEC target MSB */
    {0x34, 0xC0}, /* AEC target LSB */
    {0x36, 0x1A}, /* Sensor misc */
    {0x06, 0x88}, /* Clock control */
    {0x07, 0xC0}, /* Clock control */
    {0x0D, 0x87}, /* Sharpness */
    {0x0E, 0x41}, /* Sharpness */

    /* Common control */
    {0x4C, 0x00},
    {0x48, 0x00},
    {0x5B, 0x00},
    {0x42, 0x03},
    {0x4A, 0x81},
    {0x21, 0x99},

    /* AEC / AGC */
    {0x24, 0x40},
    {0x25, 0x38},
    {0x26, 0x82},
    {0x5C, 0x00},
    {0x63, 0x00},
    {0x61, 0x70},
    {0x62, 0x80},
    {0x7C, 0x05},
    {0x20, 0x80}, /* AEC/AGC enable */
    {0x28, 0x30},

    {0x6C, 0x00},
    {0x6D, 0x80},
    {0x6E, 0x00},
    {0x70, 0x02},
    {0x71, 0x94},
    {0x73, 0xC1},

    /* Sensor timing — UXGA */
    {0x12, 0x40}, /* COM7: UXGA, YUV output */
    {0x17, 0x11}, /* HREFST */
    {0x18, 0x43}, /* HREFEND */
    {0x19, 0x00}, /* VSTRT */
    {0x1A, 0x4B}, /* VEND */
    {0x32, 0x09}, /* HREF/VREF */
    {0x37, 0xC0}, /* Sensor misc */
    {0x4F, 0x60}, /* Banding filter */
    {0x50, 0xA8}, /* Lens correction */
    {0x6D, 0x00}, /* Reserved */

    /* Banding filter */
    {0x3D, 0x38},
    {0x46, 0x3F},
    {0x4F, 0x60},
    {0x0C, 0x3C},

    /* DSP bank — image quality pipeline */
    {0xFF, 0x00},
    {0xE5, 0x7F},
    {0xF9, 0xC0},
    {0x41, 0x24},
    {0xE0, 0x14},
    {0x76, 0xFF},
    {0x33, 0xA0},
    {0x42, 0x20},
    {0x43, 0x18},
    {0x4C, 0x00},
    {0x87, 0xD5},
    {0x88, 0x3F},
    {0xD7, 0x03},
    {0xD9, 0x10},
    {0xD3, 0x82},

    /* SDE (Software De-Noise Engine) indirect registers */
    {0xC8, 0x08},
    {0xC9, 0x80},
    {0x7C, 0x00},
    {0x7D, 0x00},
    {0x7C, 0x03},
    {0x7D, 0x48},
    {0x7D, 0x48},
    {0x7C, 0x08},
    {0x7D, 0x20},
    {0x7D, 0x10},
    {0x7D, 0x0E},

    /* Gamma curve LUT */
    {0x90, 0x00},
    {0x91, 0x0E},
    {0x91, 0x1A},
    {0x91, 0x31},
    {0x91, 0x5A},
    {0x91, 0x69},
    {0x91, 0x75},
    {0x91, 0x7E},
    {0x91, 0x88},
    {0x91, 0x8F},
    {0x91, 0x96},
    {0x91, 0xA3},
    {0x91, 0xAF},
    {0x91, 0xC4},
    {0x91, 0xD7},
    {0x91, 0xE8},
    {0x91, 0x20},
    {0x92, 0x00},

    /* AEC/AGC LUT segments */
    {0x93, 0x06},
    {0x93, 0xE3},
    {0x93, 0x05},
    {0x93, 0x05},
    {0x93, 0x00},
    {0x93, 0x04},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x93, 0x00},
    {0x96, 0x00},

    /* Histogram LUT */
    {0x97, 0x08},
    {0x97, 0x19},
    {0x97, 0x02},
    {0x97, 0x0C},
    {0x97, 0x24},
    {0x97, 0x30},
    {0x97, 0x28},
    {0x97, 0x26},
    {0x97, 0x02},
    {0x97, 0x98},
    {0x97, 0x80},
    {0x97, 0x00},
    {0x97, 0x00},

    /* LENC (Lens Correction) */
    {0xC3, 0xED},
    {0xA4, 0x00},
    {0xA8, 0x00},
    {0xC5, 0x11},
    {0xC6, 0x51},
    {0xBF, 0x80},
    {0xC7, 0x10},
    {0xB6, 0x66},
    {0xB8, 0xA5},
    {0xB7, 0x64},
    {0xB9, 0x7C},
    {0xB3, 0xAF},
    {0xB4, 0x97},
    {0xB5, 0xFF},
    {0xB0, 0xC5},
    {0xB1, 0x94},
    {0xB2, 0x0F},
    {0xC4, 0x5C},

    /* Image quality */
    {0xC0, 0x64},
    {0xC1, 0x4B},
    {0x8C, 0x00},
    {0x86, 0x3D},
    {0x50, 0x00},
    {0x51, 0xC8},
    {0x52, 0x96},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x5A, 0xC8},
    {0x5B, 0x96},
    {0x5C, 0x00},

    /* DVP speed, output format */
    {0xD3, 0x00}, /* DVP speed — was 0x7F in earlier SDK */
    {0xC3, 0xED},
    {0x7F, 0x00}, /* DSP register */
    {0xDA, 0x00}, /* Image output — DSP raw */
    {0xE5, 0x1F},
    {0xE1, 0x67},
    {0xE0, 0x00},
    {0xDD, 0x7F},
    {0x05, 0x00},

    /* Trailing quality adjustment block */
    {0x12, 0x40},
    {0xD3, 0x04},
    {0xC0, 0x16},
    {0xC1, 0x12},
    {0x8C, 0x00},
    {0x86, 0x3D},
    {0x50, 0x00},
    {0x51, 0x2C},
    {0x52, 0x24},
    {0x53, 0x00},
    {0x54, 0x00},
    {0x55, 0x00},
    {0x5A, 0x2C},
    {0x5B, 0x24},
    {0x5C, 0x00},

    {REG_TABLE_END, REG_TABLE_END}};

/*
 * OV2640_YUV422 — Configures output as YUV422 before switching to JPEG.
 * Required between OV2640_JPEG_INIT and OV2640_JPEG.
 */
static const ov2640_reg_t OV2640_YUV422[] = {
    {0xFF, 0x00}, /* DSP bank */
    {0x05, 0x00}, {0xDA, 0x10}, {0xD7, 0x03}, {0xDF, 0x00},
    {0x33, 0x80}, {0x3C, 0x40}, {0xE1, 0x77}, {REG_TABLE_END, REG_TABLE_END}};

/*
 * OV2640_JPEG — Switches the DSP output format to JPEG.
 * Must be written AFTER OV2640_JPEG_INIT + OV2640_YUV422.
 */
static const ov2640_reg_t OV2640_JPEG[] = {{0xE0, 0x14},
                                           {0xE1, 0x77},
                                           {0xE5, 0x1F},
                                           {0xD7, 0x03},
                                           {0xDA, 0x10}, /* Image output format = JPEG */
                                           {0xE0, 0x00},
                                           {0xFF, 0x01}, /* Switch to sensor bank */
                                           {0x04, 0x08}, /* Output control */

                                           {REG_TABLE_END, REG_TABLE_END}};

/*
 * Resolution-specific register tables (JPEG output).
 * Each writes the sensor bank (0xFF=0x01) window registers and
 * the DSP bank (0xFF=0x00) ZMW/CI/SIZEL registers.
 */

static const ov2640_reg_t OV2640_160x120_JPEG[] = {
    {0xFF, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1A, 0x4B}, {0x32, 0x09}, {0x4F, 0xCA}, {0x50, 0xA8}, {0x5A, 0x23},
    {0x6D, 0x00}, {0x39, 0x12}, {0x35, 0xDA}, {0x22, 0x1A}, {0x37, 0xC3},
    {0x23, 0x00}, {0x34, 0xC0}, {0x36, 0x1A}, {0x06, 0x88}, {0x07, 0xC0},
    {0x0D, 0x87}, {0x0E, 0x41}, {0x4C, 0x00}, {0xFF, 0x00}, {0xE0, 0x04},
    {0xC0, 0x64}, {0xC1, 0x4B}, {0x86, 0x35}, {0x50, 0x92}, {0x51, 0xC8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5A, 0x28}, {0x5B, 0x1E}, {0x5C, 0x00}, {0xE0, 0x00}, {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_176x144_JPEG[] = {
    {0xFF, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1A, 0x4B}, {0x32, 0x09}, {0x4F, 0xCA}, {0x50, 0xA8}, {0x5A, 0x23},
    {0x6D, 0x00}, {0x39, 0x12}, {0x35, 0xDA}, {0x22, 0x1A}, {0x37, 0xC3},
    {0x23, 0x00}, {0x34, 0xC0}, {0x36, 0x1A}, {0x06, 0x88}, {0x07, 0xC0},
    {0x0D, 0x87}, {0x0E, 0x41}, {0x4C, 0x00}, {0xFF, 0x00}, {0xE0, 0x04},
    {0xC0, 0x64}, {0xC1, 0x4B}, {0x86, 0x35}, {0x50, 0x92}, {0x51, 0xC8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5A, 0x2C}, {0x5B, 0x24}, {0x5C, 0x00}, {0xE0, 0x00}, {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_320x240_JPEG[] = {
    {0xFF, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1A, 0x4B}, {0x32, 0x09}, {0x4F, 0xCA}, {0x50, 0xA8}, {0x5A, 0x23},
    {0x6D, 0x00}, {0x39, 0x12}, {0x35, 0xDA}, {0x22, 0x1A}, {0x37, 0xC3},
    {0x23, 0x00}, {0x34, 0xC0}, {0x36, 0x1A}, {0x06, 0x88}, {0x07, 0xC0},
    {0x0D, 0x87}, {0x0E, 0x41}, {0x4C, 0x00}, {0xFF, 0x00}, {0xE0, 0x04},
    {0xC0, 0x64}, {0xC1, 0x4B}, {0x86, 0x35}, {0x50, 0x89}, {0x51, 0xC8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5A, 0x50}, {0x5B, 0x3C}, {0x5C, 0x00}, {0xE0, 0x00}, {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_352x288_JPEG[] = {
    {0xFF, 0x01}, {0x12, 0x40}, {0x17, 0x11}, {0x18, 0x43}, {0x19, 0x00},
    {0x1A, 0x4B}, {0x32, 0x09}, {0x4F, 0xCA}, {0x50, 0xA8}, {0x5A, 0x23},
    {0x6D, 0x00}, {0x39, 0x12}, {0x35, 0xDA}, {0x22, 0x1A}, {0x37, 0xC3},
    {0x23, 0x00}, {0x34, 0xC0}, {0x36, 0x1A}, {0x06, 0x88}, {0x07, 0xC0},
    {0x0D, 0x87}, {0x0E, 0x41}, {0x4C, 0x00}, {0xFF, 0x00}, {0xE0, 0x04},
    {0xC0, 0x64}, {0xC1, 0x4B}, {0x86, 0x35}, {0x50, 0x89}, {0x51, 0xC8},
    {0x52, 0x96}, {0x53, 0x00}, {0x54, 0x00}, {0x55, 0x00}, {0x57, 0x00},
    {0x5A, 0x58}, {0x5B, 0x48}, {0x5C, 0x00}, {0xE0, 0x00}, {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_640x480_JPEG[] = {{0xFF, 0x01},
                                                   {0x11, 0x01},
                                                   {0x12, 0x00},
                                                   {0x17, 0x11},
                                                   {0x18, 0x75},
                                                   {0x32, 0x36},
                                                   {0x19, 0x01},
                                                   {0x1A, 0x97},
                                                   {0x03, 0x0F},
                                                   {0x37, 0x40},
                                                   {0x4F, 0xBB},
                                                   {0x50, 0x9C},
                                                   {0x5A, 0x57},
                                                   {0x6D, 0x80},
                                                   {0x3D, 0x34},
                                                   {0x39, 0x02},
                                                   {0x35, 0x88},
                                                   {0x22, 0x0A},
                                                   {0x37, 0x40},
                                                   {0x34, 0xA0},
                                                   {0x06, 0x02},
                                                   {0x0D, 0xB7},
                                                   {0x0E, 0x01},
                                                   {0xFF, 0x00},
                                                   {0xE0, 0x04},
                                                   {0xC0, 0xC8},
                                                   {0xC1, 0x96},
                                                   {0x86, 0x3D},
                                                   {0x50, 0x89},
                                                   {0x51, 0x90},
                                                   {0x52, 0x2C},
                                                   {0x53, 0x00},
                                                   {0x54, 0x00},
                                                   {0x55, 0x88},
                                                   {0x57, 0x00},
                                                   {0x5A, 0xA0},
                                                   {0x5B, 0x78},
                                                   {0x5C, 0x00},
                                                   {0xD3, 0x04},
                                                   {0xE0, 0x00},
                                                   {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_800x600_JPEG[] = {{0xFF, 0x01},
                                                   {0x11, 0x01},
                                                   {0x12, 0x00},
                                                   {0x17, 0x11},
                                                   {0x18, 0x75},
                                                   {0x32, 0x36},
                                                   {0x19, 0x01},
                                                   {0x1A, 0x97},
                                                   {0x03, 0x0F},
                                                   {0x37, 0x40},
                                                   {0x4F, 0xBB},
                                                   {0x50, 0x9C},
                                                   {0x5A, 0x57},
                                                   {0x6D, 0x80},
                                                   {0x3D, 0x34},
                                                   {0x39, 0x02},
                                                   {0x35, 0x88},
                                                   {0x22, 0x0A},
                                                   {0x37, 0x40},
                                                   {0x34, 0xA0},
                                                   {0x06, 0x02},
                                                   {0x0D, 0xB7},
                                                   {0x0E, 0x01},
                                                   {0xFF, 0x00},
                                                   {0xE0, 0x04},
                                                   {0xC0, 0xC8},
                                                   {0xC1, 0x96},
                                                   {0x86, 0x35},
                                                   {0x50, 0x89},
                                                   {0x51, 0x90},
                                                   {0x52, 0x2C},
                                                   {0x53, 0x00},
                                                   {0x54, 0x00},
                                                   {0x55, 0x88},
                                                   {0x57, 0x00},
                                                   {0x5A, 0xC8},
                                                   {0x5B, 0x96},
                                                   {0x5C, 0x00},
                                                   {0xD3, 0x02},
                                                   {0xE0, 0x00},
                                                   {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_1024x768_JPEG[] = {
    {0xFF, 0x01}, {0x11, 0x01}, {0x12, 0x00},
    {0x17, 0x11}, {0x18, 0x75}, {0x32, 0x36},
    {0x19, 0x01}, {0x1A, 0x97}, {0x03, 0x0F},
    {0x37, 0x40}, {0x4F, 0xBB}, {0x50, 0x9C},
    {0x5A, 0x57}, {0x6D, 0x80}, {0x3D, 0x34},
    {0x39, 0x02}, {0x35, 0x88}, {0x22, 0x0A},
    {0x37, 0x40}, {0x34, 0xA0}, {0x06, 0x02},
    {0x0D, 0xB7}, {0x0E, 0x01}, {0xFF, 0x00},
    {0xC0, 0xC8}, {0xC1, 0x96}, {0x8C, 0x00},
    {0x86, 0x3D}, {0x50, 0x00}, {0x51, 0x90},
    {0x52, 0x2C}, {0x53, 0x00}, {0x54, 0x00},
    {0x55, 0x88}, {0x5A, 0x00}, {0x5B, 0xC0},
    {0x5C, 0x01}, {0xD3, 0x02}, {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_1280x1024_JPEG[] = {{0xFF, 0x01},
                                                     {0x11, 0x01},
                                                     {0x12, 0x00},
                                                     {0x17, 0x11},
                                                     {0x18, 0x75},
                                                     {0x32, 0x36},
                                                     {0x19, 0x01},
                                                     {0x1A, 0x97},
                                                     {0x03, 0x0F},
                                                     {0x37, 0x40},
                                                     {0x4F, 0xBB},
                                                     {0x50, 0x9C},
                                                     {0x5A, 0x57},
                                                     {0x6D, 0x80},
                                                     {0x3D, 0x34},
                                                     {0x39, 0x02},
                                                     {0x35, 0x88},
                                                     {0x22, 0x0A},
                                                     {0x37, 0x40},
                                                     {0x34, 0xA0},
                                                     {0x06, 0x02},
                                                     {0x0D, 0xB7},
                                                     {0x0E, 0x01},
                                                     {0xFF, 0x00},
                                                     {0xE0, 0x04},
                                                     {0xC0, 0xC8},
                                                     {0xC1, 0x96},
                                                     {0x86, 0x3D},
                                                     {0x50, 0x00},
                                                     {0x51, 0x90},
                                                     {0x52, 0x2C},
                                                     {0x53, 0x00},
                                                     {0x54, 0x00},
                                                     {0x55, 0x88},
                                                     {0x57, 0x00},
                                                     {0x5A, 0x40},
                                                     {0x5B, 0xF0},
                                                     {0x5C, 0x01},
                                                     {0xD3, 0x02},
                                                     {0xE0, 0x00},
                                                     {REG_TABLE_END, REG_TABLE_END}};

static const ov2640_reg_t OV2640_1600x1200_JPEG[] = {{0xFF, 0x01},
                                                     {0x11, 0x01},
                                                     {0x12, 0x00},
                                                     {0x17, 0x11},
                                                     {0x18, 0x75},
                                                     {0x32, 0x36},
                                                     {0x19, 0x01},
                                                     {0x1A, 0x97},
                                                     {0x03, 0x0F},
                                                     {0x37, 0x40},
                                                     {0x4F, 0xBB},
                                                     {0x50, 0x9C},
                                                     {0x5A, 0x57},
                                                     {0x6D, 0x80},
                                                     {0x3D, 0x34},
                                                     {0x39, 0x02},
                                                     {0x35, 0x88},
                                                     {0x22, 0x0A},
                                                     {0x37, 0x40},
                                                     {0x34, 0xA0},
                                                     {0x06, 0x02},
                                                     {0x0D, 0xB7},
                                                     {0x0E, 0x01},
                                                     {0xFF, 0x00},
                                                     {0xE0, 0x04},
                                                     {0xC0, 0xC8},
                                                     {0xC1, 0x96},
                                                     {0x86, 0x3D},
                                                     {0x50, 0x00},
                                                     {0x51, 0x90},
                                                     {0x52, 0x2C},
                                                     {0x53, 0x00},
                                                     {0x54, 0x00},
                                                     {0x55, 0x88},
                                                     {0x57, 0x00},
                                                     {0x5A, 0x90},
                                                     {0x5B, 0x2C},
                                                     {0x5C, 0x05},
                                                     {0xD3, 0x02},
                                                     {0xE0, 0x00},
                                                     {REG_TABLE_END, REG_TABLE_END}};

/** Index into resolution table array — order matches camera_res_t */
static const ov2640_reg_t *const RES_TABLES[] = {
    OV2640_160x120_JPEG,   /* CAM_RES_160x120    */
    OV2640_176x144_JPEG,   /* CAM_RES_176x144    */
    OV2640_320x240_JPEG,   /* CAM_RES_320x240    */
    OV2640_352x288_JPEG,   /* CAM_RES_352x288    */
    OV2640_640x480_JPEG,   /* CAM_RES_640x480    */
    OV2640_800x600_JPEG,   /* CAM_RES_800x600    */
    OV2640_1024x768_JPEG,  /* CAM_RES_1024x768   */
    OV2640_1280x1024_JPEG, /* CAM_RES_1280x1024  */
    OV2640_1600x1200_JPEG  /* CAM_RES_1600x1200  */
};

#define RES_TABLE_COUNT (sizeof(RES_TABLES) / sizeof(RES_TABLES[0]))

/* ------------------------------------------------------------------ */
/* Internal Helpers                                                    */
/* ------------------------------------------------------------------ */

#if defined(PICO_BUILD)

/*
 * I2C1 (GPIO2/3 SDA/SCL) — OV2640 SCCB register access.
 * The OV2640 uses SCCB which is compatible with I2C at address 0x30.
 */
static bool cam_i2c_write(uint8_t reg, uint8_t val)
{
  /*
   * Match official ArduCAM PICO SDK protocol exactly:
   *   write_reg(addr, val) uses i2c_write_blocking(..., buf, 2, true)
   *   — nostop=true means repeated START, not STOP.
   *   The official OV2640 probe (rdSensorReg8_8) also uses nostop=true
   *   for the address phase of reads.
   *
   * Ref: RPI-Pico-Cam/tflmicro/Arducam/src/arducam.c
   */
  uint8_t buf[2] = {reg, val};
  return i2c_write_blocking(I2C1_PORT, OV2640_I2C_ADDR, buf, 2, true) == 2;
}

static bool cam_i2c_read(uint8_t reg, uint8_t *val)
{
  /*
   * Match official ArduCAM PICO SDK protocol:
   *   rdSensorReg8_8 uses nostop=true for the write phase (repeated start),
   *   then nostop=false for the read phase.
   */
  if (i2c_write_blocking(I2C1_PORT, OV2640_I2C_ADDR, &reg, 1, true) != 1)
  {
    return false;
  }
  return i2c_read_blocking(I2C1_PORT, OV2640_I2C_ADDR, val, 1, false) == 1;
}

/**
 * Write a table of {reg, val} pairs to the OV2640 via I2C1.
 * @param table  The register table (terminated by REG_END_PAIR).
 * @param max_entries  Max entries to write. 0 = write all entries.
 */
static bool cam_write_reg_table(const ov2640_reg_t *table, size_t max_entries)
{
  size_t count = 0;
  for (const ov2640_reg_t *p = table; !REG_END_PAIR(p->reg, p->val); p++)
  {
    if (max_entries > 0 && count >= max_entries)
    {
      break; /* Stop after max_entries (excluding terminator) */
    }
    if (!cam_i2c_write(p->reg, p->val))
    {
      return false;
    }
    count++;
  }
  return true;
}

/*
 * Arducam SPI protocol (from official Pico SDK):
 *   Write: addr | 0x80, data  — bit 7 = 1 means WRITE
 *   Read:  addr & 0x7f        — bit 7 = 0 means READ
 *   Transfers are separate: write_blocking for command, read_blocking for response.
 *   A 1 ms delay follows every write to let the CPLD settle.
 */
static void cam_spi_write(uint8_t addr, uint8_t data)
{
  uint8_t buf[2] = {addr | ARDUCAM_SPI_WRITE, data};
  spi_payload_cs_select(SPI_CS_CAM_PIN);
  spi_write_blocking(SPI0_PORT, buf, 2);
  spi_payload_cs_deselect(SPI_CS_CAM_PIN);
  sleep_ms(1);
}

static uint8_t cam_spi_read(uint8_t addr)
{
  uint8_t value = 0;
  addr &= 0x7f; /* bit 7 = 0 = read */
  spi_payload_cs_select(SPI_CS_CAM_PIN);
  spi_write_blocking(SPI0_PORT, &addr, 1);
  spi_read_blocking(SPI0_PORT, 0, &value, 1);
  spi_payload_cs_deselect(SPI_CS_CAM_PIN);
  return value;
}

/*
 * XCLK control for the OV2640 pixel array.
 *
 * GPIO18 is SPI0 SCK in normal operation.  The OV2640 derives its master
 * clock (XCLK) from this pin via the CPLD bridge.  SPI SCK only runs
 * during bus transactions, starving the sensor's pixel readout between
 * writes.  By temporarily switching GPIO18 to the GPOUT0 clock generator,
 * we provide a continuous ~12 MHz clock during frame capture.
 *
 * After the frame wait, GPIO18 is restored to SPI function so the FIFO
 * can be read via the shared SPI bus.
 */
static void camera_enable_xclk(void)
{
  clock_gpio_init(SPI0_SCK_PIN, CLOCKS_CLK_GPOUT0_CTRL_AUXSRC_VALUE_CLK_SYS, 12.5f);
  sleep_ms(5);
}

static void camera_restore_sck(void)
{
  clock_stop(clk_gpout0);
  gpio_set_function(SPI0_SCK_PIN, GPIO_FUNC_SPI);
  sleep_ms(1);
}

#else /* Host/Unit Test Mocks */

/* Host-mode mock register array — non-static for test access */
uint8_t s_mock_i2c_regs[256];

static bool cam_i2c_write(uint8_t reg, uint8_t val)
{
  s_mock_i2c_regs[reg] = val;
  return true;
}

static bool cam_i2c_read(uint8_t reg, uint8_t *val)
{
  if (reg == 0x0A) /* PIDH */
  {
    *val = OV2640_CHIPID_HIGH;
  }
  else if (reg == 0x0B) /* PIDL */
  {
    *val = OV2640_CHIPID_LOW;
  }
  else
  {
    *val = s_mock_i2c_regs[reg];
  }
  return true;
}

static bool cam_write_reg_table(const ov2640_reg_t *table, size_t max_entries)
{
  size_t count = 0;
  for (const ov2640_reg_t *p = table; !REG_END_PAIR(p->reg, p->val); p++)
  {
    if (max_entries > 0 && count >= max_entries)
    {
      break;
    }
    s_mock_i2c_regs[p->reg] = p->val;
    count++;
  }
  return true;
}

static void cam_spi_write(uint8_t addr, uint8_t data)
{
  (void)addr;
  (void)data;
}

static uint8_t cam_spi_read(uint8_t addr)
{
  (void)addr;
  return 0x00;
}

#endif /* PICO_BUILD */

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/** Idempotency guard — camera_init() skips full init if already done. */
static bool s_camera_inited = false;

bool camera_init(void)
{
  if (s_camera_inited)
  {
    printf("[camera_init] Already initialized, skipping\n");
    return true;
  }

  printf("[camera_init] Starting\n");
  spi_payload_init();

#if defined(PICO_BUILD)
  /*
   * I2C1 init on GPIO2 (SDA) / GPIO3 (SCL) — dedicated camera config bus.
   * I2C1 is NOT shared with any other payload device.
   */
  i2c_init(I2C1_PORT, 100000); /* 100 kHz — safer for breadboard wiring */
  gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C1_SDA_PIN);
  gpio_pull_up(I2C1_SCL_PIN);

  /*
   * 1. CPLD reset (Arducam Mini 2MP Plus) — register 0x07, bit 7.
   *    The CPLD is the programmable logic that bridges SPI ↔ FIFO/sensor.
   *    Without this reset the SPI register map may be in an undefined state.
   */
  printf("[camera_init] Resetting CPLD (0x07 = 0x80)...\n");
  cam_spi_write(0x07, 0x80);
  sleep_ms(100);
  cam_spi_write(0x07, 0x00);
  sleep_ms(100);

  /*
   * 1.5 CPLD GPIO config — restore sensor control pins.
   * After CPLD reset the GPIO direction and output registers default to 0x00,
   * leaving the sensor without power or in reset.
   * Writing the known-good ArduCAM SDK values restores sensor control.
   */
  cam_spi_write(0x05, 0x07); /* GPIO_DIR: RST+PD+PWR_EN as outputs */
  sleep_ms(5);
  cam_spi_write(0x06, 0x05); /* GPIO_WR: RST=1, PD=0, PWR_EN=1 */
  sleep_ms(10);
  printf("[camera_init] CPLD GPIO config: 0x05=0x07 0x06=0x05 (RST=1 PD=0 PWR_EN=1)\n");

  /* 2. Verify SPI register access via ARDUCHIP_TEST1 (R/W test) */
  printf("[camera_init] Verifying SPI (TEST1 = 0x55)...\n");
  cam_spi_write(ARDUCHIP_TEST1, 0x55);
  sleep_ms(1);
  uint8_t test_val = cam_spi_read(ARDUCHIP_TEST1);
  if (test_val != 0x55)
  {
    printf("[camera_init] FAIL: SPI test read 0x%02X, expected 0x55\n", test_val);
    return false;
  }
  printf("[camera_init] SPI verification OK\n");

  /*
   * 2.5 CPLD SCCB bridge enable — exact ArduCAM SDK sequence
   *
   * The ArduChip CPLD has an internal SCCB bridge (SPI master → SCCB bus to
   * the OV2640) that is DISABLED by default after CPLD reset (0x07 = 0x00).
   *
   * Key insight from ArduCAM SDK: the bridge needs THREE things before it
   * forwards traffic:
   *   1. FIFO clear          (0x01 = 0x00)
   *   2. SCCB slave address  (0x20 = 0x30  — OV2640 7-bit addr << 1)
   *   3. Bridge enable       (0x07 = 0x20  — bit 5 = SCCB_CTRL enable)
   *   + 50 ms settle delay
   *
   * Some revisions use 0x24 instead of 0x20 for the SCCB_ID register.
   * If neither works, brute-force scan 0x00-0x3F to find the SCCB_ID reg.
   */

  bool sccb_bridge_found = false;

  /*
   * NOTE: On this CPLD revision, I2C reads through the SCCB bridge always
   * return 0xFF even when writes succeed and the sensor is correctly
   * configured. We detect bridge enable by reading back the STATUS register
   * (0x07, bit 5 = SCCB enable) instead of verifying via I2C read.
   *
   * Try two known SCCB_ID candidates first (0x20 and 0x24), then
   * brute-force 0x00-0x3F if neither works.
   */
  const uint8_t sccb_id_candidates[] = {0x20, 0x24};
  for (size_t ci = 0; ci < sizeof(sccb_id_candidates) && !sccb_bridge_found; ci++)
  {
    uint8_t id_reg = sccb_id_candidates[ci];
    printf("[camera_init] Trying SCCB enable (ID reg=0x%02X)...\n", id_reg);
    cam_spi_write(0x01, 0x00); /* 1. Clear FIFO */
    sleep_ms(5);
    cam_spi_write(id_reg, 0x30); /* 2. Set SCCB slave ID (OV2640 addr) */
    sleep_ms(5);
    cam_spi_write(0x07, 0x20);             /* 3. Enable SCCB bridge (bit 5) */
    cam_spi_write(0x03, VSYNC_LEVEL_MASK); /* 4. Configure timing */
    sleep_ms(50);                          /* 5. Critical settle delay */

    /* Verify: read STATUS register — bit 5 should be set */
    if (cam_spi_read(0x07) & 0x20)
    {
      printf("  *** SCCB BRIDGE ENABLED via ID reg 0x%02X! STATUS=0x%02X\n", id_reg,
             cam_spi_read(0x07));
      sccb_bridge_found = true;
      break;
    }
  }

  /* If known candidates failed, brute-force scan 0x00-0x3F for SCCB_ID reg */
  if (!sccb_bridge_found)
  {
    printf("[camera_init] Known SCCB_ID regs failed — brute-force scanning 0x00-0x3F...\n");
    for (uint8_t id_reg = 0x00; id_reg < 0x40 && !sccb_bridge_found; id_reg++)
    {
      /* Skip STATUS register — it is not an SCCB_ID candidate */
      if (id_reg == 0x07)
        continue;

      cam_spi_write(0x01, 0x00); /* Clear FIFO */
      sleep_ms(2);
      cam_spi_write(id_reg, 0x30); /* Try this reg as SCCB_ID */
      sleep_ms(2);
      cam_spi_write(0x07, 0x20); /* Enable bridge */
      sleep_ms(20);              /* Shorter settle for scan */

      /* Verify via STATUS register */
      if (cam_spi_read(0x07) & 0x20)
      {
        printf("  *** SCCB BRIDGE ENABLED via brute-force ID reg 0x%02X! STATUS=0x%02X\n", id_reg,
               cam_spi_read(0x07));
        sccb_bridge_found = true;
        break;
      }
    }
  }

  /*
   * 2.6 Restore CPLD GPIO registers after brute-force scan
   *
   * The brute-force scan writes 0x30 to every register 0x00-0x3F as a
   * candidate SCCB_ID, CORRUPTING GPIO_DIR (0x05) and GPIO_WR (0x06).
   * Restore the known-good values so the OV2640 has power and is out of reset.
   */
  cam_spi_write(0x05, 0x07); /* GPIO_DIR: RST+PD+PWR_EN as outputs */
  sleep_ms(2);
  cam_spi_write(0x06, 0x05); /* GPIO_WR: RST=1, PD=0, PWR_EN=1 */
  sleep_ms(10);

  if (sccb_bridge_found)
  {
    printf("[camera_init] SCCB bridge: ENABLED\n");
  }
  else
  {
    printf("[camera_init] SCCB bridge: NOT FOUND — OV2640 module may need replacement\n");
  }

  /* 3. Pre-reset Chip ID + COM7 read — verify initial state BEFORE any writes */
  {
    uint8_t pre_pidh = 0, pre_pidl = 0, pre_com7 = 0;
    bool pr1 = cam_i2c_read(0x0A, &pre_pidh);
    bool pr2 = cam_i2c_read(0x0B, &pre_pidl);
    bool pr3 = cam_i2c_read(0x12, &pre_com7);
    printf("[camera_init] Pre-reset: PIDH=0x%02X(%s) PIDL=0x%02X(%s) COM7=0x%02X(%s)\n", pre_pidh,
           pr1 ? "OK" : "FAIL", pre_pidl, pr2 ? "OK" : "FAIL", pre_com7, pr3 ? "OK" : "FAIL");
  }

  /* 4. Software reset via SCCB COM7 bit 7 (no hardware RESET pin on module) */
  printf("[camera_init] Sending SW reset (0x12=0x80)...\n");
  bool reset_ok = cam_i2c_write(0x12, 0x80);
  printf("[camera_init] SW reset: %s\n", reset_ok ? "OK" : "FAILED");
  sleep_ms(100); /* OV2640 requires ~20ms after SW reset — match official SDK at 100ms */

  /* 5. Post-reset Chip ID + COM7 — did SW reset actually change anything? */
  {
    uint8_t post_pidh = 0, post_pidl = 0, post_com7 = 0;
    bool pr1 = cam_i2c_read(0x0A, &post_pidh);
    bool pr2 = cam_i2c_read(0x0B, &post_pidl);
    bool pr3 = cam_i2c_read(0x12, &post_com7);
    printf("[camera_init] Post-reset: PIDH=0x%02X(%s) PIDL=0x%02X(%s) COM7=0x%02X(%s)\n", post_pidh,
           pr1 ? "OK" : "FAIL", post_pidl, pr2 ? "OK" : "FAIL", post_com7, pr3 ? "OK" : "FAIL");
  }
#else
  printf("[camera_init] Host build — skipping HW init\n");
#endif

  /* 6. Verify Chip ID — OV2640 should return 0x26 / 0x42 */
  uint8_t pidh = 0, pidl = 0;
  // cppcheck-suppress knownConditionTrueFalse
  bool pidh_ok = cam_i2c_read(0x0A, &pidh);
  // cppcheck-suppress knownConditionTrueFalse
  const char *pidh_status = pidh_ok ? "OK" : "FAIL";
  printf("[camera_init] PIDH read: %s, value=0x%02X (expected 0x%02X)\n", pidh_status, pidh,
         OV2640_CHIPID_HIGH);
  // cppcheck-suppress knownConditionTrueFalse
  if (!pidh_ok || pidh != OV2640_CHIPID_HIGH)
  {
    printf("[camera_init] FAIL: Chip ID high mismatch\n");
    return false;
  }

  // cppcheck-suppress knownConditionTrueFalse
  bool pidl_ok = cam_i2c_read(0x0B, &pidl);
  // cppcheck-suppress knownConditionTrueFalse
  const char *pidl_status = pidl_ok ? "OK" : "FAIL";
  printf("[camera_init] PIDL read: %s, value=0x%02X (expected 0x%02X, alt 0x41)\n", pidl_status,
         pidl, OV2640_CHIPID_LOW);
  // cppcheck-suppress knownConditionTrueFalse
  if (!pidl_ok || (pidl != OV2640_CHIPID_LOW && pidl != 0x41))
  {
    printf("[camera_init] FAIL: Chip ID low mismatch\n");
    return false;
  }

  printf("[camera_init] Chip ID verified: 0x%02X/0x%02X\n", pidh, pidl);

  /* SCCB bridge was probed in step 2.5 above (inside PICO_BUILD) */

  /*
   * PHASED WRITE STRATEGY
   *
   * After SW reset, the OV2640 is in DSP bank by default.  The CPLD SCCB
   * bridge cannot handle a second {0xFF, 0x00} (bank switch to DSP) after
   * leaving DSP bank.  The first {0xFF, 0x00} (entry 0 — no-op in DSP bank)
   * works fine; {0xFF, 0x01} (switch to SENSOR) works; but the second
   * {0xFF, 0x00} (entry 64 — back to DSP) kills I2C reads.
   *
   * Solution: write ALL DSP bank registers FIRST (while in DSP bank), THEN
   * switch to SENSOR bank ONCE and write all SENSOR registers.  Never
   * attempt to switch back to DSP.
   */
  printf("[camera_init] PHASE 1: DSP bank registers\n");

  /* DSP reserved init: entries 0-2 from JPEG_INIT */
  printf("  Writing DSP reserved init (3 entries)...\n");
  if (!cam_write_reg_table(OV2640_JPEG_INIT, 3))
  {
    printf("[camera_init] FAIL: DSP reserved init\n");
    return false;
  }

  /* DSP image processing: entries 65-189 from JPEG_INIT */
  printf("  Writing DSP image processing (%zu entries)...\n",
         sizeof(OV2640_JPEG_INIT) / sizeof(OV2640_JPEG_INIT[0]) - 1 - 65);
  if (!cam_write_reg_table(OV2640_JPEG_INIT + 65, 125))
  {
    printf("[camera_init] FAIL: DSP image processing\n");
    return false;
  }

  /* YUV422 DSP registers (skip bank switch at index 0) */
  printf("  Writing YUV422 DSP registers...\n");
  if (!cam_write_reg_table(OV2640_YUV422 + 1, 7))
  {
    printf("[camera_init] FAIL: YUV422 DSP regs\n");
    return false;
  }

  /* Resolution DSP registers for QVGA (skip bank switch at index 23) */
  printf("  Writing QVGA DSP scaler registers...\n");
  if (!cam_write_reg_table(OV2640_320x240_JPEG + 24, 15))
  {
    printf("[camera_init] FAIL: QVGA DSP scaler\n");
    return false;
  }

  /* Switch to SENSOR bank ({0xFF, 0x01} — confirmed working) */
  printf("[camera_init] Switching to SENSOR bank...\n");
  cam_i2c_write(0xFF, 0x01);

  printf("[camera_init] PHASE 2: SENSOR bank registers\n");

  /* SENSOR bank: entries 4-63 from JPEG_INIT */
  printf("  Writing SENSOR bank registers (%zu entries)...\n",
         sizeof(OV2640_JPEG_INIT) / sizeof(OV2640_JPEG_INIT[0]) - 1 - 64);
  if (!cam_write_reg_table(OV2640_JPEG_INIT + 4, 60))
  {
    printf("[camera_init] FAIL: SENSOR bank\n");
    return false;
  }

  /* Resolution SENSOR window registers (skip bank switch at index 0) */
  printf("  Writing QVGA SENSOR window registers...\n");
  if (!cam_write_reg_table(OV2640_320x240_JPEG + 1, 22))
  {
    printf("[camera_init] FAIL: QVGA SENSOR window\n");
    return false;
  }

#if defined(PICO_BUILD)
  /*
   * PHASE 3: Enable JPEG output with 0xE0 commit cycle.
   *
   * The OV2640 DSP pipeline uses 0xE0 as a "change enable → commit"
   * register.  Without the 0xE0=0x14 / 0xE0=0x00 cycle, the DSP
   * may ignore format changes (0xDA=0x10, JPEG mode).
   *
   * This requires a brief switch to DSP bank (0xFF=0x00).  Earlier
   * attempts showed that repeated {0xFF, 0x00} bank switches after
   * SENSOR bank kill I2C reads, but WRITES still go through (the
   * CPLD bridge only blocks reads).  Since we don't rely on I2C
   * reads for capture, this is safe.
   */
  printf("[camera_init] PHASE 3: JPEG output enable (with 0xE0 commit)\n");

  /* Switch to DSP bank for output format + commit cycle */
  cam_i2c_write(0xFF, 0x00);
  sleep_ms(2);
  cam_i2c_write(0xE0, 0x14); /* Image mode: change enable */
  sleep_ms(1);
  cam_i2c_write(0xDA, 0x10); /* DSP output format: JPEG */
  sleep_ms(1);
  cam_i2c_write(0xD7, 0x03); /* Image quality */
  sleep_ms(1);
  cam_i2c_write(0xE1, 0x77); /* Image adjustment */
  sleep_ms(1);
  cam_i2c_write(0xE0, 0x00); /* Image mode: commit */
  sleep_ms(2);

  /* Switch to SENSOR bank for JPEG enable + COM7 */
  cam_i2c_write(0xFF, 0x01);
  sleep_ms(2);
  cam_i2c_write(0x04, 0x08); /* Output control: bit 3 = JPEG enable */
  sleep_ms(1);
  cam_i2c_write(0x12, 0x41); /* COM7: UXGA + JPEG format bit */
  sleep_ms(1);
  cam_i2c_write(0x70, 0x00); /* Test pattern: auto mode, disabled */
  sleep_ms(1);
#endif

  /* Post-init verification: SENSOR bank registers only */
  /* NOTE: I2C reads through the CPLD SCCB bridge always return 0xFF on this
   * module revision. Register writes are verified indirectly by successful
   * JPEG capture. Values of 0xFF below are expected and NOT a failure of the
   * write path. */
  {
    uint8_t com7 = 0, out_ctrl = 0;
    // cppcheck-suppress knownConditionTrueFalse
    bool r1 = cam_i2c_read(0x12, &com7);
    // cppcheck-suppress knownConditionTrueFalse
    bool r2 = cam_i2c_read(0x04, &out_ctrl);
    printf("[camera_init] Post-init SENSOR regs: COM7=0x%02X(read=%s) OUT=0x%02X(read=%s)\n", com7,
           // cppcheck-suppress knownConditionTrueFalse
           r1 ? "OK" : "FAIL", out_ctrl, r2 ? "OK" : "FAIL");
  }

  /* Configure VSYNC polarity on the Arducam CPLD */
  printf("[camera_init] Writing ARDUCHIP_TIM (0x03) = 0x%02X...\n", VSYNC_LEVEL_MASK);
  cam_spi_write(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);

  printf("[camera_init] Camera initialized OK\n");
  s_camera_inited = true;
  return true;
}

bool camera_set_resolution(camera_res_t res)
{
  if ((unsigned int)res >= RES_TABLE_COUNT)
  {
    return false;
  }

  return cam_write_reg_table(RES_TABLES[res], 0);
}

bool camera_capture(uint32_t timeout_ms)
{
#if defined(PICO_BUILD)
  /* 1. Clear FIFO + start capture (SDK sequence) */
  camera_clear_fifo();
  cam_spi_write(ARDUCHIP_FIFO, 0x01); /* Clear FIFO flag (SDK quirk) */
  cam_spi_write(ARDUCHIP_FIFO, 0x02); /* Start capture */

  /* 2. Poll CAP_DONE */
  uint32_t start = to_ms_since_boot(get_absolute_time());
  while (true)
  {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (cam_spi_read(ARDUCHIP_TRIG) & CAP_DONE_MASK)
    {
      break;
    }
    if (now - start > timeout_ms)
    {
      printf("[camera_capture] TIMEOUT — TRIG=0x%02X FIFO_SIZE=%lu\n", cam_spi_read(ARDUCHIP_TRIG),
             (unsigned long)camera_get_fifo_length());
      return false;
    }
    sleep_ms(5);
  }
#else
  (void)timeout_ms;
#endif

  return true;
}

uint32_t camera_get_fifo_length(void)
{
  uint32_t len1 = cam_spi_read(FIFO_SIZE_1);
  uint32_t len2 = cam_spi_read(FIFO_SIZE_2);
  uint32_t len3 = cam_spi_read(FIFO_SIZE_3) & 0x7f;
  return (len3 << 16) | (len2 << 8) | len1;
}

bool camera_read_fifo_burst(uint8_t *buffer, size_t length)
{
  if (!buffer || length == 0)
  {
    return false;
  }

#if defined(PICO_BUILD)
  /*
   * ArduCAM B0067 (SDRAM-based) burst FIFO read protocol:
   *
   *   CS LOW (held throughout)
   *     → send 0x3C (BURST_READ_FIFO command)
   *     → send 0x00 (dummy byte — SDRAM controller needs 1 setup cycle)
   *     → read N bytes of FIFO data
   *   CS HIGH
   *
   * IMPORTANT: the first real FIFO byte comes back on MISO during the dummy
   * transfer (hdr_rx[1]).  The subsequent spi_read_blocking starts at FIFO
   * byte 1.  We must capture hdr_rx[1] as buffer[0] and read only length-1
   * additional bytes to avoid an off-by-one shift that truncates the JPEG SOI
   * (FF D8 → D8 without the leading FF).
   */
  spi_payload_cs_select(SPI_CS_CAM_PIN);

  /* Phase 1+2: send command + dummy — hdr_rx[1] is the first FIFO byte */
  uint8_t hdr_tx[2] = {BURST_READ_FIFO, 0x00};
  uint8_t hdr_rx[2];
  spi_write_read_blocking(SPI0_PORT, hdr_tx, hdr_rx, 2);
  buffer[0] = hdr_rx[1]; /* first real FIFO byte */

  /* Phase 3: read remaining bytes (CS stays low) */
  if (length > 1)
  {
    spi_read_blocking(SPI0_PORT, 0x00, buffer + 1, length - 1);
  }

  spi_payload_cs_deselect(SPI_CS_CAM_PIN);
#else
  memset(buffer, 0xAA, length);
#endif

  return true;
}

bool camera_arduchip_diagnostic(void)
{
#if defined(PICO_BUILD)
  printf("\n========== ARDUCHIP DIAGNOSTIC ==========\n");

  /* Test 1: write 0x55, read back */
  cam_spi_write(ARDUCHIP_TEST1, 0x55);
  sleep_ms(1);
  uint8_t rb1 = cam_spi_read(ARDUCHIP_TEST1);

  /* Test 2: write 0xAA, read back */
  cam_spi_write(ARDUCHIP_TEST1, 0xAA);
  sleep_ms(1);
  uint8_t rb2 = cam_spi_read(ARDUCHIP_TEST1);

  /* Restore known value */
  cam_spi_write(ARDUCHIP_TEST1, 0x55);

  printf("  TEST1 write 0x55, readback: 0x%02X\n", rb1);
  printf("  TEST1 write 0xAA, readback: 0x%02X\n", rb2);

  bool genuine = (rb1 == 0x55 && rb2 == 0xAA);
  printf("  Verdict: %s\n",
         genuine ? "GENUINE ArduChip (R/W)" : "CLONE/FAKE CPLD (read-only or fixed)");
  printf("=========================================\n\n");
  return genuine;
#else
  printf("[camera_arduchip_diagnostic] Host build — skipping\n");
  return false;
#endif
}

void camera_clear_fifo(void)
{
  cam_spi_write(ARDUCHIP_FIFO, 0x01);
}

bool camera_write_sensor_reg(uint8_t reg, uint8_t val)
{
#if defined(PICO_BUILD)
  cam_i2c_write(0xFF, 0x01); /* ensure SENSOR bank */
  sleep_ms(2);
  return cam_i2c_write(reg, val);
#else
  (void)reg;
  (void)val;
  return true;
#endif
}

bool camera_read_sensor_reg(uint8_t reg, uint8_t *val)
{
  if (!val)
  {
    return false;
  }
#if defined(PICO_BUILD)
  cam_i2c_write(0xFF, 0x01); /* ensure SENSOR bank */
  sleep_ms(2);
  cam_i2c_write(0xFF, 0x01); /* some CPLD bridges need the bank select before each read */
  sleep_ms(2);
  return cam_i2c_read(reg, val);
#else
  (void)reg;
  *val = 0xFF; /* sentinel for host tests */
  return true;
#endif
}

void camera_diagnostic_readback(void)
{
#if defined(PICO_BUILD)
  printf("\n========== SENSOR REGISTER READBACK DIAGNOSTIC ==========\n");

  /*
   * Test 1: Read current COM7 without writing.
   * Expected: 0x41 (UXGA + JPEG) or 0x42 (after test pattern enable).
   */
  uint8_t cur_com7 = 0xFF;
  bool r1 = camera_read_sensor_reg(0x12, &cur_com7);
  printf("  Current COM7(0x12) = 0x%02X (read=%s)\n", cur_com7, r1 ? "OK" : "FAIL");

  /*
   * Test 2: Write known value to COM7, read back.
   * Use 0x40 (gray mode, test pattern off) — safe to write.
   */
  bool w2 = camera_write_sensor_reg(0x12, 0x40);
  sleep_ms(5);
  uint8_t test_com7 = 0xFF;
  bool r2 = camera_read_sensor_reg(0x12, &test_com7);
  printf("  Write COM7=0x40 → readback=0x%02X (write=%s read=%s match=%s)\n", test_com7,
         w2 ? "OK" : "FAIL", r2 ? "OK" : "FAIL", (r2 && test_com7 == 0x40) ? "YES" : "NO");

  /*
   * Test 3: Test 0x04 (output control) with 0x00 → 0x08 → restore.
   */
  uint8_t cur_out = 0xFF;
  camera_read_sensor_reg(0x04, &cur_out);
  printf("  Current 0x04(OUT) = 0x%02X\n", cur_out);

  bool w3 = camera_write_sensor_reg(0x04, 0x00);
  sleep_ms(3);
  uint8_t test_out0 = 0xFF;
  bool r3 = camera_read_sensor_reg(0x04, &test_out0);
  printf("  Write OUT=0x00 → readback=0x%02X (write=%s read=%s match=%s)\n", test_out0,
         w3 ? "OK" : "FAIL", r3 ? "OK" : "FAIL", (r3 && test_out0 == 0x00) ? "YES" : "NO");

  bool w4 = camera_write_sensor_reg(0x04, 0x08);
  sleep_ms(3);
  uint8_t test_out8 = 0xFF;
  bool r4 = camera_read_sensor_reg(0x04, &test_out8);
  printf("  Write OUT=0x08 → readback=0x%02X (write=%s read=%s match=%s)\n", test_out8,
         w4 ? "OK" : "FAIL", r4 ? "OK" : "FAIL", (r4 && test_out8 == 0x08) ? "YES" : "NO");

  /*
   * Restore critical registers to safe state for JPEG capture.
   */
  camera_write_sensor_reg(0x12, 0x41); /* COM7: UXGA + JPEG */
  sleep_ms(3);
  uint8_t final_com7 = 0xFF;
  camera_read_sensor_reg(0x12, &final_com7);
  printf("  FINAL COM7=%s restored to 0x%02X (target 0x41)\n",
         (final_com7 == 0x41) ? "OK" : "MISMATCH", final_com7);

  printf("==========================================================\n\n");
#else
  printf("[camera_diagnostic_readback] Host build — skipping\n");
#endif
}

/**
 * @brief Enable OV2640 internal color bar test pattern.
 *
 * Writes COM7 (0x12) bit 1 = 1 on the SENSOR bank to enable the
 * on-chip test pattern generator.  The sensor outputs vertical color
 * bars / gray ramp instead of pixel data.
 *
 * This is a diagnostic that does NOT depend on I2C reads — only writes.
 * If the capture data changes from the usual "00 00 00 00 00 00 13 A2..."
 * pattern, we know the I2C write path works and JPEG config is the issue.
 * If data is unchanged, the writes may not be reaching the sensor.
 *
 * @param enable  true = set test pattern, false = restore to JPEG output
 */
void camera_set_test_pattern(bool enable)
{
#if defined(PICO_BUILD)
  if (enable)
  {
    printf("\n========== OV2640 TEST PATTERN EXPERIMENT ==========\n");
    printf("  Setting 0xFF=0x01 (SENSOR bank)\n");
    cam_i2c_write(0xFF, 0x01);
    sleep_ms(2);
    /* COM7 = 0x42 = bit 6 (gray output) + bit 1 (color bar test pattern)
     * Rest of bits left default (no JPEG, no QVGA/UXGA toggle) */
    printf("  Setting 0x12=0x42 (COM7: gray + test pattern)\n");
    cam_i2c_write(0x12, 0x42);
    sleep_ms(2);
    printf("  Test pattern ENABLED\n");
    printf("===================================================\n\n");
  }
  else
  {
    printf("[camera_set_test_pattern] Restoring normal output...\n");
    /* Restore COM7 to the value set by JPEG init tables (0x40 = gray) */
    cam_i2c_write(0xFF, 0x01);
    sleep_ms(2);
    cam_i2c_write(0x12, 0x40);
    sleep_ms(2);
    printf("[camera_set_test_pattern] Normal output restored\n");
  }
#else
  (void)enable;
  printf("[camera_set_test_pattern] Host build — skipping\n");
#endif
}

void camera_verify_jpeg_config(void)
{
#if defined(PICO_BUILD)
  printf("\n========== OV2640 JPEG CONFIG VERIFICATION ==========\n");

  /*
   * DIAG: Re-init I2C1 before read test.
   * After ~200 I2C writes in camera_init(), the RP2350 I2C peripheral
   * may be in a stale state where reads return 0xFF.  De-init + re-init
   * tells us if the problem is the peripheral (transient) or the OV2640
   * (not responding to reads after config).
   */
  printf("  [DIAG] Re-initializing I2C1 for read diagnostic...\n");
  i2c_deinit(I2C1_PORT);
  sleep_ms(1);
  i2c_init(I2C1_PORT, 100000);
  gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C1_SDA_PIN);
  gpio_pull_up(I2C1_SCL_PIN);
  sleep_ms(5);
  printf("  [DIAG] I2C1 re-init complete — reading SENSOR bank regs...\n");

  /* Ensure SENSOR bank — DO NOT write {0xFF, 0x00} (DSP bank switch) as
   * it may cause I2C bus failure on this hardware (CPLD SCCB bridge bug).
   * Only read SENSOR bank registers to verify the write took effect. */
  bool bank_ok = cam_i2c_write(0xFF, 0x01);
  sleep_ms(5);

  uint8_t com7 = 0, out_ctrl = 0;
  bool com7_ok = cam_i2c_read(0x12, &com7);
  bool out_ok = cam_i2c_read(0x04, &out_ctrl);
  printf("  SENSOR bank: BANK select write=%s\n", bank_ok ? "OK" : "FAIL");
  printf("    0x12 (COM7)        read=%s val=0x%02X (bit0=%d→%s)\n", com7_ok ? "OK" : "FAIL", com7,
         (com7 & 1) ? 1 : 0, (com7 & 1) ? "RAW/JPEG" : "YUV/RGB");
  printf("    0x04 (output ctrl) read=%s val=0x%02X (bit3=%d→%s)\n", out_ok ? "OK" : "FAIL",
         out_ctrl, (out_ctrl & 0x08) ? 1 : 0, (out_ctrl & 0x08) ? "JPEG enable" : "JPEG DISABLED");

  printf("====================================================\n\n");
#else
  printf("[camera_verify_jpeg_config] Host build — skipping\n");
#endif
}
