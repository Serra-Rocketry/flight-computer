---
name: fsm-specialist
description: Expert in flight dynamics, state machine design, and validation using real-world telemetry data.
license: MIT
compatibility: opencode
---

## Expertise
- Finite State Machine (FSM) design and validation
- Flight phase detection algorithms
- Sensor data fusion and filtering
- Threshold tuning using real telemetry
- Safety-critical state transitions

## Responsibilities
1. **FSM Design & Validation**
   - Define states, transitions, and guards
   - Validate thresholds with real flight data
   - Ensure deterministic behavior
   - Document state machine logic

2. **Flight Phase Detection**
   - Implement detection functions for each phase
   - Handle edge cases and anomalies
   - Add safety margins to thresholds
   - Validate with Python simulator

3. **Safety Assurance**
   - Prevent false positives (premature parachute deploy)
   - Detect stuck states and add timeouts
   - Implement state guards (minimum altitude, etc)
   - Add telemetry for debugging transitions

## Flight State Machine (7 States - VALIDATED)

### State Diagram
```
IDLE (0)
  │
  ├─→ LIFTOFF (1)      [totalAccel > 15 m/s² AND height > 5m]
  │     │
  │     ├─→ BURNOUT (2) [az < -8 m/s² OR totalAccel < 2 m/s²]
  │     │     │
  │     │     ├─→ APOGEE (3) [|vz| < 1 m/s AND az < -0.1 m/s²]
  │     │     │     │
  │     │     │     ├─→ FREEFALL (4) [totalAccel < 11.5 m/s² AND vz < -5 m/s]
  │     │     │     │     │
  │     │     │     │     ├─→ PARACHUTE (5) [conditions met]
  │     │     │     │     │     │
  │     │     │     │     │     └─→ LANDED (6) [vz ≈ 0 AND height < 10m]
```

### States & Actions

#### 0. IDLE
**Description**: Rocket on launchpad, sensors initialized  
**Entry Actions**:
- Initialize all sensors
- Calibrate base pressure
- Reset max_altitude = 0
- Servo closed (MINPOS = 90°)
- Start logging

**Exit Condition**: Liftoff detected

---

#### 1. LIFTOFF
**Description**: Motor ignition, rapid acceleration  
**Detection**:
```cpp
bool detectLiftoff(float ax, float ay, float az, float height) {
  float totalAccel = sqrt(ax*ax + ay*ay + az*az);
  return (totalAccel > 15.0) && (height > 5.0);
}
```

**Thresholds** (validated with 1,873 data points):
- `totalAccel > 15 m/s²` - Detects motor thrust
- `height > 5 m` - Prevents ground vibration false triggers

**Entry Actions**:
- Log "LIFTOFF DETECTED" event
- Buzz pattern: 2 short beeps

**Exit Condition**: Burnout detected

---

#### 2. BURNOUT
**Description**: Motor exhausted, coasting phase  
**Detection**:
```cpp
bool detectBurnout(float ax, float ay, float az, float height, float vz) {
  float totalAccel = sqrt(ax*ax + ay*ay + az*az);
  return (az < -8.0) || (totalAccel < 2.0);
}
```

**Thresholds**:
- `az < -8 m/s²` - Sudden deceleration when motor stops
- `totalAccel < 2 m/s²` - Coasting with minimal acceleration

**Entry Actions**:
- Log "BURNOUT DETECTED"
- Track max_altitude continuously

**Exit Condition**: Apogee detected

---

#### 3. APOGEE
**Description**: Highest point, velocity ≈ 0  
**Detection**:
```cpp
bool detectApogee(float vz, float az) {
  return (abs(vz) < 1.0) && (az < -0.1);
}
```

**Thresholds**:
- `|vz| < 1 m/s` - Vertical velocity near zero
- `az < -0.1 m/s²` - Confirming downward acceleration

**Entry Actions**:
- Log "APOGEE REACHED" with max_altitude
- Start freefall timer
- Buzz pattern: 3 short beeps

**Exit Condition**: Freefall detected

---

#### 4. FREEFALL
**Description**: Falling without parachute, accelerating downward  
**Detection**:
```cpp
bool detectFreefall(float height, float vz, float ax, float ay, float az) {
  float totalAccel = sqrt(ax*ax + ay*ay + az*az);
  return (totalAccel < 11.5) && (vz < -5.0);
}
```

**Thresholds**:
- `totalAccel < 11.5 m/s²` - Close to freefall (9.8 + air resistance)
- `vz < -5 m/s` - Falling at significant rate

