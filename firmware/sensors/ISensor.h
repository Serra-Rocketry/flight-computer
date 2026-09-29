/**
 * @file ISensor.h
 * @brief Abstract interface for flight computer sensors
 *
 * Defines the standard interface that all sensors must implement,
 * enabling abstraction and polymorphism for BMP585, LSM6DS3, GPS, etc.
 *
 * @author #11 - Serra Rocketry
 * @date 2026-04-06
 * @version 1.0.0
 *
 * @see firmware/REFACTORING_PLAN.md lines 54-57 - Directory Structure
 * @see firmware/REFACTORING_PLAN.md lines 365-386 - Base Interface (Phase 2)
 * @see firmware/sensors/BMP585Sensor.h - Concrete implementation example
 * @see firmware/flight/FlightControlTask.h - FreeRTOS task usage
 */

#ifndef ISENSOR_H
#define ISENSOR_H

#include <Arduino.h>

/**
 * @brief Abstract interface for flight computer sensors
 *
 * All sensors (barometer, IMU, GPS, etc.) must inherit from this class
 * and implement the defined methods.
 *
 * @example
 * **Example 1: Sensor Implementation (BMP585Sensor)**
 *
 * @code{.cpp}
 * #include "ISensor.h"
 * #include <Adafruit_BMP5XX.h>
 *
 * class BMP585Sensor : public ISensor {
 * private:
 *   Adafruit_BMP5XX _bmp;
 *   float _altitude;
 *   bool _isReady;
 *
 * public:
 *   bool begin() override {
 *     if (!_bmp.begin_I2C(0x77)) {
 *       return false;  // Sensor not found
 *     }
 *     _isReady = true;
 *     return true;
 *   }
 *
 *   void update() override {
 *     if (!_isReady) return;
 *     sensors_event_t temp_event, pressure_event;
 *     _bmp.getEvent(&pressure_event, &temp_event);
 *     _altitude = _bmp.readAltitude(1013.25);
 *   }
 *
 *   String getData() override {
 *     return String(_altitude) + "m";
 *   }
 *
 *   bool isReady() override {
 *     return _isReady;
 *   }
 * };
 * @endcode
 *
 * **Example 2: FreeRTOS Task Usage (50Hz)**
 *
 * @code{.cpp}
 * // Global sensor instance
 * ISensor* g_baroSensor = nullptr;
 *
 * // Setup initialization
 * void setup() {
 *   Serial.begin(115200);
 *   delay(1000);
 *
 *   g_baroSensor = new BMP585Sensor();
 *   if (!g_baroSensor->begin()) {
 *     Serial.println("ERROR: BMP585 initialization failed!");
 *     while(1);  // Safe halt
 *   }
 *   Serial.println("OK: BMP585 initialized");
 * }
 *
 * // FlightControlTask (Core 1, 50Hz)
 * void flightControlTask(void* parameter) {
 *   TickType_t xLastWakeTime = xTaskGetTickCount();
 *   const TickType_t xFrequency = pdMS_TO_TICKS(20);  // 50Hz = 20ms
 *
 *   while(true) {
 *     // Update sensor (MUST BE NON-BLOCKING!)
 *     if (g_baroSensor->isReady()) {
 *       g_baroSensor->update();
 *       String data = g_baroSensor->getData();
 *       Serial.println(data);  // CSV or JSON
 *     }
 *
 *     // Delay without blocking other tasks
 *     vTaskDelayUntil(&xLastWakeTime, xFrequency);
 *   }
 * }
 * @endcode
 *
 * @note update() **MUST BE NON-BLOCKING** to avoid blocking
 *       higher priority FreeRTOS tasks.
 *
 * @note For slow sensors (GPS), use TelemetryTask (5Hz) instead of
 *       FlightControlTask (50Hz). See firmware/REFACTORING_PLAN.md.
 */
class ISensor {
public:
  /**
   * @brief Virtual destructor (required for polymorphism)
   *
   * Allows derived objects to be deleted via base pointer
   * without memory leaks.
   */
  virtual ~ISensor() = default;

  /**
   * @brief Initializes the sensor
   *
   * Must be called in setup() before any update() calls.
   * May be blocking (e.g., during calibration).
   *
   * @return true if initialization successful, false on error
   *         (sensor not found, communication failed, etc.)
   */
  virtual bool begin() = 0;

  /**
   * @brief Updates sensor readings
   *
   * This function must be **non-blocking** and called regularly
   * by the task managing the sensor. For BMP585 and LSM6DS3, should
   * be called in FlightControlTask at 50Hz. For GPS, at 5Hz in TelemetryTask.
   */
  virtual void update() = 0;

  /**
   * @brief Returns sensor data as string (for Serial/logging)
   *
   * @return String with formatted sensor data (e.g., "BMP585: 1234.5m, 101.3hPa")
   */
  virtual String getData() = 0;

  /**
   * @brief Checks if sensor is ready for use
   *
   * @return true if sensor is ready (initialized and operational),
   *         false otherwise (not initialized, I2C error, etc.)
   */
  virtual bool isReady() = 0;
};

#endif // ISENSOR_H
