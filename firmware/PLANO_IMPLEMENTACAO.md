# Plano de Implementação — Melhorias do Firmware v2.0

**Data:** 2026-11-05
**Base:** `IMPROVEMENTS.md` (diagnóstico de 2026-11-04, 17 itens)
**Como ler este plano:** cada item abaixo descreve *o problema*, *a ideia da solução* e *as perguntas que você precisa responder antes de escrever código*. Este arquivo não entrega implementação — a resposta de "como fazer" é sua. Onde existe material de estudo confiável, ele está listado no final de cada tarefa.

**Ordem de execução:** os itens estão listados por prioridade (P0 = risco de voo → P2 = processo). Se preferir seguir o roadmap de entrega por etapas sugerido no diagnóstico original, ele está resumido no final deste arquivo.

---

## P0 — Segurança de voo

### 1. P0-1 — Quando a IMU falha no boot, o foguete fica sem paraquedas

**O problema, em uma frase:** hoje existe uma cadeia de dependências em que a falha de um único sensor no boot derruba *todas* as formas de ejetar o paraquedas — o sistema entra em *safe-hold* silencioso, buzina para sempre e nunca mais decola, mas o ponto é que uma falha **parcial** virou falha **total**.

**A ideia:** fazer o firmware enxergar graus de saúde, em vez de "tudo ou nada". Pense em modos de operação explícitos: nominal, só barômetro, só IMU, e realmente nenhum sensor (aí sim, safe-hold). Cada modo degradado precisa conseguir cumprir a missão mínima — **ejetar** — com as informações que ainda tem. Um apogeu dá para detectar só com altitude; um free-fall dá para detectar só com aceleração. Nenhum dos dois precisa dos dois sensores ao mesmo tempo.

**Perguntas para responder antes de codar:**
- Quem decide o modo? A decisão é tomada uma vez no boot, ou o sistema pode degradar (e se recuperar?) em pleno voo? O que acontece se o sensor falha *depois* do liftoff?
- No modo só-barômetro, qual combinação de evidências você aceita como "apogeu"? E no modo só-IMU, como você distingue burnout de apogeu sem altitude?
- O safe-hold atual desliga até o rádio. Vale a pena manter telemetria e buzzer vivos em todos os modos — por quê? O que a equipe em terra precisa ouvir/ver para saber em que modo o foguete está?
- Como o modo degradado chega até o time em terra (padrão de buzzer? campo no pacote de telemetria? linha no CSV?)

**Como saber que terminou:** desligue (fisicamente ou por software) a IMU no boot e o foguete ainda detecta apogeu e comanda deploy por barômetro. Repita o teste ao contrário. O modo vigente é visível em terra sem abrir o foguete.

**Onde pesquisar:**
- *The Power of Ten — Rules for Developing Safety-Critical Code* (G. J. Holzmann, NASA/JPL) — em especial a filosofia de detectar e responder a falhas em vez de propagá-las silenciosamente: https://spinroot.com/gerard/pdf/P10.pdf
- O conceito de *fail-operational* vs *fail-safe* em sistemas de ejeção — pesquise "fail operational vs fail safe avionics recovery system".

---

### 2. P0-2 — A velocidade vertical é uma derivada crua e o apogeu é decidido por uma amostra só

**O problema, em uma frase:** a velocidade usada para decidir o momento mais crítico do voo é calculada como diferença entre duas leituras barométricas ruidosas, e a decisão de apogeu dispara quando esse número ruidoso cruza um limiar **uma única vez** — a margem de segurança atual vem de sorte estatística, não de estimação.

**A ideia:** separar duas frentes que hoje estão misturadas. Primeiro, *estimação*: em vez de aceitar a leitura crua do barômetro como verdade, mantenha um modelo interno de onde o foguete está e para onde vai — algo que combine a predição física ("se estou subindo a X m/s, em 200 ms devo estar X·0,2 m mais alto") com a correção pela medição real. Existem duas famílias clássicas para isso, com custo computacional trivial para um ESP32: filtros do tipo α-β (g-h) e o Kalman 1-D. Entenda as duas, escolha uma e saiba justificar a escolha. Segundo, *decisão*: o apogeu deve ser declarado por consenso — várias evidências concordando ao longo de um intervalo (sinal de velocidade, altitude abaixo do máximo, tempo mínimo decorrido), e não um cruzamento de limiar instantâneo.

Há também um detalhe de desperdício: o sensor de pressão entrega leituras a uma taxa bem maior do que o loop de controle consome. Pense no que isso significa e se vale a pena alinhar as duas frequências — e o que precisa ser re-verificado se você mudar a frequência do loop (spoiler: todos os tempos expressos "em ciclos" do `config.h` — ver tarefa 17/P2-3).

**Perguntas para responder antes de codar:**
- Qual o ruído real do seu barômetro *na sua placa, no seu ambiente*? Como medir isso no banco e como esse número vira ganho de filtro?
- Se você incluir a aceleração da IMU na predição, o bias documentado de repouso vira um problema — como lidar com isso? (a tarefa 11/P1-2 trata da calibração)
- Como você valida que o estimador está *melhor* que a derivada crua, antes de confiar a vida do paraquedas a ele?
- O que é "consenso" em números? Quantas amostras, em qual janela, com quais condições mínimas — e como escolher esses parâmetros sem chutá-los?

**Como saber que terminou:** o replay de um voo real (ou de um voo simulado com ruído realista) passa a declarar apogeu no momento fisicamente correto, de forma repetível, mesmo quando você injeta ruído. A taxa de falso apogeu medida antes/depois é a sua métrica de sucesso.

**Onde pesquisar:**
- Filtro α-β (g-h) — https://en.wikipedia.org/wiki/Alpha_beta_filter
- Kalman 1-D explicado visualmente — https://bzarg.com/p/how-a-kalman-filter-works-in-pictures/
- RocketPy (simulação de trajetória para gerar dados de validação com ruído realista) — https://docs.rocketpy.org/
- Datasheet do BMP585 — ruído de pressão, ODR e o filtro IIR interno (entender isso muda o desenho do estimador): https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp585-ds003.pdf

---

### 3. P0-3 — "Comandei a ejeção" não é a mesma coisa que "o paraquedas abriu"

