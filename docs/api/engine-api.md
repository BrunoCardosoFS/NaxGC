# API interna do Engine

Superfície única usada por **painel**, **WebSocket**, **hotkeys** e **hot reload**. Definida em `src/core/engine/engine.hpp`. Nada fora do Engine deve tocar a Scene.

## Princípios

- **Assíncrona por construção**: métodos enfileiram `Command`s; o efeito ocorre no próximo `video_tick`.
- **Thread-safe**: pode ser chamada de qualquer thread (UI, WebSocket, worker).
- **Resultados**: validação síncrona *barata* (chave existe? tipo bate?) retorna `Result`; a execução em si é assíncrona e reporta por eventos.
- Sem exceções; erros como `Result<void, Error>`.

## Interface (esboço)

```cpp
namespace naxgc {

struct Error { std::string code; std::string message; };      // ex.: "NXG-R001"
template <class T> using Result = /* expected-like */;

class Engine {
public:
    // --- Template ---
    Result<void> load_template(const std::filesystem::path &dir);   // assíncrono (worker)
    TemplateInfo template_info() const;                              // snapshot

    // --- Valores / propriedades ---
    Result<void> set_control(std::string_view key, Value v);         // por chave do controle
    Result<void> set_property(std::string_view path, Value v);       // "layers.x.text"
    Result<void> set_many(std::span<const KV> items);                // atômico no tick
    Result<void> apply_state(std::string_view name);

    // --- Animação ---
    Result<void> run_action(std::string_view name);                  // "show", "hide"...
    Result<void> play(std::string_view clip);
    Result<void> stop(std::string_view clip);

    // --- Leitura ---
    std::shared_ptr<const Snapshot> snapshot() const;                // imutável

    // --- Eventos ---
    void subscribe(EventSink *sink);                                 // chamado fora do render
    void unsubscribe(EventSink *sink);

    // --- Loop (somente obs/) ---
    void tick(float seconds);                                        // drena fila + anima
};

}
```

## `Value`

`std::variant<bool, double, std::string, Color>`. A conversão para o tipo da propriedade segue as regras de [scene-and-properties](../architecture/scene-and-properties.md); falha → `NXG-R002`.

## Snapshot

```cpp
struct Snapshot {
    std::string template_id;
    std::unordered_map<std::string, Value> controls;   // key → valor atual
    std::vector<std::string> active_clips;
    uint64_t tick_index;
};
```

Publicado por `std::atomic<std::shared_ptr<...>>` no fim de cada tick.

## Eventos

| Evento | Quando | Dados |
|---|---|---|
| `StateChanged` | Valor de controle/propriedade-base mudou | `key`/`path`, `value`, origem (`ui`, `ws`, `action`, `state`) |
| `ClipStarted` | Instância criada | `clip`, `parent?` |
| `ClipFinished` | Instância concluída/cancelada | `clip`, `reason` (`end`, `stopped`) |
| `TemplateLoaded` | Troca concluída | `template_id`, avisos |
| `TemplateError` | Falha ao carregar | lista de erros `NXG-*` |

Entregues após o tick, nunca de dentro de `video_render`. Cada consumidor decide como mudar de thread (Qt: queued; WS: emit event).

## Códigos de erro de runtime

| Código | Significado |
|---|---|
| `NXG-R001` | Chave/caminho inexistente |
| `NXG-R002` | Valor incompatível com a propriedade |
| `NXG-R003` | Ação/clip/state inexistente |
| `NXG-R004` | Nenhum template carregado |
| `NXG-R005` | Fila cheia (comando descartado) |
| `NXG-R006` | Template em recarga; tente novamente |

## Garantias de ordem

Comandos de um mesmo chamador executam na ordem de envio. Comandos de chamadores diferentes: ordem de chegada à fila. Um `set_many` é aplicado atomicamente (nenhum quadro vê metade).

## Testes

O Engine é testável sem OBS: um *driver de teste* chama `tick(1/60.f)` N vezes e inspeciona `snapshot()` e as propriedades da Scene.
