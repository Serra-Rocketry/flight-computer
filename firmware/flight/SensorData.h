/**
 * @file SensorData.h
 * @brief Shared data structures between FreeRTOS tasks
 *
 * Defines data structures used for inter-task communication
 * via FreeRTOS queues:
 * - SensorData: Sensor data (BMP585, LSM6DS3, GPS)
 * - LogMessage: Log messages for logger task
 *
 * @author #11 - Serra Rocketry
 * @date 2026-04-06
 * @version 1.0.0
 *
 * @see firmware/REFACTORING_PLAN.md lines 243-288 - Data Structures
 * @see firmware/REFACTORING_PLAN.md lines 113-153 - FSM Flight States
 * @see firmware/flight/FlightControlTask.h - Task that populates SensorData
 * @see firmware/flight/TelemetryTask.h - Task that consumes SensorData via queue
 */

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>
#include <cstring>  // For memset()

/**
 * @brief Flight state machine states
 *
 * Simplified 4-state machine.
 * Internal states (LIFTOFF, BURNOUT, APOGEE, FREEFALL) tracked
 * via boolean flags for better diagnostics without added complexity.
 *
 * Validated with real data in extras/FSM_tester/13_30_11-Dados.csv
 * Reference: test/FSM/FSM.ino (4-state implementation)
 */
enum FlightState {
  IDLE = 0,      ///< Pre-launch, waiting on ground
  ASCENT = 1,    ///< Climbing (LIFTOFF → BURNOUT → APOGEE)
  DESCENT = 2,   ///< Descent (FREEFALL → PARACHUTE → LANDING)
  LANDED = 3     ///< Landing detected, end of flight
};

/**
 * @brief Converte enum FlightState para string
 * 
 * @param state Estado de voo
 * @return const char* Nome do estado
 */
inline const char* getFlightStateName(FlightState state) {
  switch (state) {
    case IDLE:    return "IDLE";
    case ASCENT:  return "ASCENT";
    case DESCENT: return "DESCENT";
    case LANDED:  return "LANDED";
    default:      return "UNKNOWN";
  }
}

/**
 * @brief Sensor data structure
 *
 * Contains readings from all sensors (BMP585, LSM6DS3, GPS) and
 * flight state machine state. Sent by FlightControlTask
 * to TelemetryTask via sensorDataQueue.
 *
 * Actual size: **96 bytes** (~64 bytes struct + 32 bytes alignment)
 * Queue: 25 slots × 96 bytes = ~2.4KB RAM
 *
 * @note Size is larger than estimated (64 bytes) due to:
 *       - struct alignment (padding)
 *       - doubles for latitude/longitude (8 bytes each)
 *       - Still within RAM budget (409 KB available)
 *
 * @note FlightState uses only 4 values (IDLE, ASCENT, DESCENT, LANDED).
 *       Internal states (LIFTOFF, BURNOUT, APOGEE, FREEFALL) tracked
 *       as separate flags in FlightControlTask for diagnostics.
 *       See: test/FSM/FSM.ino for reference implementation.
 */
struct SensorData {
  // === TIMESTAMP ===
  unsigned long timestamp;    ///< Timestamp in milliseconds
  uint16_t packet_count;      ///< Sequential packet number

  // === BMP585 BAROMETER ===
  float altitude;             ///< Altitude in meters (relative to launchpad)
  float pressure;             ///< Pressure in hPa
  float temperature;          ///< Temperature in °C
  float verticalVelocity;     ///< Vertical velocity (Vz) in m/s
  float maxAltitude;          ///< Maximum altitude reached in meters

  // === LSM6DS3 IMU ===
  float accelX, accelY, accelZ;   ///< Acceleration in m/s²
  float gyroX, gyroY, gyroZ;      ///< Angular velocity in rad/s
  float totalAccel;               ///< Total acceleration magnitude in m/s²

  // === GPS (OPTIONAL) ===
  double latitude, longitude;     ///< GPS coordinates
  float gpsAltitude;              ///< GPS altitude in meters
  uint8_t satellites;             ///< Number of tracked satellites
  bool gps_valid;                 ///< True if GPS has valid fix

  // === FSM STATE ===
  FlightState state;              ///< Current flight state
  bool parachute_deployed;        ///< True if parachute deployed

  /**
   * @brief Constructor with safe initialization of all fields
   *
   * CRITICAL: All fields are initialized to avoid:
   * - Reading uninitialized values (undefined behavior)
   * - NaN propagation in FSM
   * - Parachute deployment decisions based on garbage data
   *
   * @note Default state is IDLE (waits for liftoff)
   */
  SensorData()
      : timestamp(0), packet_count(0),
        // BMP585
        altitude(0.0f), pressure(1013.25f), temperature(0.0f),
        verticalVelocity(0.0f), maxAltitude(0.0f),
        // LSM6DS3
        accelX(0.0f), accelY(0.0f), accelZ(9.81f),  // accelZ = gravity
        gyroX(0.0f), gyroY(0.0f), gyroZ(0.0f),
        totalAccel(9.81f),  // Initial = pure gravity
        // GPS
        latitude(0.0), longitude(0.0), gpsAltitude(0.0f), satellites(0),
        gps_valid(false),
        // FSM (4 states: IDLE, ASCENT, DESCENT, LANDED)
        state(IDLE), parachute_deployed(false) {}
};

/**
 * @brief Identifiers for tasks that send log messages via LoggerTask
 * @see LogMessage::taskId
 */
enum TaskId : uint8_t {
  TASK_ID_FLIGHT_CONTROL = 1,
  TASK_ID_TELEMETRY      = 2,
  TASK_ID_LOGGER         = 3,
};

/**
 * @brief Log severity levels
 * @see getLogLevelName()
 */
constexpr uint8_t LOG_LEVEL_DEBUG = 0;
constexpr uint8_t LOG_LEVEL_INFO  = 1;
constexpr uint8_t LOG_LEVEL_WARN  = 2;
constexpr uint8_t LOG_LEVEL_ERROR = 3;

/**
 * @brief Log message structure
 *
 * Sent by any task to LoggerTask via logQueue.
 * Enables thread-safe logging with severity levels.
 *
 * Actual size: **144 bytes** (~140 bytes estimated)
 * Queue: 50 slots × 144 bytes = ~7.2KB RAM
 *
 * @note Message buffer initialized with '\0' to avoid
 *       reading uninitialized data or string overflow
 */
struct LogMessage {
  char message[128];          ///< Log message (max 127 chars + null terminator)
  unsigned long timestamp;    ///< Timestamp in milliseconds
  uint8_t taskId;             ///< ID of sending task (see TaskId enum above)
  uint8_t level;              ///< Severity level (see LOG_LEVEL_* constants above)

  /**
   * @brief Constructor with safe initialization
   *
   * CRITICAL: Buffer initialized with '\0' to avoid:
   * - String buffer overflow
   * - Reading undefined data
   * - Garbage characters in logs
   */
  LogMessage()
      : timestamp(0), taskId(0), level(0) {
    memset(message, 0, sizeof(message));  // Initialize buffer with zeros
  }
};

/**
 * @brief Converts log level to string
 *
 * @param level Log level
 * @return const char* Level name
 */
inline const char* getLogLevelName(uint8_t level) {
  switch (level) {
    case 0: return "[DEBUG]";
    case 1: return "[INFO]";
    case 2: return "[WARN]";
    case 3: return "[ERROR]";
    default: return "[???]";
  }
}

#endif // SENSOR_DATA_H
