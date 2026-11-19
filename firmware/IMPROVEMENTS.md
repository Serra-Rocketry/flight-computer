# Melhorias Profissionais — Firmware v2.0 (#11 Serra Rocketry)

**Data:** 2026-11-04
**Escopo:** `firmware/` (23 arquivos, ~3.950 linhas)
**Método:** navegação no grafo de conhecimento (`graphify-out/graph.json` — 324 nós, 446 arestas, 16 comunidades) + leitura integral do código + `grep` de referências cruzadas.
**Nenhum código foi alterado** — este arquivo é só diagnóstico e material de estudo.

---

## 1. O que o grafo mostrou (e por que isso importa)

| Observação do grafo | Interpretação de engenharia |
|---|---|
| `FlightStateMachine` tem **58 arestas** e betweenness **0,390** — liga 8 das 16 comunidades | É o maior ponto único de falha *e* o maior acoplamento do projeto. Mudanças nela exigem regressão completa. |
| `SensorData` (27 arestas) liga `FlightControlTask`, `TelemetryTask`, FSM e formato CSV | É o **contrato** do sistema. Qualquer campo novo (saturação, saúde, confirmação de atuação) passa por aqui. |
| `SensorData`, `BMP585Sensor`, `LSM6DS3Sensor`, `GPSModule` são god nodes, mas **GPS não aparece em nenhuma aresta de decisão de voo** | Um sensor inteiro é só telemetria: fonte independente de altitude/velocidade desperdiçada. |
| `getFlightControlStats()`, `getTelemetryStats()`, `getLoggerStats()` existem e **não têm nenhum consumidor** (confirmado por `grep`) | Há instrumentação de saúde pronta e invisível: overruns, drops de fila e logs perdidos nunca saem da placa. |
| `sendLoRa()` é chamada de **duas comunidades/tasks diferentes** (`TelemetryTask` e `LoggerTask`) sem nó de sincronização | Recurso compartilhado sem dono único → ver item **P0-5**. |
| Três caminhos distintos chamam `deployParachute()` (FSM, free-fall backstop, baro-stale contingency), mas só **um** persiste estado | Divergência de pós-condição entre caminhos de segurança → ver item **P0-4**. |
| Comunidades com coesão baixa (0,05–0,07): `FSM State and Flags`, `FSM Detection Logic`, `GPS Module` | Estado, detecção e persistência estão misturados na mesma classe; separar facilita teste unitário no host. |

**Prioridades:** `P0` = risco de segurança de voo · `P1` = robustez/observabilidade · `P2` = processo de engenharia.

---

## 2. P0 — Segurança de voo

### P0-1. Falha da IMU no boot hoje significa **nenhum paraquedas**

**Evidência**
- `FlightControlTask.cpp:386-395` — se `beginSensorWithRetry(g_imu, ...)` falha, `initFlightControlTask()` retorna `false`.
- `firmware.ino:94-118` — `initOk=false` entra em *safe-hold*: `for(;;)` com buzzer. Nenhuma task de voo é criada.
- `FlightStateMachine.cpp:98` — `update()` retorna se **qualquer** sensor não estiver pronto; `checkFreefallBackstop()` e `checkBaroStaleContingency()` também dependem da IMU.

Ou seja: o sistema tem três caminhos de deploy, mas **todos morrem juntos** se a IMU não iniciar. O *safe-hold* protege contra voo sem sensores, mas transforma uma falha parcial em falha total.

**O que fazer**
1. Degradação explícita por sensor, com modos `NOMINAL`, `BARO_ONLY`, `IMU_ONLY`, `NO_FLIGHT`:
   - `BARO_ONLY`: apogeu por barômetro (`h < maxAltitude - Δ` sustentado + `vz < 0`), sem nenhuma dependência de acelerômetro.
   - `IMU_ONLY`: timer de voo a partir do liftoff por aceleração + free-fall por `|a|`, sem barômetro.
   - `NO_FLIGHT` (ambos falharam): aí sim *safe-hold*.
2. Telemetria e buzzer devem continuar vivos em **todos** os modos — hoje o *safe-hold* também silencia o rádio, eliminando o diagnóstico em campo.
3. Telegrafar o modo degradado no CSV/LoRa e no padrão sonoro do pad.

**Por quê / fontes**
- Princípio *fail-operational* vs *fail-safe*: em aviônica de ejeção, "desligar tudo" não é o estado seguro — o estado seguro é **ejetar**.
- G. J. Holzmann (NASA/JPL), *The Power of Ten — Rules for Developing Safety-Critical Code*: regras sobre detecção e resposta a falhas em vez de propagação silenciosa. https://spinroot.com/gerard/pdf/P10.pdf

