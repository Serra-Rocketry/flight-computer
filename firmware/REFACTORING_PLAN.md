# 🚀 Plano de Refatoração: POO + FreeRTOS + FSM

**Projeto:** Flight Computer - Team #100  
**Hardware:** ESP32-S3-DevKitC-1-N8R8 (8MB Flash, 512KB RAM)  
**Data Início:** 2026-03-18  
**Status:** 📋 Planejamento Completo - Aguardando Implementação

---

## 📊 Resumo Executivo

### Objetivos
1. ✅ Refatorar código procedural para **POO seletivo** (sensores apenas)
2. ✅ Implementar **FSM** para controle de estados de voo
3. ✅ Usar **FreeRTOS** para separar lógica crítica (FSM) de I/O (telemetria)
4. ✅ Substituir sensores: **BMP280→BMP585**, **MPU6050→LSM6DS3**, **GPS N6M→N8M**
5. ✅ Preparar arquitetura para modificações futuras

### Motivação
- Código monolítico (584 linhas) chegou ao **EOL**
- Troca de sensores planejada requer abstração
- FSM necessária para controle de voo robusto
- Separação safety-critical (paraquedas) de I/O (logging)

---

## 🎯 Decisões Arquiteturais

| Aspecto | Decisão | Justificativa |
|---------|---------|---------------|
| **Abordagem POO** | Seletivo | Apenas sensores que mudam viram classes |
| **FSM** | Enum/Switch | Simples, direto, baixo overhead |
| **FreeRTOS** | 3 Tasks | Separação FSM (50Hz) / Telemetry (5Hz) / Logger |
| **Migração** | Big Bang | Refatoração completa de uma vez |
| **Testes** | Manuais/Hardware | Sem infraestrutura de unit tests |
| **Queue Size** | 25 (sensores), 50 (logs) | Cobre pior caso com margem |
| **Watchdog** | 5s, apenas Task FSM | Reset automático se travamento crítico |
| **Logging** | Híbrido (Queue + #ifdef) | Debug verboso em dev, seletivo em produção |

---

## 🏗️ Arquitetura Final

### Estrutura de Diretórios

```
firmware/
│
├── firmware.ino                    # Main - Setup + Tasks creation
│
├── config.h                        # Global configs + FSM thresholds
│
├── sensors/                        # 🆕 POO - Sensor abstractions
│   ├── ISensor.h                   # Abstract base interface
│   ├── BMP585Sensor.h/cpp          # Barometer (altitude, pressure, temp)
│   ├── LSM6DS3Sensor.h/cpp         # IMU (accelerometer + gyroscope)
│   └── GPSModule.h/cpp             # GPS positioning and time
│
├── flight/                         # 🆕 FSM + FreeRTOS Tasks
│   ├── SensorData.h                # Shared structs (SensorData, LogMessage)
│   ├── FlightStateMachine.h/cpp    # FSM - 7 states + transitions (VALIDADO)
│   ├── FlightControlTask.h/cpp     # Task 1 - FSM + Safety (50Hz, Core 1)
│   ├── TelemetryTask.h/cpp         # Task 2 - Logging + TX (5Hz, Core 0)
│   └── LoggerTask.h/cpp            # Task 3 - Debug logger (low priority)
│
└── modules/                        # Procedural modules (unchanged)
    ├── buzzer_module.h             # Audio feedback
    ├── filesystem_module.h         # LittleFS operations
    ├── lora_module.h               # LoRa transmission
    ├── parachute_module.h          # Servo + deployment logic
    └── server_module.h             # WiFi AP + Web interface
```

### Diagrama de Tasks FreeRTOS

```
┌────────────────────────────────────────────────────────────┐
│               ESP32-S3 Dual Core Architecture              │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  CORE 1 (pinned)             CORE 0 (shared)              │
│  ┌──────────────────────┐    ┌──────────────────────┐     │
│  │ Task 1: FlightCtrl   │    │ Task 2: Telemetry    │     │
│  │ Priority: 20 (HIGH)  │    │ Priority: 5 (LOW)    │     │
│  │ Freq: 50Hz (20ms)    │    │ Freq: 5Hz (200ms)    │     │
│  ├──────────────────────┤    ├──────────────────────┤     │
│  │ • BMP585->update()   │    │ • GPS->update()      │     │
│  │ • LSM6DS3->update()  │    │ • Receive Queue      │     │
│  │ • FSM->update()      │    │ • Serial.println()   │     │
│  │ • Parachute deploy   │    │ • sendLoRa()         │     │
│  │ • Send to Queue      │    │ • appendFile()       │     │
│  │ • Watchdog reset     │    │ • buzzSignal()       │     │
│  └──────────────────────┘    └──────────────────────┘     │
│           │                            │                   │
│           └──── sensorDataQueue ───────┘                   │
│                (25 slots, ~1.25KB)                         │
│                                                            │
│  ┌────────────────────────────────────────────────────┐   │
│  │ Task 3: Logger (CORE 0, Priority: 1, IDLE+)       │   │
│  │ • Receive from logQueue                           │   │
│  │ • Serial.printf() thread-safe                     │   │
│  │ • Append to /logs/flight.log (if DEBUG_MODE)      │   │
│  └────────────────────────────────────────────────────┘   │
│                      │                                     │
│                 logQueue (50 slots, ~6KB)                  │
│                      △                                     │
│          ┌───────────┴────────────┐                        │
│     Task 1 logs            Task 2 logs                     │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### FSM - Estados de Voo (⚠️ VALIDADO COM DADOS REAIS)

**CRÍTICO:** Esta FSM foi validada com 1,873 pontos de telemetria real. **NÃO ALTERE thresholds** sem consultar `extras/FSM_tester/FSM_Tester.py`.

```
┌─────────┐
│  IDLE   │  Pré-lançamento, aguardando no solo
└────┬────┘
     │ Trigger: totalAccel > 15.0 m/s²
     ▼
┌──────────┐
│ LIFTOFF  │  Subida motorizada, alta aceleração
└────┬─────┘
     │ Trigger: (az < -8.0 m/s²) OR (totalAccel < 2.0 m/s²)
     │          AND (height > 5m) AND (vz > 0.5 m/s)
     ▼
┌──────────┐
│ BURNOUT  │  Costa balística (sem motor)
└────┬─────┘
     │ Trigger: |vz| < 1.0 m/s AND az < -0.1 m/s²
     ▼
┌──────────┐
│  APOGEE  │  Altitude máxima atingida
└────┬─────┘  🪂 DEPLOY PARACHUTE! (estado crítico)
     │ Trigger: totalAccel < 11.5 m/s² AND vz < -5 m/s
     │          AND height > 5m
     ▼
┌───────────┐
│ FREEFALL  │  Queda rápida pós-apogeu
└────┬──────┘
     │ Trigger: altitude ≤ 100m AND vz < 0
     ▼
┌────────────┐
│ PARACHUTE  │  Descida controlada com paraquedas
└────┬───────┘
     │ Trigger: [Implementação futura]
     ▼
┌──────────┐
│  LANDED  │  Pouso detectado, fim do voo
└──────────┘
```

**Thresholds Validados (config.h):**
```cpp
// ⚠️ ATENÇÃO: Valores validados com dados reais de voo
// Referência: extras/FSM_tester/FSM_Tester.py

// Estado LIFTOFF (linha 137 do Python)
#define LIFTOFF_TOTAL_ACCEL_THRESHOLD  15.0   // m/s²

// Estado BURNOUT (linhas 47-59 do Python)
#define BURNOUT_AZ_THRESHOLD          -8.0    // m/s²
#define BURNOUT_TOTAL_ACCEL_MIN        2.0    // m/s²
#define BURNOUT_MIN_HEIGHT             5.0    // m
#define BURNOUT_MIN_VZ                 0.5    // m/s

// Estado APOGEE (linhas 72-79 do Python)
#define APOGEE_VZ_THRESHOLD            1.0    // m/s (absoluto)
#define APOGEE_AZ_THRESHOLD           -0.1    // m/s²

// Estado FREEFALL (linhas 95-113 do Python)
#define FREEFALL_TOTAL_ACCEL_MAX      11.5    // m/s²
#define FREEFALL_VZ_THRESHOLD         -5.0    // m/s
#define FREEFALL_MIN_HEIGHT            5.0    // m

// Estado PARACHUTE (linhas 130-153 do Python)
#define PARACHUTE_ALTITUDE_THRESHOLD  100.0   // m
#define PARACHUTE_VZ_NEGATIVE          0.0    // m/s (vz < 0)

// Validações de Segurança
#define SAFETY_CHECK_NAN_INF    true  // Obrigatório em todas detecções
#define SAFETY_MIN_ALTITUDE     0.0   // m (guard contra false positives)
```

**Validações de Segurança OBRIGATÓRIAS:**
```cpp
// CRÍTICO: Todas as funções de detecção devem incluir:
// 1. Verificação NaN/Inf (linhas 34-35, 72-73, 95-96, 130-131 do Python)
if (!std::isfinite(vz) || !std::isfinite(az) || !std::isfinite(totalAccel)) {
    return false;  // Dados inválidos
}

// 2. State Guards - condições mínimas para evitar false positives
// Exemplo BURNOUT (linhas 47-48):
if (height <= BURNOUT_MIN_HEIGHT || vz <= BURNOUT_MIN_VZ) {
    return false;  // Muito baixo/lento para ser burnout
}

// 3. Clipping de Velocidade Vertical (linha 201 do Python)
vz = constrain(vz, -200.0, 200.0);
```

---

## 📦 Bibliotecas e Dependências

### Sensores

| Sensor Antigo | Sensor Novo | Biblioteca | Instalação |
|---------------|-------------|------------|------------|
| BMP280 | **BMP585** | `Adafruit_BMP5xx` | Library Manager |
| MPU6050 | **LSM6DS3** | `Adafruit_LSM6DS` | Library Manager |
| GPS N6M | **GPS N8M** | `TinyGPSPlus` (mantém) | Já instalado |

### Dependências
- `Adafruit_Sensor` (unified sensor interface)
- `Adafruit_BusIO` (I2C/SPI abstraction)
- `ESPAsyncWebServer` (web interface)
- `ArduinoJson` (telemetry formatting)
- `LoRa` (wireless communication)

### Instalação
```bash
# Via Arduino Library Manager:
Adafruit BMP5xx
Adafruit LSM6DS
Adafruit Unified Sensor
Adafruit BusIO
TinyGPSPlus
ESPAsyncWebServer
ArduinoJson
LoRa
```

---

## 🗂️ Estruturas de Dados

### SensorData (Queue: Task 1 → Task 2)

```cpp
struct SensorData {
  // Timestamp
  unsigned long timestamp;
  uint16_t packet_count;
  
  // BMP585
  float altitude;
  float pressure;
  float temperature;
  float verticalVelocity;
  float maxAltitude;
  
  // LSM6DS3
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
  float totalAccel;  // 🆕 Calculado pelo LSM6DS3Sensor
  
  // GPS (apenas se fix disponível)
  double latitude, longitude;
  float gpsAltitude;
  uint8_t satellites;
  bool gps_valid;
  
  // FSM
  FlightState state;
  bool parachute_deployed;
};

// Queue: 25 slots × 64 bytes = ~1.6KB RAM
QueueHandle_t sensorDataQueue;
```

### LogMessage (Queue: Tasks 1,2 → Task 3)

```cpp
struct LogMessage {
  char message[128];
  unsigned long timestamp;
  uint8_t taskId;    // 1=FSM, 2=Telemetry, 3=Logger
  uint8_t level;     // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
};

// Queue: 50 slots × 140 bytes = ~7KB RAM
QueueHandle_t logQueue;
```

---

## 📝 Plano de Implementação (9 Fases)

### Checklist Geral

| Fase | Descrição | Tempo | Status |
|------|-----------|-------|--------|
| 1 | Setup e preparação | 30 min | Feito |
| 2 | Interface base + structs | 45 min | ⏳ Pendente |
| 3 | BMP585Sensor (classe) | 2 h | ⏳ Pendente |
| 4 | LSM6DS3Sensor (classe) | 1.5 h | ⏳ Pendente |
| 5 | GPSModule (classe) | 1 h | ⏳ Pendente |
| 6 | FSM - Máquina de estados | **4 h** | ⏳ Pendente |
| 7 | FreeRTOS Tasks | 4 h | ⏳ Pendente |
| 8 | Integração firmware.ino | 2 h | ⏳ Pendente |
| 9 | Adaptar módulos dependentes | 1.5 h | ⏳ Pendente |
| **TOTAL** | | **17.25h** | **0%** |

---

### FASE 1: Setup e Preparação ⏱️ 30min

**Status:** ⏳ Pendente

**Objetivos:**
- [x] Criar estrutura de diretórios (`sensors/`, `flight/`)
- [x] Reorganizar módulos existentes em `modules/`
- [x] Instalar bibliotecas via Arduino Library Manager
- [x] Criar branch `feature/oop-freertos-refactor`

**Ações:**
```bash
# 1. Criar branch
git checkout -b feature/oop-freertos-refactor

# 2. Criar estrutura de pastas
mkdir -p sensors flight modules

# 3. Mover módulos existentes
mv buzzer_module.h modules/
mv filesystem_module.h modules/
mv lora_module.h modules/
mv parachute_module.h modules/
mv server_module.h modules/

# 4. Instalar bibliotecas (via Arduino IDE Library Manager)
# - Adafruit_BMP5xx
# - Adafruit_LSM6DS
# - Adafruit_Sensor
# - Adafruit_BusIO
```

**Entregável:**
- ✅ Estrutura de pastas criada
- ✅ Bibliotecas instaladas
- ✅ Branch criado

**Notas:**
- Manter código antigo intacto até validação completa
- Testar bibliotecas com sketches simples antes de integrar

---

### FASE 2: Interface Base + Structs ⏱️ 45min

**Status:** ⏳ Pendente

**Objetivos:**
- [ ] Criar interface abstrata `ISensor`
- [ ] Criar structs de comunicação (`SensorData`, `LogMessage`)
- [ ] Definir enum `FlightState`

**Arquivos a Criar:**

**`sensors/ISensor.h`**
```cpp
#ifndef ISENSOR_H
#define ISENSOR_H

#include <Arduino.h>

/**
 * @brief Interface abstrata para sensores do flight computer
 */
class ISensor {
public:
  virtual ~ISensor() = default;
  
  virtual bool begin() = 0;
  virtual void update() = 0;
  virtual String getData() = 0;
  virtual bool isReady() = 0;
};

#endif
```

**`flight/SensorData.h`**
```cpp
#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

enum FlightState {
  IDLE = 0,
  LIFTOFF = 1,
  BURNOUT = 2,
  APOGEE = 3,
  FREEFALL = 4,
  PARACHUTE = 5,
  LANDED = 6
};

struct SensorData {
  unsigned long timestamp;
  uint16_t packet_count;
  
  float altitude;
  float pressure;
  float temperature;
  float verticalVelocity;
  float maxAltitude;
  
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
  
  FlightState state;
  bool parachute_deployed;
};

struct LogMessage {
  char message[128];
  unsigned long timestamp;
  uint8_t taskId;
  uint8_t level;
};

#endif
```

**Validação:**
- [ ] Compilação OK (headers apenas)
- [ ] Sem erros de sintaxe

---

### FASE 3: BMP585Sensor (Classe) ⏱️ 2h

**Status:** ⏳ Pendente

**Objetivos:**
- [ ] Criar classe do barômetro BMP585
- [ ] Migrar lógica de `bmp280_sensor.h`
- [ ] **IMPLEMENTAR cálculo de velocidade vertical (Vz)** - CRÍTICO para FSM
- [ ] Testar leitura de altitude/pressão/temperatura

**Arquivos:** `sensors/BMP585Sensor.h` + `sensors/BMP585Sensor.cpp`

**API Pública:**
```cpp
class BMP585Sensor : public ISensor {
public:
  BMP585Sensor();
  
  // Interface ISensor
  bool begin() override;
  void update() override;
  String getData() override;
  bool isReady() override;
  
  // Específicos do barômetro
  float getAltitude() const;
  float getMaxAltitude() const;
  float getVerticalVelocity() const;  // ⚠️ CRÍTICO: Calculado via diferenciação numérica
  void checkHighest();
  
private:
  Adafruit_BMP5XX bmp;
  float base_pressure;
  float max_altitude;
  float prev_altitude;
  unsigned long prev_time;
  float vertical_velocity;  // Vz = (altitude_current - altitude_previous) / dt
};
```

**Migração de `bmp280_sensor.h`:**
- ❌ Remover: variáveis globais `BMP`, `max_altitude`, `base_pressure`, etc.
- ✅ Transformar em: membros privados da classe
- ✅ Adaptar: `Adafruit_BMP280` → `Adafruit_BMP5XX`
- ✅ **IMPLEMENTAR cálculo de Vz** (referência: linha 201 do `FSM_Tester.py`)

**Cálculo de Velocidade Vertical (CRÍTICO):**
```cpp
// Em BMP585Sensor::update()
void BMP585Sensor::update() {
  sensors_event_t temp_event, pressure_event;
  bmp.getEvent(&pressure_event, &temp_event);
  
  float altitude_current = bmp.readAltitude(base_pressure);
  unsigned long time_current = millis();
  
  // Diferenciação numérica (Python linha 201)
  float dt = (time_current - prev_time) / 1000.0;  // segundos
  if (dt > 0.001) {  // Evitar divisão por zero
    vertical_velocity = (altitude_current - prev_altitude) / dt;
    vertical_velocity = constrain(vertical_velocity, -200.0, 200.0);  // Clipping
    
    prev_altitude = altitude_current;
    prev_time = time_current;
  }
}
```

**Teste Simples:**
```cpp
// test_bmp585.ino
#include "sensors/BMP585Sensor.h"

BMP585Sensor baro;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  if (baro.begin()) {
    Serial.println("BMP585 OK!");
  }
}

