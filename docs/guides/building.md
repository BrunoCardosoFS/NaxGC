# Compilando

O projeto utiliza a estrutura de build do [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) com presets definidos em [`CMakePresets.json`](../../CMakePresets.json).

## Pré-requisitos por plataforma

| Plataforma | Ferramentas |
|---|---|
| Windows | Visual Studio 2022 ou 2026 (workload C++ e Windows 10/11 SDK), CMake 3.28+, PowerShell |
| macOS | Xcode 16+, CMake 3.28+ |
| Linux | GCC/Clang, CMake 3.28+, Ninja, pacotes de desenvolvimento do sistema |

As dependências do OBS (`libobs`, `obs-frontend-api`, `obs-deps`) e do `Qt6` são baixadas automaticamente em `.deps/` na primeira configuração do CMake conforme o [`buildspec.json`](../../buildspec.json).

## Configurar e compilar

```bash
cmake --preset <preset>          # windows-x64 | macos | ubuntu-x86_64
cmake --build --preset <preset>
```

> **Dica (Windows com Visual Studio 2026):** utilize o CMake incluso no Visual Studio (`C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`) ou CMake 4.x para suporte ao gerador `Visual Studio 18 2026`.

## Dependências do projeto

| Dependência | Como entra |
|---|---|
| libobs / obs-frontend-api / Qt 6 | `buildspec.json` (template) |
| `obs-websocket-api.h` | copiado em `third_party/obs-websocket/` |
| nlohmann/json | `FetchContent` ou `third_party/` (versão fixada, a partir do M2) |
| lunasvg | `FetchContent` ou `third_party/` (versão fixada, a partir do M3) |
| doctest/Catch2 | somente testes (a partir do M2) |

Toda dependência nova precisa de justificativa (ver [AGENTS.md](../../AGENTS.md)).

## Opções CMake

| Opção | Efeito |
|---|---|
| `ENABLE_FRONTEND_API` | dock, hotkeys, eventos do frontend (`ON` por padrão) |
| `ENABLE_QT` | UI e rasterização de texto (`ON` por padrão) |
| `NAXGC_BUILD_TESTS` | testes do `core` sem OBS (a partir do M2) |

## Instalar no OBS para testar

- **Windows:** o OBS escaneia dois locais com estruturas diferentes:
  1. **Pasta global de plugins externos (`%ProgramData%\obs-studio\plugins\`)** — compatível com o `.zip` gerado pelo CI e com `cmake --install`:
     ```powershell
     cmake --install build_x64 --prefix "C:/ProgramData/obs-studio/plugins" --config RelWithDebInfo
     ```
     Extrai/instala a pasta `naxgc` inteira em `C:\ProgramData\obs-studio\plugins`.
- **Linux/macOS:** pasta de plugins do usuário (`~/.config/obs-studio/plugins/…`; macOS: `~/Library/Application Support/obs-studio/plugins/…`).
- Reinicie o OBS e confira em **Ajuda → Arquivos de Log → Ver Log Atual** a mensagem `[NaxGC] plugin loaded successfully (version 0.1.0)`.

## Formatação e `.gitignore` (CI)

- **Formatação obrigatória no CI (`check-format.yaml`):**
  - Código C/C++ em `src/`: formatado com `clang-format` (`-style=file -fallback-style=none`). Headers de terceiros devem ficar em `third_party/` (fora de `src/` e adicionados com `SYSTEM` em `target_include_directories`).
  - Arquivos CMake (`CMakeLists.txt`, `cmake/**/*.cmake`): validados pelo `gersemi` (indentação de 2 espaços, máximo de 120 colunas por linha).
- **`.gitignore` em modo *allowlist*:** o `.gitignore` inicia com `/*` (ignora tudo por padrão). Qualquer nova pasta ou arquivo na raiz do repositório precisa de uma regra `!/nome` explícita no `.gitignore`. Se a pasta `data/` for adicionada ou removida, execute `cmake --preset <preset>` novamente para regenerar os passos de post-build.

## Problemas comuns

- *Módulo não carrega*: versão do OBS/Qt incompatível (requer OBS 30.0+); veja o log do OBS.
- *Faltam símbolos do frontend*: `ENABLE_FRONTEND_API` desligado.
- *Fontes não aparecem*: verifique caminho em `fonts` e a licença/arquivo presente.
