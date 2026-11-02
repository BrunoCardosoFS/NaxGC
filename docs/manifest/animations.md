# Animações (clips)

`animations` é um mapa `nome → clip`. Semântica de execução em [animation-engine](../architecture/animation-engine.md).

## Clip

| Campo | Tipo | Padrão | Descrição |
|---|---|---|---|
| `tracks` | array | `[]` | Animações de propriedades |
| `triggers` | array | `[]` | Clips iniciados por este |
| `markers` | objeto | `{}` | `nome → segundos` |
| `duration` | número | calculada | Duração do clip. Obrigatória com `loop` |
| `loop` | booleano | `false` | Repete até `stop` |
| `onEnd` | `hold` \| `revert` | `hold` | O que fazer com os valores finais |
| `retrigger` | `restart` \| `ignore` \| `stack` | `restart` | Se o clip já estiver rodando |

**Duração calculada**: maior `start + dur` entre as tracks; o clip só é considerado concluído quando os filhos `linked` também concluem.

## Track

```json
{ "layer": "linha1", "prop": "x", "from": -1200, "to": 0, "start": 0, "dur": 0.6, "ease": "outCubic" }
```

| Campo | Tipo | Padrão | Descrição |
|---|---|---|---|
| `layer` | string | — | Nome da layer |
| `prop` | string | — | Propriedade animável |
| `from` | valor | valor efetivo atual | Início (omitir captura o valor ao ativar) |
| `to` | valor | — | Fim (com `from/to`) |
| `keys` | `[[t, v], ...]` | — | Alternativa a `from/to`; `t` em segundos relativos ao `start` |
| `start` | número | `0` | Atraso dentro do clip |
| `dur` | número | — | Duração (obrigatória com `from/to`; com `keys` é o último `t`) |
| `ease` | easing | `linear` | Easing (aplicado por segmento em `keys`) |
| `blend` | `override` \| `add` | `override` | Como combinar com outras tracks |

Regras:
- Use **ou** `from/to` **ou** `keys`.
- Tipos: números (`x`, `opacity`…), cores (`fill`…), booleanos (`visible`, troca no fim), strings (`text`, troca imediata).
- `x`/`y` são deslocamentos do repouso (ver [scene-and-properties](../architecture/scene-and-properties.md)).

## Easing

Nome (`outCubic`, `inOutSine`, `outBack`, …) ou cubic-bezier `[x1, y1, x2, y2]`. Lista completa no documento do motor.

## Triggers

```json
{ "play": "linha1mask.in", "at": 0.15, "mode": "linked" }
```

| Campo | Padrão | Descrição |
|---|---|---|
| `play` | — | Clip a iniciar |
| `at` | `0` | Segundos após o início do clip pai |
| `on` | — | `"end"` ou `"marker:<nome>"` (em vez de `at`) |
| `delay` | `0` | Atraso extra |
| `mode` | `linked` | `linked` \| `detached` |
| `speed` | `1` | Número ou `"inherit"` |

### Padrão: barra + máscara + borda

```json
"linha1.in": {
  "tracks":   [{ "layer": "linha1", "prop": "x", "from": -1200, "to": 0, "dur": 0.6, "ease": "outCubic" }],
  "triggers": [
    { "play": "linha1border.in", "at": 0.0,  "mode": "linked" },
    { "play": "linha1mask.in",   "at": 0.15, "mode": "linked" }
  ]
}
```

Iniciar `linha1.in` inicia sozinho a borda e a máscara; parar `linha1.in` para os três.

### Padrão: coreografia

Clip sem tracks, só com triggers (`main.in` no template) para orquestrar a entrada de várias linhas, logo e tag.

### Padrão: loop contínuo

```json
"loop_vivo": { "loop": true, "duration": 2.0,
  "tracks": [{ "layer": "tag_aovivo_dot", "prop": "opacity", "keys": [[0,1],[1,0.35],[2,1]], "ease": "inOutSine" }] }
```

Iniciado e parado por `actions`.

## Restrições validadas na carga

- Layer/propriedade existem e são compatíveis; tipos de `from/to/keys` batem com a propriedade.
- Sem **ciclos** de triggers.
- `loop` exige `duration`; `keys` ordenadas por tempo.
- `on: "marker:x"` exige o marker no clip pai.

## Futuro (backlog)

`visibleWhen` e `drive` (animação reativa a estado/velocidade), expressões, e tracks de `text` com efeito *typewriter*.
