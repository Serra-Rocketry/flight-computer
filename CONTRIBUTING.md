# Contributing to the Flight Computer Project

Thank you for considering contributing to the Serra Rocketry onboard computer! This document provides guidelines and instructions for contributing to the project.

## Code of Conduct

All contributors must maintain a respectful and inclusive environment. Abusive, discriminatory or offensive behavior will not be tolerated.

## How to Contribute

### 1. Reporting Bugs

Before opening an issue, check if the bug has not already been reported.

**When reporting a bug, include:**

- Clear and descriptive title
- Detailed description of the problem
- Specific steps to reproduce the error
- Expected vs observed behavior
- Environment (Arduino IDE version, board, libraries)
- Logs or screenshots if applicable

### 2. Suggesting Improvements

Suggestions for new features are welcome! Open an issue describing:

- The goal of the improvement
- Expected benefits
- Possible implementations (if applicable)

### 3. Submitting Pull Requests

#### Preparation

1. **Fork the repository** and create a branch for your feature/fix
2. **Branch naming**: use `feature/descriptive-name` or `fix/descriptive-name`
3. **Keep your branch updated** with `dev-2026`

#### Development

1. **Follow the project's code style**:
   - Indentation with 2 spaces (Arduino IDE standard)
   - Variable names in camelCase
   - Descriptive function names in camelCase
   - Comments in English

2. **Add documentation**:
   - Comment complex functions
   - Update docs/ if changing behavior
   - Document new configurations

3. **Test your code**:
   - Use tests in `test/` as reference
   - Test on real hardware if possible
   - Check memory consumption (flash/RAM)

4. **Meaningful commits**:
   ```
   feat: add temperature sensor to firmware
   fix: fix accelerometer calibration
   docs: update pinout in hardware.md
   test: add tests for GPS
   ```

#### Submitting PR

1. **Clear title**: summarize the change in one line
2. **Detailed description**:
   - What was changed and why
   - How to test the changes
   - Any breaking changes
   - References to related issues (#number)

3. **PR Checklist**:
   ```
   - [ ] Code follows project style
   - [ ] Documentation has been updated
   - [ ] Tests have been added/updated
   - [ ] No compiler warnings
   - [ ] Tested on hardware
   ```

## Project Structure

```
firmware/       Main code (.ino)
test/           Unit tests of components
docs/           Documentation (software, hardware, flowchart)
hardware/       KiCad schematics and component list
extras/         Scripts and experimental code
```

## Code Standards

### Arduino/C++

```cpp
// Global variables
volatile int sensorValue = 0;

// Functions with comments
void initSensors() {
  // Initialize I2C sensors
  Wire.begin();
}

// Data structures
struct SensorData {
  float altitude;
  float latitude;
  float longitude;
};
```

### Function Documentation

```cpp
/**
 * Calculate altitude based on barometer reading
 * @param pressure Pressure in Pa
 * @param seaLevelPressure Sea level pressure
 * @return Altitude in meters
 */
float calculateAltitude(float pressure, float seaLevelPressure) {
  // implementation
}
```

## Libraries and Dependencies

Before using a new library:

1. Check if a compatible one already exists in the project
2. Confirm compatibility with ESP32-C3
3. Update [software.md](docs/software.md) with the new dependency
4. Add installation instructions to README

## Testing

- Test locally before submitting
- For firmware, compile with `Sketch > Verify`
- Check for warnings and optimizations
- Test on physical board when possible

## Documentation

- Update [software.md](docs/software.md) for code changes
- Update [hardware.md](docs/hardware.md) for hardware changes
- Update [flowchart.md](docs/flowchart.md) if flow changes
- Keep code comments in English

## Questions?

- Open an **issue** for public discussions
- Contact maintainers for confidential questions
- See [docs/README.md](docs/README.md) for more documentation

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (Serra Rocketry).

---

**Thank you for helping to make this project better! 🚀**
