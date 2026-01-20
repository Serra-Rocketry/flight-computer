# Documentação de Hardware - Computador de Bordo

## Visão Geral

O computador de bordo utiliza um microcontrolador ESP32-C3 como núcleo, integrado com sensores de altitude, GPS, IMU e um módulo de comunicação LoRa. O PCB foi projetado no KiCad com compatibilidade para a forma e conectores do foguete SR21000.

## Plataforma Principal

### ESP32-C3 SuperMini

**Especificações**:

- **Processador**: RISC-V 32-bit @ 160 MHz
- **RAM**: 400 KB (SRAM)
- **Flash**: 4 MB
- **Periféricos**: UART, SPI, I2C, GPIO, ADC, Timer
- **WiFi**: 802.11b/g/n
- **Tamanho**: Breakout compacto de 18x22 mm

**Pinos Utilizados**:

```
Pino 0   → BUZZER_PIN
Pino 1   → RST_LORA (Reset do LoRa)
Pino 2   → DIO0_LORA (Interrupção do LoRa)
Pino 3   → SERVO_PIN (Servo motor)
Pino 7   → SS_LORA (Chip Select LoRa)
Pino 20  → RX_GPS (Serial 1 RX)
Pino 21  → TX_GPS (Serial 1 TX)
Pino 4   → SDA (I2C - BMP280, MPU6050)
Pino 5   → SCL (I2C - BMP280, MPU6050)
Pino 6   → MOSI (SPI - LoRa)
Pino 8   → MISO (SPI - LoRa)
Pino 9   → CLK (SPI - LoRa)
```

**Alimentação**: 3.3V nominal (regulado por LM2596)

## Sensores

### 1. BMP280 - Sensor de Pressão Barométrica

**Função**: Medição de altitude e pressão atmosférica

**Especificações**:

- **Faixa de Pressão**: 300 - 1100 hPa
- **Resolução**: 0.01 hPa (1 Pa)
- **Precisão de Altitude**: ±1 m
- **Interface**: I2C
- **Endereço I2C**: 0x77 (padrão)

**Pinagem**:
| Pino | Função | Conexão |
|------|--------|---------|
| VCC | Alimentação | 3.3V |
| GND | Ground | GND |
| SCL | Clock I2C | GPIO 5 |
| SDA | Data I2C | GPIO 4 |

**Montagem**: Breakout GY-BMP280 solda direta no PCB

### 2. MPU6050 - IMU (Acelerômetro + Giroscópio)

**Função**: Medição de aceleração e velocidade angular

**Especificações**:

- **Acelerômetro**:
  - Faixa: ±2, ±4, ±8, ±16 g (configurável)
  - Resolução: 16 bits
  - Taxa de amostragem: até 8 kHz
- **Giroscópio**:
  - Faixa: ±250, ±500, ±1000, ±2000 °/s
  - Resolução: 16 bits
  - Taxa de amostragem: até 8 kHz
- **Interface**: I2C
- **Endereço I2C**: 0x68 (padrão)

**Pinagem**:
| Pino | Função | Conexão |
|------|--------|---------|
| VCC | Alimentação | 3.3V |
| GND | Ground | GND |
| SCL | Clock I2C | GPIO 5 |
| SDA | Data I2C | GPIO 4 |

**Montagem**: Breakout com solda direta no PCB

### 3. NEO-6M - Módulo GPS

**Função**: Geolocalização e sincronização de tempo

**Especificações**:

- **Sensibilidade**: -161 dBm
- **Tempo de aquisição**: Cold start ~27s, Hot start ~3s
- **Precisão**: ±2.5 m
- **Taxa de atualização**: até 10 Hz
- **Protocolo**: NMEA 0183
- **Interface**: UART serial

**Pinagem**:
| Pino | Função | Conexão |
|------|--------|---------|
| VCC | Alimentação | 3.3V |
| GND | Ground | GND |
| RX | Serial Receive | GPIO 21 (TX_GPS) |
| TX | Serial Transmit | GPIO 20 (RX_GPS) |

**Características**:

