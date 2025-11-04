// Testado em um Arduino Uno com GPS Neo-6M

#define GPS_RX 4
#define GPS_TX 3
#define GPS_Serial_Baud 9600
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
TinyGPSPlus GPS;
// The serial connection to the GPS device
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);

String GPSData()
{
    while (gpsSerial.available() > 0) {
      GPS.encode(gpsSerial.read());
    }
    String location_data = "N/A-N/A-N/A-N/A-N/A"; // String da localização lat-long-satélites-altitude
    if (GPS.location.isValid())
    {
        location_data = String(GPS.location.lat(), 6) + "-" +
                        String(GPS.location.lng(), 6) + "-" +
                        String(GPS.satellites.value()) + "-" +
                        String(GPS.altitude.meters());
    }

    String date_data = "-N/A"; // String da data
    if (GPS.date.isValid())
    {
        date_data = "-" + String(GPS.date.year()) + "/" +
                    String(GPS.date.month()) + "/" +
                    String(GPS.date.day());
    }

    String time_data = "-N/A"; // String do horário
    if (GPS.time.isValid())
    {
        time_data = "-" + String(GPS.time.hour()) + ":" +
                    String(GPS.time.minute()) + ":" +
                    String(GPS.time.second());
    }

    return location_data + date_data + time_data;
}
void setup()
{
  Serial.begin(115200); // Beginning the serial monitor at Baudrate 115200 and make sure you select same in serial monitor
  gpsSerial.begin(GPS_Serial_Baud);
}
void loop()
{
  Serial.println(GPSData());
  Serial.println("=================");
}
