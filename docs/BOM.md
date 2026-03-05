# Bill of Materials (BOM) — CubeSat OBC Hardware

**Document ID**: BOM-OBC-001  
**Version**: 0.1 (Draft)  
**Date**: 2026-03-05  
**Branch**: `feature/hardware-bom`  
**Status**: 🔄 En construcción — agregar partes a medida que se evalúan

---

## 1. Propósito

Este documento lista todos los componentes de hardware necesarios para ensamblar
el prototipo de vuelo del OBC (On-Board Computer) basado en la Raspberry Pi Pico 2W
(RP2350, Cortex-M33). Se incluye el estado de compatibilidad con el firmware actual
y notas de integración.

**Leyenda de estado:**
| Símbolo | Significado |
|---------|-------------|
| ✅ Integrado | Driver y requisito implementados en el firmware |
| 🔄 Planificado | Partes evaluadas, driver pendiente de desarrollo |
| ❓ Por evaluar | Candidato, análisis de compatibilidad pendiente |
| ❌ Descartado | Incompatible o sustituido por otro componente |

---

## 2. Procesador / OBC Central

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 1 | Microcontrolador OBC | Raspberry Pi Pico 2W | 2 | ✅ Integrado | RP2350 (Cortex-M33 dual-core, 520 KB SRAM, Wi-Fi/BT); 1 vuelo + 1 spare |

**Referencia firmware**: `config/pico_pins.h`, `src/CMakeLists.txt`

---

## 3. Sensores de Actitud (ADCS)

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 2 | IMU 6-DOF | MPU-6050 (módulo GY-521) | 2 | ✅ Integrado | I2C @ 400 kHz, addr 0x68; GPIO4 (SDA), GPIO5 (SCL) |
| 3 | Magnetómetro 3-ejes | HMC5883L (módulo GY-271) | 2 | ✅ Integrado | I2C bus I2C0; driver `src/drivers/mag/hmc5883l.c` — Phase 5 |

**Notas de integración — Sensores de actitud:**
- IMU y magnetómetro comparten bus I2C0 (`GPIO4`/`GPIO5`, fast-mode 400 kHz).
- El EKF fusiona acelerómetro + giróscopo + magnetómetro (3-state: roll/pitch/yaw).
- Referencia: `src/tasks/sensor_read_task.c`, `include/ekf.h`

---

## 4. Navegación / Posicionamiento

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 4 | Módulo GPS | GY-NEO6Mv2 con NEO-7M + antena | 2 | 🔄 Planificado | UART @ 9600 baud, 3.3V; requiere liberar UART0 — ver §4.1 |

### 4.1 Compatibilidad GPS GY-NEO6Mv2 / NEO-7M

**Hardware**: ✅ Compatible con condición  
**Software**: 🔄 Driver y task de FreeRTOS pendientes de desarrollo

| Característica | GY-NEO6Mv2 (NEO-7M) | RP2350 / Pico 2W | Estado |
|---|---|---|---|
| Protocolo | UART (NMEA 0183) | 2× UART hardware | ✅ |
| Voltaje lógico | 3.3 V | GPIO a 3.3 V | ✅ |
| Voltaje supply | 3.3–5 V (regulador onboard) | 3.3 V disponible | ✅ |
| Antena | Pasiva cerámica (incluida) | N/A | ✅ |
| Baud rate default | 9600 | Configurable | ✅ |

**Conflicto de UART** (debe resolverse antes de comprar):

| Puerto | Pines | Uso actual | Disponible para GPS |
|--------|-------|-----------|---------------------|
| UART0 | GPIO0/GPIO1 | Debug ASCII @ 115200 | ⚠️ Liberable si debug→USB CDC |
| UART1 | GPIO4/GPIO5 | CSP/KISS telemetry @ 115200 | ❌ Ocupado |

**Solución seleccionada: Opción A — Liberar UART0 para GPS**
> Mover todo el debug output a USB CDC (ya habilitado en `src/CMakeLists.txt`,
> `pico_enable_stdio_usb = 1`). UART0 queda libre para recibir sentencias NMEA
> del GPS. Cambio de firmware estimado: mínimo (1 línea en `pico_pins.h` + driver NMEA).

