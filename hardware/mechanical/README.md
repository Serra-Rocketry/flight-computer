# Mechanical Assembly

Arquivos de modelagem 3D e montagem mecanica do satelite.

## Estrutura de pastas

```
mechanical/
├── assembly/        — montagens gerais (.SLDASM)
├── chassis/         — corpo principal (.SLDPRT)
├── electronics/     — PCBs, bateria, suportes de montagem (.SLDPRT)
├── standoff/        — parafusos, porcas, separadores (.SLDPRT)
├── srab/
│   ├── assembly/    — montagens do SRAB (.SLDASM)
│   └── parts/       — pecas do SRAB (.SLDPRT)
├── README.md
```

## Convencoes

- `.SLDPRT` — partes nativas SolidWorks
- `.SLDASM` — montagens nativas SolidWorks
- `.STEP` — formato neutro para compartilhamento (AP214)
- `.STL` — para impressao 3D / visualizacao rapida
- Nomes em lowercase, snake_case, sem espacos; sufixo `_v2`, `_v3` para revisoes

## Como contribuir

1. Salve `.SLDPRT`/`.SLDASM` nativo + `.STEP` neutro + `.STL` (se impressivel)
2. Mantenha o STEP neutro sempre atualizado como fonte de verdade para corte/CAM
3. Adicione nota aqui quando subir nova revisao