- Antena cerâmica integrada
- Frequência: L1 (1575.42 MHz)
- Constelações: GPS, GLONASS, Galileo, BeiDou

**Montagem**: Breakout com solda direta no PCB

### 4. RFM95W - Módulo LoRa

**Função**: Comunicação wireless de longo alcance com a base de lançamento

**Especificações**:

- **Frequência**: 868 MHz ISM
- **Alcance**: ~4 km (campo aberto, condições ideais)
- **Taxa de Dados**: 1.5 - 37.5 kbps
- **Potência de Transmissão**: +20 dBm (ajustável até +17 dBm)
- **Sensibilidade**: -139 dBm
- **Interface**: SPI

**Pinagem**:
| Pino | Função | Conexão |
|------|--------|---------|
| VCC | Alimentação | 3.3V |
| GND | Ground | GND |
| MOSI | SPI Data In | GPIO 6 |
| MISO | SPI Data Out | GPIO 8 |
| CLK | SPI Clock | GPIO 9 |
| NSS | SPI Chip Select | GPIO 7 (SS_LORA) |
| NRST | Reset | GPIO 1 (RST_LORA) |
| DIO0 | Interrupção TX/RX | GPIO 2 (DIO0_LORA) |

**Características**:

- Modulação: LoRa (Chirp Spread Spectrum)
- Largura de banda: 125, 250 ou 500 kHz
- Fator de espalhamento: 6-12 (trade-off range vs data rate)
- Código de sincronização: 0xF3

**Montagem**: Breakout com solda direta na PCB

## Atuadores

### Servo Motor - Controle do Paraquedas

**Função**: Abrir o mecanismo de liberação do paraquedas

**Especificações**:

- **Tipo**: Servo padrão 5V com engrenagem de metal
- **Torque**: ~4.8 kg/cm @ 5V
- **Velocidade**: ~0.23 s/60°
- **Precisão**: ±3°
- **Peso**: ~9 g

**Pinagem**:
| Pino | Função |
|------|--------|
| Marrom | Ground |
| Vermelho | +5V |
| Amarelo/Laranja | Sinal PWM (GPIO 3) |

**Montagem**: Servo orientado para baixo, acoplado mecanicamente ao sistema de paraquedas

**Posições**:

- 0° - Escotilha aberta (MAXPOS)
- 90° - Escotilha fechada (MINPOS)

### Buzzer - Sinalização Sonora

**Função**: Indicador de status de inicialização

**Especificações**:

- **Tipo**: Buzzer ativo (5V)
- **Frequência**: ~2.7 kHz
- **Volume**: ~85 dB
- **Corrente**: ~30 mA

**Pinagem**:

- Positivo → 5V (via resistor)
- Negativo → GPIO 0 (BUZZER_PIN)

**Sinais**:

- 3 bips curtos = Falha na inicialização
- Sequência de bips = Inicialização bem-sucedida

## Alimentação

### LM2596 - Conversor DC-DC Step Down

**Função**: Regular tensão das baterias para 3.3V/5V

**Especificações**:

- **Entrada**: 4.5 - 40V
- **Saída 1**: 3.3V @ 3A (para ESP32, sensores I2C)
- **Saída 2**: 5V @ 3A (para servo, buzzer, LoRa)
- **Frequência**: 150 kHz
- **Eficiência**: ~85%

### Baterias

**Configuração**: 3x 18650 em paralelo

- **Tensão Nominal**: 3.7V
- **Capacidade**: ~6000 mAh
- **Autonomia Estimada**: ~4-6 horas

## Placa de Circuito Impresso (PCB)

### Arquivos do KiCad

- **[CDB.kicad_pro](../hardware/CDB/CDB.kicad_pro)** - Arquivo de projeto
- **[CDB.kicad_sch](../hardware/CDB/CDB.kicad_sch)** - Esquema eletrônico
- **[CDB.kicad_pcb](../hardware/CDB/CDB.kicad_pcb)** - Layout da placa
- **[CDB.step](../hardware/CDB/CDB.step)** - Modelo 3D

### Conectores

- **J2**: Servo motor (3 pinos)
- **Conector de energia**: 2 pinos para 18650

