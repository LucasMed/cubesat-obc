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
| 6 | Transceiver TT&C (vuelo) | EBYTE E22-400M30S (SX1268, 433 MHz LoRa) | 2 | 🔄 Planificado | UART transparente 3.3V, 30 dBm (1W); ver §6.1 — **comprar para vuelo** |
| 6b | Transceiver TT&C (lab/GS) | HC-12 Si4463 (433 MHz FSK, TTL UART) | 2 | 🔄 **Comprar ahora** | Plug-and-play KISS/CSP; mismo firmware que E22; ~$3/ud — ver §6.2 |

> Ver también **§7 Estación Terrena** para el hardware de ground control.

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

### 6.2 Análisis específico: HC-12 Si4463 433 MHz

**Veredicto: ✅ Excelente para desarrollo/GS — ⚠️ Marginal para vuelo LEO**

El HC-12 es el candidato **más sencillo de integrar** de todos los evaluados: UART transparente, misma banda (433 MHz), y compatible con KISS/CSP sin ningún cambio de firmware.

| Característica | HC-12 (Si4463) | E22-400M30S (SX1268) |
|---|---|---|
| Interfaz con Pico 2W | ✅ UART TTL directo (3.3V–5V) | ✅ UART TTL directo (3.3V) |
| KISS/CSP plug-and-play | ✅ Sí, modo FU3 = pipe transparente | ✅ Sí, modo transparente |
| Frecuencia | ✅ 433.4–473 MHz configurable | ✅ 410–493 MHz configurable |
| Baud rate | ✅ hasta 115200 bps (AT+Bxxxx) | ✅ hasta 115200 bps |
| Potencia TX | ⚠️ 20 dBm (100 mW) | ✅ 30 dBm (1 W) |
| Sensibilidad RX | ⚠️ −117 dBm @ 5 kbps | ✅ −148 dBm (LoRa SF12) |
| Precio | ✅ ~$3–5 | ~$15 |
| Level-shifter necesario | ✅ No (acepta 3.3V y 5V) | ✅ No |
| Configuración extra (M0/M1/AUX) | ✅ No (solo UART + SET pin opcional) | ⚠️ Sí (3 GPIOs extra) |

#### Circuito de conexión — HC-12 (más simple que el E22):
```
Pico 2W                  HC-12
GPIO4 (TX) ───────────▶ TXD
GPIO5 (RX) ◀─────────── RXD
3.3V       ───────────▶ VCC  (acepta 3.2–5.5V)
GND        ───────────▶ GND
                        SET  (dejar libre = modo normal; GND = modo AT cmd)
```
> Solo 4 cables. Sin pines de modo adicionales en operación normal.

#### Link budget para LEO (600 km):

| Parámetro | HC-12 (100 mW) | E22-400M30S (1 W) |
|-----------|---------------|-------------------|
| EIRP TX | ~23 dBm (con dipolo 3 dBi) | ~33 dBm |
| Path loss LEO 600 km @ 435 MHz | ~148 dB | ~148 dB |
| Señal recibida (dipolo GS 3 dBi) | **−122 dBm** | **−112 dBm** |
| Sensibilidad RX (FSK 9600 bps) | −117 dBm | −125 dBm (FSK) |
| **Margen de enlace** | **−5 dB ❌ (insuficiente)** | **+13 dB ✅** |

> **Conclusión del link budget**: el HC-12 con 100 mW no tiene margen suficiente para un enlace LEO confiable a 600 km con antenas de dipolo. Sería viable solo con antenas yagi de alta ganancia en tierra (≥10 dBi), lo que complica la GS.

#### Roles recomendados y plan de adquisición:

| Rol | HC-12 | E22-400M30S |
|-----|-------|-------------|
| **Pruebas de laboratorio (ahora)** | ✅ **Comprar ahora** — plug-and-play, barato, mismo firmware | ✅ También válido |
| Pruebas campo corto (< 1 km) | ✅ Sobra potencia | ✅ |
| Radio de Estación Terrena (GS) | ✅ Par económico con antena yagi | ✅ Par estándar |
| **Transceiver de vuelo LEO** | ❌ Sin margen de enlace | ✅ **Comprar para vuelo** |

> **Plan sugerido**: comprar 2× HC-12 ya (~$6–10 total) para iniciar todas las pruebas
> de laboratorio KISS/CSP/telemetría. Cuando estén los E22-400M30S para vuelo,
> **el firmware no cambia** — mismo UART, misma configuración, mismos pines.

---

## 7. Estación Terrena (Ground Station)

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| GS-1 | Radio GS | LORA32U4 II 915 MHz + antena IPEX | 1 | 🔄 Planificado | PC→USB→ATmega32U4→SX1276; ver §7.1 |
| GS-2 | Radio GS alternativo | EBYTE E22-400M30S (433 MHz) | 1 | ❓ Por evaluar | Mismo módulo que el satélite; par simétrico |
| GS-3 | PC / Laptop | Cualquier PC Linux/Mac/Win | 1 | ✅ Disponible | Corre el cliente CSP de ground (Phase 3) |