void loop() {
  baro.update();
  Serial.println(baro.getData());
  delay(1000);
}
```

**Validação:**
- [ ] Compilação OK
- [ ] `begin()` retorna true
- [ ] `getData()` retorna CSV válido
- [ ] `getAltitude()` retorna valor razoável

---

### FASE 4: LSM6DS3Sensor (Classe) ⏱️ 1.5h

**Status:** ⏳ Pendente

**Objetivos:**
- [ ] Criar classe do IMU LSM6DS3
- [ ] Migrar lógica de `mpu6050_sensor.h`
- [ ] **IMPLEMENTAR cálculo de aceleração total** - CRÍTICO para FSM
- [ ] Testar leitura de aceleração/giroscópio

**Arquivos:** `sensors/LSM6DS3Sensor.h` + `sensors/LSM6DS3Sensor.cpp`

**API Pública:**
```cpp
class LSM6DS3Sensor : public ISensor {
public:
  LSM6DS3Sensor();
  
  // Interface ISensor
  bool begin() override;
  void update() override;
  String getData() override;
  bool isReady() override;
  
  // Específicos do IMU
  float getAccelZ() const;
  float getTotalAccel() const;  // ⚠️ CRÍTICO: sqrt(ax² + ay² + az²)
  
private:
  Adafruit_LSM6DS3 lsm;
  float accelX, accelY, accelZ;
  float gyroX, gyroY, gyroZ;
  float total_accel;  // Calculado em update()
};
```

**Migração:**
- ✅ API Adafruit_MPU6050 → Adafruit_LSM6DS3 (quase idêntica!)
- ✅ Manter uso de `sensors_event_t` (compatível)
- ✅ **IMPLEMENTAR cálculo de totalAccel** (referência: linha 37 do `FSM_Tester.py`)

**Cálculo de Aceleração Total (CRÍTICO):**
```cpp
// Em LSM6DS3Sensor::update()
void LSM6DS3Sensor::update() {
  sensors_event_t accel, gyro, temp;
  lsm.getEvent(&accel, &gyro, &temp);
  
  accelX = accel.acceleration.x;
  accelY = accel.acceleration.y;
  accelZ = accel.acceleration.z;
  
  gyroX = gyro.gyro.x;
  gyroY = gyro.gyro.y;
  gyroZ = gyro.gyro.z;
  
  // Magnitude total da aceleração (Python linha 37)
  total_accel = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
}
```

**Validação:**
- [ ] Compilação OK
- [ ] `begin()` retorna true
- [ ] `getAccelZ()` detecta gravidade (~9.8 m/s²)

---

### FASE 5: GPSModule (Classe) ⏱️ 1h

**Status:** ⏳ Pendente

**Objetivos:**
- [ ] Encapsular GPS em classe
- [ ] Manter `TinyGPSPlus` (sem mudança de biblioteca)
- [ ] Testar recepção de coordenadas

**Arquivos:** `sensors/GPSModule.h` + `sensors/GPSModule.cpp`

**API Pública:**
```cpp
class GPSModule : public ISensor {
public:
  GPSModule(HardwareSerial* serial);
  