### Dimensões Aproximadas

- **Comprimento**: 125 mm
- **Largura**: 50 mm
- **Altura** (com componentes): ~30 mm

## Lista de Componentes (BOM)

Veja [CDB_bom.md](../hardware/CDB_bom.md) para a lista completa.

### Componentes Eletrônicos Principais

| Label       | Componente         | Quantidade | Função               |
| ----------- | ------------------ | ---------- | -------------------- |
| A2          | ESP32-C3 SuperMini | 1          | Microcontrolador     |
| A3          | GY-BMP280 Breakout | 1          | Sensor de pressão    |
| Componente1 | MPU6050            | 1          | IMU                  |
| M3          | LM2596             | 1          | Regulador de tensão  |
| M4          | NEO-6M             | 1          | GPS                  |
| RFM95W1     | RFM95W LoRa        | 1          | Comunicação wireless |
| J1          | Buzzer 5V          | 1          | Sinalização          |
| J2          | Servo motor        | 1          | Controle paraquedas  |

### Componentes Passivos

- Capacitores: 100 nF, 10 μF, 220 μF
- Resistores: 1k, 10k, 100k Ohm
- Conectores: Pinos, JST

## Diagrama de Blocos

```
┌─────────────────────────────────────────────────────────────┐
│                     COMPUTADOR DE BORDO                      │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │ BMP280   │  │ MPU6050  │  │ NEO-6M   │  │ RFM95W   │    │
│  │Barômetro │  │   IMU    │  │   GPS    │  │  LoRa    │    │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘    │
│       │             │             │             │           │
│       └─────────────┼─────────────┼─────────────┤           │
│                     │    I2C      │    UART     │    SPI    │
│                     │             │             │           │
│              ┌──────┴─────────────┴─────┬───────┴────┐       │
│              │                          │            │       │
│         ┌────▼────────────────┐    ┌────▼──┐   ┌───▼────┐   │
│         │   ESP32-C3 SuperMini│    │GPS    │   │ LoRa   │   │
│         │                    │    │ UART  │   │SPI     │   │
│         └────┬────────────────┘    └───────┘   └────────┘   │
│              │                                              │
│        ┌─────┴──────────┐                                    │
│        │ GPIO PWM       │                                    │
│        │                │                                    │
│    ┌───▼───┐    ┌──────▼──┐                                  │
│    │ Servo │    │ Buzzer  │                                  │
│    │Motor  │    │         │                                  │
│    └───────┘    └─────────┘                                  │
│                                                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │          LM2596 DC-DC Converter                      │   │
│  │  Entrada: 7.4V (2x18650)  →  Saída: 3.3V e 5V     │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

## Códigos de Suporte/Teste

Os código de teste individuais estão em [../test/](../test/) para validar cada componente:

- **[test/basico/basico.ino](../test/basico/basico.ino)** - Diagnóstico geral
- **[test/buzzer/buzzer.ino](../test/buzzer/buzzer.ino)** - Teste do buzzer
- **[test/lora/lora.ino](../test/lora/lora.ino)** - Teste LoRa
- **[test/testeGPS/testeGPS.ino](../test/testeGPS/testeGPS.ino)** - Teste GPS
- **[test/servo/servo.ino](../test/servo/servo.ino)** - Teste servo
- **[test/LittleFS/LittleFS.ino](../test/LittleFS/LittleFS.ino)** - Teste armazenamento

## Testes Preliminares

- Verificar continuidade com multímetro
- Ligar alimentação e verificar LEDs/buzzer
- Usar código de teste básico

## Notas de Operação

- **Proteção**: Encapsulamento em cápsula isolante dentro do foguete
- **Choque**: Não foi medido a acelaração máxima suportada.

## Referências

- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
- [BMP280 Datasheet](https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280/)
- [MPU6050 Datasheet](https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050/)
- [NEO-6M GPS Module](https://www.u-blox.com/en/product/neo-6m-u-blox6)
- [RFM95W LoRa Module](https://www.semtech.com/products/wireless-rf/lora-transceivers/rfm95w)
