# Contributing to the Flight Computer Project

Thank you for considering contributing to the Serra Rocketry onboard computer!

## Code of Conduct

All contributors must maintain a respectful and inclusive environment.

## How to Contribute

### 1. Reporting Bugs

Open an issue with:
- Clear title and detailed description
- Steps to reproduce
- Expected vs observed behavior
- Environment (Arduino IDE version, board, libraries)

### 2. Suggesting Improvements

Open an issue describing the goal, benefits, and possible implementation.

### 3. Submitting Pull Requests

#### Preparation
1. Fork the repository and create a branch
2. Branch naming: `feature/descriptive-name` or `fix/descriptive-name`
3. Keep your branch updated with `main`

#### Development
1. **Follow the project's code style** (see `AGENTS.md`):
   - Indentation: 2 spaces
   - Global variables: `snake_case`
   - Local variables / functions: `camelCase`
   - Private members: `_camelCase`
   - Constants: `UPPER_CASE`
   - Doxygen comments on public APIs
   - Comments in English

2. **v2.0 Architecture** (OOP + FreeRTOS + FSM):
   - Sensors implement `ISensor` interface (`sensors/`)
   - Flight logic in `FlightStateMachine` (`flight/`)
   - Real-time work in FreeRTOS tasks (`FlightControlTask`, `TelemetryTask`)
   - Constants in `config.h` (no magic numbers)
   - Validate all sensor inputs (NaN/Inf, range checks)
   - Multi-condition parachute logic with state + altitude + velocity

3. **Test your code**:
   - `python3 extras/FSM_tester/FSM_Tester.py` (FSM validation with real data)
   - `python3 extras/validate_telemetry_format.py`
   - Test on physical hardware when possible
   - Check compiler warnings

4. **Meaningful commits**:
   ```
   feat: add temperature sensor to firmware
   fix: fix accelerometer calibration
   docs: update pinout in hardware.md
   test: add FSM tests for apogee detection
   ```

#### Submitting PR
1. Clear title summarizing the change
2. Detailed description with what/why/how
3. PR Checklist:
   ```
   - [ ] Code follows project style (AGENTS.md)
   - [ ] Documentation updated
   - [ ] Tests added/updated
   - [ ] No compiler warnings
   - [ ] Tested on hardware
   - [ ] FSM validated with FSM_Tester.py (if FSM logic changed)
   ```

## Project Structure

```
firmware/
├── firmware.ino          # setup()/loop() — orchestrates init*Task()
├── config.h              # Pins, thresholds, constants
├── sensors/              # ISensor implementations
│   ├── ISensor.h         # Abstract interface
│   ├── BMP585Sensor.h/cpp
│   ├── LSM6DS3Sensor.h/cpp
│   └── GPSModule.h/cpp
├── flight/               # Flight logic
│   ├── SensorData.h      # Shared structs + enums
│   ├── FlightStateMachine.h/cpp
│   ├── FlightControlTask.h/cpp
│   ├── TelemetryTask.h/cpp
│   └── LoggerTask.h/cpp
├── modules/              # Peripherals
│   ├── parachute_module.h/cpp
│   ├── lora_module.h
│   ├── buzzer_module.h
│   └── filesystem_module.h
├── MODULOS.md            # Module documentation
└── REFACTORING_PLAN.md   # v2.0 architecture reference
test/                     # Unit tests
docs/                     # Documentation
extras/                   # Scripts and experimental code
hardware/                 # KiCad schematics and BOM
```

## Libraries and Dependencies

Before using a new library:
1. Check if a compatible one already exists in the project
2. Confirm compatibility with ESP32-S3 (primary) or ESP32-C3 (legacy)
3. Update `docs/software.md` with the new dependency

## Testing

- Firmware build: Arduino IDE → Board: `ESP32-S3 Dev Module` (primary)
- PlatformIO (if configured): `platformio run -e esp32-s3`
- FSM validation: `python3 extras/FSM_tester/FSM_Tester.py`
- Telemetry validation: `python3 extras/validate_telemetry_format.py`

## Documentation

- Update `docs/software.md` for code changes
- Update `docs/hardware.md` for hardware/pin changes
- Update `docs/telemetry-format.md` for telemetry wire format changes (single source of truth)
- Update `CHANGELOG.md` under `[Unreleased]`
- Keep code comments in English

## Questions?

Open an issue for public discussions or contact maintainers for confidential matters.

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (Serra Rocketry).
