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

/* Forward declaration — cam_sccb_write is defined after cam_spi_read */
static bool cam_sccb_write(uint8_t reg, uint8_t val);

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
    if (!cam_sccb_write(p->reg, p->val))
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
 * SPI-driven SCCB write to OV2640 (matching official ArduCAM library).
 *
 * The official wrSensorReg8_8 does NOT use direct I2C.  It sends register
 * writes through the CPLD's SCCB bridge using SPI registers:
 *   0x01 — clear FIFO/status
 *   0x20 — SCCB slave ID (0x30 = OV2640 address)
 *   0x07 — bridge enable (bit 5)
 *   0x03 — register address (ARDUCHIP_TIM alias)
 *   0x02 — data (triggers SCCB write cycle)
 *
 * This differs from cam_i2c_write which writes directly to I2C1.
 * Some CPLD revisions require the SPI-driven method for correct
 * SCCB timing to the OV2640.
 */
static bool cam_sccb_write(uint8_t reg, uint8_t val)
{
  cam_spi_write(0x01, 0x00); /* Clear status */
  cam_spi_write(0x20, 0x30); /* SCCB_ID = OV2640 (0x30) */
  cam_spi_write(0x07, 0x20); /* Enable SCCB bridge bit 5 */
  cam_spi_write(0x03, reg);  /* Register address */
  cam_spi_write(0x02, val);  /* Data (triggers SCCB write) */
  return true;
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
  i2c_init(I2C1_PORT, 100000);
  gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C1_SDA_PIN);
  gpio_pull_up(I2C1_SCL_PIN);

  /* CPLD reset + GPIO config (RST=1, PD=0, PWR_EN=1) */
  cam_spi_write(0x07, 0x80);
  sleep_ms(100);
  cam_spi_write(0x07, 0x00);
  sleep_ms(100);
  cam_spi_write(0x05, 0x07);
  sleep_ms(5);
  cam_spi_write(0x06, 0x05);
  sleep_ms(10);

  /* SPI register access check */
  cam_spi_write(ARDUCHIP_TEST1, 0x55);
  sleep_ms(1);
  if (cam_spi_read(ARDUCHIP_TEST1) != 0x55)
  {
    printf("[camera_init] FAIL: SPI TEST1 readback mismatch\n");
    return false;
  }
  printf("[camera_init] SPI OK\n");

  /* Enable CPLD SCCB bridge — try 0x20, fallback brute-force */
  bool sccb_bridge_found = false;
  const uint8_t sccb_id_candidates[] = {0x20, 0x24};
  for (size_t ci = 0; ci < sizeof(sccb_id_candidates) && !sccb_bridge_found; ci++)
  {
    uint8_t id_reg = sccb_id_candidates[ci];
    cam_spi_write(0x01, 0x00);
    sleep_ms(5);
    cam_spi_write(id_reg, 0x30);
    sleep_ms(5);
    cam_spi_write(0x07, 0x20);
    cam_spi_write(0x03, VSYNC_LEVEL_MASK);
    sleep_ms(50);

    if (cam_spi_read(0x07) & 0x20)
    {
      sccb_bridge_found = true;
      break;
    }
  }

  if (!sccb_bridge_found)
  {
    for (uint8_t id_reg = 0x00; id_reg < 0x40 && !sccb_bridge_found; id_reg++)
    {
      if (id_reg == 0x07)
        continue;

      cam_spi_write(0x01, 0x00);
      sleep_ms(2);
      cam_spi_write(id_reg, 0x30);
      sleep_ms(2);
      cam_spi_write(0x07, 0x20);
      sleep_ms(20);

      if (cam_spi_read(0x07) & 0x20)
      {
        sccb_bridge_found = true;
        break;
      }
    }
  }

  /* Restore GPIO registers after brute-force scan */
  cam_spi_write(0x05, 0x07);
  sleep_ms(2);
  cam_spi_write(0x06, 0x05);
  sleep_ms(10);

  printf("[camera_init] SCCB bridge: %s\n", sccb_bridge_found ? "ENABLED" : "NOT FOUND");

  /* Pre-reset Chip ID */
  {
    uint8_t pidh = 0, pidl = 0, com7 = 0;
    cam_i2c_read(0x0A, &pidh);
    cam_i2c_read(0x0B, &pidl);
    cam_i2c_read(0x12, &com7);
    printf("[camera_init] Pre-reset: PIDH=0x%02X PIDL=0x%02X COM7=0x%02X\n", pidh, pidl, com7);
  }

  /* SW reset */
  cam_i2c_write(0x12, 0x80);
  sleep_ms(100);

  /* Post-reset Chip ID */
  {
    uint8_t pidh = 0, pidl = 0, com7 = 0;
    cam_i2c_read(0x0A, &pidh);
    cam_i2c_read(0x0B, &pidl);
    cam_i2c_read(0x12, &com7);
    printf("[camera_init] Post-reset: PIDH=0x%02X PIDL=0x%02X COM7=0x%02X\n", pidh, pidl, com7);
  }