**Entry Actions**:
- Log "FREEFALL DETECTED"
- Check parachute deployment conditions (see below)

**Parachute Deployment Logic**:
```cpp
bool shouldDeployParachute() {
  return (height < ALTITUDE_THRESHOLD) ||        // Below 750m AGL
         (abs(vz) > VELOCITY_THRESHOLD) ||       // Falling faster than 80 m/s
         ((max_altitude - height) > ALTITUDE_DROP_THRESHOLD); // Dropped 10m from apogee
}
```

**Exit Condition**: Parachute deployed OR conditions met

---

#### 5. PARACHUTE
**Description**: Parachute deployed, slow descent  
**Entry Actions**:
- **DEPLOY PARACHUTE**: `servo.write(MAXPOS)` (0°)
- Log "PARACHUTE DEPLOYED" with altitude and velocity
- Buzz pattern: Long continuous beep
- Reduce logging rate (conserve battery)

**Monitoring**:
- Track descent rate (should be 5-10 m/s)
- Monitor altitude to ground

**Exit Condition**: Landed detected

---

#### 6. LANDED
**Description**: Rocket on ground, mission complete  
**Detection**:
```cpp
bool detectLanded(float vz, float height) {
  return (abs(vz) < 0.5) && (height < 10.0);
}
```

**Thresholds**:
- `|vz| < 0.5 m/s` - Velocity near zero
- `height < 10 m` - Near ground level

**Entry Actions**:
- Log "LANDED" with final position
- Close data file
- Buzz pattern: Intermittent beeps (beacon mode)
- Start WiFi AP for data recovery

**Continuous Actions**:
- Keep WiFi AP active
- Beacon buzzer every 10 seconds
- GPS logging for recovery

---

## Validation Methodology

### 1. Python Simulator
**File**: `extras/FSM_tester/FSM_Tester.py`

**Usage**:
```bash
python extras/FSM_tester/FSM_Tester.py
```

**What it does**:
- Reads real flight data CSV (1,873 points)
- Calculates vertical velocity from altitude
- Runs FSM detection functions
- Plots altitude with state transitions marked
- Validates threshold values

