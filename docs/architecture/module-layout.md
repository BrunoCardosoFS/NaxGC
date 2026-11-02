# Organização do código-fonte

Estrutura-alvo de `src/` (a criar a partir do M0). Nomes de pastas e namespaces: `naxgc::<módulo>`.

```
src/
├── plugin-main.cpp              # obs_module_load / unload, registro da fonte, dock e vendor
├── core/                        # SEM libobs, SEM Qt
│   ├── manifest/                # modelo tipado + parser + validação
│   ├── scene/                   # Layer, Property, hierarquia, estados
│   ├── anim/                    # Clip, Track, Trigger, Easing, Animator
│   ├── engine/                  # Engine, Command, CommandQueue, API pública
│   └── util/                    # resultado/erro, cores, matrizes 2D, paths seguros
├── render/                      # depende de core + libobs graphics + lunasvg + Qt (texto)
│   ├── rasterizer_svg.cpp
│   ├── rasterizer_text.cpp
│   ├── texture_cache.cpp
│   ├── renderer.cpp             # desenho em z-order, máscaras
│   └── effects/mask.effect
├── obs/                         # integração com libobs
│   ├── naxgc_source.cpp         # obs_source_info (create/tick/render/properties)
│   ├── settings.cpp
│   └── hotkeys.cpp
├── ui/                          # dock Qt
│   ├── dock.cpp
│   ├── control_factory.cpp      # um widget por tipo de controle
│   └── instance_selector.cpp
└── ws/                          # vendor API do obs-websocket
    ├── vendor.cpp
    └── requests.cpp
tests/
├── core/                        # animator, easing, manifest, triggers
├── fixtures/manifests/{valid,invalid}/
└── fixtures/golden/             # saídas esperadas de avaliação de animação
schemas/manifest.schema.json
```

## Regras de dependência

```
core  ◄──  render  ◄──  obs
  ▲                       ▲
  ├──────────  ui  ───────┤
  └──────────  ws  ───────┘
```

- `core` não inclui nada de OBS, Qt ou lunasvg. Isso garante testes rápidos e portáveis.
- `render` conhece libobs (`gs_*`), lunasvg e Qt (só para texto).
- `ui` e `ws` falam com o Engine **apenas** pela API pública (`core/engine/engine.hpp`). Nunca acessam `Scene` diretamente.
- `obs` é a cola: instancia Engine + Renderer por fonte e liga `video_tick`/`video_render`.

## Convenções

- Um tipo principal por arquivo; cabeçalho `.hpp`, implementação `.cpp`.
- Erros esperados (template inválido, comando inválido) retornam `Result<T, Error>`; não usam exceções.
- Tipos que guardam recursos gráficos têm destrutor que garante `obs_enter_graphics()` quando necessário.