**O problema, em uma frase:** hoje a telemetria reporta a *decisão da FSM* como se fosse a *atuação física* — se o servo estiver desconectado, sem energia ou travado, o ground station vai mostrar `chute=YES` mesmo com o foguete em queda livre; e todos os caminhos de ejeção do firmware convergem para o **mesmo pino PWM**, ou seja, um único ponto de falha elétrica.

**A ideia:** tratar a ejeção como um sistema com realimentação, não como um comando de mão única. Conceitualmente, existem três momentos distintos que hoje estão fundidos em uma flag só: *decidi ejetar*, *apliquei o sinal*, e (idealmente) *confirmei que atuou*. A linguagem do seu contrato de dados (o `SensorData`) precisa refletir essa separação para que a equipe em terra saiba em qual das três fases as coisas pararam. Paralelamente, pense em redundância: se todos os caminhos lógicos convergem no mesmo fio, nenhuma redundância lógica sobrevive a um mau contato. Um segundo canal independente — outro GPIO, outro driver, comandado pelo caminho de segurança — muda a natureza da falha de "single point of failure" para "falha silenciosa detectável". Por fim, estabeleça o hábito de *built-in test* no pad: uma verificação curta, antes do arm, que exercite o mecanismo (sem ejetar!) e reporte o resultado de forma audível.

Não ignore a elétrica: servo e MCU na mesma alimentação significa que o pico de corrente da ejeção acontece exatamente quando um brownout seria fatal. Isso é um item de bancada: medir, não assumir.

**Perguntas para responder antes de codar:**
- Quais das três fases (commanded/actuated/confirmed) seu hardware atual consegue distinguir de verdade? O que seria necessário para ter a terceira (continuidade? posição? corrente?) e vale o custo?
- Como desenhar um BIT de servo que dê confiança sem risco de ejeção acidental no pad? Quem autoriza o BIT e como ele é reportado?
- O segundo canal precisa de driver diferente, alimentação diferente, ou ambos? O que o backstop comanda vs. o que a FSM comanda?
- Dual deployment (drogue + main) é roadmap, não agora — mas as escolhas de hoje fecham ou abrem essa porta? O que você faria diferente sabendo disso?

**Como saber que terminou:** com o servo desconectado de propósito, o ground station mostra claramente que a ejeção foi comandada mas não confirmada. O BIT roda no pad, reporta por som, e a equipe confia nele.

**Onde pesquisar:**
- Códigos de segurança e práticas de recuperação em foguetemodelismo — NAR: https://www.nar.org/ · Tripoli: https://tripoli.org/
- *Power of Ten* (Holzmann) — verificação de pós-condição em código crítico: https://spinroot.com/gerard/pdf/P10.pdf

---

### 4. P0-4 — Bug real: ejeção por backstop não é persistida, e um reboot **fecha o paraquedas em pleno voo**

**O problema, em uma frase:** dos três caminhos que podem ejetar, só um grava o estado em memória não-volátil — se o ESP32 reiniciar (watchdog, panic, brownout) logo após uma ejeção pelos outros dois caminhos, o firmware acorda sem saber que ejetou e **move o servo para a posição fechada durante a descida**, rearmando inclusive a lógica de deploy.

**A ideia:** concentrar a ejeção em um único ponto de entrada. Hoje três trechos de código distintos fazem "a mesma coisa" de formas ligeiramente diferentes — e é exatamente a diferença entre elas que esconde o bug. Um serviço único de deploy, chamado por todos os três caminhos, garante que a sequência crítica seja *sempre a mesma*: primeiro garantir que o estado sobreviva a um reboot (persistir), *depois* acionar o hardware, e registrar por quem e por quê. A ordem importa: se o reboot acontecer entre um passo e outro, qual estado você prefere encontrar?

A segunda frente é a *integridade* da persistência. O mecanismo atual de snapshot não se protege contra uma escrita interrompida no meio (energia caiu, reset aconteceu durante a gravação). Pesquise as técnicas clássicas para isso — verificação de integridade do bloco gravado e gravação alternada em slots com contador — e decida como aplicá-las ao seu formato de snapshot. Aproveite para pensar no que *mais* merece sobreviver a um reboot além da flag de deploy, para fins de auditoria pós-voo.

**Perguntas para responder antes de codar:**
- Qual a ordem correta das operações no serviço único, e por quê? O que acontece em cada janela de tempo entre passos se o reset cair ali?
- Como testar de forma *determinística* um reboot no meio da gravação? (fault injection — a tarefa 15/P2-1 cria a infraestrutura)
- O formato novo do snapshot precisa ser lido por firmware antigo (ou vice-versa)? Como versionar isso?
- A NVS do ESP-IDF tem comportamentos específicos sobre atomicidade e *wear leveling* — o que ela já garante por você e o que ainda é sua responsabilidade?

**Como saber que terminou:** para cada um dos três caminhos de deploy, force um reboot imediatamente após a ejeção (em bancada, de propósito) e verifique: o servo permanece aberto, a lógica não rearma, e o motivo da ejeção sobrevive para leitura pós-voo.

**Onde pesquisar:**
- ESP-IDF NVS (escrita atômica, wear leveling, limitações): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/storage/nvs_flash.html
- ESP-IDF Task Watchdog (a origem provável desses reboots): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html

---

### 5. P0-5 — Duas tasks disputam o mesmo rádio LoRa (e o mesmo barramento SPI do SD) sem nenhuma sincronização

**O problema, em uma frase:** telemetria e logger chamam a rotina de transmissão LoRa de tasks com prioridades diferentes, sem proteção nenhuma, sobre um barramento SPI compartilhado com o cartão SD — o resultado possível é uma preempção *no meio de um pacote*, intercalando dois payloads em um único frame e corrompendo justamente as mensagens de erro, que são as mais importantes.

**A ideia:** existe uma solução rápida e uma solução certa, e vale entender as duas. A rápida: sempre que um recurso é compartilhado entre tasks, o acesso precisa ser mediado — pesquise como o FreeRTOS trata exclusão mútua e quais os problemas clássicos (inversão de prioridade!) que um mutex mal configurado introduz. A certa: repense a posse do recurso. Um padrão poderoso em RTOS é o de *dono único*: em vez de várias tasks disputando o rádio, apenas uma task fala com ele, e as demais entregam mensagens para ela via fila. Isso elimina a contenção por construção em vez de administrá-la — e o GPS do seu próprio firmware já funciona assim, então o padrão já existe no projeto; falta estendê-lo. O rádio e o SD moram no mesmo barramento, então pense no escopo da proteção: task? barramento? ambos?

