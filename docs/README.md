# Documentação do NaxGC

Ponto de entrada. Se é a primeira vez: leia [Visão geral da arquitetura](architecture/overview.md) e depois [Especificação do manifest](manifest/specification.md).

## Mapa

| Pasta / arquivo | Conteúdo |
|---|---|
| [`ROADMAP.md`](ROADMAP.md) | Marcos, entregas e critérios de aceite |
| [`GLOSSARY.md`](GLOSSARY.md) | Termos usados no projeto |
| [`architecture/`](architecture/) | Como o plugin é organizado por dentro |
| [`manifest/`](manifest/) | Contrato público: formato do template (`manifest.json`) |
| [`api/`](api/) | Interfaces: API interna do Engine e vendor API do WebSocket |
| [`guides/`](guides/) | Passo a passo: começar, compilar, testar, criar templates, depurar |
| [`decisions/`](decisions/) | ADRs — decisões de arquitetura e seus motivos |

## architecture/

- [overview.md](architecture/overview.md) — visão geral, componentes e fluxo de dados
- [module-layout.md](architecture/module-layout.md) — árvore de código-fonte e regras de dependência
- [threading-model.md](architecture/threading-model.md) — threads, fila de comandos, sincronização
- [scene-and-properties.md](architecture/scene-and-properties.md) — modelo de cena, layers e propriedades
- [animation-engine.md](architecture/animation-engine.md) — clips, tracks, triggers, easing, tempo
- [rendering-pipeline.md](architecture/rendering-pipeline.md) — rasterização, texturas, máscaras, desenho
- [control-panel.md](architecture/control-panel.md) — como o dock é gerado do manifest

## manifest/

- [specification.md](manifest/specification.md) — estrutura geral, campos de topo, versionamento
- [layers.md](manifest/layers.md) — tipos de layer, propriedades, máscaras, hierarquia
- [animations.md](manifest/animations.md) — clips, tracks, keyframes, easings, triggers
- [controls-and-panel.md](manifest/controls-and-panel.md) — painel, seções e controles
- [actions-and-states.md](manifest/actions-and-states.md) — ações (`show`/`hide`) e estados nomeados
- [validation.md](manifest/validation.md) — erros, avisos e JSON Schema

## api/

- [engine-api.md](api/engine-api.md) — API interna única usada por painel e WebSocket
- [websocket-vendor.md](api/websocket-vendor.md) — requests e eventos do vendor `naxgc`

## guides/

- [getting-started.md](guides/getting-started.md)
- [building.md](guides/building.md)
- [testing.md](guides/testing.md)
- [authoring-templates.md](guides/authoring-templates.md)
- [svg-conventions.md](guides/svg-conventions.md)
- [debugging-and-performance.md](guides/debugging-and-performance.md)
- [contributing.md](guides/contributing.md)

## decisions/

Índice e formato em [decisions/README.md](decisions/README.md).

## Regras para manter a documentação

1. Mudou o formato do manifest? Atualize `manifest/`, o `template/` e o schema no mesmo PR.
2. Decisão nova e relevante? Crie uma ADR.
3. Mudou o escopo ou terminou um marco? Atualize o `ROADMAP.md`.
4. Documentação em pt-BR; código e comentários em inglês.
