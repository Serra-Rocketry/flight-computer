/**
 * @file GPSModule.cpp
 * @brief Implementation of GPS receiver wrapper
 * 
 * @see GPSModule.h for class definition
 * @see firmware/REFACTORING_PLAN.md Fase 5
 */

#include "sensors/GPSModule.h"
#include "config.h"

/**
 * @brief Constructor with HardwareSerial dependency injection
 * @param serial Pointer to HardwareSerial instance (e.g., &Serial1)
 */
GPSModule::GPSModule(HardwareSerial* serial) : _serial(serial), _ready(false) {}

/**
 * @brief Initializes GPS serial communication
 * 
 * @return true always (non-blocking, Serial.begin never fails)
 * @note Uses 9600 baud (default for NEO-8M), 8N1 format
 */
bool GPSModule::begin() {
  _serial->begin(9600, SERIAL_8N1, RX_GPS, TX_GPS);
  _ready = true;
  return true;
}

/**
 * @brief Feeds GPS parser with available NMEA data
 * 
 * Non-blocking: processes all available characters without waiting.
 * Called by TelemetryTask at 5Hz (slower than flight-critical sensors).
 * 
 * @return void
 * @note CRITICAL: Must be called regularly to prevent buffer overflow
 */
void GPSModule::update() {
  if (!_ready) return;
  while (_serial->available() > 0) {
    _gps.encode(_serial->read());
  }
}

String GPSModule::getData() {
  return getTimeString() + "," + getDateString() + "," +
         (hasValidFix() ? String(_gps.altitude.meters(), 2) : "nan") + "," +
         (hasValidFix() ? String(_gps.location.lat(), 8)   : "nan") + "," +
         (hasValidFix() ? String(_gps.location.lng(), 8)   : "nan") + "," +
         String(_gps.satellites.value());
}

bool GPSModule::isReady() { return _ready; }

String GPSModule::getTimeString() {
  if (!_gps.time.isValid()) return "nan";
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", _gps.time.hour(), _gps.time.minute(), _gps.time.second());
  return String(buf);
}

String GPSModule::getDateString() {
  if (!_gps.date.isValid()) return "nan";
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d", _gps.date.year(), _gps.date.month(), _gps.date.day());
  return String(buf);
}

bool GPSModule::hasValidFix() {
  return _gps.location.isValid();
}
