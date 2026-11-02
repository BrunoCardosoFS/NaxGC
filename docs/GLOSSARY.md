# Glossário

| Termo | Significado |
|---|---|
| **GC** | Gerador de caracteres: gráficos sobrepostos ao vídeo (lower thirds, tags, créditos). |
| **Lower third** | GC no terço inferior da tela (nome/cargo do entrevistado). |
| **Template** | Pasta (ou `.zip`) com `manifest.json`, `layout.svg` e assets que descrevem um GC completo. |
| **Manifest** | Arquivo `manifest.json` do template: layers, painel, estados, animações e ações. |
| **Layer** | Unidade desenhável/animável ligada a um elemento do SVG (`svg`), a um quadro de texto (`text`) ou a uma imagem. |
| **Layer de texto** | Layer que renderiza texto dinâmico dentro do retângulo `txt_*` do SVG. |
| **Máscara** | Layer com `role: "mask"`; define a região visível de outra layer (via alfa). Não é desenhada diretamente. |
| **Propriedade** | Atributo de uma layer que pode ser alterado: `x`, `y`, `scale`, `rotation`, `opacity`, `visible`, `fill`, `stroke`, `text`, `color`… |
| **Bind** | Ligação de um controle do painel a uma ou mais propriedades (`layers.<nome>.<prop>`). |
| **Controle** | Elemento do painel (texto, cor, toggle, dropdown, botão…). |
| **Painel / dock** | Janela acoplável do OBS (Qt) gerada a partir de `panel` no manifest. |
| **Clip** | Animação nomeada: conjunto de tracks e triggers com duração própria. |
| **Track** | Animação de uma propriedade de uma layer dentro de um clip (`from/to` ou `keys`). |
| **Keyframes (`keys`)** | Lista `[tempo, valor]` que define uma track por pontos. |
| **Trigger** | Regra dentro de um clip que inicia outro clip (`at`, `on: end`, `on: marker:x`). |
| **Linked / detached** | Modo do filho de um trigger: acompanha (linked) ou não (detached) o ciclo de vida do pai. |
| **Marker** | Ponto nomeado no tempo de um clip, usado como gatilho. |
| **Action** | Sequência de passos (`play`, `stop`, `set`) disparada por botão, atalho ou WebSocket. |
| **State** | Conjunto nomeado de valores de propriedades aplicável de uma vez (ex.: `offair`, temas). |
| **Off-air** | Estado em que o GC está fora do ar (tudo escondido/fora da tela). |
| **Engine** | Núcleo do plugin por instância: cena + animator + fila de comandos. API única usada por painel e WebSocket. |
| **Command** | Mensagem enfileirada (set, action, play…) consumida no `video_tick`. |
| **Vendor (obs-websocket)** | Namespace de requests/eventos customizados registrado por um plugin (`naxgc`). |
| **`video_tick` / `video_render`** | Callbacks da fonte no OBS: atualização lógica por quadro e desenho na thread gráfica. |
| **Texrender** | `gs_texrender_t`: textura alvo de renderização usada para compor máscaras. |
| **Z-order** | Ordem de empilhamento; no NaxGC vem da ordem dos elementos no SVG. |
| **ADR** | *Architecture Decision Record*: registro curto de uma decisão e seu contexto. |
