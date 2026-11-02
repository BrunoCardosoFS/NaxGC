# Visão geral da arquitetura

## Objetivo

Renderizar GCs animados no OBS com **tempo de quadro previsível**, sem depender de um navegador embutido, e permitir que designers criem GCs novos só editando arquivos (SVG + JSON).

## Princípios

1. **Relógio único**: a animação avança com o `seconds` do `video_tick` do OBS. Não há relógio paralelo.
2. **Rasterizar pouco, mover muito**: cada layer é rasterizada uma vez; movimento, escala, rotação e opacidade acontecem na GPU. Só se re-rasteriza quando o *conteúdo* muda.
3. **Declarativo**: o comportamento vem do manifest. O C++ não conhece nenhum template específico.
4. **Uma API, várias portas**: painel Qt, WebSocket e atalhos chamam a mesma API do Engine.
5. **Núcleo testável sem OBS**: manifest, cena e animator vivem no `core`, sem dependência de libobs/Qt.
6. **Falhar com elegância**: template inválido nunca derruba o OBS; mantém-se o último template válido (ou nada) e reporta-se o erro.

## Componentes

```
┌─────────────────────────────── OBS Studio ───────────────────────────────┐
│                                                                          │
│  Fonte NaxGC (obs_source_info)                                           │
│    ├─ video_tick(seconds) ──► Engine.tick(dt)                            │
│    └─ video_render()      ──► Renderer.draw(Scene)                       │
│                                                                          │
│  ┌───────────────────────── Engine (por instância) ───────────────────┐  │
│  │  CommandQueue ─► Scene (layers, props) ◄─ Animator (clips/triggers)│  │
│  │        ▲                    │                                      │  │
│  └────────┼────────────────────┼──────────────────────────────────────┘  │
│           │                    ▼                                         │
│   Dock Qt (panel)        Renderer (texturas, máscaras, matrizes)         │
│   Vendor WebSocket       Rasterizer (lunasvg + Qt texto)                 │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
        ▲
        │ carrega/valida
   Template (manifest.json + layout.svg + fonts/)
```

| Componente | Responsabilidade | Documento |
|---|---|---|
| **Template loader** | Lê o pacote, valida, produz o modelo tipado | [manifest/validation](../manifest/validation.md) |
| **Scene** | Layers, hierarquia, propriedades (valor base + valor animado) | [scene-and-properties](scene-and-properties.md) |
| **Animator** | Clips, tracks, triggers, easing, estados | [animation-engine](animation-engine.md) |
| **Rasterizer** | SVG → texturas por layer; texto Qt → texturas | [rendering-pipeline](rendering-pipeline.md) |
| **Renderer** | Desenha layers em z-order, máscaras via texrender | [rendering-pipeline](rendering-pipeline.md) |
| **Engine API** | `set`, `run_action`, `play`, `stop`, `load_template` | [api/engine-api](../api/engine-api.md) |
| **Painel (dock)** | UI gerada do `panel` | [control-panel](control-panel.md) |
| **Vendor WebSocket** | Requests/eventos `naxgc` | [api/websocket-vendor](../api/websocket-vendor.md) |

## Fluxo de dados

### Carregar template
1. A fonte recebe o caminho do template (settings) → pede ao `Engine.load_template`.
2. O loader lê e valida o pacote **fora** do caminho de render (thread de trabalho).
3. Em caso de sucesso, o resultado é entregue ao Engine e **trocado atomicamente** no próximo `video_tick`; texturas são criadas no contexto gráfico.
4. O painel é reconstruído a partir de `panel`; `initialState` é aplicado.

### Acionar uma ação (ex.: botão "Entrar")
1. UI/WebSocket cria `Command{RunAction, "show"}` e enfileira.
2. No próximo `video_tick`, o Engine drena a fila, executa os passos (`play main.in`, …).
3. O Animator avança os clips com `dt` e escreve valores animados nas propriedades.
4. Layers com conteúdo "sujo" (texto/cor alterados) são re-rasterizadas **antes** de avaliar o quadro.
5. `video_render` desenha a cena usando os valores do quadro.

### Mudar um texto
`Command{Set, "layers.linha1texto.text", "Maria"}` → propriedade marcada como suja → re-rasterização no próximo tick → textura nova usada no mesmo quadro em que o clip `in` (se acionado depois) começa. Veja a garantia de ordem em [threading-model](threading-model.md).

## Dependências externas

| Biblioteca | Uso |
|---|---|
| libobs / obs-frontend-api | fonte, gráficos, dock, hotkeys, settings (OBS 30.0+) |
| obs-websocket-api.h | vendor API |
| Qt 6 | dock, rasterização de texto, watcher de arquivos (somente Qt 6) |
| lunasvg | rasterização de nós SVG |
| nlohmann/json | parse do manifest (somente `core`) |
| doctest ou Catch2 | testes do `core` |

## Escopo e limites

- Instâncias múltiplas: cada fonte NaxGC tem seu próprio Engine; o painel escolhe a instância alvo.
- Sem rede, sem scripts embutidos no template (o template é dado, não código).
- SVG: suporta o subconjunto estático que o lunasvg desenha; **animação SMIL/CSS dentro do SVG é ignorada** — animação vem do manifest.

## Decisões de arquitetura

Veja o histórico de decisões em [ADRs](../decisions/README.md) e o plano de entregas no [ROADMAP](../ROADMAP.md).
