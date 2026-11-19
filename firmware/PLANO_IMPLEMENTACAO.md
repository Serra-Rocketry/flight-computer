# Plano de Implementação: Melhorias do Firmware v2.0

**Data:** 2026-11-05 · **Revisão:** 2026-11-19
**Base:** `IMPROVEMENTS.md` (diagnóstico de 2026-11-04, 17 itens + 3 achados da revisão)

A revisão de 2026-11-19 verificou item por item contra o código, confirmou os 17 originais e adicionou três: **P0-7** (item 21: contingência baro-stale é código morto), **P0-8** (item 22: reboot ou segundo voo destrói o log anterior) e **P1-9** (item 23: checagem de `attach()` do servo é versão-dependente), além de reforçar o item 8 (P1-2) com o retorno não checado de `getEvent()`. As issues prontas para o GitHub estão em `ISSUES.md`.

**Como ler este plano:** cada item traz o problema, a ideia da solução e as perguntas que precisam de resposta antes de escrever código. Este arquivo não entrega implementação: o "como fazer" é o trabalho de engenharia que ele prepara. Onde existe material de estudo confiável, está listado no final de cada item.

**Ordem de execução:** por prioridade (P0 = risco de voo, P2 = processo). O roadmap de entrega por etapas está no final.

---

## P0: Segurança de voo

### 1. P0-1: IMU falha no boot e o foguete fica sem paraquedas

**O problema.** O boot é tudo-ou-nada: a falha de um sensor derruba todas as formas de ejetar. O sistema entra em safe-hold silencioso, buzina para sempre e nunca decola. Uma falha parcial virou falha total, e esse é o erro central: apogeu se detecta só com altitude, queda livre só com aceleração, e nenhum dos dois precisa dos dois sensores ao mesmo tempo.

**A solução.** O firmware precisa enxergar graus de saúde, em vez de "tudo ou nada". Modos de operação explícitos: nominal, só barômetro, só IMU, e nenhum sensor (só aqui, safe-hold). Cada modo degradado precisa cumprir a missão mínima, ejetar, com as informações que ainda tem. Rádio e buzzer continuam vivos em todos os modos, e o modo atual chega ao time em terra por som, telemetria e CSV.

**Decidir antes de codar:**
- Quem define o modo: a decisão é no boot, ou o sistema pode degradar (e se recuperar?) em pleno voo? E se o sensor falhar depois do liftoff?
- No modo só-barômetro, que evidências valem como "apogeu"? No modo só-IMU, como distinguir burnout de apogeu sem altitude?
- O que o time em terra precisa ouvir e ver para saber em que modo o foguete está?

**Pronto quando:** desligue a IMU no boot e o foguete ainda detecta apogeu e ejeta por barômetro; o teste inverso também passa; o modo vigente é visível em terra sem abrir o foguete.

**Para estudar:**
- *The Power of Ten* (Holzmann, NASA/JPL), em especial detectar e responder a falhas em vez de propagá-las: https://spinroot.com/gerard/pdf/P10.pdf
- "fail operational vs fail safe avionics recovery system"

### 2. P0-2: A velocidade vertical é crua e o apogeu dispara numa amostra só

**O problema.** A velocidade que decide o momento mais crítico do voo é a diferença entre duas leituras barométricas ruidosas, e o apogeu dispara quando esse número cruza um limiar uma única vez. A margem atual vem de sorte estatística, não de estimação. Detalhe de desperdício: o barômetro entrega leituras a 25 Hz e o loop consome 5 Hz; 80% das conversões vão fora.

**A solução.** Duas frentes separadas. Estimação: em vez de aceitar a leitura crua como verdade, mantenha um modelo interno de onde o foguete está e para onde vai, combinando predição física ("subindo a X m/s, em 200 ms devo estar 0,2·X m mais alto") com correção pela medição. Duas famílias clássicas, ambas baratas para um ESP32: filtro α-β (g-h) e Kalman 1-D. Entenda as duas, escolha uma e saiba justificar. Decisão: apogeu por consenso, várias evidências concordando ao longo de um intervalo (velocidade negativa, altitude abaixo do máximo, tempo mínimo desde burnout/liftoff), nunca um cruzamento instantâneo. E alinhar a frequência do loop à do sensor, re-verificando todos os tempos expressos "em ciclos" (item 17/P2-3).

**Decidir antes de codar:**
- Qual o ruído real do barômetro na sua placa, no seu ambiente? Como medir isso em bancada e como o número vira ganho de filtro?
- Se a IMU entrar na predição, o bias de repouso vira problema (o item 8/P1-2 trata da calibração).
- Como validar que o estimador é melhor que a derivada crua, antes de confiar o paraquedas a ele?
- O que é "consenso" em números: quantas amostras, em qual janela, com quais condições mínimas, e como escolher sem chutar?

**Pronto quando:** o replay de um voo real (ou simulado com ruído realista) declara apogeu no momento fisicamente correto, de forma repetível, mesmo com ruído injetado. A taxa de falso apogeu medida antes/depois é a métrica de sucesso.

**Para estudar:**
- Filtro α-β (g-h): https://en.wikipedia.org/wiki/Alpha_beta_filter
- Kalman 1-D visual: https://bzarg.com/p/how-a-kalman-filter-works-in-pictures/
- RocketPy (gerar dados de validação com ruído realista): https://docs.rocketpy.org/
- Datasheet BMP585 (ruído, ODR e filtro IIR interno, que mudam o desenho do estimador): https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf

### 3. P0-3: "Comandei a ejeção" não é "o paraquedas abriu"

**O problema.** A telemetria reporta a decisão da FSM como se fosse a atuação física: servo desconectado, sem energia ou travado, o ground station mostra `chute=YES` com o foguete em queda livre. E todos os caminhos de ejeção convergem no mesmo pino PWM: um mau contato derrota toda a redundância lógica. Servo e MCU dividem a mesma alimentação: o pico de corrente da ejeção acontece exatamente quando um brownout seria fatal.

**A solução.** Tratar a ejeção como sistema com realimentação, não comando de mão única. Três momentos hoje fundidos numa flag só: decidi ejetar, apliquei o sinal, confirmei que atuou. O contrato de dados (`SensorData`) precisa refletir essa separação, para o time em terra saber em que fase as coisas pararam. Paralelamente, redundância real: se todos os caminhos lógicos convergem no mesmo fio, nenhum mau contato sobrevive à redundância; um segundo canal independente (outro GPIO, outro driver, comandado pelo caminho de segurança) muda a falha de ponto único para falha detectável. Por fim, o hábito de teste no pad: verificação curta antes do arm, sem ejetar, com resultado audível. A elétrica é item de bancada: medir, não assumir.

