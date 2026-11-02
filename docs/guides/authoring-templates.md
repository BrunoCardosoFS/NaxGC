# Criando templates

Um template é um pacote com SVG + manifest. Esta página é o passo a passo; a referência completa está em [`manifest/`](../manifest/specification.md). Use [`template/`](../../template/) como modelo.

## Fluxo

1. **Desenhe** o layout no Inkscape/Figma em um canvas do tamanho de saída (ex.: 1920×1080).
2. **Nomeie** o que vai se mover ou mudar com `id`s claros (ver [svg-conventions](svg-conventions.md)).
3. **Declare as layers** no manifest ligando nomes aos `id`s.
4. **Defina o painel** (textos, cores, botões) com `panel`.
5. **Crie os clips** de entrada/saída/loops e ligue-os com `triggers`.
6. **Defina `states`** (fora do ar, temas) e `actions` (`show`, `hide`).
7. **Teste** no OBS e itere (hot reload).

## Exemplo guiado: linha de texto com barra, máscara e borda

**No SVG**

```xml
<rect id="linha1" x="260" y="820" width="1100" height="84" fill="#0b2a5b"/>
<rect id="txt_linha1" x="292" y="820" width="1036" height="84" fill="none"/>
<rect id="linha1border" x="260" y="820" width="1100" height="84" fill="none" stroke="#ffd23f" stroke-width="4"/>
<g id="masks"><rect id="linha1mask" x="260" y="820" width="1100" height="84"/></g>
```

**No manifest**

```json
"layers": {
  "linha1":       { "type": "svg",  "id": "linha1" },
  "linha1border": { "type": "svg",  "id": "linha1border" },
  "linha1mask":   { "type": "svg",  "id": "linha1mask", "role": "mask" },
  "linha1texto":  { "type": "text", "id": "txt_linha1", "font": "Inter", "size": 46, "mask": "linha1mask" }
}
```

**Animação**: a barra entra; ao iniciar, dispara a borda (visível só durante o movimento) e a máscara (atrasada, revelando o texto).

```json
"linha1.in": {
  "tracks": [{ "layer": "linha1", "prop": "x", "from": -1200, "to": 0, "dur": 0.6, "ease": "outCubic" }],
  "triggers": [
    { "play": "linha1border.in", "at": 0.0,  "mode": "linked" },
    { "play": "linha1mask.in",   "at": 0.15, "mode": "linked" }
  ]
}
```

O texto fica parado; a máscara desliza e o revela.

## Dicas de design

- **Pose fora do ar em `states.offair`**: todas as layers escondidas/fora de quadro. É o `initialState`.
- **Entrada e saída simétricas**: use `from` omitido nas saídas para continuidade se o usuário sair no meio da entrada.
- **Escalas > 1** em layers: defina `rasterScale` para manter nitidez.
- **Texto**: defina `overflow: "shrink"` + `minSize` e `maxLength` no controle.
- **Coreografias**: crie `main.in`/`main.out` só com triggers; as ações ficam simples.
- **Loops**: curtos, com `duration` explícita, iniciados por `show` e parados por `hide`.
- **Cores por controle**: use `rect`/`path` direto como layer para `fill`/`stroke` funcionarem como esperado.

## Checklist de qualidade

- [ ] Manifest valida sem erros e **sem avisos**
- [ ] `show`/`hide` repetidos não deixam resíduos na tela
- [ ] Texto longo não vaza do quadro
- [ ] Interromper `in` com `hide` é suave
- [ ] Painel organizado em seções; textos de rótulos claros
- [ ] Licença das fontes incluída em `fonts/`
- [ ] `meta.id`, `meta.version` e `requires` preenchidos

## Distribuição

Pasta ou `.zip` com a mesma estrutura. Não inclua arquivos fora do pacote (caminhos externos são recusados).