### 7.1 LORA32U4 II como radio de Estación Terrena

**Veredicto para GS: ⚠️ Usable con firmware custom a 915 MHz (solo pruebas en tierra)**

Aunque no es apto para el OBC del satélite (ver §6.1), el LORA32U4 II tiene sentido como radio de la GS para el banco de pruebas:

| Aspecto | Detalle |
|---------|---------|
| **Conexión a PC** | USB nativo (ATmega32U4 tiene USB HW) → aparece como puerto serial `/dev/ttyACM0` |
| **Chip RF** | SX1276 interno → mismo core que muchos módulos LoRa |
| **Firmware requerido** | Bridge serial USB↔LoRa (ej. `RadioLib` o `arduino-lmic` con modo KISS) |
| **Banda** | 915 MHz ISM — válida para pruebas en tierra en Región 2 (Américas); **no apta para vuelo** |
| **Potencia TX** | 20 dBm (100 mW) — suficiente para distancias de banco (< 1 km) |
| **Antena IPEX** | Conector IPEX (U.FL) con cable incluido → conectar antena de dipolo 915 MHz |
| **Precio** | ~$18–22 |

**Flujo de prueba en tierra:**
```
PC (cliente CSP Python/C)
  └─ USB serial ─▶ LORA32U4 II (bridge KISS/LoRa @ 915 MHz)
                        │
                        │ RF 915 MHz
                        │
                   E22-400M30S o segundo LORA32U4 II
                        │
                        └─ UART1 ─▶ Pico 2W OBC (firmware KISS/CSP)
```

> **Nota importante**: Para pruebas tierra-tierra en banda 915 MHz, ambos extremos deben usar 915 MHz — el E22-400M30S por defecto es 433 MHz. Si se elige el LORA32U4 II como GS, el par de tierra sería: **LORA32U4 II (GS, 915) ↔ E22-900M30S (satélite, 915)**. Para vuelo real, migrar a 433/435 MHz y reemplazar el GS con un E22-400M30S conectado por UART a la PC.

**Pendientes para usar el LORA32U4 II como GS:**
- [ ] Escribir/adaptar firmware bridge KISS-serial para ATmega32U4 (Arduino + RadioLib)
- [ ] Validar que el bridge KISS sea bit-a-bit compatible con `csp_if_kiss` del OBC
- [ ] Definir banda final de vuelo (433 vs 915 MHz) para asegurar par correcto

---

## 8. Actuadores (ADCS)

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 7 | Reaction Wheels (vuelo) | (a definir — BLDC custom) | 3 | ❓ Por evaluar | GPIO6/7/8 (PWM3A/3B/4A); ver §8.1 |
| 7b | Reaction Wheels **(lab)** | Motor DC con encoder + driver TB6612 | 3 | 🔄 **Comprar ahora** | Emula inercia; PWM directo en GPIO6/7/8; ver §8.1 |
| 8 | Magnetorquers (vuelo) | Bobina custom en ferrita + H-bridge | 3 | ❓ Por evaluar | GPIO14/15/16 (PWM7A/7B/0A); ver §8.2 |
| 8b | Magnetorquers **(lab)** | Módulo L298N o DRV8833 + bobina | 3 | 🔄 **Comprar ahora** | Control PWM bidireccional; ver §8.2 |

### 8.1 Reaction Wheels — opciones de laboratorio

**Interfaz del firmware:**
- Control por **PWM** en `GPIO6` (RW1), `GPIO7` (RW2), `GPIO8` (RW3) — ver `config/pico_pins.h`
- Modelo de firmware: `reaction_wheel_apply_torque(rw, torque, dt)` → actualiza `rw->omega`
- Parámetros: `RW_MAX_OMEGA_RPM = 4000`, `RW_INERTIA = 0.001 kg·m²`
- El driver PWM de hardware del RP2350 (`hardware_pwm`) ya está disponible en los slices PWM3/PWM4

**Componentes para laboratorio (comprar ahora):**

| Componente | Modelo sugerido | Precio | Función |
|-----------|----------------|--------|---------|
| Motor DC con encoder | GA12-N20 (6V, 100–300 RPM) o N20 micro | ~$3–5 c/u | Simula la rueda de reacción |
| Driver motor H-bridge | TB6612FNG (módulo breakout) | ~$2–3 c/u | Convierte PWM del Pico → corriente bidireccional al motor |
| Disco de inercia | Disco acrílico o metal ~5 cm Ø | ~$1 | Aumenta inercia del eje para comportamiento realista |

**Circuito de conexión — Reaction Wheel × 1 eje (repetir × 3):**
```
Pico 2W                    TB6612FNG            Motor N20
GPIO6 (PWM) ─────────────▶ PWMA          ──▶ AO1/AO2 ──▶ Motor
GPIO_DIR_A  ─────────────▶ AIN1
GPIO_DIR_B  ─────────────▶ AIN2
3.3V        ─────────────▶ VCC (lógica)
VMOTOR 5V   ─────────────▶ VM  (motor)
GND         ─────────────▶ GND, STBY
```

