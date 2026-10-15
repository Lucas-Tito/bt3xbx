# Data Center e Replay (F4, tarefa 9c-2)

Continuação de `menus-telas.md`, `menus-pecas.md`, `menus-evo.md` e do inventário `menus-modos.md`.
Fontes: `DcMenu_Init` (src/menu/menu_z_c.c), `DcList_Init` e `DcList_UpdateFace` (menu_y_b.c),
`DcPass_Init` e `DcPass_UpdateFace` (menu_z_d.c), `PassChk_Init` (menu_za_c.c), `ReplayMenu_Init`
(menu_za_d.c) e `PassWin_Init` (menu_za_b.c). Conversores em `port/x360/conv/menus_dc.c`.

O pacote `gMenuArc8`, id `baseFile + 0xB` = 460, índice 459 da partição 1, tem quatro seções BPE:
1 `menu_dc_topo` (modo 53), 2 `menu_dc_lista` (55), 3 `menu_dc_senha` (54), 4 `menu_replay` (56).
O arquivo recebe `menu_pacote`, e a tabela única de `conv.c` escolhe cada conteúdo. Cada gancho
converte logo depois do `Sprite_Unpack`, antes do primeiro leitor (`Res_RelocateOffsets` da primeira
seção pedida).

As regras são as de `menus-evo.md`: N exato, conferido por `Conv_Secoes` (limites, ordem e fim);
números trocados no lugar pela largura, bytes e texto do Flash intactos, enchimento exige zero; `TexFile`
com o mínimo de registros que o leitor alcança (1 + o maior `MTEX`; 8 no fundo do `Sprite_DrawPicture`);
textos com o mínimo de linhas alcançadas; legendas com duas entradas por fala. As peças são as já
especificadas (`menus-pecas.md`, `menus.md`); as novas são a tabela de itens (seção 6) e o pacote do
PassChk (seção 5). Os textos, as legendas e a janela de senha usam as funções da 9c-1, agora públicas
(`Conv_TextosMin`, `Conv_LegendasMin`, `Conv_JanelaSenha`).

As falas do guia são as mesmas três tabelas em todas as telas (`dc_msg`, `dcenter_lips`, 28 linhas e
56 entradas no disco), mas cada tela confere só o que ela alcança: o topo diz as falas 0..25
(`DcMenu_Say`, `DCLINE_*`), a lista 0x1A e 0x1B (`DcList_Say`), a senha 0x13..0x15 (`DcPass_Say`).
`MsgWin_Draw(0, 0, voiceLine)` lê a linha da fala tocada.

## 1. Topo: `menu_dc_topo`, 459 s1, N = 12

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | LipPack | 26 falas (52 entradas) | `DcMenu_Say`, falas 0..25 |
| 2 | JanelaMsg | | `MsgWin_Init` |
| 3 | JanelaIcone | | `IconWin_Init` (a seção 9 são as texturas dela) |
| 4 | Textos, falas | 26 | `MsgWin_Draw`, `voiceLine` 0..25 |
| 5 | Flash | | `Flash_Create` |
| 6 | TexFile, fundo | 8 | `view.bg`, `Sprite_DrawPicture` |
| 7 | TexFile, guia | 4 | MTEX 0, 1, 3 |
| 8 | TexFile | 2 | MTEX 0, 1 |
| 9 | TexFile, ícones | 2 | `IconWin_Init`, registros 0 e 1 |
| 10 | TexFile | 4 | MTEX 0..3 |
| 11 | TexFile | 7 | MTEX 0..6 |
| 12 | Diálogo | | `DcSave_Init`, `Dialog_Init(pack, NULL, 0)` |

O DcSave recebe a seção 12 já convertida e faz `McFlow_Init(1)`, que descomprime o 477 e passa pelo gancho
`menu_mcflow` (src/sys/mcflow_a.c), como no Evolution Z (`menus-evo.md`). Sair com `ZPROG_DIRTY`
(`DcMenu_Input`, o cancelar) chama `DcSave_Start`.

