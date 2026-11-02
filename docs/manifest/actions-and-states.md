# Ações e estados

## States

Snapshots nomeados de propriedades: `states.<nome>` é um mapa `caminho → valor`.

```json
"states": {
  "offair": { "layers.linha1.x": -1200, "layers.logo.scale": 0 },
  "tema_vermelho": { "layers.linha1.fill": "#b00020" }
}
```

- Aplicar um state grava a **base** de cada propriedade listada.
- `initialState` (topo do manifest) é aplicado ao carregar o template.
- Usos típicos: pose **fora do ar**, **temas** (via `dropdown.apply`), variantes de layout.
- Valores devem ser compatíveis com o tipo da propriedade.

> O estado `offair` mantém as layers escondidas antes da primeira entrada. Depois de `out`, os valores finais dos clips (com `onEnd: hold`) já coincidem com a pose fora do ar; aplicar `offair` novamente é seguro (reset).

## Actions

Sequências de passos executadas, em ordem, no tick em que a ação chega (via botão, atalho, WebSocket ou `submitAction`).

```json
"actions": {
  "show": [ { "play": "main.in" }, { "play": "loop_vivo", "delay": 0.8 } ],
  "hide": [ { "play": "main.out" }, { "stop": "loop_vivo", "delay": 0.9 } ]
}
```

### Passos

| Passo | Campos | Descrição |
|---|---|---|
| play | `play: <clip>`, `delay?` | Inicia o clip |
| stop | `stop: <clip>`, `delay?` | Para o clip (e filhos `linked`) |
| set | `set: <state>`, `delay?` | Aplica um state |
| call | `call: <action>`, `delay?` | Executa outra ação (sem ciclos) |

`delay` (segundos) é relativo ao instante em que a ação foi executada e usa o mesmo relógio do tick (determinístico).

### Ações reservadas (por convenção)

| Nome | Uso |
|---|---|
| `show` | Entrar no ar |
| `hide` | Sair do ar |

Não são obrigatórias, mas painel e documentação assumem esses nomes nos exemplos. Qualquer nome é permitido (`show_urgente`, `reset`…).

## Padrões úteis

- **Reset duro**: `{ "set": "offair" }` como ação `reset`.
- **Trocar texto no ar**: hoje a troca é seca (o `Set` do texto vale no mesmo tick). Para uma transição suave, crie uma ação que toque um clip de saída do texto, aguarde (`delay`), e só então envie o novo texto pelo painel/WebSocket. Suporte declarativo (`onChange`) está no backlog.
- **Auto-saída**: `{ "play": "main.out", "delay": 8.0 }` ao final de `show` retira o GC após 8 s (verifique se o `hide` manual deve cancelar: use `stop` do clip correspondente).

## Validação

`NXG-E040` (clip/state/action inexistente num passo), `NXG-E041` (ciclo de `call`). Ver [validation](validation.md).