#else
  printf("[camera_init] Host build — skipping HW init\n");
#endif

  /* Verify Chip ID */
  uint8_t pidh = 0, pidl = 0;
  bool pidh_ok = cam_i2c_read(0x0A, &pidh);
  if (!pidh_ok || pidh != OV2640_CHIPID_HIGH)
  {
    printf("[camera_init] FAIL: Chip ID high — got 0x%02X, expected 0x%02X\n", pidh,
           OV2640_CHIPID_HIGH);
    return false;
  }
  bool pidl_ok = cam_i2c_read(0x0B, &pidl);
  if (!pidl_ok || (pidl != OV2640_CHIPID_LOW && pidl != 0x41))
  {
    printf("[camera_init] FAIL: Chip ID low — got 0x%02X\n", pidl);
    return false;
  }
  printf("[camera_init] Chip ID: 0x%02X/0x%02X\n", pidh, pidl);

  /*
   * OV2640 register init sequence (ArduCAM SDK reference order):
   *   JPEG_INIT → YUV422 → JPEG → 0xFF=0x01/0x15=0x00 → resolution table
   */
  printf("[camera_init] Writing register tables...\n");
  if (!cam_write_reg_table(OV2640_JPEG_INIT, 0)
      || !cam_write_reg_table(OV2640_YUV422, 0)
      || !cam_write_reg_table(OV2640_JPEG, 0))
  {
    printf("[camera_init] FAIL: register table write\n");
    return false;
  }

  /* SENSOR bank guard + 0x15 */
#if defined(PICO_BUILD)
  cam_sccb_write(0xFF, 0x01);
  sleep_ms(2);
  cam_sccb_write(0x15, 0x00);
  sleep_ms(1);
#else
  cam_i2c_write(0xFF, 0x01);
  cam_i2c_write(0x15, 0x00);
#endif

  if (!cam_write_reg_table(OV2640_320x240_JPEG, 0))
  {
    printf("[camera_init] FAIL: resolution table write\n");
    return false;
  }

  /* SENSOR bank for any post-init I2C access */
  cam_i2c_write(0xFF, 0x01);

  /* Configure VSYNC polarity on the CPLD */
  cam_spi_write(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);

  /* Continuous XCLK for sensor timing generator */
#if defined(PICO_BUILD)
  camera_enable_xclk();
#endif

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
  /* Restore SCK for SPI (XCLK was left running after init) */
  camera_restore_sck();
  sleep_us(100);

  /* Clear FIFO + start capture */
  camera_clear_fifo();
  cam_spi_write(ARDUCHIP_FIFO, 0x01); /* Clear FIFO flag */
  cam_spi_write(ARDUCHIP_FIFO, 0x02); /* Start capture */

  /* Continuous XCLK for sensor during frame wait */
  camera_enable_xclk();

  /* Poll CAP_DONE */
  uint32_t start = to_ms_since_boot(get_absolute_time());
  bool done = false;
  while ((to_ms_since_boot(get_absolute_time()) - start) < timeout_ms)
  {
    camera_restore_sck();
    if (cam_spi_read(ARDUCHIP_TRIG) & CAP_DONE_MASK)
    {
      done = true;
      break;
    }
    /* Switch back to XCLK for sensor readout */
    camera_enable_xclk();
    sleep_ms(5);
  }

  /* Final restore: SCK needed for subsequent FIFO burst read */
  camera_restore_sck();

  if (done)
  {
    printf("[camera_capture] CAP_DONE — FIFO_SIZE=%lu\n",
           (unsigned long)camera_get_fifo_length());
  }

  if (!done)
  {
    printf("[camera_capture] TIMEOUT — TRIG=0x%02X FIFO_SIZE=%lu\n", cam_spi_read(ARDUCHIP_TRIG),
           (unsigned long)camera_get_fifo_length());
    return false;
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


