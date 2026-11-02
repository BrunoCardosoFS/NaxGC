# 0004 — lunasvg para SVG e Qt para texto
- Status: Aceita
- Data: 2026-11-02

## Contexto
Precisamos rasterizar nós do SVG por `id` com dependência leve, e renderizar texto dinâmico (acentos, fontes do pacote, quebra, *shrink*) com qualidade. Bibliotecas SVG leves têm suporte limitado a `<text>`.

## Decisão
- **lunasvg** rasteriza elementos do SVG (formas, grupos, gradientes) por `id`.
- **Qt** (`QPainter`/`QFont`/`QImage`) rasteriza **texto**, em layers `text` cujo quadro é um `<rect id="txt_*">` do SVG. O SVG não usa `<text>` para conteúdo dinâmico.

## Consequências
- (+) Texto de alta qualidade, com fontes do pacote e métricas controláveis.
- (+) Reaproveita o Qt já exigido pelo dock.
- (−) Dependência do Qt também no render (isolada em `render/rasterizer_text`).
- (−) Designers precisam seguir a convenção de quadros de texto ([svg-conventions](../guides/svg-conventions.md)).

## Alternativas consideradas
- resvg: melhor fidelidade, mas integração C++ mais pesada.
- QtSvg: simples, porém suporte parcial a recursos e granularidade por `id` menos direta.
- FreeType/HarfBuzz diretos: mais controle, bem mais trabalho.
