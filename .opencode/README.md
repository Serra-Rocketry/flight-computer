# OpenCode Skills - Flight Computer Project

Esta pasta contém as skills especializadas e workflows para desenvolvimento do Flight Computer (Team #100 - Serra Rocketry).

## Estrutura

```
.opencode/
├── README.md                          # Este arquivo
├── TEAM.md                            # Visão geral da equipe e workflows
├── opencode.yaml                      # Configuração de skills (OpenCode)
└── skills/                            # Skills especializadas (diretórios)
    ├── embedded-architect/
    │   └── SKILL.md                  # Arquitetura de sistemas embarcados
    ├── firmware-developer/
    │   └── SKILL.md                  # Desenvolvimento de firmware ESP32
    ├── fsm-specialist/
    │   └── SKILL.md                  # Flight State Machine specialist
    ├── code-reviewer/
    │   └── SKILL.md                  # Code review (safety & quality)
    ├── test-engineer/
    │   └── SKILL.md                  # Testes (unit, HIL, simulation)
    └── documentation-specialist/
        └── SKILL.md                  # Documentação técnica
```

## Quick Start

### Para AI Coding Agents

Para **detalhes técnicos de implementação** (comandos de build, sintaxe de código, convenções de nomenclatura):
- Veja `../AGENTS.md` - Guia de desenvolvimento para agentes de IA

Este diretório (`.opencode/`) foca em **workflows organizacionais** e **skills de especialistas**.

---

### Para Desenvolvedores

#### Implementar uma Nova Feature
```
"Quero adicionar suporte ao sensor LSM6DS3"
```

O OpenCode irá automaticamente:
1. Analisar a arquitetura (Architect)
2. Implementar o código (Firmware Developer)
3. Integrar com FSM (FSM Specialist)
4. Criar testes (Test Engineer)
5. Documentar (Documentation Specialist)
6. Revisar código (Code Reviewer)

#### Resolver um Bug
```
"A FSM não está detectando o apogeu corretamente"
```

O sistema irá diagnosticar, corrigir, testar e revisar automaticamente.

#### Revisar um Pull Request
```
"Revise o PR #42 que adiciona o sensor BMP585"
```

Review completo multi-camada com todos os especialistas.

---

## Skills Disponíveis

### 🏗️ Embedded Systems Architect
**Expertise**: Arquitetura FreeRTOS, FSM design, otimização de performance

**Use quando**:
- Planejar arquitetura de novas features
- Validar decisões de design
- Otimizar uso de memória/CPU
- Revisar estrutura de tasks

**Arquivo**: `skills/embedded-architect/SKILL.md`

---

### 💻 Firmware Developer
**Expertise**: C/C++ embarcado, integração de sensores, protocolos I2C/SPI/UART

**Use quando**:
- Implementar drivers de sensores
- Adicionar módulos de comunicação
- Corrigir bugs de firmware
- Integrar novo hardware

**Arquivo**: `skills/firmware-developer/SKILL.md`

---

### 🎯 Flight State Machine Specialist
**Expertise**: FSM de voo, detecção de fases, validação com dados reais

**Use quando**:
- Projetar ou modificar a FSM
- Validar thresholds de detecção
- Debugar transições de estado
- Analisar telemetria de voo

**Arquivo**: `skills/fsm-specialist/SKILL.md`

---

### ✅ Code Reviewer (Safety & Quality)
**Expertise**: Código safety-critical, análise estática, padrões de qualidade

**Use quando**:
- Revisar Pull Requests
- Verificar lógica de paraquedas
- Identificar memory leaks
- Validar padrões de código

**Arquivo**: `skills/code-reviewer/SKILL.md`

---

### 🧪 Test Engineer
**Expertise**: Unit tests, HIL testing, simulação Python, análise de dados

**Use quando**:
- Criar testes unitários
- Validar com Python simulator
- Testar hardware (bench tests)
- Analisar dados de voo reais

**Arquivo**: `skills/test-engineer/SKILL.md`

---

### 📚 Documentation Specialist
**Expertise**: Doxygen, ADRs, diagramas Mermaid, documentação técnica

**Use quando**:
- Atualizar documentação
- Criar ADRs (Architecture Decision Records)
- Documentar APIs
- Gerar diagramas

**Arquivo**: `skills/documentation-specialist/SKILL.md`

---

## Workflows

### Workflow 1: Nova Feature (Completo)
```
Planning (Architect) 
  → Implementation (Developer) 
  → Testing (Test Engineer) 
  → Documentation (Doc Specialist) 
  → Review (Reviewer + Architect) 
  → Merge
```

### Workflow 2: Bug Fix (Rápido)
```
Diagnóstico (FSM Specialist / Developer) 
  → Correção (Developer) 
  → Validação (Test Engineer) 
  → Review (Reviewer) 
  → Merge
```

### Workflow 3: Refatoração (Arquitetural)
```
ADR (Architect + Doc Specialist) 
  → Aprovação 
  → Implementação (Developer) 
  → Testing (Test Engineer) 
  → Review (Multi-layer) 
  → Documentation 
  → Merge
```

**Detalhes completos**: Ver `TEAM.md`

---

## Checklist de Code Review

### Automated (CI/CD - Futuro)
- [ ] Build passa
- [ ] Unit tests passam
- [ ] Linting OK

### Manual (Multi-camada)

#### Code Reviewer (Safety & Quality)
- [ ] Functional correctness
- [ ] Safety (buffer overflows, null checks, NaN validation)
- [ ] Code quality (naming, docs, no magic numbers)
- [ ] Performance (no blocking, real-time constraints)

#### Architect
- [ ] Architecture compliance
- [ ] Integration with existing modules
- [ ] Memory/CPU budget

#### FSM Specialist (se aplicável)
- [ ] FSM logic validated
- [ ] Thresholds correct
- [ ] State guards present

#### Test Engineer
- [ ] Unit tests present (>80% coverage)
- [ ] Hardware test documented
- [ ] Validated with real data

#### Documentation Specialist
- [ ] Doxygen complete
- [ ] CHANGELOG updated
- [ ] README updated (if needed)

---

## Matriz RACI

Veja em `TEAM.md` a matriz completa de responsabilidades para cada tipo de atividade.

---

## Projeto Flight Computer - Contexto

### Tecnologias
- **Hardware**: ESP32-C3 / ESP32-S3
- **Linguagem**: C/C++ (Arduino/ESP-IDF)
- **RTOS**: FreeRTOS (v2.0)
- **Sensores**: BMP585, LSM6DS3, GPS NEO-8M, LoRa RFM95W
- **Testing**: Unity (unit tests), Python (simulation)

### Arquitetura (v2.0 - Planned)
```
Core 1: FlightControl Task (50Hz, priority 20)
        └─ FSM (7 estados validados)
        └─ Sensor updates
        └─ Parachute control

Core 0: Telemetry Task (5Hz, priority 5)
        Logger Task (low priority)
```

### FSM (7 Estados Validados)
```
IDLE → LIFTOFF → BURNOUT → APOGEE → FREEFALL → PARACHUTE → LANDED
```

Thresholds validados com **1.873 pontos** de telemetria real de voo.

### Documentos Chave
- `firmware/REFACTORING_PLAN.md` (1316 linhas) - Arquitetura completa v2.0
- `extras/FSM_tester/explicacao.md` (541 linhas) - FSM validation methodology
- `CONTRIBUTING.md` (165 linhas) - Padrões de código
- `docs/software.md` (295 linhas) - Arquitetura de software
- `docs/hardware.md` (333 linhas) - Especificações de hardware

---

## Como Contribuir

### Adicionar Nova Skill

1. Crie diretório em `skills/nome-skill/`
2. Crie arquivo `SKILL.md` dentro do diretório
3. Siga o template:

```markdown
---
name: nome-skill
description: Brief description of the skill
license: MIT
compatibility: opencode
---

## Role
[Descrição do papel]

## Expertise
- [Área 1]
- [Área 2]

## Responsibilities
1. [Responsabilidade 1]
2. [Responsabilidade 2]

## Project-Specific Context
[Contexto específico do Flight Computer]

## Design Patterns
[Padrões aplicáveis]

## Code Review Checklist
[Checklist para revisão]

## References
[Links para documentação]
```

4. Adicione referência em `TEAM.md`
5. Atualize este README

### Melhorar Skill Existente

1. Abra arquivo em `skills/nome-skill/SKILL.md`
2. Adicione exemplos, checklists, ou workflows
3. Mantenha consistência com outras skills
4. Documente mudanças

---

## Referências

### Internas
- `TEAM.md` - Visão geral completa da equipe
- `skills/*.md` - Documentação de cada skill

### Externas
- [NASA C Coding Standard](https://ntrs.nasa.gov/citations/20080039927)
- [MISRA C Guidelines](https://www.misra.org.uk/)
- [FreeRTOS Documentation](https://www.freertos.org/)
- [Adafruit Sensor Libraries](https://github.com/adafruit)

---

## Status

**Versão**: 1.0  
**Data**: 2026-04-01  
**Status**: ✅ Active  

### Skills Implementadas
- ✅ Embedded Systems Architect
- ✅ Firmware Developer
- ✅ Flight State Machine Specialist
- ✅ Code Reviewer (Safety & Quality)
- ✅ Test Engineer
- ✅ Documentation Specialist

### Próximos Passos
- [ ] Implementar integração com OpenCode
- [ ] Adicionar CI/CD automation
- [ ] Criar skill para Hardware Design (PCB/KiCAD)
- [ ] Adicionar skill para Telemetry Analysis

---

## Contato

**Team #100 - Serra Rocketry**  
**Projeto**: Flight Computer v2.0  
**Repository**: flight-computer/

Para questões sobre as skills, abra uma issue no repositório.
