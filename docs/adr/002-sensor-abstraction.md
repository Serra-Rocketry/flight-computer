# ADR 002: Implementar Interface Abstrata para Sensores

**Status**: Accepted  
**Date**: 2026-04-06  
**Deciders**: Team #100 - Serra Rocketry  
**Proposers**: Embedded Architecture Team

## Context

### Current Situation (v1.0)

The v1.0 firmware uses procedural code with sensor drivers directly integrated:
- BMP280 pressure sensor (`firmware/bmp280_sensor.h`)
- MPU6050 IMU (`firmware/mpu6050_sensor.h`)
- NEO-6M GPS (`firmware/gps_module.h`)

Each sensor has its own implementation pattern with no common interface. This creates:
1. Code duplication across sensor drivers
2. Tight coupling between flight control and sensor implementations
3. Difficulty adding new sensors (e.g., replacing BMP280 with BMP585)
4. Hard to mock sensors for unit testing
5. No standard for non-blocking updates (critical for FreeRTOS)

### Problem Statement

During the v2.0 refactoring to FreeRTOS architecture:
- FlightControlTask needs to update 3 different sensors at 50Hz
- Each sensor has different initialization and update patterns
- No standard way to handle sensor errors or fallbacks
- TelemetryTask cannot query sensor status uniformly

### Motivation for Change

We're transitioning from single-threaded Arduino loop to FreeRTOS multi-task architecture with:
- **FlightControlTask** (Core 1, 50Hz): Updates BMP585 + LSM6DS3
- **TelemetryTask** (Core 0, 5Hz): Updates GPS module
- **LoggerTask** (Core 0, background): Logs telemetry data

This requires:
1. Consistent interface for all sensors
2. Guaranteed non-blocking updates
3. Predictable memory footprint
4. Thread-safe data sharing via FreeRTOS queues

## Decision

**Implement an abstract ISensor interface** that all sensors must inherit from:

```cpp
class ISensor {
public:
  virtual ~ISensor() = default;
  virtual bool begin() = 0;         // Initialize sensor
  virtual void update() = 0;        // Non-blocking sensor update
  virtual String getData() = 0;     // Return formatted data
  virtual bool isReady() = 0;       // Check sensor ready state
};
```

Each physical sensor (BMP585, LSM6DS3, GPS) implements this interface:
```cpp
class BMP585Sensor : public ISensor { ... };
class LSM6DS3Sensor : public ISensor { ... };
class GPSModule : public ISensor { ... };
```

### Rationale

#### 1. **Abstraction & Polymorphism**

✅ Uniform interface for heterogeneous sensors
- Same method signatures regardless of sensor type
- FlightControlTask can update sensors without knowing their specifics
- Easy to swap implementations (e.g., BMP280 → BMP585)

```cpp
// Before (v1.0) - Tight coupling
void loop() {
  readBMP280(&altitude, &pressure);
  readMPU6050(&accelX, &accelY, &accelZ);
  readGPS(&lat, &lon);  // Different API!
}

// After (v2.0) - Uniform interface
void flightControlTask() {
  bmp->update();
  imu->update();
  gps->update();  // Same interface!
}
```

#### 2. **Testability**

✅ Mock sensors for unit testing without hardware
```cpp
class MockBMP585Sensor : public ISensor {
  float mockAltitude = 0.0f;
  void update() override { altitude = mockAltitude; }
};

// Test FSM with simulated sensor data
MockBMP585Sensor mockBaro;
mockBaro.mockAltitude = 1000.0f;
FSM.update(mockBaro);
```

#### 3. **FreeRTOS Compatibility**

✅ Explicit contract for non-blocking behavior
- `update()` must complete in <10ms (fits in 50Hz task)
- Documented as "non-blocking" in Doxygen
- Prevents tasks from blocking flight control

#### 4. **Multiple Sensor Support**

✅ Framework for sensor redundancy
```cpp
// Could have dual sensors later
class BMP585Primary : public ISensor { ... };
class BMP585Redundant : public ISensor { ... };

// FlightControlTask updates both
bmp_primary->update();
bmp_redundant->update();

// Compare readings
if (abs(alt1 - alt2) > THRESHOLD) {
  // Sensor disagreement - emergency protocol
}
```

#### 5. **Maintainability**

✅ Clear responsibility separation
- Sensor implementations: `firmware/sensors/*.h`
- Flight logic: `firmware/flight/FlightControlTask.h`
- Data structures: `firmware/flight/SensorData.h`
- Each component has single responsibility

### Design Constraints

1. **Virtual function overhead**: Minimal (~1% CPU on ESP32)
   - Sensor updates already dominate execution time
   - Branch prediction friendly on modern CPUs

2. **Memory footprint**: ~100 bytes per sensor instance
   - v-table pointers only (8 bytes)
   - Instance data (altitude, pressure, etc.)
   - Acceptable within 512KB RAM budget

3. **Real-time guarantees**: Must remain <10ms for non-blocking requirement
   - Validated by code review (no malloc/free in update())
   - No locks or mutexes in sensor updates

## Consequences

### Positive Consequences

