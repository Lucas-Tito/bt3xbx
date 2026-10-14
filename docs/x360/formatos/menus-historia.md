# As telas da História (F4, etapa 9a)

Continuação de `menus-telas.md` (o caminho essencial), `menus-pecas.md` (as peças comuns) e `menus-modos.md` (o
inventário das 82 chamadas de `Sprite_Unpack`). Aqui ficam os pacotes de tela da História (Dragon History, modos 6 a
10 do gerenciador de menus), o arquivo de cada saga e as imagens que essas telas descomprimem (episódios e fundos do
resultado), mais uma peça nova, a janela de recompensa (`GetWin`). Conversores em `port/x360/conv/menus.c`
(`Conv_MenuHistSagas`, `Conv_MenuHistLista`, `Conv_MenuHistResultado`, `Conv_MenuHistSave`, `Conv_MenuSaga`,
`Conv_MenuHistFundo`) e `menus_pecas.c` (`Conv_GetWin`), registrados nas tabelas `sFormatos` e `sFaixas` de `conv.c`.

Regra da conversão (decisões 8 e 12): in-place, cada número trocado pela largura com que o código o lê, bytes e
strings intactos, enchimento exigido zero, nada muda de tamanho nem de lugar. As regras de `menus-telas.md` 1 valem
aqui: cada `TexFile` leva o mínimo de registros que o leitor usa (1 + o maior índice de `MTEX`; 8 num fundo desenhado
por `Sprite_DrawPicture`), o N de cada tabela é exato (`Conv_Secoes`), e seção sem leitor se converte pela forma.

Fontes: o código da decomp (arquivo e linha; as linhas são as de antes dos ganchos da 9a, que deslocam as seguintes de
cada arquivo); o disco inteiro pelo `conv_tool --disco` e contagens de estrutura no WSL (só números voltaram); o
rastreio do roteiro `historia` da etapa 4 pelo `conv_tool --mapa`. Nenhum byte do jogo está aqui. **Hipótese** marca o
que o código não prova.

## 1. Os arquivos

| Arquivo | Id (como o código calcula) | Índices | Forma | Formato do arquivo | Conteúdo |
|---|---|---|---|---|---|
| `gMenuArc2` | `baseFile + 0xC` = 0x1CD (`HIST_LOAD_ARC`, menu_b_c.c:838) | 460 | pacote, 4 seções BPE | `menu_pacote` | s1 `menu_hist_sagas`, s2 `menu_hist_lista`, s3 `menu_hist_resultado`, s4 `menu_hist_save` |
| saga | `baseFile + subMenu + 0x10` = 0x1D1..0x1D8 | 464..471 | BPE inteiro | `menu_bpe` | `menu_saga` |
| episódios | `imageBase + item`, `imageBase` de 0x3CE a 0x3FA por saga (`MD_SETUP`, menu_a_d.c:185) | 973..1020 | BPE inteiro | `menu_bpe` | `menu_texfile` |
| fundos do resultado | `0x3FE + bg`, bg 0..3 (menu_c_c.c:190) | 1021..1024 | BPE inteiro | `menu_bpe` | `menu_hist_fundo` |

Quem chama, e o formato que cada gancho passa (`Port_ConvertMenuPack(buffer, rawSize, "formato")`):

| Chamada | Conteúdo | Formato |
|---|---|---|
| `HistSel_Init` menu_b_d.c:147 | 460 s1 | `menu_hist_sagas` |
| `ModeMenu_Init` menu_a_d.c:126 | 460 s2 | `menu_hist_lista` |
| `ModeMenu_Init` menu_a_d.c:128 | saga | `menu_saga` |
| `ModeMenu_UpdateImage` menu_a_d.c:76 | episódio (buffer fixo `imageRes`, 0x20800) | `menu_texfile` |
| `HistOutro_Init` menu_b_c.c:79 | 460 s2 | `menu_hist_lista` |
| `HistOutro_Init` menu_b_c.c:107 | saga | `menu_saga` |
| `HistResult_Init` menu_c_c.c:180 | 460 s3 | `menu_hist_resultado` |
| `HistResult_Init` menu_c_c.c:182 | saga | `menu_saga` |
| `HistResult_Init` menu_c_c.c:191 | fundo 0..3 | `menu_hist_fundo` |
| `HistSave_Init` menu_c_d.c:25 | 460 s4 | `menu_hist_save` |

