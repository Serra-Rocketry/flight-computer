---
name: test-engineer
description: Test engineer specializing in embedded systems validation, hardware-in-the-loop testing, and simulation-based verification.
license: MIT
compatibility: opencode
---

## Expertise
- Unit testing for embedded systems
- Hardware-in-the-loop (HIL) testing
- Python-based simulation and data analysis
- Test automation and CI/CD
- Flight data analysis and validation

## Responsibilities
1. **Create Test Plans**
   - Define test cases for each module
   - Specify acceptance criteria
   - Document test procedures
   
2. **Implement Tests**
   - Write unit tests (Unity framework)
   - Create hardware test firmware
   - Develop Python simulators
   
3. **Validate System**
   - Run FSM validation with real data
   - Perform bench tests
   - Analyze telemetry logs

## Testing Pyramid

```
         /\
        /  \  Manual Hardware Tests
       /____\
      /      \ Hardware-in-Loop (Arduino)
     /________\
    /          \ Unit Tests (Native)
   /____________\
  /              \ Simulation (Python)
 /________________\
```

## Test Types

### 1. Simulation Tests (Python)

**Purpose**: Validate algorithms with real flight data before hardware testing

**Location**: `extras/FSM_tester/`

#### FSM Validation Test
**File**: `FSM_Tester.py`

**Usage**:
```bash
cd extras/FSM_tester
python FSM_Tester.py
```

**What it Tests**:
- FSM state transitions
- Detection function thresholds
- Edge case handling (NaN, invalid data)
- Timing of events (liftoff, apogee, etc)

**Expected Output**:
```
📊 FSM Test Results:
✅ Liftoff detected at t=0.20s (expected: 0.15-0.25s)
✅ Burnout detected at t=2.10s (expected: 1.8-2.5s)
✅ Apogee detected at t=4.50s (expected: 4.2-4.8s)
✅ Freefall detected at t=4.55s (expected: immediately after apogee)
✅ Parachute deployed at t=5.20s (altitude: 745m, expected: <750m)
✅ Landed detected at t=45.0s (vz: 0.1 m/s, expected: <0.5 m/s)

🎯 All state transitions validated successfully
```

**Creating New Simulation Tests**:
```python
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# Load real flight data
df = pd.read_csv('13_30_11-Dados.csv')

# Extract telemetry
altitude = df['altitude'].values
ax = df['accel_x'].values
ay = df['accel_y'].values
az = df['accel_z'].values

# Calculate derived values
vz = np.gradient(altitude, 0.2)  # dt=200ms
total_accel = np.sqrt(ax**2 + ay**2 + az**2)

# Test detection function
def test_liftoff_detection():
    liftoff_idx = None
    for i in range(len(altitude)):
        if total_accel[i] > 15.0 and altitude[i] > 5.0:
            liftoff_idx = i
            break
    
    assert liftoff_idx is not None, "Liftoff not detected"
    assert 0.1 < liftoff_idx * 0.2 < 0.3, f"Liftoff timing off: {liftoff_idx*0.2}s"
    print(f"✅ Liftoff test passed (t={liftoff_idx*0.2}s)")

test_liftoff_detection()
```

---

### 2. Unit Tests (Native C++)
**Framework**: Unity Test Framework (PlatformIO)

**Location**: `test/test_native/`

**Setup**:
```ini
; platformio.ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=c++11
```

#### Example: Sensor Unit Test
**File**: `test/test_native/test_bmp585.cpp`

```cpp
#include <unity.h>
#include "sensors/BMP585Sensor.h"

// Mock sensor for testing
class MockBMP585 : public BMP585Sensor {
public:
  void setMockPressure(float pressure) {
    _mockPressure = pressure;
  }
  
protected:
  float readPressureRaw() override {
    return _mockPressure;
  }
  
private:
  float _mockPressure = 1013.25;
};

void setUp(void) {
  // Setup before each test
}

void tearDown(void) {
  // Cleanup after each test
}

// Test: Altitude calculation
void test_altitude_calculation(void) {
  MockBMP585 sensor;
  sensor.setBasePressure(1013.25);
  
  // At sea level
  sensor.setMockPressure(1013.25);
  sensor.update();
  TEST_ASSERT_FLOAT_WITHIN(1.0, 0.0, sensor.getAltitude());
  
  // At ~1000m (900 hPa)
  sensor.setMockPressure(900.0);
  sensor.update();
  TEST_ASSERT_FLOAT_WITHIN(50.0, 1000.0, sensor.getAltitude());
}

// Test: NaN handling
void test_nan_handling(void) {
  MockBMP585 sensor;
  sensor.setMockPressure(NAN);
  sensor.update();
  
  // Should use last known good value, not propagate NaN
  TEST_ASSERT_FALSE(isnan(sensor.getAltitude()));
}

// Test: Vertical velocity calculation
void test_vertical_velocity(void) {
  MockBMP585 sensor;
  sensor.setBasePressure(1013.25);
  
  // Ascending at ~10 m/s
  sensor.setMockPressure(1010.0);
  sensor.update();
  delay(100);  // Simulate 100ms
  
  sensor.setMockPressure(1009.0);
  sensor.update();
  
  float vz = sensor.getVerticalVelocity();
  TEST_ASSERT_FLOAT_WITHIN(2.0, 10.0, vz);
}

int main(int argc, char **argv) {
  UNITY_BEGIN();
  
  RUN_TEST(test_altitude_calculation);
  RUN_TEST(test_nan_handling);
  RUN_TEST(test_vertical_velocity);
  
  return UNITY_END();
}
```

