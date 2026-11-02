# 0002 — Template declarativo em JSON + SVG
- Status: Aceita
- Data: 2026-11-02

## Contexto
É necessário que GCs (layout, painel, animações, ações) sejam criados e alterados sem recompilar o plugin, por pessoas de design.

## Decisão
Descrever o GC por um **pacote**: `layout.svg` (visual) + `manifest.json` (layers, painel, estados, animações, ações) + fontes. O C++ não contém conhecimento de nenhum template.

## Consequências
- (+) Templates intercambiáveis e versionáveis; hot reload possível.
- (+) Painel gerado automaticamente; menos código de UI por template.
- (−) O formato do manifest vira **contrato público**: exige versionamento (`schema`), validação rigorosa e documentação.
- (−) Expressividade limitada ao que o formato suporta (sem scripts por decisão).

## Alternativas consideradas
- Templates em HTML/JS (reintroduz o problema do CEF).
- Scripts embutidos (Lua/JS): maior poder, maior superfície de segurança e de suporte.
- Formato binário/GUI própria: mais caro; JSON + SVG usa ferramentas existentes.
