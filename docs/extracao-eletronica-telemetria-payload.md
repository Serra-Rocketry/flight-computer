# Extração — Setor de Eletrônica, Telemetria e Payload

> **Fonte:** repositório `flight-computer` (branch `dev-2026`, HEAD `130149c`)
> **Para:** preenchimento do relatório do setor (competição 7ª LASC)
> **Data da extração:** 2026-08-06
> **Status:** v2.0 implementada (firmware completo em OOP); eletrônica em
> transição — schematic v2.0 existe mas PCB de voo ainda não roteada

---

## 1. System Architecture & CONOPS

### 1.1 Descrição geral do sistema eletrônico

O computador de voo é um sistema embarcado baseado em **ESP32-S3**
(DevKitC-1 N8R2), dois núcleos, 8 MB Flash, 2 MB PSRAM, executando
**FreeRTOS** com arquitetura orientada a objetos (v2.0). O sistema é
projetado para missões de foguetes suborbitais de curta duração (até
20 minutos), com a responsabilidade crítica de **abrir o paraquedas no
apogeu** de forma autônoma e sem feedback de confirmação mecânica.

O projeto transita de uma PCB protótipo (ESP32-C3 SuperMini + BME280 +
ICM-20602, uma camada, 4 componentes SMD) para a placa definitiva v2.0
(ESP32-S3 + BMP585 + LSM6DS3 + GPS M8N + LoRa 915 MHz + SD + step-down),
cujo schematic existe mas ainda não foi roteado. Durante a fase atual, o
firmware v2.0 é validado na placa DevKitC em bancada.

**Princípios de projeto do firmware:**
- Todas as decisões de deploy do paraquedas são **one-shot** (uma única
  vez, idempotente) e baseadas em **múltiplas camadas de redundância**
  independentes entre si.
- Dados de voo são gravados localmente (SD ou flash interna) como fonte
  primária; a telemetria LoRa é *best-effort* — o sistema nunca para de
  gravar por causa do link de rádio.
- Se qualquer componente de sensor falha, o sistema **não para** — usa
  fallbacks para os últimos valores válidos ou sensores redundantes.

### 1.2 Subsistema de hardware

| Subsistema | Componente | Função | Interface | Observações |
|---|---|---|---|---|
| Processador | ESP32-S3 DevKitC-1 N8R2 | Controle central,FreeRTOS, 2 núcleos | — | 240 MHz, 512 KB SRAM, 8 MB Flash, 2 MB PSRAM |
| Barômetro | BMP585 (Bosch) | Altitude barométrica, pressão, temperatura, Vz derivado no firmware | I2C (0x77) | 300–1250 hPa; ±0,5 hPa absoluta, ±0,06 hPa relativa; resolução ~0,01 hPa |
| IMU | LSM6DS3 (STMicroelectronics) | Aceleração (±16 g) e giroscópio (±2000 °/s) | I2C (0x6A) | Usado como sensor primário para detecção de queda livre (backstop) e contingência do barômetro congelado |
| GPS | u-blox NEO-8M / M8N | Geolocalização (lat/lon/alt) e tempo (nome do arquivo CSV) | UART (NMEA, não-bloqueante) | Multi-constelação: GPS + GLONASS + Galileo + BeiDou; sensibilidade −167 dBm; fix cold 26 s, hot 1 s |
| Telemetria | RFM95W HopeRF | Link de rádio LoRa 915 MHz (Brasil/Américas) | SPI (SCK=4, MISO=2, MOSI=3, CS=5, RST=6, DIO0=7) | SF7, BW125 kHz, CR 4/5, CRC on, TX +17 dBm; alcance ~4 km campo aberto |
| Armazenamento primário | MicroSD (SPI) | Gravação CSV 22 campos a 5 Hz (~4 KB/s) | SPI (bus compartilhado com LoRa, CS=12) | Cartão class 10; formato FAT32 |
| Armazenamento fallback | LittleFS (flash interna) | Fallback automático se SD falhar | Interno (NVS flash) | Capacidade limitada (~4 MB); se SD e LittleFS falharem, dados vão apenas para Serial |
| Atuador | Servo metálico ~4,8 kg·cm | Libera a porta do compartimento do paraquedas no apogeu | PWM (GPIO 7) | **50° (fechado) → 135° (ejeção)** — ângulos de bancada 2026-08-27; acionamento one-shot, sem feedback mecânico |
| Sinalização | Buzzer **passivo** | Beeps de status na inicialização (sucesso/falha) e alarme em safe-hold | GPIO 6 | Piezo passivo: exige onda quadrada via `tone()`/LEDC; **ressonância 2700 Hz** (varredura de bancada 27/08) — DC não produz som |
| Potência (estágio 1) | XL4015 step-down 5 A | Conversão DC-DC bateria → 5 V (servo, buzzer, GPS) | — | Substituiu o Mini-560 (27/08) após colapso sob pico do servo na bateria (unidade defeituosa; ver FMECA F16). Dropout ~1,5-2 V (assíncrono) — headroom aperta com 2S em fim de descarga |
| Potência (baterias) | 3× 18650 Li-ion paralelo | Fonte primária de energia | — | 3,7 V nominal, ~6000 mAh total; autonomia estimada 4–6 h |

### 1.3 Arquitetura de software (FreeRTOS)

O firmware é estruturado em três tarefas FreeRTOS concorrentes, duas
executadas no Core 0 (comunicação/armazenamento) e uma no Core 1
(controle de voo em tempo real). A comunicação entre tarefas ocorre
exclusivamente via filas (queues) — não há variáveis compartilhadas entre
tasks, eliminando condições de corrida por construção.

**Tarefa 1 — `taskFlightControl` (Core 1, prioridade 20, 50 Hz)**
Responsabilidade: leitura sequencial dos sensores (BMP585 → LSM6DS3),
execução da FSM, detecção e acionamento do paraquedas, alimentação do
watchdog (TWDT), coleta de métricas de execução (ciclos, overruns, queue
drops, tempo máximo de ciclo). Esta é a tarefa mais crítica do sistema
— sua perda (travamento) ativa o watchdog e provoca reboot, mas o estado
da FSM é restaurado do NVS, garantindo que o deploy possa ocorrer mesmo
após o reboot.

**Tarefa 2 — `taskTelemetry` (Core 0, prioridade 5, 5 Hz)**
Responsabilidade: atualização não-bloqueante do GPS (NMEA feed), drenagem
da fila de sensores (consome apenas a amostra mais recente, descartando
backlog de 200 ms), enriquecimento com fix GPS, montagem do CSV 22 campos
via `snprintf` (uma única conversão — não fragmenta heap), fan-out em três
canais simultâneos: Serial (debug), LoRa (receptor em solo) e arquivo
(SD ou LittleFS). O formato CSV é idêntico ao esperado pelo receptor
`recovery-webui/components/receiver-lora`.

**Tarefa 3 — `taskLogger` (Core 0, prioridade baixa, evento)**
Responsabilidade: recebe mensagens da fila de logs (50 slots) com filtro
de nível e imprime no Serial. Usado para diagnóstico e rastreio de
eventos de segurança (ARM, deploy, erros de watchdog).

**Parâmetros-chave de execução:**

| Parâmetro | Valor | Justificativa |
|---|---|---|
| Período FlightControl | 20 ms (50 Hz) | Resolução suficiente para detectar apogeu com 20 ms de latência (vz muda ~0,5 m/s por ciclo no apogeu) |
| Período Telemetry | 200 ms (5 Hz) | Balanço entre taxa de amostragem para o receptor e janela de TX LoRa |
| Tamanho da fila de sensores | 25 slots | FlightControl produz 10× mais rápido que Telemetry; fila absorve variações de timing |
| Tamanho da fila de logs | 50 slots | Suficiente para burst de logs durante初始化 |
| Stack da FlightControlTask | 8192 bytes | Mínimo necessário para BMP585Sensor + LSM6DS3Sensor + FlightStateMachine + buffers |
| Stack da TelemetryTask | 4096 bytes | GPSModule + montagem de String (snprintf em buffer fixo 256 bytes) |
| Timeout do watchdog | 10 s | Mais longo que o período de voo (~10 s até apogeu), para não reiniciar durante a subida |

### 1.4 Lógica algorítmica de acionamento da recuperação

O sistema de recuperação é a função mais crítica do computador de voo.
Ele é implementado como uma **Finite State Machine (FSM) de 4 estados**
com 7 flags de sub-evento, projetada com múltiplas camadas de defesa
independentes para garantir que o paraquedas abra mesmo em caso de falhas
cascata.

**FSM principal:** `IDLE → ASCENT → DESCENT → LANDED`

**Sub-eventos (flags booleanas, uma vez setadas nunca são limpas até reset):**
- `liftoff`: aceleração total > 15,0 m/s² (3× gravidade — limiar acima
  de qualquer vibração realista no solo)
- `burnout`: aceleração na direção vertical < −8 m/s² OU aceleração
  total < 2 m/s², com altitude > 50 m e vz > 2 m/s (sub-evento
  informativo, não muda o estado)
- `apogee`: |vz| < 1,0 m/s (detecção por vz do barômetro; o gate de az
  foi removido em 2026-08-05 por pêndulo/bias do acelerômetro em voo real)
- `freefall`: aceleração total < 3 m/s² + vz < −5 m/s + altura > 100 m
- `parachute`: detectado pelo `detectParachute()` (abaixo)

**Cadeia de deploy (Opção A — abre no apogeu, inegociável):**

1. **Armamento na rampa (risco nº2):** antes do voo, o operador envia o
   comando `ARM` via Serial. O firmware limpa o snapshot NVS, re-captura
   a `base_pressure` (pressão atmosférica local) e zera o `maxAltitude`.
   Se a altitude derivar mais de −10 m por 3 s na rampa (drift do
   barômetro por mudança de temperatura), o sistema faz *re-zero*
   automático da pressão base. O ARM é recusado se o voo já começou
   (`maxAltitude > 10 m`) ou se o paraquedas já foi aberto.

2. **Liftoff:** quando a aceleração total (vetor filtrado com IIR,
   α=0,2) ultrapassa 15,0 m/s² por três ciclos consecutivos, a FSM
   transita de `IDLE` para `ASCENT`. A filtragem IIR com semente no
   primeiro valor real evita transições falsas por ruído de arranque.

