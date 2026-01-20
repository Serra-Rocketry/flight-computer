# Aviônica - Computador de Bordo

Computador de bordo (CDB) para o foguete SR21000 da equipe de foguetemodelismo Serra Rocketry. Sistema embarcado que monitora altitude, localização GPS e controla a abertura automática do paraquedas durante o voo.

## Visão Geral

- Monitoramento em tempo real de altitude, velocidade e posição GPS
- Comunicação via LoRa com a base operacional
- Armazenamento de telemetria em formato CSV
- Controle automático do servo para abertura do paraquedas
- Interface web para acesso aos dados armazenados

## Estrutura do Projeto

```
firmware/       Código principal do computador de bordo
test/           Testes unitários de componentes individuais
extras/         Códigos de suporte e experimentação
hardware/       Esquemas, PCB e lista de componentes
docs/           Documentação detalhada (software e hardware)
```

## Documentação

- **[software.md](docs/software.md)** - Detalhes do firmware, funções e bibliotecas
- **[hardware.md](docs/hardware.md)** - Componentes, pinagem e especificações
- **[flowchart.md](docs/flowchart.md)** - Fluxograma de execução do sistema

## Começando

1. Abrir [firmware/firmware.ino](firmware/firmware.ino) na Arduino IDE
2. Instalar bibliotecas necessárias
3. Configurar porta COM e placa ESP32
4. Upload para o microcontrolador

## Licença

Projeto da Serra Rocketry - Equipe de Foguetemodelismo do IPRJ/UERJ
