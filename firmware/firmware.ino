// Library includes
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <ESP32Servo.h>
#include <TinyGPS++.h>
#include "LittleFS.h"
#include <LoRa.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

/* AVIONICS MODULE - START */

// Pin definitions and constants
#define INTERVAL 200 // Loop execution interval (ms, ~5Hz)

#define LORA_FREQ 868E6 // Operating frequency
#define SS_LORA 7
#define RST_LORA 1
#define DIO0_LORA 2
#define SYNC_WORD 0xF3 // Synchronization code

#define SERVO_PIN 3  // Servo motor pin
#define BUZZER_PIN 0 // Buzzer pin

#define RX_GPS 20 // GPS RX
#define TX_GPS 21 // GPS TX

// Object instantiation
Adafruit_BMP280 BMP;  // BMP280 object
Servo ParachuteServo; // Servo object
TinyGPSPlus GPS;      // GPS object
Adafruit_MPU6050 MPU; // MPU6050 object

// Global variables
int packet_count = 0; // Packet counter
unsigned long previous_millis = 0;
float previous_altitude = 0, max_altitude = 0, base_altitude = 0; // Variable and static altitudes
float base_pressure = 0;                                          // Base pressure
String file_name = "Dados.csv";                                   // File name for saving data
String file_dir = "";                                             // File directory
bool parachute_deployed = false;                                  // Parachute deployment verification
const int MAXPOS = 0, MINPOS = 90;                                // Maximum and minimum servo positions
const float ALTITUDE_DROP_THRESHOLD = 10.0;                       // Minimum 10m below max reference (adjust if necessary)
const float ALTITUDE_THRESHOLD = 750.0;                           // Minimum height to release parachute (adjust if necessary)
const float VELOCITY_THRESHOLD = 80.0;                            // Minimum descent velocity to release parachute (adjust if necessary)
const String TEAM_ID = "#100";                                    // Team ID
sensors_event_t acc, gyr, temp;                                   // Variables to store MPU6050 data

/**
 * Initialize the LittleFS file system
 * @return true if initialization successful, false otherwise
 */
bool setupLittleFS()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("Error mounting LittleFS.");
    return false;
  }
  return true;
}

/**
 * Initialize the BMP280 pressure sensor and calibrate altitude reference
 * @return true if initialization successful, false otherwise
 */
bool setupBMP()
{
  if (!BMP.begin())
  {
    Serial.println("BMP280 initialization failed.");
    return false;
  }
  base_pressure = BMP.readPressure() / 100;
  previous_altitude = BMP.readAltitude(base_pressure);
  max_altitude = previous_altitude;
  base_altitude = previous_altitude;
  return true;
}

/**
 * Initialize the LoRa communication module
 * @return true if initialization successful, false otherwise
 */
bool setupLoRa()
{
  LoRa.setPins(SS_LORA, RST_LORA, DIO0_LORA);
  if (!LoRa.begin(LORA_FREQ))
  {
    Serial.println("LoRa initialization failed.");
    return false;
  }
  LoRa.setSyncWord(SYNC_WORD);
  return true;
}

/**
 * Initialize the MPU6050 IMU sensor
 * @return true if initialization successful, false otherwise
 */
bool setupMPU()
{
  if (!MPU.begin())
  {
    Serial.println("MPU6050 initialization failed.");
    return false;
  }
  return true;
}

/**
 * Initialize the servo motor and set it to closed position
 */
void setupServo()
{
  ParachuteServo.attach(SERVO_PIN);
  delay(500);
  ParachuteServo.write(MINPOS);
}

/**
 * Generate audio signals using the buzzer for status indication
 * @param signal Signal type: "Alert" (error), "Success" (initialized), "Activated" (parachute deployed), "Beep" (standard)
 */
void buzzSignal(String signal)
{
  int frequency = 500;   // Tone frequency
  if (signal == "Alert") // Error signal during initialization
  {
    for (int i = 0; i < 5; i++)
    {
      tone(BUZZER_PIN, frequency, 200);
      delay(200 + 150);
    }
  }
  else if (signal == "Success") // Success signal on initialization
  {
    for (int i = 0; i < 3; i++)
    {
      tone(BUZZER_PIN, frequency, 100);
      delay(100 + 100);
    }
  }
  else if (signal == "Activated")
  {
    tone(BUZZER_PIN, frequency, 500);
  }
  else if (signal == "Beep") // Standard operation beep
  {
    tone(BUZZER_PIN, frequency, 50);
    delay(100);
  }
  else
  {
    Serial.println("Invalid signal!");
  }
}

