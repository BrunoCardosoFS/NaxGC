# Roadmap

Legenda: `[ ]` pendente · `[~]` em andamento · `[x]` concluído.
Cada marco tem **critério de aceite**; só marque como concluído quando ele for atendido.

Ordem pensada para validar o maior risco primeiro (fluidez a 60 fps) e só depois construir em cima.

## M0 — Fundação

- [x] Repositório criado a partir do obs-plugintemplate (nome, UUID e metadados no `buildspec.json`)
- [x] Qt e frontend API habilitados no CMake
- [x] `obs-websocket-api.h` incorporado ao projeto
- [~] CI compilando
- [x] Definir versão mínima do OBS suportada (OBS 30.0+, somente Qt6)

**Aceite:** plugin "vazio" compila e aparece na lista de módulos do OBS.

## M1 - Fonte nativa e prova de fluidez

- [ ] `obs_source_info` registrada (`naxgc_source`), com `video_tick`/`video_render`
- [ ] Retângulo animado desenhado com matrizes (`gs_matrix_*`) guiado por `seconds`
- [ ] Propriedades da fonte: pasta do template, largura/altura e opção para resetar animação ao reexibir (desativada por padrão)
- [x] Decidir e documentar comportamento de tick quando a fonte está oculta (`show`/`hide`)

**Aceite:** animação a 60 fps por 10 min sem *missed/skipped/lagged frames* atribuíveis ao plugin (Estatísticas do OBS).

## M2 - Manifest: carregamento e validação

- [ ] Parser (nlohmann/json) com modelo tipado no `core`
- [ ] Validação semântica com códigos de erro (ver `manifest/validation.md`)
- [ ] JSON Schema publicado em `schemas/manifest.schema.json`
- [ ] Carregamento seguro do pacote (caminhos, limites de tamanho)
- [ ] Testes com fixtures válidas/inválidas; `template/` valida sem avisos

**Aceite:** manifest inválido nunca derruba o OBS; erro legível no log e na UI.

## M3 - Rasterização e cena

- [ ] Cena com layers, hierarquia (pai/filho) e propriedades tipadas
- [ ] Rasterização de layers `svg` por `id` com lunasvg (bbox justo, cache e escala híbrida: detecção automática da escala máxima nos clips + override manual via `rasterScale`)
- [ ] Layers `text` rasterizadas com Qt (fonte do pacote, overflow `shrink`/`clip`/`ellipsis`/`wrap`)
- [ ] Desenho em z-order do SVG com opacidade e matrizes herdadas
- [ ] Re-rasterização só quando conteúdo muda (texto, cor)

**Aceite:** `template/` renderiza estático e fiel ao `layout.svg` em 1080p.

## M4 - Animator

- [ ] Clips, tracks `from/to` e `keys`, easings (incl. cubic-bezier)
- [ ] Triggers (`at`, `on: end`, `on: marker:*`), modos `linked`/`detached`
- [ ] Tempo residual ao disparar filhos no mesmo quadro
- [ ] Interrupção (`from` omitido = valor atual), `onEnd: hold|revert`
- [ ] Estados nomeados (`states`) e `initialState`
- [ ] Ações (`actions`) com `play`, `stop`, `set`, `delay`
- [ ] Testes de golden values (avaliação de animação por quadro, sem OBS)

**Aceite:** `main.in`/`main.out` do template reproduzem a coreografia esperada; testes do core passando.

## M5 - Máscaras

- [ ] `gs_texrender` para layer mascarada e para a máscara
- [ ] Effect (`.effect`) de composição (`alpha` e `inverse`)
- [ ] Pool de texrender para evitar alocação por quadro

**Aceite:** texto do template é revelado pela máscara; sem custo perceptível em frame time.

## M6 - Painel de controle (dock Qt)

- [ ] Dock com seletor de instância (fonte NaxGC alvo)
- [ ] Factory de controles: `text`, `number`, `toggle`, `color`, `dropdown`, `button`, `label`
- [ ] Seções (colapsáveis) e reconstrução ao trocar/recarregar template
- [ ] Persistência dos valores por template nas settings da fonte
- [ ] Hotkeys do OBS registradas para botões com `hotkey`

**Aceite:** painel do `template/` aparece e controla a cena; zero código específico do template no C++.

## M7 - Controle via obs-websocket

- [ ] Registro do vendor `naxgc` após `OBS_FRONTEND_EVENT_FINISHED_LOADING`
- [ ] Requests: `get_state`, `set`, `set_many`, `action`, `play`, `stop`, `load_template`
- [ ] Eventos: `state_changed`, `clip_started`, `clip_finished`, `template_loaded`, `error`
- [ ] Painel reflete mudanças vindas do WebSocket
- [ ] Documentação de exemplos (Companion, script, `obs-websocket-js`)

**Aceite:** mesmo resultado ao acionar pelo painel ou pelo WebSocket, incluindo atualização do painel.

## M8 - Experiência de autoria

- [ ] Hot reload (`QFileSystemWatcher`) preservando valores dos controles
- [ ] Overlay de debug (bounding boxes, ids, fps do animator)
- [ ] Mensagens de erro com linha/caminho do JSON
- [ ] Guia de autoria completo e segundo template de exemplo

**Aceite:** editar o JSON/SVG e ver o resultado no OBS em menos de 1 s, sem reiniciar.

## M9 - Release 0.1

- [ ] Empacotamento (zip/instalador por plataforma)
- [ ] Licença definida
- [ ] Revisão de segurança do carregamento de templates
- [ ] Changelog e documentação de instalação

**Aceite:** instalação limpa em máquina sem ambiente de desenvolvimento.

## Backlog (pós 0.1)

- Condições reativas: `visibleWhen`, `drive` (ex.: borda ligada à velocidade da layer) — definir sintaxe final no manifest quando este item for iniciado
- Tipo `list` (placares, crawler/ticker, múltiplas linhas repetidas)
- Layers de imagem raster e vídeo
- Texto com rich text (negrito/cor parcial)
- Múltiplos templates por fonte e troca animada de template
- Expressões simples em `bind` (ex.: concatenar campos)
- Rasterização de texto em thread separada com gating por quadro
- Importador de animações (Lottie/SVG SMIL → clips)
- Biblioteca de templates e validação via CLI
