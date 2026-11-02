# 0005 — Controle remoto via vendor API do obs-websocket
- Status: Aceita
- Data: 2026-11-02

## Contexto
O GC precisa ser controlado por Stream Deck, Companion e scripts, sem criar um servidor próprio.

## Decisão
Usar a **vendor API** do obs-websocket 5.x (`CallVendorRequest` / `VendorEvent`, vendor `naxgc`). Registro após `OBS_FRONTEND_EVENT_FINISHED_LOADING`. Requests apenas enfileiram comandos no Engine.

## Consequências
- (+) Reaproveita autenticação, porta e clientes existentes.
- (+) Integração direta com Companion/Stream Deck.
- (−) Dependência da disponibilidade do obs-websocket (incluso no OBS recente).
- (−) Callbacks vêm de outra thread → exigem a fila de comandos ([0007](0007-fila-de-comandos-e-engine-unico.md)).

## Alternativas consideradas
- Servidor HTTP/WebSocket próprio: mais superfície de segurança e configuração.
- Expor tudo via `SetInputSettings`: simples, porém ruim para comandos (entrar/sair) e eventos.
