/**
 * @file LSM6DS3Sensor.h
 * @brief LSM6DS3 IMU sensor driver
 *
 * @see LSM6DS3Sensor.cpp for implementation
 * @see firmware/REFACTORING_PLAN.md Phase 4
 */

#ifndef LSM6DS3SENSOR_H
#define LSM6DS3SENSOR_H

#include "ISensor.h"
#include <Adafruit_LSM6DS3.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>


/**
 * @brief LSM6DS3 IMU sensor implementation
 *
 * Implements ISensor interface for LSM6DS3 inertial measurement unit.
 * Provides 6-axis motion detection (accelerometer + gyroscope) with
 * safety validations (NaN/Inf rejection, range checking).
 */
class LSM6DS3Sensor : public ISensor {
public:
  LSM6DS3Sensor();

  // ISensor interface
  bool begin() override;
  void update() override;
  String getData() override;
  bool isReady() override;

  // IMU-specific getters
  float getAccelZ() const;
  float getTotalAccel() const;

  // Vector accessors (Issue #6 requirement)
  void getAcceleration(float* x, float* y, float* z) const {
    if (x) *x = _accelX;
    if (y) *y = _accelY;
    if (z) *z = _accelZ;
  }

  void getGyroscope(float* x, float* y, float* z) const {
    if (x) *x = _gyroX;
    if (y) *y = _gyroY;
    if (z) *z = _gyroZ;
  }

private:
  Adafruit_LSM6DS3 _lsm;
  bool _ready;
  float _accelX, _accelY, _accelZ;
  float _gyroX, _gyroY, _gyroZ;
  float _totalAccel;
};


#endif // LSM6DS3SENSOR_H
