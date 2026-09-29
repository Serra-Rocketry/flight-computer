# Análise de Consistência: AGENTS.md vs .opencode/

## Resumo Executivo

✅ **Status Geral**: Os documentos são **complementares e consistentes**

- **AGENTS.md**: Guia técnico para agentes (comandos, sintaxe, padrões de código)
- **.opencode/**: Framework organizacional (skills, workflows, responsabilidades)

---

## Consistências Encontradas ✅

### 1. Naming Conventions - 100% Alinhadas
Ambos documentos especificam:
- **Global variables**: `snake_case`
- **Local variables**: `camelCase`
- **Functions**: `camelCase`
- **Constants**: `UPPER_CASE`
- **Classes**: `PascalCase`
- **Private members**: `_camelCase`

### 2. Arquitetura v2.0 - Totalmente Alinhada
Ambos mencionam:
- FreeRTOS com 3 tasks
- FlightControl (50Hz, priority 20, Core 1)
- Telemetry (5Hz, priority 5, Core 0)
- Logger (low priority, Core 0)
- FSM de 7 estados validada

### 3. Documentos de Referência - Idênticos
Ambos referenciam:
- `firmware/REFACTORING_PLAN.md` (1.316 linhas)
- `extras/FSM_tester/FSM_Tester.py`
- `extras/FSM_tester/explicacao.md` (541 linhas)
- `test/FSM/FSM.ino`
- `CONTRIBUTING.md`

### 4. Safety-Critical Code - Mesmos Princípios
Ambos enfatizam:
- Validação de dados de sensores (NaN, range)
- Multi-condition parachute deployment
- Estado da FSM antes de ações críticas

---

## Diferenças (Por Design) ✅

Estas diferenças são **intencionais e benéficas**:

| Aspecto | AGENTS.md | .opencode/ |
|---------|-----------|------------|
| **Foco** | Técnico/operacional | Organizacional/estratégico |
| **Audiência** | AI coding agents | Equipe de desenvolvimento |
| **Conteúdo** | Comandos, sintaxe, exemplos | Skills, workflows, RACI |
| **Nível** | Como fazer (HOW) | Quem faz o quê (WHO/WHAT) |
| **Tamanho** | 300 linhas (conciso) | 542+ linhas (detalhado) |

**Conclusão**: Esta divisão de responsabilidades é **ideal**.

---

## Oportunidades de Integração 🔗

### 1. Cross-Reference entre Documentos

#### No AGENTS.md, adicionar seção:
```markdown
## Related Documentation

This guide focuses on **technical implementation** (commands, syntax, patterns).

For **organizational workflows** and **skill-based team structure**, see:
- `.opencode/README.md` - Skills overview and quick start
- `.opencode/TEAM.md` - Complete team structure and workflows
- `.opencode/skills/*.md` - Detailed specialist guides
```

#### No .opencode/README.md, adicionar seção:
```markdown
## For AI Coding Agents

This directory contains **organizational workflows** and **specialist skills**.

For **technical implementation details** (build commands, code syntax, naming conventions), see:
- `../AGENTS.md` - Development guide for AI agents
```

### 2. Unificar Seção de Testes

**Atualmente**:
- AGENTS.md: Tem comandos específicos de teste
- .opencode/skills/test-engineer.md: Tem metodologia detalhada

**Sugestão**: Manter separado, mas adicionar cross-reference

### 3. Consistência de Idioma (Opcional)

**Status atual**:
- AGENTS.md: Inglês (títulos) + Português (comentários inline)
- .opencode/: Português (completo)

**Sugestão**: Manter como está (reflete uso real da equipe)

---

## Inconsistências Menores (Corrigir) ⚠️

### 1. Número de Especialistas na Equipe

**.opencode/TEAM.md linha 7**:
```
A equipe é composta por 5 especialistas...
```

Mas lista **6 especialistas**:
1. Embedded Systems Architect
2. Firmware Developer
3. Flight State Machine Specialist
4. Code Reviewer
5. Test Engineer
6. Documentation Specialist

**Correção**: Alterar "5" para "6"

### 2. Referência a "AGENTS.md" no .opencode/

Não há menção ao AGENTS.md nos arquivos .opencode/

**Correção**: Adicionar cross-reference

---

## Recomendações de Melhoria

### Alta Prioridade

1. **Corrigir contagem de especialistas** (5 → 6) em `.opencode/TEAM.md`
2. **Adicionar cross-references** entre AGENTS.md e .opencode/
3. **Adicionar link para AGENTS.md** no .opencode/README.md

### Média Prioridade

4. **Sincronizar exemplos de código** (usar os mesmos snippets nos dois lugares)
5. **Criar índice integrado** que mostre onde encontrar cada informação

### Baixa Prioridade

6. Considerar traduzir AGENTS.md para inglês completo (padrão IEEE/NASA)
7. Adicionar badges de status nos READMEs

---

## Ações Sugeridas

### Ação 1: Corrigir .opencode/TEAM.md
```diff
- A equipe é composta por 5 especialistas que cobrem todas as áreas
+ A equipe é composta por 6 especialistas que cobrem todas as áreas
```

### Ação 2: Adicionar seção no final do AGENTS.md
```markdown
---

## Related Documentation

### Organizational Workflows & Team Structure
- `.opencode/README.md` - Skills overview and workflows
- `.opencode/TEAM.md` - Complete team structure and RACI matrix
- `.opencode/skills/` - Detailed specialist guides

### Project Documentation
- `CONTRIBUTING.md` - Contribution guidelines and PR process
- `firmware/REFACTORING_PLAN.md` - v2.0 architecture specification
- `docs/software.md` - Software architecture overview
```

### Ação 3: Adicionar seção no .opencode/README.md (após linha 20)
```markdown
---

## For AI Coding Agents

For **technical implementation details** (build commands, code syntax, naming conventions):
- See `../AGENTS.md` - Development guide for AI agents

This directory (`.opencode/`) focuses on **organizational workflows** and **specialist skills**.

---
```

---

## Conclusão

### Status: ✅ **BOM - Pequenas Melhorias Recomendadas**

**Pontos Fortes**:
- Naming conventions 100% consistentes
- Arquitetura alinhada
- Complementaridade clara entre os documentos
- Sem conflitos de informação

**Pontos de Melhoria**:
- Corrigir contagem de especialistas (typo)
- Adicionar cross-references
- Melhor integração entre os documentos

**Prioridade**: Média (não bloqueia uso atual, mas melhora usabilidade)

---

**Análise realizada em**: 2026-04-01  
**Documentos analisados**: AGENTS.md, .opencode/README.md, .opencode/TEAM.md  
**Especialistas validados**: 6 (Architect, Developer, FSM, Reviewer, Tester, Doc)
