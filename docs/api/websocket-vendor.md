# API WebSocket (vendor `naxgc`)

O NaxGC usa a **vendor API** do obs-websocket 5.x: requests chegam via `CallVendorRequest` com `vendorName: "naxgc"`, e eventos saem como `VendorEvent`. Tudo converge para a [API do Engine](engine-api.md).

## Registro

- Header `obs-websocket-api.h` (copiado para o projeto).
- `obs_websocket_register_vendor("naxgc")` deve ser chamado **após** `OBS_FRONTEND_EVENT_FINISHED_LOADING` (o obs-websocket só está pronto depois).
- Requests registrados com `obs_websocket_vendor_register_request`; eventos emitidos com `obs_websocket_vendor_emit_event`.
- Callbacks rodam na thread do WebSocket → só enfileiram comandos (ver [threading-model](../architecture/threading-model.md)).

## Convenções

- `source`: nome da fonte NaxGC alvo. **Opcional** se houver apenas uma instância; obrigatório (erro `NXG-R010`) se houver várias.
- Nomes de requests em `snake_case`.
- Resposta (`responseData`): `{ "ok": true, ... }` ou `{ "ok": false, "error": { "code": "NXG-R001", "message": "..." } }`.
- Requests são **assíncronas quanto ao efeito**: `ok: true` significa "aceito e enfileirado"; o resultado real chega por eventos.

## Exemplo de chamada (obs-websocket 5.x)

```json
{
  "op": 6,
  "d": {
    "requestType": "CallVendorRequest",
    "requestId": "abc-1",
    "requestData": {
      "vendorName": "naxgc",
      "requestType": "set",
      "requestData": { "source": "NaxGC Lower Third", "key": "linha1", "value": "Maria Souza" }
    }
  }
}
```

## Requests

### `get_state`
Retorna o snapshot.
```json
{ "source": "…" }
→ { "ok": true, "template": { "id": "naxgc.default.lower-third", "name": "…" },
    "controls": { "linha1": "Maria", "aovivo": true }, "activeClips": ["loop_vivo"] }
```

### `set`
Define o valor de **um controle** (`key`) ou de **uma propriedade** (`path`).
```json
{ "source": "…", "key": "linha1", "value": "Maria Souza" }
{ "source": "…", "path": "layers.linha1.fill", "value": "#b00020" }
```

### `set_many`
Aplica vários valores atomicamente.
```json
{ "source": "…", "items": [ { "key": "linha1", "value": "Maria" }, { "key": "linha2", "value": "Repórter" } ] }
```

### `action`
Executa uma ação do manifest.
```json
{ "source": "…", "name": "show" }
```

### `take`
Atalho comum: aplica valores e executa uma ação **no mesmo tick**.
```json
{ "source": "…", "items": [ { "key": "linha1", "value": "Maria" }, { "key": "linha2", "value": "Repórter" } ], "action": "show" }
```
Garante que o primeiro quadro da animação já usa o texto novo.

### `play` / `stop`
```json
{ "source": "…", "clip": "linha1.in" }
```

### `apply_state`
```json
{ "source": "…", "state": "tema_vermelho" }
```

### `load_template`
```json
{ "source": "…", "path": "C:/NaxGC/templates/jornal" }
```
Resposta imediata `ok`; resultado por `template_loaded`/`template_error`. Caminhos fora da biblioteca de templates configurada podem ser recusados (`NXG-R020`).

### `list_templates`
Lista templates da biblioteca configurada: `{ "templates": [ { "id", "name", "path" } ] }`.

### `list_sources`
Lista as fontes NaxGC existentes.

## Eventos (`VendorEvent`, `vendorName: "naxgc"`)

| `eventType` | `eventData` |
|---|---|
| `state_changed` | `{ source, key?, path?, value, origin }` |
| `clip_started` | `{ source, clip, parent? }` |
| `clip_finished` | `{ source, clip, reason }` |
| `template_loaded` | `{ source, id, warnings[] }` |
| `template_error` | `{ source, errors[] }` |

Eventos são entregues após o tick; não há garantia de entrega quadro a quadro (use para UI/estado, não para sincronia de animação).

## Erros

`NXG-R001` chave/caminho inexistente · `NXG-R002` valor incompatível · `NXG-R003` ação/clip/state inexistente · `NXG-R004` sem template · `NXG-R005` fila cheia · `NXG-R010` `source` ausente/ambígua · `NXG-R020` caminho de template não permitido.

## Integrações previstas

- **Bitfocus Companion / Stream Deck**: via módulo obs-websocket (`CallVendorRequest`).
- **Scripts**: `obs-websocket-js` (Node), `obsws-python`.
- Exemplos prontos entram no M7.

## Segurança

- Autenticação e porta são do obs-websocket (senha configurada pelo usuário). O plugin não abre portas próprias.
- Nenhuma resposta inclui caminhos absolutos do sistema do usuário além do que ele mesmo enviou.
