#include <TinyGPS++.h>

#define RXD2 16
#define TXD2 17
HardwareSerial neogps(1);

TinyGPSPlus gps;

#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

int LED = 2;

void setup() {
  Serial.begin(115200);
  //Begin serial communication Arduino IDE (Serial Monitor)

  //Begin serial communication Neo6mGPS
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);

  SerialBT.begin("ESP32"); //Bluetooth device name

  pinMode (LED, OUTPUT);
}

void loop() {

  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (neogps.available())
    {
      if (gps.encode(neogps.read()))
      {
        newData = true;
      }
    }
  }

  //If newData is true
  if (newData == true)
  {
    newData = false;
    print_speed();
  }
  else
  {
    Serial.println("No Data");
    SerialBT.println();
    SerialBT.println("No Data");
    SerialBT.println();
  }
}

void print_speed()
{
  if (gps.location.isValid() == 1)
  {
    digitalWrite (LED, HIGH);
    delay(1000);
    digitalWrite (LED, LOW);
    Serial.print("Lat: ");
    Serial.println(gps.location.lat(), 6);

    SerialBT.print(gps.location.lat(), 6);

    Serial.print("Lng: ");
    Serial.println(gps.location.lng(), 6);

    SerialBT.print(",");
    SerialBT.println(gps.location.lng(), 6);

    Serial.print("SAT:");
    Serial.println(gps.satellites.value());

    Serial.print("Data: ");
    Serial.print(gps.date.day());//LEITURA DO DIA
    Serial.print("/");
    Serial.print(gps.date.month());//LEITURA DO MêS
    Serial.print("/");
    Serial.println(gps.date.year());//LEITURA DO ANO


    Serial.print("Time: "); //LEITURA DA HORA PARA SER IMPRESSA NA SERIAL
    if (gps.time.hour() < 10) Serial.print(F("0"));
    Serial.print(gps.time.hour() - 3); //AJUSTA O FUSO HORARIO PARA NOSSA REGIAO (FUSO DE SP 03:00, POR ISSO O -3 NO CÓDIGO) E IMPRIME NA SERIAL
    Serial.print(":");
    if (gps.time.minute() < 10) Serial.print(F("0"));
    Serial.print(gps.time.minute());//IMPRIME A INFORMAÇÃO DOS MINUTOS NA SERIAL


    Serial.println();
    Serial.print("http://maps.google.com/maps?q=");
    Serial.print(gps.location.lat(), 6);
    Serial.print(",");
    Serial.print(gps.location.lng(), 6);
    Serial.println();
    Serial.println();
    
    delay(10000);
  }
  else
  {
    Serial.println("No Data2");
    SerialBT.println();
    SerialBT.println("No Data2");
    SerialBT.println();
  }
}
