# Hardware Documentation - Onboard Computer

## Overview

The onboard computer uses an **ESP32-S3** microcontroller (the v2.0 target
platform) as its core, integrated with a barometric altimeter, GPS, IMU and a
LoRa communication module.

> **Note — prototype vs target**: the early dev firmware ran on an
> ESP32-C3 SuperMini; the v2.0 design (schematic `hardware/electronics/...`)
> uses the ESP32-S3. The pin assignments in `firmware/config.h` now target the
> S3. Re-map if migrating between boards.

## Main Platform

### ESP32-S3 (v2.0 target)

**Specifications**:

- **Processor**: Xtensa LX7 dual-core @ 240 MHz
- **RAM**: 512 KB (SRAM) + PSRAM (see module variant)
- **Flash**: 8 MB (N8R8) or 2 MB (N8R2) — confirm variant on BOM/purchase order
- **Peripherals**: UART, SPI, I2C, GPIO, ADC, Timer, USB-OTG
- **Wireless**: WiFi 802.11b/g/n + Bluetooth 5 (LE)
- **Size**: ESP32-S3-DevKitC-1 form factor

**Used Pins** (from `firmware/config.h`):

```
GPIO 4  → RST_LORA   (LoRa Reset)
GPIO 5  → DIO0_LORA  (LoRa Interrupt / TX-RX done)
GPIO 6  → BUZZER_PIN
GPIO 7  → SERVO_PIN  (Parachute servo PWM)
GPIO 8  → I2C_SDA    (BMP585/BMP280 + LSM6DS3)
GPIO 9  → I2C_SCL    (BMP585/BMP280 + LSM6DS3)
GPIO 10 → SS_LORA    (LoRa Chip Select)
GPIO 11 → LORA_MOSI  (LoRa SPI MOSI)
GPIO 12 → LORA_SCK   (LoRa SPI Clock)
GPIO 13 → LORA_MISO  (LoRa SPI MISO)
GPIO 14 → SD_CS_PIN  (SD Card Chip Select)
GPIO 17 → RX_GPS     (UART RX — GPS TX)  ⚠ bench-measured 2026-08-27, see note
GPIO 18 → TX_GPS     (UART TX — GPS RX)  ⚠ bench-measured 2026-08-27, see note
```

> **GPS pinout note (2026-08-27 bench measurement):** the GPS TX line
> physically arrives on GPIO17, not GPIO18 as previously documented from the
> schematic. `config.h` was corrected (RX_GPS=17, TX_GPS=18) and validated on
> hardware: NMEA at 9600 baud, 11 satellites, 3D fix. Always trust the raw
> sniffer (`test/gps_diag/`) over the schematic comment when they disagree.

**Power Supply**: 3.3V nominal (regulated by LM2596).

## Sensors

### 1. BMP585 - Barometric Pressure Sensor

**Function**: Altitude and atmospheric pressure (vertical velocity derived in firmware).

**Specifications**:

