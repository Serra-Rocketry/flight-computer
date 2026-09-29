# Explicação: Simulador de Máquina de Estados Finitos (FSM) para Foguete

## Visão Geral

Este código Python (`FSM_Tester.py`) simula uma **Máquina de Estados Finitos (FSM - Finite State Machine)** que será implementada em um **ESP32** para controle de voo de um foguete. O objetivo é testar a lógica de detecção de eventos de voo usando dados simulados ou reais antes de implementar o código no microcontrolador embarcado.

## Propósito do Código

O simulador serve para:

1. **Validar algoritmos de detecção** de fases de voo antes da implementação em hardware
2. **Ajustar parâmetros** (thresholds) de detecção com segurança
3. **Visualizar eventos** detectados em dados de voo reais ou simulados
4. **Garantir segurança** ao testar lógica crítica sem riscos ao hardware

## Arquitetura da FSM

A máquina de estados implementa **transições sequenciais** entre as fases de voo de um foguete:

```
IDLE (Repouso)
    ↓ [Aceleração > 15 m/s²]
LIFTOFF (Decolagem)
    ↓ [Aceleração < 2 m/s² OU az < -8 m/s²]
BURNOUT (Fim do Motor)
    ↓ [Velocidade vertical ≈ 0 E aceleração negativa]
APOGEE (Apogeu)
    ↓ [Aceleração total < 11.5 m/s² E descendo]
FREEFALL (Queda Livre)
    ↓ [Altitude ≤ 100m E descendo]
PARACHUTE (Paraquedas Aberto)
    ↓
LANDED (Pouso)
```

## Componentes Principais

### 1. Funções de Detecção de Estados

#### `detect_liftoff()` - Detecção de Decolagem
**Linha:** 117-139

**Critério:** Aceleração total > 15.0 m/s²

**Funcionamento:**
- Calcula aceleração total: `√(ax² + ay² + az²)`
- Ignição do motor causa pico súbito de aceleração
- Validações: verifica NaN/Inf para evitar falsos positivos

**Threshold ESP32:** `> 15.0 m/s²`

---

#### `detect_motor_burnout()` - Detecção de Fim do Motor
**Linha:** 18-55

**Critérios:**
1. Aceleração vertical < -8.0 m/s² (gravidade domina)
2. OU aceleração total < 2.0 m/s² (coasting)

**Funcionamento:**
- Detecta quando o motor para de gerar empuxo
- Requer altitude > 5m e velocidade vertical > 0.5 m/s (evita falsos positivos no solo)
- Fim da fase propulsada → início do voo balístico

**Thresholds ESP32:**
- `az < -8.0 m/s²` OU `total_acc < 2.0 m/s²`
- Altitude mínima: `5.0 m`
- Velocidade vertical mínima: `0.5 m/s`

---

#### `detect_apogee_acceleration()` - Detecção de Apogeu
**Linha:** 58-78

**Critérios:**
1. Velocidade vertical absoluta < 1.0 m/s (quase zero)
2. E aceleração vertical < -0.1 m/s² (descendo)

**Funcionamento:**
- Ponto mais alto da trajetória
- Velocidade vertical inverte de positiva para negativa
- Momento crítico para acionamento de sistema de recuperação

**Thresholds ESP32:**
- `|vz| < 1.0 m/s`
- `az < -0.1 m/s²`

---

#### `detect_freefall()` - Detecção de Queda Livre
**Linha:** 81-114

**Critérios:**
1. Aceleração total < 11.5 m/s² (próxima à gravidade)
2. E velocidade vertical < -5 m/s (descendo rapidamente)
3. E altura > 5m

**Funcionamento:**
- Detecta fase balística após apogeu
- Aceleração dominada pela gravidade (~9.8 m/s²)
- Estado anterior ao acionamento do paraquedas

**Thresholds ESP32:**
- `total_acc < 11.5 m/s²`
- `vz < -5 m/s`
- Altitude mínima: `5.0 m`

---

#### `altitude_trigger_factory()` - Gatilho de Altitude
**Linha:** 142-155

**Critério:** Altitude ≤ altitude_alvo (padrão: 100m)

**Funcionamento:**
- Factory function que cria gatilhos configuráveis por altitude
- Opção `require_descent=True` garante que só aciona durante descida
- Usado para abrir paraquedas em altitude segura

**Configuração ESP32:**
```python
PARACHUTE_ALTITUDE = 100  # metros
parachute_trigger = altitude_trigger_factory(PARACHUTE_ALTITUDE)
```

---

### 2. Processamento de Dados