Bônus de arquitetura: o modo de transmissão bloqueante do rádio mantém a task presa pelo tempo inteiro do pacote no ar. Existe um modo assíncrono com interrupção de conclusão — investigue como funciona e o que mudaria no desenho das tasks.

**Perguntas para responder antes de codar:**
- Qual o escopo do mutex — só o rádio, ou o barramento SPI inteiro (incluindo SD)? O que cada escolha custa em tempo de espera máxima?
- Se migrar para task de rádio com fila: a fila aceita mensagens de telemetria *e* de erro? Quem tem prioridade quando a fila enche? O que é descartado primeiro e por quê?
- Como provar que o problema existia (e que foi resolvido)? Que evidência um pacote intercalado deixaria — e como capturá-la?

**Como saber que terminou:** uma sessão de teste que antes produziria pacotes corrompidos/intercalados passa a produzir zero, com as mensagens de erro do logger chegando inteiras ao ground station mesmo durante rajadas de telemetria.

**Onde pesquisar:**
- *Mastering the FreeRTOS Real Time Kernel* — capítulos sobre mutexes, exclusão mútua, inversão de prioridade e filas: https://www.freertos.org/Documentation/RTOS_book.html

---

### 6. P0-6 — A telemetria a 5 Hz fisicamente não cabe no enlace LoRa configurado

**O problema, em uma frase:** pela fórmula de *time-on-air* da modulação LoRa, o frame CSV atual (140–170 B) leva mais tempo no ar do que o intervalo entre frames a 5 Hz — o canal está acima de 100% de ocupação, a task de telemetria estoura o período em todos os ciclos, e o mecanismo de "descartar e ficar com a amostra mais nova" mascara o problema jogando dados fora.

**A ideia:** primeiro, internalize o orçamento: um enlace LoRa não é um canal onde você "manda bytes", é um canal onde você *ocupa tempo de rádio*, e esse tempo depende do tamanho do payload e dos parâmetros de modulação (SF, BW, CR). Faça as contas para a sua configuração — a nota de aplicação da Semtech abaixo tem a fórmula exata e é o material canônico do assunto. Depois, redesenhe o tráfego pensando em *dois públicos*: o ground station em tempo real precisa de poucos bytes, muitas vezes por segundo (posição, estado, flags — cabe em poucas dezenas de bytes se você pensar em formato binário compacto com verificação), enquanto a análise pós-voo precisa do frame completo — que pode ir mais devagar, ou nem ir por rádio (ver tarefa 12/P1-3 para a alternativa de log local). Essa divisão resolve o orçamento sem "reduzir a telemetria" genericamente.

Antes de tocar na taxa ou na potência, verifique o quadro regulatório: 915 MHz no Brasil tem regras de potência e *duty cycle* — aumentar potência para caber mais dados pode ser simplesmente ilegal.

**Perguntas para responder antes de codar:**
- Quais campos o ground station *realmente* usa em tempo real durante um voo? O que é essencial a 5 Hz e o que pode ir a 0,5–1 Hz — ou só depois, no CSV?
- Como medir o ToA real (não o teórico) em runtime e publicar essa medida na telemetria de saúde, para que o enlace seja auditável em campo?
- Se o frame binário curto for o caminho: como versionar o formato para que receptor e transmissor não desincronizem entre builds?
- Quais os limites da ANATEL para a sua configuração — e quem na equipe é responsável por essa verificação?

**Como saber que terminou:** com o ToA medido em runtime, a ocupação do canal fica confortavelmente abaixo do limite (a meta sugerida no diagnóstico é < 50%), sem perda de amostras pela fila de telemetria durante um voo inteiro simulado.

**Onde pesquisar:**
- Semtech AN1200.22, *LoRa Modulation Basics* — a fórmula de ToA e a relação SF/BW/CR: https://www.semtech.com/uploads/documents/an1200.22.pdf
- ANATEL (regulação de RF no Brasil): https://www.gov.br/anatel/pt-br/regulado/certificacao-de-produtos

---

## P1 — Robustez e observabilidade

### 7. P1-1 — O watchdog supervisiona uma task só, e o safe-hold não supervisiona nada

**O problema, em uma frase:** apenas a task de controle de voo está registrada no watchdog — telemetria e logger podem travar silenciosamente sem reset, e o laço infinito do safe-hold não alimenta nem registra nada, então um safe-hold em campo é uma caixa preta que buzina.

**A ideia:** o watchdog de task do ESP-IDF só supervisiona quem se registra nele — pense na cobertura como uma decisão de arquitetura: *quais* tasks devem ser supervisionadas, e o que o sistema deve fazer quando cada uma delas deixa de responder. Nem todo travamento merece o mesmo tratamento: derrubar e reiniciar tudo? Registrar e continuar? Reflita também sobre o momento *entre* o travamento e o reset: existe a oportunidade de salvar um último suspiro de diagnóstico (o snapshot, o estado, o motivo) antes da reinicialização — pesquise como funciona o fluxo de panic handler e se há hook para isso. Por fim, o histórico: um contador de reboots com a causa de cada um, exportado na telemetria, transforma "o foguete reiniciou em voo, não sabemos por quê" em "reiniciou por TWDT na task X no segundo 43".

**Perguntas para responder antes de codar:**
- Todas as três tasks no TWDT, ou existem reasons para deixar alguma de fora? O que muda na prática se a task de logger travar — o voo continua seguro?
- O que preservar no momento do panic, e para onde (a NVS tem a mesma contenção do item 4/P0-4)?
- Como distinguir reboot "programado/aceitável" de reboot "anômalo" no contador?
- O safe-hold deveria alimentar o watchdog? Argumente os dois lados antes de decidir.

**Como saber que terminou:** um travamento injetado em qualquer task supervisionada produz um reset *com causa registrada*, e o registro chega ao ground station no pacote de saúde (tarefa 10/P1-4) — nenhuma caixa-preta.

