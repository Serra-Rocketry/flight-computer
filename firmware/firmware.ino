/**
 * @file firmware.ino
 * @brief Main firmware entry point for Team #100 Flight Computer (Avionics System)
 * 
 * This is the main program file for a rocket/drone flight computer that handles
 * sensor data collection, parachute deployment, telemetry transmission, and
 * data storage. The system uses a modular architecture with separate header files
 * for each subsystem.
 * 
 * System capabilities:
 * - Altitude and atmospheric pressure monitoring (BMP280)
 * - Inertial measurement unit for orientation (MPU6050)
 * - GPS position and time tracking
 * - Long-range telemetry via LoRa radio
 * - Autonomous parachute deployment based on flight profile
 * - Data logging to onboard flash storage (LittleFS)
 * - WiFi access point for data retrieval and management
 * - Audio feedback via buzzer for system status
 * 
 * Flight phases:
 * 1. Initialization: System startup and sensor calibration
 * 2. Pre-launch: Monitoring and data collection on ground
 * 3. Ascent: High-frequency data logging during powered flight
 * 4. Apogee detection: Tracking maximum altitude
 * 5. Descent: Parachute deployment and controlled landing
 * 6. Recovery: Post-flight data access via WiFi
 * 
 * @note All configuration parameters are in config.h
 * @note Data logging interval is 200ms (5Hz) as defined by INTERVAL
 * 
 * @author Team #100 Avionics
 * @date 2024
 */

//==============================================================================
// LIBRARY INCLUDES
//==============================================================================

#include <Wire.h>    // I2C communication for BMP280 and MPU6050
#include <SPI.h>     // SPI communication for LoRa module

//==============================================================================
// MODULE INCLUDES
//==============================================================================

#include "config.h"             // Global configuration and constants
#include "bmp280_sensor.h"      // Barometric pressure and altitude sensor
#include "mpu6050_sensor.h"     // Inertial measurement unit (IMU)
#include "gps_module.h"         // GPS positioning and timing
#include "lora_module.h"        // LoRa long-range radio communication
#include "filesystem_module.h"  // LittleFS data storage
#include "parachute_module.h"   // Parachute deployment control
#include "buzzer_module.h"      // Audio feedback and alerts
#include "server_module.h"      // WiFi access point and web server
#include "telemetry_module.h"   // Data aggregation and logging

//==============================================================================
// SETUP - ONE-TIME INITIALIZATION
//==============================================================================

/**
 * @brief Initialize all system components and prepare for flight
 * 
 * This function runs once at power-on and performs the following operations:
 * 1. Initialize communication buses (Serial, I2C)
 * 2. Initialize servo motor for parachute deployment
 * 3. Startup delay for system stabilization
 * 4. Acquire GPS time for unique filename generation
 * 5. Initialize filesystem and create data file with CSV header
 * 6. Start WiFi access point and web server
 * 7. Initialize all sensors (BMP280, MPU6050) and LoRa radio
 * 8. Provide audio feedback on initialization status
 * 
 * The system will restart automatically if critical initialization fails
 * (e.g., filesystem mount error). Non-critical errors (sensor failures)
 * are logged but allow the system to continue operating.
 * 
 * @note Serial monitor must be set to 115200 baud
 * @note The 5-second startup delay allows time to open Serial monitor
 * @warning System will restart on filesystem initialization failure
 * 
 * @see setup() is called automatically once by Arduino framework
 */
