# Testes

## Estratégia

| Camada | Tipo | Precisa do OBS? |
|---|---|---|
| `core` (manifest, cena, animator, easing, comandos) | Unitários + *golden* | **Não** |
| `render` | Testes visuais offscreen (futuro) + checklist manual | Sim |
| `ui`, `ws` | Testes de integração leves (futuro) + checklist manual | Sim |
| Desempenho | Medição manual guiada (ver [debugging-and-performance](debugging-and-performance.md)) | Sim |

O objetivo é que **tudo o que é lógica** esteja no `core` e seja coberto sem OBS.

## Executando

```bash
ctest --preset <preset> --output-on-failure
```

## O que testar no `core`

### Manifest
- `tests/fixtures/manifests/valid/*.json` carregam sem erros.
- `tests/fixtures/manifests/invalid/*.json`: cada arquivo cobre **um** código (`NXG-E0xx`) e o teste confere o código e o caminho JSON reportados.
- `template/manifest.json` valida sem erros nem avisos (teste obrigatório; protege o exemplo).
- Caminhos inseguros (`..`, absolutos) são recusados.

### Easing e tracks
- Valores conhecidos em `u = 0, 0.25, 0.5, 1` para cada easing.
- `keys` com interpolação entre segmentos; `from` omitido capturando valor atual.

### Triggers e clips
- Tempo residual: trigger em `at: 0.3` com `dt` irregular (0.017, 0.1, 0.033) inicia o filho com tempo local correto.
- `linked` x `detached` ao parar o pai.
- Conclusão do pai espera filhos `linked`.
- Detecção de ciclos; limite de profundidade.
- Loop sem deriva (`t - duration`).
- Interrupção `out` durante `in`.

### Engine
- Ordem de comandos (`set` → `run_action`) no mesmo tick; `set_many` atômico.
- Coalescência de `set` repetidos na fila.
- Troca de template mantém o anterior em caso de erro.

### Golden
Avaliação do `template/` a 60 fps por N quadros: compara `x`, `opacity`, `scale` de layers-chave com `tests/fixtures/golden/*.json` (tolerância 1e-4). Regenerar golden só com justificativa no PR.

## Convenções

- Um arquivo de teste por unidade (`animator_test.cpp`, `manifest_test.cpp`…).
- Testes determinísticos: nunca use o relógio real; passe `dt` explícito.
- Nomeie casos pelo comportamento (`trigger_starts_child_with_residual_time`).

## Checklist manual antes de release (OBS real)

- [ ] 60 fps por 10 min sem *missed/skipped/lagged frames* do plugin
- [ ] Entrar/Sair repetidos 100× sem vazamento de memória/texturas
- [ ] Trocar texto durante a animação sem *hitch*
- [ ] Hot reload de template válido e inválido
- [ ] Duas instâncias simultâneas (painel + WebSocket com `source`)
- [ ] Fechar o OBS com GC no ar (sem crash)
- [ ] Windows, Linux e macOS
