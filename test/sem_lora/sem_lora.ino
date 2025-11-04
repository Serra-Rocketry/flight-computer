// Inclusão de bibliotecas
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <ESP32Servo.h>
#include <TinyGPS++.h>
#include "FS.h"
#include "SPIFFS.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// Definições de pinos e constantes
#define INTERVAL 200

#define BMP_ADDR 0x76 // Endereço I2C padrão do BMP280

#define SERVO_PIN 13 // Pino do servo motor
#define BUZZER_PIN 0 // Pino do buzzer

#define RX_GPS 20 // RX do GPS
#define TX_GPS 21 // TX do GPS

// Instanciação de objetos
Adafruit_BMP280 BMP;  // Objeto do BMP280
Servo ParachuteServo; // Objeto do servo
TinyGPSPlus GPS;      // Objeto do GPS
Adafruit_MPU6050 MPU; // Objeto do MPU6050

// Variáveis globais
unsigned long previous_millis = 0;
float previous_altitude = 0, max_altitude = 0, base_altitude = 0; // Altitudes variáveis e estáticas
float base_pressure = 0;                                          // Pressão na base
String file_name = "Dados.txt";                                   // Nome do arquivo para salvar os dados
String file_dir = "";
bool parachute_deployed = false;            // Verificação da liberação do paraquedas
const int MAXPOS = 180, MINPOS = 0;         // Posição máxima e mínima do servo
const float ALTITUDE_DROP_THRESHOLD = 10.0; // Ao menos 10m abaixo do referencial máximo (ajustar se necessário)
const float ALTITUDE_THRESHOLD = 100.0;     // Altura mínima para liberar o paraquedas (ajustar se necessário)
sensors_event_t acc, gyr, temp;             // Variáveis para armazenar os dados do MPU6050

// Setup da memória SPIFFS
bool setupSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("Erro ao montar SPIFFS.");
    return false;
  }
  return true; // Retorna true se tudo ocorreu bem
}

// Setup do módulo BMP280
bool setupBMP()
{
  if (!BMP.begin(BMP_ADDR))
  {
    Serial.println("Falha no BMP280.");
    return false;
  }
  base_pressure = BMP.readPressure() / 100;
  previous_altitude = BMP.readAltitude(base_pressure);
  max_altitude = previous_altitude;
  base_altitude = previous_altitude;
  return true; // Retorna true se tudo ocorreu bem
}

// Setup do MPU6050
bool setupMPU()
{
  if (!MPU.begin())
  {
    Serial.println("Falha no MPU6050.");
    return false;
  }
  return true; // Retorna true se tudo ocorreu bem
}

// Setup do servo motor
void setupServo()
{
  ParachuteServo.attach(SERVO_PIN);
  ParachuteServo.write(MINPOS);
}