#### Entrada de Dados (CSV)
**Linhas:** 5-16

**Formatos suportados:**
```csv
# dados_simulados.csv
millis, altp, ax, ay, az

# 13_30_11-Dados.csv e dados_filtrados.csv
millis, lat, lon, sat, alt, data, hora, altp, p, ax, ay, az, gx, gy, gz, pqd
```

**Campos utilizados:**
- `millis`: Timestamp em milissegundos (convertido para segundos)
- `altp`: Altitude barométrica em metros
- `ax, ay, az`: Acelerações nos eixos X, Y, Z em m/s²

---

#### Cálculo de Velocidade Vertical
**Linhas:** 189-202

**Método:** Diferenciação numérica
```python
vz = (altura_atual - altura_anterior) / (tempo_atual - tempo_anterior)
```

**Limites:**
- Clipping: `-200 m/s` a `+200 m/s`
- Evita valores espúrios em dados ruidosos

---

#### Vetor de Estado
**Linha:** 204
```python
state_vector = [x, y, height, vx, vy, vz]
```

**Componentes:**
- Posição: `[x, y, height]` - coordenadas espaciais (x e y não utilizados nesta versão)
- Velocidade: `[vx, vy, vz]` - componentes da velocidade (apenas vz utilizado)

---

#### Vetor de Aceleração
**Linha:** 205
```python
u_dot = [ax_dot, ay_dot, az_dot, ax, ay, az]
```

**Componentes:**
- Derivadas: `[ax_dot, ay_dot, az_dot]` - não utilizadas nesta versão
- Acelerações: `[ax, ay, az]` - leituras diretas dos acelerômetros

---

### 3. Loop Principal de Processamento
**Linhas:** 177-236

**Algoritmo:**
```
Para cada linha do CSV:
    1. Ler altitude e acelerações
    2. Calcular velocidade vertical (vz)
    3. Construir vetores de estado
    4. Verificar transições sequenciais:
       - SE não decolou E detect_liftoff() → LIFTOFF
       - SE decolou E não burnout E detect_motor_burnout() → BURNOUT
       - SE burnout E não apogeu E detect_apogee() → APOGEE
       - SE apogeu E não freefall E detect_freefall() → FREEFALL
       - SE freefall E não paraquedas E parachute_trigger() → PARACHUTE
    5. Registrar eventos detectados
```

**Características:**
- **Transições unidirecionais:** Estados só avançam, nunca retrocedem
- **Dependências em cadeia:** Cada estado requer que o anterior tenha ocorrido
- **Logging detalhado:** Imprime tempo, altitude, velocidades e acelerações de cada evento

---

### 4. Visualização
**Linhas:** 238-253

**Gráfico gerado:**
- **Eixo X:** Tempo em segundos
- **Eixo Y:** Altitude barométrica em metros
- **Marcadores:** Linhas verticais tracejadas em cada evento detectado
- **Labels:** Nome do evento (liftoff, burnout, apogee, freefall)

---

## Dados Simulados Utilizados

O código pode processar três tipos de arquivos CSV:

### 1. `dados_simulados.csv`
- Dados de simulação computacional
- 1.510 pontos de dados
- Trajetória idealizada sem ruído
- Melhor para validação inicial de lógica

### 2. `dados_filtrados.csv`
- Dados reais de voo filtrados
- 61 pontos (dados decimados)
- Inclui GPS, pressão barométrica, IMU completa
- Campo `pqd` indica acionamento de paraquedas

### 3. `13_30_11-Dados.csv`
- Dados de voo real completos
- 1.873 pontos de telemetria
- Sensor: IMU + GPS + barômetro
- Usado no código atual (linha 6)

---

## Tradução para ESP32

O código Python foi projetado para **tradução direta** para C++ no ESP32:

### Equivalências de Implementação

| Python | ESP32 (C++/Arduino) |
|--------|---------------------|
| `detect_liftoff()` | `bool detectLiftoff(float ax, float ay, float az)` |
| `detect_motor_burnout()` | `bool detectMotorBurnout(float height, float vz, float* accel)` |
| `detect_apogee()` | `bool detectApogee(float vz, float az)` |
| `detect_freefall()` | `bool detectFreefall(float height, float vz, float totalAccel)` |
| `numpy.sqrt()` | `sqrtf()` da biblioteca `<math.h>` |
| `pandas DataFrame` | Arrays circulares ou buffers de dados |
| Loop `for i, row in df.iterrows()` | `loop()` principal do Arduino |

### Exemplo de Implementação ESP32