**Trabajo de firmware requerido** (Phase 7 o nueva tarea):
- [ ] Driver NMEA parser (`src/drivers/gps/neo7m.c`)
- [ ] Tarea FreeRTOS GPS (`src/tasks/gps_task.c`) — 1 Hz
- [ ] Escritura al Data Layer (`data_layer_write_gps()`)
- [ ] Requisito en SyRS (`SYS-F-GPS-001` — posición orbital)
- [ ] Tests unitarios (`tests/unit/test_gps.c`)

---

## 5. Gestión de Energía

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 5 | Sensor temperatura | TMP102 o ADC4 interno | 1 | ✅ Integrado | Modo ADC4 activo; I2C addr 0x48 si externo |

> ⚠️ **Por definir**: regulador de voltaje del bus de batería, panel solar, EPS (Electrical Power System). Agregar en próxima iteración del BOM.

---

## 6. Comunicaciones (TT&C)

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 6 | Transceiver TT&C | EBYTE E22-400M30S (SX1268, 433 MHz LoRa) | 2 | 🔄 Planificado | UART transparente 3.3V, 30 dBm (1W); ver §6.1 |
| 6b | *(alternativa banco de pruebas)* | HC-12 (433 MHz FSK, TTL UART) | 2 | 🔄 Planificado | Solo para testing en tierra; 100 mW, $3/ud |

### 6.1 Análisis de transceivers UHF/VHF

**Requisitos del firmware para el TT&C:**
- Interfaz: **UART1** (`GPIO4 TX` / `GPIO5 RX`) @ **115200 baud**, 3.3 V TTL
- Protocolo: **KISS framing** sobre la capa física → el radio debe actuar como **pipe UART transparente**
- Stack: CSP v2 (OBC addr 10, GS addr 1); telemetría 1 Hz @ 29 bytes/paquete

> Referencia firmware: [docs/PHASE3_COMM_SPEC.md](PHASE3_COMM_SPEC.md), `src/drivers/uart/pico_usart.c`

**Candidatos evaluados:**

| Módulo | Frecuencia | Interfaz | Potencia | KISS-compatible | Precio est. | Veredicto |
|--------|-----------|---------|---------|----------------|------------|----------|
| **EBYTE E22-400M30S** (SX1268) | 410–493 MHz | UART TTL 3.3V | 30 dBm (1W) | ✅ Modo transparente | ~$15 | ✅ **Recomendado** |
| **EBYTE E22-900M30S** (SX1262) | 850–930 MHz | UART TTL 3.3V | 30 dBm (1W) | ✅ Modo transparente | ~$15 | ✅ Alternativa (banda diferente) |
| **HC-12** (Si4463) | 433 MHz FSK | UART TTL 3.3V | 20 dBm (100 mW) | ✅ Modo transparente | ~$3 | ⚠️ Solo prototipo tierra |
| **Dorji DRA818U** | 400–470 MHz UHF | UART (AT cmd) + audio analógico | 1W | ❌ Audio analógico, necesita TNC | ~$8 | ❌ No compatible directo |
| **AX5043** (IC) | Multi-banda sub-GHz | **SPI** | configurable | ❌ SPI → requiere nuevo driver | ~$10 (IC) | ❌ Cambio de arquitectura |
| **RFM98W** / SX1276 | 433/868/915 MHz | **SPI** | 20 dBm | ❌ SPI | ~$5 | ❌ Cambio de arquitectura |

**¿Por qué el E22-400M30S es el recomendado?**

1. **UART transparente nativo**: en modo de operación normal actúa como cable serial RF → el stack KISS/CSP del firmware actual funciona sin modificaciones.
2. **TTL 3.3 V**: compatible directo con GPIO del RP2350, sin level-shifter.
3. **30 dBm (1 W)**: potencia de TX adecuada para enlace LEO (~600 km) con antena de dipolo.
4. **LoRa + FSK**: configurable; para LEO se recomienda FSK @ 9600–115200 baud o LoRa SF7 para mayor link budget.
5. **Banda 433 MHz (UHF)**: alineada con frecuencias de satélites amateur (IARU Region 2: 435–438 MHz).