## 2. Lista: `menu_dc_lista`, 459 s2, N = 22

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile, fundo | 8 | `view.bg` |
| 2 | TexFile, guia | 4 | MTEX 0, 1, 3 |
| 3 | TexFile | 17 | MTEX 0..16 |
| 4 | TexFile | 5 | MTEX 0..4 |
| 5 | Flash | | filme 0 |
| 6 | TexFile | 1 | MTEX 0 |
| 7 | TexFile | 11 | MTEX 0..10 |
| 8 | TexFile | 1 | MTEX 0 |
| 9 | Flash | | filme 1 |
| 10 | Janela de senha | | `PassWin_Init` (N = 3, `menus-evo.md`) |
| 11 | JanelaMsg | | `MsgWin_Init` |
| 12 | Diálogo | | `Dialog_Init`, textos da 22 |
| 13 | Textos, falas | 28 | `MsgWin_Draw`, falas 0x1A e 0x1B |
| 14 | LipPack | 28 falas (56 entradas) | `DcList_Say` |
| 15 | Itens | 350 registros | `itemTbl`, `DcList_DrawItems` (seção 6) |
| 16 | ItemHelp | | `ItemHelp_Init` |
| 17, 18 | Textos, nomes e formas | 161 cada | `TextBox_AttachLine`, personagem 0..160 |
| 19 | TexFile | 7 | MTEX 0..6 |
| 20 | TexFile | 1 | MTEX 0 |
| 21 | Textos, itens | 350 | `TextBox_AttachLine`, id do item - 1 |
| 22 | Textos, diálogo | 3 | `Dialog_SetMsg(2)` |

## 3. Senha: `menu_dc_senha`, 459 s3, N = 21

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile | 10 | MTEX 0..9 |
| 2 | TexFile | 7 | MTEX 0..6 |
| 3 | TexFile | 14 | MTEX 0..13 |
| 4 | TexFile | 4 | MTEX 0..3 |
| 5 | TexFile | 1 | MTEX 0 |
| 6 | JanelaMsg | | `MsgWin_Init` |
| 7 | Diálogo | | `Dialog_Init`, textos da 21 |
| 8 | Textos, falas | 22 | `MsgWin_Draw`, falas 0x13..0x15 |
| 9 | LipPack | 22 falas (44 entradas) | `DcPass_Say` |
| 10 | TexFile, fundo | 8 | `view.bg` |
| 11 | TexFile, guia | 4 | MTEX 0, 1, 3 (duas vezes, uma por filme) |
| 12 | Itens | 350 registros | `itemTbl`, `ItemSet_GetBonus` (seção 6) |
| 13, 14 | Textos, nomes e formas | 161 cada | `TextBox_AttachLine`, personagem 0..160 |
| 15 | TexFile | 7 | MTEX 0..6 (duas vezes) |
| 16 | TexFile | 3 | MTEX 0..2 (duas vezes) |
| 17, 18 | Flash | | filmes 0 e 1 |
| 19 | PassChk | | `PassChk_Init` (seção 5) |
| 20 | Células | | contagem s32 em +0 (`gridCount`) e células de 9 s32 em +0x10 (`DcGridCell`, só o id lido); `Conv_Celulas` |
| 21 | Textos, diálogo | 2 | `Dialog_SetMsg(0)` e `(1)` |

## 4. Replay: `menu_replay`, 459 s4, N = 9

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | Flash | | `Flash_Create` |
| 2 | TexFile | 1 | MTEX 0 |
| 3 | TexFile | 14 | MTEX 0..13 |
| 4 | TexFile | 7 | MTEX 0..6 |
| 5 | Diálogo | | `Dialog_Init(pack, NULL, 0)` |
| 6 | TexFile, fundo | 8 | `bg` |
| 7 | TexFile | 7 | MTEX 0..6 |
| 8 | SubpacoteTex, fichas | 165 subseções | laço de relocação `i < 0xA5`, `MPACK_AT(chips, chara + 1)` |
| 9 | sem leitor | | bytes preservados (`Conv_RestoBytes`) |