```cpp
// Definição de estados
enum FlightState {
    IDLE,
    LIFTOFF,
    BURNOUT,
    APOGEE,
    FREEFALL,
    PARACHUTE_DEPLOYED,
    LANDED
};

FlightState currentState = IDLE;

// Variáveis de sensor
float ax, ay, az;        // Acelerômetro
float altitude;          // Barômetro
float vz = 0.0;         // Velocidade vertical calculada
float prevAltitude = 0.0;
unsigned long prevTime = 0;

void loop() {
    // 1. Ler sensores
    readIMU(&ax, &ay, &az);
    altitude = readBarometer();
    
    // 2. Calcular velocidade vertical
    unsigned long currentTime = millis();
    float dt = (currentTime - prevTime) / 1000.0;  // segundos
    if (dt > 0) {
        vz = (altitude - prevAltitude) / dt;
    }
    
    // 3. Máquina de estados
    switch(currentState) {
        case IDLE:
            if (detectLiftoff(ax, ay, az)) {
                currentState = LIFTOFF;
                Serial.println("LIFTOFF");
            }
            break;
            
        case LIFTOFF:
            if (detectMotorBurnout(altitude, vz, ax, ay, az)) {
                currentState = BURNOUT;
                Serial.println("BURNOUT");
            }
            break;
            
        case BURNOUT:
            if (detectApogee(vz, az)) {
                currentState = APOGEE;
                Serial.println("APOGEE");
            }
            break;
            
        case APOGEE:
            if (detectFreefall(altitude, vz, ax, ay, az)) {
                currentState = FREEFALL;
                Serial.println("FREEFALL");
            }
            break;
            
        case FREEFALL:
            if (altitude <= 100 && vz < 0) {
                currentState = PARACHUTE_DEPLOYED;
                deployParachute();  // Ativar servo/pirotécnico
                Serial.println("PARACHUTE");
            }
            break;
    }
    
    // 4. Atualizar histórico
    prevAltitude = altitude;
    prevTime = currentTime;
    
    delay(10);  // 100 Hz de atualização
}

bool detectLiftoff(float ax, float ay, float az) {
    float totalAccel = sqrtf(ax*ax + ay*ay + az*az);
    return totalAccel > 15.0;
}

bool detectMotorBurnout(float height, float vz, float ax, float ay, float az) {
    if (height < 5.0 || vz <= 0.5) return false;
    
    float totalAccel = sqrtf(ax*ax + ay*ay + az*az);
    return (az < -8.0) || (totalAccel < 2.0);
}

bool detectApogee(float vz, float az) {
    return (fabs(vz) < 1.0) && (az < -0.1);
}

bool detectFreefall(float height, float vz, float ax, float ay, float az) {
    if (height < 5.0 || vz >= -0.2) return false;
    
    float totalAccel = sqrtf(ax*ax + ay*ay + az*az);
    return (totalAccel < 11.5) && (vz < -5);
}
```

---

## Funcionalidades de Segurança

### 1. Validação de Dados
**Implementado em todas as funções de detecção:**
```python
if not all(np.isfinite([ax, ay, az])):
    return False
```

**Previne:**
- Leituras NaN (Not a Number)
- Valores infinitos
- Overflow/underflow numérico

---

### 2. Guardas de Estado
**Exemplo em `detect_motor_burnout()` (linhas 47-48):**
```python
if height < 5.0 or vz <= 0.5:
    return False
```

**Propósito:**
- Evita falsos positivos no solo (altura < 5m)
- Garante que o foguete está em movimento ascendente
- Previne acionamentos prematuros

---

### 3. Transições Condicionais
**Linha 213:**
```python
elif not burnout and detect_motor_burnout(...) and liftoff:
```

**Lógica:**
- `not burnout`: Evento ainda não ocorreu
- `detect_motor_burnout()`: Condição física atendida
- `and liftoff`: Estado anterior confirmado

**Resultado:** Transições só ocorrem na sequência correta

---

### 4. Suavização Opcional
**Linhas 180-187 (comentadas):**
```python
# height = smooth(height, prev_height, alpha=0.2)
```

**Função `smooth()` (linhas 171-174):**
```python
def smooth(value, prev_value, alpha):
    return alpha * value + (1 - alpha) * prev_value
```

**Aplicação:**
- Filtro passa-baixa (exponential moving average)
- Alpha = 0.2: 20% valor novo, 80% histórico
- Reduz ruído dos sensores
- **Comentado** para preservar resposta rápida

---

## Ajustes de Parâmetros (Tuning)

### Thresholds Críticos

