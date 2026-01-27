# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Planned new features will be listed here

### Changed

- Changes to existing functionality

### Fixed

- Bug fixes

### Removed

- Removed or discontinued features

### Security

- Security-related changes

---

## [Unreleased] - 2.0.0

### Added

- Project scaffolding for V2 Flight Computer
- Documentation placeholders (software.md, hardware.md, flowchart.md)
- Branch `dev-2026` created for active development

### Notes

- No functional firmware implemented yet
- FSM design and RTOS task planning in progress
- Repository ready for development of V2

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
