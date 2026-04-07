/**
 * @file mpu6050_sensor.h
 * @brief MPU6050 IMU sensor control module
 * 
 * This module manages the MPU6050 inertial sensor, which combines:
 * - 3-axis accelerometer (measures linear acceleration in X, Y, Z)
 * - 3-axis gyroscope (measures angular velocity in X, Y, Z)
 * - Integrated temperature sensor
 * 
 * The IMU is essential for:
 * - Determining rocket orientation (pitch, roll, yaw)
 * - Detecting flight events (launch, stage separation, etc)
 * - Flight stability analysis
 * - Rotation and tumbling detection
 * 
 * Coordinate system:
 * - X: Lateral axis (roll)
 * - Y: Front axis (pitch)
 * - Z: Vertical axis (yaw/thrust)
 * 
 * @author Team #100
 * @date 2026
 */

#ifndef MPU6050_SENSOR_H
#define MPU6050_SENSOR_H

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

//==============================================================================
// MPU6050 SENSOR GLOBAL VARIABLES
//==============================================================================

/**
 * MPU6050 sensor object
 * Communication via I2C (Wire)
 */
Adafruit_MPU6050 MPU;

/**
 * Acceleration event structure
 * Contains accelerometer readings on 3 axes (m/s²)
 * - acceleration.x: Acceleration on X axis
 * - acceleration.y: Acceleration on Y axis
 * - acceleration.z: Acceleration on Z axis (includes gravity ~9.8 m/s²)
 */
sensors_event_t acc;

/**
 * Rotation event structure (gyroscope)
 * Contains gyroscope readings on 3 axes (rad/s)
 * - gyro.x: Angular velocity on X axis (pitch)
 * - gyro.y: Angular velocity on Y axis (roll)
 * - gyro.z: Angular velocity on Z axis (yaw)
 */
sensors_event_t gyr;

/**
 * Temperature event structure
 * Contains sensor internal temperature reading (°C)
 * Useful for thermal drift compensation
 */
sensors_event_t temp;

//==============================================================================
// INITIALIZATION FUNCTIONS
//==============================================================================

/**
 * Initialize MPU6050 IMU sensor
 * 
 * Configures sensor with Adafruit library default parameters:
 * - Accelerometer: ±2g (can be changed to ±4g, ±8g or ±16g)
 * - Gyroscope: ±250°/s (can be changed to ±500, ±1000 or ±2000°/s)
 * - Sampling rate: as per default configuration
 * 
 * @return true if initialization was successful
 * @return false if sensor communication failed
 * 
 * @note Sensor must be connected to default I2C bus (SDA/SCL)
 * @warning If returns false, IMU readings will not be available
 */
bool setupMPU()
{
  // Try to initialize MPU6050 sensor via I2C
  if (!MPU.begin())
  {
    Serial.println("MPU6050 initialization failed.");
    return false;
  }
  return true;
}

//==============================================================================
// DATA READING FUNCTIONS
//==============================================================================

/**
 * Collect and format MPU6050 IMU data as CSV string
 * 
 * Reads accelerometer and gyroscope from MPU6050 simultaneously and returns
 * formatted values for telemetry. Returned string follows format:
 * "gp,gr,gy,ap,ar,ay"
 * 
 * Where:
 * - gp: Gyro Pitch (angular velocity X in rad/s)
 * - gr: Gyro Roll (angular velocity Y in rad/s)
 * - gy: Gyro Yaw (angular velocity Z in rad/s)
 * - ap: Accel Pitch (acceleration X in m/s²)
 * - ar: Accel Roll (acceleration Y in m/s²)
 * - ay: Accel Yaw (acceleration Z in m/s²)
 * 
 * @return Formatted string with 6 comma-separated values
 * 
 * @note Acceleration on Z axis includes gravity (~9.8 m/s² when at rest)
 * @note To obtain absolute orientation, data integration is required
 * @see setupMPU() must be called before using this function
 */
String MPUData()
{
  // Read all sensors at once (accelerometer, gyroscope and temperature)
  // This ensures readings are synchronous
  MPU.getEvent(&acc, &gyr, &temp);

  // Extract acceleration values (in m/s²)
  float ap = acc.acceleration.x; // Acceleration on X axis (pitch)
  float ar = acc.acceleration.y; // Acceleration on Y axis (roll)
  float ay = acc.acceleration.z; // Acceleration on Z axis (yaw/vertical)

  // Extract angular velocity values (in rad/s)
  float gp = gyr.gyro.x; // Rotation on X axis (pitch)
  float gr = gyr.gyro.y; // Rotation on Y axis (roll)
  float gy = gyr.gyro.z; // Rotation on Z axis (yaw)

  // Format and return CSV string: gyroscope first, then accelerometer
  // Order: gp,gr,gy,ap,ar,ay
  return String(gp) + "," + String(gr) + "," + String(gy) + "," + 
         String(ap) + "," + String(ar) + "," + String(ay);
}

#endif // MPU6050_SENSOR_H
