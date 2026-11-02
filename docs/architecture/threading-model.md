# Modelo de threads

## Threads envolvidas

| Thread | O que roda | Pode tocar em |
|---|---|---|
| **Gráfica do OBS** | `video_tick`, `video_render`, tudo de `gs_*` | Scene, Animator, Renderer, texturas |
| **UI (Qt)** | dock, diálogos, `QFileSystemWatcher` | apenas a API pública do Engine (enfileirar) e widgets |
| **WebSocket** | callbacks de requests do vendor `naxgc` | apenas a API pública do Engine (enfileirar) |
| **Worker de carga** | ler/validar template, ler fontes, parse de SVG | apenas dados próprios; entrega resultado por troca atômica |

> `video_tick` e `video_render` rodam na thread gráfica. Quando a fonte é ocultada e reexibida (`show`/`hide`), a cena e as animações **não resetam por padrão**; uma opção nas propriedades da fonte no OBS permite habilitar o reset para `initialState` ao reexibir.

## Regra de ouro

**A Scene só é modificada na thread gráfica, dentro do `video_tick`.** Todo o resto envia `Command`s.

## Fila de comandos

- Estrutura: fila multi-produtor/um-consumidor (mutex curto ou lock-free); capacidade limitada com política de descarte coalescendo `Set` repetidos da mesma chave.
- Consumo: no início do `video_tick`, antes de avançar o Animator.
- Tipos de comando: `Set(path, valor)`, `SetMany`, `RunAction(nome)`, `Play(clip)`, `Stop(clip)`, `ApplyState(nome)`, `LoadTemplate(path)`.
- **Ordem preservada por produtor**: dois comandos enviados em sequência (ex.: `Set texto` e depois `RunAction show`) executam nessa ordem **no mesmo tick** ou em ticks consecutivos, nunca invertidos.

## Garantia "texto pronto antes da animação"

Se um tick executa `Set texto` seguido de `RunAction show`:
1. `Set` marca a layer como suja.
2. `RunAction` inicia o clip (tempo local = `dt` do quadro).
3. Antes de avaliar/desenhar, o Engine **re-rasteriza as layers sujas**.
Assim o primeiro quadro da animação já mostra o texto novo. Se a re-rasterização for movida para worker no futuro, o Animator precisa de *gating* (esperar textura pronta antes de iniciar o clip) — ver backlog.

## Respostas e eventos para fora

- O Engine acumula **eventos** (`state_changed`, `clip_started`, `clip_finished`, `error`) em uma fila de saída durante o tick.
- Ao fim do tick, a fila é entregue por *dispatch* assíncrono: para a UI via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`; para o WebSocket via `obs_websocket_vendor_emit_event`.
- Nenhum callback externo é chamado de dentro de `video_render`.

## Leitura de estado (UI/WebSocket)

- `get_state` não lê a Scene viva. O Engine mantém um **snapshot imutável** (valores dos controles + clips ativos) atualizado ao fim de cada tick e publicado por `std::shared_ptr` atômico. Leitores pegam o ponteiro; sem locks longos.

## Troca de template

1. Worker lê e valida o pacote → produz `LoadedTemplate` (dados puros).
2. Entrega via `Command::LoadTemplate{shared_ptr<LoadedTemplate>}`.
3. No `video_tick`: Engine libera texturas antigas (`obs_enter_graphics`), cria as novas, aplica `initialState`, publica novo snapshot.
4. Falha em qualquer etapa mantém o template anterior.

## Hot reload

O watcher (thread Qt) detecta mudança → agenda recarga no worker → mesma troca acima, preservando valores atuais dos controles quando a chave ainda existe.

## Proibições em `video_tick` / `video_render`

- Leitura/escrita de disco, rede, `std::this_thread::sleep`, esperas em mutex potencialmente longas.
- Parse de JSON/SVG, validação de manifest.
- Alocações grandes por quadro (texrender e buffers vêm de pools).
