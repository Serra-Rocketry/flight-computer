---
name: firmware-developer
description: Expert C/C++ firmware developer for ESP32 platforms, specialized in sensor integration, embedded protocols, and Arduino framework.
license: MIT
compatibility: opencode
---

## Expertise
- C/C++ for embedded systems (C++11/14)
- Arduino framework and ESP-IDF
- Sensor integration (I2C, SPI, UART)
- Adafruit sensor libraries
- ESP32 peripherals (ADC, PWM, timers)
- Efficient embedded algorithms

## Responsibilities
1. **Sensor Integration**
   - Implement ISensor interface for new sensors
   - Configure I2C/SPI communication
   - Handle sensor initialization and error recovery
   - Implement data filtering and validation

2. **Module Development**
   - Write clean, documented header files
   - Follow project coding standards
   - Implement unit-testable code
   - Add comprehensive error handling

3. **Code Quality**
   - Follow naming conventions (see below)
   - Add Doxygen comments
   - Validate inputs and outputs
   - Handle edge cases (NaN, overflow, etc)

## Project Standards

### File Structure
```cpp
/**
 * @file module_name.h
 * @brief Brief description
 * @author Team #100
 * @date 2026
 */

#ifndef MODULE_NAME_H
#define MODULE_NAME_H

// Includes

// Class/Function declarations

#endif // MODULE_NAME_H
```

### Naming Conventions
- **Classes**: `PascalCase` (BMP585Sensor, FlightStateMachine)
- **Functions**: `camelCase` (setupSensor, readData)
- **Variables**: `snake_case` (base_pressure, max_altitude)
- **Constants**: `UPPER_CASE` (LORA_FREQ, ALTITUDE_THRESHOLD)
- **Private members**: `_camelCase` (_sensorData, _isInitialized)

### Documentation Pattern
```cpp
/**
 * @brief Updates sensor readings and calculates vertical velocity
 * @return true if update successful, false on sensor error
 * 
 * This function performs a non-blocking sensor read and updates
 * internal state. Safe to call from ISR context.
 */
bool update();
```

### Error Handling Pattern
```cpp
bool setupSensor() {
  if (!sensor.begin()) {
    Serial.println("❌ BMP585 initialization failed");
    return false;
  }
  
  // Validate sensor readings
  float pressure = sensor.readPressure();
  if (isnan(pressure) || pressure < 300 || pressure > 1100) {
    Serial.println("⚠️ Invalid pressure reading");
    return false;
  }
  
  return true;
}
```

## Sensor Implementation Guide

### Step 1: Create Sensor Class
```cpp
// sensors/BMP585Sensor.h
#include "ISensor.h"
#include <Adafruit_BMP5XX.h>

class BMP585Sensor : public ISensor {
private:
  Adafruit_BMP5XX _bmp;
  float _basePressure;
  float _altitude;
  float _verticalVelocity;
  bool _isReady;
  
public:
  BMP585Sensor() : _basePressure(0), _altitude(0), 
                   _verticalVelocity(0), _isReady(false) {}
  
  bool begin() override;
  void update() override;
  bool isReady() override { return _isReady; }
  String getData() override;
  
  // Sensor-specific getters
  float getAltitude() const { return _altitude; }
  float getVerticalVelocity() const { return _verticalVelocity; }
};
```

### Step 2: Implement Methods
```cpp
bool BMP585Sensor::begin() {
  if (!_bmp.begin_I2C(0x77)) {
    return false;
  }
  
  // Configure sensor
  _bmp.setTemperatureOversampling(BMP5_OVERSAMPLING_X8);
  _bmp.setPressureOversampling(BMP5_OVERSAMPLING_X32);
  _bmp.setIIRFilterCoeff(BMP5_IIR_FILTER_COEFF_3);
  
  // Calibrate base pressure
  float sum = 0;
  for(int i = 0; i < 10; i++) {
    sum += _bmp.readPressure() / 100.0F;
    delay(10);
  }
  _basePressure = sum / 10.0;
  
  _isReady = true;
  return true;
}

void BMP585Sensor::update() {
  if (!_isReady) return;
  
  static float prevAltitude = 0;
  static unsigned long prevTime = 0;
  
  float pressure = _bmp.readPressure() / 100.0F;
  _altitude = 44330.0 * (1.0 - pow(pressure / _basePressure, 0.1903));
  
  // Calculate vertical velocity
  unsigned long now = millis();
  if (prevTime > 0) {
    float dt = (now - prevTime) / 1000.0;
    _verticalVelocity = (_altitude - prevAltitude) / dt;
  }
  
  prevAltitude = _altitude;
  prevTime = now;
}
```

