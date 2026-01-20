# Documentação de Software - Computador de Bordo

## Visão Geral

O firmware principal do computador de bordo é executado em uma plataforma ESP32, gerenciando sensores, comunicação e controle do paraquedas durante o voo.

## Arquitetura

### Arquivo Principal

- **[firmware.ino](../firmware/firmware.ino)** - Código principal com setup e loop

### Organização do Código

```
setup()                 // Inicialização de todos os componentes
├── setupLittleFS()     // Sistema de arquivos
├── setupBMP()          // Sensor de pressão
├── setupMPU()          // IMU
├── setupLoRa()         // Comunicação
└── setupServo()        // Servo motor

loop()                  // Execução contínua
├── readSensors()       // Coleta de dados
├── handleParachute()   // Controle do paraquedas
├── logData()           // Armazenamento
└── sendLoRa()          // Transmissão
```

## Módulos e Sensores

### 1. BMP280 - Sensor de Pressão e Altitude

**Função**: Medir altitude e pressão atmosférica

**Bibliotecas**:

- `Adafruit_BMP280`
- `Wire` (I2C)

**Dados Coletados**:

- Altitude relativa (m)
- Pressão (hPa)
- Pressão na base (calibrada no setup)

**Código de Teste**: [test/basico/basico.ino](../test/basico/basico.ino)

**Variáveis Relacionadas**:

```cpp
float base_altitude    // Altitude inicial (referencial)
float base_pressure    // Pressão na base (hPa)
float previous_altitude // Altura anterior (para cálculo de velocidade)
float max_altitude     // Altura máxima alcançada
```

### 2. MPU6050 - IMU (Acelerômetro + Giroscópio)

**Função**: Medir aceleração e rotação (dados de dinâmica do voo)

**Bibliotecas**:

- `Adafruit_MPU6050`
- `Adafruit_Sensor`
- `Wire` (I2C)

**Dados Coletados**:

- Aceleração em X, Y, Z (m/s²)
- Velocidade angular em X, Y, Z (rad/s)
- Temperatura do sensor (°C)

**Variáveis Relacionadas**:

```cpp
sensors_event_t acc, gyr, temp  // Eventos de aceleração, giroscópio e temperatura
```

### 3. NEO-6M - Módulo GPS

**Função**: Obter latitude, longitude, altitude GPS e hora

**Bibliotecas**:

- `TinyGPS++`
- Comunicação serial UART

**Pinos de Comunicação**:

- **RX_GPS**: Pino 20 (recebe dados do GPS)
- **TX_GPS**: Pino 21 (transmite para o GPS)

**Dados Coletados**:

- Latitude (°)
- Longitude (°)
- Altitude GPS (m)
- Quantidade de satélites
- Data e hora UTC

**Características**:

- Aguarda 3 segundos no setup para sincronização
- Usa tempo GPS para nomear arquivo de log

### 4. RFM95W - Módulo LoRa

**Função**: Comunicação wireless de longo alcance com a base

**Frequência**: 868 MHz

**Bibliotecas**:

- `LoRa`
- `SPI`

**Pinos de Comunicação**:

- **SS_LORA**: Pino 7 (Chip Select)
- **RST_LORA**: Pino 1 (Reset)
- **DIO0_LORA**: Pino 2 (Interrupção)

**Configurações**:

```cpp
#define LORA_FREQ 868E6      // Frequência
#define SYNC_WORD 0xF3       // Código de sincronização
```

**Dados Transmitidos**: String concatenada com dados de todos os sensores em formato CSV

### 5. Servo Motor - Controle do Paraquedas

**Função**: Abrir o paraquedas em altitude apropriada

**Pino**: **SERVO_PIN = 3**

**Posições**:

- **MAXPOS = 0°** (Paraquedas fechado)
- **MINPOS = 90°** (Paraquedas aberto)

**Critérios de Abertura**:

```cpp
const float ALTITUDE_THRESHOLD = 750.0      // Altura mínima (m)
const float ALTITUDE_DROP_THRESHOLD = 10.0  // Queda mínima do pico (m)
const float VELOCITY_THRESHOLD = 80.0        // Velocidade de descida mínima (m/s)
```

**Função de Controle**: `handleParachute()`

### 6. Buzzer - Sinalização

**Função**: Indicar status de inicialização e operação

**Pino**: **BUZZER_PIN = 0**

**Sinais**:

- **Alerta**: Múltiplos bips curtos (falha na inicialização)
- **Sucesso**: Sequência de bips (componentes iniciados corretamente)

**Função de Controle**: `buzzSignal()`

## Sistema de Armazenamento

### LittleFS - Sistema de Arquivos

**Função**: Armazenar dados de telemetria em tempo real

**Formato de Arquivo**: CSV

**Nome do Arquivo**:

- Se GPS está sincronizado: `HH_MM_SS-Dados.csv` (hora do GPS)
- Se GPS não sincronizado: `{millis}-Dados.csv`

