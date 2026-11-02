# 0001 — Fonte nativa do OBS em vez de Browser Source
- Status: Aceita
- Data: 2026-11-02

## Contexto
O gerador de caracteres feito em HTML no Browser Source (CEF) perde quadros de forma recorrente, mesmo com 60 fps configurados e aceleração de hardware. A animação roda fora do relógio de vídeo do OBS, então a sincronia depende do navegador.

## Decisão
Implementar o GC como **fonte nativa** (`obs_source_info`) de um plugin C++. A animação avança com o `seconds` do `video_tick` e é desenhada em `video_render`.

## Consequências
- (+) Animação determinística e sincronizada com o relógio do OBS.
- (+) Controle total do custo por quadro; sem processo/GPU extra do CEF.
- (−) Maior esforço de desenvolvimento (C++, build multiplataforma).
- (−) Perde-se o ecossistema web (CSS/JS); recursos precisam ser reimplementados de forma declarativa.

## Alternativas consideradas
- **Continuar com Browser Source** e otimizar (já tentado, sem sucesso).
- **Servidor externo + NDI/captura**: adiciona latência e complexidade.
- **Script Lua/Python no OBS**: sem acesso adequado a gráficos/UX.
