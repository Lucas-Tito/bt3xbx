# Peças comuns dos pacotes de tela e o menu principal (F4, etapa 5a)

Continuação de `menus.md` (o pacote externo, o diálogo, o Flash, a tabela de textos, o `TexFile` nos menus) e de
`menus-modos.md` (o inventário das 82 chamadas de `Sprite_Unpack`, seção 2 "As peças novas"). Aqui ficam as peças que
se repetem nas telas do caminho essencial, cada uma com os leitores no código e a largura de cada campo, e o primeiro
pacote de tela que as usa: o do menu principal (`menu_principal`). As peças são funções reutilizáveis em
`port/x360/conv/menus_pecas.c` (declaradas em `conv_menus.h`), para os conversores das telas da etapa 5b.

Regra da conversão (decisões 8 e 12, `menus.md` introdução): in-place, cada número trocado pela largura com que o
código o lê, bytes e strings intactos, enchimento exigido zero, nada muda de tamanho nem de lugar.

Fontes: o código da decomp (arquivo e linha em cada tabela; as linhas são as de hoje); o disco inteiro pelo
`conv_tool --disco` e um script de estrutura no WSL (só contagens e tamanhos voltaram); o rastreio dos onze roteiros de
controle da etapa 4 (`port/build/roteiros/NOME/mapa.tsv` no WSL), pelo `conv_tool --mapa`. Nenhum byte do jogo está
aqui. **Hipótese** marca o que o código não prova.

## 1. As peças e onde aparecem

| Peça | Leitor | Forma | Seção |
|---|---|---|---|
| tabela de seções | `MPACK_AT`, `PACK_AT` (`buf[n] >> 2 << 2`) | `menus.md` 2.1 | |
| `TexFile` | `Res_RelocateOffsets`, `MTEX`, `FlashDraw_UploadTex` | `personagem.md` 4.4, `menus.md` 8 | |
| filme Flash | `Flash_Create` e o tocador | `menus.md` 7 | |
| tabela de textos | `TextBox_AttachLine`, `DIALOG_TEXT` | `menus.md` 6.1 | |
| arquivo de diálogo | `Dialog_Init` | `menus.md` 6 | |
| **JanelaMsg** | `MsgWin_Init` | subpacote: `TexFile`, Flash | 2 |
| **JanelaIcone** | `IconWin_Init` | subpacote: `TexFile`, Flash | 3 |
| **LipPack** | `Voice_PlayWithSubtitle`, `LipSync_Start`, `LipSync_Update` | tabela de blocos de chaves da boca | 4 |
| **ItemPanel**, **ItemHelp**, **ListaIds**, **Células**, **SubpacoteTex** | telas de seleção, Treino, Opções | | 5 |
| **ItemPanel**, **ItemHelp**, **ListaIds**, **Células**, **SubpacoteTex** | telas de seleção, Treino, Opções | | 5 |

Nas funções: `Conv_Secoes` (tabela com exatamente N entradas), `Conv_Vazia`, `Conv_Flash`, `Conv_Textos` e
`Conv_Dialogo` (de `menus.c`, antes estáticas), `Conv_TexFile` (`personagem.c`) e as peças novas de `menus_pecas.c`.

Onde cada peça aparece nos pacotes do caminho essencial (do inventário, `menus-modos.md` 3.3):

| Pacote | JanelaMsg | JanelaIcone | LipPack | textos | Outras |
|---|---|---|---|---|---|
| `menu_principal` (449 s2) | 21 | 22 (ícones: 23) | 27 | 28 | Flash 20 |
| `menu_duelo` (448 s1) | 22 | 23 (ícones: 24) | 26 | 21, 27 | Flash 19, diálogo 25 |
| `menu_selecao` (448 s2, 461 s2) | | 54 | | 43, 44 | ListaIds 39, 56; Células 42; SubpacoteTex 45, 46; ItemHelp 51; ItemPanel 53 |
| `menu_treino` (461 s1) | 11 | 10 (ícones: 5) | 12, 13 | 14..17 | Flash 1; ListaIds 24, 26 |
| `menu_opcoes` (453 s1) | 31 | 32 (ícones: 18) | 36 | 33, 38 | Flash 8; diálogo 35; ListaIds 37 |

## 2. JanelaMsg (`MsgWin_Init`)

`MsgWin_Init(pack, text, side)` (src/battle/view_a_b.c:30): a janela de mensagem que entra de um lado e mostra uma
linha de uma tabela de textos (`text`, do chamador).

| Parte | Formato | Quem lê | Conversão |
|---|---|---|---|
| tabela | `menus.md` 2.1, N = 2 | `PACK_AT(pack, 1)`, `PACK_AT(pack, 2)` (view_a_b.c:36, :41) | u32 (`Conv_Tabela`) |
| 1 | `TexFile` | `Res_RelocateOffsets` (:37); os registros 0 e 1 (`res->tex`, `+ FLASH_TEX_SIZE`, :38..:39) vão ao filme | `Conv_TexFile` |
| 2 | filme Flash | `Flash_Create(gMsgWin->flash, PACK_AT(pack, 2), gMsgWin->tex)` (:41); o tocador procura o clipe `mc_message_text` (`MsgWin_Draw`, :65) e os rótulos `fl_l_in`, `fl_r_in`, `fl_l_out`, `fl_r_out` | `Conv_Flash` |

O `text` é uma tabela de textos (`menus.md` 6.1) lida por `TextBox_AttachLine` (src/battle/view_b.c:232, a conta de
`TB_ATTACH_STR`, :243: `text + (text[line + 1] >> 2 << 2)`, texto u16). Ela fica no pacote do chamador e é convertida
por ele (no menu principal, a seção 28). `MsgWin_SetText` (:103) só troca o ponteiro.

No disco: o N é 2 nas JanelaMsg conferidas (menu principal, seção 21: `TexFile` de 133.824 bytes com 2 registros e um
filme de 1.920). O conversor exige N = 2 (`Conv_Secoes`); um pacote com mais seções vira erro com o nome da peça, e
a especificação ganha o caso.

Escritas do jogo: `Res_RelocateOffsets` grava os ponteiros do `TexFile` (`menus.md` 9); nada mais.

## 3. JanelaIcone (`IconWin_Init`)

`IconWin_Init(pack, icons)` (src/battle/view_a_c.c:24): uma janela com filme cuja quinta textura é um ícone escolhido
de outro `TexFile` (`icons`, do chamador, já relocado por ele).

| Parte | Formato | Quem lê | Conversão |
|---|---|---|---|
| tabela | `menus.md` 2.1, N = 2 | `PACK_AT(pack, 1)` (:30), `PACK_AT(pack, 2)` (:40) | u32 |
| 1 | `TexFile` | `Res_RelocateOffsets` (:31); registros 0, 1 e 2 (`tex[1]`, `tex[0]`, `tex[3]`, :32..:34) | `Conv_TexFile` |
| 2 | filme Flash | `Flash_Create(&gIconWin->flash, PACK_AT(pack, 2), gIconWin->tex)` (:40); rótulos `fl_in`, `fl_out` | `Conv_Flash` |

`icons` (`FlashTexRes`, include/battle/view_a.h:52): os registros 0 e `1 + ícone` (`IconWin_SetIcon`, :74) vão ao
filme. É uma seção `TexFile` do pacote do chamador (no menu principal, a 23, relocada por `MM_RES(23)` antes do
`IconWin_Init`, src/menu/menu_a.c:137), convertida por ele.

