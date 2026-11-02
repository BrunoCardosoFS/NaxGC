# Painel de controle (dock Qt)

Implementado em `src/ui/`. O painel **não conhece nenhum template**: ele é construído a partir de `panel` no manifest.

## Estrutura

```
Dock "NaxGC"
 ├─ Seletor de instância  (fontes NaxGC existentes nas cenas)
 ├─ Cabeçalho: nome do template, botão "Recarregar"
 └─ Conteúdo gerado de panel.sections
      ├─ Seção (título, colapsável)
      │    └─ Controles (um widget por controle)
      └─ ...
```

- Registro: `obs_frontend_add_dock_by_id("naxgc-dock", "NaxGC", widget)` (OBS recente; fallback `obs_frontend_add_custom_qdock` em versões antigas — ver versão mínima no ROADMAP).
- Reconstrução completa do conteúdo ao: trocar de instância, trocar/recarregar template.

## Factory de controles

`control_factory.cpp` mapeia `type` → widget:

| `type` | Widget | Valor |
|---|---|---|
| `text` | `QLineEdit` (ou `QPlainTextEdit` com `multiline: true`) | string |
| `number` | `QDoubleSpinBox` / slider | número |
| `toggle` | `QCheckBox` | booleano |
| `color` | botão + `QColorDialog` | cor `#rrggbb[aa]` |
| `dropdown` | `QComboBox` | opção escolhida (`value` ou `apply` de estado) |
| `button` | `QPushButton` | dispara `action` |
| `label` | `QLabel` | texto estático |
| `separator` | linha | — |

Tipo desconhecido → widget "indisponível" + aviso no log (não quebra o painel).

## Fluxo de valores

```
Widget muda ─► Engine.set(bind, valor)  ──(fila)──► Scene
Scene/WS muda ─► evento state_changed ──(queued)──► Widget atualiza (sem re-emitir)
```

- Evitar *eco*: ao atualizar widget por evento, bloquear sinais (`QSignalBlocker`).
- Controles com vários `bind` aplicam o mesmo valor em todas as propriedades.
- `dropdown` com `apply` aplica o `state` correspondente.
- Debounce de ~50–100 ms em campos de texto digitados (envio ao Engine) para não re-rasterizar a cada tecla; `Enter` envia imediatamente.

## Botões, ações e atalhos

- `button.action` chama `Engine.run_action(nome)`.
- `button.hotkey` é um **rótulo** para registrar um atalho no OBS (`obs_hotkey_register_*`); o usuário define a combinação em *Configurações → Atalhos*. O manifest não define teclas.
- Estado visual do botão (ex.: "no ar") pode refletir `clip_started`/`clip_finished` (futuro).

## Persistência

- Valores dos controles ficam nas **settings da fonte** (por template/`meta.id`). Ao recarregar, valores cujas chaves ainda existem são reaplicados.
- Valores inválidos para o tipo atual são descartados com log.

## Estados de erro

- Template inválido: painel mostra a lista de erros (código + caminho no JSON) e mantém o último template válido, se houver.
- Instância sem template: mensagem orientando a escolher a pasta nas propriedades da fonte.

## Acessibilidade e usabilidade

- Ordem de tabulação segue a ordem do manifest.
- `Enter` em campo de texto não aciona "Entrar" por padrão (evita ir ao ar sem querer); pode ser configurado por controle (`submitAction`).
- Rótulos e `placeholder` vêm do manifest; futuro: i18n por chave.
