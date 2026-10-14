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