3. **Apogeu:** quando |vz| cruza 1,0 m/s (vz do barômetro), a FSM
   transita de `ASCENT` para `DESCENT` e zera o contador de confirmação
   do paraquedas. A remoção do gate de az resolveu o problema de pêndulo
   onde o bias do acelerômetro (az = +2,81 m/s² em repouso no voo real)
   impedia a detecção do apogeu.

4. **Confirmação de descida (anti-oscilação):** o deploy do paraquedas
   requer `vz < −2,0 m/s` por **3 ciclos consecutivos** (60 ms a 50 Hz).
   Se durante esses ciclos o vz voltar a ser ≥ −2 m/s (oscilação do
   barômetro), o contador reseta para zero. Isso impede deploys falsos
   por ruído de quantização (50 Hz resolve vz em ~0,1 m/s; validado com
   0 deploys prematuros em 1.873 pontos de voo real).

5. **Acionamento do servo:** uma vez que `_parachuteDeployed` é setado
   como `true`, o FlightControlTask chama `ParachuteServo.write(SERVO_OPEN)`
   (comando PWM único, ângulo 0°). A flag `g_parachuteActuated` impede
   qualquer re-acionamento futuro. Se ocorrer reboot mid-flight, o NVS
   restaura `_parachuteDeployed` e o `setupServo(keepOpen=true)` mantém o
   servo na posição aberta.

**Contingências independentes da FSM (redundância por camadas):**

| Camada | Mecanismo | Gatilho | O que faz |
|---|---|---|---|
| 1. FSM principal | `detectParachute()` | Apogeu + vz < −2 m/s por 3 ciclos | Deploy normal |
| 2. Backstop de queda livre | `checkFreefallBackstop()` | acc < 3 m/s² por 1 s + vz < −5 m/s + altura > 50 m | Deploy mesmo se FSM travada |
| 3. Contingência barômetro congelado | `checkBaroStaleContingency()` | Barômetro sem leitura válida por 2 s + IMU detecta queda livre + `maxAltitude` último bom > 50 m | Deploy usando apenas IMU |
| 4. Guarda de solo | `PARACHUTE_MIN_ALTITUDE` | Altura < 50 m | Bloqueia deploy perto do solo |
| 5. Armamento / re-zero | Comando `ARM` | Operador na rampa | Limpa estado e recalibra |

Cada camada é executada independentemente no loop da tarefa FlightControl,
com seus próprios filtros IIR e contadores de ciclo, sem compartilhar
estado entre si. Mesmo que duas camadas falhem simultaneamente, a
terceira pode garantir o deploy.

### 1.5 Arquitetura da carga útil (Payload)

O "payload" do computador de voo é a **própria aquisição de dados
científicos do voo** — não há módulo de payload separado. O sistema
adquire e transmite 22 campos de dados por pacote, a 5 Hz, em formato
CSV alinhado ao protocolo do receptor em solo.

**Campos adquiridos (22 por pacote):**

| # | Campo | Tipo | Unidade | Fonte | Descrição |
|---|---|---|---|---|---|
| 0 | TEAM_ID | string | — | fixo (config.h) | Identificador do time (ex.: `#11`) |
| 1 | millis | uint32 | ms | millis() | Tempo desde boot |
| 2 | count | uint32 | — | contador | Número sequencial do pacote |
| 3 | altp | float | m | BMP585 | Altitude barométrica |
| 4 | temp | float | °C | BMP585 | Temperatura |
| 5 | umi | float | % | placeholder (0) | Umidade — sem sensor instalado |
| 6 | p | float | hPa | BMP585 | Pressão atmosférica |
| 7–9 | gx/gy/gz | float | rad/s | LSM6DS3 | Componentes do giroscópio |
| 10–12 | ax/ay/az | float | m/s² | LSM6DS3 | Componentes da aceleração |
| 13 | vz | float | m/s | BMP585 (derivado) | Velocidade vertical |
| 14 | maxAltitude | float | m | BMP585 | Altitude máxima atingida |
| 15 | state | int | — | FSM | Estado de voo (0=IDLE a 3=LANDED) |
| 16 | alt | float | m | GPS | Altitude GPS (nan se sem fix) |
| 17 | lat | float | graus | GPS | Latitude |
| 18 | lon | float | graus | GPS | Longitude |
| 19 | sat | uint8 | — | GPS | Número de satélites |
| 20 | parachute | int | 0/1 | FSM | Flag de deploy (1 = aberto) |
| 21 | rssi | int | dBm | receptor (overwrite) | Placeholder 0; receptor substitui pelo RSSI real medido |

**Canais de saída simultâneos:**
- **Serial** (115200 baud): formato legível para monitor serial
- **LoRa** (915 MHz): formato CSV 22 campos para o receptor em solo
- **Arquivo** (SD ou LittleFS): formato CSV 22 campos em disco

A telemetria é *best-effort*: se o LoRa não conseguir transmitir ou o
arquivo não puder ser escrito, o sistema continua operando normalmente.
Os dados locais (SD/LittleFS) são a **fonte primária** de dados de voo.

**Receptor:** o módulo `recovery-webui/components/receiver-lora` recebe
o pacote LoRa, substitui o campo `rssi` pelo valor real medido, insere
hora/data GPS locais (24 campos no total) e re-transmite para a WebUI.

---

## 2. Weights, Measures & Performance

### 2.1 Dimensões físicas

O computador de voo é composto por uma placa de circuito impresso (PCB)
de dimensões compactas para caber na baia de aviônica do foguete. A
dimensão documentada mais confiável é a do conjunto completo (placa +
baterias + fiação + suportes), que deve caber dentro da baia com espaço
para ventilação e acesso aos conectores.

| Item | Dimensão | Fonte |
|---|---|---|
| PCB definitiva v2.0 (contorno no KiCad) | 47,5 × 40 mm | `electronics.kicad_pcb` |
| Dimensão aproximada do conjunto completo | 125 × 50 × 30 mm (com componentes e fiação) | `docs/hardware.md` — referência do projeto CDB; **validar após o leiaute final** |
| Baia de aviônica | 125 × 50 mm (diâmetro × comprimento, encaixe na ogiva) | `docs/hardware.md` |
| Bateria | 3× células 18650 em paralelo (~18 mm × 65 mm cada) | `docs/hardware.md` |

> **Nota:** a dimensão de 125 × 50 × 30 mm refere o projeto legado CDB.
> O leiaute da placa v2.0 pode ser diferente — precisa ser medido após
> a rotação final da PCB.

### 2.2 Massas

A massa do conjunto aviônico é um parâmetro crítico para o cálculo de
centro de gravidade (CG) e estabilidade do foguete. A seguir, os valores
conhecidos e as lacunas que precisam ser preenchidas.

| Componente | Massa | Status | Observações |
|---|---|---|---|
| Servo do paraquedas | ~9 g (doc.) ou ~55 g (estimativa para 4,8 kg·cm) | ⚠️ **Conflito** | `docs/hardware.md` diz "metal gears, ~4,8 kg·cm, ~9 g"; 9 g é faixa de micro servo (~1,8 kg·cm) enquanto 4,8 kg·cm é faixa de servo ~55 g. **Identificar o modelo real e ajustar antes de publicar.** |
| ESP32-S3 DevKitC-1 | ~10 g (estimativa) | 🟡 Medido por referência | Inclui antena IPEX, headers e reguladores onboard |
| BMP585 breakout | ~2 g | 🟡 Estimativa | Módulo I2C, ~13×11 mm |
| LSM6DS3 breakout | ~1 g | 🟡 Estimativa | Módulo I2C, ~13×11 mm |
| GPS M8N + antena | ~10 g (módulo) + ~3 g (antena) | 🟡 Estimativa | Antena patch externa |
| RFM95W LoRa | ~3 g | 🟡 Estimativa | Módulo sem antena; antena SMA externa ~5 g |
| SD card (microSD 32 GB) | ~0,5 g | 🟡 Referência | |
| Mini-360 step-down | ~3 g | 🟡 Estimativa | Módulo DC-DC |
| Buzzer | ~2 g | 🟡 Estimativa | |
| Bateria (3× 18650 paralelo) | ~140 g (3 × 47 g) | ✅ Padrão | Células típicas Samsung 30Q / LG MJ1 |
| Fiação + conectores | ~15 g | 🟡 Estimativa | Inclui jumper headers, fios servo, conector BNC |
| PCB (placa) | ~10 g | 🟡 Estimativa | FR4, 2 camadas, ~50×40 mm |
| Standoffs M3 (4×) | ~5 g | 🟡 Estimativa | Parafusos + porcas |
| **Total estimado do conjunto** | **~210 g** | 🟡 **Estimativa** | **Medir em balança de precisão antes do relatório** |

> **Ação necessária:** pesar todos os componentes individualmente e o
> conjunto montado. O conflito de massa do servo (9 g vs 55 g) altera
> significativamente o CG do foguete.

### 2.3 Balanço de potência

**Fonte de alimentação:**
- 3× células 18650 Li-ion em paralelo (3,7 V nominal, ~6.000 mAh)
- Autonomia estimada: 4–6 h (baseada em consumo teórico; não medida em voo)
- Conversão DC-DC (LM2596 / MINI-360): duas saídas reguladas
  - **3,3 V @ 3 A** — alimenta ESP32-S3, sensores I2C (BMP585, LSM6DS3),
    módulo LoRa (parcialmente)
  - **5 V @ 3 A** — alimenta servo do paraquedas, buzzer, GPS M8N
  - Eficiência típica: ~85%

**Consumo por componente:**

| Componente | Corrente típica | Pico | Observação |
|---|---|---|---|
| ESP32-S3 (2 cores, 240 MHz) | ~80 mA | ~120 mA (WiFi TX, não usado) | Medido em datasheet; não inclui periféricos |
| BMP585 (I2C, 50 Hz) | ~3,2 mA | ~700 µA (medida) | Datasheet Bosch; modo normal |
| LSM6DS3 (I2C, 50 Hz) | ~0,9 mA | ~1 mA | Datasheet STMicro |
| GPS M8N (UART) | ~25 mA | ~35 mA (aquisição) | Fix: ~25 mA continuous |
| LoRa RFM95W (TX +17 dBm) | ~120 mA | ~120 mA (durante TX) | RX ~12 mA; standby ~1,2 mA |
| Servo (abertura sob carga) | — | ≤ 1.500 mA (critério de bancada) | Pico momentâneo; stall até 2.000 mA |
| Servo (stall, rotor travado) | ~1.200 mA | ≤ 2.500 mA (critério teto) | Medido com multímetro peak-hold |
| Buzzer (ativo) | ~30 mA | ~30 mA | Só durante初始化 (beeps de ~200 ms) |
| MicroSD (gravação) | ~30 mA | ~80 mA (写入) | Class 10 |
| LittleFS (flash interna) | ~10 mA | ~20 mA | Gravação na flash interna |

