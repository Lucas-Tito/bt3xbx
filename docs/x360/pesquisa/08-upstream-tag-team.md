# 08. O que as fontes do Tag Team e do BT4 andaram desde a nossa base (triagem de 11/11/2026)

Registro de possibilidades para a F9, **sem nada a fazer agora** (mesma decisão da [pesquisa 07](07-upstream-pc.md)).
Clones públicos lidos sem executar nada. Todas as fontes abaixo são AGPL-3.0 (com `LICENSE-MIT` para o que veio antes da
troca), como a frente do BT4 já aceitou ([decisão 40](../decisoes.md)). Nenhuma fala de BT4 Rev3 nem B15: o perfil fixo
B14 Rev2 continua sendo o alvo de todas.

## Tag Team nativo de PC (tehmufti, `Budokai-Tenkaichi-3-Tag-Team-Mod-PC_Recomp-`)

Nossa base de fato é `9b02e8c3` (02/11), o segundo pai do merge `f13fd93f` que incorporou o Tag Team nativo. Desde então,
um commit e uma release: `ef016d6` (09/11, v0.1.0-beta.5), só correções, todas em C e atrás de `#ifdef PORT` ou do perfil:

| Arquivo | O que corrige |
|---|---|
| `src/battle/btl_char_hit.c` | Solar Flare: o pedido de segurar volta para cada inimigo próximo e só é consumido quando o alvo pode reagir (pesa com 3 ou 4 lutadores) |
| `src/battle/btl_char_flag_opp.c` | Aliado caído não liga o flag 0xB1, que apagava o registro de entrada do espectador (reviver aliado) |
| `src/battle/tag_team.c` | Pausa cinemática: não corta as pausas de início, o ator trocando de modelo não congela, transformação compartilhada pausa os demais |
| `src/battle/btl_input.c`, `hud_a.c` | No BT4, o clash também aceita Círculo repetido, com o ícone no prompt |

Custo: horas, mais a rodada dos roteiros `f9_equipe*` e a conferência do C89 (o código usa `static inline`).

## BT4decomp

O `troca/ref/bt4decomp` é `NauTaro/DBZ-BT4-Tag-Team-Decomp` na tag `v0.1.39.5-R2` (`f1031069`, 03/11), conferido no
`troca` em 11/11 e registrado na decisão 40. Desde a R2: 69 commits e as releases R3 a R5.5
(ponta `0206e3b`, 11/11), uns 55 deles online (ranked, relays, Supabase). O que vale para o 360:

| Commit | O que traz |
|---|---|
| `83e3bd9` (R3) | Esteiras de água para até 10 lutadores (`eft_e.c`; antes, duas fixas, com corrupção acima de 2) e limites em `stageFx` (`eft_k.c`). Importante para a tela dividida |
| `83e3bd9` (R3) | Itens: índice inválido não passa dos 350 da tabela; `variant=1` na habilidade 0x7B (`battle_load.c`) |
| `49d9748` e seguintes | `CS_CELL.id` no `CharSel` e no `TeamSel`: o retrato antigo trocava o lutador escolhido |
| `83e3bd9` (R3) | Troca de corpo do Ginyu (`bt4_body_change.c`), se o escopo do BT4 a incluir |

O resto (menus widescreen, pacote HD, online) não serve. O BT4decomp também assume as correções do Tenkaichi3Decomp de
0.1.38 a 0.1.39.5, que o 360 não tem (a F9 partiu da 0.1.37).

## Para os defeitos abertos do BT4

- Quadrado branco no Z counter: nenhuma fonte trata. Pista incerta: a R4 (`275275a`) corrige "blocos coloridos" em
  especiais por uma cópia de 64 px lida da memória errada no renderizador de PC (`gs_core.c`, `gs_draw.c`); só vale
  se o nosso caminho fizer a mesma leitura.
- As cinco recusas (arenas 11 e 46, câmera `pzs4us2/02209`) e a queda no `Dialog_Init`: nada. O
  `spectator_collision.inc` (`83e3bd9`, `8caa9bd`), que valida índices de colisão contra o tamanho do bloco da fase,
  serve de modelo de verificador, não de reparo.

## Só referência

Pyrosea98 (`BT3-TagTeam-Native` v0.5 alfa, `BT3-TagTeam-Runtime` sem commits novos na `decomp-native`), o mod PS2
(14.4 a 14.6) e `j-maestre/BT4_Online_decomp` (derivado; só o reviver aliado local poderia interessar, valor baixo).