**Decidir antes de codar:**
- Quais das três fases (comandada/acionada/confirmada) o hardware atual distingue de verdade? Quanto custa a terceira (continuidade? posição? corrente?) e vale o custo?
- Como desenhar o teste do servo sem risco de ejeção acidental? Quem autoriza e como reporta?
- O segundo canal: driver diferente, alimentação diferente, ou ambos? O que o backstop comanda versus o que a FSM comanda?
- Dual deployment (drogue + main) é roadmap, não agora, mas as escolhas de hoje fecham ou abrem essa porta?

**Pronto quando:** com o servo desconectado de propósito, o ground station mostra "comandada mas não confirmada"; o teste do servo roda no pad, reporta por som, e a equipe confia nele.

**Para estudar:**
- NAR: https://www.nar.org/ · Tripoli: https://tripoli.org/
- *Power of Ten* (verificação de pós-condição): https://spinroot.com/gerard/pdf/P10.pdf

### 4. P0-4: Reboot após a ejeção fecha o paraquedas em pleno voo

**O problema.** Dos três caminhos que podem ejetar, só um grava o estado em memória não-volátil. Se o ESP32 reiniciar (watchdog, panic, brownout) logo após uma ejeção pelos outros dois, o firmware acorda sem saber que ejetou e move o servo para a posição fechada durante a descida, rearmando a lógica de deploy.

**A solução.** Um único ponto de entrada para a ejeção, chamado pelos três caminhos, com a sequência sempre igual: primeiro garantir que o estado sobrevive a reboot (persistir), depois acionar o hardware, e registrar origem e motivo. A ordem importa: se o reboot cair entre dois passos, o estado salvo deve ser o mais seguro. Segunda frente: integridade da persistência. O snapshot atual não se protege contra escrita interrompida no meio (energia caiu, reset durante a gravação); as técnicas clássicas são verificação de integridade do bloco (CRC) e gravação alternada em slots com contador de sequência. Aproveitar para definir o que mais merece sobreviver além da flag de deploy (maxAltitude, motivo) para auditoria pós-voo.

**Decidir antes de codar:**
- Qual a ordem correta das operações no serviço único, e o que acontece em cada janela entre passos se o reset cair ali?
- Como testar de forma determinística um reboot no meio da gravação (fault injection; o item 15/P2-1 cria a infraestrutura)?
- O snapshot novo precisa ser lido por firmware antigo (ou vice-versa)? Como versionar?
- O que a NVS do ESP-IDF já garante (atomicidade, wear leveling) e o que ainda é sua responsabilidade?

**Pronto quando:** para cada um dos três caminhos, um reboot forçado logo após a ejeção (em bancada, de propósito) deixa o servo aberto, não rearma a lógica, e o motivo sobrevive para leitura pós-voo.

**Para estudar:**
- ESP-IDF NVS: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- ESP-IDF Task Watchdog (a origem provável desses reboots): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html

### 5. P0-5: Duas tasks disputam o rádio (e o SPI do SD) sem sincronização

**O problema.** Telemetria e logger chamam a rotina de transmissão LoRa de tasks com prioridades diferentes, sem proteção nenhuma, sobre um barramento SPI compartilhado com o cartão SD. O resultado possível é uma preempção no meio de um pacote: dois payloads intercalados em um frame, corrompendo justamente as mensagens de erro, que são as mais importantes.

**A solução.** Existe uma solução rápida e uma certa, e vale entender as duas. A rápida: sempre que um recurso é compartilhado entre tasks, o acesso precisa ser mediado; estude como o FreeRTOS trata exclusão mútua e os problemas clássicos (inversão de prioridade) que um mutex mal configurado introduz. A certa: repense a posse do recurso. O padrão dono único: em vez de várias tasks disputando o rádio, uma task fala com ele e as demais entregam mensagens por fila. Isso elimina a contenção por construção, e o GPS do próprio firmware já funciona assim: o padrão existe no projeto, falta estendê-lo. Como rádio e SD moram no mesmo barramento, defina o escopo da proteção: task, barramento, ou ambos. Bônus: o modo de transmissão bloqueante mantém a task presa pelo tempo inteiro do pacote no ar; existe um modo assíncrono com interrupção de conclusão.

**Decidir antes de codar:**
- O escopo do mutex: só o rádio, ou o barramento SPI inteiro (incluindo SD)? O que cada escolha custa em tempo de espera máxima?
- A fila aceita telemetria e erro? Quem tem prioridade quando a fila enche? O que é descartado primeiro e por quê?
- Como capturar a evidência de um pacote intercalado, antes e depois da correção?

**Pronto quando:** uma sessão de rajada de telemetria com erros simultâneos produz zero pacotes intercalados, e as mensagens de erro chegam inteiras ao ground station.

**Para estudar:**
- *Mastering the FreeRTOS Real Time Kernel*, capítulos de mutexes, exclusão mútua, inversão de prioridade e filas: https://www.freertos.org/Documentation/RTOS_book.html

### 6. P0-6: A telemetria a 5 Hz não cabe no canal LoRa configurado

**O problema.** Pela fórmula de time-on-air da modulação LoRa, o frame CSV atual (140-170 B) leva mais tempo no ar do que o intervalo entre frames a 5 Hz. O canal está acima de 100% de ocupação, a task estoura o período em todos os ciclos, e o mecanismo de "descartar e ficar com a amostra mais nova" mascara o problema jogando dados fora.

**A solução.** Internalize o orçamento: um enlace LoRa é um canal onde você ocupa tempo de rádio, e esse tempo depende do tamanho do payload e da modulação (SF, BW, CR). Faça as contas para a sua configuração; a nota de aplicação da Semtech tem a fórmula exata. Depois, redesenhe o tráfego para dois públicos: o ground station em tempo real precisa de poucos bytes, muitas vezes por segundo (formato binário compacto com verificação cabe em poucas dezenas de bytes), e a análise pós-voo precisa do frame completo, que pode ir mais devagar ou nem ir por rádio (o item 9/P1-3 trata do log local). Antes de tocar em taxa ou potência, verifique o quadro regulatório: 915 MHz no Brasil tem regras de potência e duty cycle; aumentar potência para caber mais dados pode ser simplesmente ilegal.

