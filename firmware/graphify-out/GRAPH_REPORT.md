# Graph Report - firmware  (2026-11-05)

## Corpus Check
- Corpus is ~19,457 words - fits in a single context window. You may not need a graph.

## Summary
- 359 nodes · 518 edges · 19 communities (13 shown, 6 thin omitted)
- Extraction: 85% EXTRACTED · 15% INFERRED · 0% AMBIGUOUS · INFERRED: 78 edges (avg confidence: 0.88)
- Token cost: 4,300 input · 6,200 output

## Community Hubs (Navigation)
- Flight State Machine Internals
- GPS Module
- Flight State & Sensor Data
- Barometer Sensor Setup
- Includes & Build Config
- Logging & LoRa Telemetry
- Flight State Machine Logic
- Flight Control Task
- IMU Sensor
- Barometer Methods
- Filtering & Improvement Plan
- SD Card Storage
- State Transition Logic
- NVS Snapshot Schema
- Log Message Structure
- NVS State Persistence
- Task ID Constants

## God Nodes (most connected - your core abstractions)
1. `FlightStateMachine` - 58 edges
2. `BMP585Sensor` - 35 edges
3. `SensorData` - 29 edges
4. `LSM6DS3Sensor` - 24 edges
5. `GPSModule` - 19 edges
6. `IMPROVEMENTS.md — Melhorias Profissionais Firmware v2.0 (#11 Serra Rocketry)` - 12 edges
7. `FlightStateMachine::update()` - 11 edges
8. `ISensor` - 11 edges
9. `taskFlightControl()` - 10 edges
10. `NvsSnapshot` - 10 edges

## Surprising Connections (you probably didn't know these)
- `P0-4: Bug real — deploy por backstop/contingência não persiste em NVS; serviço único requestParachuteDeploy(DeploySource) + CRC32 e dual-slot no NvsSnapshot` --references--> `NvsSnapshot`  [INFERRED]
  IMPROVEMENTS.md → flight/FlightStateMachine.h
- `P0-3: Ejeção com realimentação — separar deploy_commanded / deploy_actuated / deploy_confirmed no SensorData, segundo canal independente, BIT no pad` --references--> `SensorData`  [INFERRED]
  IMPROVEMENTS.md → flight/SensorData.h
- `P1-2: IMU sem range explícito nem calibração — configurar ±16 g, calibrar offsets no pad (NVS), flag imu_saturated em vez de descartar amostras; bias documentado de +2,81 m/s²` --references--> `SensorData`  [INFERRED]
  IMPROVEMENTS.md → flight/SensorData.h
- `P1-5: GPS entra na decisão de voo — usar location.age() (fix não expira hoje) e GPS como terceira fonte independente de altitude/velocidade por voto de confirmação` --references--> `GPSModule`  [INFERRED]
  IMPROVEMENTS.md → sensors/GPSModule.h
- `P0-2: Estimador α-β (g-h) ou Kalman 1-D com estado (h, vz) + apogeu por consenso temporal e físico em vez de amostra única` --references--> `LSM6DS3Sensor`  [INFERRED]
  IMPROVEMENTS.md → sensors/LSM6DS3Sensor.h

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Três caminhos convergentes de deploy do paraquedas (FSM, free-fall backstop, baro-stale contingency) — só a FSM persiste o estado** — flight_flightstatemachine_flightstatemachine, flightcontroltask_checkfreefallbackstop, flightcontroltask_checkbarostalecontingency, flightcontroltask_deployparachute, parachute_module_setupservo [EXTRACTED 1.00]
- **SensorData como contrato do sistema — liga FlightControlTask, TelemetryTask, FSM e o formato CSV** — flight_sensordata_sensordata, flight_flightstatemachine_flightstatemachine [EXTRACTED 1.00]
- **Recurso LoRa/SD compartilhado sem dono único — sendLoRa chamado de duas tasks no mesmo barramento SPI** — modules_lora_module_sendlora, telemetrytask_tasktelemetry, loggertask_tasklogger [EXTRACTED 1.00]

## Communities (19 total, 6 thin omitted)

### Community 0 - "Flight State Machine Internals"
Cohesion: 0.05
Nodes (28): FlightStateMachine, _altAboveCount, _apogeeDetected, _baro, _burnoutDetected, _filtAx, _filtAy, _filtAz (+20 more)

### Community 1 - "GPS Module"
Cohesion: 0.07
Nodes (10): GPSModule::getData(), GPSModule::getDateString(), GPSModule::getTimeString(), GPSModule::GPSModule(), GPSModule::hasValidFix(), GPSModule, _gps, _ready (+2 more)

### Community 2 - "Flight State & Sensor Data"
Cohesion: 0.06
Nodes (31): FlightState, ASCENT, DESCENT, IDLE, LANDED, getFlightStateName(), SensorData, accelX (+23 more)

### Community 3 - "Barometer Sensor Setup"
Cohesion: 0.07
Nodes (13): BMP585Sensor, _altitude, _basePressure, _bmp, _bmp280, _maxAltitude, _pressure, _prevAltitude (+5 more)