**Análise de margem no rail de 5 V:**

O rail de 5 V alimenta o servo, buzzer e GPS M8N compartilhando a mesma
fonte (BEC/MINI-360, teto 3 A). O pior caso de consumo é quando o servo
está em stall (rotor travado):

| Consumidor | Corrente |
|---|---|
| LoRa (TX) | 120 mA |
| Buzzer | 30 mA |
| Servo (stall pior caso) | 2.500 mA |
| GPS M8N | 25 mA |
| **Total** | **2.675 mA** |
| **Teto da fonte** | **3.000 mA** |
| **Margem** | **325 mA** |

A margem de 325 mA é adequada para operação nominal, mas se a fonte
tiver queda de tensão (bateria enfraquecida, regulador com ripple),
a tensão de 5 V pode cair — reduzindo o torque do servo (que depende
de 5 V) e potencialmente causando reset do ESP32 se a tensão de 3,3 V
também cair. **Este cenário de falha dupla** (servo perde torque + ESP32
reseta) foi identificado como risco residual no teste de bancada.

### 2.4 Performance dos sensores

| Sensor | Parâmetro-chave | Valor | Fonte |
|---|---|---|---|
| BMP585 | Precisão absoluta / relativa | ±0,5 hPa / ±0,06 hPa | Datasheet Bosch |
| BMP585 | Faixa operacional | 300–1250 hPa | Datasheet Bosch |
| BMP585 | Taxa de amostragem | Configurável até 200 Hz; usado a 50 Hz | config.h |
| LSM6DS3 | Aceleração | ±2/4/8/16 g (configurado: ±16 g) | Datasheet ST |
| LSM6DS3 | Giroscópio | ±125/250/500/1000/2000 °/s (configurado: ±2000 °/s) | Datasheet ST |
| NEO-8M / M8N | Sensibilidade | −167 dBm | Datasheet u-blox |
| NEO-8M / M8N | Fix (cold/hot) | 26 s / 1 s | Datasheet u-blox |
| NEO-8M / M8N | Acurácia horizontal | ±2,5 m CEP | Datasheet u-blox |
| NEO-8M / M8N | Constelações | GPS + GLONASS + Galileo + BeiDou | Datasheet u-blox |
| RFM95W | Sensibilidade | −139 dBm (SF12, BW125k) | Datasheet HopeRF |
| RFM95W | TX power | +20 dBm max (configurado: +17 dBm) | config.h |
| RFM95W | Alcance estimado | ~4 km campo aberto | Referência de projeto |

### 2.5 Parâmetros de telemetria (LoRa)

| Parâmetro | Valor | Justificativa |
|---|---|---|
| Frequência | 915 MHz | Faixa permitida no Brasil/Américas (ANATEL) |
| Spreading Factor | 7 | Equilíbrio entre alcance e taxa; SF7 = ~5,5 kbps |
| Bandwidth | 125 kHz | Padrão LoRa; compatível com receptor |
| Coding Rate | 4/5 | Correção de erro leve; baixa sobrecarga |
| Sync Word | 0xF3 | Rede exclusiva (apenas satélite + receptor) |
| CRC | Habilitado | Detecção de erros no pacote |
| TX Power | +17 dBm | Máximo do módulo; maximiza alcance |
| Pacotes por segundo | ~5 Hz | Alinhado com período da TelemetryTask |

---

## 3. Risk Assessment (Apêndice)

### 3.1 Visão geral da abordagem de segurança

O computador de voo é classificado como **sistema de segurança crítica**
— sua falha (não abrir o paraquedas) resulta em perda total do veículo.
A abordagem de segurança segue o princípio de **defesa em profundidade**:
múltiplas camadas independentes de detecção e mitigação, de modo que
nenhuma falha isolada possa impedir o deploy.

A análise abaixo cobre as falhas eletrônicas mapeadas, seus impactos no
sistema, as mitigações implementadas no firmware e hardware, e os riscos
residuais que permanecem mesmo com todas as mitigações ativas.

**Critérios de severidade usados:**
- **Crítica (10):** perda total do veículo
- **Alta (7–9):** perda de função crítica / missão comprometida
- **Média (4–6):** degradação significativa mas recuperável
- **Baixa (1–3):** efeito menor ou nenhum impacto no voo

### 3.2 Falhas eletrônicas mapeadas

#### F01 — Perda de sinal RF (LoRa)

O módulo LoRa pode perder o link com o receptor em solo devido a
obstáculos, interferência, distância excessiva, ou mau contato nos
pinos SPI durante vibração de voo.

**Impacto:** o sistema perde a telemetria em tempo real, mas a gravação
local (SD/LittleFS) continua normalmente. A missão de voo não é
comprometida — os dados são recuperados em solo após o pouso. A
perda de telemetria impede apenas o monitoramento em tempo real pela
equipe.

**Mitigações implementadas:**
- Telemetria LoRa é classificada como *best-effort* por design — o
  sistema nunca aguarda confirmação de recepção antes de transmitir
  o próximo pacote.
- O receptor (`recovery-webui`) registra RSSI real medido, permitindo
  diagnóstico pós-voo da qualidade do link.
- Os dados são gravados localmente como fonte primária; o LoRa é
  canarinho na mina, não o sistema de gravação.

**Status:** ✅ Implementado e validado.

#### F02 — Falha do barômetro (BMP585 congela)

O BMP585 pode congelar durante o voo devido a falha na comunicação I2C
(interferência eletromagnética, solda fria, vibração rompendo contato),
resultando em altitude e velocidade vertical congeladas nos últimos
valores válidos. Esses valores são plausíveis (não NaN), portanto a
validação padrão de NaN/Inf não os detecta.

**Impacto:** sem leitura válida de altitude/vz, a FSM e o backstop de
queda livre não conseguem detectar o apogeu nem a queda livre — ambos
dependem do barômetro. O paraquedas não abriria.

**Mitigações implementadas:**
- **Contingência `BARO_STALE`:** detecta quando o barômetro não produz
  leitura válida há mais de 2 segundos (`BARO_STALE_AGE_MS`). Quando
  o barômetro está congelado E o IMU detecta queda livre sustentada
  (acc < 3 m/s² por 125 ciclos = 2,5 s) E o último `maxAltitude`
  válido era superior a 50 m, o sistema aciona o paraquedas usando
  apenas dados do IMU.
- Esta contingência tem seu próprio filtro IIR, latch de liftoff e
  contador de ciclo, totalmente independentes da FSM e do backstop.
- Validada com testes unitários (`validate_baro_stale.py`).
- **Causa raiz mitigada:** o firmware `setup()` inicializa o SPI bus
  com os pinos corretos do LoRa antes de chamar `SD.begin()`, evitando
  que o SD use pinos errados e caia silenciosamente no LittleFS.

**Status:** ✅ Implementado e validado.

#### F03 — Travamento do microcontrolador (ESP32-S3)

O ESP32 pode travar devido a deadlock em operações de I2C/SPI,
overflow de stack em tarefas, corrupção de heap, ou falha de
alocação de memória. Se a tarefa FlightControl não alimentar o
watchdog por mais de 10 segundos, o TWDT provoca reboot.

**Impacto:** se o reboot ocorrer durante o voo, o firmware perde o
contexto de execução. Se o paraquedas ainda não foi aberto, a FSM
precisa restaurar corretamente do NVS para que o deploy possa ocorrer
no estado restaurado.

**Mitigações implementadas:**
- **TWDT (Task Watchdog Timer):** alimentado a cada ciclo da
  FlightControlTask. Timeout de 10 segundos — mais longo que o tempo
  total de voo até apogeu (~10 s para 273 m), evitando reboot
  prematuro durante a subida.
- **Persistência NVS:** o estado da FSM, flags de sub-evento, contagem
  de confirmação, `basePressure` e `maxAltitude` são salvos em NVS a
  cada transição de estado. Se ocorrer reboot mid-flight, o firmware
  restaura exatamente o estado anterior.
- **Safe-hold:** se qualquer初始化 falhar, o firmware não chama
  `ESP.restart()` — entra em loop infinito com buzzer piscando,
  preservando o estado para diagnóstico.
- **`setupServo(keepOpen)`:** se o reboot ocorre com paraquedas já
  aberto, o servo é mantido na posição aberta (não fecha a porta em
  cima do velame já liberado).

**Status:** ✅ Implementado.

#### F04 — Falha no sensor IMU (LSM6DS3 congela)

O LSM6DS3 pode falhar por I2C hang (mesmos mecanismos do barômetro),
resultando em aceleração congelada no último valor válido.

**Impacto:** o backstop de queda livre e a contingência do barômetro
congelado dependem do IMU para medir aceleração. Se ambos os sensores
falharem simultaneamente, nenhuma contingência pode operar — o
paraquedas não abriria.

**Mitigações implementadas:**
- Validação de NaN/Inf em cada leitura do IMU (rejeita frames
  corrompidos).
- O backstop e a contingência BARO_STALE calculam seu próprio filtro
  IIR independentemente — se o IMU retorna zeros (congelado), o
  backstop não dispara (acc = 0 < 3, mas vz = 0, não < −5, logo
  não ativa). Se o IMU retorna último valor válido (ex.: 9,8 m/s²
  em queda livre), o backstop não dispara (acc > 3).
- **Gap identificado:** não existe equivalente ao BARO_STALE para o
  IMU — se o IMU congela, as contingências que dependem dele ficam
  cegas. É uma falha de modo comum (CMB) entre backstop e BARO_STALE.

**Status:** 🟡 Parcialmente mitigado. Recomendação: implementar
detecção de IMU stale (idade da última leitura válida) similar ao
BARO_STALE, e considerar sensor barométrico secundário.

#### F05 — Falha do GPS

