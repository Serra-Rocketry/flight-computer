#ifndef GPS_MODULE_OOP_H
#define GPS_MODULE_OOP_H

#include "ISensor.h"
#include <TinyGPS++.h>
#include <Arduino.h>

/**
 * @brief GPS receiver wrapper implementing ISensor interface.
 *
 * Feeds TinyGPS++ parser non-blockingly in update().
 * Expected call: TelemetryTask at 5 Hz.
 *
 * Pins configured in config.h (RX_GPS / TX_GPS), baud 9600.
 */
class GPSModule : public ISensor {
public:
  GPSModule(HardwareSerial* serial);

  // Interface ISensor
  bool begin() override;
  void update() override;
  String getData() override;
  bool isReady() override;

  // Específicos do GPS
  String getTimeString();
  String getDateString();
  bool hasValidFix();

  // Accessors para preencher SensorData
  double getLatitude()   { return _gps.location.isValid() ? _gps.location.lat() : 0.0; }
  double getLongitude()  { return _gps.location.isValid() ? _gps.location.lng() : 0.0; }
  float  getGPSAltitude() { return _gps.location.isValid() ? (float)_gps.altitude.meters() : 0.0f; }
  uint8_t getSatellites() { return (uint8_t)_gps.satellites.value(); }

private:
  TinyGPSPlus _gps;
  HardwareSerial* _serial;
  bool _ready;
};

#endif // GPS_MODULE_OOP_H