No disco: N = 2 (menu principal, seção 22: `TexFile` de 69.632 bytes e filme de 21.824). O conversor exige N = 2.

## 4. LipPack: legendas e o movimento da boca

Não é o `BObjLipData` da luta (`personagem.md` 6.6), que lê a contagem como u16: aqui ela é s32.

### 4.1 Tabela

`Voice_PlayWithSubtitle(pack, base, line)` (src/battle/view_b_c.c:36): a fala `line` toca na língua do bit 0 de
`gSaveData->flags` e, se `pack` não é nulo, a boca anda pelo bloco
`pack + (pack[line * 2 + 1] >> 2 << 2)` (primeira língua, :43) ou `pack[line * 2 + 2]` (segunda, :38). A tabela tem a
forma de `menus.md` 2.1 com duas entradas por fala: N par, `pack[1..N]` deslocamentos, `pack[N + 1]` o tamanho.
Ninguém lê `pack[0]` nem `pack[N + 1]`.

| Parte | Tipo | Quem lê | Conversão |
|---|---|---|---|
| `pack[0]` = N | u32 | ninguém | u32 (`Conv_Tabela`) |
| `pack[1..N]` | u32 | `Voice_PlayWithSubtitle` (:38, :43) | u32 |
| `pack[N + 1]` = tamanho, enchimento até a entrada 1 | u32, zeros | ninguém | u32 |
| cada entrada não vazia | bloco de chaves (4.2) | `LipSync_Start`, `LipSync_Update` | 4.2 |

Uma entrada vazia (o mesmo deslocamento da seguinte) não tem bloco próprio; o conversor converte cada bloco uma vez,
de uma entrada até a seguinte. No disco não há entrada vazia nem dois idiomas com o mesmo bloco (menu principal, seção
27: N = 130, 65 falas, 130 deslocamentos distintos).

### 4.2 Bloco de chaves (`LipSync_Start`, view_b_c.c:284)

| Desl. | Tipo | Quem lê | Conversão |
|---|---|---|---|
| 0x00 | 8 bytes | ninguém (a assinatura; uma só em todos os blocos do menu principal) | bytes |
| 0x08 | s32 contagem | `Mem_ReadS32(data + 8)` (:290; src/sys/mem_read.c:26, uma leitura nativa alinhada) | u32 |
| 0x0C | 4 bytes | ninguém | zero exigido (zero em todos os blocos do menu principal) |
| 0x10 | contagem x {u16 quadro, u16 boca} | `Mem_ReadU16` em `LipSync_Start` (:294, o quadro da última chave é a duração) e `LipSync_Update` (:319, :320 o quadro desta e da seguinte; :330 a boca, `+ 2`) | u16 x 2 por chave |
| depois das chaves | zeros até o bloco seguinte | ninguém | zero exigido |

`LIPSYNC_KEY_SIZE` = 4 (include/battle/view_b.h:170). A contagem tem de ser pelo menos 1 (a duração vem da última
chave) e as chaves têm de caber no bloco: o conversor confere as duas. No menu principal as contagens vão de 3 a 29.

Escritas do jogo: nenhuma (o estado fica em `gLipSync`).

## 5. As peças das telas de seleção, do Treino e das Opções

Especificadas do código por um agente e conferidas no disco inteiro pelo `conv_tool --pecas` (seção 7.3): cada peça
aplicada só à seção dela, em todo pacote do disco que a usa, antes de o pacote inteiro ganhar conversor. Os usos de
5.6 são a tabela `sUsos` da `conv_tool`. O que o código deixava como **hipótese** e o disco decidiu está em 5.7.

### 5.0 Como o código chega às seções

| Macro ou forma | Onde | O que lê |
|---|---|---|
| `MPACK_AT(pack, n)` | include/menu/menu_a.h:55 | `((u32 *)pack)[n] >> 2 << 2`: a entrada n da tabela, u32, arredondada para baixo a múltiplo de 4 |
| `PACK_AT(pack, n)` | include/battle/view_a.h:60 | a mesma conta (`ShenCfm_Init`, late_a_b.c) |
| `MPACK_WORD(pack, n)` | src/menu/menu_c_e.c:367 | `((s32 *)pack)[pack[n] >> 2]`: a primeira palavra da seção n, **s32** (CharSel) |
| `TS_PACK_WORD(pack, n)` | src/menu/menu_e_b.c:46 | a mesma palavra, `*(s32 *)` (TeamSel) |
| `res[res[n] >> 2]` | menu_x_c.c:129, menu_l_b.c:224, menu_m_b.c:384, menu_i_d.c:276, menu_k_f.c:198, menu_u_h.c:674 | a mesma palavra, lida como **u32** (`res` é `u32 *`) e guardada em s32 |
| `*(u32 *)Train_Section(n)` | menu_h_d.c:710, :712 | a mesma palavra, **u32** |
| `*(s32 *)DcPass_Section(n)` | menu_z_d.c:1068 | a mesma palavra, **s32** |
| `MTEX(res, n)` | include/menu/menu_a.h:52 | `res->tex + n * 0x40`: só aritmética sobre o ponteiro `TexFile.ent` (+0x10) já relocado; não lê o registro |

Todas as palavras de contagem e de id destas peças são lidas com 4 bytes, alinhadas (o arredondamento de
`MPACK_AT` e o tamanho das células, 0x24, mantêm múltiplo de 4). As funções `DcList_Section`, `DcPass_Section`,
`Train_Section` e `ReplayMenu_Section` são `MPACK_AT` com o nome do arquivo de origem do kit de desenvolvimento
(o nome não é usado; serve de pista do formato: "character_select_order_PS2_.dat", "tu_noraml_botu_PS2_.dat",
"chara_chip_PS2_.pak", "ez_item_details_JP_PS2_.pak").

### 5.1 ItemPanel

O painel de itens equipados de um lado de uma seleção de personagem. Um subpacote: tabela de seções (`menus.md`
2.1) com seções `TexFile`, Flash e tabelas de textos.

#### 5.1.1 Leitores

| Leitor | Arquivo:linha | Seções |
|---|---|---|
| `ItemPanel_Init(pack, side)` | src/menu/menu_g.c:39 | relocação (`IP_RES`, :34) das seções 16 (:47), 1 (:49), 2 (:52), 3 (:54), 6 (:57), 7 (:60), 8 (:62), 11 (:66), 9 (:69), 12 (:71), 10 (:74); `Flash_Create` da seção `4 + side` (:77); textos 13, 14, 15 (:79..:81) |
| `ItemPanel_Draw` | menu_g.c:103 | texto 13 na linha `p->chara` (:134), texto 14 na linha `p->chara` (:136), texto 15 na linha `item - 1` (:206), por `TextBox_AttachLine` (src/battle/view_b.c:232, a conta de `menus.md` 6.1); o filme por `Flash_Draw` |
| `ItemPanel_Update`, `_Show`, `_Hide`, `_ClipGoto` | menu_g.c:214, :318, :332, :16 | só o filme (estado guardado no `MFlash` da struct, fora do pacote) |

A tabela de itens (`p->items`, :82) vem do arquivo comum 4, não do pacote.

#### 5.1.2 Campos

