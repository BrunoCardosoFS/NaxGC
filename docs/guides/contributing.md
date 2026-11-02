# Contribuindo

## Fluxo

1. Escolha uma tarefa do [ROADMAP](../ROADMAP.md) (ou abra uma issue descrevendo a proposta).
2. Crie um branch: `feat/animator-triggers`, `fix/mask-pool`, `docs/manifest-layers`.
3. Faça mudanças **pequenas e focadas**.
4. Rode formatação, build e testes ([building](building.md), [testing](testing.md)).
5. Abra o PR com a descrição abaixo.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/): `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `chore`. Escopo opcional: `feat(animator): tempo residual em triggers`.

## Descrição do PR

- **O quê** e **por quê**.
- **Como testei** (testes do core; passos manuais no OBS; plataforma).
- **Desempenho**, se mexeu em render/animator (antes/depois).
- **Documentação**: links para o que foi atualizado.

## Checklist do PR

- [ ] Compila sem warnings novos
- [ ] Testes adicionados/atualizados e passando
- [ ] `template/` ainda valida sem erros/avisos
- [ ] Spec do manifest atualizada (se mudou o formato) + schema
- [ ] ADR criada/atualizada (se mudou arquitetura)
- [ ] `ROADMAP.md` atualizado (se concluiu item)
- [ ] Regras de threads/render respeitadas ([AGENTS.md](../../AGENTS.md))

## Estilo

C++17, `clang-format` do template, nomes e comentários em inglês; docs em pt-BR. Detalhes no [AGENTS.md](../../AGENTS.md).

## Mudanças de formato (manifest)

1. Discuta primeiro (issue/ADR) — o manifest é contrato público.
2. Prefira adicionar campos **opcionais** com padrão seguro.
3. Se for incompatível, incremente `schema` e documente migração.
4. Atualize `docs/manifest/`, `schemas/`, `template/` e testes no mesmo PR.

## Licença

A definir (ver README). Contribuições seguirão a licença escolhida no M9.