A seção 9 vai para `gReplayMenu->font` e ninguém a lê. Tem 960 bytes e a mesma forma aparente da seção 22
da lista e da 21 da senha (`font_datacenter`, textos do diálogo, que têm leitor), mas o nome de origem é
outro (`font_datacenter_PS2_.pak`, sem o `JP`): sem leitor, fica em bytes, pela regra da 9c-1 de não inferir
largura pela aparência.

## 5. O pacote do PassChk (459 s3, seção 19), N = 5

`PassChk_Init` (menu_za_c.c) guarda cinco ponteiros (`PassChk`, include/menu/menu_za.h:139). Cada subseção
é uma tabela; a seção inteira tem a forma da tabela (todos os registros inteiros que cabem nela), o leitor
alcança pelo menos o mínimo, e o que sobrar depois do último registro inteiro tem de ser zero.

| Sub. | Registro | Larguras | Mínimo do leitor | No disco |
|---|---|---|---|---|
| 1 | `s32` (id do jogo anterior para o deste) | 4 | 0xCA (`PassChk_IsOldValid` recusa `charId >= 0xCA` antes de `PassChk_ConvertOld` indexar) | 832 bytes, 208 registros |
| 2 | `PassOldItem` (u8 `unk0`, `level`, `defense`, `attack`) | 1 | 0x109 (`PASSCHK_OLD_ITEM_NUM`) | 1.088 bytes, 272 registros |
| 3 | `s32 [6][3][7]` (itens por classe e tipo, 999 = nenhum) | 4 | 126 | 512 bytes, 128 palavras |
| 4 | `PassItem` (u8 `flags`, u8 `group`) | 1 | 0x15F (`item >= 0x15F` recusado) | 1.216 bytes, 608 registros |
| 5 | `PassChara` (u16 `mask`, u16 `valid`) | 2 | 1 (abaixo) | 576 bytes, 144 registros |

Os registros que passam do mínimo do leitor (208 - 202 na 1, por exemplo) são a continuação da mesma tabela,
com a largura que o leitor declara para ela; não são enchimento (a primeira versão do conversor exigia zero
depois de 0xCA registros e o disco recusou: há números ali).

**Hipótese da subseção 5:** `PassChk_IsOldValid` indexa `chara[charId]` com `charId` até 0xC9 (só 0x82 e 0x84
recusados, 0xC8 trocado por 0x82), mas a tabela tem 144 registros. Uma senha do jogo anterior com `charId` de
144 a 0xC9 lê além da subseção (no PS2, os bytes seguintes do pacote: a seção 20, a grade), e no 360 esses bytes
estão convertidos com a largura da grade. Só senhas de 32 caracteres do BT2 chegam lá; o roteiro não as digita.
O conversor converte os 144 registros e não amplia nada.

## 6. A tabela de itens (459 s2 seção 15, 459 s3 seção 12)

`ZItemEntry` (include/menu/menu_z.h:71) é uma visão local do `ItemTblEntry` do arquivo comum 00003
(`comuns.md` 4.4, `zitem_parameter`): 350 registros de 0x28 e 0x10 bytes de enchimento zero (14.016 bytes).
Por registro: 4 bytes (`type`, `kind`, `picture`, `slots`), dois u32, quatro u16 (`stat`), cinco u32
(`flags`, `ability`), como o `comum3` e a tabela da GetWin (`menus-historia.md` 6.2). Leitores: `DcList_DrawItems`
(`slots`, `type`, `ItemTbl_GetClass`) e `ItemSet_GetBonus` (`slots`, `stat`). Menos de 350 registros é recusado.

`DcList_DrawItems` lê `itemTbl[item - 1].slots` também com `item == 0` (o registro -1, 0x28 bytes antes da
tabela, dentro da seção 14) e só usa o valor com `item != 0`: um byte lido e descartado, que no 360 pode sair
de um número trocado da seção 14. Não muda o que a tela mostra.