**Onde pesquisar:**
- ESP-IDF Task Watchdog: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/wdts.html
- FreeRTOS Kernel Book (seção de notificações/timers para o padrão do "último suspiro"): https://www.freertos.org/Documentation/RTOS_book.html

---

### 8. P1-2 — A IMU opera em range implícito, sem calibração, e satura sem avisar

**O problema, em uma frase:** o sensor de aceleração nunca recebe configuração explícita de range (depende do default da biblioteca), a validação atual **descarta** amostras fora do limite em vez de sinalizar saturação — perdendo justamente os dados do momento mais intenso do voo — e existe um bias de repouso documentado de +2,81 m/s² que ninguém compensa.

**A ideia:** três frentes independentes. *Configuração*: torne o range uma decisão explícita e documentada — pesquise no datasheet as opções e o trade-off entre resolução e limite (a queima do seu motor cabe em qual?). *Calibração*: o bias de repouso é mensurável — um procedimento de coleta de N amostras no pad, antes do arm, pode caracterizar o offset e guardá-lo de forma persistente; pense em quando isso roda, quem autoriza, e como o valor sobrevive a um reboot. *Sinalização*: o conceito importante aqui é *flag de saturação* vs *descarte de amostra* — uma amostra saturada não é lixo, é informação ("estou acima do range"); a diferença entre congelar a última leitura válida e marcar `imu_saturated` muda completamente o que a FSM pode inferir durante a queima. E um registro de humor: se a queima superar o range máximo do sensor, existe uma família de acelerômetros de alto-g feita exatamente para isso — avalie se o seu motor pede isso, antes de "resolver" com software.

**Perguntas para responder antes de codar:**
- Qual o pico de aceleração do seu motor (curva do fabricante ou simulação no RocketPy) vs. os ranges disponíveis no LSM6DS3?
- Onde o offset calibrado vive (NVS — mesma infraestrutura do item 4/P0-4)? O que fazer se a calibração não rodou — voar com bias conhecido ou recusar arm?
- A saturação durante a queima deve afetar a FSM *na hora*, ou só ser marcada para análise pós-voo? Quem consome a flag?

**Como saber que terminou:** no banco, com o range configurado e calibração rodada, o repouso lê ~9,8 m/s² no eixo Z (dentro da incerteza do sensor), e um movimento brusco acima do range resulta em flag de saturação — não em congelamento silencioso da amostra.

**Onde pesquisar:**
- Datasheet LSM6DS3 (ranges, sensibilidade, registers de configuração): https://www.st.com/resource/en/datasheet/lsm6ds3.pdf
- Acelerômetros de alto-g (pesquise H3LIS331DL e ADXL375) — quando 16 g não é suficiente.

---

### 9. P1-3 — Gravar na flash durante o voo causa panic; a "solução" atual é gravar menos

**O problema, em uma frase:** escrever no cartão SD durante o voo já causou panic por *flash cache off* nos dois núcleos (documentado em comentário no código), e a mitigação adotada foi reduzir a taxa de log — ou seja, abrir mão da resolução dos dados justamente nas fases mais críticas do voo.

**A ideia:** existe uma memória no seu hardware que resolve isso e que está sendo desperdiçada: a placa tem 8 MB de PSRAM, que é RAM externa — grande, e *não* envolve a flash durante o acesso. O padrão de arquitetura é o clássico *buffer-then-flush*: acumular os frames de voo em memória volátil e descarregar tudo quando a fase crítica acabou (pense em *quando* é seguro — o estado `LANDED` é um candidato natural). Reflita também sobre o padrão de acesso ao cartão: o código atual abre, escreve e fecha o arquivo a cada linha — cada `open`/`close` é uma operação custosa e arriscada que pode ser paga uma vez só. E questione o formato: gravar CSV direto obriga formatação em placa a cada linha; gravar um formato binário bruto com verificação e converter para CSV no pós-voo tira custo de CPU do momento crítico — o que é exatamente a filosofia do OpenFlightLogger/flight-computers maduros. Por fim, uma task de storage dedicada com fila isola o cartão (lento, falho) da telemetria (crítica) — e conecta com a discussão de posse de recursos da tarefa 5/P0-5.

**Perguntas para responder antes de codar:**
- Quanto de dados um voo inteiro gera a 25 Hz no seu formato? Cabe em 8 MB? E se o voo durar o dobro do previsto?
- O que acontece com os dados em PSRAM se o foguete cair com bateria (ou a bateria desligar) antes do flush? Que garantia de perda máxima você consegue oferecer — e ela é aceitável?
- Binário + CRC: qual layout, como versionar, e como será o conversor pós-voo (quem mantém, onde roda)?

**Como saber que terminou:** um voo completo (real ou simulado) com log em alta taxa na PSRAM e flush único ao pousar: zero panics por flash, zero linhas perdidas, CSV pós-voo idêntico ao que seria gravado linha a linha.

**Onde pesquisar:**
- ESP-IDF External RAM (PSRAM) — capacidades e limitações: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/external-ram.html
- Pesquise "flight computer black box buffer architecture" — o problema de gravar telemetria sem morrer tentando é antigo na aviação experimental.

---

### 10. P1-4 — A instrumentação de saúde já existe no código — e ninguém lê

**O problema, em uma frase:** as funções de estatísticas das três tasks (ciclos, overruns, drops de fila, tempo máx. de execução) estão implementadas e *zero consumidor* as chama — a saúde do sistema só é visível com cabo USB conectado, o que em um voo significa invisível.

**A ideia:** montar um *pacote de saúde* periódico — um frame de telemetria dedicado, de baixa frequência (o diagnóstico sugeriu 0,2 Hz), que agrega tudo o que já existe espalhado: as estatísticas das tasks, o *stack high-water mark* de cada uma (pesquise o que essa métrica significa — ela diz o quanto a pilha de cada task esteve de atingir o fundo, e é a forma padrão de detectar estouro de stack *antes* que ele aconteça), o menor heap já visto desde o boot, a causa do último reset (conecta com a tarefa 7/P1-1), o modo degradado (tarefa 1/P0-1) e o *git hash* do build (conecta com a tarefa 19/P2-5 — sem ele, você não sabe *que firmware* estava voando). A decisão de design interessante aqui: o pacote de saúde é telemetria como outra qualquer (então disputa o rádio — ver tarefa 6/P0-6) ou tem prioridade garantida quando o enlace está saturado? Pense no que você mais quer receber quando as coisas estão dando errado *por causa* da saúde ruim.