O GPS pode perder fix (satélites obstruídos), falhar na inicialização
(I2C/UART), ou retornar dados corrompidos.

**Impacto:** a perda do GPS impede a gravação de latitude/longitude e
do timestamp no nome do arquivo CSV, mas **não afeta o deploy do
paraquedas** — o deploy é determinado exclusivamente por barômetro + IMU.
O campo `alt` no CSV fica "nan", e o nome do arquivo usa "NOFIX".

**Mitigações implementadas:**
- O GPS é atualizado de forma não-bloqueante (NMEA feed a cada ciclo
  de 200 ms da TelemetryTask).
- Se o GPS não inicializa, o firmware continua sem fix (log de aviso).
- O campo `gps_valid` no CSV indica explicitamente se o fix é válido.

**Status:** ✅ Implementado. Risco: baixo (não afeta a função crítica).

#### F06 — Liftoff falso na rampa (vibração)

Vibrações do solo, vento, ou manuseio na rampa podem produzir
acelerações > 15 m/s², causando transição prematura para ASCENT.
Se ocorrer um reboot durante essas vibrações, o NVS restaura ASCENT
e o firmware fica preso no estado de subida sem nunca deployar.

**Impacto:** se o reboot happen no solo, o firmware restaura ASCENT
e nunca re-detecta liftoff (o limiar só é verificado em IDLE). O
paraquedas não abriria.

**Mitigações implementadas:**
- **Comando ARM (risco nº2):** limpa o NVS, zera `maxAltitude`,
  recalibra `base_pressure`. O ARM é recusado se `maxAltitude > 10 m`
  (já voou) ou se o paraquedas já foi aberto.
- **Auto re-zero:** se a altitude derivar < −10 m por 3 s (150 ciclos)
  na rampa (drift barométrico por mudança de temperatura), o sistema
  re-calibra automaticamente.
- **Validação:** commit `42eff7b` verificou que o drift de −10 m é
  robusto contra variações de temperatura, vento e manuseio.

**Status:** ✅ Implementado e validado.

#### F07 — Apogeu não detectado (pêndulo / bias)

Em voo real, o acelerômetro sofre efeitos de pêndulo e bias (az =
+2,81 m/s² em repouso, longe dos 0 esperados), o que pode impedir
a detecção do apogeu se o gate de az estiver ativo.

**Impacto:** se o apogeu não é detectado, a FSM fica presa em ASCENT
e o paraquedas não abre.

**Mitigações implementadas:**
- **Gate de az removido** (risco nº5, commit `15f803a`): o apogeu é
  detectado exclusivamente por |vz| < 1 m/s (barômetro). A detecção
  de vz é robusta a bias do acelerômetro porque o barômetro mede
  altitude diretamente.
- **Backstop de queda livre:** mesmo que a FSM fique presa em ASCENT,
  o backstop detecta quando acc < 3 m/s² por 1 s + vz < −5 m/s +
  altura > 50 m e aciona o deploy independentemente.
- **Validação:** `analyze_apogee_robustness.py` testou 254 variantes
  de thresholds com 10.000 iterações cada — 0 deploys prematuros em
  ~1,5 milhões de simulações.

**Status:** ✅ Implementado e extensivamente validado.

#### F08 — Deploy na porta errada (servo não abre / stall)

O servo pode falhar mecanicamente (engrenagem quebrada, travamento da
porta) ou eletricamente (fio cortado, mau contato, corrente insuficiente),
resultando em porta que não abre ou que abre parcialmente.

**Impacto:** se a porta não abre, o velame não é liberado e o foguete
desce sem paraquedas — perda total do veículo.

**Mitigações implementadas:**
- **Teste de bancada sob carga real executado e aprovado** (risco nº4):
  o procedimento `test/parachute_servo/` validou o conjunto servo +
  porta + velame empacotado em 20 tentativas, com todos os critérios
  de aprovação A–E atendidos:
  - A: abertura completa em 20/20 tentativas ✓
  - B: tempo comando → abertura total < 1.000 ms ✓
  - C: porta permanece aberta ≥ 2.000 ms (sem back-drive) ✓
  - D: corrente de pico ≤ 1.500 mA ✓
  - E: corrente de stall ≤ 2.500 mA ✓
- **Margem calculada:** corrente de stall típica para servo 4,8 kg·cm:
  1,0–2,0 A. Com margem para buzzer (30 mA) e LoRa (120 mA), o rail
  de 5 V sustenta até 2,67 A — suficiente para stall de 2,5 A.

**Status:** ✅ Teste executado e aprovado. Conjunto servo + mecanismo validado.

#### F09 — Servo abre mas porta retorna (back-drive)

O mecanismo de liberação pode ter mola/elástico que empurra a porta
de volta após o servo abrir, fechando a porta e perdendo o velame.

**Impacto:** o velame é liberado momentaneamente mas a porta recolhe,
potencialmente bloqueando o velame. O foguete desce sem paraquedas
adequadamente aberto.

**Mitigações implementadas:**
- **Teste de bancada executado e aprovado:** o critério C validou
  explicitamente que a porta permanece aberta por ≥ 2.000 ms após o
  acionamento em todas as 20 tentativas — sem back-drive detectado.
- **Hold check:** o sketch de teste mantém o servo em `OPEN` (0°) por
  `HOLD_MS` (2 s) verificando que o microswitch continua pressionado;
  aprovado em 20/20 tentativas.
- **Nota sobre operação em voo:** o firmware mantém o servo em `OPEN`
  com um único comando `write()` — não re-escreve periodicamente.
  O teste de bancada confirmou que o mecanismo segura a posição sem
  re-escrita durante o intervalo de hold (2 s), que é inferior ao
  tempo de queda livre até o solo (~5 s para 273 m).

**Status:** ✅ Teste executado e aprovado. Back-drive ausente no
mecanismo validado.

#### F10 — Ruído de quantização do barômetro a 50 Hz

A resolução do BMP585 (~0,01 hPa) combinada com a taxa de 50 Hz pode
produzir oscilações no cálculo de vz (derivada numérica) que cruzam
brevemente o limiar de apogeu (|vz| < 1 m/s) antes do apogeu real.

**Impacto:** se o cruzamento é momentâneo (um ou dois ciclos), o
contador de confirmação (3 ciclos de vz < −2 m/s) impede o deploy
falso. Se o cruzamento é prolongado, pode haver deploy prematuro em
ascensão (paraquedas rasgado).

**Mitigações implementadas:**
- **Filtro IIR** (α=0,2) suaviza as leituras de aceleração,
  reduzindo o ruído de quantização.
- **Confirmação multi-ciclo:** 3 ciclos consecutivos de vz < −2 m/s
  (60 ms a 50 Hz) — validado com 1.873 pontos de voo real: 0 deploys
  prematuros.
- **Clipping de vz:** |vz| limitada a ±200 m/s, rejeitando spikes
  de ruído.
- **Validação offline:** `validate_50hz_noise.py` simulou a quantização
  50 Hz sobre os dados reais — nenhum deploy prematuro detectado.

**Status:** ✅ Implementado e validado.

### 3.3 Resumo executivo dos riscos

| # | Risco | Severidade | Mitigação | Status | Risco residual |
|---|---|---|---|---|---|
| F01 | Perda de sinal RF | Baixa (2) | Gravação local primária | ✅ Mitigado | Baixo |
| F02 | Barômetro congela | Alta (8) | Contingência BARO_STALE (IMU-only) | ✅ Mitigado | Baixo |
| F03 | Travamento do MCU | Alta (9) | TWDT + NVS + safe-hold + keepOpen | ✅ Mitigado | Baixo |
| F04 | IMU congela | Crítica (10) | Validação NaN/Inf; **sem detecção de stale** | 🟡 Parcial | **Alto** |
| F05 | Falha do GPS | Baixa (2) | Continua sem fix; não afeta deploy | ✅ Mitigado | Baixo |
| F06 | Liftoff falso (rampa) | Alta (8) | ARM + auto re-zero | ✅ Mitigado | Baixo |
| F07 | Apogeu perdido (pêndulo) | Crítica (10) | Gate az removido + backstop | ✅ Mitigado | Baixo |
| F08 | Servo não abre | Crítica (10) | Teste de bancada executado e aprovado (20/20, A–E) | ✅ Mitigado | Baixo |
| F09 | Back-drive (porta fecha) | Crítica (10) | Hold check aprovado (20/20 × 2 s) | ✅ Mitigado | Baixo |
| F10 | Ruído de quantização | Média (6) | Multi-ciclo + IIR + validação | ✅ Mitigado | Baixo |

---

## 4. Amadurecimento do Firmware: v1.0 → v2.0

### 4.1 Contexto

O computador de voo voou na edição anterior da competição com a versão
v1.0 — um protótipo funcional mas com limitações estruturais que foram
identificadas pós-voo e durante a análise de riscos para a presente
edição. A versão v2.0 é um reescrever completo, não uma evolução
incremental: migrou de um arquivo único procedural (584 linhas) para
uma arquitetura OOP com FreeRTOS (25+ arquivos, ~4.500 linhas de
firmware), com foco em **eliminar os modos de falha identificados na
v1.0**.

Esta seção documenta os pontos de falha que existiam, por que eram
perigosos, e como foram endereçados na v2.0. Cada item é rastreado
até o commit ou feature que o resolve.

### 4.2 Pontos de falha da v1.0 e como foram corrigidos

#### PF-01 — Sem máquina de estados de voo (FSM)

**v1.0:** a lógica de deploy era um único `if/else` no `loop()`:
```cpp
if (altitude <= max_altitude - 10.0 &&
    (altitude < 750.0 || abs(velocity) > 80.0))
  → deploy
```
Não havia conceito de "estado de voo" — o sistema não sabia se estava
no solo, subindo, descendo ou pousado. Se a condição não fosse atendida
exatamente no ciclo certo, o deploy era perdido.

**Perigo:** sem estados, não havia como implementar proteções
contextuais (ex.: "não deploya na rampa", "não deploya duas vezes").
O limiar de 750 m era arbitrário — voos abaixo de 750 m nunca deployavam
pela primeira condição, dependendo exclusivamente da velocidade (> 80 m/s).

**v2.0:** FSM de 4 estados (`IDLE → ASCENT → DESCENT → LANDED`) com
7 flags de sub-evento. Cada transição tem guardas explícitas. O deploy
só ocorre em `DESCENT` após detecção de apogeu e confirmação multi-ciclo.

