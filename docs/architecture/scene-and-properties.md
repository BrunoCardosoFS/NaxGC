# Cena e propriedades

## Modelo

```
Scene
 ├─ canvas (width, height)
 ├─ layers: lista em z-order (ordem do SVG)
 │    └─ Layer { name, type, role, parent?, children[], origin, props, raster, mask? }
 └─ estado dos controles (valores atuais)
```

- **Layer** é identificada por `name` (chave em `layers` do manifest). O `id` aponta para o elemento do SVG.
- **Hierarquia** vem do aninhamento no SVG: se `tag_aovivo_dot` está dentro de `tag_aovivo` e ambas são layers, a primeira é filha. A matriz mundial é `parent_world × local`; a opacidade efetiva é o produto das opacidades.
- **Máscara**: referência a outra layer (`mask`). A máscara tem seu próprio transform e não participa do desenho normal.

## Propriedades

Toda propriedade tem: tipo, valor padrão, valor **base**, valor **animado** (opcional) e um *flag* de sujeira (precisa re-rasterizar?).

| Propriedade | Tipo | Unidade/domínio | Aplica-se a | Re-rasteriza? |
|---|---|---|---|---|
| `x`, `y` | número | px do canvas, deslocamento da posição de repouso | todas | não |
| `scale`, `scaleX`, `scaleY` | número | 1 = tamanho original | todas | não* |
| `rotation` | número | graus, horário | todas | não |
| `opacity` | número | 0..1 | todas | não |
| `visible` | booleano | | todas | não |
| `fill`, `stroke` | cor | `#rrggbb[aa]` | `svg` | **sim** |
| `strokeWidth` | número | px | `svg` | **sim** |
| `text` | string | UTF-8 | `text` | **sim** |
| `color` | cor | | `text` | **sim** |
| `size` | número | px | `text` | **sim** |

\* Escalas acima do tamanho rasterizado perdem nitidez; ver `rasterScale` em [layers](../manifest/layers.md).

### Origem (pivot)

`origin: [ox, oy]` (0..1, relativo ao bbox da layer; padrão `[0,0]`) define o ponto de escala/rotação.

## Resolução de valor

```
valor_efetivo = animado (se houver track ativa na propriedade) senão base
```

- **Base** muda por: `states`, controles (`bind`), `Set` via WebSocket, `onEnd: hold` de clips.
- **Animado** é escrito pelo Animator a cada tick e some quando a track termina (a menos que `onEnd: hold`, que grava o valor final na base).
- Conflito (duas tracks na mesma propriedade): vence a **iniciada por último**; `blend: "add"` soma em vez de substituir.

## Bind

`bind` usa caminho `layers.<nome>.<prop>`. Um controle pode ter **vários** binds (lista). O valor do controle é convertido para o tipo da propriedade; falha de conversão vira erro de validação (carga) ou erro de resposta (runtime).

## Estados

`states.<nome>` é um mapa `caminho → valor`. Aplicar um estado grava **base** em todas as propriedades listadas. `initialState` é aplicado ao carregar.

## Snapshot

A cada tick o Engine publica (somente leitura) os valores dos controles e a lista de clips ativos. Ver [threading-model](threading-model.md).