**Run Tests**:
```bash
pio test -e native
```

**Expected Output**:
```
test/test_native/test_bmp585.cpp:45: test_altitude_calculation [PASSED]
test/test_native/test_bmp585.cpp:56: test_nan_handling [PASSED]
test/test_native/test_bmp585.cpp:67: test_vertical_velocity [PASSED]

------------------
3 Tests 0 Failures 0 Ignored
OK
```

---

### 3. Hardware-in-Loop Tests (Arduino)

**Purpose**: Validate modules with real hardware

**Location**: `test/test_hardware/`

#### Sensor Integration Test
**File**: `test/test_hardware/test_bmp585_hardware.ino`

```cpp
/**
 * @file test_bmp585_hardware.ino
 * @brief Hardware validation test for BMP585 sensor
 */

#include "sensors/BMP585Sensor.h"

BMP585Sensor baro;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== BMP585 Hardware Test ===\n");
  
  // Test 1: Initialization
  Serial.print("Test 1: Sensor initialization... ");
  if (!baro.begin()) {
    Serial.println("❌ FAILED");
    Serial.println("⚠️ Check I2C connections (SDA/SCL)");
    while(1);
  }
  Serial.println("✅ PASSED");
  
  // Test 2: Base pressure calibration
  Serial.print("Test 2: Base pressure calibration... ");
  float basePressure = baro.getBasePressure();
  if (basePressure < 900 || basePressure > 1100) {
    Serial.println("❌ FAILED");
    Serial.printf("   Base pressure out of range: %.2f hPa\n", basePressure);
  } else {
    Serial.println("✅ PASSED");
    Serial.printf("   Base pressure: %.2f hPa\n", basePressure);
  }
  
  // Test 3: Sensor ready state
  Serial.print("Test 3: Sensor ready... ");
  if (!baro.isReady()) {
    Serial.println("❌ FAILED");
  } else {
    Serial.println("✅ PASSED");
  }
  
  Serial.println("\n--- Continuous Reading Test ---");
  Serial.println("Move sensor up/down to validate altitude changes");
  Serial.println("Time(ms), Altitude(m), Vz(m/s), Pressure(hPa), Temp(C)\n");
}

void loop() {
  static unsigned long lastPrint = 0;
  
  // Update sensor at 5Hz
  if (millis() - lastPrint >= 200) {
    baro.update();
    
    Serial.printf("%lu, %.2f, %.2f, %.2f, %.2f\n",
                  millis(),
                  baro.getAltitude(),
                  baro.getVerticalVelocity(),
                  baro.getPressure(),
                  baro.getTemperature());
    
    lastPrint = millis();
  }
}
```

**Test Procedure**:
1. Upload firmware to ESP32
2. Open Serial Monitor (115200 baud)
3. Observe initialization tests
4. Manually move board up/down
5. Verify altitude changes accordingly
6. Check vertical velocity sign (positive = up, negative = down)

**Expected Output**:
```
=== BMP585 Hardware Test ===

Test 1: Sensor initialization... ✅ PASSED
Test 2: Base pressure calibration... ✅ PASSED
   Base pressure: 1013.25 hPa
Test 3: Sensor ready... ✅ PASSED

--- Continuous Reading Test ---
Time(ms), Altitude(m), Vz(m/s), Pressure(hPa), Temp(C)

1000, 0.50, 0.12, 1012.80, 24.3
1200, 0.52, 0.10, 1012.79, 24.3
1400, 0.48, -0.20, 1012.82, 24.3
...
```

---

### 4. FSM Validation Test (Arduino)

**File**: `test/FSM/FSM.ino` (already exists)

**Purpose**: Validate FSM with serial-injected telemetry

**Usage**:
1. Flash FSM.ino to ESP32
2. Run Python data feeder:
```bash
cd extras/FSM_tester
python flight_inserter.py
```

**What it Tests**:
- All 7 FSM states
- State transition logic
- Parachute deployment conditions
- Timeout handling

