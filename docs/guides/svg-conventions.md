# Convenções do SVG

O SVG é o **visual**; o manifest é o **comportamento**. Estas regras tornam o SVG previsível para o NaxGC.

## 1. Tamanho

- `width`/`height`/`viewBox` iguais ao `canvas` do manifest (ex.: 1920×1080, origem em 0,0).
- Divergência gera aviso `NXG-W005` e escala proporcional.

## 2. `id`s

- Cada elemento que o manifest usa precisa de `id` **único**.
- Prefixos sugeridos: `txt_` (quadro de texto), `img_`/`_frame` (quadro de imagem), sufixos `mask`, `border`.
- Evite renomear `id`s sem atualizar o manifest (erro `NXG-E010`).
- Editores podem reescrever ids ao exportar; confira o arquivo final.

## 3. Ordem = z-order

A ordem dos elementos no arquivo é a ordem de empilhamento (primeiro = fundo). O manifest **não** define z-order.

## 4. Texto

- **Não use `<text>`** para texto dinâmico. Crie um `<rect id="txt_...">` com `fill="none"` como **quadro**; o NaxGC renderiza o texto dentro dele.
- Texto estático também pode ser quadro + `text` fixo no manifest (ex.: `AO VIVO`).
- Converta textos decorativos em **curvas/paths** se precisarem aparecer exatamente como no editor.

## 5. Máscaras

- Coloque as formas de máscara em um grupo (ex.: `<g id="masks">`) **no fim** do arquivo; elas têm `role: "mask"` e não são desenhadas.
- Use formas com preenchimento sólido (o que importa é o canal alfa).

## 6. Hierarquia

- Layers aninhadas herdam transformação e opacidade da layer pai (ex.: ponto pulsante dentro da tag).
- Conteúdo do grupo que **não** é layer faz parte da rasterização da layer pai.

## 7. Recursos suportados e não suportados

| Recurso | Situação |
|---|---|
| Retângulos, círculos, paths, grupos, gradientes, `opacity` | ✅ |
| `transform` estático nos elementos | ✅ (aplicado na rasterização) |
| Strokes, cantos arredondados (`rx`) | ✅ |
| Filtros complexos (blur, sombras) | ⚠️ variável; teste antes. Prefira layers separadas |
| `<text>` | ❌ para texto dinâmico (use quadros) |
| Animação SMIL/CSS dentro do SVG | ❌ ignorada (use clips do manifest) |
| Imagens externas (`href` http/arquivo) | ❌ bloqueadas; embute (`data:`) ou use layers `image` |
| Scripts | ❌ ignorados |

## 8. Dicas de exportação

- **Inkscape**: Salvar como *SVG simples*; confira IDs em Editar → Editor XML. Desative "estilo CSS" se possível (prefira atributos de apresentação).
- **Figma**: nome da layer vira `id` ao exportar; "Include `id` attribute" ligado.
- Evite *clip-path* e *mask* do próprio SVG; use máscaras do NaxGC (`role: "mask"`).

## 9. Posição de repouso

`x`/`y` nas animações são **deslocamentos** a partir de onde o elemento está desenhado no SVG. Desenhe o SVG na pose **final** (no ar); a pose fora do ar vem de `states.offair`.
