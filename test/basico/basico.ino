#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <TinyGPS++.h>

// Definições de pinos e constantes
#define INTERVAL 100

#define BMP_ADDR 0x76 // Endereço I2C padrão do BMP280
#define RX_GPS 20     // RX do GPS
#define TX_GPS 21     // TX do GPS

// Instanciação de objetos
Adafruit_BMP280 BMP;                      // Objeto do BMP280
Adafruit_MPU6050 MPU;                     // Objeto do MPU6050
TinyGPSPlus GPS;                          // Objeto do GPS

// Variáveis globais
unsigned long previous_millis = 0;
float previous_altitude = 0, max_altitude = 0, base_altitude = 0; // Altitudes variáveis e estáticas
float base_pressure = 0;                                          // Pressão na base
sensors_event_t acc, gyr, temp;                                   // Variáveis para armazenar os dados do MPU6050
bool parachute_deployed = false;                                  // Verificação da liberação do paraquedas
const float ALTITUDE_DROP_THRESHOLD = 10.0;                       // Ao menos 10m abaixo do referencial máximo (ajustar se necessário)
const float ALTITUDE_THRESHOLD = 100.0;                           // Altura mínima para liberar o paraquedas (ajustar se necessário)

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

// Registra e imprime os dados do momento
// Formato da string: tempo;lat-long-satélites-altitude-data-hora;altitude-pressão;acelX-acelY-acelZ-giroX-giroY-giroZ
void logData(unsigned long current_millis)
{
    String data_string = String(current_millis) + ";" + getDataString(); // String com os dados atuais
    Serial.println(data_string);                                         // Imprime os dados no Serial Monitor
    Serial.println("==========");
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
void handleParachute(float altitude, unsigned long current_millis)
{
    if (!parachute_deployed)
    {
        if (altitude <= max_altitude - ALTITUDE_DROP_THRESHOLD && altitude < ALTITUDE_THRESHOLD)
        {
            Serial.println("Paraquedas liberado!");
            parachute_deployed = true;
        }
    }
    previous_altitude = altitude;
}

void setup()
{
    Serial.begin(115200);
    // Diretório do arquivo de dados
    if (!(setupBMP() && setupMPU())) // Inicia a memória interna e os módulos BMP, LoRa e MPU6050
    {
        Serial.println("Erro na configuração!");
        delay(3000);
        ESP.restart();
    }
    Serial1.begin(9600, SERIAL_8N1, RX_GPS, TX_GPS);
    delay(5000);           // Aguarda o GPS estabilizar
}

void loop()
{
    unsigned long current_millis = millis();
    if (current_millis - previous_millis >= INTERVAL) // A cada 100ms
    {
        logData(current_millis);
        float altitude = BMP.readAltitude(base_pressure);
        checkHighest(altitude);
        handleParachute(altitude, current_millis);
        previous_millis = current_millis;
    }
}
