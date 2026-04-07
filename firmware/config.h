/**
 * @file config.h
 * @brief Global configuration file for the avionics system
 * 
 * This file contains all pin definitions, constants, and parameters
 * used throughout the system. Centralizing configurations here makes
 * maintenance and parameter modifications easier.
 * 
 * @author Team #100
 * @date 2026
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

//==============================================================================
// TIMING AND LOOP CONFIGURATION
//==============================================================================

/**
 * Main loop execution interval in milliseconds
 * 200ms = ~5Hz sampling rate
 * Adjust this value to increase/decrease data collection rate
 */
#define INTERVAL 200

//==============================================================================
// LORA COMMUNICATION CONFIGURATION
//==============================================================================

/**
 * LoRa module operating frequency in Hz
 * 868 MHz is the ISM frequency used in Europe
 * For Brazil/Americas, consider 915 MHz (915E6)
 */
#define LORA_FREQ 868E6

/**
 * Slave Select (SS/CS) pin for SPI communication with LoRa module
 */
#define SS_LORA 7

/**
 * Reset pin for LoRa module
 * Used to reinitialize the module in case of issues
 */
#define RST_LORA 1

/**
 * DIO0 pin of LoRa module
 * Used for interrupts and transmission complete signaling
 */
#define DIO0_LORA 2

/**
 * Synchronization word for LoRa communication
 * Ensures only devices with the same word communicate
 * Change this value to create isolated LoRa networks
 */
#define SYNC_WORD 0xF3

//==============================================================================
// PIN DEFINITIONS - ACTUATORS
//==============================================================================

/**
 * Digital pin connected to parachute servo motor
 * Uses PWM to control servo position
 */
#define SERVO_PIN 3

/**
 * Digital pin connected to piezoelectric buzzer
 * Emits sound signals for status indication
 */
#define BUZZER_PIN 0

//==============================================================================
// PIN DEFINITIONS - GPS
//==============================================================================

/**
 * RX pin for serial communication with GPS module
 * Connects to GPS module TX
 */
#define RX_GPS 20

/**
 * TX pin for serial communication with GPS module
 * Connects to GPS module RX
 */
#define TX_GPS 21

//==============================================================================
// PARACHUTE CONTROL CONSTANTS
//==============================================================================

/**
 * Maximum servo position (parachute open)
 * Value in degrees: 0° = fully open
 */
const int MAXPOS = 0;

/**
 * Minimum servo position (parachute closed)
 * Value in degrees: 90° = fully closed
 */
const int MINPOS = 90;

/**
 * Altitude drop threshold in meters
 * Parachute will only deploy after altitude drops this value
 * below the maximum peak reached (detects descent after apogee)
 */
const float ALTITUDE_DROP_THRESHOLD = 10.0;

/**
 * Minimum altitude for parachute deployment in meters
 * Prevents opening too high (outside recovery zone)
 * Adjust according to competition rules
 */
const float ALTITUDE_THRESHOLD = 750.0;

/**
 * Minimum descent velocity for parachute deployment in m/s
 * Ensures parachute only opens in free fall
 * Absolute value: 80 m/s ≈ 288 km/h
 */
const float VELOCITY_THRESHOLD = 80.0;

//==============================================================================
// TEAM IDENTIFICATION
//==============================================================================

/**
 * Team unique identifier
 * Used as prefix in all telemetry transmissions
 * Allows identifying data from different teams
 */
const String TEAM_ID = "#100";

//==============================================================================
// FILESYSTEM CONFIGURATION
//==============================================================================

/**
 * CSV data file name
 * Stores all telemetry readings
 */
String file_name = "Dados.csv";

/**
 * Complete data file path
 * Will be filled during setup() with GPS timestamp
 * Format: /HH_MM_SS-Dados.csv
 */
String file_dir = "";

//==============================================================================
// WEB SERVER CONFIGURATION
//==============================================================================

/**
 * WiFi network name (SSID) for access point
 * Concatenates "Server" with team ID
 */
String ssid_str = "Server " + TEAM_ID;

/**
 * SSID in C-string format for WiFi library compatibility
 */
const char *ssid = ssid_str.c_str();

/**
 * WiFi network password
 * WARNING: Change to a more secure password in production environment
 */
const char *password = "Iamarobot";

#endif // CONFIG_H