  bool begin() override;
  void update() override;
  String getData() override;
  bool isReady() override;
  
  String getTimeString() const;
  String getDateString() const;
};
```

**Validação:**
- [ ] Compilação OK
- [ ] GPS recebe NMEA sentences
- [ ] `getTimeString()` retorna tempo válido após fix

---

### FASE 6: FSM - Máquina de Estados ⏱️ 4h

**Status:** ⏳ Pendente

**⚠️ ATENÇÃO: Esta é a fase mais crítica - tradução linha-por-linha do Python para C++**

**Objetivos:**
- [ ] Implementar FSM com **7 estados** (IDLE → LIFTOFF → BURNOUT → APOGEE → FREEFALL → PARACHUTE → LANDED)
- [ ] Traduzir lógica de detecção do `FSM_Tester.py` **linha por linha**
- [ ] Implementar validações de segurança obrigatórias (NaN/Inf, state guards)
- [ ] Testar transições com dados simulados

**Arquivos:** `flight/FlightStateMachine.h` + `flight/FlightStateMachine.cpp`

**Estrutura:**
```cpp
class FlightStateMachine {
private:
  FlightState currentState;
  
  BMP585Sensor* baro;
  LSM6DS3Sensor* imu;
  
  // Contadores de debounce/confirmação
  uint8_t liftoffCounter;
  unsigned long apogeeDetectedTime;
  