> **Nota**: el firmware modelo actual solo calcula `omega` internamente. El siguiente paso de desarrollo es agregar el HAL PWM que convierta `torque → duty cycle` y lo escriba en `hardware_pwm`. Esto entra en **Phase 7 / actuator HAL**.

**¿Por qué no usar un servo o ESC directamente?**
- Servos/ESC de hobby usan PWM de 50 Hz con pulso 1–2 ms → requiere lógica extra
- TB6612 acepta el PWM de alta frecuencia nativo del RP2350 (hasta ~125 kHz) → más simple y fiel al control real

### 8.2 Magnetorquers — opciones de laboratorio

**Interfaz del firmware:**
- Control por **PWM** en `GPIO14` (X), `GPIO15` (Y), `GPIO16` (Z)
- Firmware: `magnetorquer_set_moment(mq, mx, my, mz)` → establece dipolo magnético [A·m²]
- La de-saturación B×L está implementada en `src/services/adcs/momentum_dump.c` (Phase 5)

**Componentes para laboratorio (comprar ahora):**

| Componente | Modelo sugerido | Precio | Función |
|-----------|----------------|--------|---------|
| Driver H-bridge bidireccional | DRV8833 (módulo) o L9110S | ~$1–2 c/u | Invierte corriente en la bobina (magneto +/-) |
| Bobina electromagnética | Electroimán 5V 12mm (ej. ZYE1-P20/15) o bobina casera en ferrita | ~$2–4 c/u | Genera dipolo magnético proporcional a la corriente |
| Núcleo de ferrita (opcional) | Barra ferrita 8×70 mm | ~$1 c/u | Aumenta permeabilidad → más momento por vuelta |

**Circuito de conexión — Magnetorquer × 1 eje (repetir × 3):**
```
Pico 2W                  DRV8833             Bobina
GPIO14 (PWM) ──────────▶ AIN1 (o IN1)
GPIO_DIR     ──────────▶ AIN2 (o IN2)  ──▶ AOUT1/AOUT2 ──▶ Bobina
3.3V         ──────────▶ VCC
GND          ──────────▶ GND
```

> La dirección de la corriente determina la polaridad del dipolo (+/−) → el firmware debe poder invertir el signo del momento para el B×L dump.

**Qué se puede probar en laboratorio con esto:**
- ✅ Ciclo completo ADCS: EKF → LQR → `magnetorquer_set_moment()` → corriente real en bobina
- ✅ Verificar que el B×L dump genera corriente proporcional al campo magnético medido por HMC5883L
- ✅ Medir campo generado con el propio magnetómetro del sistema (loop cerrado de verdad)
- ⚠️ No simula el torque real en órbita (campo terrestre ~50 µT vs laboratorio con interferencias)

**Resumen plan de adquisición — Actuadores:**

| Etapa | Qué comprar | Costo estimado | Cuándo |
|-------|------------|----------------|--------|
| **Lab ahora** | 3× Motor N20 + 3× TB6612 + 3× DRV8833 + bobinas | ~$30–40 total | Ahora |
| **Vuelo** | BLDC custom de reaction wheel + bobinas de ferrita | Por cotizar | Fase HW final |

---

## 9. Misceláneos / Pasivos

| # | Componente | P/N / Modelo | Cantidad | Estado | Notas |
|---|-----------|-------------|---------|--------|-------|
| 9 | Resistencias pull-up I2C | 4.7 kΩ 0402 | 4 | ❓ Por evaluar | Para SDA/SCL de I2C0 e I2C1 |
| 10 | Conector debug | Micro-USB o USB-C | 1 | ✅ Integrado | USB CDC habilitado en firmware |

---

## 10. Resumen de asignación de pines (RP2350 / Pico 2W)

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

## 11. Pendientes y decisiones abiertas

- [ ] Definir EPS: regulador, batería LiPo, y panel solar
- [ ] Seleccionar transceiver UHF/VHF para TT&C (UART1)
- [ ] Confirmar fabricantes y proveedores (Mouser, DigiKey, AliExpress para prototipo)
- [ ] Validar tolerancia de radiación de componentes seleccionados (LEO environment)
- [ ] Evaluar si el GPS tiene aplicación en órbita real (ventana de visibilidad, TTFF)
- [ ] Resolver asignación GPIO4/GPIO5: documentar si I2C0 y UART1 se multiplexan en tiempo o son configuraciones compiladas distintas

---

## 12. Historial de cambios

| Versión | Fecha | Autor | Descripción |
|---------|-------|-------|-------------|
| 0.1 | 2026-03-05 | — | Creación inicial; GPS GY-NEO6Mv2 evaluado; sensores actitud documentados |
| 0.2 | 2026-03-05 | — | Transceiver TT&C analizado; E22-400M30S recomendado; LORA32U4 II → GS |
| 0.3 | 2026-03-05 | — | HC-12 Si4463 433 MHz evaluado: ✅ GS/desarrollo, ❌ vuelo LEO (link budget −5 dB) |
