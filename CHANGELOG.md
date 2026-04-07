# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- **Phase 1-2 Refactoring Completed**: Project structure and base interfaces for v2.0
  - `firmware/sensors/ISensor.h` - Abstract interface for all sensors (BMP585, LSM6DS3, GPS)
  - `firmware/flight/SensorData.h` - Shared data structures (SensorData, LogMessage)
  - Sensor abstraction layer enabling polymorphic sensor implementations
  - FreeRTOS-compatible data structures for inter-task communication via queues
  - Complete Doxygen documentation for all interfaces
  - **4-state Finite State Machine** (IDLE, ASCENT, DESCENT, LANDED) synchronized with test/FSM/FSM.ino

### Changed

- Migrated from v1.0 procedural architecture to v2.0 Object-Oriented design
- Restructured firmware directory with new `sensors/` and `flight/` modules
- **FSM simplified to 4 main states** (IDLE, ASCENT, DESCENT, LANDED) with internal event flags:
  - LIFTOFF, BURNOUT, APOGEE, FREEFALL tracked as boolean flags for diagnostics
  - Matches production implementation in test/FSM/FSM.ino
  - Reduces state complexity while maintaining detailed event tracking
- Updated software.md with new project structure documentation

### Fixed

- **Critical Safety Initializations** (commit 4b0c239):
  - All SensorData struct fields now have safe default values
  - LogMessage buffer initialized with zero-terminator
  - Prevents undefined behavior from uninitialized variables
  - Ensures parachute_deployed flag cannot be random on startup
  - Protects against NaN propagation in FSM transitions

### Documentation Added

- `docs/adr/002-sensor-abstraction.md` - Architectural Decision Record for ISensor interface
- `docs/phase-1-2-summary.md` - Executive summary of Phase 1-2 completion
- Updated `docs/software.md` with new module structure and component mapping

---

## [1.0.0] - 2026-01-27

### Added

- Initial project structure
- Base firmware for ESP32-C3 Super Mini
- Altitude monitoring system (MPU6050 sensor)
- GPS integration
- LoRa communication with operational base
- Telemetry storage in LittleFS
- Web interface for data access
- Component unit tests
- Software and hardware documentation

### Release Notes

- First functional version of the onboard computer
- Parachute deployment system still under testing
- Sensor calibration required before flight

---

## Versioning Guide

### MAJOR (X.0.0)

- Incompatible changes to firmware API or data structure

### MINOR (0.X.0)

- New features backward compatible with previous version
- Functionality improvements

### PATCH (0.0.X)

- Bug fixes
- Performance optimizations
- Documentation updates

## How to Report Changes

When making commits or pull requests, use the following categories:

- `feat:` for new features
- `fix:` for bug fixes
- `docs:` for documentation
- `test:` for tests
- `refactor:` for code refactoring
- `perf:` for performance improvements
- `chore:` for maintenance tasks

Example: `feat: add temperature sensor`
