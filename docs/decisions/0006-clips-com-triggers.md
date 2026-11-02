# 0006 — Animações como clips com triggers
- Status: Aceita
- Data: 2026-11-02

## Contexto
Animações têm dependências: ao iniciar a barra (`linha1`), a máscara (`linha1mask`) e a borda (`linha1border`) devem iniciar automaticamente, com atrasos, e parar junto. Também é preciso coreografar várias linhas, logo e tag.

## Decisão
Modelar animações como **clips** reutilizáveis com `tracks` e `triggers`. Triggers iniciam outros clips (`at`, `on: end|marker:*`) em modo `linked` (acompanha o pai) ou `detached`. Filhos iniciam com **tempo residual** no mesmo tick. Clips sem tracks servem de coreografia. Estados (`states`) e ações (`actions`) complementam.

## Consequências
- (+) Dependências declaradas no manifest; sem código por template.
- (+) Sincronia exata entre clips relacionados.
- (−) Necessidade de validar ciclos, limites de profundidade e referências.
- (−) Semântica de conclusão (filhos `linked`) e interrupção precisa ser bem testada.

## Alternativas consideradas
- Uma única timeline por GC (simples, mas pouco reutilizável e difícil de compor).
- Máquina de estados completa (poderosa, mas mais complexa para autores).
- Animação reativa por expressões (`drive`): fica no backlog.
