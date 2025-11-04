# Aviônica - Computador de Bordo

Este projeto é o computador de bordo (CDB) para o foguete SR1500 da equipe de foguetemodelismo Serra Rocketry. O CDB utiliza sensores e módulos para monitorar a altitude, localização GPS, e controlar a abertura do paraquedas para garantir um voo seguro.

## Funcionalidades

- Leitura de altitude utilizando o sensor BMP280.
- Monitoramento de localização GPS com o módulo GPS NEO-6m.
- Comunicação via LoRa para transmissão de dados com a base operacional.
- Controle de um servo motor para abertura do paraquedas.
- Armazenamento de dados em LittleFS.
- Monitoramento de aceleração e giroscópio utilizando o sensor MPU6050.
- Registro de dados em formato CSV para análise posterior.
- Sinalização com buzzer para indicar status de operação.

## Hardware Utilizado

- ESP32
- Sensor de pressão BMP280
- Módulo GPS NEO-6m
- Módulo LoRa
- Servo motor
- Buzzer
- LED
- Step down
- Baterias 18650
- Sensor MPU6050

## Configuração de Pinos

- **BUZZER_PIN**: 0
- **SERVO_PIN**: 13
- **RX_GPS**: 20
- **TX_GPS**: 21
- **SS_LORA**: 7
- **RST_LORA**: 1
- **DIO0_LORA**: 2

## Bibliotecas Utilizadas

- **Wire**
- **SPI**
- **Adafruit_BMP280**
- **ESP32Servo**
- **TinyGPS++**
- **FS**
- **LittleFS**
- **LoRa**
- **Adafruit_MPU6050**
- **Adafruit_Sensor**

## Estrutura do Código

- **setup()**: Configura os sensores, módulos e inicializa as variáveis.
- **loop()**: Realiza leituras periódicas dos sensores, verifica a altitude e controla a abertura do paraquedas.
- **setupLittleFS()**: Inicializa o sistema de arquivos LittleFS.
- **setupBMP()**: Configura o sensor BMP280.
- **setupLoRa()**: Inicializa o módulo LoRa.
- **setupMPU()**: Configura o sensor MPU6050.
- **setupServo()**: Configura o servo motor.
- **buzzSignal()**: Controla o buzzer para sinalização.
- **logData()**: Registra e imprime os dados do momento.
- **handleParachute()**: Controla a abertura do paraquedas com base na altitude.
- **checkHighest()**: Verifica a maior altitude alcançada.
- **writeFile()**: Escreve dados no arquivo LittleFS.
- **appendFile()**: Anexa dados ao arquivo LittleFS.
- **printBoth()**: Imprime mensagens no Serial e envia via LoRa.
- **sendLoRa()**: Envia mensagens via LoRa.
- **GPSData()**: Obtém dados do GPS (latitude, longitude, satélites, altitude, data e hora).
- **BMPData()**: Obtém dados do sensor BMP280 (altitude e pressão).
- **MPUData()**: Obtém dados do sensor MPU6050 (aceleração e giroscópio).
- **getDataString()**: Concatena os dados do GPS, BMP280, MPU6050 e status do paraquedas em uma string.

## Códigos de Apoio

Os códigos contidos em `/extras` são apoios ao desenvolvimento. Esses códigos são funcionais e testados.
Os códigos contidos em `/test`são apoios ao desenvolvimento, como etapas para construção do código final. 