| Parâmetro | Valor | Localização | Efeito ao Aumentar |
|-----------|-------|-------------|-------------------|
| Aceleração de Liftoff | 15.0 m/s² | Linha 137 | Detecção mais tardia, menos falsos positivos |
| Aceleração de Burnout | 2.0 m/s² | Linha 53 | Detecção mais tardia |
| Az negativo (Burnout) | -8.0 m/s² | Linha 53 | Detecção mais sensível a gravidade |
| Velocidade de Apogeu | 1.0 m/s | Linha 76 | Janela de detecção mais estreita |
| Aceleração de Freefall | 11.5 m/s² | Linha 112 | Detecção mais próxima de ~1g |
| Velocidade de Freefall | -5 m/s | Linha 112 | Requer descida mais rápida |
| Altitude de Paraquedas | 100 m | Linha 158 | Abertura mais alta/baixa |

### Recomendações para Ajuste

1. **Liftoff:** Aumentar threshold se houver vibrações pré-voo
2. **Burnout:** Ajustar baseado no perfil do motor (thrust curve)
3. **Apogeu:** Reduzir `|vz|` para foguetes com apogeu baixo
4. **Freefall:** Ajustar para arrasto aerodinâmico do foguete
5. **Paraquedas:** Considerar velocidade terminal e altura mínima segura

---

## Limitações e Melhorias Futuras

### Limitações Atuais

1. **Sem Filtro de Kalman:** Velocidade calculada por diferenciação simples (ruidosa)
2. **Eixos X/Y ignorados:** Não detecta instabilidade lateral
3. **Sem validação de GPS:** Altitude barométrica pode ter drift
4. **Hardcoded thresholds:** Valores fixos não adaptam a diferentes foguetes
5. **Sem detecção de pouso:** FSM para em freefall/paraquedas

### Melhorias Sugeridas para ESP32

1. **Filtro de Kalman:** Fusão de sensores (barômetro + acelerômetro + GPS)
2. **Watchdog timer:** Reset automático se FSM travar
3. **Log em SD Card:** Gravar telemetria completa para análise pós-voo
4. **Redundância:** Múltiplos critérios para eventos críticos (ex: apogeu)
5. **Calibração automática:** Ajuste de thresholds baseado em pré-voo
6. **Detecção de anomalias:** Tumbling, perda de sinal, falha de sensor
7. **Estado de pouso:** Detectar impacto e estabilização

---

## Fluxo de Desenvolvimento

```
┌──────────────────────────────────────────────────┐
│  1. SIMULAÇÃO (Python - FSM_Tester.py)           │
│     - Testar lógica com dados históricos         │
│     - Ajustar thresholds                          │
│     - Visualizar eventos                          │
└────────────────────┬─────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────┐
│  2. TRADUÇÃO (C++/Arduino)                       │
│     - Implementar funções de detecção            │
│     - Adaptar para leitura de sensores real-time │
│     - Adicionar máquina de estados switch-case   │
└────────────────────┬─────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────┐
│  3. TESTES EM BANCADA (ESP32 + Sensores)        │
│     - Simular acelerações manualmente            │
│     - Validar leituras de sensores               │
│     - Testar acionamento de servo/pirotécnico    │
└────────────────────┬─────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────┐
│  4. VOO TESTE (Foguete real)                     │
│     - Coletar telemetria real                    │
│     - Analisar eventos detectados                │
│     - Refinar thresholds se necessário           │
└────────────────────┬─────────────────────────────┘
                     ↓
┌──────────────────────────────────────────────────┐
│  5. ITERAÇÃO (Retornar ao passo 1)               │
│     - Analisar dados do voo teste                │
│     - Ajustar simulação com dados reais          │
│     - Melhorar algoritmos                         │
└──────────────────────────────────────────────────┘
```

---

## Conclusão

Este simulador é uma ferramenta essencial para desenvolvimento seguro de software de voo. Ele permite:

✅ **Validar** lógica de detecção sem risco ao hardware  
✅ **Iterar rapidamente** em ajustes de parâmetros  
✅ **Visualizar** comportamento da FSM em dados reais  
✅ **Traduzir facilmente** para implementação em ESP32  

A arquitetura modular e os critérios defensivos garantem que o código seja **robusto**, **testável** e **pronto para produção** em sistemas críticos de segurança como computadores de voo de foguetes.

---

## Referências

- Código fonte: `FSM_Tester.py`
- Dados de voo: `13_30_11-Dados.csv`, `dados_simulados.csv`, `dados_filtrados.csv`
- Notebook de análise: `OpenCSV.ipynb`
- Documentação ESP32: [Espressif Official Docs](https://docs.espressif.com/)