---

### P0-2. `Vz` é diferença finita crua a 5 Hz e o apogeu usa **uma única amostra**

**Evidência**
- `BMP585Sensor.cpp:257-270` — `vz = (altitude - prevAltitude)/dt`, sem nenhum filtro (só *clip* em ±200 m/s).
- `flight/FlightControlTask.h:40` — `FLIGHT_CONTROL_PERIOD_MS = 200` (5 Hz). O BMP585 está configurado para 25 Hz (`BMP585Sensor.cpp:40`), então 80% das conversões são descartadas.
- `config.h:237` + `FlightStateMachine.cpp:381-383` — `detectApogee()` = `|vz| < 1.0` **em um único ciclo**.
- O próprio comentário em `FlightStateMachine.cpp:154-162` admite que o ruído de quantização faz `|vz|` cruzar zero antes do apogeu real; a proteção é indireta (o gate de deploy), não a detecção.

Com ±0,5 m de ruído barométrico em janelas de 200 ms, o `Vz` instantâneo tem ruído da ordem de ±2,5 m/s — maior que o limiar de apogeu (1,0 m/s) e da mesma ordem do gate de deploy (−2,0 m/s). A margem de segurança vem de sorte estatística, não de estimação.

**O que fazer**
1. Substituir a derivada crua por um **estimador α-β (g-h)** ou **Kalman 1-D** com estado `(h, vz)` e `dt` real medido por `esp_timer_get_time()`:
   - predição: `h⁻ = h + vz·dt`, `vz⁻ = vz`;
   - atualização com o residual do barômetro, ganhos `α/β` dimensionados pelo ruído medido no banco.
   - Evolução natural: incluir `az` da IMU como entrada de predição (modelo de 3 estados) — mas com cuidado, pois `az` em repouso já apresenta bias documentado de +2,81 m/s² (ver **P1-2**).
2. Apogeu por **consenso temporal e físico**, não por amostra única: `vz_est < 0` por N de M amostras **e** `h < maxAltitude − Δh` **e** tempo mínimo desde burnout/liftoff.
3. Subir o loop de controle para 20–25 Hz (o sensor já entrega 25 Hz) e expressar todos os tempos de confirmação em **ms**, não em ciclos (ver **P2-3**).

**Por quê / fontes**
- Filtro α-β (g-h): https://en.wikipedia.org/wiki/Alpha_beta_filter — custo computacional quase zero, ideal para ESP32.
- Kalman explicado visualmente: https://bzarg.com/p/how-a-kalman-filter-works-in-pictures/
- Validação por simulação de trajetória: RocketPy — https://docs.rocketpy.org/ (gerar perfis de `h(t)` com ruído realista e medir taxa de falso apogeu antes/depois).
- Datasheet BMP585 (ruído de pressão, ODR, IIR interno): https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf

---

### P0-3. Ejeção: canal único, sem realimentação, e "deploy" ≠ "atuou"

**Evidência**
- `FlightControlTask.cpp:367-369` — `deployParachute()` é apenas `ParachuteServo.write(SERVO_OPEN)`.
- `parachute_module.cpp:44-55` — `setupServo()` só faz `attach` + `write`; não há leitura de posição, corrente ou continuidade.
- `config.h:96-100` — dois footprints de servo (M1, M2) compartilham **o mesmo** sinal PWM: um único ponto de falha elétrica.
- `FlightControlTask.cpp:355-356` — `data.parachute_deployed = g_fsm->isParachuteDeployed()`: a telemetria informa a **decisão da FSM**, não a atuação física. Se o servo estiver desconectado ou sem energia, o ground station verá `chute=YES`.
- Servo e MCU na mesma alimentação: o pico de corrente do servo no momento da ejeção é exatamente quando não se pode ter *brownout*.

**O que fazer**
1. Separar no `SensorData`: `deploy_commanded` (FSM/backstop), `deploy_actuated` (PWM aplicado) e, se houver hardware, `deploy_confirmed` (continuidade/posição/corrente).
2. Segundo canal de ejeção independente (outro GPIO, outro driver), comandado pelo backstop — hoje os três caminhos convergem para o mesmo pino.
3. Rotina de *built-in test* no pad: varredura curta do servo antes do arm + teste de continuidade com indicação sonora (sem ejetar).
4. Revisar alimentação/aterramento do servo (rail separado ou capacitor de reserva) e medir `VBAT` durante a ejeção no banco.
5. Planejar **dual deployment** (drogue no apogeu + principal a 150–300 m) como roadmap para voos mais altos: hoje existe um único evento de ejeção no apogeu (`FlightStateMachine.cpp:392-397`, sem teto de altitude por decisão de projeto).