**Perguntas para responder antes de codar:**
- Qual a frequência certa — o suficiente para pegar um problema em voo, sem virar ruído no enlace?
- Quais campos são essenciais no pacote mínimo? (o diagnóstico lista candidatos; corte por valor de diagnóstico real)
- Como apresentar isso no ground station — número cru, ou alertas por limiar (stack < X, heap < Y)?

**Como saber que terminou:** com o foguete na mesa e sem nenhum cabo, você consegue afirmar por rádio: stack mínimo de cada task, menor heap, causa do último reset, hash do build e modo operacional.

**Onde pesquisar:**
- FreeRTOS Kernel Book — `uxTaskGetStackHighWaterMark` e diagnóstico de tasks: https://www.freertos.org/Documentation/RTOS_book.html
- ESP-IDF (heap, reset reasons): https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/mem_alloc.html

---

### 11. P1-5 — O GPS é só telemetria: um "fix válido" nunca expira e uma fonte independente de verdade é desperdiçada

**O problema, em uma frase:** a validação atual do fix (`location.isValid()`) não envelhece com o tempo — um dado capturado há minutos ainda passa por "válido" — e o GPS inteiro não participa de nenhuma decisão de voo, apesar de ser a terceira fonte independente de altitude e velocidade do sistema.

**A ideia:** duas camadas. Primeiro, *frescor*: pesquise na documentação da TinyGPSPlus a diferença entre os campos `isValid`, `isUpdated` e `age` — e defina o que "fix utilizável" significa para você em milissegundos de idade máxima. Isso é pré-requisito para qualquer uso sério do dado. Segundo, *participação*: com um fix fresco, o GPS pode entrar na decisão de voo — mas pelo padrão certo: **voto de confirmação, nunca gatilho único**. Pense na arquitetura de fusão como um comitê: barômetro e estimador (tarefa 2/P0-2) opinam sempre; o GPS opina quando tem fix fresco; a decisão muda de qualidade quando ele concorda ou discorda — mas nunca depende só dele (GPS tem latência, multipath no pad e perda de sinal em rotação: ele é uma testemunha, não um juiz). O bônus de recuperação também importa em foguetemodelismo: última posição conhecida e timestamp absoluto no log valem horas de busca em campo.

**Perguntas para responder antes de codar:**
- Qual idade máxima de fix você aceita como "fresco"? Onde esse número vive (config) e como foi escolhido?
- Como o voto do GPS entra no consenso de apogeu da tarefa 2/P0-2 — concorda/discorda/abstém? O que acontece quando ele discorda do estimador?
- A altitude GPS (geodésica vs. ortométrica) é comparável com a altitude barométrica direto, ou precisa de referência comum? (pesquise "GPS altitude MSL vs ellipsoid" — isso mordendo de verdade)

**Como saber que terminou:** um fix antigo não é mais tratado como válido (teste: desconecte a antena e cronometre); e com fix fresco, o ground station recebe a posição ao longo do voo inteiro e o log carrega timestamp absoluto.

**Onde pesquisar:**
- Documentação da TinyGPSPlus — semântica exata de `isValid`/`isUpdated`/`age` (procure o repositório da biblioteca e sua wiki)
- Pesquise "sensor fusion voting architecture" — o padrão de votador/vote de fontes independentes.

---

### 12. P1-6 — Armar o foguete exige cabo USB: o comando de arm fica onde ninguém alcança

**O problema, em uma frase:** o comando `ARM` só é aceito pela serial USB — que no pad normalmente está desconectada — e não tem confirmação, nem sequência de segurança, nem forma de saber o estado de arm a distância.

**A ideia:** mover o canal de comando para o LoRa (que já existe e já é bidirecional — o receptor do ground station é o mesmo enlace) e, ao fazer isso, tratar *segurança de comando* como requisito de primeira classe. Pesquise o padrão de *two-step arm* usado em sistemas reais: um comando de *intenção* ("quero armar") que recebe eco do estado, seguido de um comando de *confirmação* explícito — o objetivo é que um erro de digitação, um ruído no rádio ou uma mensagem repetida jamais armem o foguete sozinhos. Comandos irmãos fazem parte do desenho: `DISARM` (sim, desarmar a distância também é segurança), `STATUS` (consulta sem efeito colateral, para checagem de pad). E a regra atual de recusa (não armar se `maxAltitude`/paraquedas indicam que já voou) está correta — mantenha-a como invariante do novo canal. Não esqueça a camada humana: arm precisa ser *visível e audível* no pad — quem está a 10 m do foguete precisa saber seu estado sem consultar notebook.

**Perguntas para responder antes de codar:**
- O protocolo precisa de sequência/nonce/ACK? O que impede um replay ou duplicação do pacote de armar?
- Como fica o comando por USB — mantido como fallback de bancada, ou removido por superfície de ataque?
- Quem tem autoridade para armar — qualquer um com o rádio certo, ou existe chaveamento adicional?
- Qual feedback sonoro/visual no pad para cada estado (desarmado, arm-intent, armado, recusa — e *motivo* da recusa)?

**Como saber que terminou:** uma sequência completa de pad sem nenhum cabo: arm por rádio em dois passos com eco audível, `STATUS` respondendo, `DISARM` funcionando, e uma tentativa de arm de um passo sozinho sendo recusada.

**Onde pesquisar:**
- Pesquise "two-step arm interlock remote command safety" e padrões de comando/ack em telemetria de foguetemodelismo (OpenLV/Open FlightLogger e projetos da comunidade Lasciva/OpenRocket discutem isso)
- O seu próprio protocolo LoRa já existe — estude o formato de mensagens atual antes de estendê-lo.

---

### 13. P1-7 — Contadores que estouram no meio de um voo longo

**O problema, em uma frase:** o contador de pacotes do `SensorData` é `uint16_t` — a 5 Hz estoura em ~3,6 h, e se o loop subir para 25 Hz (tarefa 2/P0-2) estoura em ~44 min — e a conversão de tempo de voo usa `millis()`, que wrapping em ~49,7 dias.

