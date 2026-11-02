# 0007 — Fila de comandos e Engine como API única
- Status: Aceita
- Data: 2026-11-02

## Contexto
UI (Qt), WebSocket e hotkeys vivem em threads diferentes da thread gráfica, onde a cena é avaliada e desenhada. Acesso direto geraria *data races* e travamentos de render.

## Decisão
Toda ação externa passa pelo **Engine** (API única) e vira um `Command` em fila multi-produtor/um-consumidor, consumida no início do `video_tick`. A leitura de estado usa **snapshots imutáveis** publicados ao fim do tick. Eventos saem depois do tick por dispatch assíncrono.

## Consequências
- (+) Scene sem locks; modificações só na thread gráfica.
- (+) Painel e WebSocket se comportam de forma idêntica.
- (+) Ordem de comandos previsível; permite `take` (set + action no mesmo tick).
- (−) Efeitos são aplicados no próximo tick (latência ≤ 1 quadro).
- (−) Exige política para fila cheia e coalescência de `set` repetidos.

## Alternativas consideradas
- Mutex global na Scene (risco de contenção no render).
- Atores/mensagens por thread (excessivo para o escopo).