**Por quê / fontes**
- Códigos de segurança e práticas de recuperação em foguetemodelismo: NAR — https://www.nar.org/ · Tripoli — https://tripoli.org/
- Holzmann, *Power of Ten* (assertivas e verificação de pós-condição em código crítico): https://spinroot.com/gerard/pdf/P10.pdf

---

### P0-4. **Bug real:** deploy por backstop/contingência não persiste — após reboot o servo **fecha em voo**

**Evidência**
- Só a FSM grava a flag: `FlightStateMachine.cpp:190-195` (`_parachuteDeployed = true; persistToNVS();`).
- Os outros dois caminhos só marcam RAM: `FlightControlTask.cpp:498-516` (`g_parachuteActuated = true`, `logMessage(...)`), sem `persistToNVS()` e sem atualizar a FSM.
- No boot seguinte: `FlightStateMachine.cpp:58` restaura o snapshot **sem** a flag → `FlightControlTask.cpp:405` (`g_parachuteActuated = false`) → `FlightControlTask.cpp:413` chama `setupServo(false)` → `parachute_module.cpp:52` move o servo para `SERVO_CLOSED`.

Sequência: backstop ejeta no apogeu → watchdog/panic reinicia o ESP32 → o firmware **fecha o compartimento durante a descida** e rearma a lógica de deploy. É exatamente o cenário que o comentário de `setupServo(keepOpen)` diz evitar — mas a proteção só cobre o caminho da FSM.

**O que fazer**
1. Criar um **único serviço de ejeção**, por exemplo `requestParachuteDeploy(DeploySource src)`, que nesta ordem: trava o latch → persiste NVS → aciona o(s) canal(is) → registra log com a origem → atualiza `SensorData`. FSM, backstop e contingência chamam **somente** essa função.
2. Adicionar **CRC32** ao `NvsSnapshot` (hoje só `magic`+`version`, `FlightStateMachine.cpp:250-275`) e gravar em dois slots alternados com contador de sequência, para tolerar escrita interrompida.
3. Persistir também `maxAltitude` e o motivo do deploy, para auditoria pós-voo.
4. Teste obrigatório: *fault injection* de reboot imediatamente após cada um dos três caminhos de deploy (ver **P2-1**).

**Por quê / fontes**
- NVS no ESP-IDF (escrita atômica, *wear leveling*, limitações): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- Watchdog de tarefas (origem desses reboots): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html

---

### P0-5. Rádio LoRa compartilhado por duas tasks, sem mutex, no mesmo SPI do SD

**Evidência**
- `TelemetryTask.cpp:215` — `sendLoRa(telemetry)` (Core 0, prioridade 5).
- `LoggerTask.cpp:71-73` — `sendLoRa("ERR|...")` (Core 0, prioridade 1).
- `modules/lora_module.h:130-156` — `beginPacket()` → `print()` → `endPacket()` sem nenhuma proteção; a biblioteca `LoRa` não é *thread-safe*.
- `modules/filesystem_module.h:53-55` — SD no mesmo barramento SPI (`SCK=12/MISO=13/MOSI=11`), também sem mutex.

Como as prioridades diferem, a `TelemetryTask` pode **preemptar a LoggerTask no meio de um pacote**: o resultado são dois payloads intercalados em um único frame LoRa. O efeito visível é perda intermitente de pacotes de erro — justamente os mais importantes.

**O que fazer**
1. Curto prazo: `SemaphoreHandle_t` por barramento (SPI rádio+SD) adquirido em `sendLoRa()` e nas operações de arquivo.
2. Melhor: **uma única task de rádio** com fila de mensagens (telemetria + erros + comandos), tornando o rádio recurso de dono único, como já é o GPS.
3. Aproveitar para tornar o TX assíncrono (`LoRa.endPacket(true)` + DIO0) e medir o tempo de ocupação real.

**Por quê / fontes**
- *Mastering the FreeRTOS Real Time Kernel* (mutexes, exclusão mútua, inversão de prioridade): https://www.freertos.org/Documentation/RTOS_book.html

---