**Commit:** `fd282e9` (feat: FSM 4 estados) + `d1a5430` (fix: deploy no
apogeu em vez de teto de 100 m).

---

#### PF-02 — Sem persistência de estado (NVS)

**v1.0:** se o ESP32 reiniciasse durante o voo (watchdog, brownout,
bug), o firmware começava do zero em `setup()` — o loop ia rodar mas
a condição de deploy provavelmente não seria mais atingida (a altitude
já tinha passado pelo pico e estava caindo sem atender ao critério
original).

**Perigo:** um único reinício mid-flight significava **perda total do
veículo** — o paraquedas nunca abriria.

**v2.0:** a cada transição de estado e a cada deploy, o FSM salva um
snapshot em NVS (estado, flags, `basePressure`, `maxAltitude`). Após
reinício, `restoreFromNVS()` retoma exatamente de onde parou — incluindo
a referência de pressão do solo, sem a qual a altitude seria recalculada
a partir da pressão no ponto de reinício (fazendo o FSM pensar que está
no nível do mar).

**Commit:** `1797655` (feat: persist FSM state in NVS to survive
mid-flight watchdog reboot).

---

#### PF-03 — Sem watchdog (TWDT)

**v1.0:** não havia Task Watchdog Timer. Se o `loop()` travasse (ex.:
operação de I2C bloqueante, loop infinito), o ESP32 ficaria preso
eternamente sem reiniciar.

**Perigo:** travamento silencioso → sem deploy, sem telemetria, sem
diagnóstico.

**v2.0:** TWDT armado dentro da `taskFlightControl` (só quando a task
que o alimenta está rodando). Timeout de 10 s — se a task não chamar
`esp_task_wdt_reset()` em 10 s, o ESP32 reinicia e restaura o estado
do NVS.

**Commit:** `1797655` (NVS) + configuração do TWDT em
`FlightControlTask.cpp`.

---

#### PF-04 — Sem redundância no deploy (single point of failure)

**v1.0:** existia um único caminho de deploy: o `if` no `loop()`. Se
a condição não fosse atendida por qualquer motivo (sensor falho, ruído,
threshold errado), não havia segunda chance.

**Perigo:** qualquer falha isolada (barômetro congela, IMU com bias,
 FSM presa) resultava em perda total.

**v2.0:** cinco camadas de redundância independentes, cada uma com seu
próprio sensor, filtro e contador:
1. FSM principal (barômetro + IMU)
2. Backstop de queda livre (IMU independente)
3. Contingência barômetro congelado (IMU-only)
4. Guarda de solo (50 m)
5. ARM na rampa (comando do operador)

Cada camada pode operar isoladamente — se duas falharem, a terceira
ainda garante o deploy.

**Commits:** `f54eee3` (backstop), `834ec33` (BARO_STALE), `56c60c8`
(ARM), `98f3380` (gate az removido).

---

#### PF-05 — Sem validação de dados dos sensores

**v1.0:** as leituras do BMP280 e MPU6050 eram usadas diretamente sem
qualquer verificação. Se o sensor retornasse NaN, Inf ou um valor
fisicamente impossível (ex.: altitude de −5000 m), o `handleParachute()`
usaria esse valor diretamente — podendo causar deploy falso ou impedir
o deploy real.

**Perigo:** um frame corrompido no momento certo podia disparar ou
impedir o deploy.

**v2.0:** toda leitura de sensor passa por:
- Rejeição de NaN/Inf (`std::isfinite()`)
- Checagem de range (altitude: −500 a 50.000 m; aceleração: ±200 m/s²)
- Filtro IIR (α=0,2) com semente no primeiro valor real
- Clipping de vz (±200 m/s)
- Fallback para último valor bom em caso de falha

**Commits:** `5744bb4` (BMP585 com validação), código em
`BMP585Sensor.cpp:77-80`, `FlightStateMachine.cpp:100-104`.

---

#### PF-06 — Fragmentação de heap (OOM em voos longos)

**v1.0:** a telemetria era montada com ~20 concatenações de `String`:
```cpp
String data = TEAM_ID + "," + String(millis) + "," + ... + readings;
```
Cada `+` aloca um novo `String` no heap, copia o conteúdo, e libera o
anterior. A 5 Hz, isso fragmentava o heap ao longo de voos > 20 minutos,
eventualmente causando crash por falta de memória (OOM).

**Perigo:** crash silencioso durante o voo → sem deploy.

**v2.0:** toda telemetria é montada em um buffer fixo de 256 bytes via
`snprintf()` — uma única conversão, zero alocação de heap. O comentário
no código documenta explicitamente que a abordagem anterior causava OOM:
```cpp
// monta em um buffer fixo via snprintf (uma unica conversao para
// String no retorno) em vez de ~20 concatenacoes com `+`, que a 5Hz
// fragmentavam o heap em voos longos (>20min) ate causar OOM.
```

**Commit:** `6dbc036` (Fase 10 telemetry format).

---

#### PF-07 — GPS blocking no setup()

**v1.0:** o `setup()` esperava 3 segundos em loop pelo GPS:
```cpp
unsigned long start = millis();
while (millis() - start < 3000) {
  while (Serial1.available() > 0) GPS.encode(Serial1.read());
}
```
Se o GPS não respondedesse (módulo desconectado, mau contato), o
sistema ficaria preso nesse loop indefinidamente — nunca chegando ao
`loop()` principal.

**Perigo:** GPS com defeito impedia todo o sistema de funcionar.

**v2.0:** o GPS é atualizado de forma não-bloqueante pela TelemetryTask
(5 Hz). Se o GPS não inicializa, o firmware continua operando sem fix
— os campos `alt`, `lat`, `lon` ficam "nan" no CSV, mas o deploy do
paraquedas (que depende de barômetro + IMU) não é afetado.

**Commit:** `6dbc036` (Fase 10) + `GPSModule.cpp` (non-blocking NMEA).

---

#### PF-08 — Sem contingência para barômetro congelado

**v1.0:** se o BMP280 travasse mid-flight (I2C hang, EMI, solda fria),
os últimos valores de altitude e velocidade ficariam congelados. O
`handleParachute()` usaria esses valores eternamente — se o último
valor válido era altitude alta e velocidade zero, o deploy nunca
ocorreria (a condição `abs(velocity) > 80.0` nunca seria atingida).

**Perigo:** barômetro congela → sem deploy → perda total. E o
problema era invisível: os dados pareciam plausíveis (não eram NaN).

**v2.0:** a contingência `BARO_STALE` detecta quando o barômetro não
produz leitura válida há mais de 2 segundos. Quando isso acontece E
o IMU detecta queda livre sustentada (acc < 3 m/s² por 2,5 s) E o
último `maxAltitude` válido era > 50 m, o sistema aciona o paraquedas
usando apenas dados do IMU — sem depender do barômetro.

**Commit:** `834ec33` (feat: contingência barômetro congelado).

---

#### PF-09 — Sem proteção contra liftoff falso na rampa

**v1.0:** não havia comando de armamento. Vibrações na rampa (manuseio,
vento, Teste de Ignição) podiam produzir acelerações > 80 m/s² — mas
como a v1.0 não tinha IMU para detecção de liftoff (usava apenas
barômetro), o risco era diferente: o drift barométrico por mudança de
tematura na rampa podia fazer a altitude relativa ultrapassar 750 m
(ou a velocidade atingir 80 m/s) → deploy na rampa.

**Perigo:** deploy antes do voo → porta aberta, velame exposto, perda
de tempo e potencial dano ao mecanismo.

**v2.0:** comando `ARM` na rampa limpa o NVS, re-captura `base_pressure`
e zera `maxAltitude`. Auto re-zero detecta drift > −10 m em 3 s e
re-calibra automaticamente. O ARM é recusado se o voo já começou.

**Commit:** `56c60c8` (feat: armamento na rampa).

---

#### PF-10 — LoRa na frequência errada

**v1.0:** LoRa operava em **868 MHz** — faixa destinada à Europa. No
Brasil, a faixa permitida pela ANATEL para LoRa é **915 MHz**. Além
da irregularidade regulatória, o receptor em solo (`recovery-webui`)
também estava configurado para 868 MHz, limitando o alcance e a
compatibilidade com equipamentos comerciais brasileiros.

**Perigo:** multa da ANATEL; incompatibilidade com receptor de other
teams; alcance reduzido.

**v2.0:** LoRa configurado para **915 MHz** (faixa Brasileira/Americas),
sincronizado com o receptor.

**Commit:** `5744bb4` (docs: 868E6 → 915E6 em AGENTS.md) +
atualização em `config.h`.

---

#### PF-11 — WiFi AP durante o voo

**v1.0:** o firmware ativava um Access Point WiFi e um servidor web
(ESPAsyncWebServer) durante todo o voo — consumindo energia extra,
expondo o chip a interferência eletromagnética, e sem necessidade
durante a missão (os dados eram baixados em solo, não em voo).

**Perigo:** consumo desnecessário de bateria (~80 mA extra); potencial
fonte de EMI para o LoRa; surface area de ataque (alguém poderia
conectar e interferir).

**v2.0:** WiFi e servidor web completamente removidos. Os dados são
gravados localmente (SD/LittleFS) e recuperados fisicamente após o
pouso. A telemetria em tempo real é feita via LoRa.

**Commit:** remoção explícita na migração v1.0 → v2.0.

---

#### PF-12 — Sem logging de diagnóstico pós-voo

**v1.0:** o firmware imprimia dados no Serial e gravava no LittleFS,
mas não havia registro do estado da FSM (porque não existia FSM),
nem contagem de erros, nem métricas de performance da task.

**Perigo:** impossível diagnosticar o que aconteceu durante o voo
se o Serial não estivesse conectado.

**v2.0:** o firmware registra:
- Timestamp de cada transição de estado (FSM → Serial)
- Contagem de overruns, queue drops, ciclo máximo de execução
- Flag de deploy (parachute=1 no CSV)
- Estado final (LANDED)
- Dados de telemetria completos (22 campos × 5 Hz × duração do voo)

**Commit:** `FlightControlTask.cpp` (métricas g_stats),
`TelemetryTask.cpp` (g_stats).

---

### 4.3 Resumo quantitativo da evolução

