// INCLUSÃO DE BIBLIOTECAS
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <Servo.h>

long int cont = 0;

unsigned long previousmillis = 0;

const long intervalo = 100; // Precisao Leitura Dados milissegundos

String dir = "";
String filedir = "";

int BUZZER = 0;

Adafruit_BMP280 bmp;

int CS = 15;

float altantes = 0;

int maxPos = 180;
int minPos = 0;
int servoPin = 16;

Servo myservo;

void writeFile(String path, String dataString)
{
  File dataFile = SD.open(path, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUZZER, OUTPUT);
  myservo.attach(servoPin);

  myservo.write(minPos);

  if (!SD.begin(CS))
  {
    while (1) {
      Serial.println("Falha no SDcard");
    }
  }

  if (!bmp.begin(0x76))
  {
    while (1)
    {
      Serial.println("Nao foi possivel encontrar um sensor BMP280 valido, verifique a conexao!");
    }
  }

  altantes = bmp.readAltitude(1013.25);

  String mensagemSetup = "\n";

  Serial.println("Escrevendo Data no Arquivo Dados.txt"); // Imprime na tela
  dir = "/";
  filedir = "";
  filedir = dir + "Dados.txt";
  writeFile(filedir, " ");

  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(BUZZER, LOW);
  delay(500);
  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(BUZZER, LOW);
  delay(500);
  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(BUZZER, LOW);
}

void loop()
{
  unsigned long currentmillis = millis();
  String leitura = "";

  if (currentmillis - previousmillis >= intervalo)
  {
    float altitude = bmp.readAltitude(1013.25);
    float temp = bmp.readTemperature();
    float pressure = bmp.readPressure();
    Serial.print(altitude, 3);
    leitura.concat(String(altitude, 3));
    Serial.print(";");
    leitura.concat(";");
    Serial.print(pressure, 3);
    leitura.concat(String(pressure, 3));
    Serial.print(";");
    leitura.concat(";");
    Serial.print(temp, 3);
    leitura.concat(String(temp, 3));
    Serial.print(";");
    leitura.concat(";");
    Serial.println(cont);
    leitura.concat(cont);
    writeFile(filedir, leitura);
    
    float derivada = (altitude - altantes) / (currentmillis - previousmillis);
    
    if (derivada <= -0.01)
    {
      digitalWrite(BUZZER, HIGH);
      myservo.write(maxPos);
    }

    altantes = altitude;
    cont = cont + 100;
    previousmillis = currentmillis;
  }
}