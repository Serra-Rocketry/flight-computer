# Diagnóstico do firmware v2.0: o que está errado e como resolver

**Data do diagnóstico:** 2026-11-04 · **Revisão de código:** 2026-11-19 (confirmou os 17 itens e encontrou 3 novos: P0-7, P0-8 e P1-9, seção 5)
**Escopo:** `firmware/` (23 arquivos, ~3.950 linhas)
**Método:** grafo de dependências do código (`graphify-out/graph.json`, 324 nós, 446 arestas) + leitura completa dos arquivos + busca de referências cruzadas.
**Nenhum código foi alterado.** Este arquivo é o diagnóstico. O plano de execução está em `PLANO_IMPLEMENTACAO.md` e as issues prontas para o GitHub em `ISSUES.md`.

**Prioridades:**
- **P0:** risco direto de o paraquedas não abrir em voo.
- **P1:** o sistema voa, mas cego, frágil ou sem rastro do que aconteceu.
- **P2:** processo de engenharia (testes, build, documentação).

Cada item segue o mesmo formato: o problema (com o lugar exato no código), a solução proposta e o porquê importa.

## 1. O que a análise de dependências mostrou

- A `FlightStateMachine` é o maior ponto único de falha: 58 conexões, atravessa metade do sistema. Qualquer mudança nela exige teste de regressão completo.
- O `SensorData` é o contrato de dados entre as tasks: qualquer campo novo (saturação, saúde, confirmação de ejeção) passa por ele.
- O GPS não entra em nenhuma decisão de voo: é só telemetria, sendo a terceira fonte independente de altitude e velocidade.
- As funções de estatísticas das tasks existem, mas nenhum código as chama.
- Duas tasks usam o rádio sem sincronização (ver P0-5).
- Três caminhos de código podem ejetar o paraquedas, mas só um grava o estado (ver P0-4).

## 2. P0: riscos direto ao voo

### P0-1. Se a IMU falhar no boot, o foguete fica sem paraquedas

**O problema.** O boot é tudo-ou-nada: se qualquer sensor falhar, o firmware entra em safe-hold (trava tudo, buzina para sempre) e nenhuma task de voo é criada. Se a IMU falhar, o barômetro sadio vai junto para o lixo. E mesmo que voasse assim, a FSM para quando qualquer sensor não está pronto, e o backstop e a contingência também dependem da IMU. Os três caminhos de ejeção morrem com uma falha só.

O detalhe que torna isso evitável: apogeu se detecta só com altitude. Queda livre se detecta só com aceleração. Nenhum dos dois precisa dos dois sensores.

**No código:**
- `FlightControlTask.cpp:386-395`: IMU ou barômetro falham no init e tudo falha.
- `firmware.ino:94-118`: init falho vira safe-hold com buzzer infinito, sem rádio.
- `FlightStateMachine.cpp:98`: a FSM para se qualquer sensor não estiver pronto.

**A solução.**
1. Modos de operação explícitos: `NOMINAL` (tudo sadio), `BARO_ONLY` (apogeu por altitude caindo, sem acelerômetro), `IMU_ONLY` (timer de voo + queda livre por aceleração, sem barômetro), `NO_FLIGHT` (só aqui o safe-hold).
2. Rádio e buzzer continuam ligados em todos os modos. Hoje o safe-hold desliga o rádio e o foguete vira uma caixa preta.
3. O modo atual aparece na telemetria, no CSV e no padrão do buzzer, para o time em terra saber.

**Por quê.** Numa aviônica de ejeção, o estado seguro é conseguir ejetar, não desligar tudo. Uma falha parcial virou falha total. Referência: Holzmann (NASA/JPL), *The Power of Ten*: https://spinroot.com/gerard/pdf/P10.pdf

### P0-2. A velocidade vertical é ruidosa e o apogeu dispara numa amostra só

**O problema.** A velocidade vertical (vz) é calculada assim: altitude de agora menos altitude de 200 ms atrás, dividido pelo tempo. O barômetro tem ruído de ±0,5 m em janelas de 200 ms, e essa subtração transforma o ruído em ±2,5 m/s de erro em vz. O limiar de apogeu é 1,0 m/s. O ruído é maior que o sinal. Ainda assim, o firmware declara apogeu quando |vz| < 1,0 m/s em um ciclo único: funciona por sorte estatística, não por estimação. O próprio comentário em `FlightStateMachine.cpp:154-162` admite que a quantização faz |vz| cruzar zero antes do apogeu real. E ainda desperdiça: o sensor entrega 25 leituras/s e o loop consome 5 (80% dos dados vão fora).