void setup()
{
  //----------------------------------------------------------------------------
  // Communication and Hardware Initialization
  //----------------------------------------------------------------------------
  
  Serial.begin(115200);   // Initialize USB serial at 115200 baud
  Wire.begin();           // Initialize I2C bus for sensors (SDA/SCL default pins)
  setupServo();           // Initialize servo motor for parachute deployment
  
  //----------------------------------------------------------------------------
  // Startup Delay and Status Messages
  //----------------------------------------------------------------------------
  
  pinMode(BUZZER_PIN, OUTPUT);  // Configure buzzer pin as output
  
  // 5-second countdown with status messages
  // Provides time to open Serial monitor and stabilize sensors
  for (int i = 0; i < 5; i++)
  {
    Serial.println("Initializing...");
    delay(1000);  // 1 second delay per iteration
  }

  //----------------------------------------------------------------------------
  // GPS Time Acquisition and Filename Generation
  //----------------------------------------------------------------------------
  
  setupGPS();  // Initialize GPS module and begin receiving data

  // Attempt to get GPS time for unique filename, fallback to default if unavailable
  // Format: HHMMSS-data.csv (e.g., "143052-data.csv" for 2:30:52 PM)
  String time_data = getGPSTimeString();
  file_dir = "/" + time_data + "-" + file_name;
  Serial.print("Saving data to: ");
  Serial.println(file_dir);

  //----------------------------------------------------------------------------
  // Filesystem Initialization and Data File Creation
  //----------------------------------------------------------------------------
  
  // CSV header defining all telemetry fields
  String data_header = "TEAM_ID,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd";
  
  // Mount filesystem and create data file with header
  // Critical operation - system will restart on failure
  if (!(setupLittleFS() && writeFile(file_dir, data_header)))
  {
    Serial.println("Filesystem error!");
    buzzSignal("Alert");    // 5 rapid beeps to indicate error
    delay(3000);            // Allow time to read error message
    ESP.restart();          // Restart system to retry initialization
  }

  //----------------------------------------------------------------------------
  // WiFi Access Point and Web Server Initialization
  //----------------------------------------------------------------------------
  
  setupServer();  // Create WiFi AP and start web server for data access

  //----------------------------------------------------------------------------
  // Sensor and Communication Module Initialization
  //----------------------------------------------------------------------------
  
  // Initialize all sensors and LoRa radio
  // Non-critical - system continues if initialization fails
  if (!(setupBMP() && setupMPU() && setupLoRa()))
  {
    printBoth("Module configuration error!");  // Log error to Serial and LoRa
    buzzSignal("Alert");                       // Audio alert (5 beeps)
    delay(3000);                               // Delay for error acknowledgment
  }
  else
  {
    printBoth("All modules initialized successfully!");  // Success message
    buzzSignal("Success");                                // Audio confirmation (3 beeps)
  }
}

//==============================================================================
// MAIN LOOP - CONTINUOUS OPERATION
//==============================================================================

/**
 * @brief Main flight computer control loop
 * 
 * This function runs continuously after setup() completes. It performs
 * time-based sensor sampling, data logging, and parachute deployment logic
 * at regular intervals defined by INTERVAL (default: 200ms = 5Hz).
 * 
 * Operations performed each cycle:
 * 1. Check if logging interval has elapsed (200ms)
 * 2. Read current altitude from BMP280 sensor
 * 3. Calculate vertical velocity from altitude change
 * 4. Log telemetry data (all sensors + timestamp + parachute status)
 * 5. Update highest altitude reached (for apogee detection)
 * 6. Evaluate parachute deployment conditions
 * 7. Update timestamp for next cycle
 * 
 * Parachute deployment logic (see parachute_module.h for details):
 * - Must be descending from apogee (altitude drop threshold)
 * - AND either: altitude below threshold OR descent velocity exceeds threshold
 * - Once deployed, parachute remains deployed (no retraction)
 * 
 * @note The web server runs asynchronously and doesn't need to be called here
 * @note GPS updates happen automatically via hardware serial interrupts
 * @note Loop frequency is controlled by INTERVAL constant in config.h
 * 
 * @see loop() is called repeatedly by Arduino framework
 * @see INTERVAL is defined in config.h (default: 200ms)
 * @see handleParachute() in parachute_module.h for deployment logic
 */
void loop()
{
  //----------------------------------------------------------------------------
  // Time-Based Execution Control
  //----------------------------------------------------------------------------
  
  unsigned long current_millis = millis();  // Get current time in milliseconds
  
  // Execute data logging and control logic at fixed interval (200ms)
  if (current_millis - previous_millis >= INTERVAL)
  {
    //--------------------------------------------------------------------------
    // Sensor Reading and Calculation
    //--------------------------------------------------------------------------
    
    // Read current altitude from pressure sensor
    float altitude = BMP.readAltitude(base_pressure);
    
    // Calculate vertical velocity (m/s) from altitude change over time
    // Positive = ascending, Negative = descending
    float velocity = (altitude - previous_altitude) / ((current_millis - previous_millis) / 1000.0);
    
    //--------------------------------------------------------------------------
    // Data Logging and Telemetry
    //--------------------------------------------------------------------------
    
    // Log complete telemetry packet to Serial, LoRa, and filesystem
    logData(current_millis, parachute_deployed);
    
    //--------------------------------------------------------------------------
    // Flight State Management
    //--------------------------------------------------------------------------
    
    // Update maximum altitude reached (for apogee detection)
    checkHighest(altitude);
    
    // Evaluate parachute deployment conditions and deploy if criteria met
    handleParachute(altitude, velocity);
    
    //--------------------------------------------------------------------------
    // Timestamp Update for Next Cycle
    //--------------------------------------------------------------------------
    
    previous_millis = current_millis;  // Update timestamp for next interval
  }
}
