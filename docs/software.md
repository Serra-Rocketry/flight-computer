# Software Documentation - Onboard Computer

## Overview

The main firmware of the onboard computer runs on an ESP32 platform, managing sensors, communication and parachute control during flight.

## Architecture

### Main File

- **[firmware.ino](../firmware/firmware.ino)** - Main code with setup and loop

### Code Organization

```
setup()                 // Initialization of all components
├── setupLittleFS()     // File system
├── setupBMP()          // Pressure sensor
├── setupMPU()          // IMU
├── setupLoRa()         // Communication
└── setupServo()        // Servo motor

loop()                  // Continuous execution
├── readSensors()       // Data collection
├── handleParachute()   // Parachute control
├── logData()           // Storage
└── sendLoRa()          // Transmission
```

## Modules and Sensors

### 1. BMP280 - Barometric Pressure Sensor

**Function**: Measure altitude and atmospheric pressure

**Libraries**:

- `Adafruit_BMP280`
- `Wire` (I2C)

**Collected Data**:

- Relative altitude (m)
- Pressure (hPa)
- Base pressure (calibrated at setup)

**Test Code**: [test/basico/basico.ino](../test/basico/basico.ino)

**Related Variables**:

```cpp
float base_altitude    // Initial altitude (reference)
float base_pressure    // Base pressure (hPa)
float previous_altitude // Previous height (for velocity calculation)
float max_altitude     // Maximum height reached
```

### 2. MPU6050 - IMU (Accelerometer + Gyroscope)

**Function**: Measure acceleration and rotation (flight dynamics data)

**Libraries**:

- `Adafruit_MPU6050`
- `Adafruit_Sensor`
- `Wire` (I2C)

**Collected Data**:

- Acceleration in X, Y, Z (m/s²)
- Angular velocity in X, Y, Z (rad/s)
- Sensor temperature (°C)

**Related Variables**:

```cpp
sensors_event_t acc, gyr, temp  // Acceleration, gyroscope and temperature events
```

### 3. NEO-6M - GPS Module

**Function**: Get latitude, longitude, GPS altitude and time

**Libraries**:

- `TinyGPS++`
- Serial UART communication

**Communication Pins**:

- **RX_GPS**: Pin 20 (receives data from GPS)
- **TX_GPS**: Pin 21 (sends data to GPS)

**Collected Data**:

- Latitude (°)
- Longitude (°)
- GPS Altitude (m)
- Number of satellites
- Date and time UTC

**Features**:

- Waits 3 seconds in setup for synchronization
- Uses GPS time to name log file

### 4. RFM95W - LoRa Module

**Function**: Long-range wireless communication with the base

**Frequency**: 868 MHz

**Libraries**:

- `LoRa`
- `SPI`

**Communication Pins**:

- **SS_LORA**: Pin 7 (Chip Select)
- **RST_LORA**: Pin 1 (Reset)
- **DIO0_LORA**: Pin 2 (Interrupt)

**Configuration**:

```cpp
#define LORA_FREQ 868E6      // Frequency
#define SYNC_WORD 0xF3       // Sync code
```

**Transmitted Data**: Concatenated string with data from all sensors in CSV format

### 5. Servo Motor - Parachute Control

**Function**: Open parachute at appropriate altitude

**Pin**: **SERVO_PIN = 3**

**Positions**:

- **MAXPOS = 0°** (Parachute closed)
- **MINPOS = 90°** (Parachute open)

**Opening Criteria**:

```cpp
const float ALTITUDE_THRESHOLD = 750.0      // Minimum height (m)
const float ALTITUDE_DROP_THRESHOLD = 10.0  // Minimum drop from peak (m)
const float VELOCITY_THRESHOLD = 80.0        // Descent velocity minimum (m/s)
```

**Control Function**: `handleParachute()`

### 6. Buzzer - Signaling

**Function**: Indicate initialization status and operation

**Pin**: **BUZZER_PIN = 0**

**Signals**:

- **Alert**: Multiple short beeps (initialization failure)
- **Success**: Beep sequence (components started correctly)

**Control Function**: `buzzSignal()`

## Storage System

### LittleFS - File System

**Function**: Store telemetry data in real time

**File Format**: CSV

**File Name**:

- If GPS is synchronized: `HH_MM_SS-Dados.csv` (GPS time)
- If GPS not synchronized: `{millis}-Dados.csv`