**Circuito de conexión (sin level-shifter necesario):**
```
Pico 2W                    E22-400M30S
GPIO4 (TX) ─────────────▶ RXD
GPIO5 (RX) ◀───────────── TXD
3.3V       ─────────────▶ VCC
GND        ─────────────▶ GND
GPIO[libre]─────────────▶ M0  (modo: 00 = transparente)
GPIO[libre]─────────────▶ M1
GPIO[libre]─────────────▶ AUX (busy/ready flag, opcional)
```

**Trabajo de firmware requerido** (mínimo — compatibilidad directa):
- [ ] Confirmar baud rate del E22 configurado a 115200 (configurable vía AT antes del vuelo)
- [ ] Agregar configuración M0/M1/AUX en `pico_pins.h` con GPIOs libres
- [ ] Test end-to-end KISS/CSP con hardware real

**Notas de RF para LEO:**
- Requiere licencia amateur (IARU coordinar frecuencia) o banda ISM 433 MHz (potencia limitada a 10 mW en algunos países en ISM — verificar regulación local)
- Antena: dipolo 1/4 onda (~16.4 cm a 434 MHz) o antena helicoidal para mayor ganancia
- Link budget LEO 600 km con dipolo: ~-120 dBm recibido @ 1W TX → viable con E22 sensibilidad típica -148 dBm (LoRa SF12)

---

## 7. Actuadores (ADCS)

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 7 | Reaction Wheels | (a definir) | 3 | ❓ Por evaluar | Control PWM/SPI; 3 ejes ortogonales |
| 8 | Magnetorquers | (a definir) | 3 | ❓ Por evaluar | Control PWM; de-saturation via `B×L` dump (Phase 5) |

---

## 8. Misceláneos / Pasivos

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 9 | Resistencias pull-up I2C | 4.7 kΩ 0402 | 4 | ❓ Por evaluar | Para SDA/SCL de I2C0 e I2C1 |
| 10 | Conector debug | Micro-USB o USB-C | 1 | ✅ Integrado | USB CDC habilitado en firmware |

---

## 9. Resumen de asignación de pines (RP2350 / Pico 2W)

```
GPIO0  — UART0 TX  → 🔄 Reasignar a GPS TX (debug → USB CDC)
GPIO1  — UART0 RX  → 🔄 Reasignar a GPS RX
GPIO2  — I2C1 SDA  (expansión futura)
GPIO3  — I2C1 SCL  (expansión futura)
GPIO4  — I2C0 SDA  / UART1 TX  ← MPU6050 + HMC5883L; UART1 = CSP TT&C
GPIO5  — I2C0 SCL  / UART1 RX  ← MPU6050 + HMC5883L; seleccionar uno
GPIO25 — LED status onboard
ADC4   — Sensor temperatura interno
```

> **Nota**: GPIO4/GPIO5 están mapeados tanto a I2C0 como a UART1. El firmware
> actual activa I2C0 para sensores y UART1 para CSP. No usar simultáneamente.

---

## 10. Pendientes y decisiones abiertas

- [ ] Definir EPS: regulador, batería LiPo, y panel solar
- [ ] Seleccionar transceiver UHF/VHF para TT&C (UART1)
- [ ] Confirmar fabricantes y proveedores (Mouser, DigiKey, AliExpress para prototipo)
- [ ] Validar tolerancia de radiación de componentes seleccionados (LEO environment)
- [ ] Evaluar si el GPS tiene aplicación en órbita real (ventana de visibilidad, TTFF)
- [ ] Resolver asignación GPIO4/GPIO5: documentar si I2C0 y UART1 se multiplexan en tiempo o son configuraciones compiladas distintas

---

## 11. Historial de cambios

| Versión | Fecha | Autor | Descripción |
|---------|-------|-------|-------------|
| 0.1 | 2026-03-05 | — | Creación inicial; GPS GY-NEO6Mv2 evaluado; sensores actitud documentados |