### P0-6. A telemetria a 5 Hz **não cabe** no enlace LoRa configurado

**Evidência e cálculo**
`config.h:66-69` → SF7, BW 125 kHz, CR 4/5, CRC ligado, preâmbulo 8. Pela fórmula de *time-on-air* da Semtech:

| Payload CSV | Time-on-Air | Ocupação a 5 Hz (200 ms) | Taxa máxima |
|---|---|---|---|
| 100 B | 174 ms | 87% | 5,7 pkt/s |
| 140 B | 231 ms | **115%** | 4,3 pkt/s |
| 160 B | 261 ms | **131%** | 3,8 pkt/s |
| 200 B | 318 ms | **159%** | 3,2 pkt/s |

O frame de 22 campos (`TelemetryTask.cpp:82-94`) fica tipicamente na faixa de 140–170 B → **acima de 100% do canal**. Como `LoRa.endPacket()` é bloqueante (`lora_module.h:126`), a `TelemetryTask` estoura o período em todos os ciclos; o dreno de fila que "fica só com a amostra mais nova" (`TelemetryTask.cpp:196-200`) mascara o problema descartando dados. O divisor de escrita em flash (`++flashWriteDivider >= 5`) também deixa de corresponder a 1 Hz.

**O que fazer**
1. **Frame binário curto** (20–30 B: alt, vz, maxAlt, estado, flags, contador, CRC) a 5 Hz + frame completo CSV a 0,5–1 Hz. ToA cai para ~60–90 ms.
2. Ou reduzir a taxa de TX para 1–2 Hz, mantendo 5 Hz só no log local.
3. Medir o ToA real com `esp_timer_get_time()` em volta de `sendLoRa()` e publicar na telemetria de saúde (**P1-4**).
4. Verificar limites regulatórios de potência/duty cycle da faixa de 915 MHz no Brasil antes de aumentar taxa ou potência: ANATEL — https://www.gov.br/anatel/pt-br/regulado/certificacao-de-produtos

**Por quê / fontes**
- Semtech AN1200.22, *LoRa Modulation Basics* (fórmula de ToA, relação SF/BW/CR): https://www.semtech.com/uploads/documents/an1200.22.pdf

---

## 3. P1 — Robustez e observabilidade

### P1-1. Watchdog cobre só uma task; *safe-hold* fica fora de qualquer supervisão
`esp_task_wdt_add(nullptr)` só em `taskFlightControl` (`FlightControlTask.cpp:458`). Telemetry e Logger podem travar sem reset; o `for(;;)` de `firmware.ino:112` não alimenta nem registra nada. Adicionar: registro das três tasks no TWDT, *panic handler* que preserve o snapshot antes do reset, e contador de reboots com causa (`esp_reset_reason()`) na telemetria.
Fonte: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html

### P1-2. IMU sem range explícito, sem calibração e sem flag de saturação
`LSM6DS3Sensor.cpp` nunca chama `setAccelRange()`/`setGyroRange()` — depende do default da biblioteca; a validação de range (`MAX_ACCEL = 200`) **descarta** amostras em vez de sinalizar saturação; e o comentário da FSM registra bias de +2,81 m/s² em repouso sem nenhuma rotina de compensação.
Fazer: configurar ±16 g explicitamente, calibrar offsets com N amostras no pad (guardar em NVS), marcar `imu_saturated` no `SensorData` em vez de congelar a última amostra, e avaliar acelerômetro de alto-g (ex.: ST H3LIS331DL / ADXL375) se a queima superar 16 g.
Fonte: datasheet LSM6DS3 — https://www.st.com/resource/en/datasheet/lsm6ds3.pdf

### P1-3. Escrita em flash durante o voo: usar a PSRAM como buffer e abrir o arquivo uma vez
O comentário em `TelemetryTask.cpp:217-222` já documenta panic por *flash cache off* nos dois núcleos. A mitigação atual é reduzir para 1 Hz — ou seja, perde-se resolução de log justamente nas fases críticas.
Fazer: acumular frames em **PSRAM** (a placa N8R8 tem 8 MB) e descarregar em `LANDED`; manter `File` aberto em vez de `open/append/close` por linha (`filesystem_module.h:132-161`); gravar binário com CRC e converter para CSV no pós-voo; task de storage dedicada com fila.
Fonte: ESP-IDF *External RAM* — https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html