**No código:**
- `BMP585Sensor.cpp:257-270`: vz é a diferença crua entre duas altitudes, sem filtro.
- `FlightControlTask.h:40`: loop a 5 Hz; `BMP585Sensor.cpp:40`: sensor a 25 Hz.
- `config.h:237` + `FlightStateMachine.cpp:381-383`: apogeu = |vz| < 1,0 numa amostra só.

**A solução.**
1. Estimador de estado (filtro α-β ou Kalman 1-D) com altitude e velocidade: prediz fisicamente onde o foguete deve estar e corrige com a medição. Ganhos calibrados com o ruído medido na placa real, não no chute.
2. Apogeu por consenso, não por amostra única: vz negativo em N de M amostras, altitude abaixo do máximo e tempo mínimo desde burnout/liftoff, tudo junto.
3. Subir o loop para 20-25 Hz (o sensor já entrega) e expressar todos os tempos em ms (ver P2-3).
4. Depois: usar a aceleração da IMU na predição (exige compensar o bias de +2,81 m/s², ver P1-2).

**Por quê.** O momento mais crítico do voo é decidido pelo número mais ruidoso do sistema. Validar antes de confiar: replay do voo real e simulações RocketPy com ruído, medindo falsos apogeus antes e depois. Fontes: filtro α-β (https://en.wikipedia.org/wiki/Alpha_beta_filter), Kalman 1-D (https://bzarg.com/p/how-a-kalman-filter-works-in-pictures/), RocketPy (https://docs.rocketpy.org/), datasheet BMP585 (https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf).

### P0-3. "Comandei a ejeção" não é "o paraquedas abriu"

**O problema.** A telemetria reporta a decisão da FSM como se fosse a atuação física: `data.parachute_deployed = g_fsm->isParachuteDeployed()`. Com o servo desconectado, sem energia ou travado, o ground station mostra `chute=YES` enquanto o foguete cai. Ninguém verifica se o servo realmente se moveu. E todos os caminhos de ejeção convergem no mesmo pino PWM: um mau contato derrota toda a redundância lógica. Para completar, servo e MCU dividem a mesma alimentação: o pico de corrente da ejeção acontece exatamente quando um brownout seria fatal.

**No código:**
- `FlightControlTask.cpp:367-369`: deploy é só `ParachuteServo.write(SERVO_OPEN)`.
- `FlightControlTask.cpp:355-356`: telemetria reporta a decisão, não a atuação.
- `parachute_module.cpp:44-55`: sem leitura de posição, corrente ou continuidade.
- `config.h:96-100`: dois footprints de servo, um só sinal PWM.

**A solução.**
1. Separar três estados no `SensorData`: `deploy_commanded` (a FSM decidiu), `deploy_actuated` (o sinal foi aplicado) e, se houver hardware, `deploy_confirmed` (o servo realmente atuou).
2. Segundo canal de ejeção independente: outro GPIO, outro driver, comandado pelo backstop.
3. Teste do servo no pad antes do arm, curto e audível, sem risco de ejetar.
4. Medir a tensão da bateria durante a ejeção em bancada; avaliar alimentação separada ou capacitor de reserva.
5. Roadmap futuro: dual deployment (drogue no apogeu + main a 150-300 m). Hoje existe um único evento, sem teto de altitude por decisão de projeto (`FlightStateMachine.cpp:392-397`); as escolhas de hoje abrem ou fecham essa porta.

**Por quê.** Ejeção sem realimentação é comando às cegas: ninguém sabe se funcionou até ver o foguete cair. Fontes: códigos de segurança NAR (https://www.nar.org/) e Tripoli (https://tripoli.org/), *Power of Ten* (verificação de pós-condição: https://spinroot.com/gerard/pdf/P10.pdf).

### P0-4. Reboot após a ejeção fecha o paraquedas em pleno voo

**O problema.** Três caminhos de código podem ejetar: a FSM, o backstop e a contingência baro-stale. Só a FSM grava o estado em memória que sobrevive a reboot (NVS). Se o ESP32 reiniciar (watchdog, panic, brownout) logo após uma ejeção pelo backstop ou pela contingência, o firmware acorda sem saber que ejetou: a flag não existe, `setupServo(false)` roda no boot e o servo vai para a posição FECHADA durante a descida. O compartimento fecha e a lógica de deploy rearma.

**No código:**
- `FlightStateMachine.cpp:190-195`: só a FSM persiste.
- `FlightControlTask.cpp:498-504` e `:509-516`: backstop e contingência só marcam RAM, sem `persistToNVS()`.
- `FlightControlTask.cpp:405` + `:413` + `parachute_module.cpp:52`: boot sem a flag fecha o servo.

**A solução.**
1. Um único serviço de ejeção, chamado pelos três caminhos, nesta ordem: trava o latch, grava em NVS, aciona o(s) canal(is), registra origem e motivo, atualiza o `SensorData`. A ordem importa: se o reboot cair entre dois passos, o estado salvo deve ser o mais seguro.
2. Proteger o snapshot: CRC32 (hoje só tem magic + version, `FlightStateMachine.cpp:250-275`) e dois slots alternados com contador de sequência, para tolerar escrita interrompida.
3. Gravar também `maxAltitude` e o motivo do deploy (auditoria pós-voo).
4. Teste obrigatório: reboot forçado logo após cada um dos três caminhos (ver P2-1).

**Por quê.** Esse bug só aparece quando algo já deu errado (o reboot), ou seja, no pior momento possível para descobrir uma segunda falha. Fontes: NVS do ESP-IDF (https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html), watchdog de tarefas (https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html).

### P0-5. Duas tasks disputam o rádio LoRa sem sincronização

**O problema.** Telemetria (Core 0, prioridade 5) e logger (Core 0, prioridade 1) chamam `sendLoRa()` sem proteção nenhuma. A telemetria pode interromper o logger no meio de um pacote: dois payloads misturados em um único frame. E como as mensagens do logger são justamente os erros, o que se perde é o mais importante. O rádio e o cartão SD ainda dividem o mesmo barramento SPI, também sem mutex.

**No código:** `TelemetryTask.cpp:215`, `LoggerTask.cpp:71-73`, `modules/lora_module.h:130-156` (`beginPacket → print → endPacket` sem proteção; a lib LoRa não é thread-safe), `modules/filesystem_module.h` (SD no mesmo SPI, SCK=12/MISO=13/MOSI=11).

**A solução.**
1. Curto prazo: mutex por barramento (rádio + SD) em `sendLoRa()` e nas operações de arquivo. Estudar inversão de prioridade antes de escolher.
2. Melhor: uma única task de rádio com fila de mensagens (telemetria + erros), o padrão dono único que o GPS do próprio firmware já usa. Definir quem tem prioridade quando a fila enche e o que é descartado primeiro.
3. Bônus: TX assíncrono (`LoRa.endPacket(true)` + DIO0) e medição da ocupação real do canal.

**Por quê.** Concorrência sem proteção vira corrupção intermitente: a classe de bug mais cara de reproduzir em campo, porque não aparece em bancada com tráfego calmo. Fonte: *Mastering the FreeRTOS Real Time Kernel* (https://www.freertos.org/Documentation/RTOS_book.html).

### P0-6. A telemetria a 5 Hz não cabe no canal LoRa configurado

**O problema.** LoRa não é um canal onde você manda bytes: é um canal onde você ocupa tempo de rádio, e esse tempo depende do payload e da modulação. Com a configuração atual (SF7, BW 125 kHz, CR 4/5, CRC ligado, `config.h:66-69`), o frame CSV de 22 campos (`TelemetryTask.cpp:82-94`) fica em 140-170 B e leva 231-261 ms no ar. A 5 Hz, o intervalo entre frames é 200 ms: o canal está acima de 100% de ocupação, a task estoura o período em todos os ciclos, e o mecanismo de "descartar o backlog e ficar com a amostra mais nova" (`TelemetryTask.cpp:196-200`) esconde o problema jogando dados fora.

| Payload | Tempo no ar | Ocupação a 5 Hz | Taxa máxima |
|---|---|---|---|
| 100 B | ~174 ms | 87% | 5,7 pkt/s |
| 140 B | ~231 ms | 115% | 4,3 pkt/s |
| 160 B | ~261 ms | 131% | 3,8 pkt/s |
| 200 B | ~318 ms | 159% | 3,2 pkt/s |

**A solução.**
1. Dois formatos: frame binário curto (20-30 B: altitude, vz, maxAlt, estado, flags, contador, CRC) a 5 Hz para o ground station + frame CSV completo a 0,5-1 Hz (ou só no log local). O tempo no ar cai para ~60-90 ms e a meta de ocupação fica abaixo de 50%.
2. Alternativa: reduzir a TX para 1-2 Hz, mantendo 5 Hz só no log local.
3. Medir o tempo real no ar em runtime (`esp_timer_get_time()` em volta de `sendLoRa()`) e publicar na telemetria de saúde (P1-4), para o enlace ser auditável em campo.
4. Antes de mexer em taxa ou potência: verificar os limites de potência e duty cycle da ANATEL para 915 MHz no Brasil. Aumentar potência para caber mais dados pode ser simplesmente ilegal.

**Por quê.** Estourar o orçamento do enlace não degrada a telemetria aos poucos: corrompe em rajadas que o descarte silencioso esconde. Fontes: Semtech AN1200.22 (https://www.semtech.com/uploads/documents/an1200.22.pdf), ANATEL (https://www.gov.br/anatel/pt-br/regulado/certificacao-de-produtos).

## 3. P1: robustez e observação

### P1-1. O watchdog supervisiona uma task só; o safe-hold, nenhuma

**O problema.** Só a task de controle de voo está registrada no watchdog. Telemetria e logger podem travar sem reset e sem aviso. E o laço infinito do safe-hold não registra nada: em campo, é uma caixa preta que buzina.

**No código:** `FlightControlTask.cpp:458` (único `esp_task_wdt_add`), `firmware.ino:112` (safe-hold sem log).

**A solução.** Registrar as três tasks com decisão explícita do que fazer quando cada uma morre (derrubar e reiniciar, ou registrar e continuar); salvar um último registro de diagnóstico antes do reset (estado, motivo), estudando o panic handler do ESP-IDF; contador de reboots com causa (`esp_reset_reason()`), distinguindo reboot programado de anômalo, exportado na telemetria.

**Por quê.** "O foguete reiniciou, não sabemos por quê" é um diagnóstico que o próprio sistema poderia entregar. Fonte: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html

### P1-2. IMU sem range configurado, sem calibração, saturação descartada, leitura falha virando dado

**O problema.** Quatro falhas na IMU:
1. O range do acelerômetro nunca é configurado: depende do default da biblioteca.
2. Amostras acima de `MAX_ACCEL = 200` são descartadas (`LSM6DS3Sensor.cpp:105-111`). O momento mais intenso do voo simplesmente some.
3. O bias de repouso de +2,81 m/s², documentado em comentário (`FlightStateMachine.cpp:154-162`), nunca é compensado.
4. O retorno de `_lsm.getEvent()` não é checado (`LSM6DS3Sensor.cpp:87`, achado de 2026-11-19): se a transação I2C morrer, o código injeta zeros no lugar da medição. E aceleração ~0 é indistinguível de queda livre para o backstop.

**A solução.** Configurar ±16 g explicitamente e comparar com o pico do motor (curva do fabricante ou RocketPy); calibrar o bias com N amostras no pad antes do arm e guardar em NVS (decidir: sem calibração, voar com bias conhecido ou recusar arm?); marcar saturação como flag `imu_saturated` no `SensorData` em vez de congelar a amostra; tratar leitura falha como falha (flag/contador), nunca como dado; se a queima superar 16 g, avaliar acelerômetro de alto-g (H3LIS331DL, ADXL375) antes de tentar resolver por software.

**Por quê.** Amostra saturada é informação ("estou acima do range"), não lixo. E zeros injetados por falha podem enganar o detector de queda livre. Fonte: datasheet LSM6DS3 (https://www.st.com/resource/en/datasheet/lsm6ds3.pdf).

### P1-3. Gravar na flash em voo causa panic; a resposta foi gravar menos

**O problema.** Escrever no SD durante o voo já causou panic por flash cache off nos dois núcleos (documentado em comentário, `TelemetryTask.cpp:217-227`). A mitigação foi reduzir o log para 1 Hz: menos resolução justamente nas fases críticas. A placa tem 8 MB de PSRAM sem uso. E o `filesystem_module.h:132-161` abre, escreve e fecha o arquivo a cada linha: cada open é caro e arriscado. A FSM também grava NVS a cada transição (`FlightStateMachine.cpp:245`): mais escrita de flash em voo.

**A solução.** Acumular frames na PSRAM e descarregar quando seguro (o estado LANDED é o candidato natural); manter o `File` aberto em vez de open/close por linha; gravar binário com CRC e converter para CSV no pós-voo (a filosofia do OpenFlightLogger); task de storage dedicada com fila, isolando o cartão (lento, falho) da telemetria (crítica).

**Por quê.** O dado mais valioso do voo é gravado no momento em que gravá-lo pode derrubar o sistema. Fonte: ESP-IDF External RAM (https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html).

### P1-4. A instrumentação de saúde existe e ninguém lê

**O problema.** `getFlightControlStats()`, `getTelemetryStats()` e `getLoggerStats()` não têm um único consumidor. Overruns, drops de fila e logs perdidos nunca saem da placa. A saúde do sistema só é visível com cabo USB, o que em voo significa invisível.

**A solução.** Pacote de saúde periódico (~0,2 Hz) por rádio: estatísticas das tasks, pilha mínima de cada uma (`uxTaskGetStackHighWaterMark()`), menor heap desde o boot (`esp_get_minimum_free_heap_size()`), causa do último reset (P1-1), modo degradado (P0-1), git hash do build (P2-5). Decidir se ele disputa o rádio como telemetria comum ou tem prioridade garantida quando o enlace está saturado.

**Por quê.** O stack high-water mark mostra o quanto cada task esteve de tocar o fundo da pilha: é a forma padrão de detectar estouro antes que aconteça. Fontes: livro do FreeRTOS (https://www.freertos.org/Documentation/RTOS_book.html), heap do ESP-IDF (https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html).

### P1-5. GPS: fix "válido" nunca expira e o sensor não vota em nada

**O problema.** `hasValidFix()` é só `_gps.location.isValid()` (`GPSModule.cpp:70-72`), que não envelhece: um dado capturado há minutos ainda passa por válido. E o GPS não participa de nenhuma decisão de voo, apesar de ser a terceira fonte independente de altitude e velocidade.

**A solução.** Idade máxima de fix configurável e justificada (usar `location.age()` da TinyGPSPlus; teste: desconectar a antena e cronometrar); com fix fresco, GPS como voto de confirmação no consenso de apogeu (P0-2), nunca gatilho único (latência, multipath no pad, perda de sinal em rotação); atenção à referência de altitude (MSL vs elipsoidal) antes de comparar com o barômetro; última posição conhecida e timestamp absoluto no log, que valem horas de busca em campo.

**Por quê.** Um dado que não expira não é confiável, e uma testemunha independente que nunca depõe é desperdício.

### P1-6. Armar o foguete exige cabo USB

**O problema.** O comando ARM só é aceito pela serial USB (`FlightControlTask.cpp:239-271`), que no pad está desconectada. Sem confirmação, sem sequência de segurança, sem estado de arm visível a distância.

**A solução.** Mover o comando para o LoRa (o enlace já é bidirecional) com arm em dois passos: intenção, eco do estado, confirmação explícita. Um erro de digitação, ruído ou mensagem repetida jamais armam sozinhos. Proteção contra replay (sequência/nonce/ACK). Comandos irmãos: `DISARM` (desarmar a distância também é segurança) e `STATUS` (consulta sem efeito colateral). Manter a recusa por maxAltitude/paraquedas como regra invariante. Feedback sonoro por estado no pad, incluindo o motivo da recusa.

**Por quê.** Um único pacote mal decodificado não pode armar um foguete, e quem está a 10 m precisa saber o estado sem notebook.

### P1-7. Contadores que estouram em voo longo

**O problema.** `SensorData.packet_count` é `uint16_t` (`SensorData.h:82`): a 5 Hz estoura em ~3,6 h; a 25 Hz (se o P0-2 subir o loop), em ~44 min. E o tempo de voo usa `millis()` (`SensorData.h:81`), que volta a zero em ~49,7 dias: comparação direta "maior que" não atravessa o wrap.

**A solução.** Migrar para `uint32_t` versionando o formato do CSV/binário; auditar cada uso de `millis()` para o padrão wrap-safe `(now - previous) >= interval`; tabela de todos os contadores com largura, taxa e tempo até o overflow, cobrindo a missão com folga.

**Por quê.** Contador que estoura no meio da descida gera telemetria incoerente exatamente quando ela importa; se contador e tempo discordarem após um wrap, o bug fantasma que segue não tem fim.

### P1-8. Comentários de pinos mentem

**O problema.** `config.h:96` diz "Schematic: GPIO7" e o define é `SERVO_PIN 39`. O comentário do SPI do SD (`filesystem_module.h:53`) cita SCK=4, MISO=2, MOSI=3 para um barramento real em 12/13/11 (`config.h:38-40`). Em hardware de voo, comentário errado de pino é defeito de segurança esperando alguém confiar nele.

**A solução.** Auditoria define por define contra o esquemático KiCad, com o hardware que voa como árbitro (às vezes o firmware compensa erro de layout: decidir caso a caso); uma única fonte de verdade (o `config.h`); prosa referencia o define, nunca copia o valor; tabela de pinos gerada por script no CI (P2-2).

**Por quê.** Dado duplicado em prosa sempre deriva; em pinografia, deriva com consequência elétrica.

## 4. P2: processo de engenharia

### P2-1. Testar o C++ real, não uma cópia em Python

**O problema.** A FSM é validada por um modelo paralelo em Python que já derrapou da realidade: `firmware.ino:28-29` diz 50 Hz, `FlightControlTask.h:40` diz 5 Hz, e os comentários de `LIFTOFF_CONFIRM_MS` citam "5 cycles @ 50Hz" enquanto `config.h:227` diz "1 cycle @ 5Hz". A revisão de 2026-11-19 mostrou o custo real: o validador da contingência baro-stale arma o latch na ordem certa, o C++ embarcado não, e o teste passou enquanto o firmware estava morto (ver P0-7). O que é testado não é o que voa.

**A solução.** PlatformIO com ambiente `native` + Unity: compilar `FlightStateMachine.cpp` e os drivers com stubs de Arduino (`millis`, `Serial`, NVS, I2C) e clock fake determinístico; replay do CSV de voo real contra o C++ de produção; fault injection que em placa é impossível ou cara: barômetro congelado, NaN, saturação de IMU, reboot logo após cada caminho de deploy (o P0-4 exige esse teste), perda de GPS.

**Por quê.** Testar o artefato, não a imitação dele: o que é validado precisa ser exatamente o que voa. Fontes: https://docs.platformio.org/en/latest/advanced/unit_testing/ · https://github.com/ThrowTheSwitch/Unity

### P2-2. Build manual na IDE; sem CI, sem análise estática

**O problema.** O firmware que voa é compilado à mão na Arduino IDE, na máquina de quem compilou. Nenhuma garantia de que o próximo build é o mesmo, nenhum portão automático de qualidade entre commit e voo.

**A solução.** Migrar para PlatformIO (mesma toolchain Arduino-ESP32, porém scriptável e versionável); CI a cada push: compilação com `-Wall -Wextra` e warnings como erro (medir o baseline antes de ativar `-Werror`), `cppcheck`, `clang-tidy`, `clang-format` e o job de testes nativos (P2-1).

**Por quê.** Merge com CI verde deve significar: qualquer membro do time reproduz o mesmo binário com um comando. Fontes: https://docs.platformio.org/en/latest/ · https://clang.llvm.org/extra/clang-tidy/ · https://cppcheck.sourceforge.io/ · https://spinroot.com/gerard/pdf/P10.pdf

### P2-3. Tempo em ms com `static_assert`, não "ciclos a X Hz"

**O problema.** O `config.h` expressa janelas críticas de tempo como contagens de ciclos comentadas com a frequência atual: `FREEFALL_BACKSTOP_CYCLES = 5 // ~1.0s @5Hz`, `BARO_STALE_SUSTAIN_CYCLES = 13 // 2.6s @5Hz`, `PARACHUTE_CONFIRM_CYCLES = 3 // 600ms @5Hz`. Se alguém mudar a frequência do loop (o que o P0-2 propõe), todos os tempos de segurança mudam juntos e em silêncio.

**A solução.** Declarar a intenção em ms (`*_MS`, cada um com o porquê do número documentado) e derivar os ciclos por `constexpr` a partir do período; `static_assert` em compilação garantindo que janela pretendida e ciclos derivados correspondem: mudar a frequência sem revisar as janelas quebra o build com mensagem legível; ciclos não-inteiros arredondam para o lado seguro (janelas de confirmação: para cima).

**Por quê.** Tempo é tempo; ciclo é detalhe de implementação derivado. Segurança recalibrada em silêncio por uma mudança de frequência é a pior deriva possível, e o build quebrando com mensagem legível é a forma barata de impedi-la.

### P2-4. Alocação dinâmica e `String` no caminho de voo

**O problema.** O caminho de voo usa `new` sem verificação (`FlightControlTask.cpp:377-397`, `TelemetryTask.cpp:130`; em arduino-esp32, `new` sem memória devolve NULL, que é dereferenciado em seguida) e `String`, que aloca por baixo, em `logMessage()`, `getData()`, `file_dir` e no retorno de `assembleTelemetry()`. Falha de alocação e fragmentação de heap em plena descida são modos de falha que código de segurança não pode ter.

**A solução.** Sensores e FSM como instâncias estáticas (quantidade fixa e conhecida no boot: nada justifica heap); buffers de texto de tamanho fixo com `snprintf`; heap permitido só na inicialização, onde falha é detectável e safe-hold é barato; migrar pelo caminho de voo primeiro (telemetria, FSM, logger).

**Por quê.** Regra 3 do *Power of Ten*: sem alocação dinâmica depois da inicialização. Alocação pode falhar no pior momento, e alocação repetida fragmenta: o sistema pode ter memória total sobrando e falhar uma alocação específica. Fonte: https://spinroot.com/gerard/pdf/P10.pdf

### P2-5. Com quais parâmetros esse voo voou?

**O problema.** Dois CSVs de dois voos são incomparáveis: não há registro dos thresholds efetivos, do firmware exato (git hash) nem do modo operacional de cada um.

**A solução.** Bloco de metadados no cabeçalho do CSV e no pacote de saúde: thresholds lidos dos valores em uso no momento (pós-calibração, pós-override, não dos defaults do código), git hash do build capturado em compilação (script gerando `version.h`), modo degradado vigente; versionar o cabeçalho para não quebrar parsers existentes.

**Por quê.** Quando um voo se comportar estranho, a primeira pergunta ("que build, com que parâmetros?") deve ter resposta no primeiro arquivo que você abrir, sem arqueologia de git.

### P2-6. Documentação viva: matar os drifts e impedir que voltem

**O problema.** Código e prosa já divergem em vários pontos: 50 Hz vs 5 Hz (`firmware.ino:28-29,134` vs `FlightControlTask.h:40`), "Every 20ms" (`FlightControlTask.h:6`), `overrunCount > 20ms` com período real de 200 ms (`FlightControlTask.h:71`), janela "@50Hz" (`FlightStateMachine.cpp:342` vs `config.h:227`), autor "#100" vs projeto "#11" (`filesystem_module.h:12`), pinos do P1-8. Cada drift corrigido à mão volta, porque nada estrutural impede código e prosa de divergirem de novo.

**A solução.** Corrigir os drifts na fonte com uma passada de "o que mais este arquivo afirma sobre o sistema que posso verificar contra o código agora?"; gerar as tabelas (thresholds, pinos) a partir do `config.h` por script, com o CI (P2-2) falhando se a doc gerada estiver desatualizada; prosa referencia o define, nunca copia o valor; decidir o modelo de `@author`.

**Por quê.** Prosa não é lugar de dado: todo número repetido em dois lugares deriva, e o fórum onde isso custa mais caro é um manual de hardware de voo.

## 5. Achados da revisão de 2026-11-19

A auditoria confirmou os 17 itens acima e encontrou três defeitos novos (detalhes em `PLANO_IMPLEMENTACAO.md`, itens 21 a 23, e nas issues do `ISSUES.md`).

### P0-7. A contingência baro-stale é código morto

O detector que deveria salvar o voo quando o barômetro congela nunca dispara. A variável que arma o detector (`g_bsLiftoffLatched`) só é avaliada depois do guarda que exige que ela já esteja armada (`FlightControlTask.cpp:170-230`: o guarda das linhas 183-187 roda antes do único trecho que seta o latch, linhas 214-216). Ela nasce `false` (linha 81) e nenhum código a escreve: `checkBaroStaleContingency()` não tem como retornar `true`. Barômetro congela na subida: a FSM não vê apogeu (vz congelado), o backstop não dispara (vz congelado), a contingência é morta: descida balística sem paraquedas, exatamente o cenário que o comentário do próprio código descreve como motivação. O validador Python (`extras/FSM_tester/validate_baro_stale.py:145-146`) arma o latch antes dos guardas, ou seja, implementa a intenção e não o C++ real: por isso o bug passou.

### P0-8. Reboot ou segundo voo destrói o log de voo anterior

O nome do CSV vem da hora do GPS (`HH_MM_SS`) ou do literal "NOFIX" quando não há fix (`TelemetryTask.cpp:42-50`), sem data e sem contador de boot, e o arquivo abre com `FILE_WRITE`, que trunca (`TelemetryTask.cpp:148` + `filesystem_module.h:91-97`). Um reboot em pleno voo abre o mesmo arquivo e apaga o trecho pré-reboot; dois voos sem GPS dividem o mesmo `NOFIX-Dados.csv`; o mesmo segundo do dia em dias diferentes colide. E `LittleFS.begin(true)` (`filesystem_module.h:68`) formata a partição em falha de montagem: histórico de voos apagado em silêncio.

### P1-9. A checagem de `attach()` do servo é versão-dependente

`if (!ParachuteServo.attach(SERVO_PIN))` (`parachute_module.cpp:46`) trata o retorno como bool, mas a ESP32Servo devolve o número do canal PWM, e 0 (primeiro canal LEDC) é sucesso. Hoje o boot funciona porque o caminho MCPWM do ESP32-S3 devolve valores >= 100: por coincidência de alocação, não por contrato. Num fallback LEDC ou numa versão antiga da biblioteca, um servo sadio pode fazer o boot terminar em safe-hold, que significa foguete sem paraquedas. Correção: verificar `ParachuteServo.attached()`.

### Menores

- `_pressure`/`_temperature` são escritos antes da validação de altitude (`BMP585Sensor.cpp:206-212`): uma amostra rejeitada ainda envenena a pressão usada pelo re-capture da base no ARM (a guarda de 100-1200 hPa em `setBasePressure` mitiga o extremo, não o meio).
- `persistToNVS()` roda a cada transição da FSM (`FlightStateMachine.cpp:245`): escrita de flash durante o voo, o mesmo modo de falha do P1-3. O serviço único do P0-4 deve medir esse custo em bancada antes de confiar na ordem "persiste primeiro, atua depois".
- `buzzSignal("Activated")` nunca é chamado: a ejeção não tem assinatura sonora própria no pad.

## 6. Roadmap sugerido

| Etapa | Itens | Como saber que a etapa fechou |
|---|---|---|
| 1: riscos imediatos | P0-4, P0-7, P1-9, P0-5, P0-6 | Reboot após ejeção mantém o servo aberto; contingência baro-stale dispara em teste; boot sem safe-hold esporádico; zero pacotes intercalados; canal abaixo de 50% de ocupação |
| 2: não depender de um sensor só | P0-1, P0-3, P1-2 | Modos BARO_ONLY/IMU_ONLY ejetam em simulação e drop test; teste de servo no pad |
| 3: estimação e apogeu | P2-3, P0-2, P1-5 | Zero falsos apogeus no replay + RocketPy; Vz filtrado com ganhos documentados |
| 4: observabilidade | P1-1, P1-3, P1-4, P1-7, P0-8, P1-6 | Pacote de saúde no ground; log de alta taxa na PSRAM sem apagar o voo anterior; sem panic de flash; arm por rádio em dois passos |
| 5: processo | P2-1, P2-2, P2-4, P2-5, P2-6, P1-8 | CI verde com testes nativos do C++ real e análise estática; docs sem drift; pinos auditados |

## 7. Como revalidar antes de voar

1. Replay do CSV de voo real no C++ de produção, incluindo injeção de falhas (P2-1).
2. Teste de bancada da ejeção com medição de bateria e continuidade (P0-3).
3. Reboot forçado em cada estado da FSM e após cada origem de deploy (P0-4).
4. Teste do enlace: tempo no ar medido, taxa recebida e RSSI no ground station (P0-6).
5. Drop test com e sem barômetro/IMU operantes (P0-1).
6. Barômetro congelado de propósito em plena subida: a contingência baro-stale ejeta (P0-7).
7. Reboot no meio do voo: o log do trecho anterior não é sobrescrito (P0-8).

## Fontes principais (verificadas em 2026-11-04)

- Holzmann, *The Power of Ten*: https://spinroot.com/gerard/pdf/P10.pdf
- FreeRTOS Kernel Book: https://www.freertos.org/Documentation/RTOS_book.html
- ESP-IDF Task Watchdog: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html
- ESP-IDF NVS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- ESP-IDF External RAM (PSRAM): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html
- Semtech AN1200.22 (LoRa ToA): https://www.semtech.com/uploads/documents/an1200.22.pdf
- Filtro α-β: https://en.wikipedia.org/wiki/Alpha_beta_filter
- Kalman visual: https://bzarg.com/p/how-a-kalman-filter-works-in-pictures/
- BMP585 (datasheet Bosch): https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf
- LSM6DS3 (datasheet ST): https://www.st.com/resource/en/datasheet/lsm6ds3.pdf
- PlatformIO Unit Testing: https://docs.platformio.org/en/latest/advanced/unit_testing/
- Unity: https://github.com/ThrowTheSwitch/Unity
- clang-tidy: https://clang.llvm.org/extra/clang-tidy/
- RocketPy: https://docs.rocketpy.org/
- ANATEL (certificação/uso de RF): https://www.gov.br/anatel/pt-br/regulado/certificacao-de-produtos
- NAR / Tripoli (segurança e recuperação): https://www.nar.org/ · https://tripoli.org/