**CSV Header**:

```csv
ID,Packet,Time,Latitude,Longitude,GPS_Altitude,Satellites,Date,Hours,Minutes,Seconds,
BMP_Altitude,Pressure,AccX,AccY,AccZ,GyroX,GyroY,GyroZ,Temp,ParachuteStatus
```

**File Functions**:

- `setupLittleFS()` - Initializes the system
- `writeFile()` - Creates new file with header
- `appendFile()` - Adds line to file

## Communication

### Serial UART

- **Baud Rate**: 115200
- **Use**: Debug and real-time monitoring

### LoRa

- **Range**: Up to ~4 km (in open field)
- **Data Rate**: ~1-5 kbps
- **Frequency**: 868 MHz

## Web Interface

**Functionality**: Asynchronous server on port 80

**Endpoints**:

- `GET /` - Home page (HTML)
- `GET /api/files` - List files in JSON
- `GET /api/file?name=` - Download file
- `DELETE /api/file?name=` - Delete file

**Libraries**:

- `ESPAsyncWebServer`
- `WiFi`
- `ArduinoJson`

## Adjustable Parameters

```cpp
#define INTERVAL 200                              // Reading interval (ms)
const float ALTITUDE_THRESHOLD = 750.0            // Minimum height for parachute (m)
const float ALTITUDE_DROP_THRESHOLD = 10.0        // Drop from reference (m)
const float VELOCITY_THRESHOLD = 80.0              // Descent velocity minimum (m/s)
const String TEAM_ID = "#100"                     // Team ID
```

## Execution Flow

1. **Initialization** (setup)
   - Configure serial communication
   - Initialize I2C and SPI
   - Wait for GPS synchronization (3s)
   - Create log file
   - Initialize sensors and modules
   - Start web server

2. **Main Loop**
   - Check 200ms interval
   - Read altitude and IMU
   - Calculate descent velocity
   - Check parachute opening criteria
   - Record data in file
   - Transmit via LoRa

3. **Parachute Control**
   - Wait for minimum height
   - Monitor drop relative to peak
   - Check descent velocity
   - Open servo when all criteria are met

## Support Code

The files in [extras/](../extras/) contain functional and tested code for reference:

- **[extras/ino_files/](../extras/ino_files/)** - Earlier versions of integrated code
- **[extras/FileBrowser/FileBrowser.ino](../extras/FileBrowser/FileBrowser.ino)** - File browser, base for async server
- **[extras/LoraReceiver/LoraReceiver.ino](../extras/LoraReceiver/LoraReceiver.ino)** - LoRa receiver for base
- **[extras/Serial/Serial.py](../extras/Serial/Serial.py)** - Python script for serial monitoring

## Tests

Unit tests are located in [test/](../test/):

- **[test/basico/basico.ino](../test/basico/basico.ino)** - Basic initialization test
- **[test/buzzer/buzzer.ino](../test/buzzer/buzzer.ino)** - Buzzer test
- **[test/lora/lora.ino](../test/lora/lora.ino)** - LoRa communication test
- **[test/testeGPS/testeGPS.ino](../test/testeGPS/testeGPS.ino)** - GPS module test
- **[test/servo/servo.ino](../test/servo/servo.ino)** - Servo motor test
- **[test/LittleFS/LittleFS.ino](../test/LittleFS/LittleFS.ino)** - File system test

## Dependencies - Arduino Libraries

| Library           | Version | Use                |
| ----------------- | ------- | ------------------ |
| Adafruit BMP280   | Latest  | Pressure sensor    |
| Adafruit MPU6050  | Latest  | IMU                |
| Adafruit Sensor   | Latest  | Sensor base        |
| TinyGPS++         | Latest  | GPS decoding       |
| LoRa              | Latest  | LoRa module        |
| ESP32Servo        | Latest  | Servo control      |
| ArduinoJson       | ^6.0    | JSON serialization |
| ESPAsyncWebServer | Latest  | Web server         |

## Development Notes

- **Synchronization**: The system waits for GPS synchronization before flight starts
- **Redundancy**: The parachute uses multiple criteria to prevent incorrect opening
- **Logging**: All data is stored locally before transmission via LoRa
- **Power Efficiency**: The ESP32 operates in continuous mode during flight