| Desl. ou seção | Tipo | Largura | Quem lê | Conversão |
|---|---|---|---|---|
| +0x00 N | u32 | 4 | ninguém | u32 (com a tabela, `Conv_Secoes`) |
| +4·n, n = 1..16 | u32 deslocamento | 4 | `MPACK_AT` (menu_g.c:35, :77, :79..:81) | u32 |
| +4·17 fim (= tamanho) | u32 | 4 | ninguém | u32 (`Conv_Tabela` confere = tamanho) |
| enchimento até a seção 1 | zeros | | ninguém | u32 (`Conv_Tabela`) |
| seção 1 | `TexFile`, usa registros 0..3 | | :49..:51 | `Conv_TexFile` |
| seção 2 | `TexFile`, registro 1 | | :52..:53 | `Conv_TexFile` |
| seção 3 | `TexFile`, 0 e 1 | | :54..:56 | `Conv_TexFile` |
| seção 4 | Flash do lado 0 | | :77 | `Conv_Flash` |
| seção 5 | Flash do lado 1 | | :77 com `side` = 1 (só CharSel e TeamSel) | `Conv_Flash` (ver 5.1.3) |
| seção 6 | `TexFile`, 0, 1, 3 | | :57..:59 | `Conv_TexFile` |
| seção 7 | `TexFile`, 0 | | :60..:61 | `Conv_TexFile` |
| seção 8 | `TexFile`, 1..3 | | :62..:65 | `Conv_TexFile` |
| seção 9 | `TexFile`, 0 | | :69..:70 | `Conv_TexFile` |
| seção 10 | `TexFile`, 3 e 4 | | :74..:76 | `Conv_TexFile` |
| seção 11 | `TexFile`, 0 | | :66..:68 | `Conv_TexFile` |
| seção 12 | `TexFile`, 0 e 1 | | :71..:73 | `Conv_TexFile` |
| seção 13 | textos: nomes de personagem (linha = id 0..0xA0) | | :79, :134 | `Conv_Textos` |
| seção 14 | textos: nomes de forma (linha = id) | | :80, :136 | `Conv_Textos` |
| seção 15 | textos: nomes de item (linha = id do item - 1) | | :81, :206 | `Conv_Textos` (ver 5.1.6) |
| seção 16 | `TexFile`, 0 | | :47..:48 | `Conv_TexFile` |

#### 5.1.3 O que ninguém lê

- `buf[0]` (N) e `buf[17]` (o fim): convertidos como u32 pela tabela.
- Seção 5 nos pacotes em que só o lado 0 existe (EntrySel, SoloSel, UbTeamSel: `ItemPanel_Init(..., 0)`): sem
  leitor ali. Era hipótese que fosse filme em todos; o disco confirmou nos cinco pacotes (5.7), e o conversor a trata
  como filme sempre.
- Registros de `TexFile` além dos usados (o `Res_RelocateOffsets` reloca todos, então todos são convertidos pelo
  `Conv_TexFile`, lidos ou não).

#### 5.1.4 N da tabela

O código lê as entradas 1..16 (a maior é a 16, :47) e nunca a 0 nem a 17. N = 16 é **hipótese**; o conversor exige
exatamente 16 (`Conv_Secoes`) e o disco confirma ou recusa.

#### 5.1.5 Escritas do jogo depois da conversão

`Res_RelocateOffsets` (src/sys/gfxm_b_b.c:244) grava `TexFile.ent` (+0x10) e `TexEntry.pix`/`clut` (+0x38, +0x3C)
em cada `TexFile` relocado, sobre o próprio buffer (`menus.md` 9). No CharSel e no TeamSel o `ItemPanel_Init` roda
duas vezes sobre o mesmo pacote: a relocação é idempotente (refaz o ponteiro a partir do deslocamento) e grava os
mesmos valores. Nada mais grava no pacote.

#### 5.1.6 Conferências do conversor

- Tabela com N = 16, ordenada, fim = tamanho (`Conv_Secoes`), nenhuma seção vazia (todas têm leitor no CharSel e
  no TeamSel; uma seção vazia faria o leitor pegar a seguinte).
- Contagem de registros (`TexFile.count`, +0, lida antes de converter) pelo menos 1 + o maior índice de `MTEX`:
  seção 1 >= 4, 2 >= 2, 3 >= 2, 6 >= 4, 7 >= 1, 8 >= 4, 9 >= 1, 10 >= 5, 11 >= 1, 12 >= 2, 16 >= 1. O código não
  confere (`MTEX` só soma), mas um índice além da contagem daria ao filme um registro não relocado.
- Textos 13 e 14: pelo menos 161 entradas (`p->chara` vai a 0xA0; **hipótese**: o painel nunca recebe um marcador
  0xA1..0xA4). Texto 15: tantas entradas quanto itens (`SAVE_ITEM_COUNT` = 350, include/sys/save.h:21; **hipótese**
  quanto ao número exato). O `Conv_Textos` recusava mais de 0x100 entradas; o teto subiu para 0x400 (5.7).

#### 5.1.7 Uso

| Pacote (arquivo, seção externa) | Tela | Seção do ItemPanel | Chamada | Lados |
|---|---|---|---|---|
| 448 s2 (e 461 s2 no modo 0x2D, Treino) | CharSel | 53 | menu_c_e.c:517..:519 | 0 e 1 |
| 448 s2 | TeamSel | 53 | menu_e_b.c:603..:606 | 0 e 1 |
| 450 s2 | EntrySel | 39 | menu_i_d.c:261 | 0 |
| 451 s4 | SoloSel | 22 | menu_l_b.c:209 | 0 |
| 451 s4 | UbTeamSel | 22 | menu_m_b.c:369 | 0 |

### 5.2 ItemHelp

A página de detalhes de um item (nome, descrição, ícone, custo). Subpacote: tabela de seções com `TexFile`, Flash e
textos.

#### 5.2.1 Leitores

| Leitor | Arquivo:linha | Seções |
|---|---|---|
| `ItemHelp_Init(pack)` | src/menu/menu_v_c.c:14 | relocação da 1 (:21..:22, registros 0, 4, 8, 9 em :23..:26) e da 3 (:28..:29, registros 0 e 1 em :30..:31); `Flash_Create` da 2 (:33); textos 6 (`nameText`, :37) e 5 (`descText`, :38) |
| `ItemHelp_Draw(item)` | menu_v_c.c:64 | texto 6 na linha `item` (:76), texto 5 na linha `item` (:78), por `TextBox_AttachLine`; o filme (:71, :109) |
| `ItemHelp_Open`, `ItemHelp_Close` | menu_v_c.c:114, :119 | só o filme |

`gItemHelp` só é tocado em menu_v_c.c (o `grep` não acha outro leitor). A tabela de itens (`items`, :36) vem do
arquivo comum 4, não do pacote.

#### 5.2.2 Campos

| Desl. ou seção | Tipo | Quem lê | Conversão |
|---|---|---|---|
| +0x00 N | u32 | ninguém | u32 (tabela) |
| +4·n, n = 1..6 | u32 deslocamento | `MPACK_AT` (n = 1, 2, 3, 5, 6) | u32 |
| +4·7 fim | u32 | ninguém | u32 |
| seção 1 | `TexFile`, registros 0, 4, 8, 9 | :21..:26 | `Conv_TexFile` |
| seção 2 | Flash | :33 | `Conv_Flash` |
| seção 3 | `TexFile`, 0 e 1 | :28..:31 | `Conv_TexFile` |
| seção 4 | **sem leitor** | ninguém | bytes (5.2.3) |
| seção 5 | textos: descrições (linha = item, 0-based) | :38, :78 | `Conv_Textos` |
| seção 6 | textos: nomes de item (linha = item) | :37, :76 | `Conv_Textos` |