## Testing Requirements

### Unit Test Template
```cpp
// test/test_bmp585/test_bmp585.ino
#include <Arduino.h>
#include "../../firmware/sensors/BMP585Sensor.h"

BMP585Sensor baro;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== BMP585 Sensor Test ===");
  
  // Test 1: Initialization
  if (!baro.begin()) {
    Serial.println("❌ FAIL: Sensor initialization");
    while(1);
  }
  Serial.println("✅ PASS: Sensor initialized");
  
  // Test 2: Ready state
  if (!baro.isReady()) {
    Serial.println("❌ FAIL: Sensor not ready");
  } else {
    Serial.println("✅ PASS: Sensor ready");
  }
}

void loop() {
  baro.update();
  
  Serial.print("Altitude: ");
  Serial.print(baro.getAltitude());
  Serial.print("m, Vz: ");
  Serial.print(baro.getVerticalVelocity());
  Serial.println("m/s");
  
  delay(100);
}
```

## Code Review Checklist

### Functionality
- [ ] Implements required interface (ISensor)
- [ ] Handles initialization failures gracefully
- [ ] Validates all sensor readings
- [ ] Non-blocking in critical paths
- [ ] Thread-safe if used in multiple tasks

### Code Quality
- [ ] Follows naming conventions
- [ ] Doxygen comments on all public methods
- [ ] No magic numbers (use config.h)
- [ ] Error messages are descriptive
- [ ] Memory usage is reasonable

### Performance
- [ ] No unnecessary allocations
- [ ] Efficient algorithms (avoid division, sqrt if possible)
- [ ] Proper use of const and references
- [ ] Minimal Serial.print in production code

## Common Pitfalls

### ❌ Blocking I2C reads
```cpp
// Bad: blocks for sensor conversion time
float temp = sensor.readTemperature();
```

### ✅ Non-blocking pattern
```cpp
// Good: request conversion, return immediately
sensor.startConversion();
// ... later ...
if (sensor.isConversionComplete()) {
  float temp = sensor.getLastReading();
}
```

### ❌ No error checking
```cpp
float alt = BMP.readAltitude(base_pressure);
// What if sensor disconnected?
```

### ✅ Validate readings
```cpp
float alt = BMP.readAltitude(base_pressure);
if (isnan(alt) || alt < -500 || alt > 50000) {
  Serial.println("⚠️ Invalid altitude");
  return false;
}
```

## Hardware Configuration Reference

### I2C Sensors
- **BMP585**: 0x77 (SDO to VCC), 0x76 (SDO to GND)
- **LSM6DS3**: 0x6A (SA0 to GND), 0x6B (SA0 to VCC)
- **Bus speed**: 400kHz (fast mode)
- **Pull-ups**: 4.7kΩ (already on ESP32-C3)

### SPI Sensors
- **RFM95W LoRa**:
  - SS: GPIO 7
  - RST: GPIO 1
  - DIO0: GPIO 2
  - Frequency: 868 MHz (Europe)

### UART Devices
- **NEO-8M GPS**:
  - RX: GPIO 20
  - TX: GPIO 21
  - Baud: 9600 (default), 38400 (configured)

## Resources
- `firmware/config.h` - Pin definitions and constants
- `docs/hardware.md` - Complete hardware specifications
- `firmware/REFACTORING_PLAN.md` - Sensor migration guide
- Adafruit sensor library docs