### P1-4. Exportar a saúde do sistema (a instrumentação já existe e não é lida)
Nenhum consumidor para `getFlightControlStats()` / `getTelemetryStats()` / `getLoggerStats()`. Criar um pacote de saúde a 0,2 Hz com: `cycleCount`, `overrunCount`, `queueDropCount`, `maxExecTimeUs`, logs descartados, `uxTaskGetStackHighWaterMark()` das três tasks, `esp_get_minimum_free_heap_size()`, causa do último reset, modo degradado (**P0-1**) e *git hash* do build. Sem isso, todo diagnóstico de campo depende de cabo USB.
Fontes: FreeRTOS book (acima) e ESP-IDF WDT.

### P1-5. GPS entra na decisão de voo e o fix precisa de idade
Hoje `hasValidFix()` = `_gps.location.isValid()` (`GPSModule.cpp:70-72`), que não expira com o tempo — um fix antigo continua "válido". Usar `location.age()` e, com fix fresco, GPS como terceira fonte independente de altitude/velocidade (voto de confirmação, não gatilho único) e timestamp absoluto no log. Bônus: geofence/última posição conhecida para recuperação.
Fonte: documentação TinyGPSPlus (campos `isValid`/`isUpdated`/`age`).

### P1-6. Comando `ARM` só por USB, sem confirmação nem estado visível
`FlightControlTask.cpp:239-271` lê `Serial` (USB) — no pad, normalmente desconectado. Evoluir para comando via LoRa com *ack*, sequência de dois passos (`ARM?` → `ARM!`), `DISARM`/`STATUS`, e indicação sonora/telemetria do estado de arm. Manter a recusa por `maxAltitude`/paraquedas, que está correta.

### P1-7. Contadores que estouram em voo longo
`SensorData.packet_count` é `uint16_t` (`SensorData.h:82`): a 5 Hz estoura em ~3,6 h; se o loop subir para 25 Hz (**P0-2**), em ~44 min. Passar para `uint32_t` e tratar o wrap de `millis()` (~49,7 dias) na conversão para tempo de voo.

### P1-8. `SERVO_PIN` e pinos: documentação divergente do código
`config.h:96` diz "Schematic: GPIO7" e `config.h:100` define `SERVO_PIN 39`; `filesystem_module.h:53` cita `SCK=4, MISO=2, MOSI=3` enquanto o barramento real é 12/13/11. Em hardware de voo, comentário errado de pino é defeito de segurança — revisar contra o esquemático KiCad e manter uma única tabela de pinos.

---

## 4. P2 — Processo de engenharia

### P2-1. Testar o **C++ real** no host, não uma reimplementação em Python
Hoje a validação da FSM é feita por `extras/FSM_tester/FSM_Tester.py`, um modelo paralelo. O drift já aparece na documentação: `firmware.ino:28-29` diz 50 Hz, `FlightControlTask.h:40` diz 5 Hz, e os comentários de `LIFTOFF_CONFIRM_MS` citam "5 cycles @ 50Hz" enquanto `config.h:227` diz "1 cycle @5Hz".
Fazer: `platformio` com ambiente `native` + Unity, compilando `FlightStateMachine.cpp` e `BMP585Sensor.cpp` com *stubs* de Arduino, e *replay* do `13_30_11-Dados.csv` contra o código de produção. Adicionar *fault injection*: barômetro congelado, NaN, saturação de IMU, reboot após cada caminho de deploy (**P0-4**), perda de GPS.
Fontes: PlatformIO Unit Testing — https://docs.platformio.org/en/latest/advanced/unit-testing/ · Unity — https://github.com/ThrowTheSwitch/Unity

### P2-2. Build reproduzível + análise estática em CI
Hoje o build é manual na Arduino IDE. Migrar para PlatformIO (mesma toolchain, scriptável) e rodar em CI: `-Wall -Wextra -Werror`, `cppcheck`, `clang-tidy`, `clang-format` e um *job* de testes nativos do item anterior. Referência de regras: https://clang.llvm.org/extra/clang-tidy/ e https://spinroot.com/gerard/pdf/P10.pdf

### P2-3. Tempo em **ms**, com `static_assert`, em vez de "ciclos a X Hz"
`config.h` mistura os dois: `FREEFALL_BACKSTOP_CYCLES = 5 // ~1.0s @5Hz`, `BARO_STALE_SUSTAIN_CYCLES = 13 // 2.6s @5Hz`, `PARACHUTE_CONFIRM_CYCLES = 3 // 600ms @5Hz`. Ao mudar o período, todos os tempos mudam juntos e em silêncio.
Fazer: definir `*_MS` e derivar os ciclos por `constexpr`, com `static_assert` garantindo a janela pretendida — assim a mudança de 5 Hz para 25 Hz (**P0-2**) não recalibraria o sistema por acidente.