#### 5.2.3 O que ninguém lê

- Seção 4: nenhum leitor em nenhum dos nove chamadores. Não se inventa formato. A proposta era decidir pela forma,
  como a seção 4 do título (`menus.md` 5.4) e a seção 2 dos logos (`menus.md` 5.2); o disco mostrou que ela não tem a
  forma de nenhuma peça conhecida (nem filme, nem `TexFile`, nem tabela de seções; a mesma nos sete pacotes). Fica
  em bytes (`Conv_RestoBytes`), o que só vale porque nenhum código a lê; se um leitor aparecer, ela ganha
  especificação.
- `buf[0]` e `buf[7]`: u32 com a tabela.

#### 5.2.4 N da tabela

O código lê 1, 2, 3, 5, 6; a maior é a 6 (:37). O código não prova N = 6; o conversor exige 6, e o disco o tem
(5.7).

#### 5.2.5 Escritas do jogo

Só a relocação das seções 1 e 3 (5.1.5). Nada mais.

#### 5.2.6 Conferências

- N = 6, seções 1, 2, 3, 5 e 6 não vazias.
- `TexFile.count`: seção 1 >= 10 (registro 9 em :26), seção 3 >= 2.
- Textos 5 e 6: uma entrada por item (`item` vai a 349 se a tabela de itens tiver 350 registros); o teto do
  `Conv_Textos` (5.1.6).

#### 5.2.7 Uso

| Pacote | Tela | Seção do ItemHelp | Chamada |
|---|---|---|---|
| 448 s2 (e 461 s2) | CharSel | 51 | menu_c_e.c:520 |
| 448 s2 | TeamSel | 51 | menu_e_b.c:608 |
| 450 s2 | EntrySel | 35 | menu_i_d.c:262 |
| 451 s4 | SoloSel | 24 | menu_l_b.c:214 |
| 451 s4 | UbTeamSel | 24 | menu_m_b.c:374 |
| 452 s3 | EvoZ (`EvoZ_Load`) | 2 | menu_u_h.c:658 |
| 452 s2 | Loja (`Shop_Init`) | 25 | menu_v_d.c:260 |
| 459 s2 | DcList | 16 | menu_y_b.c:1106 |
| 472 s2, dentro da seção 6 (subpacote do `ShenCfm`) | Shenlong | 4 do `ShenCfm` | src/sys/late_a_b.c:81 (`ShenCfm_Init` chamado em src/sys/late_a.c:586) |

O ItemHelp do Shenlong está aninhado dois níveis: 472 s2 seção 6 é o `ShenCfm` (1 Flash, 2 e 3 `TexFile`, 4 o
ItemHelp, late_a_b.c:63..:81), e o conversor do `menu_shenlong` chama o do `ShenCfm`, que chama `Conv_ItemHelp`.

### 5.3 ListaIds

Uma lista de ids s32 com uma contagem na frente. Não é tabela de seções.

#### 5.3.1 Leitores

| Leitor | Arquivo:linha | O que faz |
|---|---|---|
| `CharSel_Init` (estágios, seção 39) | menu_c_e.c:536..:538 | `stageIds` = seção + 0x10; `stageCount` = `MPACK_WORD` (s32); `StgGrid_ApplyUnlocks` |
| `CharSel_Init` (músicas, seção 56) | menu_c_e.c:604..:618 | `bgmIds` = seção + 0x10; `bgmCount` = `MPACK_WORD`; `BgmList_ApplyUnlocks`; `bgmCount -= 4`; grava `bgmIds[bgmCount - 1] = 0x18` (:618) |
| `TeamSel_Init` (39 e 56) | menu_e_b.c:624..:632 | o mesmo, com `TS_PACK_WORD`; grava em :632 |
| `StgGrid_ApplyUnlocks` | src/battle/view_a_e.c:897 | lê `ids[i]` (:905) para i < contagem; grava `ids[i]` = marcador (:910, :917, :919, :926) |
| `BgmList_ApplyUnlocks` | view_a_e.c:941 | lê `*ids` para i < contagem; grava o marcador 0x19 (`BGMLIST_ID_LOCKED`) |
| grade de estágios | `StgGrid_IsSelectable` view_a_e.c:681, `StgGrid_MoveRight` :686, `_MoveLeft` :698, `_MoveRightWrap` :712, `_MoveDown` :730, `_MoveUp` :747; `CharSel_SetStageChips` menu_c_e.c:85 (:90); `TeamSel_SetStageChips` menu_e_b.c:122 (:127); menu_c_e.c:707, :717, :1267..:1306, :1925; menu_e_b.c:734, :744, :1344..:1379 | `ids[linha * 6 + coluna]`, 6 linhas (`stageRows` = 6, menu_c_e.c:598; `TS_STAGE_COLS` no TeamSel): **36 ids lidos sem olhar a contagem**; :1925 sorteia em `ids[Rand_Range(count - 1)]` |
| lista de músicas | menu_c_e.c:695, :740, :743, :968, :1049..:1051, :1378..:1385, :1913, :1930, :1944; menu_e_b.c:705, :770..:776, :949, :1011..:1013, :1438..:1446 | `ids[cursor]`, cursor < contagem - 4 |
| `Train_Init` (seções 24 e 26) | menu_h_d.c:709..:712 | `skip`/`noImage` = seção + 0x10; `skipNum`/`noImageNum` = `*(u32 *)` da seção |
| `Train_BuildLists` | menu_h_d.c:523 (chamada em :721), pelo inline `Train_IsSkipped` (:510, laço :514..:515) | compara `skip[i]` (s32) com o número da página (u32), i < `skipNum` (u32). **Assembly no binário original** (INCLUDE_ASM, :555); o port compila a versão C do `#if 0`, que `port/tools/portsrc.py` liga (`#if 1` em port/build/gen) |
| `Train_PageHasImage` | menu_h_d.c:928 (laço :931..:932; chamada em :1071) | compara `noImage[i]` com `voiceLine`, i < `noImageNum` |
| `Option_Init` (seção 37) | menu_x_c.c:128..:132 | `bgmIds` = seção + 0x10; `bgmCount` = `res[res[37] >> 2]` (u32) - 5; `BgmList_ApplyUnlocks` sobre count - 5 |
| `Option_Input`, `Option_Draw` | menu_x_c.c:722..:723; :1854, :1859, :1872, :1877 | `bgmIds[cursor]`, `bgmIds[bgmTop + i]` (6 linhas), `bgmIds[bgmExtra]` |

Os blocos `#ifdef PORT` de menu_c_e.c (:539..:587, :619..:649) e menu_e_b.c (:633..:664) copiam a lista para
buffers do port depois do `ApplyUnlocks` (só leem `count` entradas, já nativas): não mudam o formato.

#### 5.3.2 Campos

| Desl. | Tipo | Largura | Quem lê | Conversão |
|---|---|---|---|---|
| +0x00 | contagem n | 4 | s32 (`MPACK_WORD` menu_c_e.c:537, :605; `TS_PACK_WORD` menu_e_b.c:625, :628); u32 (menu_x_c.c:129; menu_h_d.c:710, :712) | u32 |
| +0x04..+0x0F | não determinado | | **ninguém** | zeros exigidos (`Conv_Zeros`); se o disco tiver bytes não nulos, `Conv_RestoBytes` (ninguém lê) |
| +0x10 + 4·i, i < n | id | 4 | s32 (os leitores de 3.1) | u32 |
| depois de +0x10 + 4·n até o fim da seção | | | ninguém pela contagem (ver 5.3.4, estágios) | zeros exigidos (**hipótese**: a seção termina na lista) |