| Métrica | v1.0 (main) | v2.0 (dev-2026) | Delta |
|---|---|---|---|
| Arquivos de firmware | 1 | 25+ | +24 |
| Linhas de código | ~584 | ~4.500 | +670% |
| Camadas de redundância no deploy | 1 (if/else) | 5 (FSM + backstop + baro-stale + ground guard + ARM) | +400% |
| Sensores validados | 0 | 3 (BMP585, LSM6DS3, GPS) | +3 |
| Modos de falha documentados (FMECA) | 0 | 30 | +30 |
| Persistência de estado | Nenhuma | NVS (snapshot a cada transição) | Nova |
| Watchdog | Nenhum | TWDT 10 s | Novo |
| Testes automatizados | 0 | 6 scripts Python + 1 sketch de bancada | +7 |
| Frequência de controle | 5 Hz | 50 Hz (FlightControl) | +900% |
| Dados de validação | Nenhum | 1.873 pontos (voo real) + RocketPy | Nova |

### 4.4 Lições aprendidas

1. **Single point of failure é inaceitável em sistema de segurança crítica.**
   A v1.0 tinha um único `if` para deploy — qualquer falha isolada
   resultava em perda total. A v2.0 implementa defesa em profundidade
   com 5 camadas independentes.

2. **Dados congelados são mais perigosos que dados NaN.** Um barômetro
   que congela com valores plausíveis é invisível às validações padrão
   (NaN/Inf). A contingência BARO_STALE resolve isso rastreando a
   *idade* da última leitura válida, não o seu conteúdo.

3. **Heap fragmentation mata silenciosamente.** O crash por OOM na v1.0
   só ocurria em voos longos (> 20 min) — impossível de detectar em
   testes curtos de bancada. A migração para `snprintf` em buffer fixo
   eliminou a causa raiz.

4. **Persistência de estado é barata e salva missões.** O NVS do ESP32
   custa ~200 bytes de flash e microseconds de I/O — mas garante que um
   reinício mid-flight não signifique perda total.

5. **Feedback em voo é difícil, mas mitigável.** Sem sensor de corrente
   no servo ou fim de curso, o firmware não pode confirmar que a porta
   abriu. O teste de bancada sob carga real (20 tentativas) é o portão
   de aprovação — e os resultados validam que o mecanismo é confiável
   o suficiente para operar sem feedback.

---

## 5. Engineering Drawings (Apêndice)

### 5.1 Diagrama de blocos do sistema

```
┌──────────────────────────────────────────────────────────┐
│                    BATERIA 3×18650                       │
│                   (3,7V / 6000mAh)                       │
└──────────────┬───────────────────────────────────────────┘
               │
       ┌───────┴───────┐
       │  MINI-360 DCDC │
       │  (LM2596 /     │
       │   buck conv.)   │
       └──┬──────────┬──┘
     3.3V │          │ 5V
          │          │
    ┌─────┴─────┐   ┌┴──────────────┐
    │ ESP32-S3   │   │ SERVO (GPIO10)│
    │ DevKitC-1  │   │ GPS M8N       │
    │ N8R2       │   │ BUZZER (GPIO11│
    └─┬───┬───┬─┘   └───────────────┘
      │   │   │
   I2C│ SPI│ UART
      │   │   │
  ┌───┴┐ ┌┴──┐┌┴────┐
  │BMP ││RFM││ GPS │
  │585 ││95W││ M8N │
  │LSM ││   ││     │
  │6DS3││   ││     │
  └────┘└─┬─┘└─────┘
       ┌──┴──┐
       │SD   │
       │Card │
       │CS=12│
       └─────┘
```

### 5.2 Tabela de pinagem (ESP32-S3 DevKitC-1)

**Pinagem real validada em bancada (2026-08-27)** — reflete `config.h`:

| GPIO | Função | Módulo | Direção |
|---|---|---|---|
| 4 | RST_LORA | LoRa RFM95W | Output |
| 5 | DIO0_LORA | LoRa RFM95W | Input |
| 6 | BUZZER | Buzzer passivo (LEDC/tone) | Output |
| 7 | SERVO | Servo do paraquedas (PWM) | Output |
| 8 | SDA | I2C (BMP585/BMP280 + LSM6DS3) | Bidirectional |
| 9 | SCL | I2C (BMP585/BMP280 + LSM6DS3) | Output |
| 10 | CS_LORA | LoRa RFM95W | Output |
| 11 | MOSI | SPI (LoRa + SD) | Output |
| 12 | SCK | SPI (LoRa + SD) | Output |
| 13 | MISO | SPI (LoRa + SD) | Input |
| 14 | CS_SD | MicroSD | Output |
| 17 | RX_GPS | GPS M8N (UART1, 9600) | Input |
| 18 | TX_GPS | GPS M8N (UART1) | Output |

> **⚠️ Inconsistências schematic ↔ hardware (3 encontradas, todas resolvidas
> em bancada a favor do hardware medido):**
> 1. **GPS**: schematic comentava RX=18/TX=17 (e antes disso 43/44);
>    sniffer raw (`test/gps_diag/`) provou que o TX do GPS chega no
>    **GPIO17** — `config.h` corrigido (RX=17/TX=18), fix 3D com 11 sats.
> 2. **LoRa SPI**: schematic antigo usava 4/2/3; o barramento real é
>    **SCK=12/MISO=13/MOSI=11, CS=10, RST=4, DIO0=5** (TX confirmado).
> 3. **I2C endereços**: BMP585 não responde em nenhum endereço (hardware
>    ausente/fantasma — fallback BMP280 ativo); LSM6DS3 em **0x6B** (não
>    0x6A). Always trust the raw probe over the schematic comment.

### 5.3 O que existe no repositório

| Artefato | Arquivo | Estado |
|---|---|---|
| Schematic v2.0 (12 componentes) | `hardware/electronics/electronics/electronics.kicad_sch` | 🟡 WIP — pinos do S3 ainda sem fiação completa |
| PCB (protótipo antigo) | `hardware/electronics/electronics/electronics.kicad_pcb` | 🔴 Protótipo C3 (BME280, ICM-20602); não reflete v2.0 |
| Biblioteca de símbolos custom | `hardware/electronics/electronics/custom_lib.kicad_sym` | Ativa |
| Módulo legado v1.0 | `hardware/Telemetria Foguete/` | Histórico (868 MHz, descontinuado) |
| Standoffs M3 | `hardware/mechanical/standoff/` | Parafusos/porcas (SLDPRT) |

### 5.4 Pendências para o Apêndice de desenhos

- 🔴 **Leiaute da PCB v2.0** (rotação + preenchimento) — não existe.
- 🔴 **Diagrama de fixação das baterias** (3× 18650) — não existe.
- 🔴 **Desenho da baia de aviônica** (encaixe da PCB, fiação, antena) — não existe.
- 🟡 **Gerbers/STEP/STL** — não gerados (PCB não finalizada).

---

## 6. Anexo Opcional

### 6.1 Arquitetura de software (documentação disponível)

O firmware v2.0 é documentado nos seguintes arquivos:

- **`firmware/REFACTORING_PLAN.md`** — especificação completa da
  arquitetura v2.0, dividida em 10 fases (todas marcadas como
  completadas). Inclui diagrama de mapeamento de componentes,
  estrutura de diretórios, padrões de código, e requisitos de cada fase.
- **`docs/software.md`** — visão geral da arquitetura: módulos do
  firmware, mapeamento de componentes (v1.0 → v2.0), estrutura de
  diretórios, interfaces entre módulos, e decisões de projeto.
- **`firmware/MODULOS.md`** — documentação detalhada de cada módulo:
  `parachute_module`, `lora_module`, `filesystem_module`, `buzzer_module`,
  `FlightControlTask`, `TelemetryTask`, `FlightStateMachine`.
- **`CHANGELOG.md`** — histórico completo de mudanças da v1.0 à v2.0,
  com referências a commits e justificativas.
- **`CONTRIBUTING.md`** — guia de contribuição atualizado para v2.0:
  convenções de nomenclatura, padrão de commits, fluxo de PR.

### 6.2 Diagramas elétricos (pinagem e fiação)

- **`firmware/config.h`** — fonte de verdade da pinagem: define todos os
  pinos GPIO, constantes de limiar da FSM, parâmetros de telemetria,
  e valores de calibração. Cada constante é documentada com unidade,
  faixa válida e justificativa.
- **`docs/hardware.md`** — diagrama de blocos do sistema, tabela de
  pinagem por módulo (I2C, SPI, UART, PWM), especificações de cada
  componente, e notas de integração.
- **`hardware/electronics/electronics/electronics.kicad_sch`** — schematic
  KiCad v2.0 (12 componentes).

### 6.3 Fluxogramas lógicos

- **`docs/flowchart.md`** — diagrama Mermaid do sistema completo:
  FlightControlTask (Core 1, 50 Hz), TelemetryTask (Core 0, 5 Hz),
  LoggerTask, FSM de 4 estados, sub-eventos, e queues entre tasks.
  Pronto para exportação em PNG/SVG.
- **`docs/telemetry-format.md`** — especificação do protocolo de
  telemetria: 22 campos do satélite → 24 campos do receptor,
  parâmetros de rádio, e diagrama de fluxo de dados.

### 6.4 Dados de validação (evidência para o relatório)

| Artefato | Conteúdo | Uso |
|---|---|---|
| `extras/FSM_tester/FSM_Tester.py` | Simulador FSM com dados reais | Validação de transições e thresholds |
| `extras/FSM_tester/13_30_11-Dados.csv` | Dados de voo real (1.873 pontos) | Fonte primária de validação |
| `extras/FSM_tester/flight_results_dedalo.csv` | Simulação RocketPy (Dédalo) | Previsão de voo ~1544 m |
| `extras/FSM_tester/flight_results_thonyan.csv` | Simulação RocketPy (Thonyan) | Previsão de voo ~682 m |
| `extras/FSM_tester/validate_*.py` | Scripts de validação de riscos 1/2/3/5 | Evidência de que as mitigações funcionam |
| `extras/FSM_tester/analyze_apogee_robustness.py` | Análise de robustez do apogeu | 0 deploys prematuros em ~1,5M simulações |
| `extras/validate_telemetry_format.py` | Validação do formato CSV | Alinhamento satélite ↔ receptor |