  void checkTransitions();
  void transitionTo(FlightState newState);
  
  // Funções de detecção (traduzidas do Python)
  bool detectLiftoff();     // Python linhas 127-138
  bool detectBurnout();     // Python linhas 34-60
  bool detectApogee();      // Python linhas 68-80
  bool detectFreefall();    // Python linhas 86-114
  bool detectParachute();   // Python linhas 121-154
  bool detectLanded();      // [Implementação futura]

public:
  FlightStateMachine(BMP585Sensor* b, LSM6DS3Sensor* i);
  
  void begin();
  void update();
  FlightState getState() const;
  const char* getStateName() const;
};
```

**Implementação das Funções de Detecção (Referência Completa):**

**`detectLiftoff()` - Python linhas 127-138:**
```cpp
bool FlightStateMachine::detectLiftoff() {
  // LINHA 130-131: Validação de segurança
  float totalAccel = imu->getTotalAccel();
  if (!std::isfinite(totalAccel)) {
    return false;
  }
  
  // LINHA 137: Threshold validado
  if (totalAccel > LIFTOFF_TOTAL_ACCEL_THRESHOLD) {  // 15.0 m/s²
    liftoffCounter++;
    if (liftoffCounter >= 3) {  // Debounce: 3 leituras consecutivas
      return true;
    }
  } else {
    liftoffCounter = 0;
  }
  
  return false;
}
```

**`detectBurnout()` - Python linhas 34-60:**
```cpp
bool FlightStateMachine::detectBurnout() {
  // LINHA 34-35: Validação de segurança
  float az = imu->getAccelZ();
  float totalAccel = imu->getTotalAccel();
  float vz = baro->getVerticalVelocity();
  float height = baro->getAltitude();
  
  if (!std::isfinite(vz) || !std::isfinite(az) || !std::isfinite(totalAccel)) {
    return false;
  }
  
  // LINHA 47-48: State guards
  if (height <= BURNOUT_MIN_HEIGHT || vz <= BURNOUT_MIN_VZ) {
    return false;
  }
  
  // LINHA 53: Condição principal (OR lógico)
  bool condition1 = (az < BURNOUT_AZ_THRESHOLD);  // -8.0 m/s²
  bool condition2 = (totalAccel < BURNOUT_TOTAL_ACCEL_MIN);  // 2.0 m/s²
  
  return (condition1 || condition2);
}
```

**`detectApogee()` - Python linhas 68-80:**
```cpp
bool FlightStateMachine::detectApogee() {
  // LINHA 72-73: Validação de segurança
  float vz = baro->getVerticalVelocity();
  float az = imu->getAccelZ();
  
  if (!std::isfinite(vz) || !std::isfinite(az)) {
    return false;
  }
  
  // LINHA 76: Condição (valor absoluto de Vz)
  if (abs(vz) < APOGEE_VZ_THRESHOLD &&  // 1.0 m/s
      az < APOGEE_AZ_THRESHOLD) {        // -0.1 m/s²
    return true;
  }
  
  return false;
}
```

**`detectFreefall()` - Python linhas 86-114:**
```cpp
bool FlightStateMachine::detectFreefall() {
  // LINHA 95-96: Validação de segurança
  float totalAccel = imu->getTotalAccel();
  float vz = baro->getVerticalVelocity();
  float height = baro->getAltitude();
  
  if (!std::isfinite(totalAccel) || !std::isfinite(vz)) {
    return false;
  }
  
  // LINHA 108-109: State guards
  if (height <= FREEFALL_MIN_HEIGHT) {
    return false;
  }
  
  // LINHA 112: Condição principal (AND lógico)
  if (totalAccel < FREEFALL_TOTAL_ACCEL_MAX &&  // 11.5 m/s²
      vz < FREEFALL_VZ_THRESHOLD &&              // -5.0 m/s
      height > FREEFALL_MIN_HEIGHT) {            // 5.0 m
    return true;
  }
  
  return false;
}
```

**`detectParachute()` - Python linhas 121-154:**
```cpp
bool FlightStateMachine::detectParachute() {
  // LINHA 130-131: Validação de segurança
  float altitude = baro->getAltitude();
  float vz = baro->getVerticalVelocity();
  
  if (!std::isfinite(altitude) || !std::isfinite(vz)) {
    return false;
  }
  
  // LINHA 152: Condição principal
  if (altitude <= PARACHUTE_ALTITUDE_THRESHOLD &&  // 100.0 m
      vz < PARACHUTE_VZ_NEGATIVE) {                // vz < 0 (descendo)
    return true;
  }
  
  return false;
}
```

**Lógica de Transições:**

| De | Para | Função | Linha Python |
|----|------|--------|--------------|
| IDLE | LIFTOFF | `detectLiftoff()` | 127-138 |
| LIFTOFF | BURNOUT | `detectBurnout()` | 34-60 |
| BURNOUT | APOGEE | `detectApogee()` | 68-80 |
| APOGEE | FREEFALL | `detectFreefall()` | 86-114 |
| FREEFALL | PARACHUTE | `detectParachute()` | 121-154 |
| PARACHUTE | LANDED | `detectLanded()` | [Implementação futura] |

**Loop Principal (`checkTransitions()`):**
```cpp
void FlightStateMachine::checkTransitions() {
  switch (currentState) {
    case IDLE:
      if (detectLiftoff()) {
        transitionTo(LIFTOFF);
      }
      break;
      
    case LIFTOFF:
      if (detectBurnout()) {
        transitionTo(BURNOUT);
      }
      break;
      
    case BURNOUT:
      if (detectApogee()) {
        transitionTo(APOGEE);
      }
      break;
      
    case APOGEE:
      if (detectFreefall()) {
        transitionTo(FREEFALL);
      }
      break;
      
    case FREEFALL:
      if (detectParachute()) {
        transitionTo(PARACHUTE);
      }
      break;
      
    case PARACHUTE:
      if (detectLanded()) {
        transitionTo(LANDED);
      }
      break;
      
    case LANDED:
      // Estado final
      break;
  }
}
```

**Validação:**
- [ ] Todas as 6 transições funcionam (IDLE→PARACHUTE validadas, LANDED pendente)
- [ ] Validações de segurança (NaN/Inf) impedem crashes
- [ ] State guards evitam false positives
- [ ] Simulação com dados do CSV (13_30_11-Dados.csv)
- [ ] Logs de transição claros com timestamps

---

### FASE 7: FreeRTOS Tasks ⏱️ 4h

**Status:** ⏳ Pendente

**Objetivos:**
- [ ] Implementar Task 1 (FlightControl - 50Hz)
- [ ] Implementar Task 2 (Telemetry - 5Hz)
- [ ] Implementar Task 3 (Logger - low priority)
- [ ] Criar Queues de comunicação
- [ ] Configurar Watchdog

**Arquivos:**
- `flight/FlightControlTask.h/cpp`
- `flight/TelemetryTask.h/cpp`
- `flight/LoggerTask.h/cpp`

**Task 1 - FlightControl:**
```cpp
void taskFlightControl(void* parameter) {
  esp_task_wdt_add(NULL);  // Watchdog
  TickType_t xLastWakeTime = xTaskGetTickCount();
  
  while(1) {
    esp_task_wdt_reset();
    
    baroSensor->update();
    imuSensor->update();
    flightFSM->update();
    
    if (flightFSM->getState() == APOGEE && !parachute_deployed) {
      deployParachute();
      parachute_deployed = true;
      
      LogMessage log = {"[CRITICAL] Parachute deployed at APOGEE!", millis(), 1, 2};
      xQueueSend(logQueue, &log, 0);
    }
    
    SensorData data = { /* ... */ };
    xQueueSend(sensorDataQueue, &data, 0);
    
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(20));
  }
}
```

**Task 2 - Telemetry:**
```cpp
void taskTelemetry(void* parameter) {
  SensorData data;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  
  while(1) {
    gpsModule->update();
    
    if (xQueueReceive(sensorDataQueue, &data, 0) == pdPASS) {
      String telemetry = assembleTelemetry(data);
      
      Serial.println(telemetry);
      sendLoRa(telemetry);
      appendFile(file_dir, telemetry);
    }
    
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
  }
}
```

**Task 3 - Logger:**
```cpp
void taskLogger(void* parameter) {
  LogMessage log;
  
  while(1) {
    if (xQueueReceive(logQueue, &log, portMAX_DELAY) == pdPASS) {
      Serial.printf("[%lu][T%d] %s\n", 
        log.timestamp, log.taskId, log.message);
      
      #ifdef LOG_TO_FILE
        appendFile("/logs/flight.log", log.message);
      #endif
    }
  }
}
```

**Configuração:**
```cpp
// Watchdog
esp_task_wdt_init(5, true);  // 5s timeout, panic on trigger

