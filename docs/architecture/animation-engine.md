# Motor de animação

Implementado em `src/core/anim/`. Sem dependência de OBS: recebe `dt` e escreve propriedades.

## Conceitos

- **Clip**: animação nomeada. Contém `tracks`, `triggers`, `markers`, `loop`, `duration` opcional.
- **Track**: anima uma propriedade de uma layer (`from/to` ou `keys`) com `start`, `dur`, `ease`.
- **Trigger**: dentro de um clip, inicia outro clip (`at`, `on: end`, `on: marker:<nome>`).
- **ClipInstance**: execução de um clip em andamento (tempo local, velocidade, pai, triggers pendentes).

## Tempo

- Fonte de tempo: `dt` = `seconds` recebido em `video_tick`. Nada de relógio de parede.
- `dt` é limitado a `kMaxDt` (padrão 0,1 s) para evitar saltos após travamentos (ex.: arrastar a janela do OBS). Configurável.
- Tempo local de cada instância: `t += dt * speed`.
- Cada tick executa, nesta ordem:
  1. drenar a fila de comandos (inicia/para clips, aplica estados, `Set`);
  2. re-rasterizar layers sujas (ver [threading-model](threading-model.md));
  3. avançar instâncias e disparar triggers (com tempo residual);
  4. avaliar tracks e escrever valores animados;
  5. finalizar instâncias concluídas e emitir eventos.

## Avaliação de uma track

Seja `u = (t_clip - start) / dur`, `u` limitado a [0,1]:

- Antes de `start`: a track ainda não atua (propriedade mantém base/outra track).
- `from/to`: `valor = lerp(from, to, ease(u))`. Para cores, interpolação por canal (em espaço linear, opção futura); para booleanos, troca em `u >= 1` ou conforme `step`.
- `keys`: lista `[tempo, valor]` com tempo **relativo ao `start` da track** (em segundos); interpola entre chaves vizinhas usando o `ease` da track em cada segmento.
- `from` omitido: usa o **valor efetivo no instante em que a track começa** (captura na ativação). Isso dá continuidade quando um clip interrompe outro.
- Depois de `start + dur`: valor final; ao terminar o clip aplica-se `onEnd`.

### `onEnd`

| Valor | Efeito |
|---|---|
| `hold` (padrão) | grava o valor final na base da propriedade |
| `revert` | volta ao valor base anterior ao clip |

## Easings

Nomes suportados: `linear`, `inQuad`, `outQuad`, `inOutQuad`, `inCubic`, `outCubic`, `inOutCubic`, `inSine`, `outSine`, `inOutSine`, `inExpo`, `outExpo`, `inOutExpo`, `inBack`, `outBack`, `inOutBack`, `outElastic`, `outBounce`. Também `[x1, y1, x2, y2]` (cubic-bezier CSS). Padrão: `linear`.

## Triggers

```json
{ "play": "linha1mask.in", "at": 0.15, "mode": "linked", "speed": "inherit" }
```

| Campo | Padrão | Descrição |
|---|---|---|
| `play` | — | clip a iniciar (obrigatório) |
| `at` | `0` | segundos após o início do pai |
| `on` | — | alternativa a `at`: `"end"` ou `"marker:<nome>"` |
| `delay` | `0` | atraso extra após o evento/instante |
| `mode` | `linked` | `linked` acompanha o pai; `detached` é independente |
| `speed` | `1` | número, ou `"inherit"` para acompanhar a velocidade do pai |

### Semântica

- **Tempo residual**: se o trigger vence no meio do tick, o filho inicia com `t = t_clip_pai - instante_do_trigger`, no **mesmo** tick. Mantém sincronia exata independente do dt.
- **linked**: se o pai for parado/cancelado, o filho também é; o pai só é considerado *concluído* quando suas tracks e todos os filhos linked concluírem.
- **detached**: o filho segue sozinho; não afeta a conclusão do pai.
- Clip sem `tracks` e só com `triggers` é uma **coreografia** (ex.: `main.in`).
- Reentrância: iniciar um clip que já está rodando reinicia a instância (comportamento `restart`). Política alternativa por clip: `"retrigger": "ignore" | "restart" | "stack"`.

## Loop

`loop: true` exige `duration`. A instância volta a `t = 0` ao atingir `duration` (sem acumular erro: `t = t - duration`). Termina apenas com `stop`.

## Markers

`"markers": { "impacto": 0.4 }` — pontos nomeados no tempo do clip; disparam triggers `on: "marker:impacto"` e emitem evento opcional.

## Interrupção

- `stop` sobre um clip: cancela a instância e seus filhos `linked`; valores animados são removidos e prevalece a base (ou o que `hold` já gravou).
- Iniciar `out` durante `in`: tracks do `out` capturam o valor atual (via `from` omitido), garantindo continuidade; filhos linked do `in` são cancelados.

## Estados e ações

- `set` aplica um `state` (grava base). Pode ser usado antes/depois de clips.
- `actions` são listas de passos executados em ordem no tick em que a ação chega; `delay` agenda o passo relativo a esse instante (usando o mesmo relógio do tick).

## Validação

Feita na carga (ver [manifest/validation](../manifest/validation.md)): referências a clips/layers/propriedades, **ciclos de triggers**, markers inexistentes, `from/to` com tipos incompatíveis, loop sem duração. Limites: profundidade máxima de triggers e número máximo de instâncias simultâneas.

## Testes

- Unitários: easings (valores conhecidos), keyframes, tempo residual, ordem de triggers, interrupção, loop.
- *Golden*: avaliar `template/` a 60 fps por N quadros e comparar propriedades-chave com arquivos em `tests/fixtures/golden/`.