#### 5.3.3 Escritas do jogo (depois da conversão, em ordem nativa)

- `StgGrid_ApplyUnlocks` (view_a_e.c:910..:926): troca ids de estágio travados por `STGGRID_ID_LOCKED` e, no modo
  0x28, os estágios 4 e 0x1B por `STGGRID_ID_EMPTY` (0x3E e 0x3F no port, include/battle/view_a.h:306..:312; 0x24 e
  0x25 no PS2).
- `BgmList_ApplyUnlocks` (view_a_e.c:941..:958): troca as músicas travadas por 0x19 e, no modo 0x3E, a 0x18 por
  0x19. Roda nas seções 56 da seleção e 37 das Opções (menu_x_c.c:132).
- `bgmIds[n - 5] = 0x18` (menu_c_e.c:618, menu_e_b.c:632).
- Treino: nenhuma.

O pacote é descomprimido de novo a cada `_Init`, então as escritas nunca encontram dado já convertido duas vezes.
A comparação de memória x86 contra PPC (`menus.md` 10.4) tem de saber que essas palavras mudam depois do gancho.

#### 5.3.4 Contagens e limites a conferir

- `0x10 + 4·n <= tamanho da seção`; n como s32 não negativo.
- Início da seção múltiplo de 4 (o `MPACK_AT` arredonda; se o deslocamento da tabela não for múltiplo de 4 o código
  leria 1 a 3 bytes antes do que o conversor converte).
- Estágios (39): a grade lê 36 ids sem olhar n, então **n >= 36** (senão a grade lê palavras que o conversor tratou
  como enchimento). Ids: `stageBits >> id` em u64 (view_a_e.c:916, :925) exige id < 64; o ícone `MPACK_AT(stagePack,
  id + 1)` (menu_c_e.c:112, só fora do port) exige id <= 0x25 (subpacote de 38). Teto proposto 0x25; que o disco só
  tenha 0..0x23 é **hipótese** (no port um 0x24 ou 0x25 vindo do disco seria lido como estágio adicionado, e não
  como marcador: o port moveu os marcadores para 0x3E/0x3F).
- Músicas (56): n >= 5 (o jogo grava em `ids[n - 5]`); id < 32 (`1 << id` em u32, view_a_e.c:954); teto proposto
  0x19 (**hipótese**: 0..0x18 no disco, 0x19 só gravado pelo jogo).
- Músicas das Opções (37): n >= 11 (`Option_Draw` desenha 6 linhas a partir de `bgmTop` sobre n - 5 entradas;
  **hipótese** de que o cursor nunca passe de n - 5); mesmo teto.
- Treino (24, 26): ids são números de página e de fala, sem teto no código; só n.

#### 5.3.5 Uso

| Pacote | Tela | Seção | Conteúdo | Chamada |
|---|---|---|---|---|
| 448 s2 (e 461 s2 no modo 0x2D) | CharSel | 39 | estágios | menu_c_e.c:536..:538 |
| 448 s2 (e 461 s2) | CharSel | 56 | músicas | menu_c_e.c:604..:618 |
| 448 s2 | TeamSel | 39 | estágios | menu_e_b.c:624..:626 |
| 448 s2 | TeamSel | 56 | músicas | menu_e_b.c:627..:632 |
| 461 s1 | Treino | 24 | páginas fora das listas ("tu_noraml_botu") | menu_h_d.c:709..:710 |
| 461 s1 | Treino | 26 | falas sem figura ("tu_noraml_notexture") | menu_h_d.c:711..:712 |
| 453 s1 | Opções | 37 | músicas do teste de som | menu_x_c.c:128..:132 |

Parecidas e que **não** são ListaIds (tabelas s32 que começam em +0, sem contagem nem cabeçalho): UbRank 12 e 13
(menu_n_c.c:149..:150, 28 estágios e 11 músicas, sorteadas em :49, :52), SurvSel 14 (menu_s_b.c:95, sorteada em
:60), CharRef 17, 26..28 (menu_h_b.c:107..:113). Ficam nos conversores dos pacotes delas (etapa 9).

### 5.4 Células

A lista mestra de uma grade de personagens: contagem e células de 0x24 bytes (`ChrGridCell`,
include/battle/view_a.h:298; vistas locais `MChrCell` include/menu/menu_d.h:14, `TsCell` menu_e.h:24, `LChrCell`
menu_l.h:59, `ESelCell` menu_j.h:34, `UbCell` menu_m.h:38, `UChrCell` menu_u.h:132, `VChrCell` menu_v.h:84,
`BrkCell` menu_k.h:57, `DcGridCell` menu_z.h:316; e as de lista `LChrGridList` menu_l.h:70, `ESelGridList`
menu_j.h:45, `UChrGridList` menu_u.h:143: `s32 count`, `s32 unk4[3]`, células em +0x10). Todas as vistas têm o
mesmo layout, conferido pelos comentários `/* 0x.. */`: 0x00 s32 id, 0x04 s32 formCount, 0x08 s32 form[7], total
0x24.

#### 5.4.1 Leitores

| Leitor | Arquivo:linha | O que lê |
|---|---|---|
| `ChrGrid_Build(outCount, out, inCount, in, customCount, custom)` | src/battle/view_a_e.c:776 | `in[i].formCount` (:810); `in[i].form[j]`, j < formCount (:812..:814); `in[i].id` (:835 `switch`, :851); cópia da célula inteira `out[n] = in[i]` (:840, :847, :852: 9 palavras s32); no laço dos personagens salvos, `in[j].id` (:879). Só lê `in`; grava `out` e `custom` (buffers da tela). **Assembly no binário original** (INCLUDE_ASM, :892); o port compila a versão C do `#if 0` ("Not matching, 210 of 347", diferença só de registradores e de código compartilhado), ligada por `port/tools/portsrc.py`. O `.s` não está no repositório (gerado pelo splat do ELF): o layout lido vem do C e dos headers |
| `DcPass_IsListed` | menu_z_d.c:435 (laço :438..:439; chamada em :496) | só `grid[i].id`, i < `gridCount`; sem `ChrGrid_Build` |

Depois do `ChrGrid_Build` as telas trocam `grid` pelo buffer de saída (`gridBuf`, `grid[]`, `cells[]`): todos os
outros leitores (`ChrGrid_Move*`, `ChrGrid_IsSelectable`, `_SetRowChips`, ...) leem o buffer da tela, não o pacote.

#### 5.4.2 Campos

| Desl. | Tipo | Largura | Quem lê | Conversão |
|---|---|---|---|---|
| +0x00 | contagem n | 4 | s32 (`MPACK_WORD` menu_c_e.c:662; `TS_PACK_WORD` menu_e_b.c:666; `*(s32 *)` menu_z_d.c:1068); u32 em s32 (menu_l_b.c:224, menu_m_b.c:384, menu_i_d.c:276, menu_k_f.c:198, menu_u_h.c:674) | u32 |
| +0x04..+0x0F | `unk4[3]` (s32 nas structs) | | **ninguém** | zeros exigidos; se não forem, `Conv_RestoBytes` (ninguém lê) |
| +0x10 + 0x24·i +0x00 | s32 id | 4 | `ChrGrid_Build` :835, :851, :879; `DcPass_IsListed` :439 | u32 |
| +0x04 | s32 formCount | 4 | :810, :812 | u32 |
| +0x08..+0x23 | s32 form[7] | 4 cada | :813..:814 para j < formCount; as demais só copiadas como parte da struct (:840, :847, :852) | u32 cada (a struct é s32; copiadas e nunca lidas como outra largura) |
| depois de +0x10 + 0x24·n | | | ninguém | zeros exigidos (**hipótese**) |

