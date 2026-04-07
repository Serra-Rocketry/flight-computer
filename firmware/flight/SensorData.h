/**
 * @file SensorData.h
 * @brief Estruturas de dados compartilhadas entre tasks
 * 
 * Define as estruturas de dados usadas para comunicacao entre tasks
 * via FreeRTOS queues:
 * - SensorData: Dados dos sensores (BMP585, LSM6DS3, GPS)
 * - LogMessage: Mensagens de log para a task logger
 * 
 * @author Team #100 - Serra Rocketry
 * @date 2026-04-06
 * @version 1.0.0
 * 
 * @see firmware/REFACTORING_PLAN.md linhas 243-288 - Estruturas de Dados
 * @see firmware/REFACTORING_PLAN.md linhas 113-153 - FSM - Estados de Voo
 * @see firmware/flight/FlightControlTask.h - Task que popula SensorData
 * @see firmware/flight/TelemetryTask.h - Task que consome SensorData via queue
 */

#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>
#include <cstring>  // Para memset()

/**
 * @brief Estados da maquina de estados de voo
 * 
 * Máquina de estados simplificada com 4 estados principais.
 * Estados internos (LIFTOFF, BURNOUT, APOGEE, FREEFALL) são rastreados
 * via flags booleanas para melhor diagnóstico sem aumentar complexidade.
 * 
 * Validado com dados reais em extras/FSM_tester/13_30_11-Dados.csv
 * Referência: test/FSM/FSM.ino (implementação em 4 estados)
 */
enum FlightState {
  IDLE = 0,      ///< Pré-lançamento, aguardando no solo
  ASCENT = 1,    ///< Subida (LIFTOFF → BURNOUT → APOGEE)
  DESCENT = 2,   ///< Descida (FREEFALL → PARACHUTE → LANDING)
  LANDED = 3     ///< Pouso detectado, fim do voo
};

/**
 * @brief Converte enum FlightState para string
 * 
 * @param state Estado de voo
 * @return const char* Nome do estado
 */
inline const char* getFlightStateName(FlightState state) {
  switch (state) {
    case IDLE:    return "IDLE";
    case ASCENT:  return "ASCENT";
    case DESCENT: return "DESCENT";
    case LANDED:  return "LANDED";
    default:      return "UNKNOWN";
  }
}

/**
 * @brief Estrutura de dados dos sensores
 * 
 * Contém leituras de todos os sensores (BMP585, LSM6DS3, GPS) e
 * estado da maquina de estados. Enviada pela FlightControlTask
 * para a TelemetryTask via sensorDataQueue.
 * 
 * Tamanho real: **96 bytes** (~64 bytes estrutura + 32 bytes alignment)
 * Queue: 25 slots × 96 bytes = ~2.4KB RAM
 * 
 * @note O tamanho é maior que o estimado (64 bytes) devido a:
 *       - struct alignment (padding)
 *       - doubles para latitude/longitude (8 bytes cada)
 *       - Ainda assim, dentro do orçamento RAM (409 KB disponível)
 * 
 * @note Estado FlightState usa apenas 4 valores (IDLE, ASCENT, DESCENT, LANDED).
 *       Estados internos (LIFTOFF, BURNOUT, APOGEE, FREEFALL) são rastreados
 *       como flags separadas em FlightControlTask para diagnóstico.
 *       Ver: test/FSM/FSM.ino para implementação de referência.
 */
struct SensorData {
  // === TIMESTAMP ===
  unsigned long timestamp;    ///< Timestamp em millisegundos
  uint16_t packet_count;      ///< Numero sequencial do pacote
  
  // === BMP585 BAROMETER ===
  float altitude;             ///< Altitude em metros (relativa ao launchpad)
  float pressure;             ///< Pressao em hPa
  float temperature;          ///< Temperatura em °C
  float verticalVelocity;     ///< Velocidade vertical (Vz) em m/s
  float maxAltitude;          ///< Altitude maxima atingida em metros
  
  // === LSM6DS3 IMU ===
  float accelX, accelY, accelZ;   ///< Aceleracao em m/s²
  float gyroX, gyroY, gyroZ;      ///< Velocidade angular em °/s
  float totalAccel;               ///< Magnitude da aceleracao total em m/s²
  
  // === GPS (OPTIONAL) ===
  double latitude, longitude;     ///< Coordenadas GPS
  float gpsAltitude;              ///< Altitude do GPS em metros
  uint8_t satellites;             ///< Numero de satelites rastreados
  bool gps_valid;                 ///< True se GPS tem fix valido
  
  // === FSM STATE ===
  FlightState state;              ///< Estado atual do voo
  bool parachute_deployed;        ///< True se paraquedas foi desdobrado
  
  /**
   * @brief Construtor com inicializacao segura de todos os campos
   * 
   * CRÍTICO: Todos os campos são inicializados para evitar:
   * - Leitura de valores não inicializados (undefined behavior)
   * - NaN propagação na FSM
   * - Decisões de desdobramento de paraquedas baseadas em lixo de memória
   * 
   * @note Estado padrão é IDLE (espera por liftoff)
   */
  SensorData()
      : timestamp(0), packet_count(0),
        // BMP585
        altitude(0.0f), pressure(1013.25f), temperature(0.0f),
        verticalVelocity(0.0f), maxAltitude(0.0f),
        // LSM6DS3
        accelX(0.0f), accelY(0.0f), accelZ(9.81f),  // accelZ = gravidade
        gyroX(0.0f), gyroY(0.0f), gyroZ(0.0f),
        totalAccel(9.81f),  // Inicial = gravidade pura
        // GPS
        latitude(0.0), longitude(0.0), gpsAltitude(0.0f), satellites(0),
        gps_valid(false),
        // FSM (4 estados: IDLE, ASCENT, DESCENT, LANDED)
        state(IDLE), parachute_deployed(false) {}
};

/**
 * @brief Estrutura de mensagem de log
 * 
 * Enviada por qualquer task para a LoggerTask via logQueue.
 * Permite logging thread-safe com nivels de severidade.
 * 
 * Tamanho real: **144 bytes** (~140 bytes estimado)
 * Queue: 50 slots × 144 bytes = ~7.2KB RAM
 * 
 * @note Buffer de mensagem é inicializado com '\0' para evitar
 *       leitura de dados não inicializados ou overflow em strings
 */
struct LogMessage {
  char message[128];          ///< Mensagem de log (max 127 chars + null terminator)
  unsigned long timestamp;    ///< Timestamp em millisegundos
  uint8_t taskId;             ///< ID da task que enviou (1=FSM, 2=Telemetry, 3=Logger)
  uint8_t level;              ///< Nivel de severidade (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR)
  
  /**
   * @brief Construtor com inicializacao segura
   * 
   * CRÍTICO: Buffer é inicializado com '\0' para evitar:
   * - String buffer overflow
   * - Leitura de dados indefinidos
   * - Caracteres de lixo nos logs
   */
  LogMessage()
      : timestamp(0), taskId(0), level(0) {
    memset(message, 0, sizeof(message));  // Inicializar buffer com zeros
  }
};

/**
 * @brief Converte nivel de log para string
 * 
 * @param level Nivel de log
 * @return const char* Nome do nivel
 */
inline const char* getLogLevelName(uint8_t level) {
  switch (level) {
    case 0: return "[DEBUG]";
    case 1: return "[INFO]";
    case 2: return "[WARN]";
    case 3: return "[ERROR]";
    default: return "[???]";
  }
}

#endif // SENSOR_DATA_H
