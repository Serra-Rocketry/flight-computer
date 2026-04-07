# Estrutura Modular do Firmware

Este documento descreve a organização modular do firmware do computador de voo.

## Arquivos Criados

### 1. `config.h`
Contém todas as definições de configuração, pinos e constantes do sistema:
- Definições de pinos (LoRa, Servo, Buzzer, GPS)
- Constantes do sistema (intervalos, thresholds)
- Configurações derede WiFi
- ID da equipe

### 2. `bmp280_sensor.h`
Módulo do sensor de pressão e altitude BMP280:
- `setupBMP()` - Inicializa o sensor e calibra altitude base
- `BMPData()` - Retorna string com dados de altitude, temperatura e pressão
- `checkHighest()` - Atualiza altitude máxima alcançada
- Variáveis globais: `base_pressure`, `previous_altitude`, `max_altitude`, `base_altitude`

### 3. `mpu6050_sensor.h`
Módulo do sensor IMU MPU6050 (acelerômetro e giroscópio):
- `setupMPU()` - Inicializa o sensor MPU6050
- `MPUData()` - Retorna string com dados de giroscópio e acelerômetro
- Variáveis globais: `acc`, `gyr`, `temp` (eventos de sensor)

### 4. `gps_module.h`
Módulo do GPS:
- `setupGPS()` - Inicializa comunicação serial com GPS
- `getGPSTimeString()` - Retorna horário GPS formatado para nome de arquivo
- `GPSData()` - Retorna string com horário, data, altitude, latitude, longitude e satélites
- Variável global: `GPS` (objeto TinyGPSPlus)

### 5. `lora_module.h`
Módulo de comunicação LoRa:
- `setupLoRa()` - Inicializa o módulo LoRa com frequência e palavra de sincronização
- `sendLoRa()` - Envia mensagem via LoRa

### 6. `filesystem_module.h`
Módulo de gerenciamento do sistema de arquivos LittleFS:
- `setupLittleFS()` - Inicializa o sistema de arquivos
- `writeFile()` - Cria arquivo com dados
- `appendFile()` - Adiciona dados a arquivo existente

### 7. `parachute_module.h`
Módulo de controle do paraquedas e servo motor:
- `setupServo()` - Inicializa servo motor na posição fechada
- `handleParachute()` - Gerencia lógica de abertura do paraquedas baseada em altitude e velocidade
- Variáveis globais: `ParachuteServo`, `parachute_deployed`

### 8. `buzzer_module.h`
Módulo de controle do buzzer:
- `buzzSignal()` - Gera sinais sonoros diferentes para cada situação:
  - "Alert" - Erro na inicialização
  - "Success" - Inicialização bem-sucedida
  - "Activated" - Paraquedas acionado
  - "Beep" - Operação padrão

### 9. `server_module.h`
Módulo do servidor web WiFi:
- `setServerRoutes()` - Configura rotas HTTP do servidor
- `setupServer()` - Inicializa access point WiFi e servidor web
- Rotas disponíveis:
  - `/` - Interface web principal
  - `/api/files` - Lista arquivos do sistema
  - `/api/file` - Recupera ou deleta arquivo específico

### 10. `telemetry_module.h`
Módulo de telemetria e logging:
- `getDataString()` - Agrega dados de todos os sensores
- `printBoth()` - Envia mensagem para Serial e LoRa
- `logData()` - Registra dados de telemetria no arquivo e envia via LoRa
- Variáveis globais: `packet_count`, `previous_millis`

### 11. `firmware.ino`
Arquivo principal simplificado:
- Inclui todos os módulos necessários
- `setup()` - Inicializa todos os sistemas
- `loop()` - Loop principal de leitura de sensores e controle

## Vantagens da Refatoração

1. **Manutenibilidade**: Cada sensor/função em arquivo separado facilita modificações
2. **Organização**: Código estruturado por funcionalidade
3. **Reutilização**: Módulos podem ser facilmente reutilizados em outros projetos
4. **Legibilidade**: Arquivo principal reduzido de 584 para ~80 linhas
5. **Depuração**: Mais fácil identificar e corrigir problemas em módulos específicos
6. **Desenvolvimento em equipe**: Membros podem trabalhar em módulos diferentes simultaneamente

## Como Modificar

### Para adicionar novo sensor:
1. Crie arquivo `novo_sensor.h`
2. Implemente funções de inicialização e leitura
3. Inclua no `firmware.ino`
4. Chame funções de setup e loop conforme necessário

### Para alterar configurações:
- Edite `config.h` para pins, constantes e thresholds

### Para modificar lógica de telemetria:
- Edite `telemetry_module.h` para formato de dados

### Para ajustar servidor web:
- Edite `server_module.h` para rotas e funcionalidades

## Compilação

O Arduino IDE automaticamente compila todos os arquivos `.h` no mesmo diretório que o `.ino`.
Nenhuma configuração adicional é necessária.
