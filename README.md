# NaxGC

Gerador de caracteres (GC) nativo para OBS Studio, com SVG animado, painel de controle gerado automaticamente e controle remoto via obs-websocket, tudo definido por um template em JSON.

> **Status:** pré-código. A documentação e o template de exemplo estão prontos; a implementação segue o [roadmap](docs/ROADMAP.md).

## Por que existe

Geradores de caracteres feitos em HTML rodando no Browser Source (CEF) sofrem com perda de quadros, porque a animação roda fora de sincronia com o relógio de vídeo do OBS. O NaxGC é uma **fonte nativa**: a animação é calculada a partir do tempo de cada quadro do OBS (`video_tick`) e desenhada na GPU, então fica determinística e suave.

## O que ele faz

- **Fonte nativa** do OBS que renderiza o template.
- **Template declarativo**: `manifest.json` + `layout.svg`.
- **Animações em JSON**: clips com tracks, easings, keyframes, loops e triggers (uma animação dispara outras).
- **Máscaras** e **layers aninhadas**.
- **Painel de controle (dock Qt) gerado do manifest**: campos de texto, cores, toggles, dropdowns, botões, seções.
- **Controle via obs-websocket** (vendor `naxgc`): o mesmo que o painel faz, Companion/Stream Deck/scripts também fazem.
- **Hot reload** do template para iterar rápido ao criar GCs.

## Como funciona (resumo)

```
template/ (manifest.json + layout.svg + fonts)
        │  parse + validação
        ▼
   Cena (layers, propriedades) ◄── Painel Qt / obs-websocket  (via fila de comandos)
        │
   Animator (clips, triggers, easing)  ── avança com o tempo do video_tick
        │
   Renderizador (texturas por layer, matrizes, máscaras)  ──► OBS
```

Detalhes em [`docs/architecture/overview.md`](docs/architecture/overview.md).

## Exemplo de manifest (trecho)

```json
{
  "layers": {
    "linha1":     { "type": "svg",  "id": "linha1" },
    "linha1mask": { "type": "svg",  "id": "linha1mask", "role": "mask" },
    "linha1texto": { "type": "text", "id": "txt_linha1", "mask": "linha1mask", "font": "Inter", "size": 46 }
  },
  "animations": {
    "linha1.in": {
      "tracks": [{ "layer": "linha1", "prop": "x", "from": -1200, "to": 0, "dur": 0.6, "ease": "outCubic" }],
      "triggers": [{ "play": "linha1mask.in", "at": 0.15, "mode": "linked" }]
    }
  }
}
```

O template completo está em [`template/`](template/).

## Estrutura do repositório

```
AGENTS.md          instruções para agentes de código e contribuidores
README.md          este arquivo
docs/              documentação (índice em docs/README.md)
template/          template padrão de exemplo
  manifest.json    layers, painel, estados, animações, ações
  layout.svg       visual (grupos/elementos com id)
  fonts/           fontes do template (Inter, licença OFL)
```

Quando o código existir: `src/` (core, render, obs, ui, ws), `tests/`, `schemas/` — ver [`AGENTS.md`](AGENTS.md).

## Requisitos (previstos)

- OBS Studio com Qt 6 e frontend API (versão mínima a confirmar no marco M0).
- obs-websocket 5.x (já incluso no OBS recente) para o controle remoto.
- Para compilar: CMake, compilador C++17, toolchain da plataforma (ver [`docs/guides/building.md`](docs/guides/building.md)).

## Documentação

- [Índice da documentação](docs/README.md)
- [Roadmap](docs/ROADMAP.md)
- [Especificação do manifest](docs/manifest/specification.md)
- [Guia para criar templates](docs/guides/authoring-templates.md)
- [API WebSocket](docs/api/websocket-vendor.md)

## Licença

A definir. As fontes em `template/fonts/` mantêm suas próprias licenças (Inter: SIL OFL 1.1, ver `template/fonts/OFL.txt`).
