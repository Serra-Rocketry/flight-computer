/**
 * @file telemetry_module.h
 * @brief Telemetry data aggregation, logging, and transmission system
 * 
 * This module serves as the central hub for collecting sensor data from all
 * subsystems, formatting it into standardized telemetry packets, and distributing
 * it through multiple channels (Serial, LoRa, filesystem). It orchestrates the
 * entire data pipeline from sensor reading to storage and transmission.
 * 
 * Data flow:
 * 1. Aggregate sensor data from BMP280, MPU6050, and GPS modules
 * 2. Format data into CSV string with timestamp and metadata
 * 3. Transmit via Serial monitor and LoRa radio
 * 4. Store to LittleFS filesystem for post-flight analysis
 * 5. Provide audio feedback via buzzer
 * 
 * Telemetry format (CSV):
 * TEAM_ID,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd
 * 
 * @note This module depends on all sensor modules being properly initialized
 * @warning Filesystem must be ready before calling logData() to prevent data loss
 * 
 * @author Team #100 Avionics
 * @date 2024
 */

#ifndef TELEMETRY_MODULE_H
#define TELEMETRY_MODULE_H

#include <Arduino.h>
#include "config.h"
#include "bmp280_sensor.h"
#include "mpu6050_sensor.h"
#include "gps_module.h"
#include "lora_module.h"
#include "filesystem_module.h"
#include "buzzer_module.h"

//==============================================================================
// GLOBAL TELEMETRY VARIABLES
//==============================================================================

/**
 * @brief Telemetry packet counter
 * 
 * Incremented with each data logging operation. Used to track packet sequence
 * and identify missing data in post-flight analysis. Resets on power cycle.
 */
int packet_count = 0;

/**
 * @brief Previous timestamp in milliseconds
 * 
 * Stores the last time data was logged, used in main loop to control
 * logging interval (typically 200ms as defined by INTERVAL in config.h).
 */
unsigned long previous_millis = 0;

//==============================================================================
// DATA AGGREGATION
//==============================================================================

/**
 * @brief Aggregate all sensor data into a single CSV-formatted string
 * 
 * Collects current readings from all sensors and combines them into a
 * comma-separated string ready for transmission and storage. This function
 * calls the data retrieval functions from each sensor module.
 * 
 * Data sources:
 * - BMPData(): Altitude (pressure-based), temperature, humidity, pressure
 * - MPUData(): Gyroscope (x,y,z), Accelerometer (x,y,z)
 * - GPSData(): Time, date, altitude (GPS), latitude, longitude, satellites
 * 
 * @return Complete telemetry string in CSV format combining all sensor data
 * 
 * @note The returned string does NOT include TEAM_ID, timestamp, count, or parachute status
 * @note These fields are added later by logData() function
 * 
 * @see BMPData() in bmp280_sensor.h
 * @see MPUData() in mpu6050_sensor.h
 * @see GPSData() in gps_module.h
 */
String getDataString()
{
  return BMPData() + "," + MPUData() + "," + GPSData();
}

//==============================================================================
// DUAL-CHANNEL OUTPUT
//==============================================================================

/**
 * @brief Print message to both Serial monitor and LoRa radio simultaneously
 * 
 * Provides redundant output to ensure critical messages reach the ground
 * station through at least one channel. Also triggers a confirmation beep
 * to provide audio feedback for each transmission.
 * 
 * Use cases:
 * - System initialization messages
 * - Error notifications
 * - Telemetry data packets
 * 
 * @param message Message string to transmit through both channels
 * 
 * @note Message is sent to Serial at configured baud rate (115200)
 * @note LoRa transmission may take several milliseconds depending on message length
 * @note Buzzer beep provides audio confirmation of transmission
 * 
 * @see sendLoRa() in lora_module.h for LoRa transmission details
 * @see buzzSignal() in buzzer_module.h for audio feedback
 */
void printBoth(const String &message)
{
  Serial.println(message);     // Output to USB serial connection
  sendLoRa(message);           // Transmit via LoRa radio
  buzzSignal("Beep");          // Audio confirmation of transmission
}

//==============================================================================
// TELEMETRY LOGGING
//==============================================================================

/**
 * @brief Log and transmit complete telemetry data packet from all sensors
 * 
 * This is the main telemetry function called periodically from the main loop.
 * It performs the complete data pipeline: collection, formatting, transmission,
 * and storage. The function assembles a complete telemetry packet including
 * metadata and sensor readings.
 * 
 * Telemetry packet format (CSV):
 * TEAM_ID,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd
 * 
 * Where:
 * - TEAM_ID: Team identification number (from config.h)
 * - millis: Time since startup in milliseconds
 * - count: Sequential packet number
 * - altp: Altitude from pressure sensor (meters)
 * - temp: Temperature (°C)
 * - umi: Humidity (%) - Note: BMP280 doesn't have humidity, may be 0
 * - p: Atmospheric pressure (Pa)
 * - gp, gr, gy: Gyroscope pitch, roll, yaw (°/s)
 * - ap, ar, ay: Accelerometer pitch, roll, yaw (g)
 * - hora: GPS time (HH:MM:SS)
 * - data: GPS date (DD/MM/YYYY)
 * - alt: GPS altitude (meters)
 * - lat: Latitude (decimal degrees)
 * - lon: Longitude (decimal degrees)
 * - sat: Number of GPS satellites
 * - pqd: Parachute deployment status (0=stowed, 1=deployed)
 * 
 * Operations performed:
 * 1. Collect sensor data via getDataString()
 * 2. Prepend metadata (TEAM_ID, timestamp, packet count)
 * 3. Append parachute deployment status
 * 4. Transmit via Serial and LoRa (with audio feedback)
 * 5. Append to data file in filesystem
 * 6. Increment packet counter
 * 
 * @param current_millis Current time in milliseconds since system startup
 * @param parachute_deployed Boolean status of parachute deployment (true=deployed)
 * 
 * @note This function should be called at regular intervals (e.g., every 200ms)
 * @note Filesystem must be initialized before calling this function
 * @warning If filesystem write fails, data will be lost (only transmitted, not stored)
 * 
 * @see getDataString() for sensor data collection
 * @see printBoth() for dual-channel transmission
 * @see appendFile() in filesystem_module.h for data storage
 * @see TEAM_ID and file_dir are defined in config.h
 */
void logData(unsigned long current_millis, bool parachute_deployed)
{
  // Collect all sensor readings in CSV format
  String readings = getDataString();
  
  // Assemble complete telemetry packet with metadata
  String data_string = TEAM_ID + "," + String(current_millis) + "," + 
                       String(packet_count) + "," + readings + "," + 
                       parachute_deployed;
  
  // Transmit through multiple channels and store to filesystem
  printBoth(data_string);              // Send via Serial and LoRa (with beep)
  appendFile(file_dir, data_string);   // Append to data file on LittleFS
  
  // Increment packet counter for next transmission
  packet_count++;
}

#endif // TELEMETRY_MODULE_H
