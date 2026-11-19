# ISSUES: Implementação das melhorias do firmware v2.0

**Gerado em:** 2026-11-19 (revisão de código que confirmou os 17 itens de `IMPROVEMENTS.md` e adicionou P0-7, P0-8 e P1-9; ver `PLANO_IMPLEMENTACAO.md`, itens 21 a 23)
**Como usar:** cada bloco abaixo é uma issue pronta. O título é o header `## #N`; o que vem depois de `Corpo →` é o campo *body* da issue (copie o conteúdo do bloco cercado). Crie com `gh issue create --title "..." --body-file ...` ou pela interface do GitHub.
**Labels sugeridas (criar no repo):** `P0`, `P1`, `P2`, `safety-critical`, `bug`, `firmware`, `observability`, `process`, `hardware-bench`, `good-first-issue`
**Milestones sugeridos:** Etapa 1: Riscos imediatos · Etapa 2: Redundância de sensor · Etapa 3: Estimação e apogeu · Etapa 4: Observabilidade · Etapa 5: Processo

## Ordem de criação sugerida

| # | ID | Título curto | Milestone | Depende de |
|---|---|---|---|---|
| 1 | P0-4 | Serviço único de ejeção + persistência (reboot fecha paraquedas) | Etapa 1 | nenhuma |
| 2 | P0-7 | Contingência baro-stale é código morto | Etapa 1 | #18 (regressão; não bloqueia) |
| 3 | P1-9 | Checagem de `attach()` do servo versão-dependente | Etapa 1 | nenhuma |
| 4 | P0-5 | LoRa/SPI disputado por 2 tasks sem sincronização | Etapa 1 | nenhuma |
| 5 | P0-6 | Telemetria 5 Hz não cabe no enlace LoRa | Etapa 1 | nenhuma |
| 6 | P0-1 | Modos degradados (BARO_ONLY / IMU_ONLY) | Etapa 2 | nenhuma |
| 7 | P0-3 | commanded/actuated/confirmed + 2º canal + BIT | Etapa 2 | #1, #6 (contrato SensorData) |
| 8 | P1-2 | IMU: range, calibração, saturação, leitura falha | Etapa 2 | nenhuma |
| 9 | P2-3 | Tempos em ms com `static_assert` | Etapa 3 | nenhuma (pré-requisito do #10) |
| 10 | P0-2 | Estimador (α-β/Kalman) + apogeu por consenso | Etapa 3 | #9; soft: #8 |
| 11 | P1-5 | GPS: fix com idade + voto de confirmação | Etapa 3 | soft: #10 |
| 12 | P1-1 | TWDT em todas as tasks + causa de reset | Etapa 4 | nenhuma |
| 13 | P1-3 | Buffer PSRAM + flush em LANDED (panic de flash) | Etapa 4 | soft: #4 |
| 14 | P1-4 | Pacote de saúde por rádio | Etapa 4 | soft: #12, #21 |
| 15 | P1-7 | Contadores: largura + wrap de `millis()` | Etapa 4 | nenhuma |
| 16 | P0-8 | Log não sobrevive a reboot/segundo voo | Etapa 4 | soft: #12 |
| 17 | P1-6 | ARM por LoRa em dois passos + DISARM/STATUS | Etapa 4 | #4 |
| 18 | P2-1 | Testes nativos no host + replay + fault injection | Etapa 5 | nenhuma (desbloqueia regressões de #1, #2, #10) |
| 19 | P2-2 | CI: build reproduzível + análise estática | Etapa 5 | #18 |
| 20 | P2-4 | Zero alocação dinâmica/`String` no caminho crítico | Etapa 5 | nenhuma |
| 21 | P2-5 | Metadados de voo: thresholds efetivos + git hash | Etapa 5 | nenhuma |
| 22 | P2-6 | Docs vivas: matar drifts e impedir que voltem | Etapa 5 | #23 |
| 23 | P1-8 | Auditoria de pinos contra o esquemático | Etapa 5 | nenhuma |

---

## #1 · [P0-4] Bug crítico: reboot após deploy fecha o paraquedas em pleno voo

**Labels:** `P0` `safety-critical` `bug` `firmware`
**Milestone:** Etapa 1: Riscos imediatos
**Referência:** `PLANO_IMPLEMENTACAO.md` item 4 · `IMPROVEMENTS.md` P0-4

Corpo →

```markdown
## Problema

Dos três caminhos que podem ejetar, só um grava o estado em memória não-volátil. Se o ESP32 reiniciar (watchdog, panic, brownout) logo após uma ejeção pelos outros dois, o firmware acorda sem saber que ejetou e move o servo para a posição fechada durante a descida, rearmando inclusive a lógica de deploy.

## Evidência

- Só a FSM persiste: `FlightStateMachine.cpp:190-195` (`_parachuteDeployed = true; persistToNVS();`).
- Backstop só marca RAM: `FlightControlTask.cpp:498-504`, sem `persistToNVS()`, sem atualizar a FSM.
- Contingência baro-stale idem: `FlightControlTask.cpp:509-516`.
- Boot seguinte sem a flag: `FlightControlTask.cpp:405` (`g_parachuteActuated=false`) e `:413` chama `setupServo(false)`, que escreve `SERVO_CLOSED` (`parachute_module.cpp:52`).

## O que fazer

1. Serviço único de ejeção, ex. `requestParachuteDeploy(DeploySource src)`, chamado pelos três caminhos, nesta ordem:
   a. travar latch;
   b. persistir em NVS (o estado precisa sobreviver a reboot antes de tocar no hardware);
   c. acionar o(s) canal(is);
   d. registrar origem e motivo;
   e. atualizar o `SensorData`.
2. Integridade do snapshot: CRC32 no `NvsSnapshot` (hoje só `magic`+`version`, `FlightStateMachine.cpp:250-275`) e gravação alternada em 2 slots com contador de sequência, para tolerar escrita interrompida.
3. Persistir também `maxAltitude` e o motivo do deploy (auditoria pós-voo).
4. Atenção: a persistência é uma escrita de flash durante o voo (o mesmo modo de falha do P1-3, panic de flash-cache). A ordem "persiste primeiro, atua depois" é a correta, mas meça em bancada o stall de I2C durante uma escrita NVS deliberada.

## Critérios de aceite

- [ ] Para cada um dos três caminhos de deploy: reboot forçado logo após a ejeção (bancada) deixa o servo aberto, a lógica não rearma, e o motivo sobrevive para leitura pós-voo.
- [ ] Snapshot corrompido injetado (bit flip ou truncado) é rejeitado pelo CRC, não restaurado.
- [ ] Os três caminhos chamam o mesmo ponto de entrada (grep: nenhum `ParachuteServo.write(SERVO_OPEN)` fora do serviço).
- [ ] O teste de fault injection de reboot pós-deploy vira regressão permanente (ver issue #18).
```

---

## #2 · [P0-7] Bug crítico: contingência baro-stale é código morto, sem deploy se o barômetro congelar

**Labels:** `P0` `safety-critical` `bug` `firmware`
**Milestone:** Etapa 1: Riscos imediatos
**Referência:** `PLANO_IMPLEMENTACAO.md` item 21 (achado da revisão 2026-11-19)

Corpo →

```markdown
## Problema

O detector que deveria salvar o voo quando o barômetro congela nunca dispara. A variável que o arma (`g_bsLiftoffLatched`) só é avaliada depois do guarda que exige que ela já esteja armada: ela jamais muda de false para true, e `checkBaroStaleContingency()` não tem como retornar true.

Cenário resultante: barômetro congela na subida, a FSM não vê apogeu (vz congelado), o backstop não dispara (vz congelado), a contingência é morta: descida balística sem paraquedas, exatamente o common-mode failure que o comentário do próprio código descreve como motivação.

## Evidência (FlightControlTask.cpp:170-230)

1. Voo normal (baro fresco): a função retorna em `getLastReadingAgeMs() < BARO_STALE_AGE_MS` (linhas 176-179), antes de qualquer leitura da IMU.
2. Baro travado: o guarda `if (!g_bsLiftoffLatched || maxAltitude <= guarda) return false;` (linhas 183-187) rejeita a execução antes do único trecho que seta o latch (`acc > LIFTOFF_ACCEL_THRESHOLD`, linhas 214-216).
3. O latch nasce false (linha 81) e nenhum outro código o escreve.

O validador Python (`extras/FSM_tester/validate_baro_stale.py:145-146`) arma o latch antes dos guardas: implementa a intenção, não o C++ real. Por isso o bug passou: testa-se o modelo, não o artefato.

## O que fazer

1. Atualizar o latch "o voo começou" todo ciclo, independente do estado do barômetro (a IMU não depende dele); só depois aplicar os guardas de staleness/altitude.
2. Revalidar a janela de 2,6 s (BARO_STALE_SUSTAIN_CYCLES) contra o CSV de voo real, agora que o latch vive desde o liftoff real.
3. Tornar o cenário "barômetro congelado após liftoff" um teste de regressão permanente (fault injection no host, issue #18; enquanto #18 não existir, validar congelando o `update()` do baro por software em bancada).

## Critérios de aceite

- [ ] Barômetro congelado de propósito (por software) após liftoff simulado: paraquedas comandado pela contingência dentro da janela esperada.
- [ ] A contingência continua sem disparar no pad (sem liftoff) e com baro sadio.
- [ ] Teste de regressão "baro freeze mid-ascent" rodando (no host quando #18 existir; em bancada enquanto isso).
```

---

## #3 · [P1-9] `setupServo()` trata retorno de `attach()` como bool: canal 0 é sucesso

**Labels:** `P1` `safety-critical` `firmware` `good-first-issue`
**Milestone:** Etapa 1: Riscos imediatos
**Referência:** `PLANO_IMPLEMENTACAO.md` item 23 (achado da revisão 2026-11-19)

Corpo →

```markdown
## Problema

`if (!ParachuteServo.attach(SERVO_PIN))` (parachute_module.cpp:46) testa o retorno como booleano, mas `ESP32Servo::attach()` devolve o número do canal PWM, e o primeiro canal LEDC é 0, um caso de sucesso. Um servo sadio alocado no canal 0 faria `setupServo()` retornar false, o init virar FATAL, e um sistema sadio terminar em safe-hold: foguete sem paraquedas (a mesma consequência do P0-1).

O banco funciona hoje por coincidência: no ESP32-S3 o caminho MCPWM da biblioteca devolve >= 100. No fallback LEDC (ou em versões mais antigas da lib) o retorno pode ser 0. O comentário local ("attach() returns true on success") documenta um contrato que a biblioteca não tem.

## Evidência

- `parachute_module.cpp:44-48`: `!attach()`.
- ESP32Servo (madhephaestus): `Servo::attach()` termina com `return pwm.getChannel();`, e `ESP32PWM::getChannel()` devolve `pwmChannel` (0-15 no LEDC; >= 100 no MCPWM).

## O que fazer

1. Chamar `attach()` e verificar `ParachuteServo.attached()` (o estado real do pino) em vez do retorno.
2. Registrar o canal alocado no boot.
3. Definir o que um attach genuinamente falho significa (um safe-hold distinguível do safe-hold de sensores?).

## Critérios de aceite

- [ ] Boot limpo em bancada imprime o canal alocado, e `setupServo` não reporta falso com hardware sadio.
- [ ] Revisão rápida: nenhum `!obj.method()` restante cujo retorno não seja bool.
```

---

## #4 · [P0-5] LoRa e SPI compartilhados por duas tasks sem sincronização

**Labels:** `P0` `firmware`
**Milestone:** Etapa 1: Riscos imediatos
**Referência:** `PLANO_IMPLEMENTACAO.md` item 5 · `IMPROVEMENTS.md` P0-5

Corpo →

```markdown
## Problema

Telemetria e logger chamam `sendLoRa()` de tasks com prioridades diferentes, sem proteção nenhuma, sobre um barramento SPI compartilhado com o SD. Uma preempção no meio de um pacote intercala dois payloads em um único frame, corrompendo justamente as mensagens de erro, as mais importantes.

## Evidência

- `TelemetryTask.cpp:215`: `sendLoRa(telemetry)` (Core 0, prio 5).
- `LoggerTask.cpp:71-73`: `sendLoRa("ERR|...")` (Core 0, prio 1).
- `modules/lora_module.h:130-156`: `beginPacket() → print() → endPacket()` sem proteção; a lib LoRa não é thread-safe.
- `modules/filesystem_module.h`: SD no mesmo SPI (SCK=12/MISO=13/MOSI=11).

## O que fazer

1. Curto prazo: mutex por barramento (rádio + SD) em `sendLoRa()` e nas operações de arquivo; pesquisar inversão de prioridade.
2. Melhor: task de rádio única com fila (telemetria + erros + comandos), o padrão dono único que o GPS do próprio firmware já usa; definir a política de prioridade/descarte quando a fila enche.
3. Avaliar TX assíncrono (`LoRa.endPacket(true)` + DIO0) e medir o tempo real de ocupação.

## Critérios de aceite

- [ ] Sessão de stress (rajada de telemetria + erros simultâneos) produz zero pacotes intercalados.
- [ ] Mensagens de erro chegam inteiras no ground station durante rajada de telemetria.
- [ ] Evidência de captura do problema antes da correção documentada (prova de que existia).
```

---

## #5 · [P0-6] Telemetria a 5 Hz não cabe no enlace LoRa configurado

**Labels:** `P0` `firmware`
**Milestone:** Etapa 1: Riscos imediatos
**Referência:** `PLANO_IMPLEMENTACAO.md` item 6 · `IMPROVEMENTS.md` P0-6

Corpo →

```markdown
## Problema

Pela fórmula de time-on-air da Semtech (SF7, BW 125 kHz, CR 4/5, CRC on), o frame CSV atual (140-170 B) leva ~231-261 ms no ar, mais que os 200 ms de intervalo entre frames a 5 Hz. O canal está acima de 100% de ocupação: a task estoura o período em todos os ciclos, e o dreno "fica com a amostra mais nova" mascara o problema jogando dados fora.

| Payload | ToA | Ocupação a 5 Hz |
|---|---|---|
| 100 B | ~174 ms | 87% |
| 140 B | ~231 ms | 115% |
| 160 B | ~261 ms | 131% |

## Evidência

- `config.h:66-69`: SF7/BW125/CR5.
- `TelemetryTask.cpp:82-94`: frame de 22 campos; `sendLoRa` bloqueante (`lora_module.h`).
- `TelemetryTask.cpp:196-200`: descarte de backlog que mascara o problema.

## O que fazer

1. Medir o ToA real em runtime (`esp_timer_get_time()` em volta de `sendLoRa()`) e publicar na telemetria de saúde.
2. Redesenhar o tráfego: frame binário curto (20-30 B) a 5 Hz para o ground + frame CSV completo a 0,5-1 Hz (ou só no log local). Meta de ocupação < 50%.
3. Versionar o formato binário para receptor e transmissor não desincronizarem.
4. Antes de mexer em taxa/potência: verificar limites ANATEL de potência e duty cycle em 915 MHz.

## Critérios de aceite

- [ ] ToA real medido e visível na telemetria de saúde.
- [ ] Ocupação do canal < 50% durante um voo inteiro simulado, sem perda de amostras da fila.
- [ ] Registro da verificação regulatória (ANATEL) no PR.
```

---

## #6 · [P0-1] Falha de um sensor no boot derruba todas as formas de ejeção

**Labels:** `P0` `safety-critical` `firmware`
**Milestone:** Etapa 2: Redundância de sensor
**Referência:** `PLANO_IMPLEMENTACAO.md` item 1 · `IMPROVEMENTS.md` P0-1

Corpo →

```markdown
## Problema

Hoje o boot é tudo-ou-nada: a falha de um único sensor derruba todas as formas de ejetar. O sistema entra em safe-hold silencioso, buzina para sempre e nunca decola. Uma falha parcial virou falha total, e isso é o erro central: apogeu se detecta só com altitude, free-fall só com aceleração, nenhum dos dois precisa dos dois sensores.

## Evidência

- `FlightControlTask.cpp:386-395`: falha de IMU ou baro faz `initFlightControlTask()` retornar false.
- `firmware.ino:94-118`: init falho vira safe-hold `for(;;)` com buzzer; nenhuma task criada, rádio morto.
- `FlightStateMachine.cpp:98`: `update()` retorna se qualquer sensor não estiver pronto; backstop e contingência também dependem da IMU.

## O que fazer

1. Modos de operação explícitos: `NOMINAL`, `BARO_ONLY` (apogeu por h < maxAlt - Δ sustentado + vz < 0), `IMU_ONLY` (timer de voo desde liftoff + free-fall por |a|), `NO_FLIGHT` (só aqui safe-hold).
2. Decidir: o modo é fixado no boot ou degrada/recupera em pleno voo? O que muda se o sensor falhar depois do liftoff?
3. Telemetria e buzzer vivos em todos os modos (hoje o safe-hold silencia o rádio).
4. Modo degradado visível em terra: campo no pacote de telemetria, linha no CSV, padrão sonoro no pad.

## Critérios de aceite

- [ ] IMU desligada fisicamente no boot: o foguete detecta apogeu e comanda deploy por barômetro (teste inverso também).
- [ ] Modo vigente visível em terra sem abrir o foguete (rádio + buzzer).
- [ ] Drop test com e sem barômetro/IMU operantes documentado.
```

---

## #7 · [P0-3] "Comandei a ejeção" não é "o paraquedas abriu" + canal único de atuação

**Labels:** `P0` `safety-critical` `firmware` `hardware-bench`
**Milestone:** Etapa 2: Redundância de sensor
**Depende de:** #1 (serviço de deploy), #6 (contrato SensorData)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 3 · `IMPROVEMENTS.md` P0-3

Corpo →

```markdown
## Problema

A telemetria reporta a decisão da FSM como se fosse a atuação física: servo desconectado, sem energia ou travado, e o ground station mostra `chute=YES` com o foguete em queda livre. E todos os caminhos de ejeção convergem no mesmo pino PWM: um único mau contato derrota toda a redundância lógica.

## Evidência

- `FlightControlTask.cpp:367-369`: `deployParachute()` é apenas `ParachuteServo.write(SERVO_OPEN)`.
- `FlightControlTask.cpp:355-356`: `data.parachute_deployed = g_fsm->isParachuteDeployed()` (decisão, não atuação).
- `parachute_module.cpp:44-55`: sem leitura de posição, corrente ou continuidade.
- `config.h:96-100`: dois footprints de servo (M1, M2) compartilham o mesmo sinal PWM.
- Servo e MCU na mesma alimentação: pico de corrente da ejeção exatamente quando um brownout seria fatal.

## O que fazer

1. Separar no `SensorData`: `deploy_commanded` (FSM/backstop), `deploy_actuated` (PWM aplicado) e, se houver hardware, `deploy_confirmed` (continuidade/posição/corrente). Definir quais das três fases o hardware atual distingue de verdade.
2. Segundo canal de ejeção independente (outro GPIO, outro driver), comandado pelo backstop.
3. BIT de servo no pad: verificação curta antes do arm (sem ejetar!), com resultado audível. Quem autoriza o BIT e como reporta?
4. Item de bancada: medir VBAT durante a ejeção; avaliar rail separado/capacitor de reserva.
5. Dual deployment (drogue + main) é roadmap: registrar as escolhas de hoje que abrem/fecham essa porta.

## Critérios de aceite

- [ ] Servo desconectado de propósito: o ground station mostra claramente "comandada mas não confirmada".
- [ ] BIT roda no pad, reporta por som, e a equipe confia nele.
- [ ] Medição de VBAT na ejeção documentada (bancada).
```

---

## #8 · [P1-2] IMU: range implícito, sem calibração, saturação descartada, leitura falha injetada como dado

**Labels:** `P1` `firmware` `hardware-bench`
**Milestone:** Etapa 2: Redundância de sensor
**Referência:** `PLANO_IMPLEMENTACAO.md` item 8 · `IMPROVEMENTS.md` P1-2 (+ achado 2026-11-19: `getEvent` sem checagem)

Corpo →

```markdown
## Problema

O acelerômetro nunca recebe range explícito (depende do default da lib); a validação atual descarta amostras fora do limite em vez de sinalizar saturação, perdendo o momento mais intenso do voo; o bias de repouso documentado de +2,81 m/s² não é compensado; e o retorno de `_lsm.getEvent()` nunca é checado: uma transação I2C morta injeta zeros/stale no lugar da medição, e aceleração ~0 é indistinguível de queda livre para o backstop.

## Evidência

- `LSM6DS3Sensor.cpp`: nenhuma chamada de `setAccelRange()`/`setGyroRange()`.
- `LSM6DS3Sensor.cpp:105-111`: `MAX_ACCEL = 200` descarta a amostra.
- `LSM6DS3Sensor.cpp:87`: `_lsm.getEvent(...)` sem checar o retorno (achado 2026-11-19).
- `FlightStateMachine.cpp:154-162`: comentário registra bias +2,81 m/s² sem compensação.

## O que fazer

1. Range explícito e documentado (datasheet: ±16 g) vs. pico do motor (curva do fabricante ou RocketPy). Se a queima superar 16 g: avaliar alto-g (H3LIS331DL/ADXL375) antes de "resolver" com software.
2. Calibração de bias no pad: N amostras antes do arm, offset persistido em NVS. Sem calibração rodada, voar com bias conhecido ou recusar arm?
3. Flag `imu_saturated` no `SensorData` em vez de congelar a amostra. Quem consome a flag: FSM em tempo real ou só análise pós-voo?
4. Leitura falha não é dado: checar retorno de `getEvent()`; marcar `imu_stale`/contador de falhas e excluir do filtro.

## Critérios de aceite

- [ ] Repouso lê ~9,8 m/s² no eixo Z (dentro da incerteza) após calibração em bancada.
- [ ] Movimento acima do range: flag de saturação, não congelamento silencioso.
- [ ] Leitura I2C falha (fio puxado em bancada): reportada como falha, não injetada como dado.
```

---

## #9 · [P2-3] Tempos em ms com `static_assert` (não "ciclos a X Hz")

**Labels:** `P2` `firmware` `process`
**Milestone:** Etapa 3: Estimação e apogeu
**Referência:** `PLANO_IMPLEMENTACAO.md` item 17 · `IMPROVEMENTS.md` P2-3
**Nota:** pré-requisito para a issue #10 (que propõe mudar a frequência do loop).

Corpo →

```markdown
## Problema

O `config.h` expressa janelas críticas de tempo como contagens de ciclos comentadas com a frequência atual ("5 ciclos @ 50 Hz", "13 ciclos // 2.6s @5Hz"). Se alguém mudar a frequência do loop, todos os tempos de segurança mudam juntos e sem aviso.

## Evidência

- `config.h:259`: `FREEFALL_BACKSTOP_CYCLES = 5 // ~1.0s @5Hz`
- `config.h:273`: `BARO_STALE_SUSTAIN_CYCLES = 13 // 2.6s @5Hz`
- `config.h:287`: `PARACHUTE_CONFIRM_CYCLES = 3 // 600ms @5Hz`
- `config.h:227`: `LIFTOFF_CONFIRM_MS` comentado como "1 cycle @5Hz" (drift: o comentário de `FlightStateMachine.cpp:342` ainda diz "5 cycles @ 50Hz")

## O que fazer

1. Inverter a dependência: declarar a intenção em ms (`*_MS`, cada uma com comentário explicando o porquê do número) e derivar os ciclos por `constexpr` a partir do período.
2. `static_assert` em tempo de compilação garantindo janela pretendida ↔ ciclos derivados. Mudar a frequência sem revisar as janelas deve quebrar o build com mensagem legível.
3. Ciclos não-inteiros: arredondar para o lado seguro (janelas de confirmação: para cima).

## Critérios de aceite

- [ ] Mudar a frequência do loop de propósito num commit de teste quebra o build com `static_assert` explicativo.
- [ ] Nenhum comentário "ciclos @ XHz" restante no código.
- [ ] Cada janela em ms tem o porquê documentado (análise/simulação/chute assumido).
```

---

## #10 · [P0-2] Vz é derivada crua e o apogeu é decidido por uma amostra só

**Labels:** `P0` `safety-critical` `firmware`
**Milestone:** Etapa 3: Estimação e apogeu
**Depende de:** #9 (tempos em ms); soft: #8 (calibração IMU)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 2 · `IMPROVEMENTS.md` P0-2

Corpo →

```markdown
## Problema

A velocidade que decide o momento mais crítico do voo é a diferença entre duas leituras barométricas ruidosas, e a decisão de apogeu dispara quando esse número cruza um limiar uma única vez. Com ±0,5 m de ruído barométrico em janelas de 200 ms, o Vz instantâneo carrega ruído de ±2,5 m/s: maior que o limiar de apogeu (1,0 m/s) e da ordem do gate de deploy (-2,0 m/s). A margem vem de sorte estatística, não de estimação. Bônus de desperdício: o barômetro entrega 25 Hz e o loop consome 5 Hz (80% das conversões descartadas).

## Evidência

- `BMP585Sensor.cpp:257-270`: `vz = (altitude - prevAltitude)/dt` sem filtro (só clip ±200 m/s).
- `FlightControlTask.h:40`: loop a 5 Hz; `BMP585Sensor.cpp:40`: ODR 25 Hz.
- `config.h:237` + `FlightStateMachine.cpp:381-383`: `detectApogee()` = `|vz| < 1.0` em um ciclo.

## O que fazer

1. Estimação: filtro α-β (g-h) ou Kalman 1-D com estado (h, vz) e dt real (`esp_timer_get_time()`); predição física + correção pela medição. Ganhos dimensionados pelo ruído medido na sua placa (bancada). Justificar a escolha entre as duas famílias.
2. Decisão: apogeu por consenso: vz_est < 0 por N de M amostras E h < maxAltitude - Δh E tempo mínimo desde burnout/liftoff. Parâmetros escolhidos com método (replay + RocketPy), não no chute.
3. Alinhar o loop de controle ao sensor (20-25 Hz), com todas as janelas já em ms (issue #9).
4. Se incluir az da IMU na predição: o bias de +2,81 m/s² (issue #8) precisa estar compensado.

## Critérios de aceite

- [ ] Replay do voo real + perfis RocketPy com ruído realista: zero falsos apogeus, apogeu declarado no momento fisicamente correto, de forma repetível sob ruído.
- [ ] Taxa de falso apogeu medida antes/depois documentada como métrica de sucesso.
- [ ] Ganhos do filtro documentados com o ruído medido que os originou.
```

---

## #11 · [P1-5] GPS: fix "válido" nunca expira e uma fonte independente de verdade é desperdiçada

**Labels:** `P1` `firmware`
**Milestone:** Etapa 3: Estimação e apogeu
**Depende de:** soft #10 (arquitetura de consenso)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 11 · `IMPROVEMENTS.md` P1-5

Corpo →

```markdown
## Problema

`hasValidFix()` = `location.isValid()` não envelhece: um dado capturado há minutos ainda passa por válido. E o GPS inteiro não participa de nenhuma decisão de voo, apesar de ser a terceira fonte independente de altitude/velocidade.

## Evidência

- `GPSModule.cpp:70-72`: `hasValidFix()` = `_gps.location.isValid()`.
- Grafo de dependências: GPS não aparece em nenhuma aresta de decisão de voo (IMPROVEMENTS.md §1).

## O que fazer

1. Frescor: estudar `isValid`/`isUpdated`/`age` da TinyGPSPlus; definir "fix utilizável" = idade máxima em ms (config, número justificado). Teste: desconectar a antena e cronometrar.
2. Participação: voto de confirmação, nunca gatilho único. Barômetro + estimador opinam sempre; GPS opina com fix fresco; a decisão muda de qualidade quando ele concorda/discorda, mas nunca depende só dele (latência, multipath no pad, perda de sinal em rotação).
3. Altitude GPS vs barométrica: MSL vs elipsoidal, precisam de referência comum antes de comparar.
4. Recuperação: última posição conhecida + timestamp absoluto no log.

## Critérios de aceite

- [ ] Fix antigo não é mais tratado como válido (teste com antena desconectada).
- [ ] Ground station recebe posição do voo inteiro com fix fresco; log carrega timestamp absoluto.
- [ ] O voto do GPS no consenso de apogeu documentado (concorda/discorda/abstém).
```

---

## #12 · [P1-1] Watchdog supervisiona uma task só; safe-hold não supervisiona nada

**Labels:** `P1` `firmware` `observability`
**Milestone:** Etapa 4: Observabilidade
**Referência:** `PLANO_IMPLEMENTACAO.md` item 7 · `IMPROVEMENTS.md` P1-1

Corpo →

```markdown
## Problema

Só a task de controle de voo está registrada no watchdog: telemetria e logger podem travar silenciosamente sem reset. E o laço infinito do safe-hold não alimenta nem registra nada: um safe-hold em campo é uma caixa preta que buzina.

## Evidência

- `FlightControlTask.cpp:458`: único `esp_task_wdt_add(nullptr)`.
- `firmware.ino:112`: safe-hold `for(;;)` sem feed, sem log.

## O que fazer

1. Decidir arquiteturalmente quais tasks são supervisionadas e o que fazer quando cada uma morre (derrubar tudo? registrar e seguir?). Argumentar os dois lados de o safe-hold alimentar o TWDT.
2. Entre o travamento e o reset: salvar um "último suspiro" (estado, motivo), estudando o panic handler do ESP-IDF.
3. Contador de reboots com causa (`esp_reset_reason()`), distinguindo reboot programado de anômalo, exportado na telemetria (base para o boot-counter da issue #16).

## Critérios de aceite

- [ ] Travamento injetado em qualquer task supervisionada produz reset com causa registrada.
- [ ] A causa chega ao ground station no pacote de saúde (issue #14): nenhuma caixa-preta.
```

---

## #13 · [P1-3] Gravar na flash durante o voo causa panic; a "solução" atual é gravar menos

**Labels:** `P1` `firmware`
**Milestone:** Etapa 4: Observabilidade
**Depende de:** soft #4 (posse do rádio/SD)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 9 · `IMPROVEMENTS.md` P1-3

Corpo →

```markdown
## Problema

Escrever no SD durante o voo já causou panic por flash-cache off nos dois núcleos (documentado em comentário); a mitigação foi reduzir o log para 1 Hz, abrindo mão da resolução justamente nas fases críticas. A placa tem 8 MB de PSRAM sendo desperdiçados.

## Evidência

- `TelemetryTask.cpp:217-227`: comentário do panic (bench 2026-08-27) + divisor `flashWriteDivider` (5 Hz → 1 Hz).
- `filesystem_module.h:132-161`: open/append/close por linha (cada open é caro e arriscado).
- NVS: o FSM persiste a cada transição (`FlightStateMachine.cpp:245`): escritas de flash em voo não são só do log.

## O que fazer

1. Buffer-then-flush: acumular frames na PSRAM e descarregar quando seguro (o estado LANDED é candidato natural). Fazer a conta: bytes de um voo a 25 Hz × duração máxima vs. 8 MB.
2. Manter o `File` aberto em vez de open/close por linha.
3. Avaliar binário + CRC com conversor pós-voo (tira formatação de CPU do momento crítico), a filosofia OpenFlightLogger.
4. Task de storage dedicada com fila, isolando o cartão (lento, falho) da telemetria (crítica).
5. Responder: o que acontece com os dados em PSRAM se a bateria morrer antes do flush? Qual perda máxima é aceitável e como mitigá-la (flushes parciais em pontos seguros)?

## Critérios de aceite

- [ ] Voo completo simulado com log em alta taxa na PSRAM + flush único ao pousar: zero panics de flash, zero linhas perdidas.
- [ ] CSV pós-voo idêntico ao que seria gravado linha a linha (validação do pipeline).
```

---

## #14 · [P1-4] Instrumentação de saúde existe e ninguém lê

**Labels:** `P1` `firmware` `observability`
**Milestone:** Etapa 4: Observabilidade
**Depende de:** soft #12 (causa de reset), soft #21 (git hash), #5 (orçamento do enlace)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 10 · `IMPROVEMENTS.md` P1-4

Corpo →

```markdown
## Problema

As estatísticas das três tasks (ciclos, overruns, drops de fila, tempo máx. de execução) estão implementadas e nenhum consumidor as chama. A saúde do sistema só é visível com cabo USB, o que em voo significa invisível.

## Evidência

- `getFlightControlStats()` / `getTelemetryStats()` / `getLoggerStats()`: sem nenhum consumidor (grep).
- `LoggerTask.h:88-90`: `droppedCount` nunca sai da placa.

## O que fazer

Pacote de saúde periódico (~0,2 Hz) por rádio agregando: estatísticas das tasks; `uxTaskGetStackHighWaterMark()` das três; menor heap desde o boot (`esp_get_minimum_free_heap_size()`); causa do último reset (#12); modo degradado vigente (#6); git hash do build (#21); ToA real do rádio (#5).

Decisão de design: o pacote de saúde disputa o rádio como qualquer telemetria, ou tem prioridade garantida quando o enlace está saturado? (O que você mais quer receber quando a saúde está ruim?)

## Critérios de aceite

- [ ] Foguete na mesa, sem nenhum cabo: stack mínimo de cada task, menor heap, causa do último reset, hash do build e modo operacional afirmáveis por rádio.
- [ ] Alertas por limiar definidos (stack < X, heap < Y) no ground station, não só número cru.
```

---

## #15 · [P1-7] Contadores que estouram em voo longo

**Labels:** `P1` `firmware`
**Milestone:** Etapa 4: Observabilidade
**Referência:** `PLANO_IMPLEMENTACAO.md` item 13 · `IMPROVEMENTS.md` P1-7

Corpo →

```markdown
## Problema

`SensorData.packet_count` é uint16_t: a 5 Hz estoura em ~3,6 h; se o loop subir para 25 Hz (issue #10), em ~44 min. E a conversão de tempo de voo usa `millis()`, que volta a zero em ~49,7 dias.

## Evidência

- `SensorData.h:82`: `uint16_t packet_count`.
- `FlightControlTask.cpp:343`: `static_cast<uint16_t>(g_stats.cycleCount)` (trunca em silêncio).
- `SensorData.h:81`: `timestamp` de `millis()`.

## O que fazer

1. Tabela de todos os contadores × largura × taxa → duração até overflow; escolher larguras com folga para a missão máxima.
2. Wrapping de `millis()`: o idiomático `(now - previous) >= interval` (subtração) é wrap-safe; comparação direta "maior que" não é. Auditar todos os usos.
3. Migrar `packet_count` sem invalidar parsers existentes (versionar o formato; ver #21).

## Critérios de aceite

- [ ] Teste com contadores fakeados além do overflow: tempo de voo e contadores continuam monotônicos e coerentes através do wrap.
- [ ] Tabela contador × estouro documentada (inclui o cenário de 25 Hz).
```

---

## #16 · [P0-8] Reboot ou segundo voo destrói o log de voo anterior

**Labels:** `P0` `bug` `firmware`
**Milestone:** Etapa 4: Observabilidade
**Depende de:** soft #12 (contador de boot em NVS)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 22 (achado da revisão 2026-11-19)

Corpo →

```markdown
## Problema

O nome do CSV vem da hora do GPS ("HH_MM_SS-Dados.csv") ou do literal "NOFIX" sem fix: sem data, sem contador de boot, sem sufixo único, e o arquivo é aberto com FILE_WRITE (TRUNCA). Um reboot em pleno voo, ou qualquer segundo voo sem fix de GPS, abre o mesmo arquivo e destrói os dados do voo anterior, justamente a evidência que a auditoria do deploy (#1) precisaria.

## Evidência

- `TelemetryTask.cpp:42-50`: `buildDataFilePath()`: só HH:MM:SS ou "NOFIX".
- `TelemetryTask.cpp:148` + `filesystem_module.h:91-97`: `SD.open(path, FILE_WRITE)` = modo "w", trunca.
- `filesystem_module.h:68`: `LittleFS.begin(true)` formata a partição se a montagem falhar: histórico de voos apagado silenciosamente.
- Cenários: (1) watchdog reinicia na subida → só resta o pedaço pós-reboot; (2) dois voos sem GPS → mesmo "NOFIX-Dados.csv"; (3) mesmo segundo do dia em dias diferentes → colisão.

## O que fazer

1. Nome único por voo: contador de boot persistido em NVS + data do GPS quando houver (reutilizar a infraestrutura do contador de reboots da issue #12).
2. Escrita append-only entre reboots: `FILE_APPEND` + header apenas quando o arquivo é novo.
3. LittleFS: não formatar silenciosamente. Falhar ruidosamente (formatar em campo apaga o histórico sem ninguém pedir).
4. Política para storage cheio (rotação ou alarme no pad).

## Critérios de aceite

- [ ] Reboot em pleno voo simulado: log anterior intacto, novo trecho em arquivo próprio.
- [ ] Dois voos consecutivos sem GPS → dois arquivos distintos.
- [ ] Montagem corrompida de LittleFS não formata em silêncio.
```

---

## #17 · [P1-6] Armar exige cabo USB: comando de arm onde ninguém alcança

**Labels:** `P1` `firmware` `safety-critical`
**Milestone:** Etapa 4: Observabilidade
**Depende de:** #4 (canal de rádio ordenado)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 12 · `IMPROVEMENTS.md` P1-6

Corpo →

```markdown
## Problema

O comando ARM só é aceito pela serial USB, no pad normalmente desconectada. Sem confirmação, sem sequência de segurança, sem estado de arm visível a distância.

## Evidência

- `FlightControlTask.cpp:239-271`: lê `Serial` apenas.

## O que fazer

1. Mover o comando para o LoRa (bidirecional, mesmo enlace do receiver) com two-step ARM: "intenção" → eco do estado → confirmação explícita. Erro de digitação, ruído ou mensagem repetida jamais armam sozinhos. Sequência/nonce/ACK: o que impede replay?
2. Comandos irmãos: `DISARM` (desarmar a distância também é segurança), `STATUS` (consulta sem efeito colateral).
3. Manter a recusa por maxAltitude/paraquedas como invariante do novo canal.
4. Feedback sonoro/visual no pad por estado (desarmado, arm-intent, armado, recusa + motivo): quem está a 10 m precisa saber sem notebook.

## Critérios de aceite

- [ ] Sequência de pad completa sem nenhum cabo: arm em dois passos com eco audível; STATUS responde; DISARM funciona; arm de um passo sozinho é recusado.
```

---

## #18 · [P2-1] Testar o C++ real no host, não uma reimplementação em Python

**Labels:** `P2` `process` `testing`
**Milestone:** Etapa 5: Processo
**Referência:** `PLANO_IMPLEMENTACAO.md` item 15 · `IMPROVEMENTS.md` P2-1

Corpo →

```markdown
## Problema

A validação da FSM é um modelo paralelo em Python que já derrapou da realidade (docs citam 50 Hz num lugar, 5 Hz no outro; comentários citam janelas "@50Hz" que não correspondem ao config) e MASCAROU um bug real de guarda/latch no C++ (ver issue #2: o validador da contingência baro-stale implementa a intenção, não o código embarcado). O que é testado não é o que voa.

## O que fazer

1. PlatformIO com ambiente `native` + Unity: compilar `FlightStateMachine.cpp` e drivers com stubs de Arduino (`millis`, `Serial`, NVS, I2C) e um clock fake determinístico.
2. Replay do CSV de voo real (13_30_11-Dados.csv) contra o C++ de produção. Critério de passe: mesmos eventos, mesma ordem, tolerância de timing.
3. Fault injection em host (impossível/caro em placa): barômetro congelado (regressão da issue #2), NaN, saturação de IMU, reboot imediatamente após cada caminho de deploy (regressão da issue #1), perda de GPS.
4. Começar pela FSM (maior risco: 58 arestas no grafo, betweenness 0,39).

## Critérios de aceite

- [ ] `pio test -e native` roda a FSM de produção + replay do CSV + suíte de fault injection, sem hardware, em segundos, verde.
- [ ] As regressões das issues #1 e #2 estão na suíte.
```

---

## #19 · [P2-2] CI: build reproduzível + análise estática (fim do build manual na IDE)

**Labels:** `P2` `process` `ci`
**Milestone:** Etapa 5: Processo
**Depende de:** #18 (job de testes)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 16 · `IMPROVEMENTS.md` P2-2

Corpo →

```markdown
## Problema

O firmware que voa é compilado à mão, na IDE, na máquina de quem compilou: sem garantia de reprodução e sem portão automático de qualidade entre commit e firmware de voo.

## O que fazer

1. Migrar para PlatformIO (mesma toolchain Arduino-ESP32, scriptável; `platformio.ini` no repo).
2. CI a cada push: compilação com `-Wall -Wextra` e warnings→erro (MEDIR o baseline de warnings antes de ativar `-Werror`; saltos grandes exigem limpeza em etapas), `cppcheck`, `clang-tidy`, `clang-format` (fechar o assunto de estilo: o valor está na consistência), job de testes nativos (#18).
3. Objetivo: merge com CI verde = binário que qualquer um reproduz bit a bit com um comando.

## Critérios de aceite

- [ ] Commit que quebra warning, teste ou análise estática é rejeitado automaticamente.
- [ ] Qualquer membro do time gera o mesmo binário com um comando, sem IDE.
- [ ] O diff gigante do clang-format dividido sem destruir o `git blame` (commits separados por área).
```

---

## #20 · [P2-4] Zero alocação dinâmica/`String` no caminho crítico

**Labels:** `P2` `firmware` `process`
**Milestone:** Etapa 5: Processo
**Referência:** `PLANO_IMPLEMENTACAO.md` item 18 · `IMPROVEMENTS.md` P2-4

Corpo →

```markdown
## Problema

O caminho de execução do voo usa `new` sem verificação e `String` (que aloca por baixo) em pontos críticos. Fragmentação de heap e falha de alocação em plena descida são modos de falha que código de segurança não pode ter (regra 3 do Power of Ten, NASA/JPL).

## Evidência

- `FlightControlTask.cpp:377-397`: `new` sem null-check (em arduino-esp32, `new` OOM devolve NULL → deref de NULL no boot).
- `TelemetryTask.cpp:130`: idem GPS.
- `String` em `logMessage`, `getData`, `assembleTelemetry` (retorno), `file_dir`.

## O que fazer

1. Sensores e FSM como instâncias estáticas (número fixo e conhecido no boot: nada justifica heap).
2. Buffers de texto de tamanho fixo + família `snprintf` no lugar de `String` (mensurar o tamanho máximo real antes de fixar).
3. Heap permitido só na inicialização, onde falha é detectável e safe-hold é barato. Migrar por partes: caminho de voo primeiro (telemetria, FSM, logger).
4. Verificar zero alocação pós-boot (hook/métrica do ESP-IDF → conectar ao pacote de saúde #14).

## Critérios de aceite

- [ ] Instrumentação de alocação mostra zero malloc/new após a inicialização em voo simulado.
- [ ] Grep por `String` no caminho de voo não retorna nada.
```

---

## #21 · [P2-5] Rastreabilidade: com quais parâmetros *esse* voo voou?

**Labels:** `P2` `process` `firmware`
**Milestone:** Etapa 5: Processo
**Referência:** `PLANO_IMPLEMENTACAO.md` item 19 · `IMPROVEMENTS.md` P2-5

Corpo →

```markdown
## Problema

Dois CSVs de dois voos são incomparáveis: não há registro dos thresholds efetivos, do firmware exato (git hash) nem do modo operacional de cada voo.

## O que fazer

1. Bloco de metadados no cabeçalho do CSV (e no pacote de saúde #14): thresholds efetivos lidos dos valores em uso (pós-calibração/pós-override, não dos defaults do código), git hash do build, modo degradado vigente.
2. Hash capturado no build (script gerando `version.h`; pesquisar "embed git hash compile time").
3. Versionar o cabeçalho para não quebrar parsers existentes (ground station, scripts).

## Critérios de aceite

- [ ] Abrindo o CSV de um voo, a primeira seção responde: build (hash), thresholds em uso, modo de voo, sem consultar o repositório.
```

---

## #22 · [P2-6] Docs vivas: matar os drifts e impedir que voltem

**Labels:** `P2` `process` `documentation`
**Milestone:** Etapa 5: Processo
**Depende de:** #23 (pinos auditados primeiro)
**Referência:** `PLANO_IMPLEMENTACAO.md` item 20 · `IMPROVEMENTS.md` P2-6

Corpo →

```markdown
## Problema

Comentários e docs já divergem da realidade em vários pontos, e cada drift corrigido à mão volta, porque nada estrutural impede código e prosa de divergirem.

## Drifts conhecidos

- 50 Hz vs 5 Hz: `firmware.ino:28-29,134` diz 50 Hz; `FlightControlTask.h:40` = 200 ms (5 Hz).
- "Every 20ms": `FlightControlTask.h:6`.
- `overrunCount > 20ms`: `FlightControlTask.h:71` (período real 200 ms).
- Janela "@50Hz" vs config: `FlightStateMachine.cpp:342` vs `config.h:227`.
- `@author Team #100` vs `#11`: `filesystem_module.h:12`.
- Pinos: `config.h:96` ("GPIO7" vs `SERVO_PIN 39`), `filesystem_module.h:53` (SCK=4/2/3 vs real 12/13/11): resolver com a issue #23 primeiro.

## O que fazer

1. Corrigir os drifts na fonte, com uma passada de "o que mais este arquivo afirma sobre o sistema que posso verificar contra o código agora?".
2. Estrutural: prosa não é lugar de dado. Gerar a doc a partir da fonte (tabela de thresholds/pinos gerada do `config.h` por script; CI (#19) falha se a doc gerada não está fresca) ou referenciar o define sem repetir o valor.
3. Decidir o modelo de `@author` (congelado no tempo vs. manutenção real).

## Critérios de aceite

- [ ] Auditoria manual encontra zero divergências código↔docs nos pontos conhecidos.
- [ ] CI falha se alguém editar o código sem regenerar a doc gerada.
```

---

## #23 · [P1-8] Comentários de pinos mentem: auditoria contra o esquemático

**Labels:** `P1` `firmware` `hardware-bench` `documentation`
**Milestone:** Etapa 5: Processo
**Referência:** `PLANO_IMPLEMENTACAO.md` item 14 · `IMPROVEMENTS.md` P1-8

Corpo →

```markdown
## Problema

O código define `SERVO_PIN` como 39 enquanto o comentário diz "GPIO7", e o comentário do SPI do SD cita pinos que não são do barramento real. Em hardware de voo, comentário errado de pino é defeito de segurança esperando alguém confiar nele.

## Evidência

- `config.h:96-100`: "Schematic: GPIO7" vs `#define SERVO_PIN 39`.
- `filesystem_module.h:53`: "SCK=4, MISO=2, MOSI=3" vs barramento real 12/13/11 (`config.h:38-40`).

## O que fazer

1. Auditoria define por define contra o esquemático KiCad, com o hardware que voa como árbitro final (às vezes o firmware compensa erro de layout: decidir caso a caso).
2. Uma única fonte de verdade para pinos (o `config.h`); qualquer menção de pino em prosa vira referência, nunca cópia.
3. Registrar a auditoria (tabela no PR comentado).
4. Fechar junto com #22: tabela de pinos gerada do `config.h`.

## Critérios de aceite

- [ ] Auditoria completa define por define registrada no repositório.
- [ ] Nenhuma menção de pino em prosa sem apontar para a fonte única.
```

---

## Pendências menores (achados da revisão 2026-11-19; tratar dentro das issues acima ou em PRs pequenos)

1. **`_pressure`/`_temperature` escritos antes da validação de altitude** (`BMP585Sensor.cpp:206-212`): uma amostra rejeitada ainda envenena `_pressure`, que é usado pelo re-capture de base no ARM/auto-rezero (a guarda 100-1200 hPa mitiga o extremo, não o meio). Corrigir junto com o #10 (hardening do barômetro).
2. **NVS `persistToNVS()` a cada transição da FSM = escrita de flash em voo** (`FlightStateMachine.cpp:245`): mesmo modo de falha do #13. O design do serviço de deploy (#1) deve sequenciar persiste→atua e medir o stall de I2C em bancada.
3. **`buzzSignal("Activated")` nunca é chamado**: o deploy não tem assinatura sonora própria no pad. Coberto indiretamente por #6/#17 (audibilidade do estado).
```
