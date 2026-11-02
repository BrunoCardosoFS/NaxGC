# 0003 — Rasterizar por layer e animar na GPU
- Status: Aceita
- Data: 2026-11-02

## Contexto
Rasterizar o SVG inteiro a cada quadro em 1080p60 faz da CPU o gargalo e ameaça a fluidez, que é a razão do projeto.

## Decisão
Rasterizar **cada layer uma vez** (bbox justo) em textura própria e re-rasterizar **somente quando o conteúdo muda** (texto, cor, traço). Posição, escala, rotação e opacidade são aplicadas por matrizes/uniforms na GPU. Máscaras compostas por `gs_texrender` + effect.

## Consequências
- (+) Custo por quadro baixo e estável; animações suaves.
- (+) Modelo simples de "propriedades animáveis" no `core`.
- (−) Animações que mudam o *desenho* (ex.: morph de path, blur animado) não são suportadas diretamente.
- (−) Escalar acima do tamanho rasterizado perde nitidez → `rasterScale` por layer.
- (−) Gestão de texturas/pool de texrender exige cuidado com ciclo de vida no contexto gráfico.

## Alternativas consideradas
- Rasterizar o SVG completo por quadro (CPU-bound).
- Renderizador vetorial na GPU (ex.: Skia/NanoVG): dependência grande; reavaliar se necessário.