**A ideia:** este é o clássico problema de *largura de campo vs. taxa vs. duração máxima de missão*: para cada contador do sistema, faça a conta "bits × frequência = duração até o overflow" e escolha larguras que cubram a missão com folga generosa (voos de campanha de teste podem ser mais longos que o voo nominal). O segundo problema é mais sutil e merece estudo: `millis()` no Arduino/ESP32 é um contador que *volta a zero* — pesquise como o idiomático de comparação de tempo monótono lida com isso (a comparação de diferença com subtração funciona através do wrap; comparação direta de "maior que" não funciona — entenda por quê). Se o contador de pacotes e o de tempo não concordarem depois de um wrap, você vai debugar um fantasma.

**Perguntas para responder antes de codar:**
- Liste *todos* os contadores do sistema e suas larguras — qual o primeiro a estourar em um voo de 10 min? E em um de 2 h (futuro, voos maiores)?
- Onde o tempo de voo é derivado de `millis()` — e a derivação é wrap-safe?
- O `packet_count` novo quebra o formato do CSV/binário de telemetria? Como migrar sem invalidar logs antigos?

**Como saber que terminou:** um teste que simula/fakeia o avançar dos contadores além do overflow mostra tempo de voo e contadores continuando monotônicos e coerentes através do wrap.

**Onde pesquisar:**
- Pesquise "millis() overflow arduino" — o idioma `(long)(current - previous) >= interval` e por que ele é seguro.
- Pesquise "integer overflow rates" para construir a tabela bits × taxa × duração.

---

### 14. P1-8 — Comentários de pinos mentem: documentação divergente do código

**O problema, em uma frase:** o código define `SERVO_PIN` como 39 enquanto o comentário diz "GPIO7", e os pinos citados para o SPI do SD no comentário não são os pinos do barramento real — em hardware de voo, um comentário errado de pino não é deslize de documentação, é *defeito de segurança* esperando alguém confiar nele.

**A ideia:** o problema real não é esses dois comentários — é que a verdade sobre pinos está *duplicada* em prosa em vários lugares, e toda duplicação deriva. A correção estrutural é ter **uma única fonte de verdade** para o mapa de pinos (o `config.h` já é o candidato natural, pois os defines são a verdade operacional) e tratar qualquer menção de pino em prosa como *referência*, nunca como cópia. A auditoria imediata: sente com o esquemático do KiCad aberto, e confira define por define contra o hardware — qualquer divergência entre esquemático, define e comentário é um bug a resolver (e nem sempre o define é o errado: às vezes o firmware compensa o erro de layout — decida caso a caso com o esquemático como árbitro). Estabeleça o costume: mudou pino, muda em um lugar, e a tabela gera-se de lá.

**Perguntas para responder antes de codar:**
- Quem é o árbitro final — esquemático ou código? (Dica: o hardware que voa é o árbitro; tudo deve convergir para ele)
- Como garantir que novos comentários/READMEs não reintroduzam cópias de pinos? (conecta com a tarefa 20/P2-6)
- Existe alguma placa antiga voando com o pino do comentário? Como saber?

**Como saber que terminou:** auditoria completa define-por-define contra o esquemático registrada (um PR comentado ou tabela no repositório), e nenhuma menção de pino em prosa sem apontar para a fonte única.

**Onde pesquisar:**
- O esquemático do seu próprio projeto (KiCad) — não existe atalho para esta auditoria.

---

## P2 — Processo de engenharia

### 15. P2-1 — Testar o C++ real no host, não uma reimplementação em Python

**O problema, em uma frase:** a validação da FSM hoje é feita por um modelo paralelo em Python — que já derrapou da realidade (a documentação cita 50 Hz num lugar, 5 Hz no outro, e os comentários de confirmação citam janelas em ciclos "@50Hz" que não correspondem ao config atual) — ou seja: o que é testado não é o que voa.

**A ideia:** o princípio é *testar o artefato, não uma imitação dele*. O firmware atual roda na Arduino IDE, mas o mesmo código-fonte pode ser compilado para o host em um ambiente *native* do PlatformIO — e as dependências de Arduino (funções `millis`, `Serial`, `delay`, NVS, etc.) substituídas por *stubs* controláveis no teste. O ganho transformador: no host você pode *replayar* um CSV de voo real contra a FSM de produção, clock a clock, e — mais importante — injetar falhas que são impossíveis (ou caras) de causar em placa: barômetro congelado, NaN, saturação de IMU, reboot imediatamente após cada caminho de deploy (a tarefa 4/P0-4 *exige* esse teste), perda de GPS. Fault injection em host é o superpoder do teste embarcado. Escolha um framework de asserções leve (Unity é o clássico C/C++ embarcado) e comece pequeno: a FSM primeiro, por ser o maior risco (58 arestas no grafo, betweenness 0,390 — mudanças nela exigem regressão completa).

**Perguntas para responder antes de codar:**
- Quais dependências de Arduino a FSM/sensores tocam, e qual a superfície mínima de stubs para compilar no host? (liste antes de escrever o primeiro stub)
- Como representar o tempo no teste — um "clock fake" que avança por comando? Isso permite determinismo total; como desenhar?
- O replay do CSV real vira teste de regressão *permanente* (roda no CI — tarefa 16/P2-2)? Qual critério de passe (mesmos eventos, mesma ordem, tolerância de timing)?

**Como saber que terminou:** `pio test -e native` roda a FSM de produção contra o CSV de voo real + suíte de fault injection, tudo sem hardware, em segundos, e verde.

**Onde pesquisar:**
- PlatformIO Unit Testing (ambiente native + on-host): https://docs.platformio.org/en/latest/advanced/unit-testing/
- Unity (framework de testes): https://github.com/ThrowTheSwitch/Unity
- Pesquise "hardware abstraction layer embedded unit testing stubs" — o padrão de desacoplar tempo/periféricos para testar lógica.

---

### 16. P2-2 — Build manual na IDE → build reproduzível com análise estática em CI

**O problema, em uma frase:** o firmware que voa é compilado à mão, na IDE, na máquina de quem compilou — nenhum garantia de que o próximo build é o mesmo, nenhum portão automático de qualidade entre um commit e um firmware de voo.

