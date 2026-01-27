# Hardware Documentation - Onboard Computer

## Overview

The onboard computer uses an ESP32-C3 microcontroller as its core, integrated with altitude sensors, GPS, IMU and a LoRa communication module. The PCB was designed in KiCad with compatibility for the shape and connectors of the SR21000 rocket.

## Main Platform

### ESP32-C3 SuperMini

**Specifications**:

- **Processor**: RISC-V 32-bit @ 160 MHz
- **RAM**: 400 KB (SRAM)
- **Flash**: 4 MB
- **Peripherals**: UART, SPI, I2C, GPIO, ADC, Timer
- **WiFi**: 802.11b/g/n
- **Size**: Compact 18x22 mm breakout

**Used Pins**:

```
Pin 0   → BUZZER_PIN
Pin 1   → RST_LORA (LoRa Reset)
Pin 2   → DIO0_LORA (LoRa Interrupt)
Pin 3   → SERVO_PIN (Servo motor)
Pin 7   → SS_LORA (LoRa Chip Select)
Pin 20  → RX_GPS (Serial 1 RX)
Pin 21  → TX_GPS (Serial 1 TX)
Pin 4   → SDA (I2C - BMP280, MPU6050)
Pin 5   → SCL (I2C - BMP280, MPU6050)
Pin 6   → MOSI (SPI - LoRa)
Pin 8   → MISO (SPI - LoRa)
Pin 9   → CLK (SPI - LoRa)
```

**Power Supply**: 3.3V nominal (regulated by LM2596)

## Sensors

### 1. BMP280 - Barometric Pressure Sensor

**Function**: Measurement of altitude and atmospheric pressure

**Specifications**:

- **Pressure Range**: 300 - 1100 hPa
- **Resolution**: 0.01 hPa (1 Pa)
- **Altitude Accuracy**: ±1 m
- **Interface**: I2C
- **I2C Address**: 0x77 (default)

**Pinout**:
| Pin | Function | Connection |
|-----|----------|------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| SCL | I2C Clock| GPIO 5 |
| SDA | I2C Data | GPIO 4 |

**Assembly**: GY-BMP280 breakout direct solder on PCB

### 2. MPU6050 - IMU (Accelerometer + Gyroscope)

**Function**: Measurement of acceleration and angular velocity

**Specifications**:

- **Accelerometer**:
  - Range: ±2, ±4, ±8, ±16 g (configurable)
  - Resolution: 16 bits
  - Sample rate: up to 8 kHz
- **Gyroscope**:
  - Range: ±250, ±500, ±1000, ±2000 °/s
  - Resolution: 16 bits
  - Sample rate: up to 8 kHz
- **Interface**: I2C
- **I2C Address**: 0x68 (default)

**Pinout**:
| Pin | Function | Connection |
|-----|----------|------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| SCL | I2C Clock| GPIO 5 |
| SDA | I2C Data | GPIO 4 |

**Assembly**: Breakout with direct solder on PCB

### 3. NEO-6M - GPS Module

**Function**: Geolocation and time synchronization

**Specifications**:

- **Sensitivity**: -161 dBm
- **Acquisition Time**: Cold start ~27s, Hot start ~3s
- **Accuracy**: ±2.5 m
- **Update Rate**: up to 10 Hz
- **Protocol**: NMEA 0183
- **Interface**: Serial UART

**Pinout**:
| Pin | Function | Connection |
|-----|----------|---------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| RX | Serial RX| GPIO 21 |
| TX | Serial TX| GPIO 20 |

**Features**:

- Integrated ceramic antenna
- Frequency: L1 (1575.42 MHz)
- Constellations: GPS, GLONASS, Galileo, BeiDou

**Assembly**: Breakout with direct solder on PCB

### 4. RFM95W - LoRa Module

**Function**: Long-range wireless communication with launch base

**Specifications**:

- **Frequency**: 868 MHz ISM
- **Range**: ~4 km (open field, ideal conditions)
- **Data Rate**: 1.5 - 37.5 kbps
- **Transmit Power**: +20 dBm (adjustable to +17 dBm)
- **Sensitivity**: -139 dBm
- **Interface**: SPI

**Pinout**:
| Pin | Function | Connection |
|-----|----------|---------------|
| VCC | Power | 3.3V |
| GND | Ground | GND |
| MOSI| SPI Data | GPIO 6 |
| MISO| SPI Data | GPIO 8 |
| CLK | SPI Clock| GPIO 9 |
| NSS | SPI CS | GPIO 7 |
| NRST| Reset | GPIO 1 |
| DIO0| TX/RX Int| GPIO 2 |

**Features**:

- Modulation: LoRa (Chirp Spread Spectrum)
- Bandwidth: 125, 250 or 500 kHz
- Spreading Factor: 6-12 (range vs data rate trade-off)
- Sync Code: 0xF3

**Assembly**: Breakout with direct solder on PCB

## Actuators

### Servo Motor - Parachute Control