**Decidir antes de codar:**
- Quais campos o ground station realmente usa em tempo real? O que é essencial a 5 Hz e o que pode ir a 0,5-1 Hz, ou só depois no CSV?
- Como medir o tempo no ar real (não o teórico) em runtime e publicar na telemetria de saúde, para o enlace ser auditável em campo?
- Se o frame binário curto for o caminho: como versionar o formato para receptor e transmissor não desincronizarem entre builds?
- Quais os limites da ANATEL para a sua configuração, e quem na equipe é responsável por essa verificação?

**Pronto quando:** com o tempo no ar medido em runtime, a ocupação do canal fica confortavelmente abaixo de 50% durante um voo inteiro simulado, sem perda de amostras da fila de telemetria.

**Para estudar:**
- Semtech AN1200.22, *LoRa Modulation Basics*: https://www.semtech.com/uploads/documents/an1200.22.pdf
- ANATEL: https://www.gov.br/anatel/pt-br/regulado/certificacao-de-produtos

### 21. P0-7: [Revisão 2026-11-19] A contingência baro-stale é código morto

**O problema.** O detector que deveria salvar o voo quando o barômetro congela nunca dispara: a variável que o arma (`g_bsLiftoffLatched`) só é avaliada depois do guarda que exige que ela já esteja armada. Ela nasce `false` e nunca muda. Barômetro congela na subida: a FSM não vê apogeu (vz congelado), o backstop não dispara (vz congelado), a contingência é morta: descida balística sem paraquedas, exatamente o cenário que o comentário do próprio código descreve como motivação.

**Evidência (`FlightControlTask.cpp:170-230`):** em voo normal (baro fresco), a função retorna nas linhas 176-179, antes de qualquer leitura da IMU; com o baro travado, o guarda das linhas 183-187 rejeita a execução antes do único trecho que seta o latch (linhas 214-216); o latch nasce `false` na linha 81 e nenhum outro código o escreve. O validador Python (`extras/FSM_tester/validate_baro_stale.py:145-146`) arma o latch antes dos guardas: implementa a intenção, não o C++ real. Por isso o bug passou: testa-se o modelo, não o artefato (item 15/P2-1).

**A solução.** A ordem importa: o latch "o voo começou" precisa ser atualizado todo ciclo, independente do estado do barômetro (a aceleração da IMU não depende dele), e só então os guardas de staleness/altitude se aplicam. Revalidar a janela de 2,6 s contra o CSV de voo real, agora que o latch pode existir desde o liftoff real. E a classe deste bug (ordem entre guardas e atualização de estado) só é pega com confiança por fault injection contra o C++ real: um replay com barômetro congelado de propósito em plena subida vira o teste de regressão permanente.

**Decidir antes de codar:**
- Latch por aceleração crua ou filtrada? O latch em si não ejeta: qual a pior consequência de armá-lo cedo demais, e o que a guarda de altitude/maxAltitude já cobre?
- A janela de 2,6 s continua apropriada com o latch desde o liftoff real? Revalidar contra o CSV.
- Como demonstrar em bancada, sem ferramentas extras, que a contingência dispara (congelar o `update()` do barômetro por software)?

**Pronto quando:** com o barômetro congelado de propósito (por software) após um liftoff simulado, o paraquedas é comandado pela contingência dentro da janela; e o teste roda no host (item 15) como regressão permanente.

**Para estudar:**
- A própria `checkBaroStaleContingency()` e seu comentário de intenção: a divergência entre intenção e implementação é o achado.
- `extras/FSM_tester/validate_baro_stale.py`: comparar a ordem latch/guardas do Python com a do C++.

### 22. P0-8: [Revisão 2026-11-19] Reboot ou segundo voo destrói o log de voo anterior

**O problema.** O nome do arquivo CSV vem da hora do GPS (`HH_MM_SS`) ou do literal "NOFIX" quando não há fix, sem data, sem contador de boot, sem sufixo único, e o arquivo é aberto com `FILE_WRITE` (trunca). Um reboot em pleno voo, ou qualquer segundo voo sem fix de GPS, abre o mesmo arquivo e destrói os dados do voo anterior justamente quando eles mais importam (auditoria do deploy, item 4/P0-4).

**Evidência:**
- `TelemetryTask.cpp:42-50`: `buildDataFilePath()` usa só `HH:MM:SS` ou "NOFIX".
- `TelemetryTask.cpp:148` + `filesystem_module.h:91-97`: `SD.open(path, FILE_WRITE)` é modo "w": trunca arquivo existente.
- `filesystem_module.h:68`: `LittleFS.begin(true)` formata a partição se a montagem falhar: histórico apagado em silêncio.
- Cenários concretos: (1) watchdog reinicia na subida, e o pós-voo encontra só o pedaço pós-reboot; (2) dois voos sem GPS dividem o mesmo `NOFIX-Dados.csv`; (3) mesmo segundo do dia em dias diferentes colide.

**A solução.** O log é a caixa-preta do sistema: precisa ser append-only entre reboots e único entre voos. Nome único por contador de boot persistido em NVS + data do GPS quando houver (o contador será necessário de qualquer forma para o histórico de reboots do item 7/P1-1: mesma infraestrutura). Escrita com `FILE_APPEND` + header apenas quando o arquivo é novo. E questione a formatação automática do LittleFS: formatar em campo apaga o histórico sem ninguém pedir; prefira falhar ruidosamente.

**Decidir antes de codar:**
- Onde vive o contador de boot (mesma NVS do snapshot do item 4: cuidado com contenção de escrita e formato do slot)?
- O parser do ground station e os scripts de análise dependem do padrão atual de nome de arquivo? (conecta com o item 19/P2-5)
- Storage cheio: rotação, ou alarme audível no pad?

**Pronto quando:** reboot forçado em pleno voo simulado deixa o log anterior intacto com o novo trecho em arquivo próprio; dois voos consecutivos sem GPS produzem dois arquivos distintos; montagem corrompida do LittleFS não formata em silêncio.

**Para estudar:**
- ESP-IDF NVS (contador de boot persistente): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- LittleFS Arduino: semântica de `begin(autoFormat)` e o custo silencioso de formatar.

