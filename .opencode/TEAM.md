# Equipe de Desenvolvimento - Flight Computer Project

Este documento define a estrutura da equipe de desenvolvimento e code review para o projeto Flight Computer (Team #100), incluindo skills especializadas, agentes, e workflows.

## Visão Geral da Equipe

A equipe é composta por 6 especialistas que cobrem todas as áreas críticas do desenvolvimento de sistemas embarcados para aplicações aeroespaciais:

```
┌─────────────────────────────────────────────────────────────┐
│                    EQUIPE DE DESENVOLVIMENTO                 │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  🏗️  Embedded Systems Architect                             │
│     └─ Arquitetura, FreeRTOS, design de FSM                 │
│                                                              │
│  💻 Firmware Developer                                       │
│     └─ Implementação C/C++, integração de sensores          │
│                                                              │
│  🎯 Flight State Machine Specialist                          │
│     └─ FSM, detecção de fases de voo, validação             │
│                                                              │
│  ✅ Code Reviewer (Safety & Quality)                         │
│     └─ Revisão de segurança, qualidade, padrões             │
│                                                              │
│  🧪 Test Engineer                                            │
│     └─ Testes unitários, HIL, simulação Python              │
│                                                              │
│  📚 Documentation Specialist                                 │
│     └─ Documentação técnica, ADRs, Doxygen                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Skills Disponíveis

### 1. Embedded Systems Architect
**Arquivo**: `.opencode/skills/embedded-architect.md`

**Quando usar**:
- Planejamento de arquitetura (FreeRTOS tasks, estruturas de dados)
- Decisões de design (OOP vs procedural, sensor abstraction)
- Otimização de performance e memória
- Review de PRs para validação arquitetural

**Comandos típicos**:
- "Revise a arquitetura desta implementação FreeRTOS"
- "Como devo estruturar as tasks para o flight control?"
- "Valide se esta implementação atende os requisitos de tempo real"

---

### 2. Firmware Developer
**Arquivo**: `.opencode/skills/firmware-developer.md`

**Quando usar**:
- Implementação de novos sensores (ISensor interface)
- Módulos de comunicação (LoRa, GPS, WiFi)
- Integração de hardware (I2C, SPI, UART)
- Resolução de bugs de firmware

**Comandos típicos**:
- "Implemente o driver para o sensor BMP585"
- "Adicione suporte ao novo módulo GPS NEO-8M"
- "Corrija o bug de comunicação I2C"

---

### 3. Flight State Machine Specialist
**Arquivo**: `.opencode/skills/fsm-specialist.md`

**Quando usar**:
- Design da FSM de voo (estados, transições, guards)
- Validação de thresholds com dados reais
- Implementação de funções de detecção
- Debug de transições incorretas de estado

**Comandos típicos**:
- "Valide os thresholds da FSM com os dados de voo reais"
- "Implemente a função de detecção de apogeu"
- "Por que a FSM está presa no estado BURNOUT?"

---

### 4. Code Reviewer (Safety & Quality)
**Arquivo**: `.opencode/skills/code-reviewer.md`

**Quando usar**:
- Review de Pull Requests
- Análise de segurança (parachute deployment logic)
- Verificação de padrões de código
- Identificação de memory leaks e race conditions

**Comandos típicos**:
- "Revise este PR focando em safety-critical issues"
- "Analise esta implementação da lógica de paraquedas"
- "Verifique se há race conditions nesta task FreeRTOS"

---

### 5. Test Engineer
**Arquivo**: `.opencode/skills/test-engineer.md`

**Quando usar**:
- Criação de testes unitários (Unity framework)
- Testes de hardware (Arduino test firmware)
- Validação com Python simulator
- Análise de dados de voo

**Comandos típicos**:
- "Crie testes unitários para o módulo BMP585Sensor"
- "Valide a FSM com os dados reais de voo"
- "Execute os testes de hardware para o módulo LoRa"

---

### 6. Documentation Specialist
**Arquivo**: `.opencode/skills/documentation-specialist.md`

**Quando usar**:
- Atualização de documentação (README, CHANGELOG)
- Criação de Architecture Decision Records (ADRs)
- Documentação de APIs (Doxygen)
- Diagramas e flowcharts (Mermaid)

**Comandos típicos**:
- "Atualize o README com as novas features"
- "Crie um ADR para a decisão de migrar para FreeRTOS"
- "Documente a API do módulo FSM"

---

## Workflow de Desenvolvimento

### 1. Nova Feature (Ciclo Completo)

```
┌─────────────────────────────────────────────────────────────┐
│ 1. PLANNING                                                  │
└─────────────────────────────────────────────────────────────┘
    ↓
    👤 Usuário: "Adicionar suporte ao sensor BMP585"
    ↓
    🏗️ Architect:
       - Analisa impacto na arquitetura
       - Define estrutura de classes (ISensor)
       - Especifica integração com FSM
       - Define budget de memória/performance
    ↓
┌─────────────────────────────────────────────────────────────┐
│ 2. IMPLEMENTATION                                            │
└─────────────────────────────────────────────────────────────┘
    ↓
    💻 Firmware Developer:
       - Cria sensors/BMP585Sensor.h/.cpp
       - Implementa ISensor interface
       - Adiciona validação de dados (NaN, range)
       - Implementa cálculo de velocidade vertical
    ↓
┌─────────────────────────────────────────────────────────────┐
│ 3. TESTING                                                   │
└─────────────────────────────────────────────────────────────┘
    ↓
    🧪 Test Engineer:
       - Cria unit tests (test/test_native/test_bmp585.cpp)
       - Cria hardware test (test/test_hardware/test_bmp585.ino)
       - Valida com dados reais
       - Verifica edge cases (sensor disconnect, NaN)
    ↓
┌─────────────────────────────────────────────────────────────┐
│ 4. DOCUMENTATION                                             │
└─────────────────────────────────────────────────────────────┘
    ↓
    📚 Documentation Specialist:
       - Adiciona Doxygen comments
       - Atualiza README.md
       - Adiciona entrada no CHANGELOG.md
       - Cria usage example
    ↓
┌─────────────────────────────────────────────────────────────┐
│ 5. CODE REVIEW                                               │
└─────────────────────────────────────────────────────────────┘
    ↓
    ✅ Code Reviewer:
       - Verifica safety issues
       - Valida padrões de código
       - Checa performance
       - Verifica documentação
    ↓
    🏗️ Architect:
       - Valida arquitetura
       - Verifica integração com sistema
    ↓
┌─────────────────────────────────────────────────────────────┐
│ 6. MERGE & RELEASE                                           │
└─────────────────────────────────────────────────────────────┘
    ↓
    ✅ Aprovado → Merge para branch dev-2026
```

---

### 2. Bug Fix (Ciclo Rápido)

```
👤 Usuário: "A FSM está presa no estado BURNOUT"
    ↓
🎯 FSM Specialist:
   - Analisa logs de telemetria
   - Identifica threshold incorreto
   - Propõe correção
    ↓
💻 Firmware Developer:
   - Implementa correção
    ↓
🧪 Test Engineer:
   - Valida correção com Python simulator
   - Testa com dados reais
    ↓
✅ Code Reviewer:
   - Review rápido (safety focus)
    ↓
📚 Documentation Specialist:
   - Atualiza CHANGELOG (Fixed section)
    ↓
✅ Merge e release patch
```

---

### 3. Refatoração (Arquitetural)

```
👤 Usuário: "Migrar de single-thread para FreeRTOS"
    ↓
🏗️ Architect:
   - Cria Architecture Decision Record (ADR)
   - Define task structure (FlightControl, Telemetry, Logger)
   - Especifica prioridades e core affinity
   - Documenta em REFACTORING_PLAN.md
    ↓
👤 Aprovação do time
    ↓
💻 Firmware Developer:
   - Implementa tasks FreeRTOS
   - Adiciona queues e mutexes
   - Migra código para nova estrutura
    ↓
🧪 Test Engineer:
   - Testa timing de tasks (execution time)
   - Valida comportamento multi-core
   - Testa synchronization (race conditions)
    ↓
✅ Code Reviewer:
   - Review profundo (safety + performance)
   - Verifica mutex usage
   - Valida real-time constraints
    ↓
🏗️ Architect:
   - Valida implementação vs ADR
    ↓
📚 Documentation Specialist:
   - Finaliza ADR
   - Atualiza docs/software.md
   - Cria migration guide
    ↓
✅ Merge após validação completa
```

---

## Matriz de Responsabilidades (RACI)

| Atividade | Architect | Developer | FSM Spec | Reviewer | Tester | Doc Spec |
|-----------|:---------:|:---------:|:--------:|:--------:|:------:|:--------:|
| **Planning** |
| Definir arquitetura | **R** | C | C | I | I | I |
| Estimar recursos | **A** | C | I | I | I | I |
| Definir thresholds FSM | C | I | **R** | I | **A** | I |
| **Implementation** |
| Escrever código | I | **R/A** | C | I | I | I |
| Implementar FSM | C | **R** | **A** | I | I | I |
| Integrar sensores | C | **R/A** | I | I | I | I |
| **Testing** |
| Unit tests | I | C | I | I | **R/A** | I |
| Hardware tests | I | C | C | I | **R/A** | I |
| FSM validation | C | I | **A** | I | **R** | I |
| **Review** |
| Code review | **A** | I | I | **R** | C | I |
| Safety review | **A** | I | C | **R** | I | I |
| Architecture review | **R/A** | I | I | C | I | I |
| **Documentation** |
| Doxygen comments | I | **R** | **R** | C | I | **A** |
| README/CHANGELOG | C | I | I | I | I | **R/A** |
| ADRs | **A** | I | I | I | I | **R** |

**Legenda**:
- **R** = Responsible (executa)
- **A** = Accountable (responsável final)
- **C** = Consulted (consultado)
- **I** = Informed (informado)

---

## Comandos por Tipo de Tarefa

### Implementar Nova Feature
```bash
# 1. Planning
"@embedded-architect Analise o impacto de adicionar o sensor BMP585 na arquitetura FreeRTOS atual"

# 2. Implementation
"@firmware-developer Implemente o driver BMP585Sensor seguindo a interface ISensor"

# 3. FSM Integration
"@fsm-specialist Integre o BMP585 à FSM, substituindo as referências ao BMP280"

# 4. Testing
"@test-engineer Crie testes unitários e de hardware para o BMP585Sensor"

# 5. Documentation
"@documentation-specialist Documente o BMP585Sensor com Doxygen e atualize o README"

# 6. Review
"@code-reviewer Revise a implementação do BMP585Sensor focando em safety e qualidade"
```

### Resolver Bug
```bash
# 1. Diagnóstico
"@fsm-specialist Por que a FSM não detecta o apogeu corretamente?"

# 2. Correção
"@firmware-developer Corrija a função detectApogee() baseado na análise do FSM Specialist"

# 3. Validação
"@test-engineer Valide a correção usando o simulador Python e dados reais"

# 4. Review
"@code-reviewer Revise a correção rapidamente, foco em safety"

# 5. Doc
"@documentation-specialist Atualize o CHANGELOG com o bug fix"
```

### Code Review de PR
```bash
# Review multi-especialista
"@code-reviewer Revise o PR #42 focando em safety-critical issues"
"@embedded-architect Valide se o PR #42 segue a arquitetura definida"
"@test-engineer Verifique se o PR #42 inclui testes adequados"
"@documentation-specialist Verifique se o PR #42 tem documentação completa"
```

---

## Processo de Code Review

### Checklist de Review (Multi-camada)

#### Camada 1: Automated Checks (CI/CD - Futuro)
```bash
✓ Build passa (pio run)
✓ Unit tests passam (pio test -e native)
✓ Linting OK (clang-tidy, cppcheck)
✓ Documentação gerada (doxygen)
```

#### Camada 2: Code Reviewer (Safety & Quality)
```
✅ Functional Correctness
   ✓ Implementa requisitos
   ✓ Edge cases tratados
   ✓ Validação de inputs
   
✅ Safety (Critical)
   ✓ Sem buffer overflows
   ✓ Sem null pointer dereferences
   ✓ Validação de dados de sensores (NaN)
   ✓ Lógica de paraquedas segura
   
✅ Code Quality
   ✓ Segue naming conventions
   ✓ Doxygen comments presentes
   ✓ Sem magic numbers
   
✅ Performance
   ✓ Sem dynamic allocation em critical paths
   ✓ Tasks atendem timing requirements
```

#### Camada 3: Architect (Architecture Compliance)
```
✅ Architecture
   ✓ Segue ISensor interface (se aplicável)
   ✓ Task priorities corretas
   ✓ Mutexes usados corretamente
   ✓ Memory budget respeitado
   
✅ Integration
   ✓ Integra com módulos existentes
   ✓ Não quebra APIs públicas
```

#### Camada 4: FSM Specialist (se aplicável)
```
✅ FSM Logic
   ✓ Thresholds validados com dados reais
   ✓ Transições com guards apropriados
   ✓ Timeouts para evitar stuck states
```

#### Camada 5: Test Engineer
```
✅ Testing
   ✓ Unit tests presentes (>80% coverage)
   ✓ Hardware test procedure documentado
   ✓ Validado com dados reais (se FSM)
```

#### Camada 6: Documentation Specialist
```
✅ Documentation
   ✓ Doxygen comments completos
   ✓ CHANGELOG.md atualizado
   ✓ README atualizado (se necessário)
   ✓ Usage examples fornecidos
```

### Severidades de Issues

#### 🔴 Critical (Blocking)
- Safety issues (parachute deployment)
- Memory corruption
- Race conditions
- Real-time violations

**Ação**: Block merge

#### 🟠 Major (Should Fix)
- Missing error handling
- Resource leaks
- Performance issues (>20% over budget)

**Ação**: Request changes

#### 🟡 Minor (Nice to Have)
- Code style
- Missing docs
- Refactoring suggestions

**Ação**: Approve with comments

---

## Como Usar Este Sistema

### Para Usuários/Desenvolvedores

#### 1. Ao Iniciar uma Nova Feature
```
Você: "Quero adicionar suporte ao sensor LSM6DS3 (IMU)"

OpenCode irá:
1. Invocar @embedded-architect para análise de arquitetura
2. Invocar @firmware-developer para implementação
3. Invocar @fsm-specialist para integração com FSM
4. Invocar @test-engineer para testes
5. Invocar @documentation-specialist para docs
6. Invocar @code-reviewer para review final
```

#### 2. Ao Encontrar um Bug
```
Você: "A FSM não está detectando o burnout corretamente"

OpenCode irá:
1. Invocar @fsm-specialist para diagnóstico
2. Invocar @firmware-developer para correção
3. Invocar @test-engineer para validação
4. Invocar @code-reviewer para review rápido
```

#### 3. Ao Fazer um Pull Request
```
Você: "Revise meu PR que adiciona o sensor BMP585"

OpenCode irá:
1. Invocar @code-reviewer para safety/quality review
2. Invocar @embedded-architect para architecture review
3. Invocar @test-engineer para test coverage review
4. Invocar @documentation-specialist para doc review
5. Gerar relatório consolidado de review
```

---

## Referências

### Arquivos de Skills
- `.opencode/skills/embedded-architect.md`
- `.opencode/skills/firmware-developer.md`
- `.opencode/skills/fsm-specialist.md`
- `.opencode/skills/code-reviewer.md`
- `.opencode/skills/test-engineer.md`
- `.opencode/skills/documentation-specialist.md`

### Documentação do Projeto
- `firmware/REFACTORING_PLAN.md` - Plano completo v2.0
- `CONTRIBUTING.md` - Guia de contribuição
- `docs/software.md` - Arquitetura de software
- `docs/hardware.md` - Especificações de hardware

### Padrões e Guidelines
- NASA C Coding Standard (referência para safety-critical)
- MISRA C Guidelines (automotive, aplicável a aerospace)
- Keep a Changelog (formato de CHANGELOG.md)
- Semantic Versioning (versionamento)

---

## Próximos Passos

### Implementação das Skills no OpenCode
As skills foram criadas como arquivos markdown em `.opencode/skills/`. Para ativá-las:

1. **Opção 1**: Integração futura do OpenCode com skills markdown
2. **Opção 2**: Converter para formato de agentes especializados
3. **Opção 3**: Usar como referência em prompts customizados

### Automação (Futuro)
- GitHub Actions para CI/CD
- Pre-commit hooks para linting
- Automated testing em PRs
- Automated documentation generation

---

**Versão**: 1.0  
**Data**: 2026-04-01  
**Autor**: Team #100 - Serra Rocketry  
**Status**: Active