Observação para a 9c: as seções sem leitor da 9c-1 (1 do EvoZ e 23 da loja, `menus-evo.md`) têm os mesmos
14.016 bytes e a mesma forma aparente desta tabela. Continuam em bytes (sem leitor); a pergunta está na nota
da tarefa em `docs/x360/plano/F4/09c.md`.

## 7. Os retratos

`DcList_Init` (:1020, o retrato do registro sob o cursor, que ninguém lê antes de `DcList_UpdateFace` refazer),
`DcList_UpdateFace` (:957) e `DcPass_UpdateFace` (:535) descomprimem `0x2F9 + personagem` (760..920, faixa
inteira já convertida, `menu_texfile`) para um `faceRes` de 0x20800, e o leitor é `Res_RelocateOffsets` e o
registro 0. Os ganchos são `menu_texfile`.

## 8. Conferência do disco e recusas (15/10/2026)

`conv_tool --disco gamedata`: 3.301 arquivos e 691 conteúdos convertidos, zero erros (eram 3.300 e 687 na 9c-1:
entram o 459 e as quatro seções). Os dois caminhos (marcas por byte e o mapa de bits do jogo) dão os mesmos bytes,
resultado e cobertura; o maior conteúdo continua 0x81C580, dentro de `CONV_JOGO_MAX`. No censo dos menus
(448..477), 21 arquivos convertidos de 30 e 38 conteúdos BPE com conversor de 49.

| Conteúdo | Tamanho | Bytes em números trocados | Cobertura |
|---|---:|---:|---:|
| 459 s1, `menu_dc_topo` | 1.055.616 | 56.376 | 100% |
| 459 s2, `menu_dc_lista` | 2.453.120 | 207.894 | 100% |
| 459 s3, `menu_dc_senha` | 1.983.040 | 129.890 | 100% |
| 459 s4, `menu_replay` | 1.798.976 | 103.438 | 100% |
| 459, `menu_pacote` | 3.329.600 | 96 | 100% |

Cada conversor sobre cópias intactas (saída 0) e estragadas (saída 1, com a seção na mensagem), por
`conv_tool --formato`, no WSL (os dados e o script em `~/9c2/estragos`, fora do git): 27 casos, 4 intactos e
23 estragos, todos com a saída esperada.

| Formato | Estrago | Recusa |
|---|---|---|
| topo | N = 11 | fim da tabela diferente do arquivo |
| topo | seção 11 com 6 texturas | o leitor usa 7 |
| topo | seção 4 com 25 linhas | o leitor usa 26 |
| topo | seção 1 com 50 entradas | a tela toca 26 falas |
| topo | diálogo da seção 12 com N = 5 | fim da tabela diferente |
| lista | N = 21 | fim da tabela diferente |
| lista | seção 3 com 16 texturas | o leitor usa 17 |
| lista | seção 13 com 27 linhas | o leitor usa 28 |
| lista | seção 17 com 160 linhas | o leitor usa 161 |
| lista | janela de senha com N = 2 | fim da tabela diferente |
| lista | um u32 não nulo no enchimento dos itens | não é enchimento |
| lista | seção 22 com 2 linhas | o leitor usa 3 |
| senha | N = 20 | fim da tabela diferente |
| senha | seção 1 com 9 texturas | o leitor usa 10 |
| senha | seção 8 com 21 linhas | o leitor usa 22 |
| senha | PassChk com N = 4 | fim da tabela diferente |
| senha | PassChk 1 com 200 registros | o leitor alcança 202 |
| senha | grade com 9.999 células | a grade cabe em 165 |
| senha | seção 21 com 1 linha | o leitor usa 2 |
| replay | N = 8 | fim da tabela diferente |
| replay | seção 3 com 13 texturas | o leitor usa 14 |
| replay | fichas com 164 subseções | fim da tabela diferente |
| replay | diálogo da seção 5 com N = 5 | fim da tabela diferente |