✅ **Design Flexibility**
- Adding new sensor types requires only `ISensor` implementation
- Can support BMP585, BMP390, MS5607 without touching flight logic

✅ **Testability & Quality**
- Unit tests can use mock sensors
- Easier to validate FSM transitions with simulated data
- Clear contracts for sensor behavior

✅ **Code Reusability**
- Common initialization pattern for all sensors
- Consistent error handling strategy
- Shared data structure (SensorData) reduces coupling

✅ **FreeRTOS Ready**
- Non-blocking `update()` method fits RTOS paradigm
- Thread-safe via data queues (not shared pointers)
- Easy to add new tasks for new sensors

✅ **Safety-Critical Compliance**
- Explicit interface contracts reduce undefined behavior
- Doxygen documentation for all virtual methods
- Type-safe implementation patterns

### Negative Consequences

❌ **Virtual Function Call Overhead**
- ~5-10 CPU cycles per call on ESP32
- Minimal impact (sensor updates dominate computation)
- Could be optimized with final keyword if needed

```cpp
// If performance critical:
class BMP585Sensor final : public ISensor { ... };
```

❌ **Slightly More Complex Code**
- Requires understanding of virtual methods and inheritance
- Extra abstractions could confuse beginners
- Mitigated by clear documentation and examples

❌ **Runtime Binding vs Compile-Time Optimization**
- Virtual calls prevent some compiler optimizations
- But safety/flexibility tradeoff is worth it
- Always defers to ISensor pointer at task level

## Alternatives Considered

### Alternative 1: Keep Procedural Code (v1.0 Status Quo)

**Rejected** ✗ because:
- Cannot scale to multi-task architecture
- Difficult to add new sensors
- No standard for non-blocking requirement
- Hard to test without hardware

### Alternative 2: Template-Based Sensors (C++ Templates)

```cpp
template<typename SensorType>
class GenericSensor { ... };

class GenericSensor<BMP585> : public ISensor { ... };
```

**Rejected** ✗ because:
- Requires template specialization for each sensor
- Higher complexity than virtual inheritance
- Not better for FreeRTOS task management
- Same runtime characteristics

### Alternative 3: Functional Approach (Function Pointers)

```cpp
struct SensorOps {
  bool (*begin)(void);
  void (*update)(void);
  String (*getData)(void);
};
```

**Rejected** ✗ because:
- No type safety (C-like pattern)
- Harder to maintain state (data and functions separated)
- Less idiomatic C++

### Selected Solution: Virtual Inheritance (ISensor)

✅ Best match for:
- Type-safe polymorphism
- OOP design patterns
- Clear responsibility separation
- Industry-standard approach
- FreeRTOS best practices

## Implementation Plan

### Phase 2 (Completed - 2026-04-06)

- [x] Create `firmware/sensors/ISensor.h` interface
- [x] Create `firmware/flight/SensorData.h` data structures
- [x] Document with Doxygen
- [x] Pass code review (Safety-Critical)

### Phase 3 (In Progress)

- [ ] Implement `firmware/sensors/BMP585Sensor.h` (inherits ISensor)
- [ ] Implement `firmware/sensors/LSM6DS3Sensor.h` (inherits ISensor)
- [ ] Implement `firmware/sensors/GPSModule.h` (inherits ISensor)
- [ ] Unit tests with mock sensors

### Phase 4 (Planned)

- [ ] Integrate into FlightControlTask
- [ ] Integrate into TelemetryTask
- [ ] System integration tests on hardware

## Validation

### Code Review Validation
✅ Passes safety-critical code review (SAFETY_REVIEW_PHASE_1_2.md)
- Destrutor virtual correctly implemented
- No memory leaks via RAII pattern
- Type-safe inheritance

### Architecture Validation
✅ Conforms to REFACTORING_PLAN.md specification
- 4 virtual methods match plan (begin, update, getData, isReady)
- Documentation matches design intent
- Compatible with FreeRTOS queues

### Testing Strategy
- Unit tests with mock sensors
- Hardware-in-the-loop tests on ESP32-S3
- FSM validation with real flight data

## Future Decisions

This ADR enables future decisions:
1. **ADR 003**: Sensor redundancy and voting
2. **ADR 004**: Real-time scheduling for sensor tasks
3. **ADR 005**: Sensor failure detection and fallback strategies

## References

- [ISensor Interface](../../firmware/sensors/ISensor.h)
- [SensorData Structures](../../firmware/flight/SensorData.h)
- [Refactoring Plan](../../firmware/REFACTORING_PLAN.md) - Lines 365-386
- [Safety Review](../../SAFETY_REVIEW_PHASE_1_2.md)
- [FreeRTOS Best Practices](https://www.freertos.org/)
- [C++ Virtual Methods - cppreference.com](https://en.cppreference.com/w/cpp/language/virtual)

## Document History

| Date | Version | Author | Change |
|------|---------|--------|--------|
| 2026-04-06 | 1.0 | Embedded Architect | Initial ADR created |

---

**Decision Status**: ✅ **ACCEPTED**

This ADR was accepted on 2026-04-06 and is actively being implemented in Phase 2-3 of the Flight Computer v2.0 refactoring.