Cada chamada descomprime num buffer novo (o `Sprite_Unpack(src, NULL, NULL)`), liberado no `_Term` da tela, menos o
episódio: `ModeMenu_UpdateImage` reescreve o `imageRes` inteiro a cada imagem (o conteúdo de um episódio tem 0x20560
bytes no disco, menos que os 0x20800 do buffer), então converter depois de cada descompressão não converte duas vezes.

## 2. A seleção de saga (`menu_hist_sagas`: 00460, seção 1)

`HistSel_Init(1)` (src/menu/menu_b_d.c:137). Tabela: N = 40 (o inventário dizia 40; o disco confirma). Conteúdo
descomprimido: 4.960.256 bytes, o maior da História (abaixo dos 8 MB de `CONV_JOGO_MAX`).

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | `TexFile`, o fundo | 8 | `SEL_RES(1)`, `gHistSel->bg`, `Sprite_DrawPicture` (`HistSel_Draw`, :357) |
| 2 | `TexFile` | 4 | `SEL_TEX` 0..3 |
| 3 | `TexFile` | 4 | 0..3 |
| 4 | `TexFile` | 6 | 0..5 |
| 5 | `TexFile` | 23 | 0..6 e, no laço, `MTEX(res, i) + 7 * 0x40` para i = 0..15: os registros 7..22 |
| 6 | `TexFile` | 4 | `SEL_ICONS` (0, 1, 3) |
| 7 | `TexFile` | 1 | 0 |
| 8 | `TexFile` | 9 | 0..8 |
| 9 | `TexFile` | 9 | 0..6 e 8 |
| 10 | Flash | | `Flash_Create` (:233) |
| 11 | `TexFile`, os ícones da janela de ícone | 1 | `IconWin_Init(pack 13, res)` |
| 12 | `TexFile` | 2 | `MTEX(res, 1)` |
| 13 | JanelaIcone | | `IconWin_Init` |
| 14 | JanelaMsg | | `MsgWin_Init(pack 14, NULL, ...)` |
| 15..23 | textos (`menus.md` 6.1) | | `msgText[8]` = 15, `msgText[0..7]` = 16..23 (`MsgWin_SetText`, :483; `TextBox`) |
| 24..32 | LipPack | | `subtitles[8]` = 24, `subtitles[0..7]` = 25..32 (`Voice_PlayWithSubtitle`, :20..:44) |
| 33..40 | `TexFile`, os ícones de cada saga | 4 | `SEL_RES(33 + i)` e `SEL_ICONS` (0, 1, 3) |

Todas as seções têm leitor.

## 3. A lista de episódios e o fim da saga (`menu_hist_lista`: 00460, seção 2)

Lida por duas telas: `ModeMenu_Init(2)` (src/menu/menu_a_d.c:119, modo 7) e `HistOutro_Init(2)` (src/menu/menu_b_c.c:71,
modo 9). Tabela: N = 8 (o disco confirma). Conteúdo descomprimido: 695.616 bytes.

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | `TexFile` | 9 | ModeMenu: `MD_RES(res, 1)`, registros 0, 1, 2, 4..8; HistOutro: 7 |
| 2 | `TexFile` | 1 | ModeMenu: 0 |
| 3 | Flash | | ModeMenu: `Flash_Create` |
| 4 | `TexFile` | 1 | HistOutro: 0 |
| 5 | Flash | | HistOutro: `Flash_Create` |
| 6 | textos | | as duas: `TextBox_Init(box, text)` e `TextBox_AttachLine(..., descr.line + i, ...)` |
| 7 | `TexFile` | 2 | ModeMenu: 1 |
| 8 | JanelaMsg | | as duas: `MsgWin_Init(pack 8, msgText, ...)`, com os textos da seção 4 da saga |

