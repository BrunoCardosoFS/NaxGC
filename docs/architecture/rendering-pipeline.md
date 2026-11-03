# Pipeline de renderização

Implementado em `src/render/`. Toda chamada `gs_*` roda na thread gráfica.

## Visão geral

```
Carga (worker)            Tick (gráfica)                 Render (gráfica)
SVG ──► nós por id  ─►  layers sujas?  ─► rasteriza ─►  para cada layer em z-order:
fontes ─► QFontDatabase     (texto/cor)    (lunasvg/Qt)     matriz × opacidade × (máscara?)
                                              │               gs_draw_sprite
                                              ▼
                                       gs_texture_t (cache)
```

## Rasterização

### Layers `svg`
- Para cada layer, renderiza **apenas o nó** (e descendentes que não sejam outras layers) com lunasvg: `getElementById(id)` → bbox → bitmap.
- Textura com **bbox justo** (+ margem de 1–2 px para antialias) em vez de canvas inteiro; guarda o `offset` (posição do bbox no canvas) para desenhar no lugar certo. Reduz memória e tempo de upload.
- Formato: BGRA 8 bits, alfa **pré-multiplicado**.
- Mudança de `fill`/`stroke`/`strokeWidth` → reescreve o atributo no nó (cópia do documento) e re-rasteriza **só essa layer**.

### Layers `text`
- Quadro vem do `<rect id="txt_*">` (bbox no SVG). Texto desenhado com `QPainter` em `QImage` (ARGB32 pré-multiplicado) do tamanho do quadro.
- Fonte: família + peso + tamanho; fontes do pacote carregadas com `QFontDatabase::addApplicationFontFromData` (isoladas por template quando possível).
- Overflow: `shrink` (reduz até `minSize`), `clip`, `ellipsis`, `wrap` (com `maxLines`).
- Alinhamento horizontal/vertical, `letterSpacing`, `lineHeight`, `uppercase`.

### Escala e qualidade (estratégia híbrida)
- Rasteriza em `canvas_scale × effectiveRasterScale`.
- **Detecção automática:** se `rasterScale` não for informado na layer, o loader inspeciona a base, `states` e tracks de `clips` daquela layer para descobrir o maior `scale`/`scaleX`/`scaleY` atingido (mínimo `1.0`, limitado a um teto de segurança como `4.0` para proteger VRAM).
- **Override manual:** se a layer declarar `rasterScale` no manifest (ex.: `1.5`), esse valor prevalece sobre o cálculo automático.
- Filtro linear; sem mipmaps (evita borrão em animações de escala).

## Texturas e cache

- `TextureCache`: uma textura por layer; criada/atualizada com `gs_texture_create`/`gs_texture_set_image` dentro do contexto gráfico.
- Re-rasterização só quando `dirty`. Limite de re-rasterizações por tick configurável (para não estourar frame time quando várias mudam juntas).
- Liberação em `destroy`, troca de template e `obs_module_unload`, sempre com `obs_enter_graphics()`.

## Desenho

Para cada layer visível, em z-order:

1. Calcular matriz mundial (`parent × local`, com `origin`) e opacidade efetiva.
2. `gs_matrix_push()`; aplicar `translate/rotate/scale`.
3. Blend premultiplicado: `gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA)`.
4. Efeito padrão do OBS (`OBS_EFFECT_DEFAULT`) com `image` = textura e `color` = opacidade; `gs_draw_sprite`.
5. `gs_matrix_pop()`.

Fonte registrada com `OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_DO_NOT_DUPLICATE | OBS_SOURCE_SRGB`. A flag `OBS_SOURCE_DO_NOT_DUPLICATE` garante que transições de cena e o Modo Estúdio (Preview vs Program) compartilhem a mesma instância viva da fonte por referência, evitando clones paralelos e garantindo sincronia perfeita ao cortar cenas.

## Máscaras

Layer com `mask: "<nome>"` é desenhada assim:

1. `texrender A`: desenha a layer mascarada (com seu transform) em textura do tamanho do canvas.
2. `texrender B`: desenha a layer de máscara (com seu transform).
3. Compõe A × alfa(B) no alvo final com o effect `mask.effect` (`maskMode: alpha` ou `inverse`).

Otimizações:
- **Pool** de texrender reutilizados (sem alocar por quadro).
- Usar bbox da união (camada + máscara) como região em vez do canvas todo quando possível.
- Pular layers/máscaras invisíveis ou com opacidade 0.
- Se a máscara for estática e a layer não mudar, cachear o resultado composto (futuro).

## Ordem e hierarquia

- z-order = ordem dos elementos no SVG.
- Layers filhas desenham logo após o pai (ordem do SVG) com matriz/opacidade herdadas.
- Máscaras com `role: mask` nunca entram na lista de desenho.

## Desempenho (metas)

| Item | Meta |
|---|---|
| Tempo de `video_tick` + `video_render` por fonte | < 1 ms típico (template de exemplo, 1080p) |
| Alocações por quadro | zero (steady state) |
| Re-rasterização de texto | < 2 ms por layer |
| Uploads de textura por quadro | apenas layers sujas |

Medição e ferramentas: [guides/debugging-and-performance](../guides/debugging-and-performance.md).

## Riscos conhecidos

- Suporte de `<text>` no lunasvg é limitado → por isso texto vive em layers `text` (Qt).
- Fidelidade de filtros SVG (blur, sombras) pode variar → preferir recursos simples no layout ou rasterizar sombras como layers separadas.
- Fontes: uso de fontes do pacote depende de licença; documentar no template.