---

## P1: Robustez e observabilidade

### 7. P1-1: O watchdog supervisiona uma task só; o safe-hold, nenhuma

**O problema.** Só a task de controle de voo está registrada no watchdog. Telemetria e logger podem travar silenciosamente sem reset, e o laço infinito do safe-hold não alimenta nem registra nada: um safe-hold em campo é uma caixa preta que buzina.

**A solução.** O watchdog de task do ESP-IDF só supervisiona quem se registra nele. Decida a cobertura como decisão de arquitetura: quais tasks são supervisionadas, e o que o sistema faz quando cada uma deixa de responder. Nem todo travamento merece o mesmo tratamento: derrubar e reiniciar tudo, ou registrar e continuar? Reflita também sobre a janela entre o travamento e o reset: a oportunidade de salvar um último suspiro de diagnóstico (estado, motivo), estudando o fluxo de panic handler do ESP-IDF. Por fim, o histórico: um contador de reboots com a causa de cada um, exportado na telemetria, transforma "o foguete reiniciou em voo, não sabemos por quê" em "reiniciou por TWDT na task X no segundo 43".

**Decidir antes de codar:**
- As três tasks no TWDT, ou existe razão para deixar alguma de fora? Se o logger travar, o voo continua seguro?
- O que preservar no momento do panic, e para onde (a NVS tem a mesma contenção do item 4)?
- Como distinguir reboot "programado/aceitável" de "anômalo" no contador?
- O safe-hold deveria alimentar o watchdog? Argumente os dois lados antes de decidir.

**Pronto quando:** um travamento injetado em qualquer task supervisionada produz reset com causa registrada, e a causa chega ao ground station no pacote de saúde (item 10/P1-4). Nenhuma caixa-preta.

**Para estudar:**
- ESP-IDF Task Watchdog: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html
- FreeRTOS Kernel Book (notificações/timers para o padrão do último suspiro): https://www.freertos.org/Documentation/RTOS_book.html

### 8. P1-2: IMU em range implícito, sem calibração, saturando sem avisar

**O problema.** O acelerômetro nunca recebe range explícito (depende do default da biblioteca), a validação descarta amostras fora do limite em vez de sinalizar saturação (perdendo o momento mais intenso do voo), o bias de repouso documentado de +2,81 m/s² nunca é compensado, e o retorno de `getEvent()` não é checado (achado da revisão): uma transação I2C morta injeta zeros no lugar da medição, e aceleração ~0 é indistinguível de queda livre para o backstop.

**A solução.** Quatro frentes independentes. Configuração: torne o range uma decisão explícita e documentada (pesquise no datasheet as opções e o trade-off resolução vs. limite: a queima do seu motor cabe em qual?). Calibração: o bias de repouso é mensurável; colete N amostras no pad antes do arm, caracterize o offset e persista em NVS. Sinalização: amostra saturada não é lixo, é informação ("estou acima do range"); marcar `imu_saturated` muda completamente o que a FSM pode inferir durante a queima, em comparação com congelar a última leitura válida. Leitura falha não é dado: checar o retorno de `getEvent()`, marcar o dado como inválido (flag/contador) e excluir dos filtros, nunca entrar neles. Se a queima superar o range máximo, existe uma família de acelerômetros de alto-g feita para isso: avalie antes de "resolver" com software.

**Decidir antes de codar:**
- Qual o pico de aceleração do motor (curva do fabricante ou RocketPy) vs. os ranges do LSM6DS3?
- Onde o offset calibrado vive (NVS, mesma infraestrutura do item 4)? Sem calibração rodada, voar com bias conhecido ou recusar arm?
- A saturação durante a queima afeta a FSM na hora, ou só a análise pós-voo? Quem consome a flag?

**Pronto quando:** no banco, com range configurado e calibração rodada, o repouso lê ~9,8 m/s² no eixo Z (dentro da incerteza do sensor); movimento brusco acima do range resulta em flag de saturação, não em congelamento silencioso; leitura I2C falhando é reportada como falha.

**Para estudar:**
- Datasheet LSM6DS3 (ranges, sensibilidade, registers): https://www.st.com/resource/en/datasheet/lsm6ds3.pdf
- Acelerômetros de alto-g (H3LIS331DL, ADXL375): quando 16 g não é suficiente.

### 9. P1-3: Gravar na flash em voo causa panic; a resposta foi gravar menos

**O problema.** Escrever no SD em voo já causou panic por flash cache off nos dois núcleos (documentado em comentário no código), e a mitigação foi reduzir a taxa de log para 1 Hz: abrir mão da resolução justamente nas fases mais críticas. A placa tem 8 MB de PSRAM sendo desperdiçados.

**A solução.** O padrão buffer-then-flush: acumular os frames de voo em memória volátil e descarregar quando a fase crítica acabar (o estado LANDED é o candidato natural). Repense o padrão de acesso: o código atual abre, escreve e fecha a cada linha, e cada open/close é custoso e arriscado, pago uma vez só se o arquivo ficar aberto. Questione o formato: gravar CSV direto obriga formatação em placa a cada linha; gravar binário bruto com verificação e converter para CSV no pós-voo tira a CPU do momento crítico (a filosofia do OpenFlightLogger e de flight computers maduros). Por fim, uma task de storage dedicada com fila isola o cartão (lento, falho) da telemetria (crítica), e conecta com a discussão de posse de recursos do item 5/P0-5.

**Decidir antes de codar:**
- Quanto um voo inteiro gera a 25 Hz no seu formato? Cabe em 8 MB? E se o voo durar o dobro do previsto?
- O que acontece com os dados em PSRAM se a bateria morrer antes do flush? Qual perda máxima é aceitável, e como mitigá-la (flushes parciais em pontos seguros)?
- Binário + CRC: qual layout, como versionar, e quem mantém o conversor pós-voo?

**Pronto quando:** um voo completo (real ou simulado) com log em alta taxa na PSRAM e flush único ao pousar: zero panics de flash, zero linhas perdidas, CSV pós-voo idêntico ao que seria gravado linha a linha.

**Para estudar:**
- ESP-IDF External RAM (PSRAM): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html
- "flight computer black box buffer architecture": o problema de gravar telemetria sem morrer tentando é antigo na aviação experimental.

### 10. P1-4: A instrumentação de saúde existe e ninguém lê