## 4. O resultado (`menu_hist_resultado`: 00460, seção 3)

`HistResult_Init(3)` (src/menu/menu_c_c.c:171, modo 8). Tabela: N = 5 (o disco confirma). Conteúdo: 1.510.208 bytes.

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | `TexFile`, o fundo | 8 | `HR_RES(res, 1)`, `gHistResult->bg`, `Sprite_DrawPicture` (`HistResult_Draw`) |
| 2 | `TexFile` | 6 | 0..5 |
| 3 | `TexFile` | 2 | 0, 1 |
| 4 | Flash | | `Flash_Create` (:223) |
| 5 | GetWin (seção 6) | | `GetWin_Init(pack 5, 1)` (:225) |

## 5. O save da História (`menu_hist_save`: 00460, seção 4)

`HistSave_Init(4)` (src/menu/menu_c_d.c:21, modo 10): só `Dialog_Init(MPACK_AT(res, 1), NULL, 0)`, o diálogo do cartão
que o `McFlow_Start(0)` usa. Tabela: N = 1, a seção 1 um arquivo de diálogo (`menus.md` 6), a mesma forma da tela do
cartão (`menu_cartao`, `menus.md` 5.1). Conteúdo: 81.408 bytes. O `McFlow_Init(1)` carrega o 00477 pelo caminho dele
(`menu_mcflow`, já convertido).

## 6. A janela de recompensa (`GetWin`, peça nova)

`GetWin_Init(pack, lang)` (src/battle/view_a.c:42). Três chamadores: o resultado da História (460 s3, seção 5, lang 1),
a chave do torneio (`Bracket_Load`, menu_k_f.c:189: 454..458, seção 42, lang 1) e o Shenlong (late_a.c:590: 472 s2,
seção 11, lang 0). Tabela: N = 11; nenhuma vazia (`cheias`).

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | `TexFile` | 4 | `res->tex + FLASH_TEX_SIZE * k`, k = 0..3 (`FLASH_TEX_SIZE` = 0x40, o passo do `MTEX`) |
| 2 | `TexFile` | 3 | k = 0..2 |
| 3 | `TexFile` | 3 | k = 0..2 |
| 4 | `TexFile` | 1 | k = 0 |
| 5 | Flash | | `Flash_Create` com lang = 1 |
| 6 | Flash | | com lang != 1 (no resultado da História fica sem leitor; é filme no disco, então Flash sempre, como a seção 5 do ItemPanel) |
| 7, 8, 9, 11 | textos | | `text[1]`, `text[0]`, `text[2]`, `text[3]` e `text[4]` (`TextBox_Init`) |
| 10 | tabela de itens | | `gGetWin->items` (`GetWinItem`, 0x28 bytes) |

### 6.1 A tabela de itens (seção 10)

`GetWinItem` (include/battle/view_a.h:111) é `{u8 type; u8 unk1[0x27]}`, e o jogo só lê o `type` (`GetWin_Setup`,
view_a.c:280, `items[value].type`). O registro tem 0x28 bytes, o mesmo tamanho do `ItemTblEntry` (include/battle/
view_b.h:121) e dos itens de 00003 (`comuns.md` 4.4). No disco a seção tem 350 registros (`SAVE_ITEM_COUNT`) e 16 bytes
de enchimento zerado: é a tabela de itens, e o resto do registro se converte pela forma de 00003 (4 bytes, dois u32,
quatro s16, cinco s32). **Hipótese:** que os campos não lidos tenham essas larguras (a forma vem do 00003; ninguém os
lê aqui). O enchimento depois dos registros é exigido zero.

No `--pecas` os sete usos do disco (460 s3, 454..458 e 472 s2) convertem a peça inteira sem erro, com 100% de
cobertura.

## 7. O arquivo de uma saga (`menu_saga`: 00464..00471, o contêiner inteiro)