**A ideia:** migrar o build para PlatformIO (mesma toolchain Arduino-ESP32, porém scriptável e versionável — o `platformio.ini` passa a descrever o build *no repositório*), e sobre essa base montar um pipeline de CI que roda a cada push: compilação com warnings promovidos a erro (pesquise as flags `-Wall -Wextra -Werror` e por que um warning ignorado vira bug de produção), análise estática com `cppcheck` e `clang-tidy` (cada um pega classes diferentes de problemas — vale entender o repertório de cada um), formatação automática com `clang-format` (discuta com o time um estilo e *feche* o assunto — o valor está na consistência, não na escolha "perfeita"), e o job de testes nativos da tarefa 15/P2-1. Referência de rigor: as regras do *Power of Ten* da NASA/JPL são a leitura recomendada para calibrar o que um firmware crítico merece exigir de si mesmo. O objetivo final: um merge no CI verde = um firmware que *qualquer um* no time consegue reproduzir bit a bit.

**Perguntas para responder antes de codar:**
- GitHub Actions ou outro CI — o que o time já usa/knows? Qual o menor pipeline que já entrega valor (só build + test primeiro)?
- Quantos warnings o codebase atual tem com `-Wall -Wextra`? (rode antes de decidir `-Werror` — um salto de 0 para 300 warnings exige uma estratégia de limpeza em etapas)
- `clang-format` sobre o código existente gerará um diff gigante — como dividir isso sem destruir o `git blame`/histórico de uma vez?

**Como saber que terminou:** um commit que quebra warning, teste ou análise estática é *rejeitado automaticamente* pelo CI; e qualquer membro do time gera o mesmo binário com um comando, sem IDE.

**Onde pesquisar:**
- PlatformIO (CI + environments): https://docs.platformio.org/en/latest/
- clang-tidy: https://clang.llvm.org/extra/clang-tidy/ · cppcheck: https://cppcheck.sourceforge.io/
- Holzmann, *Power of Ten* (o padrão de rigor que o CI vai *enforcar*): https://spinroot.com/gerard/pdf/P10.pdf

---

### 17. P2-3 — Tempos em "ciclos a X Hz" espalhados pelo config: mudar a frequência recalibra o sistema em silêncio

**O problema, em uma frase:** o `config.h` expressa janelas de tempo críticas como contagens de ciclos comentadas com a frequência atual ("5 ciclos @ 50 Hz", "13 ciclos // 2.6s @5Hz") — se alguém mudar a frequência do loop (o que a tarefa 2/P0-2 propõe!), *todos os tempos de segurança do sistema mudam juntos e sem aviso*.

**A ideia:** a regra de ouro é: **tempo é tempo, e se expressa em milissegundos**; ciclos são detalhe de implementação derivado. Inverta a dependência: os defines passam a declarar a *intenção* em ms (as janelas de confirmação de freefall, de barômetro travado, de paraquedas — cada uma com um comentário explicando o *porquê* daquele número, que é o que se perde primeiro), e a contagem de ciclos é calculada a partir do período. A ferramenta que torna isso seguro é `static_assert` — verificação em tempo de *compilação* de que a janela em ms pretendida e a contagem de ciclos derivada ainda correspondem; se alguém mudar a frequência, o build *quebra* com uma mensagem legível em vez de recalibrar a segurança do foguete em silêncio. Pesquise `static_assert` e `constexpr` em C++ — o par permite esse tipo de matemática de configuração verificada pelo compilador, de graça.

**Perguntas para responder antes de codar:**
- Liste todas as constantes expressas em ciclos no config atual — para cada uma, qual a *janela em ms pretendida*? (o comentário às vezes diz, às vezes é preciso raciocinar — e é aí que se descobre surpresa)
- Ciclos não-inteiros: se a janela em ms não divide o período exatamente, arredonda para cima ou para baixo — e o que isso significa em termos de segurança? (dica: para janelas de confirmação, a resposta é sempre "para o lado seguro")
- Como documentar o *porquê* de cada janela — 600 ms de confirmação de paraquedas vem de quê (análise? simulação? chute documentado)?

**Como saber que terminou:** mudar a frequência do loop de propósito no commit de teste *quebra o build* com um `static_assert` explicativo — e todos os comentários de "ciclos @ XHz" desaparecem do código.

**Onde pesquisar:**
- Pesquise `static_assert` C++11 e "compile time configuration checking embedded"
- O histórico do seu próprio `config.h` (git blame nos comentários) — entender de onde veio cada número.

---

### 18. P2-4 — Alocação dinâmica e `String` no caminho crítico

**O problema, em uma frase:** o caminho de execução do voo usa `new` sem verificação e `String` (que aloca por baixo) em pontos críticos — fragmentação de heap e falha de alocação em pleno voo são modos de falha que o código crítico de um sistema de segurança não pode se dar ao luxo de ter.

**A ideia:** a regra (que é literalmente uma das dez do *Power of Ten* da NASA) é: **em código crítico, nenhuma alocação dinâmica depois da inicialização**. A razão é dupla e vale entender as duas: (1) alocação pode *falhar* — e falha de alocação descoberta em plena descida é o pior momento possível para descobrir; (2) alocação/desalocação repetida *fragmenta* o heap — o sistema pode ter memória total sobrando e ainda assim falhar uma alocação específica depois de horas de fragmentação (pesquise "heap fragmentation embedded" — é um problema sutil e clássico). O plano de migração: sensores e FSM como instâncias estáticas (o número deles é fixo e conhecido no boot — nada justifica heap), buffers de texto com tamanho fixo e a família `snprintf` no lugar de `String` (pesquise as diferenças de segurança das funções da família `printf` — tem armadilha), e o heap permitido *só* na inicialização, onde falha é detectável e o safe-hold é barato. Migre por partes, começando pelo caminho do voo (telemetria, FSM, logger), não pelo código de setup.

**Perguntas para responder antes de codar:**
- Onde exatamente `String` aparece no caminho de voo (formatação de telemetria? log?) e qual o tamanho máximo real de cada string formatada? (mensurar antes de fixar buffers)
- Como verificar que *zero* alocação acontece pós-boot — existe hook/métrica no ESP-IDF para isso? (conecta com o pacote de saúde da tarefa 10/P1-4)
- O que fazer com as mensagens de log de emergência — buffers fixos reduzidos? Qual o orçamento?

**Como saber que terminou:** instrumentação de alocação mostra zero malloc/new após a inicialização em um voo completo simulado; e um grep por `String` no caminho de voo não retorna nada.

