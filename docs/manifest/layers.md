# Layers

Cada entrada de `layers` liga um **nome** a algo desenhável. O nome é usado em animações, binds e estados.

```json
"layers": {
  "linha1": { "type": "svg", "id": "linha1" }
}
```

## Campos comuns

| Campo | Tipo | Padrão | Descrição |
|---|---|---|---|
| `type` | `"svg"` \| `"text"` \| `"image"` | — | Tipo da layer (obrigatório) |
| `id` | string | — | `id` do elemento no SVG (obrigatório para `svg` e `text`) |
| `role` | `"mask"` | — | Marca a layer como máscara (não é desenhada) |
| `origin` | `[ox, oy]` | `[0,0]` | Pivot de escala/rotação (0..1 do bbox) |
| `opacity` | número | `1` | Opacidade base |
| `visible` | booleano | `true` | Visibilidade base |
| `mask` | string | — | Nome de uma layer com `role: "mask"` |
| `maskMode` | `"alpha"` \| `"inverse"` | `"alpha"` | Como usar a máscara |
| `rasterScale` | número | `auto` | Multiplicador de resolução da rasterização (se omitido, usa a maior escala detectada nos clips/states da layer, min `1`) |

Valores iniciais definidos aqui são a **base**; `states` e controles podem sobrescrevê-los.

## `type: "svg"`

Rasteriza o elemento do SVG com o `id` dado (qualquer `g`, `rect`, `path`, `circle`…).

- Descendentes que também são layers **não** entram nesta rasterização: tornam-se **filhos** (herdam transformação e opacidade).
- Propriedades extras: `fill`, `stroke`, `strokeWidth` (sobrescrevem atributos do nó; para grupos, aplicam-se a todos os descendentes não-layer que tenham preenchimento/traço).
- Dica: para colorir por controle, use um `rect`/`path` direto como layer (como `linha1` no template) em vez de um grupo.

## `type: "text"`

Texto dinâmico renderizado dentro de um **quadro**: o bbox do elemento `id` (normalmente `<rect id="txt_...">` sem preenchimento).

| Campo | Tipo | Padrão | Descrição |
|---|---|---|---|
| `text` | string | `""` | Texto base |
| `font` | string | — | `family` declarada em `fonts` (ou do sistema) |
| `weight` | inteiro | `400` | 100–900 |
| `italic` | booleano | `false` | |
| `size` | número | `32` | Tamanho em px |
| `minSize` | número | `size` | Mínimo para `overflow: "shrink"` |
| `color` | cor | `#ffffff` | |
| `align` | `left` \| `center` \| `right` | `left` | |
| `valign` | `top` \| `middle` \| `bottom` | `middle` | |
| `overflow` | `shrink` \| `clip` \| `ellipsis` \| `wrap` | `clip` | Comportamento quando excede o quadro |
| `maxLines` | inteiro | `1` | Para `wrap` |
| `lineHeight` | número | `1.2` | Multiplicador |
| `letterSpacing` | número | `0` | px |
| `uppercase` | booleano | `false` | |

O elemento `id` do quadro **não é desenhado**. A posição do texto acompanha o quadro; animar `x`/`y` da layer de texto move o texto relativamente a essa posição.

## `type: "image"`

```json
"foto": { "type": "image", "id": "foto_frame", "src": "assets/foto.png", "fit": "cover" }
```

- `id`: elemento do SVG que define o quadro (como no texto).
- `src`: arquivo dentro do pacote (PNG/JPG/WebP/SVG). Pode ser alterado em runtime (`layers.foto.src`) por controle.
- `fit`: `contain` | `cover` | `stretch`.

## Máscaras

```json
"linha1mask":  { "type": "svg",  "id": "linha1mask", "role": "mask" },
"linha1texto": { "type": "text", "id": "txt_linha1", "mask": "linha1mask" }
```

- A máscara é uma layer normal em tudo (pode ter `x`, `scale`, animações), exceto que **não é desenhada**.
- O resultado visível é `alfa(layer) × alfa(máscara)` (`alpha`) ou `alfa(layer) × (1 − alfa(máscara))` (`inverse`).
- Uma máscara pode servir a várias layers. Máscara de máscara não é suportada (erro `NXG-E012`).
- A máscara é avaliada no **espaço do canvas** (não herda o transform da layer mascarada).

## Hierarquia

Vem do aninhamento no SVG:

```xml
<g id="tag_aovivo">
  <rect .../>                         <!-- parte da rasterização de tag_aovivo -->
  <circle id="tag_aovivo_dot" .../>   <!-- layer filha -->
  <rect id="txt_aovivo" .../>         <!-- layer de texto filha -->
</g>
```

`visible: false`, `opacity` e transformações de `tag_aovivo` afetam as filhas.

## Elementos do SVG sem layer

Conteúdo visual que não pertence a nenhuma layer declarada é desenhado como **fundo estático implícito** na posição do z-order, com aviso `W001` (para você perceber esquecimentos).

## Propriedades animáveis

`x`, `y`, `scale`, `scaleX`, `scaleY`, `rotation`, `opacity`, `visible`, `fill`, `stroke`, `strokeWidth` (svg), `text`, `color`, `size` (text). Ver tabela completa em [scene-and-properties](../architecture/scene-and-properties.md).
