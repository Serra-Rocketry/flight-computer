```mermaid
graph TD
    %% Estilos
    classDef error fill:#f96,stroke:#333,stroke-width:2px;
    classDef success fill:#9f6,stroke:#333,stroke-width:2px;
    classDef logic fill:#add8e6,stroke:#333,stroke-width:2px;
    classDef io fill:#fffacd,stroke:#333,stroke-width:2px;

    Start([Início - Power On]) --> InitBasic[Iniciar Serial, Wire e Servo]
    InitBasic --> GPSWait[Aguardar GPS: 3 segundos]

    %% Bloco de Definição de Arquivo
    GPSWait --> GPSTime{GPS tem Hora?}
    GPSTime -- Sim --> NameTime[Nome do arquivo: Hora GPS]
    GPSTime -- Não --> NameMillis[Nome do arquivo: Millis do Boot]
    NameTime --> SetupFS
    NameMillis --> SetupFS

    %% Bloco de Sistema de Arquivos
    SetupFS{Iniciar LittleFS +<br>Criar Cabeçalho CSV}
    SetupFS -- Falha --> FSError[Buzzer: Alerta<br>Reiniciar ESP32]:::error
    SetupFS -- Sucesso --> StartWiFi[Iniciar WiFi AP +<br>Configurar Rotas Web Server]

    %% Bloco de Sensores e LoRa
    StartWiFi --> InitMods{Iniciar BMP280,<br>MPU6050 e LoRa}
    InitMods -- Falha --> ModError[Buzzer: Alerta<br>Continuar mesmo assim]:::error
    InitMods -- Sucesso --> ModOk[Buzzer: Sucesso]:::success

    %% Transição para o Loop
    ModError --> LoopStart((Iniciar Loop))
    ModOk --> LoopStart

    %% Processos Assíncronos (Servidor)
    subgraph AsyncServer [Servidor Web Assíncrono]
        direction TB
        Listen[Escutar Porta 80]
        Route1[GET /: Home]
        Route2[GET /api/files: Listar JSON]
        Route3[GET /api/file: Download]
        Route4[DELETE /api/file: Apagar]
        Listen -.-> Route1 & Route2 & Route3 & Route4
    end
    StartWiFi -.-> AsyncServer

    %% Loop Principal
    LoopStart --> TimerCheck{Passou 200ms?}
    TimerCheck -- Não --> LoopStart
    TimerCheck -- Sim --> ReadSensors[Ler Altitude BMP<br>Calcular Velocidade]

    %% Coleta e Log
    ReadSensors --> GetData[Coletar Dados:<br>GPS, BMP, MPU]:::io
    GetData --> BuildStr[Montar String CSV]
    BuildStr --> LogDisk[Salvar no LittleFS]:::io
    LogDisk --> SendLoRa[Enviar via LoRa]:::io

    %% Lógica de Apogeu
    SendLoRa --> CheckApogee{Alt > Max Alt?}
    CheckApogee -- Sim --> UpdateMax[Atualizar Max Altitude]:::logic
    CheckApogee -- Não --> CheckParachute

    %% Lógica do Paraquedas
    UpdateMax --> CheckParachute{Paraquedas<br>Acionado?}
    CheckParachute -- Sim --> Beep[Buzzer: Beep Ativado]
    CheckParachute -- Não --> DropCond{Caiu 10m do Apogeu?}

    DropCond -- Não --> UpdateTimer
    DropCond -- Sim --> SafeCond{Alt < 200m OU<br>Vel > 5m/s?}

    SafeCond -- Não --> UpdateTimer
    SafeCond -- Sim --> Deploy[Mover Servo 90° -> 0°<br>Flag = true]:::success
    Deploy --> UpdateTimer[Atualizar Tempo Anterior]
    UpdateTimer --> LoopStart

```
