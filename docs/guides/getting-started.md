# Primeiros passos

## O que você precisa

- OBS Studio **30.0+** instalado (somente Qt 6).
- Git, CMake, um compilador C++17 (MSVC 2022, Xcode, GCC/Clang).
- Um editor com suporte a JSON Schema (VS Code recomendado).

## Para quem vai criar templates (sem compilar)

1. Copie `template/` para a biblioteca de templates (caminho definido no M6; por enquanto, qualquer pasta).
2. Edite `layout.svg` no Inkscape/Figma mantendo os `id`s (ver [svg-conventions](svg-conventions.md)).
3. Ajuste `manifest.json` (layers, painel, animações). Com `$schema` configurado, o VS Code valida enquanto você edita.
4. No OBS: adicionar fonte **NaxGC** → escolher a pasta do template → usar o dock **NaxGC**.
5. Com hot reload (M8), salve o arquivo e veja o resultado na hora.

Leitura: [authoring-templates](authoring-templates.md).

## Para quem vai desenvolver o plugin

1. Leia [architecture/overview](../architecture/overview.md) e [threading-model](../architecture/threading-model.md).
2. Veja o [ROADMAP](../ROADMAP.md) e escolha uma tarefa do marco atual.
3. Prepare o ambiente: [building](building.md).
4. Rode os testes do core: [testing](testing.md).
5. Siga [contributing](contributing.md) e o [AGENTS.md](../../AGENTS.md).

## Primeira verificação do ambiente (quando houver código)

- O plugin compila com o preset da sua plataforma.
- O módulo aparece no OBS (log: `[NaxGC] plugin loaded`).
- O template de exemplo carrega sem erros nem avisos.