**Function**: Open the parachute release mechanism

**Specifications**:

- **Type**: Standard 5V servo with metal gears
- **Torque**: ~4.8 kg/cm @ 5V
- **Speed**: ~0.23 s/60°
- **Accuracy**: ±3°
- **Weight**: ~9 g

**Pinout**:
| Pin | Function |
|-----|----------|
| Brown | Ground |
| Red | +5V |
| Yellow/Orange | PWM Signal (GPIO 3) |

**Assembly**: Servo oriented downward, mechanically coupled to parachute system

**Positions**:

- 0° - Hatch open (MAXPOS)
- 90° - Hatch closed (MINPOS)

### Buzzer - Audio Signaling

**Function**: Initialization status indicator

**Specifications**:

- **Type**: Active buzzer (5V)
- **Frequency**: ~2.7 kHz
- **Volume**: ~85 dB
- **Current**: ~30 mA

**Pinout**:

- Positive → 5V (via resistor)
- Negative → GPIO 0 (BUZZER_PIN)

**Signals**:

- 3 short beeps = Initialization failure
- Beep sequence = Successful initialization

## Power Supply

### LM2596 - DC-DC Step Down Converter

**Function**: Regulate battery voltage to 3.3V/5V

**Specifications**:

- **Input**: 4.5 - 40V
- **Output 1**: 3.3V @ 3A (for ESP32, I2C sensors)
- **Output 2**: 5V @ 3A (for servo, buzzer, LoRa)
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

| Label       | Component          | Quantity | Function               |
| ----------- | ------------------ | -------- | ---------------------- |
| A2          | ESP32-C3 SuperMini | 1        | Microcontroller        |
| A3          | GY-BMP280 Breakout | 1        | Pressure sensor        |
| Componente1 | MPU6050            | 1        | IMU                    |
| M3          | LM2596             | 1        | Voltage regulator      |
| M4          | NEO-6M             | 1        | GPS                    |
| RFM95W1     | RFM95W LoRa        | 1        | Wireless communication |
| J1          | 5V Buzzer          | 1        | Signaling              |
| J2          | Servo motor        | 1        | Parachute control      |

### Passive Components

- Capacitors: 100 nF, 10 μF, 220 μF
- Resistors: 1k, 10k, 100k Ohm
- Connectors: Pins, JST

## Block Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                  ONBOARD COMPUTER                            │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │ BMP280   │  │ MPU6050  │  │ NEO-6M   │  │ RFM95W   │    │
│  │Barometer │  │   IMU    │  │   GPS    │  │  LoRa    │    │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘    │
│       │             │             │             │           │
│       └─────────────┼─────────────┼─────────────┤           │
│                     │    I2C      │    UART     │    SPI    │
│                     │             │             │           │
│              ┌──────┴─────────────┴─────┬───────┴────┐       │
│              │                          │            │       │
│         ┌────▼────────────────┐    ┌────▼──┐   ┌───▼────┐   │
│         │   ESP32-C3 SuperMini│    │GPS    │   │ LoRa   │   │
│         │                    │    │ UART  │   │SPI     │   │
│         └────┬────────────────┘    └───────┘   └────────┘   │
│              │                                              │
│        ┌─────┴──────────┐                                    │
│        │ GPIO PWM       │                                    │
│        │                │                                    │
│    ┌───▼───┐    ┌──────▼──┐                                  │
│    │ Servo │    │ Buzzer  │                                  │
│    │Motor  │    │         │                                  │
│    └───────┘    └─────────┘                                  │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │          LM2596 DC-DC Converter                      │   │
│  │  Input: 7.4V (2x18650)  →  Output: 3.3V and 5V    │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

## Support/Test Code

Individual test code is located in [../test/](../test/) to validate each component:

- **[test/basico/basico.ino](../test/basico/basico.ino)** - General diagnostics
- **[test/buzzer/buzzer.ino](../test/buzzer/buzzer.ino)** - Buzzer test
- **[test/lora/lora.ino](../test/lora/lora.ino)** - LoRa test
- **[test/testeGPS/testeGPS.ino](../test/testeGPS/testeGPS.ino)** - GPS test
- **[test/servo/servo.ino](../test/servo/servo.ino)** - Servo test
- **[test/LittleFS/LittleFS.ino](../test/LittleFS/LittleFS.ino)** - Storage test

## Preliminary Tests

- Check continuity with multimeter
- Power on and check LEDs/buzzer
- Use basic test code

## Operation Notes

- **Protection**: Encapsulation in isolating capsule inside rocket
- **Shock**: Maximum acceleration tolerance not yet measured

## References

- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [BMP280 Datasheet](https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280/)
- [MPU6050 Datasheet](https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/)
- [NEO-6M GPS Module](https://www.u-blox.com/en/product/neo-6m-u-blox6)
- [RFM95W LoRa Module](https://www.semtech.com/products/wireless-rf/lora-transceivers/rfm95w)