Lido por quatro funções: `ModeMenu_Init` (a lista), `HistOutro_Init` (o fim da saga), `HistResult_Init` (o resultado)
e `ModeBg_Init` (src/menu/menu_b_b.c:14, o fundo das duas primeiras). Tabela: N = 10 nas oito sagas (o disco decide;
o código não lê a 11). Conteúdo: de 866.112 bytes (saga 3) a 1.585.152 (saga 4).

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | `TexFile`, o fundo | 8 | `ModeBg_Init`, `gModeBg->bg`, `Sprite_DrawPicture` (`ModeBg_Draw`) |
| 2 | `TexFile` | 2 | ModeMenu 0, 1; HistOutro 0; HistResult 1 |
| 3 | `TexFile` | 2 | ModeMenu 0, 1; HistOutro 0 |
| 4 | textos | | `msgText` da janela de mensagem (ModeMenu, HistOutro) |
| 5 | LipPack | | `subtitles` (`Voice_PlayWithSubtitle`, ModeMenu e HistOutro) |
| 6 | descrições (7.2) | | `descr` (ModeMenu e HistOutro) |
| 7 | `TexFile`, os ícones da saga | 4 | `MD_ICONS`, `OUTRO_ICONS` (0, 1, 3) |
| 8 | `TexFile`, os ícones do parceiro | 4 | HistOutro: `OUTRO_ICONS` pelo `partner` |
| 9 | Flash, o fundo animado | | `ModeBg_Init` (menos na saga 3) |
| 10 | `TexFile`, as texturas do fundo animado | por saga: 6, 6, 3, -, 6, 3, 13, 4 | `ModeBg_Init`, `BG_TEX` (menos na saga 3) |

### 7.1 A saga 3 e os mínimos por saga

A saga 3 (00467) não tem fundo animado: `ModeBg_Init` volta logo depois da seção 1 e `ModeBg_Draw` não desenha o filme,
e no disco as seções 9 e 10 dela estão vazias. O conversor aceita as duas vazias; numa saga lida que as tenha vazias
(as outras sete), o leitor cairia.

O conversor recebe só os bytes: no jogo o nome é o do formato (`Port_ConvertMenuPack`), e a mesma chamada converte
qualquer saga. O que depende da saga (o mínimo da seção 10, o número de descrições, as seções 9 e 10 cheias) se confere
com o mínimo comum no jogo e exato na `conv_tool`, que tem o nome do arquivo (`pzs3us1/00464.bin@0x0#bpe`) e valida as
oito sagas do disco. O resultado sobre os dados certos é o mesmo nos dois caminhos.

### 7.2 As descrições (`ModeMenuDescr`, seção 6)

`ModeMenuDescr` (include/menu/menu_b.h:21): `{s32 count; s32 line; s32 delay[6]}`, 0x20 bytes, oito s32. A lista lê o
registro de cada episódio (`descr[items[cursor]]`, 0..itemMax - 1), o fim da saga lê o de `itemMax`
(`descr[gHistOutro->itemMax]`, menu_b_c.c:282): itemMax + 1 registros, com itemMax = 3, 4, 7, 5, 16, 5, 4, 4 por saga.
`count` vai a 3 (as três caixas de texto) e o conversor o confere. No disco: 4, 6, 8, 6, 18, 6, 6 e 6 registros, a
seção um múltiplo exato de 0x20. Mínimo no jogo: 4 registros.

### 7.3 Escritas do jogo depois da conversão

- `gModeMenu->descr->count = 2` na saga 7 (`ModeMenu_Init`, menu_a_d.c:222): o `count` do registro 0 do 00471, gravado
  em ordem nativa no buffer já convertido. Uma comparação de memória da saga 7 tem de contar com isso; os roteiros da 9a
  visitam só a saga 0.
- Nenhuma outra tela da História grava nos pacotes (as escritas vão ao save, `gSaveData->slot[]`, e às estruturas de
  cada tela).

## 8. As imagens (`menu_texfile`, `menu_hist_fundo`)

- **Episódios** (973..1020): `ModeMenu_UpdateImage` lê só o registro 0 (`gModeMenu->tex[9] = MTEX(res, 0)`): o
  `menu_texfile` (1 registro) de `menus-telas.md` 6. As 48 imagens do disco convertem com 100% de cobertura.