### P2-4. Eliminar alocação dinâmica e `String` do caminho crítico
`new` sem verificação em `FlightControlTask.cpp:377-397` e `TelemetryTask.cpp:130`; `String` em `logMessage()`, `getData()`, `file_dir` e no retorno de `assembleTelemetry()`.
Fazer: instâncias estáticas dos sensores/FSM, buffers fixos com `snprintf`, e reserva de heap só na inicialização. É a regra 3 do *Power of Ten*: sem alocação dinâmica depois da inicialização em código crítico.

### P2-5. Rastreabilidade de configuração de voo
Gravar no cabeçalho do CSV (e no pacote de saúde) os thresholds efetivos, o *git hash* e o modo degradado. Ao comparar dois voos, hoje não há como saber com quais parâmetros cada um voou.

### P2-6. Documentação viva
Corrigir os drifts já encontrados: 50 Hz vs 5 Hz (`firmware.ino:28-29,134`), "Every 20ms" (`FlightControlTask.h:6`), `overrunCount > 20ms` (`FlightControlTask.h:71`), `@author Team #100` vs `#11`, pinos do item **P1-8**. Gerar a tabela de thresholds a partir do `config.h` para não duplicar valores em prosa.

---

## 5. Roadmap sugerido

| Etapa | Itens | Critério de aceite |
|---|---|---|
| **1 — Corrigir riscos imediatos** | P0-4, P0-5, P0-6 | Reboot após qualquer deploy mantém servo aberto; zero pacotes intercalados; ToA medido < 50% do canal |
| **2 — Não depender de um único sensor** | P0-1, P0-3, P1-2 | Modos `BARO_ONLY`/`IMU_ONLY` ejetam em simulação e em teste de queda; BIT do servo no pad |
| **3 — Estimação e apogeu** | P0-2, P2-3, P1-5 | Zero falsos apogeus no replay do CSV real + perfis RocketPy com ruído; `Vz` filtrado com ganhos documentados |
| **4 — Observabilidade** | P1-1, P1-3, P1-4, P1-7 | Pacote de saúde no ground station; log de voo completo em alta taxa na PSRAM; nenhum panic por flash |
| **5 — Processo** | P2-1, P2-2, P2-4, P2-5, P2-6 | CI verde com testes nativos do C++ real e análise estática; docs sem drift |

## 6. Como revalidar antes de voar

1. Replay do CSV de voo real no C++ de produção (P2-1), incluindo injeção de falhas.
2. Teste de bancada de ejeção com medição de `VBAT` e continuidade (P0-3).
3. Teste de reboot forçado em cada estado da FSM e após cada origem de deploy (P0-4).
4. Teste de enlace: ToA medido, taxa de pacotes recebidos e RSSI no ground station (P0-6).
5. Teste de queda (*drop test*) com e sem barômetro/IMU operantes (P0-1).

---

### Fontes principais (verificadas em 2026-11-04)

- Holzmann, *The Power of Ten*: https://spinroot.com/gerard/pdf/P10.pdf
- FreeRTOS Kernel Book: https://www.freertos.org/Documentation/RTOS_book.html
- ESP-IDF Task Watchdog: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html
- ESP-IDF NVS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- ESP-IDF External RAM (PSRAM): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html
- Semtech AN1200.22 (LoRa ToA): https://www.semtech.com/uploads/documents/an1200.22.pdf
- Filtro α-β: https://en.wikipedia.org/wiki/Alpha_beta_filter
- Kalman visual: https://bzarg.com/p/how-a-kalman-filter-works-in-pictures/
- BMP585 (produto): https://www.bosch-sensortec.com/en/products/environmental-sensors/pressure-sensors/bmp585
- LSM6DS3 (datasheet ST): https://www.st.com/resource/en/datasheet/lsm6ds3.pdf
- PlatformIO Unit Testing: https://docs.platformio.org/en/latest/advanced/unit-testing/
- Unity: https://github.com/ThrowTheSwitch/Unity
- clang-tidy: https://clang.llvm.org/extra/clang-tidy/
- RocketPy: https://docs.rocketpy.org/
- ANATEL (certificação/uso de RF): https://www.gov.br/anatel/pt-br/regulado/certificacao-de-produtos
- NAR / Tripoli (segurança e recuperação): https://www.nar.org/ · https://tripoli.org/