**Validation Dataset**: `extras/FSM_tester/13_30_11-Dados.csv`
- **Source**: Real rocket flight (Team #100)
- **Points**: 1,873 telemetry samples
- **Phases captured**: Liftoff → Burnout → Apogee → Freefall → Landing

### 2. Expected Results
```
IDLE      → 0.00s (launch)
LIFTOFF   → 0.20s (15+ m/s² detected)
BURNOUT   → 2.10s (motor cutoff)
APOGEE    → 4.50s (vz ≈ 0 m/s)
FREEFALL  → 4.55s (falling, no chute)
PARACHUTE → 5.20s (deployed at 750m)
LANDED    → 45.0s (ground contact)
```

### 3. Safety Validation Checklist
- [ ] No premature parachute deployment (before apogee)
- [ ] No stuck states (max 30s timeout per state)
- [ ] Robust to sensor noise (NaN, spikes)
- [ ] Works with missing GPS data
- [ ] Handles IMU saturation (>16g)

## Implementation Guide

### Step 1: Create FSM Class
```cpp
// flight/FlightStateMachine.h
#ifndef FLIGHT_STATE_MACHINE_H
#define FLIGHT_STATE_MACHINE_H

#include "../sensors/BMP585Sensor.h"
#include "../sensors/LSM6DS3Sensor.h"

enum FlightState {
  IDLE = 0,
  LIFTOFF = 1,
  BURNOUT = 2,
  APOGEE = 3,
  FREEFALL = 4,
  PARACHUTE = 5,
  LANDED = 6
};

class FlightStateMachine {
private:
  FlightState _currentState;
  unsigned long _stateEntryTime;
  float _maxAltitude;
  
  BMP585Sensor* _baro;
  LSM6DS3Sensor* _imu;
  
  // Detection functions
  bool detectLiftoff();
  bool detectBurnout();
  bool detectApogee();
  bool detectFreefall();
  bool detectParachute();
  bool detectLanded();
  
public:
  FlightStateMachine(BMP585Sensor* baro, LSM6DS3Sensor* imu);
  
  void update();
  FlightState getState() const { return _currentState; }
  const char* getStateName() const;
  unsigned long getTimeInState() const;
};

#endif
```

### Step 2: Implement Detection Functions
```cpp
bool FlightStateMachine::detectLiftoff() {
  if (_currentState != IDLE) return false;
  
  float ax, ay, az;
  _imu->getAcceleration(&ax, &ay, &az);
  float totalAccel = sqrt(ax*ax + ay*ay + az*az);
  float height = _baro->getAltitude();
  
  // Guard: Must be above 5m to confirm liftoff
  if (height < 5.0) return false;
  
  // Threshold: Total acceleration > 15 m/s²
  if (totalAccel > 15.0) {
    Serial.println("🚀 LIFTOFF DETECTED");
    Serial.printf("   Total Accel: %.2f m/s², Height: %.2f m\n", 
                  totalAccel, height);
    return true;
  }
  
  return false;
}
```

### Step 3: Update FSM
```cpp
void FlightStateMachine::update() {
  unsigned long timeInState = millis() - _stateEntryTime;
  
  // Update max altitude continuously
  float currentAlt = _baro->getAltitude();
  if (currentAlt > _maxAltitude) {
    _maxAltitude = currentAlt;
  }
  
  // State machine logic
  switch (_currentState) {
    case IDLE:
      if (detectLiftoff()) {
        transitionTo(LIFTOFF);
      }
      break;
      
    case LIFTOFF:
      if (detectBurnout()) {
        transitionTo(BURNOUT);
      }
      // Timeout: If in LIFTOFF for >10s, assume burnout
      else if (timeInState > 10000) {
        Serial.println("⚠️ LIFTOFF timeout, forcing BURNOUT");
        transitionTo(BURNOUT);
      }
      break;
      
    case BURNOUT:
      if (detectApogee()) {
        transitionTo(APOGEE);
      }
      // Timeout: Max 30s coast phase
      else if (timeInState > 30000) {
        Serial.println("⚠️ BURNOUT timeout, forcing APOGEE");
        transitionTo(APOGEE);
      }
      break;
      
    // ... (continue for other states)
  }
}

void FlightStateMachine::transitionTo(FlightState newState) {
  Serial.printf("FSM: %s → %s\n", 
                getStateName(_currentState), 
                getStateName(newState));
  
  _currentState = newState;
  _stateEntryTime = millis();
  
  // Entry actions
  switch (newState) {
    case LIFTOFF:
      buzzer.beep(100, 2);  // 2 short beeps
      break;
    case APOGEE:
      buzzer.beep(100, 3);  // 3 short beeps
      break;
    case PARACHUTE:
      deployParachute();
      buzzer.beepContinuous();
      break;
    case LANDED:
      buzzer.beaconMode();
      startWiFiAP();
      break;
  }
}
```

## Testing Protocol

### Bench Test (Serial Input)
```bash
# Flash test firmware
cd test/FSM
# Upload FSM.ino

# Run Python data feeder
cd extras/FSM_tester
python flight_inserter.py
```

### Expected Output
```
🚀 FSM Test Starting
📊 Reading telemetry from Serial...

[0.00s] STATE: IDLE
[0.20s] LIFTOFF DETECTED (accel: 18.5 m/s²)
[0.20s] STATE: LIFTOFF
[2.10s] BURNOUT DETECTED (az: -9.2 m/s²)
[2.10s] STATE: BURNOUT
[4.50s] APOGEE REACHED (max_alt: 1253m, vz: 0.2 m/s)
[4.50s] STATE: APOGEE
...
```

## Common Issues & Fixes

### False Liftoff Detection
**Symptom**: FSM enters LIFTOFF on launchpad  
**Cause**: Vibrations or sensor noise  
**Fix**: Add height guard (must be >5m)

### Stuck in BURNOUT
**Symptom**: Never reaches APOGEE  
**Cause**: vz threshold too strict  
**Fix**: Add timeout (30s) to force transition

### Premature Parachute
**Symptom**: Deploys before apogee  
**Cause**: False FREEFALL detection  
**Fix**: Ensure APOGEE state entered first

### No Landing Detection
**Symptom**: Stays in PARACHUTE forever  
**Cause**: Altitude offset or vz noise  
**Fix**: Widen thresholds (vz < 0.5, height < 10m)

## Resources
- `firmware/REFACTORING_PLAN.md` - Complete FSM specification
- `extras/FSM_tester/explicacao.md` - Detailed FSM explanation (541 lines)
- `extras/FSM_tester/FSM_Tester.py` - Python simulator
- `extras/FSM_tester/13_30_11-Dados.csv` - Real flight data
- `test/FSM/FSM.ino` - Arduino FSM test