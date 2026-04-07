/**
 * @file bmp280_sensor.h
 * @brief BMP280 barometric sensor control module
 * 
 * This module manages the BMP280 pressure sensor, responsible for measuring:
 * - Atmospheric pressure
 * - Ambient temperature
 * - Barometric altitude (calculated from pressure)
 * 
 * The sensor is crucial for determining flight apogee and triggering the
 * parachute at the correct moment. Altitude is calculated using the
 * international barometric formula with calibration based on local sea-level pressure.
 * 
 * @author Team #100
 * @date 2026
 */

#ifndef BMP280_SENSOR_H
#define BMP280_SENSOR_H

#include <Arduino.h>
#include <Adafruit_BMP280.h>

//==============================================================================
// BMP280 SENSOR GLOBAL VARIABLES
//==============================================================================

/**
 * BMP280 sensor object
 * Communication via I2C (Wire)
 */
Adafruit_BMP280 BMP;

/**
 * Previous altitude in meters
 * Used to calculate vertical velocity (ascent/descent rate)
 */
float previous_altitude = 0;

/**
 * Maximum altitude reached in meters (apogee)
 * Updated continuously during flight
 * Used as reference to detect descent start
 */
float max_altitude = 0;

/**
 * Base altitude in meters (altitude at initialization)
 * Represents ground level at launch site
 */
float base_altitude = 0;

/**
 * Base pressure in hPa (hectopascal)
 * Atmospheric pressure measured at ground level
 * Used as reference for relative altitude calculation
 */
float base_pressure = 0;

//==============================================================================
// INITIALIZATION AND CALIBRATION FUNCTIONS
//==============================================================================

/**
 * Initialize BMP280 sensor and calibrate altitude reference
 * 
 * This function should be called during setup() before any reading.
 * Performs the following operations:
 * 1. Initialize I2C communication with sensor
 * 2. Read atmospheric pressure at ground level (base_pressure)
 * 3. Calculate initial altitude reference
 * 4. Initialize maximum and previous altitude values
 * 
 * @return true if initialization was successful
 * @return false if sensor communication failed
 * 
 * @note Ensure the rocket is at rest on the ground during this calibration
 * @warning If returns false, system will not have valid altitude readings
 */
bool setupBMP()
{
  // Try to initialize BMP280 sensor via I2C
  if (!BMP.begin())
  {
    Serial.println("BMP280 initialization failed.");
    return false;
  }
  
  // Read current atmospheric pressure and convert from Pa to hPa (divide by 100)
  base_pressure = BMP.readPressure() / 100;
  
  // Calculate initial altitude using base pressure as reference
  previous_altitude = BMP.readAltitude(base_pressure);
  
  // Initialize maximum altitude with current value (ground)
  max_altitude = previous_altitude;
  
  // Store base altitude (ground level)
  base_altitude = previous_altitude;
  
  return true;
}

//==============================================================================
// DATA READING FUNCTIONS
//==============================================================================

/**
 * Collect and format BMP280 sensor data as CSV string
 * 
 * Reads BMP280 sensors and returns formatted values for telemetry.
 * Returned string follows format: "altitude,temperature,humidity,pressure"
 * 
 * Returned fields:
 * - altitude: Barometric altitude in meters (relative to base pressure)
 * - temperature: Ambient temperature in degrees Celsius
 * - humidity: "nan" (BMP280 doesn't measure humidity, reserved for future sensor)
 * - pressure: Atmospheric pressure in hPa (hectopascal)
 * 
 * @return Formatted string with comma-separated values
 * 
 * @note Returned altitude is relative to calibration point (setupBMP)
 * @see setupBMP() for base pressure calibration
 */
String BMPData()
{
  // Read altitude using base pressure as reference (relative altitude)
  // Read temperature in degrees Celsius
  // "nan" for humidity (not supported by BMP280)
  // Read pressure and convert from Pa to hPa
  return String(BMP.readAltitude(base_pressure)) + "," + 
         String(BMP.readTemperature()) + "," + "nan," + 
         String(BMP.readPressure() / 100.0F);
}

//==============================================================================
// ANALYSIS AND TRACKING FUNCTIONS
//==============================================================================

/**
 * Check and update maximum altitude reached (apogee)
 * 
 * This function compares current altitude with recorded maximum and updates
 * max_altitude variable when a new peak is detected.
 * 
 * Essential for detecting flight apogee, the moment when altitude stops
 * increasing and starts decreasing. This point is used as reference
 * to trigger recovery system (parachute).
 * 
 * @param altitude Current altitude in meters (obtained from BMP280)
 * 
 * @note Should be called every cycle of main loop
 * @note max_altitude is a global variable updated by this function
 * @see handleParachute() which uses max_altitude to detect descent
 */
void checkHighest(float altitude)
{
  // If current altitude is greater than recorded maximum
  if (altitude > max_altitude)
  {
    // Update maximum altitude record
    max_altitude = altitude;
  }
}

#endif // BMP280_SENSOR_H