**Cabeçalho CSV**:

```csv
ID,Packet,Time,Latitude,Longitude,GPS_Altitude,Satellites,Date,Hours,Minutes,Seconds,
BMP_Altitude,Pressure,AccX,AccY,AccZ,GyroX,GyroY,GyroZ,Temp,ParachuteStatus
```

**Funções de Arquivo**:

- `setupLittleFS()` - Inicializa o sistema
- `writeFile()` - Cria novo arquivo com cabeçalho
- `appendFile()` - Adiciona linha ao arquivo

## Comunicação

### Serial UART

- **Baud Rate**: 115200
- **Uso**: Debug e monitoramento em tempo real

### LoRa

- **Alcance**: Até ~4 km (em campo aberto)
- **Taxa de Dados**: ~1-5 kbps
- **Frequência**: 868 MHz

## Interface Web

**Funcionalidade**: Servidor assíncrono na porta 80

**Endpoints**:

- `GET /` - Página inicial (HTML)
- `GET /api/files` - Lista arquivos em JSON
- `GET /api/file?name=` - Download de arquivo
- `DELETE /api/file?name=` - Deletar arquivo

**Bibliotecas**:

- `ESPAsyncWebServer`
- `WiFi`
- `ArduinoJson`

## Parâmetros Ajustáveis

```cpp
#define INTERVAL 200                              // Intervalo de leitura (ms)
const float ALTITUDE_THRESHOLD = 750.0            // Altura mínima para paraquedas (m)
const float ALTITUDE_DROP_THRESHOLD = 10.0        // Queda de referencial (m)
const float VELOCITY_THRESHOLD = 80.0              // Velocidade mínima descida (m/s)
const String TEAM_ID = "#100"                     // ID da equipe
```

## Fluxo de Execução

1. **Inicialização** (setup)
   - Configura comunicação serial
   - Inicializa I2C e SPI
   - Aguarda sincronização GPS (3s)
   - Cria arquivo de log
   - Inicializa sensores e módulos
   - Inicia servidor web

2. **Loop Principal**
   - Verifica intervalo de 200ms
   - Lê altitude e IMU
   - Calcula velocidade de descida
   - Verifica critério de abertura do paraquedas
   - Registra dados em arquivo
   - Transmite via LoRa

3. **Controle de Paraquedas**
   - Aguarda altura mínima
   - Monitora queda em relação ao pico
   - Verifica velocidade de descida
   - Abre servo quando todos os critérios são atendidos

## Códigos de Suporte

Os arquivos em [extras/](../extras/) contêm código funcional e testado para referência:

- **[extras/ino_files/](../extras/ino_files/)** - Versões anteriores do código integrado
- **[extras/FileBrowser/FileBrowser.ino](../extras/FileBrowser/FileBrowser.ino)** - Navegador de arquivos, base para o servidor assíncrono
- **[extras/LoraReceiver/LoraReceiver.ino](../extras/LoraReceiver/LoraReceiver.ino)** - Receptor LoRa para base
- **[extras/Serial/Serial.py](../extras/Serial/Serial.py)** - Script Python para monitoramento serial

## Testes

Os testes unitários estão em [test/](../test/):

- **[test/basico/basico.ino](../test/basico/basico.ino)** - Teste básico de inicialização
- **[test/buzzer/buzzer.ino](../test/buzzer/buzzer.ino)** - Teste do buzzer
- **[test/lora/lora.ino](../test/lora/lora.ino)** - Teste da comunicação LoRa
- **[test/testeGPS/testeGPS.ino](../test/testeGPS/testeGPS.ino)** - Teste do módulo GPS
- **[test/servo/servo.ino](../test/servo/servo.ino)** - Teste do servo motor
- **[test/LittleFS/LittleFS.ino](../test/LittleFS/LittleFS.ino)** - Teste do sistema de arquivos

## Dependências - Bibliotecas Arduino

| Biblioteca        | Versão | Uso                |
| ----------------- | ------ | ------------------ |
| Adafruit BMP280   | Latest | Sensor de pressão  |
| Adafruit MPU6050  | Latest | IMU                |
| Adafruit Sensor   | Latest | Base para sensores |
| TinyGPS++         | Latest | Decodificação GPS  |
| LoRa              | Latest | Módulo LoRa        |
| ESP32Servo        | Latest | Controle de servo  |
| ArduinoJson       | ^6.0   | Serialização JSON  |
| ESPAsyncWebServer | Latest | Servidor web       |

## Notas de Desenvolvimento

- **Sincronização**: O sistema aguarda sincronização do GPS antes de iniciar o voo
- **Redundância**: O paraquedas utiliza múltiplos critérios para evitar abertura incorreta
- **Logging**: Todos os dados são armazenados localmente antes da transmissão via LoRa
- **Power Efficiency**: O ESP32 opera em modo contínuo durante o voo
