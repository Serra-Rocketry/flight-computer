# Teste de Bancada — Servo do Paraquedas sob Carga Real (Risco nº 4)

**Local:** `test/parachute_servo/` · **Sketch:** `parachute_servo.ino` · **Analisador:** `extras/FSM_tester/analyze_servo_open_times.py`

## 1. Objetivo e contexto (por que este teste existe)

O firmware de voo (`firmware/parachute_module.cpp`, `firmware/config.h`) abre o
paraquedas com um único comando: `ParachuteServo.write(SERVO_OPEN)` (0°), partindo
de `SERVO_CLOSED` (90°), e depois apenas seta uma flag lógica
(`g_parachuteActuated`). **Não existe feedback**: não há fim de curso, nem medição
de corrente, nem confirmação de que a porta do compartimento realmente abriu.

Risco nº 4 do flight-computer: **se a porta travar, o velame empacotado exigir
mais torque do que o servo entrega, ou o elástico/mola empurrar o servo de volta
(back-drive), o servo trava (stall) sem o firmware perceber** — e o paraquedas
não abre no apogeu (regra inegociável: deploy no apogeu; `PARACHUTE_MIN_ALTITUDE`
= 50 m é só guarda de solo).

Este teste de bancada valida o conjunto **servo + mecanismo real** (porta + velame
empacotado + elástico/mola, se existir) medindo:

1. **Abertura completa** sob carga real (porta abre até o fim de curso).
2. **Tempo** comando → abertura total (com microswitch de bancada ou cronômetro).
3. **Repetibilidade** — 20 tentativas, estatística (média, desvio, pior caso).
4. **Falhas de movimento** — tempo acima do limite, porta que não segura aberta.
5. **Corrente de stall** — medida com multímetro em série (procedimento na seção 5).

> ⚠️ **Escopo:** teste de BANCADA. Não substitui o teste de integração em voo
> real, mas é o primeiro portão de aprovação do mecanismo de deploy. O microswitch
> é **fixture de bancada**, não é hardware de voo.

## 2. Materiais

| Item | Especificação |
|------|---------------|
| ESP32-S3 (mesma placa do voo) | Para reproduzir o mesmo PWM do firmware |
| Servo do paraquedas | O mesmo servo real do foguete (ver nota na seção 8) |
| Carga real | Porta do compartimento + velame empacotado + elástico/mola (mesma montagem do voo) |
| Microswitch de bancada (recomendado) | Fim de curso preso na estrutura: a porta o aciona ao abrir totalmente (liga FEEDBACK_PIN → GND) |
| Multímetro | Modo corrente (A), de preferência com **peak-hold**; ponteiras jacaré |
| Fonte / bateria 5 V | Mesma alimentação do voo (o BEC entrega 5 V / 3 A; ver seção 5) |
| Cabo USB + terminal serial | 115200 baud |
| Cronômetro (fallback) | Se não houver microswitch |

## 3. Montagem (rig de bancada)

1. **Prenda a porta com a carga real**: monte o compartimento exatamente como no
   voo — porta articulada no eixo, velame empacotado dentro, elástico/mola
   tensionado no estado fechado (se o mecanismo tiver). A carga **precisa ser a
   real**, senão o teste não mede o risco.
2. **Servo**: sinal (amarelo/laranja) → GPIO 10 (`SERVO_PIN`); vermelho → +5 V;
   marrom → GND.
3. **Microswitch (opcional, recomendado)**: fixe na estrutura do compartimento de
   forma que a porta, **totalmente aberta**, pressione o botão. Ligue
   `FEEDBACK_PIN` (GPIO 4, padrão do sketch) → microswitch → GND. O sketch usa
   `INPUT_PULLUP`; porta aberta = nível `LOW`. Se não usar microswitch, deixe
   `FEEDBACK_PIN = -1` no sketch (modo manual, seção 4.4).
4. **Multímetro em série** (para corrente): corte/estenda o fio vermelho do servo
   e ligue o multímetro em série entre a fonte 5 V e o servo. Configure o modo
   **A (corrente)**, porta 10 A se disponível, e ative **peak-hold** para capturar
   o pico.
