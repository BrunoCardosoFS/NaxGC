# Especificação do manifest (schema 1)

Contrato público do NaxGC. O exemplo de referência é [`template/manifest.json`](../../template/manifest.json).

## Pacote de template

```
meu-template/
├── manifest.json     # obrigatório
├── layout.svg        # obrigatório (nome configurável em "svg")
├── fonts/            # opcional
└── assets/           # opcional (imagens raster/SVG usadas por layers "image")
```

- Pasta ou `.zip` (mesma estrutura). Todos os caminhos do manifest são **relativos** à raiz do pacote.
- Proibido: caminhos absolutos, `..`, links simbólicos que saiam do pacote.
- Codificação: UTF-8. Comentários não são suportados em JSON (use `meta.description`).

## Estrutura de topo

| Campo | Tipo | Obrig. | Descrição |
|---|---|---|---|
| `$schema` | string | não | Caminho/URL do JSON Schema (autocomplete no editor) |
| `schema` | inteiro | **sim** | Versão do formato. Atual: `1` |
| `meta` | objeto | **sim** | Identificação do template |
| `canvas` | objeto | **sim** | Tamanho e taxa de referência |
| `svg` | string | **sim** | Arquivo SVG de layout |
| `fonts` | array | não | Fontes do pacote |
| `layers` | objeto | **sim** | Layers por nome |
| `states` | objeto | não | Estados nomeados (snapshots de propriedades) |
| `initialState` | string | não | Estado aplicado ao carregar (padrão: nenhum) |
| `panel` | objeto | não | Definição do painel de controle |
| `animations` | objeto | não | Clips por nome |
| `actions` | objeto | não | Ações por nome |

Campos desconhecidos geram **aviso** (não erro) para permitir extensões futuras (prefixo `x-` é reservado para campos do autor e nunca avisa).

## `meta`

| Campo | Tipo | Obrig. | Descrição |
|---|---|---|---|
| `id` | string | **sim** | Identificador único, estilo `dominio.nome` (`[a-z0-9.-]`) |
| `name` | string | **sim** | Nome exibido |
| `version` | string | não | SemVer do template |
| `author` | string | não | |
| `description` | string | não | |
| `requires` | objeto | não | Ex.: `{ "naxgc": ">=0.1.0" }`; template incompatível é recusado com mensagem clara |

## `canvas`

| Campo | Tipo | Descrição |
|---|---|---|
| `width`, `height` | inteiro | Tamanho em px; deve coincidir com o `viewBox`/tamanho do SVG (senão, aviso e escala proporcional) |
| `fps` | número | Taxa de referência **informativa**; a animação usa o tempo real do OBS |

O tamanho da fonte no OBS pode sobrescrever `canvas` (escala uniforme), ver `obs/settings`.

## `fonts`

```json
"fonts": [ { "family": "Inter", "file": "fonts/Inter.ttf" } ]
```

- `family`: nome usado em `layers[].font`.
- `file`: TTF/OTF. Fontes variáveis são aceitas (peso escolhido em `weight`).
- Fonte ausente: **aviso** `W002` e uso de fonte do sistema/fallback.
- Respeite a licença; inclua o arquivo de licença no pacote (ex.: `fonts/OFL.txt`).

## Convenções de nomes

- Nomes de layers, clips, states, actions e controles: `[A-Za-z0-9_.-]+`, únicos dentro do seu grupo.
- Caminhos de propriedade (`bind`, `states`): `layers.<layer>.<prop>`.
- Tempos em **segundos**; distâncias em **pixels do canvas**; ângulos em **graus**; opacidade em **0..1**; cores `#rgb`, `#rrggbb` ou `#rrggbbaa`.

## Ordem de empilhamento

Definida pela **ordem dos elementos no SVG**, não pela ordem de `layers`.

## Versionamento do formato

- `schema` inteiro. Mudanças compatíveis (campos novos opcionais) não incrementam; mudanças incompatíveis incrementam.
- O motor recusa `schema` maior que o suportado (`NXG-E002`) e mantém suporte a versões anteriores enquanto possível, com migração documentada em `CHANGELOG`.

## Exemplo mínimo

```json
{
  "schema": 1,
  "meta": { "id": "exemplo.minimo", "name": "Mínimo" },
  "canvas": { "width": 1920, "height": 1080 },
  "svg": "layout.svg",
  "layers": { "barra": { "type": "svg", "id": "barra" } },
  "animations": {
    "in": { "tracks": [{ "layer": "barra", "prop": "x", "from": -800, "to": 0, "dur": 0.5, "ease": "outCubic" }] }
  },
  "actions": { "show": [{ "play": "in" }] }
}
```

## Documentos relacionados

[layers](layers.md) · [animations](animations.md) · [controls-and-panel](controls-and-panel.md) · [actions-and-states](actions-and-states.md) · [validation](validation.md)