- **Fundos do resultado** (1021..1024): o fundo 0 (01021) lê os registros 0 e 1 (`tex[10]`, `tex[9]`), os outros só o
  0. A chamada é uma só para os quatro (menu_c_c.c:191), com um formato só: `menu_hist_fundo`, mínimo 1 no jogo e 2 no
  01021 na `conv_tool` (pelo nome, como as sagas). No disco os três últimos têm 1 registro, então um mínimo de 2 para
  os quatro não serve. Não são desenhados por `Sprite_DrawPicture` (vão ao filme pelo `tex[]`).

## 9. Cobertura conferida

### 9.1 Contra o disco (`conv_tool --disco`)

Sem erro: os 4 conteúdos do 00460, as 8 sagas, os 48 episódios e os 4 fundos, todos com 100% de cobertura (e os
arquivos inteiros 00460, 00464..00471, 00973..01024 como `menu_pacote` e `menu_bpe`). O censo: a faixa 973..1024
passou de 0 para 52 de 52 arquivos e contêineres com conversor; na faixa dos menus, o 00460 e as oito sagas. O `--pecas`
valida a `GetWin` sozinha nos sete usos do disco (83 usos sem erro no total).

### 9.2 Contra o rastreio (`conv_tool --mapa`, roteiro `historia` da etapa 4)

O roteiro da etapa 4 vai da seleção de saga à luta do primeiro episódio: alcança 460 s1, 460 s2, a saga 0 e o episódio
973. Nenhuma posição divergente:

| Conteúdo | Posições do rastreio | Iguais | Outra largura | Não trocadas |
|---|---|---|---|---|
| 00460 (o arquivo) | 6 | 6 | 0 | 0 |
| 460 s1 | 23.828 | 23.827 (+1 em cadeia de texto lida por inteiro) | 0 | 0 |
| 460 s2 | 6.470 | 6.469 (+1 em cadeia de texto) | 0 | 0 |
| 00464 (o arquivo) e a saga 0 | 2 e 570 | todas | 0 | 0 |
| 00973 (o arquivo) e o episódio | 2 e 10 | todas | 0 | 0 |

460 s3, 460 s4, as outras sagas e os fundos só aparecem depois de uma luta terminada: entram no `--mapa` com o roteiro
estendido da 9a.

### 9.3 Os conversores vistos falhando

No WSL, sobre cópias do conteúdo antes da conversão (o `.orig` do `conv_tool --marcas`), cada um estragado num ponto e
convertido com `--formato`: a cópia intacta sai com 0 e a estragada com 1 e o nome da seção:

| Formato | Estrago | Mensagem |
|---|---|---|
| `menu_hist_sagas` | o `TexFile` da seção 5 com 3 registros | "3 registros, o leitor usa 23 [seção 5 da seleção de saga]" |
| `menu_hist_lista` | a janela de mensagem com 3 seções | "Conv_Tabela: 3 entradas (máximo 2) [janela de mensagem] [seção 8 da lista de episódios]" |
| `menu_hist_resultado` | um byte não nulo no enchimento depois dos itens da `GetWin` | "byte ... não reconhecido [seção 10 da janela de recompensa] [seção 5 do resultado da História]" |
| `menu_hist_save` | a tabela com 2 seções | "Conv_Tabela: 2 entradas (máximo 1) [save da História]" |
| `menu_saga` | a descrição 1 com 7 linhas | "descrição 1 com 7 linhas (as caixas são três) [seção 6 da saga]" |
| `menu_hist_fundo` | o `TexFile` sem registros | "0 registros, o leitor usa 1" |

## 10. Hipóteses e o que fica aberto

- As larguras dos campos não lidos da tabela de itens da `GetWin` (6.1), pela forma do 00003.
- A seção 6 da `GetWin` (o filme de lang != 1) não tem leitor na História; o Shenlong a lê.
- A comparação de memória da saga 7 tem de saber da escrita do `count` (7.3).