#### 5.4.3 Escritas do jogo

Nenhuma no pacote: `ChrGrid_Build` grava só em `out` e `custom`, e o DcPass só lê.

#### 5.4.4 Contagens e limites

- `0x10 + 0x24·n <= tamanho da seção`; início da seção múltiplo de 4.
- `ceil(n / 7)·7 <= 165`, isto é, **n <= 161**: `ChrGrid_Build` completa a grade até múltiplo de 7 (:862..:869) num
  buffer de 165 células em toda tela (`MCHR_CELL_MAX`, `TS_CELL_MAX`, `LCHR_CELL_MAX`, `ESEL_CELL_MAX`,
  `UB_CELL_MAX`, `BRK_CELL_MAX` e `cells[165]` do EvoZ).
- formCount de 0 a 7 (o laço grava `out.form[out.formCount]`, 7 posições).
- formCount = 0: id de 0 a 0xA4 (personagem 0..0xA0 ou marcador 0xA1..0xA4; o id + 1 indexa o subpacote de 165
  fichas, seção 5; `CHARA_UNLOCKED` lê `charaBits[id / 64]`, 3 palavras).
- formCount > 0: form[j], j < formCount, de 0 a 0xA0 (personagens; **hipótese**: o disco não põe marcador numa
  forma). O id dessas células também é lido (:879) e fica sem teto.

#### 5.4.5 Uso

| Pacote | Tela | Seção | Chamada (leitura e `ChrGrid_Build`) |
|---|---|---|---|
| 448 s2 (e 461 s2 no modo 0x2D) | CharSel | 42 | menu_c_e.c:661..:667 (duas vezes, uma por lado, sobre a mesma lista) |
| 448 s2 | TeamSel | 42 | menu_e_b.c:665..:673 (duas vezes) |
| 450 s2 | EntrySel | 29 | menu_i_d.c:275..:277 |
| 451 s4 | SoloSel | 30 | menu_l_b.c:223..:225 |
| 451 s4 | UbTeamSel | 30 | menu_m_b.c:383..:385 |
| 454..458 | Bracket | 24 | menu_k_f.c:197..:198 (`Bracket_Load`); `ChrGrid_Build` em menu_k_b.c:35 |
| 452 s3 | EvoZ | 33 | menu_u_h.c:673..:674 (`EvoZ_Load`); `ChrGrid_Build` em menu_u_g.c:22 |
| 459 s3 | DcPass | 20 ("character_select_order") | menu_z_d.c:1067..:1068; leitor `DcPass_IsListed` |

### 5.5 SubpacoteTex

Uma seção que é ela mesma uma tabela de seções (`menus.md` 2.1), cada subseção um `TexFile` (a ficha de um
personagem ou o ícone de um estágio). A base dos deslocamentos de cada `TexFile` é o início da própria subseção
(`Res_RelocateOffsets(&res, res, res)`).

#### 5.5.1 Leitores

| Leitor | Arquivo:linha | O que lê |
|---|---|---|
| laço de relocação | ver 5.5.5 | `MPACK_AT(sub, i + 1)` para i < 165 (ou < 38), `Res_RelocateOffsets` em cada: todas as subseções 1..165 (1..38) |
| fichas por id | ver 5.5.5 | `MPACK_AT(sub, id + 1)->tex`, o registro 0 (`res->tex` ou `MTEX(res, 0)`) |
| MisSel, UbzSel | menu_o_d.c:288, menu_n_b.c:221 | a subseção 165 direto (`MISSEL_CHIP_NUM`, `N_CHIP_NUM`: a ficha "vazia", id 0xA4 + 1) |
| ícones de estágio | `CharSel_SetStageChips` menu_c_e.c:109 (port: ícone 37 para travado, 38 para vazio, `1 + (id - 0x24) % 36` para estágio adicionado) e :112 (PS2: `id + 1`); `TeamSel_SetStageChips` menu_e_b.c:136, :139 | a subseção 1..38, registro 0 |

#### 5.5.2 Campos

| Desl. ou subseção | Tipo | Quem lê | Conversão |
|---|---|---|---|
| +0x00 N | u32 | ninguém | u32 (tabela) |
| +4·k, k = 1..N | u32 deslocamento | `MPACK_AT` (todas, pelo laço) | u32 |
| +4·(N + 1) fim | u32 | ninguém | u32 |
| enchimento até a subseção 1 | zeros | ninguém | u32 (`Conv_Tabela`) |
| subseção k | `TexFile` | `Res_RelocateOffsets` (todos os registros); o registro 0 pelo leitor | `Conv_TexFile` (base = início da subseção) |

#### 5.5.3 N, subseções vazias, sem leitor

- Fichas: o laço reloca 1..165 em todo chamador (`MCHR_CELL_MAX`, `TS_CELL_MAX`, `ESEL_CELL_MAX`, `LCHR_CELL_MAX`,
  `UB_CELL_MAX`, `N_CHIP_NUM`, `MISSEL_CHIP_NUM`, `SIMTOP_FACE_NUM`, 165 literal no EvoZ e 0xA5 no Replay), e o id
  vai a 0xA4: as 165 têm leitor. Estágios: 1..38, todas com leitor (36 estágios, 37 travado, 38 vazio). O código
  lê a entrada 165 (38) e nunca a 0 nem a 166 (39): **N = 165 e N = 38 são hipótese**; o conversor do pacote confere
  N exato.
- Subseção vazia: o código não a impede (uma vazia faria o `Res_RelocateOffsets` relocar a seguinte de novo, o que é
  idempotente, e a ficha mostraria a textura da seguinte; se a última fosse vazia, leria além do subpacote). O
  conversor recusa subseção vazia (**hipótese**: nenhuma é vazia); se o disco tiver, aceitar vazia só quando houver
  uma seguinte não vazia e anotar.
- Nada sem leitor dentro do subpacote além de `buf[0]` e do fim.

#### 5.5.4 Escritas do jogo

A relocação de cada `TexFile` (5.1.5), uma vez por tela.

#### 5.5.5 Uso

| Pacote | Tela | Seção | Subseções | Laço | Leitores por id |
|---|---|---|---|---|---|
| 448 s2 (e 461 s2) | CharSel | 45 | 165 | menu_c_e.c:527..:530 | menu_c_e.c:130, :152, :169, :191 |
| 448 s2 (e 461 s2) | CharSel | 46 | 38 | menu_c_e.c:531..:534 | menu_c_e.c:109, :112 |
| 448 s2 | TeamSel | 45 | 165 | menu_e_b.c:614..:618 | menu_e_b.c:157, :181, :198, :222, :240 |
| 448 s2 | TeamSel | 46 | 38 | menu_e_b.c:619..:622 | menu_e_b.c:136, :139 |
| 450 s2 | EntrySel | 32 | 165 | menu_i_d.c:270..:273 | menu_i_d.c:31, :53, :68 |
| 451 s4 | SoloSel | 27 | 165 | menu_l_b.c:218..:221 | menu_l_b.c:30, :52 |
| 451 s4 | UbTeamSel | 27 | 165 | menu_m_b.c:378..:381 | menu_m_b.c:96, :118, :139, :146 |
| 451 s2 | MisSel | 15 | 165 | menu_o_d.c:191..:194 | menu_o_d.c:288, :290 |
| 451 s5 | SimTop (retratos) | 7 | 165 | menu_r_d.c:105..:108 | menu_r_d.c:248, :252 |
| 452 s3 | EvoZ | 36 | 165 | menu_u_h.c:668..:671 | menu_v_b.c:728 |
| 454..458 | Bracket | 23 | 165 | menu_k_f.c:192..:195 | menu_k_f.c:638, :748, :750, :768, :770 |
| 459 s4 | Replay ("chara_chip") | 8 | 165 (0xA5) | menu_za_d.c:290..:293 | menu_za_d.c:46 |
| 475 | UbRank | 6 | 165 | menu_n_c.c:211..:214 | `UR_CHIP_RES` (menu_n_c.c:29) em :332, :364, :398, :400, :411 |
| 476 | UbzSel | 6 | 165 | menu_n_b.c:143..:146 | menu_n_b.c:221, :223 |