/**
 * Log and print telemetry data from all sensors
 * Format: TEAM_ID,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd
 * @param current_millis Current time in milliseconds since startup
 */
void logData(unsigned long current_millis)
{
  String readings = getDataString();                                                                                                    // Get data from GPS, BMP280, and MPU6050
  String data_string = TEAM_ID + "," + String(current_millis) + "," + String(packet_count) + "," + readings + "," + parachute_deployed; // String with current data
  printBoth(data_string);
  appendFile(file_dir, data_string);
  packet_count++;
}

/**
 * Check if current altitude is the maximum reached and update if necessary
 * @param altitude Current altitude in meters
 */
void checkHighest(float altitude)
{
  if (altitude > max_altitude)
  {
    max_altitude = altitude;
  }
}

/**
 * Handle parachute deployment based on altitude and velocity criteria
 * Deploys parachute when: altitude drops 10m below peak AND (altitude < threshold OR descent velocity > threshold)
 * @param altitude Current altitude in meters
 * @param velocity Current descent velocity in m/s
 */
void handleParachute(float altitude, float velocity)
{
  if (!parachute_deployed) // Check if parachute has already been deployed
  {
    if (altitude <= max_altitude - ALTITUDE_DROP_THRESHOLD && (altitude < ALTITUDE_THRESHOLD || abs(velocity) > VELOCITY_THRESHOLD))
    {
      ParachuteServo.write(MAXPOS);
      unsigned long startTime = millis();
      while (millis() - startTime < 500) // Wait 500ms to verify servo opened
      {
        if (ParachuteServo.read() == MAXPOS)
        {
          break;
        }
      }
      if (ParachuteServo.read() != MAXPOS)
      {
        printBoth("ERROR: Servo failed to open!");
      }
      printBoth("Parachute deployed. Altitude: " + String(altitude) + " Vel: " + String(velocity));
      parachute_deployed = true;
    }
  }
  else
  {
    buzzSignal("Activated");
  }
  previous_altitude = altitude;
}

/**
 * Write data to file (creates new file with data)
 * @param path File path
 * @param data_string Data string to write
 * @return true if write successful, false otherwise
 */
bool writeFile(const String &path, const String &data_string)
{
  File file = LittleFS.open(path, FILE_WRITE);
  if (!file) // If file open fails
  {
    Serial.println("Failed to open file for writing.");
    return false;
  }
  if (file.println(data_string)) // If write to file succeeds
  {
    Serial.println("File written.");
  }
  else // If write fails
  {
    Serial.println("File write failed.");
    file.close();
    return false;
  }
  file.close();
  return true;
}

/**
 * Append data to existing file
 * @param path File path
 * @param message Message to append
 */
void appendFile(const String &path, const String &message)
{
  File file = LittleFS.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending.");
    return;
  }

  if (!file.println(message))
  {
    Serial.println("Failed to append message.");
  }
  file.close();
}

/**
 * Print message to both Serial monitor and LoRa communication
 * @param message Message to transmit
 */
void printBoth(const String &message)
{
  Serial.println(message);
  sendLoRa(message);
}

/**
 * Send message via LoRa communication module
 * @param message Message to transmit via LoRa
 */
void sendLoRa(const String &message)
{
  LoRa.beginPacket();
  LoRa.print(message);
  if (LoRa.endPacket())
  {
    Serial.println("LoRa message sent.");
    buzzSignal("Beep");
  }
  else
  {
    Serial.println("ERROR sending LoRa message!");
  }
}

/**
 * Collect and format GPS data (time, date, altitude, latitude, longitude, satellites)
 * @return Formatted GPS data string with comma-separated values
 */