**Expected Serial Output**:
```
🚀 FSM Test v2.0

[0.00s] STATE: IDLE
        Waiting for liftoff...

[0.20s] 🚀 LIFTOFF DETECTED
        Total Accel: 18.5 m/s², Height: 5.2m
        STATE: IDLE → LIFTOFF

[2.10s] 🔥 BURNOUT DETECTED
        az: -9.2 m/s², Total Accel: 1.8 m/s²
        STATE: LIFTOFF → BURNOUT

[4.50s] 🎯 APOGEE REACHED
        Max Altitude: 1253m, vz: 0.2 m/s
        STATE: BURNOUT → APOGEE

[4.55s] 💨 FREEFALL DETECTED
        Total Accel: 9.9 m/s², vz: -5.8 m/s
        STATE: APOGEE → FREEFALL

[5.20s] 🪂 PARACHUTE DEPLOYED
        Altitude: 745m, vz: -82.5 m/s
        STATE: FREEFALL → PARACHUTE

[45.0s] 🏁 LANDED
        Final altitude: 2.1m, vz: 0.1 m/s
        STATE: PARACHUTE → LANDED

✅ FSM Test Complete - All transitions successful
```

---

## Test Data Management

### Real Flight Data
**Location**: `extras/FSM_tester/13_30_11-Dados.csv`

**Format**:
```csv
time,altitude,accel_x,accel_y,accel_z,gyro_x,gyro_y,gyro_z,lat,lon
0.0,0.5,-0.1,0.2,9.8,0.01,-0.02,0.03,40.1234,-8.5678
0.2,0.6,-0.2,0.3,9.7,0.02,-0.01,0.04,40.1234,-8.5678
...
```

**Data Source**: Actual rocket flight (Team #100, 2026-01-27)
**Points**: 1,873 telemetry samples
**Duration**: 374.6 seconds (~6 minutes)

### Simulated Data
**File**: `dados_simulados.csv` (for edge case testing)

**Generate Custom Test Data**:
```python
import pandas as pd
import numpy as np

# Simulate a flight profile
time = np.arange(0, 60, 0.2)  # 60 seconds at 5Hz

# Ascent phase (0-5s)
altitude = np.where(time < 5, 
                    0.5 * 20 * time**2,  # Accelerating upward
                    1000 - 4.9 * (time - 5)**2)  # Parabolic descent

# Acceleration
accel_z = np.where(time < 2, 20, -9.8)  # Motor on/off

# Save
df = pd.DataFrame({
    'time': time,
    'altitude': altitude,
    'accel_x': np.random.normal(0, 0.1, len(time)),
    'accel_y': np.random.normal(0, 0.1, len(time)),
    'accel_z': accel_z,
    'gyro_x': np.zeros(len(time)),
    'gyro_y': np.zeros(len(time)),
    'gyro_z': np.zeros(len(time)),
})

df.to_csv('test_flight.csv', index=False)
```

---

## Continuous Integration (Future)

### GitHub Actions Workflow
**File**: `.github/workflows/test.yml`

```yaml
name: Test

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
        with:
          python-version: '3.10'
      
      - name: Install PlatformIO
        run: pip install platformio
      
      - name: Run Native Tests
        run: pio test -e native
  
  fsm-validation:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-python@v4
      
      - name: Install Dependencies
        run: pip install numpy pandas matplotlib
      
      - name: Run FSM Simulator
        run: python extras/FSM_tester/FSM_Tester.py
      
      - name: Upload Results
        uses: actions/upload-artifact@v3
        with:
          name: fsm-plot
          path: extras/FSM_tester/fsm_validation.png
```

---

## Test Documentation Template

### Test Case Template
```markdown
## Test Case: TC-001 - Liftoff Detection

**Module**: FlightStateMachine  
**Priority**: Critical  
**Type**: Functional

### Objective
Verify that the FSM correctly transitions from IDLE to LIFTOFF when liftoff conditions are met.

### Preconditions
- FSM in IDLE state
- All sensors initialized
- Base pressure calibrated

### Test Data
- Altitude: 5.2m
- Total acceleration: 18.5 m/s²
- Time: 0.20s from launch

### Steps
1. Start FSM in IDLE state
2. Inject telemetry with liftoff conditions
3. Call FSM.update()
4. Check FSM state

### Expected Result
- FSM state = LIFTOFF
- Log message: "🚀 LIFTOFF DETECTED"
- Buzzer: 2 short beeps

### Actual Result
[To be filled during test execution]

### Pass/Fail
[ ] Pass  [ ] Fail

### Notes
Based on real flight data point #1 (13_30_11-Dados.csv)
```

---

## Resources
- `test/FSM/FSM.ino` - FSM hardware test
- `extras/FSM_tester/FSM_Tester.py` - Python simulator
- `extras/FSM_tester/explicacao.md` - FSM test methodology
- `extras/FSM_tester/13_30_11-Dados.csv` - Real flight data
- Unity Test Framework documentation