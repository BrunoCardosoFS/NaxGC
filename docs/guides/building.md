# Compilando

> Estado: pré-código. Esta página descreve o fluxo previsto com base no [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate). Confirme nomes de presets em `CMakePresets.json` quando o repositório for criado (M0).

## Pré-requisitos por plataforma

| Plataforma | Ferramentas |
|---|---|
| Windows | Visual Studio 2022 (workload C++), CMake, PowerShell |
| macOS | Xcode, CMake |
| Linux | GCC/Clang, CMake, Ninja, pacotes de desenvolvimento do sistema |

As dependências do OBS e do Qt são baixadas automaticamente pelo `buildspec.json` do template.

## Configurar e compilar

```bash
cmake --preset <preset>          # windows-x64 | macos | ubuntu-x86_64
cmake --build --preset <preset>
```

## Dependências do projeto

| Dependência | Como entra |
|---|---|
| libobs / obs-frontend-api / Qt | `buildspec.json` (template) |
| `obs-websocket-api.h` | copiado em `third_party/obs-websocket/` |
| nlohmann/json | `FetchContent` ou `third_party/` (versão fixada) |
| lunasvg | `FetchContent` ou `third_party/` (versão fixada) |
| doctest/Catch2 | somente testes |

Toda dependência nova precisa de justificativa (ver [AGENTS.md](../../AGENTS.md)).

## Opções CMake previstas

| Opção | Efeito |
|---|---|
| `ENABLE_FRONTEND_API` | dock, hotkeys, eventos do frontend |
| `ENABLE_QT` | UI e rasterização de texto |
| `NAXGC_BUILD_TESTS` | testes do `core` (sem OBS) |

## Instalar no OBS para testar

- Windows: copiar a DLL e `data/` para a pasta de plugins do OBS (ou usar o alvo `install` do preset).
- Linux/macOS: pasta de plugins do usuário (`~/.config/obs-studio/plugins/…`; macOS: `~/Library/Application Support/obs-studio/plugins/…`).
- Reinicie o OBS e confira o log.

## Problemas comuns

- *Módulo não carrega*: versão do OBS/Qt incompatível; veja o log do OBS.
- *Faltam símbolos do frontend*: `ENABLE_FRONTEND_API` desligado.
- *Fontes não aparecem*: verifique caminho em `fonts` e a licença/arquivo presente.
