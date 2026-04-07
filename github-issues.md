# GitHub Issues - Refatoração v2.0 Flight Computer

**Projeto**: Flight Computer - Team #100  
**Branch**: `feature/oop-freertos-refactor`  
**Total de Issues**: 14  
**Tempo Total Estimado**: ~22 horas (atualizado após simplificação da Issue #6)
**Issues Críticas**: #4, #6, #7, #12

---

## Issue #1: Setup do Projeto e Estrutura de Diretórios

**Labels**: `setup`, `refactor`  
**Prioridade**: Alta  
**Tempo Estimado**: 30 minutos  
**Dependências**: Nenhuma

### Descrição

Criar a estrutura de diretórios para a refatoração v2.0 e reorganizar módulos existentes seguindo o plano detalhado em `firmware/REFACTORING_PLAN.md` (Fase 1).

Esta é a base fundamental para toda a refatoração OOP + FreeRTOS.

### Tarefas

- Criar branch `feature/oop-freertos-refactor` a partir de `main`
- Criar diretórios da nova estrutura:
  - `firmware/sensors/` - Classes de sensores OOP
  - `firmware/flight/` - FSM e tasks FreeRTOS
  - `firmware/modules/` - Módulos procedurais existentes (temporariamente)
- Mover módulos procedurais para `firmware/modules/`:
  - `buzzer_module.h`
  - `filesystem_module.h`
  - `lora_module.h`
  - `parachute_module.h`
  - `server_module.h`
- Instalar bibliotecas via Arduino Library Manager:
  - `Adafruit_BMP5xx` (para BMP585)
  - `Adafruit_LSM6DS` (para LSM6DS3)
  - `Adafruit_Sensor`
  - `Adafruit_BusIO`
- Verificar que o código existente ainda compila após reorganização

### Critérios de Aceitação

- Estrutura de pastas criada conforme especificado
- Módulos reorganizados sem quebrar código atual
- Bibliotecas instaladas e projeto compila sem erros
- Arduino IDE reconhece todas as dependências

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 1 (Setup)
- `AGENTS.md` - Seção de Build Commands

---

## Issue #2: Criar Interface Base ISensor e Estruturas de Dados

**Labels**: `refactor`, `architecture`  
**Prioridade**: Alta  
**Tempo Estimado**: 45 minutos  
**Dependências**: Issue #1

### Descrição

Implementar a interface abstrata `ISensor` e as estruturas de dados compartilhadas (`SensorData`, `LogMessage`) que serão usadas por todas as classes de sensores e tasks FreeRTOS. Esta é a fundação da arquitetura OOP.

### Tarefas

- Criar `firmware/sensors/ISensor.h` com interface abstrata pura:
  ```cpp
  class ISensor {
  public:
    virtual ~ISensor() = default;
    virtual bool begin() = 0;
    virtual void update() = 0;
    virtual String getData() = 0;
    virtual bool isReady() = 0;
  };
  ```
- Criar `firmware/flight/SensorData.h` com estruturas FreeRTOS:
  - `struct SensorData` (64 bytes max) com campos para todos os sensores
  - `struct LogMessage` (140 bytes max) com timestamp e nível de log
  - Declarar handles das queues FreeRTOS:
    ```cpp
    extern QueueHandle_t sensorDataQueue;
    extern QueueHandle_t logQueue;
    ```
- Documentar todas as estruturas com comentários Doxygen detalhados
- Verificar que as estruturas não excedem limites de memória (64B e 140B)

### Critérios de Aceitação

- `ISensor.h` compila sem erros
- `SensorData.h` compila sem erros
- Estruturas documentadas com comentários Doxygen completos
- Interfaces estão bem definidas para implementação pelos sensores concretos

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 2 (Interfaces)
- `AGENTS.md` - Seção de Code Style Guidelines

---

## Issue #3: Implementar Classe BMP585Sensor (Barômetro)

**Labels**: `sensor`, `refactor`  
**Prioridade**: Média  
**Tempo Estimado**: 2 horas  
**Dependências**: Issue #2

### Descrição

Migrar a lógica do módulo procedural `bmp280_sensor.h` para uma classe OOP `BMP585Sensor` que implementa a interface `ISensor`. O sensor será atualizado do BMP280 para o BMP585, mantendo a funcionalidade de cálculo de altitude e velocidade vertical.

### Tarefas

- Criar `firmware/sensors/BMP585Sensor.h` com declaração da classe
- Criar `firmware/sensors/BMP585Sensor.cpp` com implementação
- Implementar métodos da interface `ISensor`:
  - `bool begin()` - Inicialização do sensor e calibração de pressão base
  - `void update()` - Leitura de pressão, cálculo de altitude e velocidade vertical
  - `String getData()` - Retorno de dados em formato CSV
  - `bool isReady()` - Verificação se sensor está operacional
- Adicionar métodos específicos da classe:
  - `float getAltitude() const` - Altitude atual em metros
  - `float getVerticalVelocity() const` - Velocidade vertical em m/s
  - `float getMaxAltitude() const` - Altitude máxima atingida
- Implementar validação rigorosa de dados:
  - Verificação de valores NaN ou Inf
  - Validação de range (altitude entre -500m e 50000m)
  - Filtro de outliers baseado em derivadas
- Criar teste simples em `test/test_bmp585/test_bmp585.ino` para validação básica

### Critérios de Aceitação

- Código compila sem warnings no Arduino IDE
- `begin()` retorna `true` quando sensor está conectado
- `getAltitude()` retorna valores razoáveis (±10m da altitude real conhecida)
- `getVerticalVelocity()` calcula corretamente usando derivada numérica
- Validação de NaN funciona (retorna valor anterior válido)
- Teste básico passa com sensor conectado

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 3 (BMP585)
- `AGENTS.md` - Seção de Error Handling
- `firmware/bmp280_sensor.h` - Código atual para referência

---

## Issue #4: Implementar Classe LSM6DS3Sensor (IMU)

**Labels**: `sensor`, `refactor`, `critical`  
**Prioridade**: Alta (crítico para FSM)  
**Tempo Estimado**: 1.5 horas  
**Dependências**: Issue #2

### Descrição

Migrar a lógica do módulo procedural `mpu6050_sensor.h` para uma classe OOP `LSM6DS3Sensor`. CRÍTICO: Esta implementação deve incluir o cálculo de aceleração total que é essencial para o funcionamento da Flight State Machine (FSM).

### Tarefas

- Criar `firmware/sensors/LSM6DS3Sensor.h` com declaração da classe
- Criar `firmware/sensors/LSM6DS3Sensor.cpp` com implementação
- Implementar métodos da interface `ISensor`:
  - `bool begin()` - Inicialização do sensor LSM6DS3
  - `void update()` - Leitura de acelerômetro e giroscópio
  - `String getData()` - Dados em formato CSV
  - `bool isReady()` - Status operacional
- **[CRÍTICO]** Implementar cálculo de aceleração total:
  ```cpp
  total_accel = sqrt(accelX*accelX + accelY*accelY + accelZ*accelZ);
  ```
- Adicionar métodos específicos:
  - `float getAccelZ() const` - Aceleração no eixo Z
  - `float getTotalAccel() const` - Aceleração total (essencial para FSM)
  - `void getAcceleration(float* x, float* y, float* z) const` - Vetor aceleração
  - `void getGyroscope(float* x, float* y, float* z) const` - Vetor giroscópio
- Implementar validações de dados (NaN, ranges realistas)
- Criar teste em `test/test_lsm6ds3/test_lsm6ds3.ino`

### Critérios de Aceitação

- `getTotalAccel()` retorna aproximadamente 9.8 m/s² em repouso
- `getAccelZ()` detecta corretamente orientação (positivo/negativo)
- Giroscópio retorna valores realistas
- Sensor responde a movimentos e acelerações

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 4 (LSM6DS3)
- `extras/FSM_tester/FSM_Tester.py` - Uso de total_accel na FSM
- `firmware/mpu6050_sensor.h` - Código atual para referência

---

## Issue #5: Implementar Classe GPSModule

**Labels**: `sensor`, `refactor`  
**Prioridade**: Baixa  
**Tempo Estimado**: 1 hora  
**Dependências**: Issue #2

### Descrição

Encapsular a funcionalidade GPS em uma classe OOP `GPSModule` que implementa `ISensor`. Manter a biblioteca `TinyGPSPlus` existente, apenas reorganizar em arquitetura orientada a objetos.

### Tarefas

- Criar `firmware/sensors/GPSModule.h` com declaração da classe
- Criar `firmware/sensors/GPSModule.cpp` com implementação
- Implementar interface `ISensor`:
  - `bool begin()` - Inicialização da serial GPS
  - `void update()` - Processamento de dados NMEA
  - `String getData()` - Dados GPS em CSV
  - `bool isReady()` - Status do GPS
- Adicionar métodos específicos:
  - `String getTimeString() const` - Timestamp formatado
  - `String getDateString() const` - Data formatada
  - `bool hasValidFix() const` - Fix GPS válido
- Garantir que não bloqueie se não houver sinal GPS
- Criar teste em `test/test_gps/test_gps.ino`

### Critérios de Aceitação

- GPS recebe e processa sentenças NMEA
- `getTimeString()` retorna tempo válido após fix
- Não bloqueia execução se não houver sinal
- Fix GPS detectado corretamente

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 5 (GPS)
- `firmware/gps_module.h` - Código atual para referência

---

## Issue #6: Migrar FSM Testada para Classe OOP - CRÍTICO

**Labels**: `fsm`, `critical`, `refactor`  
**Prioridade**: Crítica  
**Tempo Estimado**: 1.5 horas (reduzido)  
**Dependências**: Issue #3, Issue #4

### Descrição

Migrar a implementação FSM testada e validada de `test/FSM/FSM.ino` para uma classe OOP `FlightStateMachine` que implementa a interface `ISensor`. Esta versão reduzida usa 4 estados principais (IDLE, ASCENT, DESCENT, LANDED) com os mesmos thresholds validados do código Python.

CRÍTICO: Esta implementação controla diretamente o deploy do paraquedas.

### Tarefas

- Criar `firmware/flight/FlightStateMachine.h` com declaração da classe
- Criar `firmware/flight/FlightStateMachine.cpp` com implementação
- Migrar enum `FlightState` com 4 estados:
  ```cpp
  enum FlightState { 
    IDLE = 0, 
    ASCENT = 1, 
    DESCENT = 2, 
    LANDED = 3 
  };
  ```
- Migrar funções de detecção validadas:
  - `detect_liftoff()` - `total_accel > 15.0`
  - `detect_burnout()` - `(az < -8.0 || total_acc < 2.0) && height > 5.0 && vz > 0.5`
  - `detect_apogee()` - `abs(vz) < 1.0 && az < -0.1`
  - `detect_freefall()` - `total_acc < 11.5 && height > 5.0 && vz < -5.0`
  - `detect_parachute()` - `height <= PARACHUTE_ALTITUDE && vz < 0`
  - `detect_landed()` - `abs(vz) < 0.5 && height < 2.0`
- Migrar lógica de filtros e suavização (alpha = 0.2)
- Implementar métodos da interface `ISensor`
- Adicionar método `FlightState getCurrentState() const`
- Manter validações de NaN e ranges de segurança
- Adicionar logging de transições de estado

### Critérios de Aceitação

- FSM detecta os 4 estados corretamente com dados reais
- Thresholds idênticos ao arquivo `test/FSM/FSM.ino`
- Mesmo comportamento que a versão testada
- Sem transições falsas (false positives)
- Deploy de paraquedas APENAS no estado DESCENT com condições corretas
- Código compila sem warnings

### Referências

- `test/FSM/FSM.ino` - Implementação testada e validada (322 linhas)
- `extras/FSM_tester/FSM_Tester.py` - Thresholds originais validados
- `extras/FSM_tester/explicacao.md` - Explicação dos estados
- `firmware/REFACTORING_PLAN.md` - Fase 6 (FSM)

---

## Issue #7: Implementar FlightControlTask (Core 1, 50Hz)

**Labels**: `freertos`, `critical`, `refactor`  
**Prioridade**: Alta  
**Tempo Estimado**: 2 horas  
**Dependências**: Issue #6

### Descrição

Implementar a task crítica do FreeRTOS que executa a FSM e controla o deploy do paraquedas. Esta task roda a 50Hz no Core 1 (dedicado) com prioridade máxima.

### Tarefas

- Criar `firmware/flight/FlightControlTask.h` com declaração da task
- Criar `firmware/flight/FlightControlTask.cpp` com implementação
- Implementar `taskFlightControl()`:
  - Loop preciso a 50Hz (20ms) usando `vTaskDelayUntil()`
  - Atualização sequencial de BMP585 e LSM6DS3
  - Execução da FSM (FlightStateMachine)
  - Envio de dados consolidados para `sensorDataQueue`
  - Alimentação do watchdog do sistema
- Configurar task FreeRTOS:
  - Prioridade: 20 (máxima)
  - Core: 1 (pinned/dedicado)
  - Stack size: 4096 bytes
  - Task name: "FlightControl"
- Implementar medição de tempo de execução (deve ser < 20ms)

### Critérios de Aceitação

- Task executa consistentemente a 50Hz (±1ms de jitter)
- Tempo de execução < 20ms (medido com `esp_timer_get_time()`)
- Watchdog não reseta o sistema (feed correto)
- Dados enviados para queue sem perda
- FSM executa corretamente dentro da task

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 7 (FreeRTOS Tasks)
- `AGENTS.md` - Seção FreeRTOS Guidelines

---

## Issue #8: Implementar TelemetryTask (Core 0, 5Hz)

**Labels**: `freertos`, `refactor`  
**Prioridade**: Média  
**Tempo Estimado**: 1.5 horas  
**Dependências**: Issue #7

### Descrição

Implementar task de telemetria que processa dados dos sensores e os transmite via múltiplos canais (LoRa, Serial, LittleFS) a 5Hz no Core 0.

### Tarefas

- Criar `firmware/flight/TelemetryTask.h` com declaração
- Criar `firmware/flight/TelemetryTask.cpp` com implementação
- Implementar `taskTelemetry()`:
  - Loop a 5Hz (200ms) usando `vTaskDelayUntil()`
  - Recebimento de dados da `sensorDataQueue`
  - Atualização do módulo GPS
  - Transmissão via LoRa (protocolo binário)
  - Saída para Serial Monitor (formato legível)
  - Append para arquivo CSV no LittleFS
- Configurar task FreeRTOS:
  - Prioridade: 5 (média)
  - Core: 0 (compartilhado)
  - Stack size: 4096 bytes
  - Task name: "Telemetry"
- Implementar timeout para queue (não bloquear indefinidamente)

### Critérios de Aceitação

- Queue recebe dados corretamente da FlightControlTask
- Transmissão LoRa funciona sem bloquear outras tasks
- Arquivo CSV é criado e populado com dados timestamped
- Serial output mostra dados formatados corretamente

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 7 (FreeRTOS Tasks)
- `firmware/lora_module.h` - Protocolo atual para referência

---

## Issue #9: Implementar LoggerTask (Core 0, baixa prioridade)

**Labels**: `freertos`, `refactor`  
**Prioridade**: Baixa  
**Tempo Estimado**: 1 hora  
**Dependências**: Issue #8

### Descrição

Implementar task assíncrona de logging de debug com baixa prioridade que não interfere com as operações críticas do sistema.

### Tarefas

- Criar `firmware/flight/LoggerTask.h` com declaração
- Criar `firmware/flight/LoggerTask.cpp` com implementação
- Implementar `taskLogger()`:
  - Recebimento assíncrono da `logQueue`
  - Impressão no Serial com timestamp formatado
  - Filtros por nível de log (DEBUG, INFO, WARN, ERROR)
  - Bufferização para evitar perda de mensagens
- Configurar task FreeRTOS:
  - Prioridade: 1 (mínima)
  - Core: 0 (compartilhado)
  - Stack size: 2048 bytes
  - Task name: "Logger"
- Implementar níveis de log configuráveis

### Critérios de Aceitação

- Logs aparecem no Serial Monitor com timestamps
- Não interfere com tasks críticas (FlightControl, Telemetry)
- Filtragem por nível funciona corretamente
- Buffer não overflow em alta frequência de logs

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 7 (FreeRTOS Tasks)
- `AGENTS.md` - Seção Error Handling

---

## Issue #10: Integrar Tudo no firmware.ino

**Labels**: `integration`, `refactor`  
**Prioridade**: Alta  
**Tempo Estimado**: 2 horas  
**Dependências**: Issue #7, Issue #8, Issue #9

### Descrição

Integrar todas as partes do sistema no arquivo principal `firmware.ino`, criando as queues FreeRTOS e iniciando todas as tasks. Esta é a fase de integração completa do sistema v2.0.

### Tarefas

- Atualizar `firmware/firmware.ino`:
  - Incluir todos os novos headers de sensores e tasks
  - Instanciar objetos dos sensores (BMP585, LSM6DS3, GPS)
  - Criar as queues FreeRTOS no `setup()`:
    ```cpp
    sensorDataQueue = xQueueCreate(10, sizeof(SensorData));
    logQueue = xQueueCreate(20, sizeof(LogMessage));
    ```
  - Criar e iniciar as 3 tasks FreeRTOS
  - Loop principal vazio (FreeRTOS assume controle)
- Testar inicialização completa do sistema
- Verificar uso de memória:
  - RAM utilizada < 103KB (limite ESP32-C3)
  - Flash utilizada < 270KB
- Implementar tratamento de erros na inicialização

### Critérios de Aceitação

- Código compila sem warnings
- Todas as 3 tasks iniciam corretamente
- Sistema não reseta durante inicialização (watchdog OK)
- Telemetria sendo enviada (LoRa + Serial)
- Uso de memória dentro dos limites do ESP32-C3

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 8 (Integração)
- `AGENTS.md` - Seção FreeRTOS Guidelines

---

## Issue #11: Adaptar Módulos Dependentes

**Labels**: `refactor`, `integration`  
**Prioridade**: Média  
**Tempo Estimado**: 1.5 horas  
**Dependências**: Issue #10

### Descrição

Atualizar os módulos procedurais existentes que dependem de variáveis globais antigas para usar a nova arquitetura OOP com getters e estruturas de dados.

### Tarefas

- Atualizar `modules/parachute_module.h`:
  - Remover dependências de variáveis globais (`max_altitude`)
  - Receber `max_altitude` via parâmetro de função
  - Receber `state` da FSM via parâmetro
  - Manter lógica de deploy de paraquedas inalterada
- Atualizar `modules/telemetry_module.h`:
  - Usar `SensorData` struct ao invés de funções individuais
  - Adaptar para receber dados consolidados
- Atualizar `modules/server_module.h`:
  - Acessar dados via getters dos objetos sensor
  - Remover acesso direto a variáveis globais
- Verificar que todos os módulos ainda compilam
- Testar integração com o sistema principal

### Critérios de Aceitação

- Todos os módulos compilam sem erros
- Não há mais dependências de variáveis globais antigas
- Funcionalidade dos módulos mantida (telemetria, servidor, paraquedas)
- Integração com firmware.ino funciona corretamente

### Referências

- `firmware/REFACTORING_PLAN.md` - Fase 9 (Integração Final)
- `AGENTS.md` - Seção Code Style Guidelines

---

## Issue #12: Validar FSM com Dados Reais de Voo

**Labels**: `testing`, `validation`, `critical`  
**Prioridade**: Crítica  
**Tempo Estimado**: 2 horas  
**Dependências**: Issue #10

### Descrição

Validar que a implementação C++ da FSM funciona idênticamente ao código Python validado, usando os 1.873 pontos de telemetria real de voo.

### Tarefas

- Executar baseline com `extras/FSM_tester/FSM_Tester.py` (Python validado)
- Implementar modo de teste no firmware para injetar dados CSV
- Comparar saídas lado a lado:
  - Timestamps de cada transição de estado
  - Ordem das transições
  - Condições que triggeram cada estado
- Verificar diferenças (se houver) e explicar justificativas
- Documentar resultados da validação

### Critérios de Aceitação

- Liftoff detectado em ~0.20s (igual ao Python)
- Burnout detectado em ~2.10s (igual ao Python)
- Apogee detectado em ~4.50s (igual ao Python)
- Freefall detectado em ~4.55s (igual ao Python)
- Parachute deploy em ~5.20s a 745m (igual ao Python)
- Landed detectado em ~45.0s (igual ao Python)
- Todas as transições ocorrem nos mesmos timestamps (±0.2s)

### Referências

- `extras/FSM_tester/FSM_Tester.py` - Baseline validado
- `extras/FSM_tester/explicacao.md` - Análise detalhada
- `extras/FSM_tester/13_30_11-Dados.csv` - Dados reais (1.873 pontos)

**Labels**: `testing`, `hardware`  
**Prioridade**: Média  
**Tempo Estimado**: 3 horas  
**Dependências**: Issue #10

### Descrição

Testar o firmware completo v2.0 em hardware real ESP32 com todos os sensores conectados, simulando condições de voo.

### Tarefas

- Montar bancada de testes completa:
  - ESP32-C3 SuperMini
  - BMP585 (barômetro)
  - LSM6DS3 (IMU)
  - GPS NEO-8M
  - Módulo LoRa
- Testar inicialização de todos os sensores
- Testar transições da FSM manualmente (simulando aceleração/queda)
- Verificar transmissão de telemetria LoRa
- Verificar logging para LittleFS
- Verificar servidor WiFi (se aplicável)
- Executar testes de stress (múltiplas horas de operação)

### Critérios de Aceitação

- Todos os sensores inicializam corretamente
- FSM responde a movimentos físicos do ESP32
- Transmissão LoRa é recebida por ground station
- Arquivos CSV são criados e contêm dados válidos
- Servidor WiFi é acessível (se implementado)
- Sistema roda por múltiplas horas sem crashes

### Referências

- `test/FSM/FSM.ino` - Teste FSM atual para referência
- `AGENTS.md` - Seção Test Commands

---

## Issue #13: Testes de Bench (Hardware Real)

**Labels**: `testing`, `hardware`  
**Prioridade**: Média  
**Tempo Estimado**: 3 horas  
**Dependências**: Issue #10

### Descrição

Testar o firmware completo v2.0 em hardware real ESP32 com todos os sensores conectados, simulando condições de voo.

### Tarefas

- Montar bancada de testes completa:
  - ESP32-C3 SuperMini
  - BMP585 (barômetro)
  - LSM6DS3 (IMU)
  - GPS NEO-8M
  - Módulo LoRa
- Testar inicialização de todos os sensores
- Testar transições da FSM manualmente (simulando aceleração/queda)
- Verificar transmissão de telemetria LoRa
- Verificar logging para LittleFS
- Verificar servidor WiFi (se aplicável)
- Executar testes de stress (múltiplas horas de operação)

### Critérios de Aceitação

- Todos os sensores inicializam corretamente
- FSM responde a movimentos físicos do ESP32
- Transmissão LoRa é recebida por ground station
- Arquivos CSV são criados e contêm dados válidos
- Servidor WiFi é acessível (se implementado)
- Sistema roda por múltiplas horas sem crashes

### Referências

- `test/FSM/FSM.ino` - Teste FSM atual para referência
- `AGENTS.md` - Seção Test Commands

---

## Issue #14: Atualizar Documentação Pós-Refatoração

**Labels**: `documentation`  
**Prioridade**: Baixa  
**Tempo Estimado**: 2 horas  
**Dependências**: Issue #10

### Descrição

Atualizar toda a documentação do projeto para refletir a nova arquitetura v2.0 OOP + FreeRTOS após refatoração completa.

### Tarefas

- Atualizar `README.md` com nova arquitetura e diagramas
- Atualizar `docs/software.md` com descrição das tasks FreeRTOS
- Atualizar `firmware/MODULOS.md` com novos sensores OOP
- Adicionar diagramas Mermaid das tasks FreeRTOS e fluxo de dados
- Atualizar `CHANGELOG.md` com versão 2.0.0 e todas as mudanças
- Revisar `CONTRIBUTING.md` para refletir novos padrões
- Atualizar referências em `AGENTS.md` se necessário

### Critérios de Aceitação

- Toda documentação reflete o estado atual do código
- Diagramas Mermaid estão corretos e atualizados
- CHANGELOG documenta todas as mudanças significativas
- Links e referências estão funcionais
- Documentação é consistente com código implementado

### Referências

- `firmware/REFACTORING_PLAN.md` - Arquitetura final implementada
- `AGENTS.md` - Guia para agentes de desenvolvimento
- `.opencode/skills/` - Skills dos especialistas

---

## Ordem de Implementação Recomendada

```
#1 → #2 → [#3, #4, #5] → #6 → #7 → #8 → #9 → #10 → #11 → [#12, #13] → #14
```

**Notas**: 
- Issues entre [] podem ser implementadas em paralelo
- Issues marcadas como 'critical' requerem code review obrigatório por 2 pessoas
- Issues #6, #7, #12 são safety-critical (controlam paraquedas)

**Tempo Total Estimado**: ~22 horas (atualizado após simplificação da Issue #6)
**Issues Críticas**: 4 (#4, #6, #7, #12)