**O problema.** As estatísticas das três tasks (ciclos, overruns, drops de fila, tempo máximo de execução) estão implementadas e nenhum consumidor as chama. A saúde do sistema só é visível com cabo USB conectado, o que em voo significa invisível.

**A solução.** Um pacote de saúde periódico, de baixa frequência (~0,2 Hz), por rádio, agregando o que já existe espalhado: estatísticas das tasks, o stack high-water mark de cada uma (diz o quanto a pilha esteve de tocar o fundo: a forma padrão de detectar estouro de stack antes que aconteça), o menor heap já visto desde o boot, a causa do último reset (item 7/P1-1), o modo degradado (item 1/P0-1) e o git hash do build (item 19/P2-5: sem ele, você não sabe que firmware estava voando). A decisão de design: o pacote de saúde disputa o rádio como telemetria comum (item 6/P0-6), ou tem prioridade garantida quando o enlace está saturado? Pense no que você mais quer receber quando as coisas estão indo mal por causa da saúde ruim.

**Decidir antes de codar:**
- Qual frequência: suficiente para pegar um problema em voo, sem virar ruído no enlace?
- Quais campos são essenciais no pacote mínimo? Corte por valor de diagnóstico real.
- Como apresentar no ground station: número cru, ou alertas por limiar (stack < X, heap < Y)?

**Pronto quando:** com o foguete na mesa e sem nenhum cabo, você afirma por rádio: stack mínimo de cada task, menor heap, causa do último reset, hash do build e modo operacional.

**Para estudar:**
- FreeRTOS Kernel Book, `uxTaskGetStackHighWaterMark`: https://www.freertos.org/Documentation/RTOS_book.html
- ESP-IDF (heap, reset reasons): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html

### 11. P1-5: GPS: fix que nunca expira e um sensor que não vota em nada

**O problema.** A validação atual do fix (`location.isValid()`) não envelhece com o tempo: um dado capturado há minutos ainda passa por válido. E o GPS inteiro não participa de nenhuma decisão de voo, apesar de ser a terceira fonte independente de altitude e velocidade do sistema.

**A solução.** Duas camadas. Frescor: pesquise na TinyGPSPlus a diferença entre `isValid`, `isUpdated` e `age`, e defina o que "fix utilizável" significa em milissegundos de idade máxima (config, número justificado). Participação: com fix fresco, o GPS entra na decisão de voo pelo padrão certo: voto de confirmação, nunca gatilho único. A arquitetura de fusão é um comitê: barômetro e estimador (item 2/P0-2) opinam sempre; o GPS opina quando tem fix fresco; a decisão muda de qualidade quando ele concorda ou discorda, mas nunca depende só dele (latência, multipath no pad, perda de sinal em rotação: ele é testemunha, não juiz). E o bônus de recuperação: última posição conhecida e timestamp absoluto no log valem horas de busca em campo.

**Decidir antes de codar:**
- Qual idade máxima de fix você aceita como "fresco"? Onde o número vive (config) e como foi escolhido?
- Como o voto entra no consenso de apogeu do item 2: concorda, discorda, abstém? O que acontece quando ele discorda do estimador?
- A altitude GPS (geodésica vs. ortométrica) é comparável com a barométrica direto, ou precisa de referência comum? (pesquise "GPS altitude MSL vs ellipsoid")

**Pronto quando:** um fix antigo não passa mais por válido (desconecte a antena e cronometre); com fix fresco, o ground station recebe a posição ao longo do voo inteiro e o log carrega timestamp absoluto.

**Para estudar:**
- Documentação da TinyGPSPlus: semântica exata de `isValid`/`isUpdated`/`age`.
- "sensor fusion voting architecture": o padrão de voto entre fontes independentes.

### 12. P1-6: Armar exige cabo USB: o comando de arm fica onde ninguém alcança

**O problema.** O comando ARM só é aceito pela serial USB, que no pad está desconectada. Sem confirmação, sem sequência de segurança, sem forma de saber o estado de arm a distância.

**A solução.** Mover o canal de comando para o LoRa (que já existe e já é bidirecional: o receptor do ground station é o mesmo enlace) e tratar segurança de comando como requisito de primeira classe. O padrão two-step arm: um comando de intenção ("quero armar") que recebe eco do estado, seguido de uma confirmação explícita; o objetivo é que erro de digitação, ruído no rádio ou mensagem repetida jamais armem o foguete sozinhos. Comandos irmãos fazem parte do desenho: `DISARM` (desarmar a distância também é segurança), `STATUS` (consulta sem efeito colateral, para checagem de pad). A regra atual de recusa (não armar se maxAltitude/paraquedas indicam que já voou) está correta: mantenha como invariante do novo canal. E a camada humana: arm precisa ser visível e audível no pad; quem está a 10 m precisa saber o estado sem consultar notebook.

**Decidir antes de codar:**
- O protocolo precisa de sequência/nonce/ACK? O que impede um replay ou duplicação do pacote de armar?
- O comando por USB fica como fallback de bancada, ou é removido por superfície de ataque?
- Quem tem autoridade para armar: qualquer um com o rádio certo, ou existe chaveamento adicional?
- Qual feedback sonoro/visual no pad para cada estado (desarmado, arm-intent, armado, recusa e motivo)?

**Pronto quando:** uma sequência completa de pad sem nenhum cabo: arm por rádio em dois passos com eco audível, `STATUS` respondendo, `DISARM` funcionando, e uma tentativa de arm de um passo sozinho sendo recusada.

**Para estudar:**
- "two-step arm interlock remote command safety" e padrões de comando/ack em telemetria de foguetemodelismo.
- O protocolo LoRa do próprio projeto: estude o formato de mensagens atual antes de estendê-lo.

### 13. P1-7: Contadores que estouram em voo longo

**O problema.** O contador de pacotes do `SensorData` é `uint16_t`: a 5 Hz estoura em ~3,6 h, e a 25 Hz (se o item 2 subir o loop) em ~44 min. E a conversão de tempo de voo usa `millis()`, que volta a zero em ~49,7 dias: comparação direta "maior que" não atravessa o wrap.

**A solução.** O clássico problema de largura de campo vs. taxa vs. duração de missão: para cada contador, a conta "bits × frequência = duração até o overflow", escolhendo larguras que cubram a missão com folga (campanhas de teste podem ser mais longas que o voo nominal). O segundo problema é sutil: `millis()` volta a zero; o idioma wrap-safe é a comparação de diferença com subtração (`(long)(current - previous) >= interval` funciona através do wrap; "maior que" direto não funciona). Se contador de pacotes e tempo de voo discordarem depois de um wrap, você vai debugar um fantasma.