**Onde pesquisar:**
- *Power of Ten* — a regra 3 e sua justificativa: https://spinroot.com/gerard/pdf/P10.pdf
- Pesquise "heap fragmentation embedded systems" e "snprintf vs sprintf safety C"

---

### 19. P2-5 — Rastreabilidade de configuração: com quais parâmetros *esse* voo voou?

**O problema, em uma frase:** dois CSVs de dois voos diferentes são incomparáveis hoje — não há registro dos thresholds efetivos, do firmware exato (git hash) nem do modo operacional com que cada um voou.

**A ideia:** o princípio é o do *black box de aviação*: o registro do voo carrega sua própria identidade. Duas peças: (1) todo artifact de dados de um voo (cabeçalho do CSV, pacote de saúde da tarefa 10/P1-4) começa com um bloco de metadados — thresholds *efetivos* (lidos dos valores em uso no momento, não dos defaults do código, que podem divergir do build), hash do commit do build, e modo degradado vigente; (2) o hash do build precisa ser capturado *no build* (pesquise como injetar o git hash no binário em tempo de compilação — flags de pré-processador geradas por script — e como expô-lo como constante legível). O efeito prático: quando um voo se comportar estranho, a primeira pergunta — "com que parâmetros e com que build isso voou?" — passa a ter resposta objetiva no primeiro arquivo que você abrir, sem arqueologia de git.

**Perguntas para responder antes de codar:**
- O cabeçalho de metadados quebra parsers existentes do CSV (scripts de análise, ground station)? Como versionar o formato do cabeçalho?
- Quais thresholds listar — todos, ou só os que participam de decisão de voo? (equilibre completude vs. tamanho — o CSV roda no rádio também)
- Como garantir que os valores reportados são os *efetivos* (pós-calibração, pós-override) e não os defaults do código?

**Como saber que terminou:** abrindo o CSV de um voo, a primeira seção responde: build (hash), thresholds em uso, modo de voo — sem consultar o repositório.

**Onde pesquisar:**
- Pesquise "embed git hash compile time C++" — o padrão `build_id`/`version.h` gerado por script de build.
- OpenFlightLogger e black boxes de aviação (pesquise "flight data recorder metadata header") — precedentes de formato.

---

### 20. P2-6 — Documentação viva: matar os drifts e impedir que voltem a nascer

**O problema, em uma frase:** os comentários e docs já derraparam da realidade em vários pontos (50 Hz vs 5 Hz, "Every 20ms", autor "#100" vs "#11", pinos do item 14/P1-8) — e cada drift corrigido à mão *voltará*, porque nada estrutural impede o código e a prosa de divergirem de novo.

**A ideia:** duas camadas. A imediata: a caça aos drifts conhecidos (todos mapeados no diagnóstico original — frequências, períodos, autor, pinos), corrigidos na fonte, com uma passada de "o que mais este arquivo afirma sobre o sistema que posso verificar contra o código agora?". A estrutural: o princípio é que **prosa não é lugar de dado** — qualquer número que já existe no código (frequência, período, threshold, pino) e é repetido em comentário/README *vai* deriva; a saída é gerar a documentação a partir da fonte (a tabela de thresholds, por exemplo, pode ser gerada do `config.h` por script — conecta com a tarefa 16/P2-2 de CI, que pode verificar automaticamente que docs geradas estão frescas) ou referenciar o define sem repetir o valor. Reflita sobre o hábito do time: onde os números "convenientemente repetidos" entram em review — e como o checklist de review os pega.

**Perguntas para responder antes de codar:**
- Quem é o dono da verdade para cada categoria de informação (pinos, tempos, frequências, autor, hardware)? Onde essa verdade mora?
- Docs geradas por script: quais valem a geração (tabela de thresholds, mapa de pinos) e quais ficam prosa pura?
- Como tratar o `@author` — um autor por arquivo congelado no tempo, ou algo que reflita a manutenção real?

**Como saber que terminou:** uma auditoria manual encontra zero divergências código↔docs nos pontos conhecidos; e o CI (tarefa 16/P2-2) falha se alguém editar o código sem regenerar a doc gerada.

**Onde pesquisar:**
- O próprio diagnóstico `IMPROVEMENTS.md` — a lista de drifts conhecidos (seção P2-6) é o checklist inicial.
- Pesquise "docs as code single source of truth" — o padrão de gerar docs a partir da fonte.

---

## Roadmap de entrega por etapas (do diagnóstico original)

| Etapa | Itens (números deste plano) | Critério de aceite |
|---|---|---|
| **1 — Riscos imediatos** | 4 (P0-4), 5 (P0-5), 6 (P0-6) | Reboot após qualquer deploy mantém servo aberto; zero pacotes intercalados; ToA medido < 50% do canal |
| **2 — Redundância de sensor** | 1 (P0-1), 3 (P0-3), 8 (P1-2) | Modos degradados ejetam em simulação e drop test; BIT do servo no pad |
| **3 — Estimação e apogeu** | 2 (P0-2), 17 (P2-3), 11 (P1-5) | Zero falsos apogeus no replay + RocketPy; Vz filtrado com ganhos documentados |
| **4 — Observabilidade** | 7 (P1-1), 9 (P1-3), 10 (P1-4), 13 (P1-7) | Pacote de saúde no ground; log de alta taxa na PSRAM; sem panic por flash |
| **5 — Processo** | 15 (P2-1), 16 (P2-2), 18 (P2-4), 19 (P2-5), 20 (P2-6) | CI verde com testes nativos e análise estática; docs sem drift |

## Revalidação antes de voar (checklist do diagnóstico)

1. Replay do CSV de voo real no C++ de produção, incluindo fault injection (tarefa 15).
2. Teste de bancada de ejeção com medição de VBAT e continuidade (tarefa 3).
3. Reboot forçado em cada estado da FSM e após cada origem de deploy (tarefa 4).
4. Teste de enlace: ToA medido, taxa recebida e RSSI no ground station (tarefa 6).
5. Drop test com e sem barômetro/IMU operantes (tarefa 1).

---

*Este plano descreve problemas, ideias e direções de estudo — deliberadamente sem código ou soluções prontas. A implementação de cada item é o trabalho de engenharia que ele prepara.*