// Queues
sensorDataQueue = xQueueCreate(25, sizeof(SensorData));
logQueue = xQueueCreate(50, sizeof(LogMessage));

// Tasks
xTaskCreatePinnedToCore(taskFlightControl, "FlightCtrl", 8192, NULL, 20, NULL, 1);
xTaskCreatePinnedToCore(taskTelemetry, "Telemetry", 16384, NULL, 5, NULL, 0);
xTaskCreatePinnedToCore(taskLogger, "Logger", 4096, NULL, 1, NULL, 0);
```

**Validação:**
- [ ] 3 tasks executando em paralelo
- [ ] Queue não overflow (monitorar `uxQueueMessagesWaiting()`)
- [ ] Watchdog reseta se Task 1 travar (testar!)

---

### FASE 8: Integração firmware.ino ⏱️ 2h

**Status:** ⏳ Pendente

**Objetivos:**
- [ ] Refatorar `firmware.ino` principal
- [ ] Criar tasks no `setup()`
- [ ] `loop()` vazio (tasks assumem controle)

**Estrutura:**
```cpp
#include <Wire.h>
#include <SPI.h>

#include "sensors/BMP585Sensor.h"
#include "sensors/LSM6DS3Sensor.h"
#include "sensors/GPSModule.h"
#include "flight/FlightStateMachine.h"
#include "flight/FlightControlTask.h"
#include "flight/TelemetryTask.h"
#include "flight/LoggerTask.h"