**Decidir antes de codar:**
- Liste todos os contadores e larguras: qual o primeiro a estourar num voo de 10 min? E num de 2 h?
- Onde o tempo de voo é derivado de `millis()`, e a derivação é wrap-safe?
- O `packet_count` novo quebra o formato do CSV/binário? Como migrar sem invalidar logs antigos?

**Pronto quando:** um teste com contadores simulados além do overflow mostra tempo de voo e contadores coerentes e monotônicos através do wrap.

**Para estudar:**
- "millis() overflow arduino": o idioma `(long)(current - previous) >= interval` e por que ele é seguro.
- "integer overflow rates": para construir a tabela bits × taxa × duração.

### 14. P1-8: Comentários de pinos mentem: documentação divergente do código

**O problema.** O código define `SERVO_PIN` como 39 enquanto o comentário diz "GPIO7", e os pinos citados para o SPI do SD não são os do barramento real. Em hardware de voo, comentário errado de pino não é deslize de documentação: é defeito de segurança esperando alguém confiar nele.

**A solução.** O problema real não é esses dois comentários: é que a verdade sobre pinos está duplicada em prosa em vários lugares, e toda duplicação deriva. A correção estrutural é uma única fonte de verdade (o `config.h` já é a verdade operacional), com qualquer menção de pino em prosa tratada como referência, nunca cópia. A auditoria imediata: sente com o esquemático KiCad aberto e confira define por define; qualquer divergência entre esquemático, define e comentário é um bug a resolver (e nem sempre o define é o errado: às vezes o firmware compensa erro de layout; decida caso a caso com o hardware que voa como árbitro). Estabeleça o costume: mudou pino, muda em um lugar, e a tabela gera-se de lá.

**Decidir antes de codar:**
- Quem é o árbitro final: esquemático ou código? (O hardware que voa decide; tudo converge para ele.)
- Como garantir que novos comentários/READMEs não reintroduzam cópias de pinos? (conecta com o item 20/P2-6)
- Existe alguma placa antiga voando com o pino do comentário? Como saber?

**Pronto quando:** auditoria completa define por define contra o esquemático registrada no repositório, e nenhuma menção de pino em prosa sem apontar para a fonte única.

**Para estudar:**
- O esquemático do próprio projeto (KiCad): não existe atalho para esta auditoria.

### 23. P1-9: [Revisão 2026-11-19] `attach()` tratado como bool, mas devolve o número do canal (0 é sucesso)

**O problema.** `if (!ParachuteServo.attach(SERVO_PIN))` (`parachute_module.cpp:46`) testa o retorno como booleano, mas `ESP32Servo::attach()` devolve o número do canal PWM, e o primeiro canal LEDC é 0, um caso de sucesso. Dependendo da versão da biblioteca e do caminho de alocação (MCPWM no ESP32-S3 devolve >= 100, LEDC devolve 0-15), um servo perfeitamente sadio alocado no canal 0 faria `setupServo()` retornar `false`, o init virar FATAL e o boot terminar em safe-hold de um sistema sadio, que, como no item 1 (P0-1), significa foguete sem paraquedas.

**Evidência:** `parachute_module.cpp:44-48` (`!attach()`); na fonte da ESP32Servo, `attach()` termina com `return pwm.getChannel()`, e `ESP32PWM::getChannel()` devolve `pwmChannel`, cujo primeiro valor alocável no caminho LEDC é 0. O comentário local ("attach() returns true on success") documenta um contrato que a biblioteca não tem. O banco funciona hoje porque o caminho MCPWM do ESP32-S3 devolve >= 100: por coincidência de alocação, não por contrato.

**A solução.** Checagem por contrato, não por coincidência: chamar `attach()` e verificar `ParachuteServo.attached()` (o estado real do pino). Três linhas de mudança + um log de boot imprimindo o canal alocado. E o hábito que fica: para todo retorno de biblioteca cuja semântica não é bool, ler o cabeçalho/fonte em vez de assumir.

**Decidir antes de codar:**
- Confirmar em banco, com o build atual, qual canal é alocado em boot limpo vs. boot com retries: o resultado muda?
- Um attach genuinamente falho (canal indisponível) hoje leva a safe-hold silencioso: qual deveria ser o comportamento (o foguete voa sem servo? o alarme é distinguível do safe-hold de sensores)?

**Pronto quando:** no banco, o boot imprime o canal alocado e `setupServo` não reporta falso com hardware sadio; e uma revisão rápida não encontra mais nenhum `!obj.method()` cujo retorno não seja bool.

**Para estudar:**
- ESP32Servo (`madhephaestus/ESP32Servo`): `Servo::attach()` e `ESP32PWM::getChannel()`/`allocatenext()` no fonte da biblioteca.

---

## P2: Processo de engenharia

### 15. P2-1: Testar o C++ real no host, não uma reimplementação em Python

**O problema.** A FSM é validada por um modelo paralelo em Python que já derrapou da realidade (a documentação cita 50 Hz num lugar, 5 Hz no outro, e os comentários citam janelas em ciclos "@50Hz" que não correspondem ao config atual) e mascarou um bug real (item 21/P0-7: o validador arma o latch na ordem certa, o C++ não, e o teste passou com o firmware morto). O que é testado não é o que voa.

**A solução.** O princípio: testar o artefato, não uma imitação dele. O mesmo código-fonte pode ser compilado para o host no ambiente native do PlatformIO, com as dependências de Arduino (`millis`, `Serial`, `delay`, NVS, I2C) substituídas por stubs controláveis e um clock fake determinístico. O ganho transformador: replayar um CSV de voo real contra a FSM de produção, clock a clock, e injetar falhas que são impossíveis ou caras em placa: barômetro congelado, NaN, saturação de IMU, reboot logo após cada caminho de deploy (o item 4/P0-4 exige esse teste), perda de GPS. Fault injection em host é o superpoder do teste embarcado. Framework leve (Unity é o clássico) e começar pequeno: a FSM primeiro, por ser o maior risco (58 arestas no grafo, betweenness 0,390).