5. **Compile e grave**:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32s3 test/parachute_servo
   arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 test/parachute_servo
   ```
   Abra o monitor serial a **115200 baud**.

> 🔒 **Atenção:** não altere nada em `hardware/electronics/**` — contém mudanças
> pré-existentes de outro trabalho. Este teste vive em `test/` e `extras/`.

## 4. Procedimento

### 4.1 Regras gerais

- **Fixe os critérios ANTES do teste** (seção 6). Não afrouxe critérios depois de
  ver os resultados — se falhar, corrija o mecanismo.
- Entre cada tentativa: **re-empacote o velame e feche a porta** (a carga precisa
  ser re-armada do zero, igual ao pré-voo).
- Registre o log serial completo (copie/cole no analisador, seção 7).

### 4.2 Sequência de teste (20 tentativas)

1. Power-on: o sketch arma o servo em `CLOSED` (90°) — porta fechada/travada.
2. Feche a porta manualmente e re-empacote o velame (estado de pré-voo).
3. Envie **`o`** no serial → o sketch varre o servo de `CLOSED` a `OPEN` (90° → 0°)
   e mede o tempo até o microswitch acusar abertura total (ou pede o tempo do
   cronômetro, no modo manual).
4. O sketch segura `OPEN` por `HOLD_MS` (2 s) verificando que a porta **continua
   aberta** (elástico/mola não pode empurrar o servo de volta).
5. Registra a linha CSV (`RUN,OPEN_MS,TRIPPED,OPEN_FAIL,HOLD_FAIL`), volta para
   `CLOSED` e imprime `PASS`/`FAIL`.
6. Repita os passos 2–5 até **20 tentativas** (`REPETITIONS`). Envie **`s`** para
   ver a estatística parcial/quando quiser.
7. Ao final, envie **`s`** e registre o resumo. **Passe o veredito** (seção 6).

### 4.3 Medição de corrente (stall)

- Com o multímetro em série + peak-hold, anote o **pico de corrente de abertura**
  em cada tentativa (ou ao menos nas 20).
- Para a **corrente de stall real (rotor travado)**: com o mecanismo DESARMADO
  (sem porta), envie **`b`** — o sketch dá 2 s de aviso, vai a `OPEN` e segura por
  `STALL_HOLD_MS` (2 s) enquanto você **segura o braço do servo com a mão**,
  impedindo o movimento. Leia o pico no multímetro. Solte ao terminar.
  > ⚠️ Faça o teste bloqueado **por último e brevemente**: rotor travado = corrente
  > máxima + aquecimento. Não repita várias vezes seguidas.

### 4.4 Modo manual (sem microswitch)

Com `FEEDBACK_PIN = -1`, o sketch não detecta abertura: ele varre, imprime o
número da tentativa e pede `Enter measured open time (ms):`. Use um cronômetro
(ou vídeo em câmera lenta) do momento do comando até a porta abrir totalmente e
digite o valor. A verificação de *hold* fica desabilitada nesse modo — a porta
deve ser observada manualmente durante os 2 s seguintes.

## 5. Medição de corrente — como e por quê

O ESP32 não mede corrente (não há sensor no hardware). O procedimento é:

- Multímetro em série no fio **vermelho** (5 V) do servo, modo corrente, porta
  alta (10 A) ou peak-hold.
- Medir: (a) pico de corrente durante a abertura sob carga (cada tentativa) e
  (b) stall de rotor travado (comando `b`).
- **Por que importa:** a fonte de voo é um BEC 5 V / 3 A compartilhado com buzzer
  (~30 mA) e LoRa (~120 mA). Se a corrente de stall exceder a folga do rail, a
  tensão 5 V cai, o torque cai ainda mais e o ESP32 pode resetar — um falha dupla.
- **Corrente esperada:** para servo padrão 5 V com engrenagem metálica na faixa
  de ~4,8 kg·cm, o stall fica tipicamente entre **1,0 e 2,0 A**. Confirme contra
  o datasheet do servo real (ver seção 8).

## 6. Critérios de aprovação

Fixe antes de começar. Padrões sugeridos (ajuste às medidas do servo real **antes**
do primeiro run — nunca depois):

| # | Critério | Limite | Verificação |
|---|----------|--------|-------------|
| A | Abertura completa (porta no fim de curso) | **20/20 tentativas** | microswitch tripado (ou cronômetro, modo manual) |
| B | Tempo comando → abertura total | **< 1000 ms** em cada tentativa | `OPEN_MS` do log (servo especifica ~0,23 s/60° → ~345 ms sem carga; limite com folga para carga) |
| C | Porta permanece aberta (sem back-drive) | **20/20 tentativas** por ≥ 2000 ms | hold check do sketch |
| D | Corrente de pico de abertura | **≤ 1500 mA** (típico) | multímetro peak-hold |
| E | Corrente de stall (rotor travado) | **≤ 2500 mA** (teto do BEC: 3 A − buzzer − LoRa ≈ 2,7 A, com margem) | comando `b` + multímetro |

**Resultado final:** PASS se A–E passarem. Qualquer falha = FAIL do mecanismo
(só o servo em si não é aprovado).

## 7. Análise dos tempos

Copie o log serial (linhas `RUN,OPEN_MS,...`) para um arquivo e rode:

```bash
python3 extras/FSM_tester/analyze_servo_open_times.py log_servo.csv
# ou cole via stdin:
python3 extras/FSM_tester/analyze_servo_open_times.py < log_servo.csv
```

O script calcula média, desvio, pior caso e o veredito contra os critérios B/C
(exit code 0 = PASS, 1 = FAIL).

## 8. O que fazer se falhar

| Sintoma | Provável causa | Ação |
|---------|----------------|------|
| Abertura lenta (> 1000 ms) ou não abre | Torque insuficiente | Servo maior/mais torque; alimentar o servo com 6 V direto (se o BEC permitir); verificar sag de tensão no 5 V durante o stall (fonte fraca derruba o torque) |
| Trava em posição intermediária | Atrito / interferência da porta | Lubrificar dobradiça/trilho; verificar se o velame empacotado prende na borda; aliviar pontos de contato |
| Porta não segura aberta (back-drive) | Elástico/mola forte demais para o servo em `OPEN` | Reduzir pré-carga da mola; mudar ponto de fixação do braço (mais alavanca); ou servo com maior torque de holding |
| Corrente de stall acima do teto | Servo subdimensionado / BEC marginal | Servo com menor corrente de stall; fonte dedicada para o servo; revisar o rail 5 V |
| Pico de corrente na abertura > 1,5 A | Mecanismo com muita resistência | Reduzir atrito antes de trocar o servo (item 2 acima) |

Regra geral: **primeiro reduza atrito/pré-carga, depois aumente torque** — a
margem de torque medida na bancada é a margem que existe em voo.

## 9. Registro de resultados (preencher na execução)

| Data | Operador | Servo (modelo) | Trials | Aberturas OK | Tempo médio (ms) | Pior caso (ms) | I pico abertura (mA) | I stall (mA) | Veredito |
|------|----------|----------------|--------|--------------|------------------|----------------|-----------------------|--------------|----------|
|      |          |                | 20     |              |                  |                |                       |              |          |

## 10. Notas técnicas

- **Servo real a confirmar:** `docs/hardware.md` especifica "metal gears, ~4,8
  kg·cm, ~9 g" — 9 g é faixa de micro servo (~1,8 kg·cm, ~0,7 A de stall), 4,8
  kg·cm é faixa de servo de ~55 g (~2 A de stall). **Identifique o modelo real e
  ajuste os critérios D/E ao datasheet** antes do primeiro run.
- **Constantes espelhadas:** `SERVO_PIN`, `SERVO_OPEN`, `SERVO_CLOSED` duplicam
  `firmware/config.h` de propósito (o sketch é standalone). Se mudar no firmware,
  mude aqui também.
- **Critério B × firmware:** o firmware manda `write(SERVO_OPEN)` uma única vez e
  segue a vida (não re-escrita periódica). O hold check mede se o servo segura a
  posição sozinho sem refresh — se falhar, o firmware pode precisar re-escrever a
  posição periodicamente após o deploy (discutir com o time de firmware).