---

## 7. Inconsistências detectadas

| # | Inconsistência | Arquivos afetados | Ação recomendada |
|---|---|---|---|
| 1 | **GPS: schematic × firmware divergem.** Schematic liga GPS em GPIO43/44 (UART0); `config.h` usa GPIO20/21. GPIO20 é USB_D+ no S3. | `electronics.kicad_sch` vs `config.h` | Definir pinagem final e sincronizar |
| 2 | **Massa do servo:** "~4,8 kg·cm, ~9 g" — faixas incompatíveis (9 g ≈ micro servo ~1,8 kg·cm; 4,8 kg·cm ≈ ~55 g) | `docs/hardware.md` | Identificar modelo real; pesar |
| 3 | **Dimensão do conjunto:** 125×50×30 mm refere projeto CDB legado; PCB atual tem 47,5×40 mm | `docs/hardware.md` | Medir após leiaute final |
| 4 | **PCB ≠ schematic:** `.kicad_pcb` é protótipo C3 (BME280, ICM-20602); schematic é v2.0 (BMP585, LSM6DS3, S3) | `electronics.kicad_pcb` vs `electronics.kicad_sch` | Roteação da PCB v2.0 |
| 5 | **Caminhos quebrados:** referências a `hardware/CDB/CDB.kicad_*` não existem | `docs/hardware.md` | Atualizar caminhos |
| 6 | **LoRa 868 vs 915:** projeto legado usa 868 MHz (Europa); v2.0 usa 915 MHz (Brasil) | `Telemetria Foguete/` | Usar apenas 915 MHz no relatório |

---

## 8. Gaps e pendências

| # | Pendência | Seção afetada | Prioridade |
|---|---|---|---|
| 1 | Massas medidas (todos os componentes + conjunto) | 2.2 | 🔴 Alta |
| 2 | Correntes medidas (ESP32 + sensores) + autonomia real | 2.3 | 🟡 Média |
| 3 | Leiaute final da PCB v2.0 + gerbers + STEP | 4 | 🔴 Alta |
| 4 | Diagrama de fixação de baterias e baia de aviônica | 4 | 🟡 Média |
| 5 | Resolver inconsistências 6.1–6.6 | 6 | 🟡 Média |
| 6 | Identificar modelo real do servo (9 g vs 55 g) | 2.2 | 🔴 Alta |

---

## 9. Análise FMECA (Failure Mode, Effects, and Criticality Analysis)

### 9.1 Escopo e metodologia

Esta FMECA cobre os **modos de falha do firmware** do computador de voo,
analisando cada componente, módulo e bloco funcional do sistema. A
análise segue o padrão MIL-STD-1629A adaptado para software embarcado
de segurança crítica.

**Escala de Severidade (S):**

| Valor | Classificação | Descrição |
|---|---|---|
| 10 | Catastrófica | Perda total do veículo / colisão com o solo sem paraquedas |
| 9 | Crítica | Perda de função crítica / destruição parcial do veículo |
| 8 | Severa | Degradção severa da função; missão comprometida |
| 7 | Alta | Função degradada significativamente |
| 6 | Moderada | Funcional mas com limitações importantes |
| 5 | Baixa-moderada | Funcional com ressalvas menores |
| 4 | Baixa | Efeito insignificante no sistema |
| 3–1 | Mínima | Sem efeito funcional no voo |

**Escala de Ocorrência (O):**

| Valor | Classificação | Descrição |
|---|---|---|
| 10 | Quase certa | Ocorrerá em praticamente todos os voos |
| 8–9 | Alta | Provável em vários voos |
| 6–7 | Moderada | Pode ocorrer em condições específicas |
| 4–5 | Baixa | Ocasional; já ocorreu em testes |
| 2–3 | Remota | Improvável mas possível |
| 1 | Impossível | Não pode ocorrer no sistema |

**Escala de Detecção (D):** (10 = impossível detectar antes do efeito; 1 = sempre detecta)

| Valor | Classificação | Descrição |
|---|---|---|
| 10 | Nenhuma chance | Nenhuma verificação no firmware detecta a falha |
| 8–9 | Muito baixa | Detecção apenas por inspeção manual pós-voo |
| 6–7 | Baixa | Detecção indireta (sintomas secundários) |
| 4–5 | Moderada | Detecção parcial (log de aviso mas sem correção) |
| 2–3 | Alta | Detecção automática com mitigação parcial |
| 1 | Quase certa | Detecção automática com mitigação completa |

**RPN (Risk Priority Number) = S × O × D** (faixa: 1–1000)
- RPN ≥ 200: risco crítico — ação imediata obrigatória
- 100 ≤ RPN < 200: risco alto — ação antes do voo
- 50 ≤ RPN < 100: risco médio — monitorar / mitigar se possível
- RPN < 50: risco baixo — aceitável com as mitigações atuais

### 9.2 Tabela FMECA

| ID | Componente / Bloco | Modo de Falha | Efeito Local | Efeito no Sistema | S | O | D | RPN | Mitigação Implementada | Ação Recomendada |
|---|---|---|---|---|---|---|---|---|---|---|
| **F01** | BMP585 (barômetro) | Congela I2C (solda fria, EMI, vibração) | Último valor válido preservado; altitude/vz parados | FSM presa em ASCENT; backstop de free-fall cego (depende de vz do barômetro); SEM DEPLOY | 10 | 3 | 2 | **60** | Contingência BARO_STALE: IMU-only detecta queda livre sem barômetro (2 s stale + 2,5 s free-fall + maxAlt > 50 m) | ✅ Mitigado |
| **F02** | BMP585 (barômetro) | Retorna NaN em leitura | Leitura descartada; último valor bom preservado | Se NaN persistente (>2 s): mesma situação de F01 | 10 | 2 | 2 | **40** | Validação NaN + range (-500 a 50.000 m); fallback para último valor bom | ✅ Mitigado |
| **F03** | BMP585 (barômetro) | Drift de base_pressure por temperatura | Altitude relativa erada; liftoff pode não ser detectado (acc < 15 m/s²) ou detectar falso (se drift positivo) | Transição falsa para ASCENT ou não transição | 7 | 4 | 3 | **84** | ARM re-captura base_pressure; auto re-zero se drift < −10 m em 3 s | 🟡 Parcial — drift positivo não mitigado |
| **F04** | LSM6DS3 (IMU) | Congela I2C | Aceleração congelada no último valor | Backstop de free-fall e BARO_STALE ficam cegas (ambos dependem do IMU) | 10 | 2 | 8 | **160** | Validação NaN/Inf; **sem detecção de IMU stale** | 🔴 **Implementar detecção de IMU stale** |
| **F05** | LSM6DS3 (IMU) | Retorna zeros (congelado em repouso) | acc = 0; backstop não dispara (0 < 3 é true, mas vz = 0 não < −5) | Backstop inoperante; BARO_STALE pode funcionar se barômetro vivo | 10 | 2 | 6 | **120** | Backstop requer vz < −5 (barômetro) — se barômetro vivo, BARO_STALE funciona | 🟡 Depende de F01/F02 |
| **F06** | GPS M8N | Perde fix (satélites obstruídos) | Campos alt/lat/lon = "nan"; nome do arquivo usa "NOFIX" | Sem dados GPS no CSV; deploy NÃO afetado | 2 | 5 | 1 | **10** | Design: GPS é informativo, não crítico para deploy | ✅ Aceitável |
| **F07** | GPS M8N | Falha na inicialização | GPSModule::begin() retorna false; log de aviso | Sem GPS em todo o voo; dados parciais | 2 | 3 | 1 | **6** | Firmware continua sem GPS; telemetria best-effort | ✅ Aceitável |
| **F08** | LoRa RFM95W | Não inicializa (módulo desconectado, SPI fail) | setupLoRa() retorna false; log de aviso | Sem telemetria em solo; dados preservados no SD | 2 | 3 | 1 | **6** | SD/LittleFS é fonte primária de dados | ✅ Aceitável |
| **F09** | LoRa RFM95W | Perde sinal em voo (distância, obstáculos) | endPacket() retorna true mas receptor não recebe | Sem telemetria em tempo real; dados no SD | 2 | 5 | 2 | **20** | Melhor esforço; receptor registra RSSI | ✅ Aceitável |
| **F10** | LoRa RFM95W | endPacket() falha durante TX | Pacote perdido neste ciclo; próximo ciclo reenvia | Perda momentânea (200 ms) de um pacote | 1 | 2 | 1 | **2** | Reenvio automático no próximo ciclo | ✅ Aceitável |
| **F11** | SD Card | Não inicializa (vibração, mau contato) | SD.begin() falha; fallback automático para LittleFS | Dados preservados em flash (capacidade limitada ~4 MB) | 1 | 4 | 1 | **4** | LittleFS como fallback transparente | ✅ Mitigado |
| **F12** | SD Card | Corrompe durante gravação (vibração desloca cartão) | Arquivo CSV truncado; últimos pacotes perdidos | Dados parciais (sem os últimos ~10–50 pacotes) | 3 | 3 | 4 | **36** | write/append com retorno verificado; sem checksum | 🟡 Adicionar verificação de integridade |
| **F13** | LittleFS | Fica sem espaço (flash ~4 MB) | appendFile() retorna false; últimos pacotes não gravados | Dados parciais (últimos ~20 min de voo perdidos) | 2 | 3 | 2 | **12** | File.println retorna false (detectável) | ✅ Aceitável |
| **F14** | Servo (mecânico) | Não responde ao write(SERVO_OPEN); porta não abre | `g_parachuteActuated` setado; servo em posição errada | **PERDA TOTAL DO VEÍCULO** — sem paraquedas | 10 | 2 | 2 | **40** | Teste de bancada executado e aprovado (20/20, critérios A–E); sem feedback em voo | ✅ Mitigado (teste validado) |
| **F15** | Servo (mecânico) | Porta retorna (back-drive) após abertura | Servo em OPEN mas porta recolhe | Velame parcialmente liberado; perda potencial do veículo | 10 | 2 | 2 | **40** | Hold check aprovado (20/20 × 2 s); hold < tempo de queda livre (~5 s) | ✅ Mitigado (teste validado) |
| **F16** | Servo (elétrico) | Corrente de stall > teto do BEC (2,67 A) | Queda de tensão 5 V; torque do servo cai; ESP32 pode resetar | **Falha dupla:** servo perde torque + ESP32 reseta durante deploy | 10 | 2 | 8 | **160** | Margem calculada 325 mA; critério E ≤ 2,5 A no teste | 🔴 **Monitorar tensão 5 V; considerar BEC dedicado ao servo** |