- **Pressure Range**: 300 - 1250 hPa
- **Relative Accuracy**: ±0.06 hPa (typ.)
- **Absolute Accuracy**: ±0.5 hPa (typ.)
- **Interface**: I2C
- **I2C Address**: 0x7E (bench-measured 2026-08-27; factory default is 0x76/0x77
  selected by CSB). **Fallback**: if the BMP585 is absent, the driver
  (`BMP585Sensor`) falls back to a **BMP280** at 0x76/0x77 — same satellite
  pattern (`BME280Sensor.cpp` in the #213 repo).

**Pinout**:

| Pin | Function | Connection |
|-----|----------|------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| SCL | I2C Clock | GPIO 9 |
| SDA | I2C Data | GPIO 8 |

**Assembly**: Breakout direct solder on PCB.

### 2. LSM6DS3 - IMU (Accelerometer + Gyroscope)

**Function**: Acceleration and angular velocity (flight dynamics).

**Specifications**:

- **Accelerometer**: ±2, ±4, ±8, ±16 g (configurable)
- **Gyroscope**: ±125, ±250, ±500, ±1000, ±2000 °/s
- **Interface**: I2C
- **I2C Address**: 0x6B (bench-measured 2026-08-27; SDO strap selects 0x6A/0x6B)

**Pinout**:

| Pin | Function | Connection |
|-----|----------|------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| SCL | I2C Clock | GPIO 9 |
| SDA | I2C Data | GPIO 8 |

**Assembly**: Breakout direct solder on PCB.

### 3. NEO-8M - GPS Module

**Function**: Geolocation and time synchronization.

**Specifications**:

- **Sensitivity**: -167 dBm (tracking)
- **Acquisition**: Cold ~26s, Hot ~1s
- **Accuracy**: ±2.5 m
- **Update Rate**: up to 10 Hz
- **Protocol**: NMEA 0183
- **Interface**: Serial UART (HardwareSerial, non-blocking in firmware)

**Pinout**:

| Pin | Function | Connection |
|-----|----------|---------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| RX | Serial RX | GPIO 21 (TX_GPS) |
| TX | Serial TX | GPIO 20 (RX_GPS) |

**Features**:

- Integrated ceramic antenna
- Frequency: L1 (1575.42 MHz)
- Constellations: GPS, GLONASS, Galileo, BeiDou

**Assembly**: Breakout direct solder on PCB.

### 4. RFM95W - LoRa Module

**Function**: Long-range wireless telemetry to the recovery receiver.

**Specifications**:

- **Frequency**: 915 MHz ISM (Americas/Brazil; matches receiver-lora)
- **Range**: ~4 km (open field, ideal conditions)
- **Data Rate**: 0.3 - 37.5 kbps (SF/BW dependent)
- **Transmit Power**: +17 dBm (firmware setting)
- **Sensitivity**: -139 dBm (SF12, BW 125 kHz)
- **Interface**: SPI

**Configuration (firmware `config.h` + `lora_module.h`)**:

- Frequency: 915 MHz
- Sync Word: 0xF3
- Spreading Factor: 7
- Bandwidth: 125 kHz
- Coding Rate: 4/5
- CRC: enabled

**Pinout** (matches `config.h` — SPI bus is remapped via `SPI.begin(4,2,3,5)` in `setupLoRa()`):

| Pin | Function | Connection |
|-----|----------|---------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| SCK | SPI Clock | GPIO 4 (LORA_SCK) |
| MOSI| SPI Data | GPIO 3 (LORA_MOSI) |
| MISO| SPI Data | GPIO 2 (LORA_MISO) |
| NSS | SPI CS | GPIO 5 (SS_LORA) |
| NRST| Reset | GPIO 6 (RST_LORA) |
| DIO0| TX/RX Int| GPIO 7 (DIO0_LORA) |

**Assembly**: Breakout direct solder on PCB.

### 5. MicroSD Card — Data Logging

**Function**: Primary flight-data storage. The firmware writes the 22-field
CSV telemetry log here; if the card is absent or fails to mount, logging
automatically falls back to LittleFS (internal flash).

**Interface**: SPI (shares the same SCK/MOSI/MISO bus as the RFM95W, with an
independent chip-select line).

**Pinout**:

| Pin | Function | Connection |
|-----|----------|---------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| SCK | SPI Clock | GPIO 4 (shared with LoRa) |
| MOSI| SPI Data | GPIO 3 (shared with LoRa) |
| MISO| SPI Data | GPIO 2 (shared with LoRa) |
| CS  | SPI CS | GPIO 12 (SD_CS_PIN) |

**Note**: Because SD and LoRa share one SPI bus, `setupStorage()` (SD probe)
runs *before* `setupLoRa()` inside `initTelemetryTask()`, and both backends
use the same `SPI` object remapped to GPIO 4/3/2. A card present at boot is
preferred; LittleFS is the safety net.

## Actuators

### Parachute Servo

**Function**: Release the parachute mechanism at apogee (FSM Option A).

**Specifications**:

- **Type**: Standard 5V servo with metal gears
- **Torque**: ~4.8 kg/cm @ 5V
- **Speed**: ~0.23 s/60°
- **Weight**: ~9 g

**Pinout**:

| Wire | Function |
|------|----------|
| Brown | Ground |
| Red | +5V |
| Yellow/Orange | PWM Signal (GPIO 10, SERVO_PIN) |

**Positions** (see `parachute_module.h`):

- Closed (pre-flight): `CLOSED_POS`
- Open (deployed): `OPEN_POS`

### Buzzer - Audio Signaling

**Function**: Initialization status indicator.

**Specifications**:

- **Type**: Active buzzer (5V)
- **Frequency**: ~2.7 kHz
- **Volume**: ~85 dB
- **Current**: ~30 mA

**Pinout**:

- Positive → 5V (via current-limiting resistor)
- Negative → GPIO 11 (BUZZER_PIN)

**Signals**:

- 3 short beeps = Initialization failure
- Beep sequence = Successful initialization

## Power Supply

### LM2596 - DC-DC Step Down Converter

**Function**: Regulate battery voltage to 3.3V / 5V.

**Specifications**:

- **Input**: 4.5 - 40V
- **Output 1**: 3.3V @ 3A (ESP32, I2C sensors)
- **Output 2**: 5V @ 3A (servo, buzzer, LoRa)
- **Frequency**: 150 kHz
- **Efficiency**: ~85%

### Batteries

**Configuration**: 3x 18650 in parallel

- **Nominal Voltage**: 3.7V
- **Capacity**: ~6000 mAh
- **Estimated Autonomy**: ~4-6 hours

## Printed Circuit Board (PCB)

### KiCad Files

- **[CDB.kicad_pro](../hardware/CDB/CDB.kicad_pro)** - Project file
- **[CDB.kicad_sch](../hardware/CDB/CDB.kicad_sch)** - Schematic
- **[CDB.kicad_pcb](../hardware/CDB/CDB.kicad_pcb)** - Board layout
- **[CDB.step](../hardware/CDB/CDB.step)** - 3D model

### Connectors

- **J2**: Servo motor (3 pins)
- **Power Connector**: 2 pins for 18650

### Approximate Dimensions

- **Length**: 125 mm
- **Width**: 50 mm
- **Height** (with components): ~30 mm

## Component List (BOM)

See [CDB_bom.md](../hardware/CDB_bom.md) for the complete list.

### Main Electronic Components

| Label | Component | Quantity | Function |
|-------|-----------|----------|----------|
| A2 | ESP32-S3 (DevKitC-1) | 1 | Microcontroller (v2.0 target; N8R8 or N8R2 — confirm) |
| A3 | BMP585 Breakout | 1 | Pressure/altitude sensor |
| — | LSM6DS3 Breakout | 1 | IMU |
| M3 | LM2596 | 1 | Voltage regulator |
| M4 | NEO-8M | 1 | GPS |
| RFM95W1 | RFM95W LoRa | 1 | Wireless telemetry |
| SD1 | MicroSD card module | 1 | Flight-data logging (SPI, GPIO 12 CS) |
| J1 | 5V Buzzer | 1 | Signaling |
| J2 | Servo motor | 1 | Parachute control |

### Passive Components

- Capacitors: 100 nF, 10 μF, 220 μF
- Resistors: 1k, 10k, 100k Ohm
- Connectors: Pins, JST

## Block Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                  ONBOARD COMPUTER                            │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │ BMP585   │  │ LSM6DS3  │  │ NEO-8M   │  │ RFM95W   │    │
│  │Barometer │  │   IMU    │  │   GPS    │  │  LoRa    │    │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘    │
│       │ I2C         │ I2C         │ UART         │ SPI     │
│       └─────────────┼─────────────┼──────────────┤        │
│              ┌──────┴─────────────┴──────┬────────┴────┐   │
│              │      ESP32-S3 (DevKitC-1)   │             │   │
│              │  FreeRTOS: FlightControl  │  Telemetry  │   │
│              │  (50Hz) → FSM → Servo     │  (5Hz) LoRa │   │
│              └────┬──────────┬───────────┴─────┬───────┘   │
│        GPIO PWM  │          │  GPIO BUZZER     │ SPI       │
│    ┌─────────────▼┐    ┌────▼──────┐    ┌──────▼────┐      │
│    │ Servo Motor  │    │ Buzzer    │    │ RFM95W    │      │
│    └──────────────┘    └───────────┘    └───────────┘      │
│  ┌──────────────────────────────────────────────────────┐  │
│  │          LM2596 DC-DC Converter                      │  │
│  │  Input: 3.7V (3x18650 parallel)  →  Output: 3.3V and 5V  │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Support/Test Code

Individual test code is in [../test/](../test/):

- `test/basico/basico.ino` — general diagnostics
- `test/buzzer/buzzer.ino` — buzzer
- `test/lora/lora.ino` — LoRa
- `test/testeGPS/testeGPS.ino` — GPS
- `test/servo/servo.ino` — servo
- `test/LittleFS/LittleFS.ino` — storage
- `test/FSM/FSM.ino` — FSM reference (validated)

## Preliminary Tests

- Check continuity with multimeter
- Power on and check LEDs/buzzer
- Use basic test code

## Operation Notes

- **Encapsulation**: isolating capsule inside rocket
- **Shock**: Maximum acceleration tolerance not yet measured

## References

- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [BMP585 Datasheet](https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp585/)
- [LSM6DS3 Datasheet](https://www.st.com/resource/en/datasheet/lsm6ds3.pdf)
- [NEO-8M GPS Module](https://www.u-blox.com/en/product/neo-8m-series)
- [RFM95W LoRa Module](https://www.semtech.com/products/wireless-rf/lora-transceivers/rfm95w)
