# Decisões de arquitetura (ADRs)

Cada ADR registra **uma** decisão: contexto, decisão, consequências e alternativas. ADRs aceitas só mudam por uma nova ADR que as **substitua** (status `Substituída por NNNN`).

## Formato

```markdown
# NNNN — Título
- Status: Proposta | Aceita | Substituída por NNNN | Rejeitada
- Data: AAAA-MM-DD

## Contexto
## Decisão
## Consequências
## Alternativas consideradas
```

## Índice

| Nº | Título | Status |
|---|---|---|
| [0001](0001-fonte-nativa-em-vez-de-browser-source.md) | Fonte nativa do OBS em vez de Browser Source | Aceita |
| [0002](0002-template-declarativo-em-json-e-svg.md) | Template declarativo em JSON + SVG | Aceita |
| [0003](0003-rasterizar-por-layer-e-animar-na-gpu.md) | Rasterizar por layer e animar na GPU | Aceita |
| [0004](0004-lunasvg-e-qt-para-texto.md) | lunasvg para SVG e Qt para texto | Aceita |
| [0005](0005-vendor-api-do-obs-websocket.md) | Controle remoto via vendor API do obs-websocket | Aceita |
| [0006](0006-clips-com-triggers.md) | Animações como clips com triggers | Aceita |
| [0007](0007-fila-de-comandos-e-engine-unico.md) | Fila de comandos e Engine como API única | Aceita |

Próximo número livre: **0008**.
