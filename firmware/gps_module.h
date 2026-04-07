/**
 * @file gps_module.h
 * @brief GPS module control and data acquisition
 * 
 * This module manages the GPS receiver, providing:
 * - Precise time and date (UTC)
 * - Geographic coordinates (latitude/longitude)
 * - GPS-derived altitude
 * - Number of satellites in view
 * 
 * The GPS is essential for:
 * - Recovery operations (locate the rocket after landing)
 * - Flight path reconstruction
 * - Timestamp generation for data logging
 * - Altitude cross-validation with barometric sensor
 * 
 * Communication: Serial1 (UART) at 9600 baud
 * Protocol: NMEA 0183 (parsed by TinyGPS++ library)
 * 
 * @author Team #100
 * @date 2026
 */

#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"

//==============================================================================
// GPS GLOBAL VARIABLES
//==============================================================================

/**
 * TinyGPS++ parser object
 * Decodes NMEA sentences from GPS module
 * Provides easy access to position, time, date, altitude, and satellites
 */
TinyGPSPlus GPS;

//==============================================================================
// INITIALIZATION FUNCTIONS
//==============================================================================

/**
 * Initialize GPS module communication
 * 
 * Configures Serial1 for GPS communication and waits for initial fix.
 * The 3-second warm-up period allows the GPS to start outputting data.
 * 
 * Serial configuration:
 * - Baud rate: 9600 (standard for most GPS modules)
 * - Data format: 8N1 (8 data bits, no parity, 1 stop bit)
 * - RX/TX pins: Defined in config.h (RX_GPS, TX_GPS)
 * 
 * @note This function should be called in setup() before using GPS data
 * @note GPS may take 30-60 seconds for first fix (cold start)
 * @see getGPSTimeString() to check if time is valid
 */
void setupGPS()
{
  // Initialize Serial1 with GPS pins
  Serial1.begin(9600, SERIAL_8N1, RX_GPS, TX_GPS);
  
  unsigned long start = millis();
  
  // Wait 3 seconds for GPS initialization and feed incoming data
  while (millis() - start < 3000)
  {
    // Process any available GPS data
    while (Serial1.available() > 0)
    {
      GPS.encode(Serial1.read());
    }
  }
}

//==============================================================================
// TIME AND DATE FUNCTIONS
//==============================================================================

/**
 * Get GPS time formatted for filename generation
 * 
 * Returns current GPS time in HH_MM_SS format, suitable for creating
 * unique filenames. If GPS time is not yet valid (no fix), returns
 * a fallback string using millis().
 * 
 * @return Time string in format "HH_MM_SS" if GPS has valid time
 * @return Time string in format "init_<millis>" as fallback
 * 
 * @note GPS time is UTC (not local time)
 * @see GPSData() for formatted time with colons
 */
String getGPSTimeString()
{
  String time_data = "";
  
  // Check if GPS has valid time fix
  if (GPS.time.isValid())
  {
    // Format: HH_MM_SS (hours, minutes, seconds)
    time_data = String(GPS.time.hour()) + "_" + 
                String(GPS.time.minute()) + "_" + 
                String(GPS.time.second());
  }
  else
  {
    // GPS not ready yet, use system uptime as fallback
    time_data = "init_" + String(millis());
  }
  
  return time_data;
}

//==============================================================================
// DATA ACQUISITION FUNCTIONS
//==============================================================================

/**
 * Collect and format GPS data as CSV string
 * 
 * Reads GPS module and returns all available data formatted for telemetry.
 * Processes any pending NMEA sentences before extracting data.
 * Returns "nan" or "0" for invalid/unavailable fields.
 * 
 * Returned CSV format: "time,date,altitude,latitude,longitude,satellites"
 * 
 * Fields:
 * - time: UTC time in HH:MM:SS format (or "nan")
 * - date: Date in YYYY/MM/DD format (or "nan")
 * - altitude: GPS altitude in meters with 2 decimal places (or "nan")
 * - latitude: Latitude in decimal degrees with 8 decimals (or "nan")
 * - longitude: Longitude in decimal degrees with 8 decimals (or "nan")
 * - satellites: Number of satellites used in fix (or "0")
 * 
 * @return Formatted CSV string with GPS data
 * 
 * @note GPS altitude is MSL (Mean Sea Level), different from barometric
 * @note High precision (8 decimals) provides ~1mm resolution
 * @see setupGPS() must be called first
 */
String GPSData()
{
  // Process any pending GPS data from serial buffer
  while (Serial1.available() > 0)
  {
    GPS.encode(Serial1.read());
  }

  //----------------------------------------------------------------------------
  // TIME EXTRACTION
  //----------------------------------------------------------------------------
  
  String time_data = "nan";
  
  if (GPS.time.isValid())
  {
    int h = GPS.time.hour();      // UTC hour (0-23)
    int m = GPS.time.minute();    // Minutes (0-59)
    int s = GPS.time.second();    // Seconds (0-59)
    
    // Format with leading zeros: HH:MM:SS
    char buf[16];
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    time_data = String(buf);
  }

  //----------------------------------------------------------------------------
  // DATE EXTRACTION
  //----------------------------------------------------------------------------
  
  String date_data = "nan";
  
  if (GPS.date.isValid())
  {
    int y = GPS.date.year();      // Full year (e.g., 2026)
    int mo = GPS.date.month();    // Month (1-12)
    int d = GPS.date.day();       // Day (1-31)
    
    // Format: YYYY/MM/DD
    char buf[16];
    sprintf(buf, "%04d/%02d/%02d", y, mo, d);
    date_data = String(buf);
  }

  //----------------------------------------------------------------------------
  // LOCATION EXTRACTION
  //----------------------------------------------------------------------------
  
  String alt = "nan";   // Altitude
  String lat = "nan";   // Latitude
  String lon = "nan";   // Longitude
  String sats = "0";    // Satellites count

  if (GPS.location.isValid())
  {
    // GPS altitude in meters (MSL - Mean Sea Level)
    // 2 decimal places = ~1cm precision
    alt = String(GPS.altitude.meters(), 2);
    
    // Latitude in decimal degrees
    // 8 decimal places = ~1mm precision
    lat = String(GPS.location.lat(), 8);
    
    // Longitude in decimal degrees
    // 8 decimal places = ~1mm precision
    lon = String(GPS.location.lng(), 8);
    
    // Number of satellites used in position fix
    // More satellites = better accuracy (4+ recommended)
    sats = String(GPS.satellites.value());
  }

  // Combine all fields into CSV string
  String result = time_data + "," + date_data + "," + alt + "," + 
                  lat + "," + lon + "," + sats;
  
  return result;
}

#endif // GPS_MODULE_H