#include "modules/buzzer_module.h"
#include "modules/filesystem_module.h"
#include "modules/lora_module.h"
#include "modules/parachute_module.h"
#include "modules/server_module.h"

BMP585Sensor* baroSensor;
LSM6DS3Sensor* imuSensor;
GPSModule* gpsModule;
FlightStateMachine* flightFSM;

QueueHandle_t sensorDataQueue;
QueueHandle_t logQueue;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  baroSensor = new BMP585Sensor();
  imuSensor = new LSM6DS3Sensor();
  gpsModule = new GPSModule(&Serial1);
  
  if (!baroSensor->begin() || !imuSensor->begin() || !gpsModule->begin()) {
    Serial.println("FATAL: Sensor init failed!");
    while(1);
  }
  
  flightFSM = new FlightStateMachine(baroSensor, imuSensor);
  
  setupServo();
  setupLittleFS();
  setupServer();
  setupLoRa();
  
  sensorDataQueue = xQueueCreate(25, sizeof(SensorData));
  logQueue = xQueueCreate(50, sizeof(LogMessage));
  
  esp_task_wdt_init(5, true);
  
  xTaskCreatePinnedToCore(taskFlightControl, "FlightCtrl", 8192, NULL, 20, NULL, 1);
  xTaskCreatePinnedToCore(taskTelemetry, "Telemetry", 16384, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(taskLogger, "Logger", 4096, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelay(portMAX_DELAY);  // Tasks controlam tudo
}
```

**Validação:**
- [ ] Compilação OK
- [ ] Sistema inicializa sem erros
- [ ] Tasks executando corretamente

---

### FASE 9: Adaptar Módulos Dependentes ⏱️ 1.5h

**Status:** ⏳ Pendente

**Objetivos:**
- [ ] Atualizar `parachute_module.h` para usar classes
- [ ] Remover código antigo (bmp280_sensor.h, mpu6050_sensor.h, gps_module.h)
- [ ] Validação final

**Módulos a Adaptar:**

**`modules/parachute_module.h`:**
- Antes: `if (altitude < max_altitude - DESCENT_THRESHOLD)`
- Depois: `void handleParachute(BMP585Sensor* baro)`

**Deletar:**
- ❌ `bmp280_sensor.h`
- ❌ `mpu6050_sensor.h`
- ❌ `gps_module.h`

**Validação Final:**
- [ ] Nenhum módulo acessa variáveis globais de sensores
- [ ] Compilação limpa (sem warnings)
- [ ] Código antigo removido

---

## 🧪 Validação e Testes

### Checklist de Funcionalidades

**Sensores:**
- [ ] BMP585 lê altitude/pressão/temperatura
- [ ] LSM6DS3 lê aceleração/giroscópio (6 eixos)
- [ ] GPS recebe fix e coordenadas
- [ ] Calibração de base_pressure funciona

**FSM:**
- [ ] Transição IDLE → LIFTOFF (totalAccel > 15.0 m/s²)
- [ ] Transição LIFTOFF → BURNOUT (az < -8.0 OR totalAccel < 2.0)
- [ ] Transição BURNOUT → APOGEE (|vz| < 1.0 AND az < -0.1)
- [ ] Transição APOGEE → FREEFALL (totalAccel < 11.5 AND vz < -5.0)
- [ ] Transição FREEFALL → PARACHUTE (altitude ≤ 100 AND vz < 0)
- [ ] Validações de segurança (NaN/Inf) funcionando
- [ ] State guards evitando false positives

**FreeRTOS:**
- [ ] Task FSM roda a 50Hz preciso
- [ ] Task Telemetry roda a 5Hz
- [ ] Watchdog reseta se Task FSM travar
- [ ] Queue não overflow durante operação normal

**Telemetria:**
- [ ] Dados transmitidos via Serial
- [ ] Dados transmitidos via LoRa
- [ ] Dados gravados em `/data/HHMMSS-data.csv`
- [ ] Logs gravados em `/logs/HHMMSS-log.txt` (se habilitado)

**Paraquedas:**
- [ ] Deploy aciona no estado APOGEE
- [ ] Servo move para posição correta
- [ ] Buzzer toca sinal "Activated"

**WiFi Server:**
- [ ] Access Point criado
- [ ] Web interface acessível
- [ ] Download de arquivos funciona
- [ ] Deletar arquivos funciona

### Testes de Estresse

**Simulações:**
- [ ] Atraso Task Telemetry (500ms) - Task FSM continua?
- [ ] Sensor travado - Watchdog reseta sistema?
- [ ] Queue cheia - Sistema continua operando?
- [ ] Voo completo simulado (IDLE → LANDED)

**Hardware:**
- [ ] Teste de bancada (sensores reais)
- [ ] Teste de queda livre (simular apogee)
- [ ] Teste de vibração (simular liftoff)
- [ ] Validação de dados CSV gerados

---

## 📊 Uso de Recursos

### Memória Estimada

| Componente | RAM | Flash |
|------------|-----|-------|
| Código base | ~50 KB | ~200 KB |
| Bibliotecas Adafruit | ~10 KB | ~50 KB |
| FreeRTOS overhead | ~2 KB | ~20 KB |
| Task stacks (3 × 8KB avg) | 28 KB | - |
| Queues (25+50 slots) | ~8 KB | - |
| Objetos (sensors, FSM) | ~5 KB | - |
| **TOTAL** | **~103 KB** | **~270 KB** |

**Disponível:**
- RAM: 512 KB → Uso: ~20% ✅
- Flash: 8 MB → Uso: ~3% ✅

### Performance

| Métrica | Valor | Limite |
|---------|-------|--------|
| Task FSM cycle time | ~5-10ms | 20ms (50Hz) |
| Task Telemetry cycle | ~50-100ms | 200ms (5Hz) |
| Queue send latency | <1ms | - |
| Watchdog timeout | 5000ms | - |

---

## ⚠️ Riscos e Mitigações

| Risco | Prob. | Impacto | Mitigação |
|-------|-------|---------|-----------|
| API Adafruit_BMP5xx diferente | Média | Alto | Testar biblioteca ANTES (Fase 3) |
| Deadlock entre tasks | Baixa | Alto | Nunca usar `portMAX_DELAY` em Task FSM |
| Queue overflow | Média | Médio | Monitorar `uxQueueMessagesWaiting()` |
| Watchdog falso positivo | Baixa | Alto | Testar com timeout maior (10s) primeiro |
| GPS sem fix indoor | Alta | Baixo | Usar fallback "NOFIX" para filename |
| Filesystem lento | Média | Baixo | OK, Task 2 pode atrasar |

---

## 🐛 Troubleshooting

### Problemas Comuns

**"BMP585 initialization failed"**
- Verificar conexões I2C (SDA/SCL)
- Testar com I2C scanner
- Verificar endereço I2C (0x76 ou 0x77)

**"Task watchdog got triggered"**
- Task FSM travou (sensor não responde?)
- Aumentar timeout temporariamente
- Adicionar logs antes de operações bloqueantes

**"Queue full" ou dados perdidos**
- Task Telemetry muito lenta (filesystem?)
- Aumentar queue size (25 → 50)
- Otimizar Task 2 (remover delays desnecessários)

**FSM não transita de estado**
- Verificar thresholds em `config.h`
- Adicionar logs de debug nas condições
- Testar com valores simulados

---

## 📚 Referências

### Documentação
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)
- [FreeRTOS API Reference](https://www.freertos.org/a00106.html)
- [Adafruit BMP5xx Guide](https://learn.adafruit.com/adafruit-bmp5xx)
- [Adafruit LSM6DS Guide](https://learn.adafruit.com/lsm6ds)
- [TinyGPS++ Documentation](http://arduiniana.org/libraries/tinygpsplus/)

### Código de Referência
- [firmware.ino original](firmware.ino) - Código procedural (EOL)
- [MODULOS.md](MODULOS.md) - Documentação módulos antigos
- **[extras/FSM_tester/FSM_Tester.py](../extras/FSM_tester/FSM_Tester.py)** - ⚠️ **CRÍTICO:** FSM validado em Python
- **[extras/FSM_tester/explicacao.md](../extras/FSM_tester/explicacao.md)** - Documentação completa do FSM
- **[extras/FSM_tester/13_30_11-Dados.csv](../extras/FSM_tester/13_30_11-Dados.csv)** - Dataset de validação (1,873 pontos)

---

## 📝 Changelog

### 2026-03-18 - Planejamento Completo (v1.0)
- ✅ Arquitetura definida (POO + FreeRTOS + FSM)
- ✅ Decisões técnicas tomadas
- ✅ Plano de 9 fases criado
- ✅ Estimativas de tempo e recursos

### 2026-03-18 - Atualização v2.0 (FSM Validado)
- ✅ FSM corrigido para **7 estados** (adicionado PARACHUTE)
- ✅ Thresholds validados com dados reais (1,873 pontos de telemetria)
- ✅ Referências linha-por-linha ao código Python
- ✅ Validações de segurança obrigatórias documentadas
- ✅ Cálculos de Vz e totalAccel especificados
- ✅ Tempo total ajustado: 16.25h → 17.25h (Fase 6 aumentada)

### Próximas Etapas
- ⏳ Aguardando início da implementação (Fase 1)

---

## 🤝 Contribuindo

### Para Desenvolvedores

**Antes de começar uma fase:**
1. Ler esta documentação completa
2. Marcar fase como "Em Progresso" (⏳ → 🔄)
3. Criar branch específica: `git checkout -b fase-N-descricao`

**Durante implementação:**
1. Seguir estrutura proposta
2. Fazer commits pequenos e frequentes
3. Testar cada componente isoladamente

**Ao concluir fase:**
1. Marcar como "Concluída" (🔄 → ✅)
2. Validar checklist da fase
3. Fazer PR para branch principal
4. Atualizar este documento

### Para Agentes de Código

**Você pode usar este documento para:**
- Entender arquitetura do sistema
- Consultar decisões técnicas
- Verificar progresso das fases
- Tirar dúvidas sobre implementação
- Encontrar referências e exemplos

**Ao fazer modificações:**
- Sempre atualizar este documento
- Manter coerência com decisões tomadas
- Seguir estrutura de pastas definida
- Respeitar convenções de nomenclatura

---

## 📞 Contato

**Team #100 Avionics**  
**Projeto:** Flight Computer  
**Repositório:** `/home/vinicius/Documentos/Projetos/flight-computer/`

---

**Última atualização:** 2026-03-18  
**Versão do documento:** 2.0  
**Status geral:** 📋 Planejamento Completo - Pronto para Implementação

---

## ⚠️ ATUALIZAÇÃO v2.0: FSM Validado com Dados Reais

**IMPORTANTE:** Este documento foi atualizado para refletir o **FSM de 7 estados** validado com dados reais de voo.

**Fonte de Validação:**
- Arquivo: `extras/FSM_tester/FSM_Tester.py` (implementação Python completa)
- Dataset: `extras/FSM_tester/13_30_11-Dados.csv` (1,873 pontos de telemetria)
- Documentação: `extras/FSM_tester/explicacao.md` (lógica completa com referências de linha)

**Mudanças principais:**
1. FSM agora possui **7 estados** (não 6): `IDLE → LIFTOFF → BURNOUT → APOGEE → FREEFALL → PARACHUTE → LANDED`
2. Thresholds validados com dados reais (não estimativas)
3. Validações de segurança obrigatórias (NaN/Inf checks, state guards)
4. Lógica linha-por-linha documentada para tradução C++
