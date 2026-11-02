# Depuração e desempenho

## Onde olhar

| O que | Onde |
|---|---|
| Logs do plugin | Ajuda → Arquivos de log do OBS; prefixo `[NaxGC]` |
| Quadros perdidos/atrasados | Janela **Estatísticas** do OBS (*Missed frames*, *Skipped frames*, *Frames rendering lag*) |
| Tempo de render da fonte | `obs_source_get_...` + medição interna (ver abaixo) |
| Erros de template | Painel do dock + log (`NXG-E…`/`NXG-W…`) |

## Opções de diagnóstico (previstas, M8)

- **Overlay de debug** (propriedade da fonte): desenha bounding boxes, `id`s das layers, clips ativos e tempo do tick.
- **Log verboso** (propriedade da fonte): eventos de clip, re-rasterizações e trocas de template.
- **Contadores internos**: tempo médio/máximo de `video_tick` e `video_render`, nº de texturas, re-rasterizações por segundo.

## Checklist de desempenho

- Nenhum I/O, parse ou alocação grande em `video_tick`/`video_render` (ver regras no [AGENTS.md](../../AGENTS.md)).
- Re-rasterização somente para layers sujas; texto digitado com *debounce* no painel.
- Texturas com bbox justo; sem texturas do tamanho do canvas para elementos pequenos.
- Máscaras com pool de texrender; sem criar/destruir por quadro.
- Layers invisíveis ou com opacidade 0 não são desenhadas.

## Reproduzindo problemas de fluidez

1. Cena controlada: apenas a fonte NaxGC, saída 1080p60.
2. Rode `show`/`hide` em loop (script via WebSocket) por 10 min.
3. Observe *Estatísticas* e compare com o Browser Source (mesma animação).
4. Se houver picos: ative contadores e cheque re-rasterizações e trocas de template.

## Problemas comuns

| Sintoma | Causas prováveis |
|---|---|
| Texto pisca ao trocar | re-rasterização + animação fora de ordem; use `take` (set + action no mesmo tick) |
| Borda/máscara desalinhada | ids/`x` diferentes entre clips; verifique `at` e `ease` dos triggers |
| Layer não aparece | `visible`/`opacity` base; estado `offair` não revertido; z-order do SVG |
| Cor não muda | `fill` aplicado em grupo sem formas diretas; use `rect`/`path` como layer |
| Borrão ao escalar | falta `rasterScale` |
| Fonte errada | `family` não declarada em `fonts` ou arquivo ausente (`NXG-W002`) |

## Ferramentas externas

- GPU: PIX/RenderDoc (Windows/Linux) para capturar quadros do OBS.
- CPU: Superluminal/Tracy (opcional) para *profiling* do tick.