**Decidir antes de codar:**
- Quais dependências de Arduino a FSM/sensores tocam? Qual a superfície mínima de stubs? (liste antes de escrever o primeiro)
- Como representar o tempo no teste: um clock fake que avança por comando permite determinismo total; como desenhar?
- O replay do CSV real vira regressão permanente no CI (item 16)? Qual critério de passe (mesmos eventos, mesma ordem, tolerância de timing)?

**Pronto quando:** `pio test -e native` roda a FSM de produção + replay do CSV + suíte de fault injection, sem hardware, em segundos, e verde.

**Para estudar:**
- PlatformIO Unit Testing (ambiente native + on-host): https://docs.platformio.org/en/latest/advanced/unit_testing/
- Unity: https://github.com/ThrowTheSwitch/Unity
- "hardware abstraction layer embedded unit testing stubs": desacoplar tempo/periféricos para testar lógica.

### 16. P2-2: Build manual na IDE para build reproduzível com análise estática em CI

**O problema.** O firmware que voa é compilado à mão, na IDE, na máquina de quem compilou: nenhuma garantia de que o próximo build é o mesmo, nenhum portão automático de qualidade entre commit e firmware de voo.

**A solução.** Migrar para PlatformIO (mesma toolchain Arduino-ESP32, porém scriptável e versionável: o `platformio.ini` descreve o build no repositório) e montar CI a cada push: compilação com warnings promovidos a erro (entenda as flags `-Wall -Wextra -Werror` e por que um warning ignorado vira bug de produção; meça o baseline antes de ativar `-Werror`, porque um salto de 0 para 300 warnings exige limpeza em etapas), análise estática com `cppcheck` e `clang-tidy` (cada um pega classes diferentes de problemas), formatação automática com `clang-format` (discuta um estilo com o time e feche o assunto: o valor está na consistência, não na escolha perfeita), e o job de testes nativos do item 15. Referência de rigor: as regras do *Power of Ten* calibram o que um firmware crítico merece exigir de si mesmo. Objetivo final: merge com CI verde é um firmware que qualquer um no time reproduz bit a bit com um comando.

**Decidir antes de codar:**
- GitHub Actions ou outro CI: o que o time já usa/conhece? Qual o menor pipeline que já entrega valor (só build + test primeiro)?
- Quantos warnings o codebase atual tem com `-Wall -Wextra`? (rode antes de decidir)
- O diff gigante do clang-format: como dividir sem destruir o `git blame`/histórico de uma vez? (commits separados por área)

**Pronto quando:** um commit que quebra warning, teste ou análise estática é rejeitado automaticamente; e qualquer membro do time gera o mesmo binário com um comando, sem IDE.

**Para estudar:**
- PlatformIO (CI + environments): https://docs.platformio.org/en/latest/
- clang-tidy: https://clang.llvm.org/extra/clang-tidy/ · cppcheck: https://cppcheck.sourceforge.io/
- Holzmann, *Power of Ten* (o padrão de rigor que o CI vai exigir): https://spinroot.com/gerard/pdf/P10.pdf

### 17. P2-3: Tempos em "ciclos a X Hz" no config: mudar a frequência recalibra o sistema em silêncio

**O problema.** O `config.h` expressa janelas críticas de tempo como contagens de ciclos comentadas com a frequência atual ("5 ciclos @ 50 Hz", "13 ciclos // 2.6s @5Hz"). Se alguém mudar a frequência do loop (o que o item 2/P0-2 propõe), todos os tempos de segurança do sistema mudam juntos e sem aviso.

**A solução.** Regra de ouro: tempo é tempo, e se expressa em milissegundos; ciclos são detalhe de implementação derivado. Inverta a dependência: os defines declaram a intenção em ms (cada janela com um comentário explicando o porquê do número, que é o que se perde primeiro), e a contagem de ciclos é calculada a partir do período. A ferramenta que torna isso seguro é `static_assert` + `constexpr`: verificação em tempo de compilação de que janela pretendida e ciclos derivados correspondem; se alguém mudar a frequência sem revisar as janelas, o build quebra com mensagem legível em vez de recalibrar a segurança em silêncio.

**Decidir antes de codar:**
- Liste todas as constantes em ciclos no config: qual a janela em ms pretendida de cada uma? (às vezes o comentário diz; quando não diz, é aí que se descobre surpresa)
- Ciclos não-inteiros: arredondar para cima ou para baixo, e o que isso significa em segurança? (para janelas de confirmação, a resposta é sempre o lado seguro)
- Como documentar o porquê de cada janela: 600 ms de confirmação de paraquedas vem de quê (análise, simulação, chute documentado)?

**Pronto quando:** mudar a frequência do loop de propósito num commit de teste quebra o build com `static_assert` explicativo, e nenhum comentário "ciclos @ XHz" resta no código.

**Para estudar:**
- `static_assert` C++11 e "compile time configuration checking embedded"
- O histórico do próprio `config.h` (git blame nos comentários): entender de onde veio cada número.

### 18. P2-4: Alocação dinâmica e `String` no caminho crítico

**O problema.** O caminho de execução do voo usa `new` sem verificação (em arduino-esp32, `new` sem memória devolve NULL, que é dereferenciado em seguida) e `String`, que aloca por baixo, em pontos críticos. Falha de alocação e fragmentação de heap em plena descida são modos de falha que código de segurança não pode ter.

**A solução.** A regra (literalmente uma das dez do *Power of Ten* da NASA): em código crítico, nenhuma alocação dinâmica depois da inicialização. Duas razões: (1) alocação pode falhar, e falha descoberta em plena descida é o pior momento; (2) alocação/desalocação repetida fragmenta o heap, e o sistema pode ter memória total sobrando e ainda falhar uma alocação específica. O plano: sensores e FSM como instâncias estáticas (número fixo e conhecido no boot: nada justifica heap), buffers de texto de tamanho fixo com a família `snprintf` no lugar de `String` (mensure o tamanho máximo real antes de fixar), e heap permitido só na inicialização, onde falha é detectável e safe-hold é barato. Migrar por partes, começando pelo caminho de voo (telemetria, FSM, logger), não pelo setup.

**Decidir antes de codar:**
- Onde exatamente `String` aparece no caminho de voo, e qual o tamanho máximo real de cada string formatada? (mensurar antes de fixar buffers)
- Como verificar zero alocação pós-boot: existe hook/métrica no ESP-IDF? (conecta com o pacote de saúde do item 10)
- Orçamento para mensagens de log de emergência com buffers fixos reduzidos?

