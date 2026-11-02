# Painel e controles

O painel (`panel`) descreve a UI que o plugin gera no dock. Implementação: [architecture/control-panel](../architecture/control-panel.md).

```json
"panel": {
  "title": "Lower Third",
  "sections": [
    { "title": "Conteúdo", "controls": [ ... ] },
    { "title": "Aparência", "collapsed": true, "controls": [ ... ] }
  ]
}
```

## `panel`

| Campo | Tipo | Descrição |
|---|---|---|
| `title` | string | Título exibido no cabeçalho |
| `sections` | array | Seções em ordem de exibição |

## Seção

| Campo | Tipo | Padrão | Descrição |
|---|---|---|---|
| `title` | string | — | Título |
| `collapsed` | booleano | `false` | Inicia recolhida |
| `columns` | inteiro | `1` | Colunas de layout (1–3) |
| `controls` | array | — | Controles da seção |

## Campos comuns de controle

| Campo | Tipo | Descrição |
|---|---|---|
| `type` | string | `text`, `number`, `toggle`, `color`, `dropdown`, `button`, `label`, `separator` |
| `key` | string | Identificador único do controle (obrigatório, exceto `button`, `label`, `separator`) |
| `label` | string | Rótulo |
| `bind` | string \| string[] | Propriedade(s) alvo (`layers.<nome>.<prop>`) |
| `default` | valor | Valor inicial |
| `tooltip` | string | Dica |

A `key` é usada pelo WebSocket (`set`) e para persistir valores.

## Tipos

### `text`
`placeholder`, `maxLength`, `multiline` (bool), `submitAction` (nome de ação executada ao pressionar Enter; padrão: nenhuma).

### `number`
`min`, `max`, `step`, `slider` (bool), `unit` (ex.: `"px"`).

### `toggle`
Booleano. Ex.: `bind: "layers.tag_aovivo.visible"`.

### `color`
Cor `#rrggbb[aa]`. `bind` pode ser lista para aplicar em várias propriedades:

```json
{ "key": "cor_borda", "type": "color", "default": "#ffd23f",
  "bind": ["layers.linha1border.stroke", "layers.linha2border.stroke"] }
```

### `dropdown`
Cada opção tem `label` e **um** destes:

| Campo | Efeito |
|---|---|
| `value` | valor atribuído ao `bind` |
| `apply` | nome de um `state` aplicado (temas) |

```json
{ "key": "tema", "type": "dropdown", "default": "tema_azul",
  "options": [ { "label": "Azul", "apply": "tema_azul" }, { "label": "Vermelho", "apply": "tema_vermelho" } ] }
```

### `button`
```json
{ "type": "button", "label": "▶ Entrar", "action": "show", "style": "primary", "hotkey": "NaxGC: Entrar" }
```

| Campo | Descrição |
|---|---|
| `action` | nome em `actions` (obrigatório) |
| `style` | `default` \| `primary` \| `danger` |
| `hotkey` | rótulo do atalho registrado no OBS (a tecla é do usuário) |

### `label` / `separator`
Texto estático e divisor visual.

## Boas práticas

- Ordem do manifest = ordem de tabulação.
- Agrupe por intenção: **Conteúdo** (textos), **Aparência** (cores/temas), **Exibição** (botões).
- Um controle de texto por linha de GC; evite `submitAction: "show"` salvo se for desejado ir ao ar com Enter.
- Use `maxLength` coerente com o quadro do texto para evitar `overflow` constante.

## Validação

`NXG-E030` (bind inválido), `NXG-E031` (`key` duplicada), `NXG-E032` (`action`/`apply` inexistente), `NXG-E033` (valor padrão incompatível com a propriedade). Ver [validation](validation.md).