String GPSData()
{
  while (Serial1.available() > 0)
  {
    GPS.encode(Serial1.read());
  }

  // ----- Time -----
  String time_data = "nan";
  if (GPS.time.isValid())
  {
    int h = GPS.time.hour();
    int m = GPS.time.minute();
    int s = GPS.time.second();
    char buf[16];
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    time_data = String(buf);
  }

  // ----- Date -----
  String date_data = "nan";
  if (GPS.date.isValid())
  {
    int y = GPS.date.year();
    int mo = GPS.date.month();
    int d = GPS.date.day();
    char buf[16];
    sprintf(buf, "%04d/%02d/%02d", y, mo, d);
    date_data = String(buf);
  }

  // ----- Location -----
  String alt = "nan";
  String lat = "nan";
  String lon = "nan";
  String sats = "0";

  if (GPS.location.isValid())
  {
    alt = String(GPS.altitude.meters(), 2);
    lat = String(GPS.location.lat(), 8);
    lon = String(GPS.location.lng(), 8);
    sats = String(GPS.satellites.value());
  }

  String result = time_data + "," + date_data + "," + alt + "," + lat + "," + lon + "," + sats;
  return result;
}

/**
 * Collect and format BMP280 sensor data (altitude, temperature, pressure)
 * @return Formatted BMP data string with comma-separated values
 */
String BMPData()
{
  return String(BMP.readAltitude(base_pressure)) + "," + String(BMP.readTemperature()) + "," + "nan," + String(BMP.readPressure() / 100.0F);
}

/**
 * Collect and format MPU6050 IMU data (gyroscope and accelerometer readings)
 * @return Formatted IMU data string with comma-separated values (gp,gr,gy,ap,ar,ay)
 */
String MPUData()
{
  MPU.getEvent(&acc, &gyr, &temp); // Read data from MPU6050

  float ap = acc.acceleration.x; // Acceleration X - pitch
  float ar = acc.acceleration.y; // Acceleration Y - roll
  float ay = acc.acceleration.z; // Acceleration Z - yaw

  float gp = gyr.gyro.x; // Gyroscope X - pitch
  float gr = gyr.gyro.y; // Gyroscope Y - roll
  float gy = gyr.gyro.z; // Gyroscope Z - yaw

  return String(gp) + "," + String(gr) + "," + String(gy) + "," + String(ap) + "," + String(ar) + "," + String(ay);
}

/**
 * Aggregate all sensor data into a single CSV-formatted string
 * @return Complete telemetry string combining BMP280, MPU6050, and GPS data
 */
String getDataString()
{
  return BMPData() + "," + MPUData() + "," + GPSData();
}
/* AVIONICS MODULE - END */

/* SERVER MODULE - START */

// Network access credentials
String ssid_str = "Server " + TEAM_ID;
const char *ssid = ssid_str.c_str();
const char *password = "Iamarobot";

// Create HTTP server listening on port 80
AsyncWebServer server(80);

/**
 * Configure web server routes for file serving and API endpoints
 */