**Pronto quando:** instrumentação de alocação mostra zero malloc/new após a inicialização em voo simulado; e grep por `String` no caminho de voo não retorna nada.

**Para estudar:**
- *Power of Ten*, regra 3 e sua justificativa: https://spinroot.com/gerard/pdf/P10.pdf
- "heap fragmentation embedded systems" e "snprintf vs sprintf safety C"

### 19. P2-5: Com quais parâmetros *esse* voo voou?

**O problema.** Dois CSVs de dois voos diferentes são incomparáveis: não há registro dos thresholds efetivos, do firmware exato (git hash) nem do modo operacional com que cada um voou.

**A solução.** O princípio do black box de aviação: o registro do voo carrega sua própria identidade. Duas peças: (1) todo artifact de dados de um voo (cabeçalho do CSV, pacote de saúde do item 10) começa com um bloco de metadados: thresholds efetivos lidos dos valores em uso no momento (pós-calibração, pós-override, não dos defaults do código, que podem divergir), hash do commit do build, e modo degradado vigente; (2) o hash do build capturado no build (script gerando `version.h`, o padrão de flags de pré-processador em tempo de compilação). O efeito prático: quando um voo se comportar estranho, a primeira pergunta ("com que parâmetros e com que build isso voou?") tem resposta no primeiro arquivo que você abre, sem arqueologia de git.

**Decidir antes de codar:**
- O cabeçalho de metadados quebra parsers existentes (scripts de análise, ground station)? Como versionar o formato do cabeçalho?
- Listar todos os thresholds, ou só os de decisão de voo? (equilibre completude vs. tamanho: o CSV roda no rádio também)
- Como garantir que os valores reportados são os efetivos (pós-calibração, pós-override) e não os defaults?

**Pronto quando:** abrindo o CSV de um voo, a primeira seção responde build (hash), thresholds em uso e modo de voo, sem consultar o repositório.

**Para estudar:**
- "embed git hash compile time C++": o padrão `build_id`/`version.h` gerado por script de build.
- "flight data recorder metadata header": precedentes de formato.

### 20. P2-6: Documentação viva: matar os drifts e impedir que voltem

**O problema.** Código e prosa já divergem em vários pontos (50 Hz vs 5 Hz, "Every 20ms", autor "#100" vs "#11", pinos do item 14), e cada drift corrigido à mão volta, porque nada estrutural impede o código e a prosa de divergirem de novo.

**A solução.** Duas camadas. A imediata: a caça aos drifts conhecidos (todos mapeados no diagnóstico: frequências, períodos, autor, pinos), corrigidos na fonte, com uma passada de "o que mais este arquivo afirma sobre o sistema que posso verificar contra o código agora?". A estrutural: prosa não é lugar de dado. Qualquer número que já existe no código (frequência, período, threshold, pino) e é repetido em comentário/README vai deriva; a saída é gerar a documentação a partir da fonte (a tabela de thresholds, por exemplo, pode ser gerada do `config.h` por script, com o CI do item 16 falhando se a doc gerada estiver desatualizada) ou referenciar o define sem repetir o valor. Reflita sobre o hábito do time: onde os números "convenientemente repetidos" entram em review, e como o checklist os pega.

**Decidir antes de codar:**
- Quem é a fonte de verdade para cada categoria (pinos, tempos, frequências, autor, hardware)? Onde essa verdade mora?
- Quais docs valem geração por script (tabela de thresholds, mapa de pinos), e quais ficam prosa pura?
- Como tratar o `@author`: congelado no tempo, ou refletindo a manutenção real?

**Pronto quando:** uma auditoria manual encontra zero divergências código/docs nos pontos conhecidos; e o CI (item 16) falha se alguém editar o código sem regenerar a doc gerada.

**Para estudar:**
- A lista de drifts no diagnóstico (`IMPROVEMENTS.md`, seção P2-6) é o checklist inicial.
- "docs as code single source of truth": gerar docs a partir da fonte.

---

## Roadmap de entrega por etapas (do diagnóstico original)

| Etapa | Itens (números deste plano) | Como saber que a etapa fechou |
|---|---|---|
| 1: Riscos imediatos | 4 (P0-4), 21 (P0-7), 23 (P1-9), 5 (P0-5), 6 (P0-6) | Reboot após ejeção mantém o servo aberto; contingência baro-stale demonstrada viva em teste; boot nunca falha esporadicamente no `attach()`; zero pacotes intercalados; tempo no ar medido abaixo de 50% do canal |
| 2: Redundância de sensor | 1 (P0-1), 3 (P0-3), 8 (P1-2) | Modos degradados ejetam em simulação e drop test; teste do servo no pad |
| 3: Estimação e apogeu | 2 (P0-2), 17 (P2-3), 11 (P1-5) | Zero falsos apogeus no replay + RocketPy; Vz filtrado com ganhos documentados |
| 4: Observabilidade | 7 (P1-1), 9 (P1-3), 10 (P1-4), 13 (P1-7), 22 (P0-8), 12 (P1-6) | Pacote de saúde no ground; log de alta taxa na PSRAM sem sobrescrever o voo anterior; sem panic de flash; arm por rádio em dois passos |
| 5: Processo | 15 (P2-1), 16 (P2-2), 18 (P2-4), 19 (P2-5), 20 (P2-6), 14 (P1-8) | CI verde com testes nativos do C++ real e análise estática; docs sem drift; pinos auditados contra o esquemático |

## Revalidação antes de voar (checklist do diagnóstico)

1. Replay do CSV de voo real no C++ de produção, incluindo fault injection (item 15).
2. Teste de bancada de ejeção com medição de bateria e continuidade (item 3).
3. Reboot forçado em cada estado da FSM e após cada origem de deploy (item 4).
4. Teste de enlace: tempo no ar medido, taxa recebida e RSSI no ground station (item 6).
5. Drop test com e sem barômetro/IMU operantes (item 1).
6. Barômetro congelado de propósito em plena subida (bancada/simulação): a contingência baro-stale ejeta (item 21).
7. Reboot no meio do voo: o log do trecho anterior não é sobrescrito (item 22).

---

*Este plano descreve problemas, ideias e direções de estudo, deliberadamente sem código ou soluções prontas. A implementação de cada item é o trabalho de engenharia que ele prepara.*