### Community 5 - "Logging & LoRa Telemetry"
Cohesion: 0.09
Nodes (16): LoggerStats, droppedCount, printedCount, receivedCount, getLogLevelName(), Semtech AN1200.22 — LoRa Modulation Basics (fórmula de time-on-air, relação SF/BW/CR), taskLogger(), buzzRecoveryBeep() (+8 more)

### Community 6 - "Flight State Machine Logic"
Cohesion: 0.08
Nodes (9): FlightStateMachine::detectLiftoffTimed(), FlightStateMachine::FlightStateMachine(), FlightStateMachine::getData(), FlightStateMachine::isParachuteDeployed(), FlightStateMachine::persistToNVS(), FlightStateMachine::restoreFromNVS(), Dual deployment — drogue no apogeu + main a 150-300 m (roadmap; hoje um único evento de ejeção no apogeu), ESP-IDF NVS — escrita atômica, wear leveling e limitações do armazenamento não-volátil (+1 more)

### Community 7 - "Flight Control Task"
Cohesion: 0.14
Nodes (14): beginSensorWithRetry(), buildSensorData(), checkAutoRezero(), checkBaroGlitch(), checkBaroStaleContingency(), checkFreefallBackstop(), deployParachute(), handleArmCommand() (+6 more)

### Community 8 - "IMU Sensor"
Cohesion: 0.11
Nodes (10): LSM6DS3Sensor, _accelX, _accelY, _accelZ, _gyroX, _gyroY, _gyroZ, _lsm (+2 more)

### Community 10 - "Filtering & Improvement Plan"
Cohesion: 0.13
Nodes (13): BMP585Sensor::getVerticalVelocity(), packet_count, FlightStateMachine::detectApogee(), Filtro α-β (g-h) — estimador de baixo custo computacional para (h, vz) no ESP32, Datasheet BMP585 (Bosch) — ruído de pressão, ODR, IIR interno, Mastering the FreeRTOS Real Time Kernel (mutexes, exclusão mútua, inversão de prioridade), Kalman 1-D — filtro com estado (h, vz), predição h⁻ = h + vz·dt e atualização pelo residual do barômetro, Datasheet LSM6DS3 (ST) — ranges do acelerômetro/gyro (+5 more)

### Community 11 - "SD Card Storage"
Cohesion: 0.21
Nodes (6): getStorageName(), getStorageType(), isStorageReady(), setupStorage(), writeFile(), initTelemetryTask()

### Community 13 - "NVS Snapshot Schema"
Cohesion: 0.22
Nodes (9): NvsSnapshot, basePressure, confirmCount, flags, magic, maxAltitude, reserved, state (+1 more)

### Community 14 - "Log Message Structure"
Cohesion: 0.33
Nodes (5): LogMessage, level, message, taskId, timestamp

### Community 16 - "Task ID Constants"
Cohesion: 0.50
Nodes (4): TaskId, TASK_ID_FLIGHT_CONTROL, TASK_ID_LOGGER, TASK_ID_TELEMETRY

## Knowledge Gaps
- **94 isolated node(s):** `_baro`, `_imu`, `_state`, `_ready`, `_liftoffDetected` (+89 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 212 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `FlightStateMachine` connect `Flight State Machine Internals` to `GPS Module`, `Flight State & Sensor Data`, `Barometer Sensor Setup`, `Includes & Build Config`, `Flight State Machine Logic`, `IMU Sensor`, `State Transition Logic`, `NVS Snapshot Schema`, `NVS State Persistence`?**
  _High betweenness centrality (0.335) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `SensorData` (e.g. with `P0-3: Ejeção com realimentação — separar deploy_commanded / deploy_actuated / deploy_confirmed no SensorData, segundo canal independente, BIT no pad` and `P1-2: IMU sem range explícito nem calibração — configurar ±16 g, calibrar offsets no pad (NVS), flag imu_saturated em vez de descartar amostras; bias documentado de +2,81 m/s²`) actually correct?**
  _`SensorData` has 2 INFERRED edges - model-reasoned connections that need verification._
- **What connects `_baro`, `_imu`, `_state` to the rest of the system?**
  _94 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Flight State Machine Internals` be split into smaller, more focused modules?**
  _Cohesion score 0.04878048780487805 - nodes in this community are weakly interconnected._
- **Why does `BMP585Sensor` connect `Barometer Sensor Setup` to `Flight State Machine Internals`, `Barometer Methods`, `Flight State Machine Logic`, `GPS Module`?**
  _High betweenness centrality (0.171) - this node is a cross-community bridge._
- **Are the 2 inferred relationships involving `LSM6DS3Sensor` (e.g. with `P0-2: Estimador α-β (g-h) ou Kalman 1-D com estado (h, vz) + apogeu por consenso temporal e físico em vez de amostra única` and `P1-2: IMU sem range explícito nem calibração — configurar ±16 g, calibrar offsets no pad (NVS), flag imu_saturated em vez de descartar amostras; bias documentado de +2,81 m/s²`) actually correct?**
  _`LSM6DS3Sensor` has 2 INFERRED edges - model-reasoned connections that need verification._
- **Should `GPS Module` be split into smaller, more focused modules?**
  _Cohesion score 0.06554621848739496 - nodes in this community are weakly interconnected._