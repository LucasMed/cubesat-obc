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

## 6. Comunicaciones

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 6 | Enlace TT&C | (a definir — radio/transceiver UHF/VHF) | 1 | ❓ Por evaluar | Conecta a UART1 (CSP/KISS @ 115200 baud) |

> UART1 (`GPIO4 TX`, `GPIO5 RX`) está reservado para el uplink/downlink CSP.
> El transceiver debe ser compatible con niveles lógicos 3.3 V y RS232 o TTL UART.

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
