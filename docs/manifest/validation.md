# Validação do manifest

Feita na carga (worker), antes de qualquer troca de template. **Erros** impedem o uso do template; **avisos** não.

Formato de mensagem: `NXG-<código> <caminho JSON>: <mensagem>` — ex.: `NXG-E021 animations.linha1.in.triggers[0]: ciclo de triggers: linha1.in → linha1mask.in → linha1.in`.

## Etapas

1. **Sintaxe**: JSON válido, UTF-8, tamanho máximo.
2. **Estrutura**: tipos e campos obrigatórios (equivale ao JSON Schema).
3. **Pacote**: arquivos existem, caminhos seguros, limites de tamanho.
4. **SVG**: parse OK; ids declarados existem; tamanho coerente com `canvas`.
5. **Semântica**: referências cruzadas, tipos de propriedades, ciclos, binds.
6. **Pós-processo**: avisos (conteúdo sem layer, fonte ausente, texto potencialmente maior que o quadro).

## Catálogo

### Erros

| Código | Descrição |
|---|---|
| `NXG-E001` | JSON inválido ou arquivo acima do limite |
| `NXG-E002` | `schema` não suportado (maior que o do motor) |
| `NXG-E003` | Arquivo ausente, caminho inseguro (`..`, absoluto, symlink externo) ou acima do limite |
| `NXG-E004` | SVG inválido ou não carregável |
| `NXG-E005` | Campo obrigatório ausente ou tipo errado |
| `NXG-E010` | `id` da layer não existe no SVG |
| `NXG-E011` | Nome de layer ou `id` duplicado |
| `NXG-E012` | `mask` aponta para layer sem `role: "mask"`, inexistente, ou máscara de máscara |
| `NXG-E013` | Tipo de layer desconhecido |
| `NXG-E020` | Clip referenciado não existe |
| `NXG-E021` | Ciclo de triggers |
| `NXG-E022` | Track aponta para layer inexistente |
| `NXG-E023` | Propriedade inexistente/incompatível com o tipo da layer |
| `NXG-E024` | `from/to/keys` inválidos (tipo, ordem de chaves, `loop` sem `duration`) |
| `NXG-E025` | Marker referenciado não existe |
| `NXG-E030` | `bind` inválido |
| `NXG-E031` | `key` de controle duplicada |
| `NXG-E032` | `action`/`apply` inexistente |
| `NXG-E033` | Valor padrão incompatível com a propriedade |
| `NXG-E040` | Passo de ação referencia clip/state/ação inexistente |
| `NXG-E041` | Ciclo de `call` entre ações |
| `NXG-E050` | `initialState` inexistente |
| `NXG-E060` | Limite excedido (clips, triggers, profundidade, layers) |

### Avisos

| Código | Descrição |
|---|---|
| `NXG-W001` | Conteúdo do SVG sem layer (desenhado como fundo implícito) |
| `NXG-W002` | Fonte do pacote ausente; usando fallback |
| `NXG-W003` | `maxLength` ou texto padrão provavelmente maior que o quadro |
| `NXG-W004` | Campo desconhecido (ignorado) |
| `NXG-W005` | `canvas` difere do tamanho do SVG; aplicada escala |
| `NXG-W006` | Layer declarada nunca usada em animações/controles/estados |

## Limites (padrões)

| Recurso | Limite |
|---|---|
| `manifest.json` | 1 MB |
| `layout.svg` | 8 MB |
| Cada fonte / imagem | 20 MB / 20 MB |
| Layers | 512 |
| Clips | 512 |
| Triggers por clip | 64 |
| Profundidade de triggers | 16 |
| Instâncias de clip simultâneas | 256 |

Configuráveis em constantes do `core`; mudanças devem ser documentadas aqui.

## JSON Schema

- Arquivo: `schemas/manifest.schema.json` (a criar no M2), referenciado pelo `$schema` do template.
- Cobre a etapa 2 (estrutura). As etapas 3–6 exigem lógica e ficam no motor.
- Uso no VS Code: com `$schema` apontando para o arquivo, há autocomplete e validação enquanto se edita.

## Comportamento em erro

- **Carga inicial**: instância fica sem template; painel exibe erros.
- **Recarga (hot reload/troca)**: mantém o template anterior; erros no painel e no log.
- Erros nunca interrompem o OBS; o log usa `[NaxGC]` e inclui o código.
