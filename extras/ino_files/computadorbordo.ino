#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <Servo.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// Defina o Token do Blynk, SSID e senha Wi-Fi
char auth[] = "SEU_AUTH_TOKEN";  // Substitua com seu token Blynk (enviado por email)
char ssid[] = "SEU_SSID";        // Nome da sua rede Wi-Fi (ou dados móveis)
char pass[] = "SUA_SENHA";       // Senha da sua rede Wi-Fi (ou dados móveis)

// Configuração do GPS
TinyGPSPlus gps;
SoftwareSerial ss(D6, D5);  // RX para D6, TX para D5 no Wemos D1 Mini

// Configuração do pino virtual para o widget de mapa
#define GPS_MAP_VPIN V1  // Pino virtual para o widget de mapa no Blynk

// Variáveis do sistema de armazenamento e sensores
long int cont = 0;
unsigned long previousmillis = 0;
const long intervalo = 100; // Precisão Leitura Dados milissegundos
String dir = "";
String filedir = "";
int CS = 15;
int BUZZER = 0;
Adafruit_BMP280 bmp;
int maxPos = 180;
int minPos = 0;
int servoPin = 16;
Servo myservo;
float altantes = 0;

// Função para escrever dados no cartão SD
void writeFile(String path, String dataString) {
  File dataFile = SD.open(path, FILE_WRITE);
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
  } else {
    Serial.println("Erro ao abrir o arquivo no cartão SD");
  }
}

void setup() {
  // Inicializa a comunicação serial
  Serial.begin(115200);
  ss.begin(9600); // Inicializa o GPS
  Blynk.begin(auth, ssid, pass);  // Conecta ao Blynk

  pinMode(BUZZER, OUTPUT);
  myservo.attach(servoPin);
  myservo.write(minPos);

  // Inicializa o cartão SD
  if (!SD.begin(CS)) {
    while (1) {
      Serial.println("Falha no SDcard");
    }
  }

  // Inicializa o sensor BMP280
  if (!bmp.begin(0x76)) {
    while (1) {
      Serial.println("Não foi possível encontrar um sensor BMP280 válido, verifique a conexão!");
    }
  }

  // Inicializa o arquivo no cartão SD
  dir = "/";
  filedir = dir + "Dados.txt";
  writeFile(filedir, " ");  // Escreve um cabeçalho ou espaço inicial

  // Sinal de inicialização com o buzzer
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
  delay(500);
}

void loop() {
  unsigned long currentmillis = millis();
  String leitura = "";

  // Processa dados do GPS
  while (ss.available() > 0) {
    if (gps.encode(ss.read())) {
      if (gps.location.isValid()) {
        float latitude = gps.location.lat();
        float longitude = gps.location.lng();

        // Envia as coordenadas para o widget de mapa do Blynk
        Blynk.virtualWrite(GPS_MAP_VPIN, latitude, longitude, "Foguete");

        // Exibe as coordenadas no monitor serial (opcional)
        Serial.print("Latitude: ");
        Serial.print(latitude, 6);
        Serial.print(" Longitude: ");
        Serial.println(longitude, 6);
      }
    }
  }

  // Leitura dos dados do BMP280 e armazenamento no SD
  if (currentmillis - previousmillis >= intervalo) {
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
    writeFile(filedir, leitura);  // Armazena os dados no cartão SD

    // Cálculo da derivada da altitude
    float derivada = (altitude - altantes) / (currentmillis - previousmillis);
    if (derivada <= -0.01) {  // Sistema de segurança baseado na altitude
      digitalWrite(BUZZER, HIGH);
      myservo.write(maxPos);
    }

    altantes = altitude;
    cont += 100;
    previousmillis = currentmillis;
  }

  // Executa o código Blynk
  Blynk.run();
}