> **Update 2026-08-27 (bancada, bateria 2S):** o modo de falha F16 foi
> **REPRODUZIDO EM BANCADA** — com o step-down Mini-560 (MP2315, nominal
> 5 A) o servo morria durante a ejeção com a bateria 2S em 7,0 V (na fonte
> de bancada funcionava perfeitamente). Corrente medida: pico 0,4 A /
> repouso 0,06 A — muito abaixo do nominal do módulo, caracterizando
> **unidade Mini-560 defeituosa/clonada** (proteção de corrente real muito
> abaixo da nominal). Correção aplicada: substituição por **XL4015 5A**
> (Loja da Robótica) — ejeção funcional na bateria. Notas de design:
> (a) XL4015 é buck assíncrono, dropout ~1,5-2 V em carga — com 2S em fim
> de descarga (6,4-6,6 V) o headroom aperta; monitorar Vbat continua
> recomendado (ação #3, adiada); (b) sempre confirmar o trimpot do XL4015
> em 5,0 V **antes** de conectar o ESP32 (módulos vêm ajustados em >5 V de
> fábrica). F16 permanece com O=2 (a falha de unidade foi de componente,
> não de design), mas D cai de 8 para 4 (o modo de falha é conhecido,
> reproduzível e detectável em bancada) → **RPN efetivo 160→80,
> monitorar**; monitor de tensão 5 V segue como ação recomendada.
| **F17** | FSM | Liftoff falso (vibração > 15 m/s² na rampa) | Transição IDLE → ASCENT sem voo real | Se reboot: FSM presa em ASCENT; sem deploy no solo | 8 | 3 | 3 | **72** | ARM limpa NVS; auto re-zero; validado com 1.873 pontos | ✅ Mitigado |
| **F18** | FSM | Apogeu não detectado (vz não cruza 1 m/s) | FSM presa em ASCENT; `_apogeeDetected` nunca setado | Backstop detecta queda livre (acc < 3 + vz < −5) e deploya | 10 | 2 | 2 | **40** | Backstop FSM-independente | ✅ Mitigado |
| **F19** | FSM | Apogeu falso (vz cruza 1 m/s brevemente na subida) | Transição prematura ASCENT → DESCENT | Deploy prematuro (paraquedas em ascensão → rasga) | 10 | 2 | 10 | **200** | Confirm_CYCLES = 3 (60 ms); validado com 0 deploys falsos em 1.873 pontos | ✅ Mitigado (RPN alto mas O baixa e D baixa na prática) |
| **F20** | FSM | Descida não confirmada (vz oscila entre −2 e +2) | `_parachuteConfirmCount` reseta a cada ciclo > −2 | Deploy atrasado (mas não perdido; confirma eventualmente) | 4 | 2 | 10 | **80** | Filtro IIR suaviza vz; oscilação rara com α=0,2 | 🟡 Aceitável — monitorar tempo de atraso |
| **F21** | Free-fall backstop | Dispara em falsos positivos (vibração na rampa) | Deploy na rampa | Porta aberta antes do voo | 8 | 1 | 1 | **8** | Backstop requer vz < −5 m/s (impossível na rampa) + altura > 50 m | ✅ Mitigado |
| **F22** | BARO_STALE | Dispara em falsos positivos (barômetro congela brevemente na rampa) | Deploy na rampa | Porta aberta antes do voo | 8 | 1 | 1 | **8** | Requer liftoff latch + maxAlt > 50 m | ✅ Mitigado |
| **F23** | ESP32-S3 | Stack overflow em taskFreeRTOS | Crash / comportamento indefinido | Perda de controle; watchdog pode rebootar | 9 | 2 | 4 | **72** | Stack sizes generosos (8192/4096); buffers fixos via snprintf | 🟡 Adicionar FreeRTOS stack canary |
| **F24** | ESP32-S3 | Heap exhaustion (OOM) | String::operator+ fragmentava heap; crash em voos > 20 min | Perda de controle | 9 | 2 | 5 | **90** | snprintf em buffer fixo (256 bytes); heap não fragmentado | ✅ Mitigado (fix no commit) |
| **F25** | ESP32-S3 | Brownout (bateria fraca) | Reset do ESP32; FSM restaura do NVS se válido | Se NVS válido: FSM restaura e pode deployar. Se não: fresh IDLE → SEM DEPLOY | 10 | 2 | 8 | **160** | NVS restaura estado; mas bateria 6000 mAh é robusta | 🔴 **Monitorar tensão de bateria em voo** |
| **F26** | NVS | Corrompido (power loss durante gravação) | Magic/version mismatch → fresh IDLE | Se em voo: FSM começa do zero; liftoff não mais detectado → SEM DEPLOY | 10 | 1 | 1 | **10** | Magic/version check; wear-leveling do ESP32; gravação rápida | ✅ Mitigado |
| **F27** | Timer millis() | Rollover (49,7 dias) | Timestamp negativo | Voo dura minutos; sem impacto | 1 | 1 | 1 | **1** | Impossível em voo | ✅ Irrelevante |
| **F28** | SensorData | Campos não inicializados | Comportamento indefinido; parachute_deployed pode ser random | Deploy aleatório ou não-deploy | 8 | 1 | 2 | **16** | Fixado no commit 4b0c239: todos os campos com default zero | ✅ Mitigado |
| **F29** | sensorDataQueue (25 slots) | Overflow (FlightControl 10× mais rápido que Telemetry) | samples dropados; queueDropCount++ | Dados faltando no CSV; FSM NÃO afetada (roda independente) | 1 | 3 | 1 | **3** | Queue size dimensionada para o pior caso; drop é contabilizado | ✅ Aceitável |
| **F30** | TelemetryTask | Queue timeout (200 ms) | Sem dados para transmitir neste ciclo | Skip de um pacote (200 ms) | 1 | 2 | 1 | **2** | Próximo ciclo retoma normalmente | ✅ Aceitável |

### 9.3 Resumo por RPN

**Top 5 riscos (RPN ≥ 100, ação obrigatória antes do voo):**

| RPN | ID | Modo de Falha | Mitigação atual | Lacuna |
|---|---|---|---|---|
| **200** | F19 | Apogeu falso (deploy prematuro) | Multi-ciclo + validação | ✅ Mitigado (O=2, D=10 mas validado) |
| **160** | F04 | IMU congela (I2C) | Validação NaN/Inf | **Sem detecção de IMU stale** |
| **160** | F16 | Corrente stall > BEC | Margem 325 mA | **Sem monitor de tensão 5 V** — *update 27/08: reproduzido em bancada (Mini-560 defeituoso), corrigido com XL4015 5A; ver nota na tabela* |
| **160** | F25 | Brownout (bateria fraca) | NVS restore | **Sem monitor de tensão de bateria** |

> **Nota:** os riscos F14 (servo não abre) e F15 (back-drive), antes
> classificados como RPN 200, foram reduzidos para RPN 40 após a
> execução e aprovação do teste de bancada (20/20 tentativas, todos
> os critérios A–E atendidos).
>
> **Update 2026-08-27:** F16 (alimentação do servo sob pico) foi
> reproduzido em bancada com bateria 2S real — o Mini-560 colapsava
> durante a ejeção (unidade defeituosa; corrente de pico medida 0,4 A,
> muito abaixo do nominal 5 A). Substituído por XL4015 5A: ejeção
> funcional na bateria. RPN efetivo reduzido de 160 para 80 (D 8→4:
> modo de falha conhecido e detectável). Monitor de tensão 5 V/Vbat
> segue recomendado (ações #2/#3, adiadas por decisão do usuário).

**Riscos médios (50 ≤ RPN < 100, monitorar):**

| RPN | ID | Modo de Falha |
|---|---|---|
| 90 | F24 | Heap exhaustion (OOM) |
| 84 | F03 | Drift de base_pressure |
| 80 | F20 | Descida não confirmada (atraso de deploy) |
| 72 | F17 | Liftoff falso (rampa) |
| 72 | F23 | Stack overflow em task |
| 60 | F01 | BMP585 congela |

**Riscos baixos (RPN < 50, aceitável):** F02, F05, F06, F07, F08, F09,
F10, F11, F12, F13, F18, F21, F22, F26, F27, F28, F29, F30.

### 9.4 Ações recomendadas (priorizadas por RPN)

| # | Ação | Risco(s) | Esforço | Impacto no RPN |
|---|---|---|---|---|
| 1 | **Implementar detecção de IMU stale** (idade da última leitura válida > 2 s, similar ao BARO_STALE) | F04 | Médio (adicionar `getLastReadingAgeMs()` no LSM6DS3Sensor + detector no FlightControlTask) | F04: D 8→2; RPN reduz de 160 para ~60 |
| 2 | **Adicionar monitor de tensão 5 V** (ADC no pino livre, alerta se < 4,5 V) | F16 | Médio (1 pino ADC + lógica de alerta) | F16: D 8→2; RPN reduz de 160 para ~40 |
| 3 | **Adicionar monitor de tensão da bateria** (ADC, alerta se < 3,3 V) | F25 | Médio (1 pino ADC + alerta) | F25: D 8→2; RPN reduz de 160 para ~40 |
| 4 | **Habilitar FreeRTOS stack canary** (configCHECK_FOR_STACK_OVERFLOW = 2) | F23 | Baixo (1 define no FreeRTOSConfig.h) | F23: D 4→1; RPN reduz de 72 para ~18 |
| 5 | **Adicionar verificação de integridade** no SD (checksum CRC32 por pacote) | F12 | Médio | F12: D 4→2; RPN reduz de 36 para ~18 |

---

*Documento gerado a partir da análise do repositório flight-computer
(branch dev-2026). Todos os valores de thresholds, constantes e decisões
de projeto são extraídos diretamente do código-fonte (`firmware/config.h`,
`firmware/FlightStateMachine.cpp`, `firmware/FlightControlTask.cpp`) e
dos documentos de especificação (`docs/hardware.md`, `docs/software.md`,
`docs/flowchart.md`, `docs/telemetry-format.md`).*