void setServerRoutes()
{
  // Basic routes for website access
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/server/index.html", "text/html"); });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/server/style.css", "text/css"); });

  server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/server/index.js", "application/javascript"); });

  // API routes for frontend access to backend data
  // Not strictly RESTful due to library limitations and ESP32 constraints

  // Route to list all files in filesystem root with their metadata
  // Used by homepage to populate the file list table
  server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    JsonDocument fsFiles; // Create JSON object
    File root = LittleFS.open("/"); // Open filesystem root

    // Iterate through all files in directory
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        // Add file name and size to JSON object
        JsonObject fsFile = fsFiles.add<JsonObject>();
        fsFile["name"] = String(file.name());
        fsFile["size"] = file.size();
      }
      file = root.openNextFile();
    }
    // Convert JSON object to string and send to client
    char jsonString[8000] = { 0 };
    serializeJson(fsFiles, jsonString);
    request->send(200, "application/json; charset=utf-8", jsonString); });

  // Route to retrieve a specific file from filesystem root
  // Used by frontend for file viewing and downloading
  server.on("/api/file", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    // Return error if filename parameter is missing
    if (!(request->hasParam("filename"))) {
      request->send(400, "text/plain; charset=utf-8", "Missing URL parameter <filename>.");
      return;
    }

    // Return error if requested file doesn't exist in filesystem
    const AsyncWebParameter* param = request->getParam("filename");
    String filename = param->value();
    if (!LittleFS.exists("/" + filename)) {
      request->send(404, "text/plain; charset=utf-8", "File <" + filename + 
          "> not found in filesystem");
      return;
    }

    // Return file for download or simple viewing based on 'download' parameter
    if (request->hasParam("download")) {
      request->send(LittleFS, "/" + filename, String(), true);
    } else {
      request->send(LittleFS, "/" + filename, "text/plain; charset=utf-8");
    } });

  // Route to delete a specific file from filesystem
  // Similar to previous route but uses HTTP DELETE and removes file instead of returning it
  // Used by frontend file deletion button
  server.on("/api/file", HTTP_DELETE, [](AsyncWebServerRequest *request)
            {
    Serial.println(request->method());
    if (!(request->hasParam("filename"))) {
      request->send(400, "text/plain; charset=utf-8", "Missing URL parameter <filename>.");
      return;
    }

    const AsyncWebParameter* param = request->getParam("filename");
    String filename = param->value();

    if (!LittleFS.exists("/" + filename)) {
      request->send(404, "text/plain; charset=utf-8", "Cannot delete non-existent file <" + filename + ">.");
      return;
    }

    if (LittleFS.remove("/" + filename)) {
      request->send(200, "text/plain; charset=utf-8", "File deleted successfully.");
    } else {
      request->send(400, "text/plain; charset=utf-8", "Unknown error deleting file <" + filename + ">.");
    } });
}

/* SERVER MODULE - END */

void setup()
{
  Serial.begin(115200);
  Wire.begin();
  setupServo(); // Initialize servo motor

  pinMode(BUZZER_PIN, OUTPUT);
  for (int i = 0; i < 5; i++)
  {
    Serial.println("Initializing...");
    delay(1000);
  }

  Serial1.begin(9600, SERIAL_8N1, RX_GPS, TX_GPS); // Initialize GPS (must use Serial1)
  unsigned long start = millis();
  while (millis() - start < 3000) // Wait 3 seconds for GPS initialization
  {
    while (Serial1.available() > 0)
    {
      GPS.encode(Serial1.read());
    }
  }

  String time_data = ""; // Time string
  if (GPS.time.isValid())
  {
    time_data = String(GPS.time.hour()) + "_" + String(GPS.time.minute()) + "_" + String(GPS.time.second());
  }
  else
  {
    // If GPS has no valid time yet, use millis() as fallback
    time_data = "init_" + String(millis());
  }

  file_dir = "/" + time_data + "-" + file_name; // Data file directory
  Serial.print("Saving data to: ");
  Serial.println(file_dir);

  String data_header = "TEAM_ID,millis,count,altp,temp,umi,p,gp,gr,gy,ap,ar,ay,hora,data,alt,lat,lon,sat,pqd"; // File header
  if (!(setupLittleFS() && writeFile(file_dir, data_header)))                                                  // Initialize internal memory
  {
    Serial.println("Filesystem error!");
    buzzSignal("Alert");
    delay(3000);
    ESP.restart();
  }

  /* Server Block - START */

  // Create wireless access point
  WiFi.softAP(ssid, password);
  Serial.println("Creating WiFi access point...");
  Serial.println(WiFi.softAPIP());

  // Configure server routes and start server
  setServerRoutes();
  server.begin();

  /* Server Block - END */

  if (!(setupBMP() && setupMPU() && setupLoRa())) // Initialize BMP, MPU6050, and LoRa modules
  {
    printBoth("Module configuration error!");
    buzzSignal("Alert");
    delay(3000);
  }
  else
  {
    printBoth("All modules initialized successfully!");
    buzzSignal("Success");
  }
}

void loop()
{
  unsigned long current_millis = millis();
  if (current_millis - previous_millis >= INTERVAL) // Every 200ms
  {
    float altitude = BMP.readAltitude(base_pressure);
    float velocity = (altitude - previous_altitude) / ((current_millis - previous_millis) / 1000.0);
    logData(current_millis);
    checkHighest(altitude);
    handleParachute(altitude, velocity);
    previous_millis = current_millis;
  }
}