### 5.6 Uso por pacote (para os conversores de pacote)

| Pacote | Formato (`menus-modos.md` 3) | ItemPanel | ItemHelp | ListaIds | Células | SubpacoteTex |
|---|---|---|---|---|---|---|
| 448 s2, 461 s2 | `menu_selecao` (ess.) | 53 (2 lados) | 51 | 39 (estágios), 56 (músicas) | 42 | 45 (165), 46 (38) |
| 461 s1 | `menu_treino` (ess.) | | | 24, 26 | | |
| 453 s1 | `menu_opcoes` (ess.) | | | 37 | | |
| 450 s2 | `menu_entrysel` | 39 (lado 0) | 35 | | 29 | 32 (165) |
| 451 s4 | `menu_ubsel` | 22 (lado 0) | 24 | | 30 | 27 (165) |
| 451 s2 | `menu_ub_selecao` | | | | | 15 (165) |
| 451 s5 | `menu_sim_inicio` | | | | | 7 (165) |
| 452 s2 | `menu_loja` | | 25 | | | |
| 452 s3 | `menu_evoz` | | 2 | | 33 | 36 (165) |
| 454..458 | `menu_bracket` | | | | 24 | 23 (165) |
| 459 s2 | `menu_dc_lista` | | 16 | | | |
| 459 s3 | `menu_dc_senha` | | | | 20 | |
| 459 s4 | `menu_replay` | | | | | 8 (165) |
| 472 s2 | `menu_shenlong` | | 4 do `ShenCfm` (seção 6) | | | |
| 475 | `menu_ub_ranking` | | | | | 6 (165) |
| 476 | `menu_ub_cursos` | | | | | 6 (165) |

### 5.7 O que o disco decidiu e as surpresas

Conferido pelo `conv_tool --pecas` nos 75 usos (seção 7.3), sem erro:

- N exato: ItemPanel 16, ItemHelp 6, fichas 165, ícones de estágio 38 (o código nunca lê a entrada N + 1; o conversor
  exige o N e todos os pacotes do disco o têm).
- A seção 5 do ItemPanel é filme nos cinco pacotes, também onde só o lado 0 existe (EntrySel, SoloSel, UbTeamSel).
- A seção 4 do ItemHelp (sem leitor nos nove chamadores) não tem a forma de nenhuma peça conhecida (nem filme, nem
  `TexFile`, nem tabela de seções; 14.016 bytes, a mesma nos sete pacotes): fica em bytes (`Conv_RestoBytes`), o que
  só vale porque o jogo não a lê.
- +4..+0xF das listas de ids e das células e o que vem depois da última entrada até o fim da seção: zero em todas.
- Estágios com contagem de pelo menos 36 e ids até 0x25; músicas com ids até 0x19 e contagem de pelo menos 5 (seleção)
  e 11 (Opções); formas das células só personagens (até 0xA0), ids até 0xA4; nenhuma subseção vazia.
- Os textos de item cabem no teto do `Conv_Textos`, que subiu de 0x100 para 0x400 entradas (os de item têm uma por
  item, `SAVE_ITEM_COUNT` = 350).
- 461 s2 tem as mesmas peças nas mesmas seções de 448 s2 (39, 42..46, 51, 53, 54, 56): a hipótese de layout igual
  (`menus-modos.md` 8) se sustenta nessas seções.
- A seção 20 do DcPass tem a forma Células.

Surpresas do código (para a etapa 5b):

1. **Leitores em assembly no binário original:** `ChrGrid_Build` (view_a_e.c:892) e `Train_BuildLists`
   (menu_h_d.c:555) são INCLUDE_ASM; o port compila as tentativas em C do `#if 0` (o `portsrc.py` as liga). O layout
   lido aqui vem dessas versões C e das structs.
2. **O jogo grava nas ListaIds** depois da conversão (5.3.3), já em ordem nativa; a grade de estágios lê 36 ids sem
   olhar a contagem (o conversor exige 36 ou mais na seção 39).
3. **`ItemPanel_Init` roda duas vezes** sobre o mesmo pacote no CharSel e no TeamSel (relocação repetida,
   idempotente): a conversão é por pacote, no gancho do `Sprite_Unpack`, nunca por chamada.
4. **ItemHelp aninhado** no Shenlong (472 s2, seção 6, seção 4 do `ShenCfm`): entra na etapa 9 com o `menu_shenlong`.
5. O header do CharSel diz "a pack of 57 sections" (include/menu/menu_d.h:146) e o inventário diz 56: o disco tem 56
   nos dois pacotes (`menus-telas.md` 3, etapa 5b).

## 6. Menu principal (`menu_principal`: 00449, seção 2)

`MainMenu_Init(section)` (src/menu/menu_a.c:45) descomprime a seção 2 de `gMenuArc1` (`Progress_Main` chama
`MainMenu_Run(2)`, src/menu/menu_a_b.c:132) num buffer novo (`Sprite_Unpack(pack, NULL, NULL)`, :55) e o converte logo
depois, no gancho (`Port_ConvertMenuPack(..., "menu_principal")`, :56..:61, só em `PORT_BIG_ENDIAN`). O buffer é
liberado em `MainMenu_Term` (:185, :194): cada visita ao menu descomprime e converte de novo, sem conversão dupla.

Tabela: N = 28, todas as seções lidas. Conteúdo descomprimido: 2.714.752 bytes.

| Seção | Bytes | Formato | Quem lê |
|---|---|---|---|
| 1 | 272.960 | `TexFile`, 8 registros | `MM_RES(1)` (:62), o fundo (`gMainMenu->bg`, `Sprite_DrawPicture` em `MainMenu_Draw`) |
| 2..19 | 5.504 a 264.896 | `TexFile`, 1 a 6 registros | `MM_RES(n)` (:64..:132); `MTEX(res, k)` em `gMainMenu->tex[]`, a tabela de texturas do filme |
| 20 | 129.664 | filme Flash | `Flash_Create(&gMainMenu->flash[0], MPACK_AT(res, 20), tex)` (:135); `Flash_FindLabel` e `Flash_ClipGotoLabel` (`MainMenu_StartBallLoops`, :27..:37, chamada em :401) e o tocador |
| 21 | 135.808 | JanelaMsg (2) | `MsgWin_Init(MPACK_AT(res, 21), msgText, 1, 0)` (:142) |
| 22 | 91.520 | JanelaIcone (3) | `IconWin_Init(MPACK_AT(res, 22), res)` (:138), com os ícones da 23 |
| 23 | 39.616 | `TexFile`, 2 registros | `MM_RES(23)` (:137): os ícones da janela de ícone |
| 24..26 | 53.248, 44.992, 53.248 | `TexFile`, 3, 6 e 3 registros | `MM_RES(26)` (:99), `MM_RES(24)` (:112), `MM_RES(25)` (:116) |
| 27 | 13.888 | LipPack (4), 65 falas | `gMainMenu->subtitles` (:141); `Voice_PlayWithSubtitle(subtitles, MAINMENU_VOICE_BASE, voiceLine)` (:369, :438, :536) |
| 28 | 9.344 | tabela de textos (`menus.md` 6.1), N = 64 | `gMainMenu->msgText` (:140), o texto da janela de mensagem |

