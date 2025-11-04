#include <ESP32Servo.h>

#define SERVO_PIN 10 // Pino do servo motor

const int MAXPOS = 180, MINPOS = 0;

Servo ParachuteServo; // Objeto do servo

void setupServo()
{
  ParachuteServo.attach(SERVO_PIN);
  ParachuteServo.write(MINPOS);
}

void setup()
{
  Serial.begin(115200);
  setupServo();
  ParachuteServo.write(MINPOS);
  delay(5000);
}

void loop()
{
  ParachuteServo.write(MAXPOS);
  unsigned long startTime = millis();
  while (millis() - startTime < 500) // Aguarda 500ms para conferir se o servo motor abriu
  {
    if (ParachuteServo.read() == MAXPOS)
    {
      break;
    }
  }
  if (ParachuteServo.read() != MAXPOS)
  {
  }
  delay(1000);
}