// Sinalização com o buzzer
void buzzSignal(String signal)
{
  if (signal == "Alerta")
  {
    for (int i = 0; i < 5; i++)
    {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(500);
      digitalWrite(BUZZER_PIN, LOW);
      delay(500);
    }
  }
  else if (signal == "Sucesso")
  {
    for (int i = 0; i < 3; i++)
    {
      digitalWrite(BUZZER_PIN, HIGH);
      delay(100);
      digitalWrite(BUZZER_PIN, LOW);
      delay(100);
    }
  }
  else if (signal == "Ativado")
  {
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else if (signal == "Funcionando")
  {
    bool State = !digitalRead(BUZZER_PIN);
    digitalWrite(BUZZER_PIN, State);
  }
  else
  {
    Serial.println("Sinal inválido!");
  }
}

// Registra e imprime os dados do momento
// Formato da string: tempo;lat-long-satélites-altitude-data-hora;altitude-pressão;acelX-acelY-acelZ-giroX-giroY-giroZ
void logData(unsigned long current_millis)
{
  String data_string = String(current_millis) + ";" + getDataString(); // String com os dados atuais
  printBoth(data_string);
  appendFile(file_dir, data_string);
}

// Verifica se a altura atual é a máxima já atingida
void checkHighest(float altitude)
{
  if (altitude > max_altitude)
  {
    max_altitude = altitude;
  }
}

// Lida com a abertura do paraquedas
void handleParachute(float altitude)
{
  if (!parachute_deployed)
  {
    if (altitude <= max_altitude - ALTITUDE_DROP_THRESHOLD && altitude < ALTITUDE_THRESHOLD)
    {
      buzzSignal("Ativado");
      ParachuteServo.write(MAXPOS);
      unsigned long startTime = millis();
      while (millis() - startTime < 500)
      {
        if (ParachuteServo.read() == MAXPOS)
        {
          break;
        }
      }
      if (ParachuteServo.read() != MAXPOS)
      {
        printBoth("ERRO: Servo não abriu!");
      }
      printBoth("Paraquedas acionado. Altitude: " + String(altitude));
      parachute_deployed = true;
    }
  }
  previous_altitude = altitude;
}

// Escreve os dados no arquivo - escrita
bool writeFile(const String &path, const String &data_string)
{
  File file = SPIFFS.open(path, FILE_APPEND);
  if (!file) // Se houver falha ao abrir o arquivo
  {
    Serial.println("Falha ao abrir arquivo para gravação.");
    return false;
  }
  if (file.println(data_string)) // Se a escrita no arquivo for bem-sucedida
  {
    Serial.println("Arquivo escrito.");
  }
  else // Se houver falha na escrita
  {
    Serial.println("Falha na gravação do arquivo.");
    file.close();
    return false;
  }
  file.close();
  return true; // Retorna true se tudo ocorreu bem
}

// Escreve os dados no arquivo - anexação
void appendFile(const String &path, const String &message)
{
  File file = SPIFFS.open(path, FILE_APPEND);
  if (!file) // Se houver falha ao abrir o arquivo
  {
    Serial.println("Falha ao abrir arquivo para anexar.");
  }
  if (file.print(message + "\n")) // Se a escrita no arquivo for bem-sucedida
  {
    Serial.println("Mensagem anexada.");
  }
  else // Se houver falha na escrita
  {
    Serial.println("Falha ao anexar mensagem.");
    file.close();
  }
  file.close();
}

// Imprime a mensagem no Serial
void printBoth(const String &message)
{
  Serial.println(message);
}

// Retorna os dados de latitude-longitude-satélites-altitude-data-hora
String GPSData()
{
  while (Serial1.available() > 0)
  {
    GPS.encode(Serial1.read());
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

// Retorna os dados de altitude-pressão
String BMPData()
{
  return String(BMP.readAltitude(base_pressure)) + "-" +
         String(BMP.readPressure() / 100.0F);
}

// Retorna os dados de acelerômetro-giroscópio
String MPUData()
{
  MPU.getEvent(&acc, &gyr, &temp); // Lê os dados do MPU6050

  float ax = acc.acceleration.x; // Aceleração em x
  float ay = acc.acceleration.y; // Aceleração em y
  float az = acc.acceleration.z; // Aceleração em z

  float gx = gyr.gyro.x; // Giroscópio em x
  float gy = gyr.gyro.y; // Giroscópio em y
  float gz = gyr.gyro.z; // Giroscópio em z

  return String(ax) + "-" +
         String(ay) + "-" +
         String(az) + "-" +
         String(gx) + "-" +
         String(gy) + "-" +
         String(gz);
}

// Retorna a string com os dados do GPS, BMP280 e MPU6050
String getDataString()
{
  return GPSData() + ";" + BMPData() + ";" + MPUData();
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial1.begin(9600, SERIAL_8N1, RX_GPS, TX_GPS); // Inicia o GPS
  if (!Serial1)
  {
    Serial.println("Falha ao iniciar o GPS.");
    ESP.restart();
  }
  delay(1000); // Aguarda o GPS inicializar

  while (true)
  {
    while (Serial1.available() > 0)
    {
      GPS.encode(Serial1.read());
    }
    Serial.println("Aguardando GPS...");
    if (GPS.location.isValid())
    {
      break;
    }
    delay(500);
  }

  String time_data = ""; // String do horário
  time_data = String(GPS.time.hour()) + ":" +
              String(GPS.time.minute()) + ":" +
              String(GPS.time.second());

  file_dir = "/" + time_data + "-" + file_name; // Diretório do arquivo de dados
  Serial.print("Salvando dados em: ");
  Serial.println(file_dir);
  if (!(setupSPIFFS() && setupBMP() && setupMPU() && writeFile(file_dir, ""))) // Inicia a memória interna e os módulos BMP e MPU6050
  {
    Serial.println("Erro na configuração!");
    buzzSignal("Alerta");
    delay(3000);
    ESP.restart();
  }

  setupServo(); // Inicia o servo motor

  buzzSignal("Sucesso");
}

void loop()
{
  unsigned long current_millis = millis();
  if (current_millis - previous_millis >= INTERVAL) // A cada 100ms
  {
    logData(current_millis);
    float altitude = BMP.readAltitude(base_pressure);
    checkHighest(altitude);
    handleParachute(altitude);
    previous_millis = current_millis;
  }
}