"Todas as variantes": o pacote é um só (só `MainMenu_Run(2)` o abre); os ramos que a tela pode tomar (o item do
Shenlong, `items[10]`, só com as sete esferas, e o laço das esferas no filme; as duas línguas das falas; o Dragon Net
Battle escondido no 360) leem as mesmas seções, e o conversor converte todas, lidas ou não na rodada.

Conversor: `Conv_MenuPrincipal` (port/x360/conv/menus.c), que segue a tabela acima seção a seção. Registro: a linha
`{0x1C2 - 1, 0x1C2 - 1, 2, "menu_principal"}` de `sFaixas` (conv.c).

### 6.1 O Flash e o mapa por posição

A PPC com o mapa do rastreio parava no `Flash_Create` do menu principal (guia, seção 6): o rastreio vê três posições do
filme da seção 20 lidas com duas larguras, "[4, 8]" (`Flash_GetRecord`, `Flash_SkipRecords`, `FlashDraw_TransformQuad`),
e o mapa não inverte conflito. As três são o u32 `size` do cabeçalho de um registro (`FlashTag`, `menus.md` 7.2): o
`Flash_SkipRecords` lê 4 bytes, e uma cópia de 8 bytes (o cabeçalho inteiro) passa por cima. O conversor do Flash
percorre o filme pela estrutura e marca u32 nessas posições, a largura do leitor. Um quarto conflito, "[1, 4]" (só nos
roteiros das Opções, da referência e do Shenlong, `FlashTl_RunFrame`, `Flash_SkipNamed`), é o fim de uma cadeia de
rótulo: o laço `while (p[ofs++] != 0)` lê byte, e a comparação de rótulo por `strcmp` aparece no rastreio como uma
leitura da cadeia inteira; o conversor deixa bytes. O mapa por posição não serve para o Flash; o conversor serve.

## 7. Cobertura conferida

### 7.1 Contra o rastreio (`conv_tool --mapa`)

`pzs3us1/00449.bin@0xAB5C0#bpe` (o menu principal) contra o `mapa.tsv` de cada um dos onze roteiros da etapa 4:
convertido, cobertura de 100% (107.790 bytes em números trocados), e todas as posições iguais, sem nenhuma com outra
largura nem não trocada:

| Roteiro | Posições | Roteiro | Posições |
|---|---|---|---|
| duelo | 14.298 | ultimate | 12.795 |
| duelo_time | 14.298 | torneio | 13.577 |
| treino | 15.538 | evolution | 15.003 |
| opcoes | 13.698 | datacenter | 16.251 |
| historia | 12.099 | referencia | 14.343 |
| | | shenlong | 12.825 |

### 7.2 Contra o disco (`conv_tool --disco`)

2.386 arquivos sem erro (os 2.383 de antes mais 00448, 00453 e 00461, agora `menu_pacote`) e 8 conteúdos
descomprimidos sem erro (os 7 de antes mais o menu principal), todos com 100% de cobertura. Censo dos menus: 6 arquivos
convertidos de 30 (eram 3), 5 contêineres com conversor de 49 (eram 4).

### 7.3 As peças sozinhas (`conv_tool --pecas`)

Cada peça aplicada só à seção dela, nos 75 usos da tabela `sUsos` (a de 5.6 e a do Duelo, do Treino e das Opções de
1), em todo pacote do disco que a usa: os 75 convertidos sem erro e com 100% de cobertura da seção. Contra o
`mapa.tsv` de cada um dos onze roteiros (só as posições que caem nas seções das peças): nenhuma com outra largura nem
não trocada.

| Peça | Posições do rastreio conferidas (onze roteiros) |
|---|---|
| Flash (das telas do Duelo, do Treino e das Opções) | 22.602 |
| JanelaIcone | 21.652 |
| FichasPersonagem (SubpacoteTex de 165) | 7.851 |
| Dialogo | 2.011 |
| Células | 1.891 |
| ItemPanel | 1.698 |
| Textos | 1.621 |
| ItemHelp | 1.535 |
| JanelaMsg | 738 |
| IconesEstagio (SubpacoteTex de 38) | 678 |
| LipPack | 294 |
| ListaEstagios, ListaMusicas, ListaMusicasOpcoes, ListaIds | 111, 78, 21, 53 |

Seis posições (uma no Duelo, uma no Time, três no Treino, uma nas Opções) o rastreio dá como número de 4 bytes no
meio do rótulo de um quadro do Flash: o laço que procura o fim do rótulo (`FlashTl_RunFrame`, gfxm_c.c:88, que o
compilador da x86r pode trocar por `strlen`) aparece no rastreio como leitura de 4 bytes. O conversor deixa a cadeia
em bytes, que é o certo; a `conv_tool` conta essas posições à parte ("em cadeia de texto lida por inteiro": os w bytes
marcados como byte e todos texto imprimível ou o zero do fim), e uma leitura de 4 bytes sobre bytes que não são texto
continua sendo erro (visto com uma linha forjada no mapa sobre o cabeçalho "FOD" 0x11 de um filme).

Visto falhando: com as peças trocadas na tabela (a lista de estágios como células, as células como lista de estágios,
o ItemPanel como ItemHelp), a `conv_tool --pecas` sai com 1 e diz o porquê de cada uma.

### 7.4 A PPC sem mapa (o critério da etapa 5)

Nos quatro roteiros do essencial a PPC sem mapa, só com os conversores, passa do menu principal com o rastro e as
leituras de arquivo iguais aos da x86 e para na guarda da tela seguinte (`DuelMenu_Init`, `Train_Init`,
`Option_Init`). No último vblank da primeira visita ao menu, a memória das duas builds: 133 blocos do heap no lugar, e
o arquivo 00449 (1.544.708 números e bytes) e o pacote do menu (2.653.772) conformes, nenhum fora do formato
(`roteiros.sh` com `SEMMAPA=1`; plano, etapa 5a).

## 8. Hipóteses e o que fica aberto

- O N das JanelaMsg e JanelaIcone das outras telas: o conversor exige 2; o disco já o confirmou nas do Duelo, do
  Treino e das Opções (`conv_tool --pecas`, 7.3), e as dos outros modos se conferem quando o pacote delas ganhar
  conversor.
- O conteúdo da seção 4 do ItemHelp (sem leitor; fica em bytes, 5.2.3).
- O número mínimo de entradas dos textos de nomes (161, um por personagem) e de itens (350): o conversor não o
  confere (o leitor indexa pelo id); fica para o conversor de cada tela, com o caso que o disco mostrar.
- A largura dos 4 últimos bytes da assinatura do bloco de chaves (ninguém lê; ficam bytes, como em `personagem.md`
  6.6).
