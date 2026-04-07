/**
 * @file ISensor.h
 * @brief Interface abstrata para sensores do flight computer
 * 
 * Define a interface padrão que todos os sensores devem implementar,
 * permitindo abstração e polimorfismo para BMP585, LSM6DS3, GPS, etc.
 * 
 * @author Team #100 - Serra Rocketry
 * @date 2026-04-06
 * @version 1.0.0
 * 
 * @see firmware/REFACTORING_PLAN.md linhas 54-57 - Estrutura de Diretórios
 * @see firmware/REFACTORING_PLAN.md linhas 365-386 - Interface Base (Fase 2)
 * @see firmware/sensors/BMP585Sensor.h - Exemplo de implementação concreta
 * @see firmware/flight/FlightControlTask.h - Uso em tasks FreeRTOS
 */

#ifndef ISENSOR_H
#define ISENSOR_H

#include <Arduino.h>

/**
 * @brief Interface abstrata para sensores do flight computer
 * 
 * Todos os sensores (barometro, IMU, GPS, etc.) devem herdar desta classe
 * e implementar os metodos definidos aqui.
 * 
 * @example
 * **Exemplo 1: Implementação de um sensor (BMP585Sensor)**
 * 
 * @code{.cpp}
 * #include "ISensor.h"
 * #include <Adafruit_BMP5XX.h>
 * 
 * class BMP585Sensor : public ISensor {
 * private:
 *   Adafruit_BMP5XX _bmp;
 *   float _altitude;
 *   bool _isReady;
 * 
 * public:
 *   bool begin() override {
 *     if (!_bmp.begin_I2C(0x77)) {
 *       return false;  // Sensor não encontrado
 *     }
 *     _isReady = true;
 *     return true;
 *   }
 * 
 *   void update() override {
 *     if (!_isReady) return;
 *     sensors_event_t temp_event, pressure_event;
 *     _bmp.getEvent(&pressure_event, &temp_event);
 *     _altitude = _bmp.readAltitude(1013.25);
 *   }
 * 
 *   String getData() override {
 *     return String(_altitude) + "m";
 *   }
 * 
 *   bool isReady() override {
 *     return _isReady;
 *   }
 * };
 * @endcode
 * 
 * **Exemplo 2: Uso em FreeRTOS Task (50Hz)**
 * 
 * @code{.cpp}
 * // Instância global do sensor
 * ISensor* g_baroSensor = nullptr;
 * 
 * // Inicialização na setup()
 * void setup() {
 *   Serial.begin(115200);
 *   delay(1000);
 * 
 *   g_baroSensor = new BMP585Sensor();
 *   if (!g_baroSensor->begin()) {
 *     Serial.println("❌ Erro ao inicializar BMP585!");
 *     while(1);  // Travamento seguro
 *   }
 *   Serial.println("✅ BMP585 inicializado");
 * }
 * 
 * // FlightControlTask (Core 1, 50Hz)
 * void flightControlTask(void* parameter) {
 *   TickType_t xLastWakeTime = xTaskGetTickCount();
 *   const TickType_t xFrequency = pdMS_TO_TICKS(20);  // 50Hz = 20ms
 * 
 *   while(true) {
 *     // Atualizar sensor (DEVE SER NÃO-BLOQUEANTE!)
 *     if (g_baroSensor->isReady()) {
 *       g_baroSensor->update();
 *       String data = g_baroSensor->getData();
 *       Serial.println(data);  // CSV ou JSON
 *     }
 * 
 *     // Delay sem bloquear outras tasks
 *     vTaskDelayUntil(&xLastWakeTime, xFrequency);
 *   }
 * }
 * @endcode
 * 
 * @note A chamada a update() **DEVE SER NÃO-BLOQUEANTE** para não afetar
 *       outras tasks de maior prioridade em FreeRTOS.
 * 
 * @note Para sensores lentos (GPS), usar TelemetryTask (5Hz) ao invés de
 *       FlightControlTask (50Hz). Ver firmware/REFACTORING_PLAN.md.
 */
class ISensor {
public:
  /**
   * @brief Destrutor virtual (necessario para polimorfismo)
   * 
   * Permite que objetos derivados sejam deletados via ponteiro base
   * sem vazamento de memoria.
   */
  virtual ~ISensor() = default;
  
  /**
   * @brief Inicializa o sensor
   * 
   * Deve ser chamado na setup() da aplicacao, antes de qualquer update().
   * Pode ser bloqueante (durante calibracao, por exemplo).
   * 
   * @return true se inicializacao bem-sucedida, false em caso de erro
   *         (sensor nao encontrado, comunicacao falhou, etc.)
   */
  virtual bool begin() = 0;
  
  /**
   * @brief Atualiza as leituras do sensor
   * 
   * Esta funcao deve ser **nao-bloqueante** e chamada regularmente
   * pelo task que gerencia o sensor. Para BMP585 e LSM6DS3, deve ser
   * chamada em FlightControlTask a 50Hz. Para GPS, a 5Hz em TelemetryTask.
   */
  virtual void update() = 0;
  
  /**
   * @brief Retorna uma string com os dados do sensor (para Serial/logging)
   * 
   * @return String com dados formatados do sensor (ex: "BMP585: 1234.5m, 101.3hPa")
   */
  virtual String getData() = 0;
  
  /**
   * @brief Verifica se o sensor esta pronto para uso
   * 
   * @return true se sensor esta pronto (inicializado e operacional),
   *         false caso contrario (nao inicializado, erro de I2C, etc.)
   */
  virtual bool isReady() = 0;
};

#endif // ISENSOR_H
