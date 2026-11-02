# AGENTS.md

Instruções para agentes de código e para humanos que contribuem com o **NaxGC**.
Este arquivo segue o formato aberto [agents.md](https://agents.md/). Detalhes mais longos ficam em [`docs/`](docs/README.md); aqui está só o que é preciso para trabalhar sem errar.

## Visão geral do projeto

NaxGC é um plugin **nativo C++ para OBS Studio** que funciona como gerador de caracteres (GC) de TV/streaming. Ele:

- renderiza SVG animado direto no pipeline gráfico do OBS (fonte nativa, sem Browser Source/CEF);
- é totalmente guiado por um **template** em disco: `manifest.json` + `layout.svg` + fontes;
- gera o **painel de controle** (dock Qt) a partir do manifest;
- é controlado também via **obs-websocket** (vendor API, vendor `naxgc`).

Motivação: o Browser Source perdia quadros. No NaxGC a animação roda no relógio do OBS (`video_tick`), é calculada de forma determinística e desenhada na GPU.

Estado atual: **pré-código**. Existem a documentação (`docs/`) e um template de exemplo (`template/`). Veja [`docs/ROADMAP.md`](docs/ROADMAP.md) para saber em que marco estamos.

## Fonte da verdade

1. `docs/manifest/` define o **formato do manifest** (contrato público). Se o código e a spec divergirem, a spec manda — ou a spec é atualizada **no mesmo PR**.
2. `docs/architecture/` define as camadas e regras de threads/renderização.
3. `docs/decisions/` registra o porquê das decisões (ADRs). Não contrarie uma ADR aceita sem criar outra que a substitua.
4. `template/` é o template de referência; ele deve **sempre** carregar e validar com a versão atual do motor.

## Layout do repositório

```
AGENTS.md  README.md
docs/                  # documentação (começar por docs/README.md)
template/              # template padrão de exemplo (manifest.json, layout.svg, fonts/)
src/                   # (a criar) código do plugin
  core/                # lógica pura, SEM dependência de OBS ou Qt: manifest, cena, animator, easing
  render/              # rasterização (lunasvg/Qt) e desenho (libobs graphics)
  obs/                 # integração com libobs: obs_source_info, settings, hotkeys
  ui/                  # dock Qt gerado do manifest
  ws/                  # vendor API do obs-websocket
tests/                 # (a criar) testes do core, fixtures de manifest válidos/inválidos
schemas/               # (a criar) JSON Schema do manifest
cmake/ CMakeLists.txt CMakePresets.json buildspec.json   # vindos do obs-plugintemplate
```

Regra de dependência (nunca inverter): `core` ← `render` ← `obs` / `ui` / `ws`. O `core` **não** inclui headers do OBS nem do Qt.

## Setup, build e testes

Base: [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) (CMake presets, `buildspec.json`, CI). Os comandos abaixo valem quando o código existir; confirme os nomes dos presets em `CMakePresets.json`.

```bash
# configurar e compilar (exemplo Linux; use o preset da sua plataforma)
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64

# testes do core (não precisam do OBS instalado)
ctest --preset ubuntu-x86_64 --output-on-failure
```

- Windows: preset `windows-x64`. macOS: preset `macos`.
- Antes de abrir PR: formatar (`clang-format` para `src/` e `gersemi` para `CMakeLists.txt`/`cmake/*.cmake`, limite de 120 colunas e indentação de 2 espaços), compilar sem warnings novos (`/WX` / `-Werror`) e rodar os testes.
- Headers de terceiros ficam em `third_party/` (fora de `src/` e incluídos com `SYSTEM` no CMake) para não dispararem o `clang-format` nem warnings de compilação.
- Mudou manifest, clip, easing ou trigger? Adicione/atualize um teste em `tests/` e confira que `template/` ainda valida.

## Convenções de código

- **C++17**, estilo do `.clang-format` do obs-plugintemplate. Nomes: tipos em `PascalCase`, funções/variáveis em `snake_case`, constantes `kPascalCase`, namespace `naxgc`.
- Identificadores e comentários de código em **inglês**; documentação (`docs/`, README) em **português (pt-BR)**.
- Log com `blog(LOG_INFO/WARNING/ERROR, "[NaxGC] ...")`. Nunca `printf`/`std::cout`.
- Sem exceções atravessando callbacks do OBS: capture na borda (`obs_source_info`, handlers do WebSocket, slots do Qt) e converta em log/erro de resposta.
- Prefira RAII (`std::unique_ptr` com deleters para `gs_texture_t`, `gs_texrender_t`, `obs_data_t`).
- JSON: `nlohmann/json` apenas no `core`. Rasterização SVG: `lunasvg`. Texto: Qt (`QPainter`/`QFont`).

## Regras de threads e renderização (críticas)

- Chamadas `gs_*` só no contexto gráfico (dentro de `video_render` ou entre `obs_enter_graphics()` / `obs_leave_graphics()`).
- **Nada de bloqueio, I/O de disco, parse de JSON ou alocação grande em `video_render`/`video_tick`.** Carregar template, ler arquivos e validar acontecem fora dessas funções.
- UI (thread Qt) e WebSocket **não** mexem na cena diretamente: enfileiram `Command`s que o `video_tick` consome (ver `docs/architecture/threading-model.md`).
- O tempo da animação vem **exclusivamente** do `seconds` do `video_tick`. Proibido usar relógio de parede (`std::chrono::steady_clock`) para avançar clips.
- Rasterização acontece só quando o conteúdo da layer muda (texto, cor); movimento, escala, rotação e opacidade são matrizes/uniforms na GPU.

## Segurança

- O template é **entrada não confiável**: valide caminhos (sem `..`, sem caminhos absolutos, sem links simbólicos para fora da pasta do template), limite tamanhos (manifest, SVG, imagens, fontes) e profundidade/quantidade de clips e triggers.
- Nenhum acesso à rede pelo plugin. O SVG não pode carregar recursos externos (`href` http/file fora do pacote é ignorado e logado).
- Respostas do WebSocket nunca devem vazar caminhos absolutos do sistema de arquivos.

## Documentação e fluxo de trabalho

- Commits no padrão **Conventional Commits** (`feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`), escopo opcional (`feat(animator): ...`).
- Mudança de comportamento público → atualizar `docs/` e, se aplicável, `template/` e o `ROADMAP.md`.
- Decisão arquitetural nova → criar ADR em `docs/decisions/` (próximo número livre).
- PRs pequenos e focados; descreva como testou (inclua nota de desempenho quando mexer em render/animator).
- Não adicione dependências novas sem ADR ou justificativa explícita no PR.

## Armadilhas conhecidas

- **`.gitignore` em modo *allowlist* (`/*`)**: o repositório ignora tudo por padrão na raiz e libera apenas entradas explícitas com `!`. Qualquer nova pasta ou arquivo de topo precisa ser liberado no `.gitignore`, senão o Git o ignora silenciosamente. Se a pasta `data/` for criada ou removida, reconfigure o CMake (`cmake --preset ...`) para atualizar as regras de post-build.
- **Locale do OBS**: não usar `OBS_MODULE_USE_DEFAULT_LOCALE` enquanto o plugin não possuir arquivos `data/locale/en-US.ini` reais, para não gerar erro de carregamento de locale no log do OBS.
- **Instalação de plugins no Windows**: o pacote gerado pelo CI (`naxgc/bin/64bit/naxgc.dll`) deve ser extraído em `%ProgramData%\obs-studio\plugins\` (`C:\ProgramData\obs-studio\plugins\naxgc\...`, pasta oculta na raiz do `C:\`).
- O `x`/`y` de uma layer é **deslocamento em pixels do canvas a partir da posição de repouso do SVG**, não coordenada absoluta.
- O z-order vem da **ordem dos elementos no SVG**, não da ordem das chaves em `layers`.
- Uma layer de texto usa um `<rect id="txt_*">` como quadro; o SVG não usa `<text>`.
- Máscara (`role: "mask"`) nunca é desenhada diretamente; só afeta a layer que aponta para ela.
- Um clip com `triggers` só termina quando suas tracks **e** seus filhos `linked` terminam.
