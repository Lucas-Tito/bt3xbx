# As telas do resto do caminho essencial (F4, etapa 5b)

Continuação de `menus-pecas.md` (as peças comuns e o menu principal) e de `menus-modos.md` (o inventário das 82
chamadas de `Sprite_Unpack`). Aqui ficam os pacotes de tela do Duelo, das seleções de personagem e de time, do Treino e
das Opções, e as imagens únicas que essas telas descomprimem (retratos, estágios, páginas do Treino): cada pacote é a
tabela de seções chamando as peças prontas (`menus-pecas.md` 1 e 5.6). Conversores em `port/x360/conv/menus.c`
(`Conv_MenuDuelo`, `Conv_MenuSelecao`, `Conv_MenuTreino`, `Conv_MenuOpcoes`, `Conv_MenuTexFile`, `Conv_MenuFundo`,
`Conv_LicoesTreino`), registrados na tabela única `sFaixas` de `conv.c`.

Regra da conversão (decisões 8 e 12): in-place, cada número trocado pela largura com que o código o lê, bytes e
strings intactos, enchimento exigido zero, nada muda de tamanho nem de lugar.

Fontes: o código da decomp (arquivo e linha; as linhas são as de hoje, com os ganchos); o disco inteiro pelo
`conv_tool --disco` e um script de estrutura no WSL (só contagens, tamanhos e formas voltaram); o rastreio dos
roteiros da etapa 4 pelo `conv_tool --mapa`. Nenhum byte do jogo está aqui. **Hipótese** marca o que o código não prova.

## 1. O que muda em relação às peças

- Cada `TexFile` de tela leva o mínimo de registros que o leitor usa: 1 + o maior índice de `MTEX` sobre aquela seção
  (contado no `_Init` de cada tela; nenhuma outra função relê as seções, conferido pelo `grep` dos acessos ao pacote
  fora das macros `DM_RES`, `CS_RES`, `TS_RES`, `TR_RES`, `OPT_RES`), e **8 no fundo**: `Sprite_DrawPicture`
  (src/sys/sprite.c) desenha as texturas 0..7 de uma folha como a imagem de 512x448. O código não confere a contagem;
  um índice além dela daria ao filme ou ao fundo um registro não relocado. A peça é `Conv_TexFileMin` (antes estática
  em `menus_pecas.c`).
- Seção sem leitor: convertida pela forma quando uma peça conhecida a converte inteira sem erro (o precedente é a
  seção 4 do título e a 2 dos logos, `menus.md` 5.2 e 5.4); senão fica em bytes (`Conv_RestoBytes`), o que só vale
  porque o jogo não a lê (o precedente é a seção 4 do ItemHelp, `menus-pecas.md` 5.2.3). Vazia no disco: `Conv_Vazia`
  exige que continue vazia.
- O N de cada tabela é exato (`Conv_Secoes`): o disco decidiu os que eram hipótese.

## 2. O submenu do Duelo (`menu_duelo`: 00448, seção 1)

`DuelMenu_Init(section)` (src/menu/menu_g_c.c:188) descomprime a seção 1 de `gMenuArc5` num buffer novo e converte no
gancho logo depois (:195..:201). O buffer é liberado em `DuelMenu_Term`: cada visita descomprime e converte de novo.

Tabela: N = 27 (era hipótese; o disco confirma). Conteúdo descomprimido: 3.391.808 bytes.

| Seção | Formato | Registros mínimos | Quem lê |
|---|---|---|---|
| 1 | `TexFile`, o fundo | 8 | `DM_RES(1)`, `DM->bg`, `Sprite_DrawPicture` (`DuelMenu_Draw`) |
| 2..18 | `TexFile` | 3, 4, 4, 6, 3, 3, 4, 1, 3, 2, 3, 2, 2, 2, 2, 6, 2 | `DM_RES(n)` e `MTEX` em `DM->tex[]`, a tabela de texturas do filme; a 10 também em `DM->setRes` (`MTEX(setRes, 2)`, `DuelMenu_SetupValue`) |
| 19 | filme Flash | | `Flash_Create(&DM->flash[0], MPACK_AT(DM->res, 19), DM->tex)` e o tocador |
| 20 | vazia no disco | | ninguém (`Conv_Vazia`) |
| 21 | tabela de textos | | `DM->msgText`: o texto da janela de mensagem (`MsgWin_Init`, `MsgWin_SetText`) |
| 22 | JanelaMsg | | `MsgWin_Init(MPACK_AT(DM->res, 22), DM->msgText, 1, 0)` |
| 23 | JanelaIcone | | `IconWin_Init(MPACK_AT(DM->res, 23), res)`, com os ícones da 24 |
| 24 | `TexFile`, os ícones | 1 | `DM_RES(24)` antes do `IconWin_Init` |
| 25 | arquivo de diálogo | | `Dialog_Init(MPACK_AT(DM->res, 25), DM->setText, 0)` |
| 26 | LipPack | | `DM->subtitles`, `Voice_PlayWithSubtitle(DM->subtitles, DUEL_VOICE_BASE, ...)` (:20) |
| 27 | tabela de textos | | `DM->setText`: os textos do diálogo e da janela de mensagem (:613) |

Escritas do jogo depois da conversão: só a relocação dos `TexFile` (`menus.md` 9).

## 3. As seleções (`menu_selecao`: 00448 seção 2 e 00461 seção 2)

Três chamadas descomprimem o mesmo formato: `CharSel_Init` (src/menu/menu_c_e.c) sobre a seção 2 de `gMenuArc5`
(:409, Duelo, modo 39) ou de `gMenuArc6` (:400, o CharSel do Treino, modo 0x2D), e `TeamSel_Init`
(src/menu/menu_e_b.c:469, modo 40) sobre a seção 2 de `gMenuArc5`. As duas telas leem as mesmas peças nas mesmas
seções; o conversor cobre a união.

Tabela: N = 56. O header do CharSel diz "a pack of 57 sections" (include/menu/menu_d.h); o disco tem 56 nos dois
pacotes, e nenhum leitor pede a 57. Conteúdo descomprimido: 7.074.880 bytes nos dois (00448 e 00461).

### 3.1 Seções

| Seção | Formato | Mínimo | Quem lê |
|---|---|---|---|
| 1..14, 16..23, 26..31, 34..36, 47..50, 55 | `TexFile` | 1 + o maior `MTEX` das duas telas (`minimo[]` de `Conv_MenuSelecao`); 13, 14, 16 e 50 só no TeamSel ou com dois jogadores | `CS_RES`, `TS_RES` |
| 15, 24, 25, 32, 33 | filme Flash | | `Flash_Create` das duas telas (o filme da tela, os dois lados, as duas placas) |
| 37, 38 | filme Flash | | só o TeamSel (`flash[1]`, `flash[2]`, as listas dos times) |
| 39 | ListaIds de estágios (`menus-pecas.md` 5.3), contagem pelo menos 36, ids até 0x25 | | `MPACK_WORD`, `TS_PACK_WORD`, `StgGrid_*` |
| 40 | sem leitor, 128 bytes; nenhuma peça a converte inteira | | ninguém: bytes |
| 41 | sem leitor, 640 bytes, com a forma de células | | ninguém: `Conv_Celulas` pela forma |
| 42 | Células (`menus-pecas.md` 5.4) | | `ChrGrid_Build` |
| 43, 44 | tabelas de textos (nomes de personagem e de forma) | | `TextBox_Init` |
| 45 | SubpacoteTex de 165 fichas | | o laço de relocação e os leitores por id |
| 46 | SubpacoteTex de 38 ícones de estágio | | idem |
| 51 | ItemHelp | | `ItemHelp_Init` |
| 52 | sem leitor, 14.016 bytes (o mesmo tamanho da seção 4 sem leitor do ItemHelp); nenhuma peça a converte inteira | | ninguém: bytes |
| 53 | ItemPanel (dois lados) | | `ItemPanel_Init(data, 0)` e `(data, 1)` |
| 54 | JanelaIcone, com os ícones da 55 | | `IconWin_Init` |
| 56 | ListaIds de músicas, contagem pelo menos 5, ids até 0x19 | | `MPACK_WORD`, `BgmList_ApplyUnlocks` |

### 3.2 Escritas do jogo depois da conversão

A relocação dos `TexFile` (também a das fichas e dos ícones, e a do ItemPanel duas vezes, idempotente) e as listas de
ids: `StgGrid_ApplyUnlocks` na 39, `BgmList_ApplyUnlocks` e `bgmIds[n - 5] = 0x18` na 56 (`menus-pecas.md` 5.3.3).
Gravam números nativos depois do gancho; uma comparação de memória x86 contra PPC com esta tela viva vê essas palavras
mudadas nas duas builds igualmente (o mesmo valor nativo), e elas continuam "conformes" pela largura do conversor.

## 4. O Treino (`menu_treino`: 00461, seção 1)

`Train_Init(section)` (src/menu/menu_h_d.c:643) descomprime a seção 1 de `gMenuArc6` e converte no gancho (:650..:656).
O nome de arquivo do kit de cada seção está no segundo argumento de `Train_Section` (só pista; o código não o usa).

Tabela: N = 26. Conteúdo descomprimido: 2.507.584 bytes.

### 4.1 Seções

| Seção | Formato | Mínimo | Quem lê |
|---|---|---|---|
| 1 | filme Flash | | `Flash_Create(&gTrain->flash[0], Train_Section(1), gTrain->tex)` |
| 2 | `TexFile`, o fundo | 8 | `TR_RES(2)`, `gTrain->bg`, `Sprite_DrawPicture` (`Train_DrawBg`) |
| 3, 4, 6, 7, 8, 9 | `TexFile` | 9, 5, 4, 16, 1, 5 | `TR_RES(n)` e `MTEX` em `gTrain->tex[]` |
| 5 | `TexFile`, os ícones da janela de ícone | 1 | `TR_RES(5)` antes do `IconWin_Init` |
| 10 | JanelaIcone | | `IconWin_Init(Train_Section(10), res)` |
| 11 | JanelaMsg | | `MsgWin_Init(Train_Section(11), gTrain->msgText[0], ...)` |
| 12, 13 | LipPack | | `gTrain->subtitlesA`, `subtitlesB` (as falas do Treino e do tutorial de luta) |
| 14, 15 | tabelas de textos | | `gTrain->msgText[0]`, `[1]` (a janela de mensagem) |
| 16 | tabela de textos | | `gTrain->text`, `TextBox_Init(&gTrain->box[i], gTrain->text, 2)` |
| 17 | tabela de textos | | `gTrain->pageText`, `MsgWin_SetText(gTrain->pageText)` (o texto das páginas) |
| 18, 19, 20 | vazias no disco | | ninguém |
| 21 | sem leitor, 640 bytes, com a forma das aulas (4.2) | | ninguém: `Conv_LicoesTreino` pela forma |
| 22 | as aulas (4.2) | | `gTrain->tbl.lessons`: `Train_BuildLists`, `Train_FindLesson`, `Train_FindLessonIndex`, `Train_Leave` |
| 23, 25 | sem leitor, 128 bytes, com a forma de lista de ids | | ninguém: `Conv_ListaIds` pela forma |
| 24 | ListaIds: páginas fora das listas ("tu_noraml_botu"), 24 ids, de 9 a 397; teto 397 (`Conv_ListaIdsFaixa`, como as aulas) | | `gTrain->tbl.skip`, `skipNum` (`*(u32 *)`); `Train_IsSkipped` |
| 26 | ListaIds: falas sem figura ("tu_noraml_notexture"), 27 ids | | `gTrain->tbl.noImage`, `noImageNum`; `Train_PageHasImage` |

### 4.2 As aulas (`TrainLesson`, include/menu/menu_i.h:62)

Três classes de 13, 11 e 14 aulas seguidas (`TRAIN_CLASS0_NUM` .. `TRAIN_CLASS2_NUM`); `Train_ClassLessons` acha o
começo de cada uma, e `Train_BuildLists` anda de registro em registro até o que tem `TRAIN_LESSON_LAST`. O leitor da
lista é assembly no binário original (`INCLUDE_ASM`, menu_h_d.c); o port compila a versão C do `#if 0`
(`menus-pecas.md` 5.7, surpresa 1).

| Desl. | Tipo | Quem lê | Conversão |
|---|---|---|---|
| 0x00 | u16 id (a linha do nome da aula; a chave da tabela de `Train_Leave`) | `Train_BuildLists`, `Train_FindLesson`, `Train_Leave` | u16 |
| 0x02 | u8 pageNum | `Train_BuildLists` | byte |
| 0x03 | u8 flags (`TRAIN_LESSON_TUTORIAL` 1, `_BATTLE` 2, `_LAST` 4) | `Train_BuildLists`, `Train_Leave` | byte |
| 0x04 | s32 firstPage (a linha do texto e o arquivo 0x402 + página) | `Train_BuildLists` | u32 |
| 0x08 | s32 voice | `Train_BuildLists` | u32 |
| 0x0C | u8 stage, u8 bgm, u8 chara[2] | `Train_Leave` (`BattleSetup_SetRule`, `BattleSetup_SetMember`) | bytes |

No disco: 38 registros e 0x20 bytes de zeros (640 bytes); `TRAIN_LESSON_LAST` exatamente nos registros 12, 23 e 37;
as páginas vão de 0 a 397 (os arquivos 0x402..0x58F, índices 1025..1422, até os slots de lutador em 0x590); pageNum
no máximo 22. O conversor confere: 38 registros, o `LAST` só no fim de cada classe, `pageNum <= TRAIN_PAGE_MAX` (30)
e `firstPage + pageNum <= 0x18E` (as páginas cabem nos arquivos de página); depois, zeros.

## 5. As Opções (`menu_opcoes`: 00453, seção 1)

`Option_Init(section)` (src/menu/menu_x_c.c:30) descomprime a seção 1 de `gMenuArc10` e converte no gancho
(:39..:45). Salvar e carregar passam pelo `McFlow` (`McFlow_Init(1)` e o diálogo da seção 35), cujo arquivo de
recursos (00477, `menu_mcflow`) já tinha gancho.

Tabela: N = 38 (era hipótese; o disco confirma). Conteúdo descomprimido: 5.221.056 bytes.

| Seção | Formato | Mínimo | Quem lê |
|---|---|---|---|
| 1..6, 9..29, 34 | `TexFile` | 1 + o maior `MTEX` (`minimo[]` de `Conv_MenuOpcoes`; a 19 usa o registro 9) | `OPT_RES(n)` |
| 18 | `TexFile`, os ícones da janela de ícone | 1 | `OPT_RES(18)` antes do `IconWin_Init` |
| 30 | `TexFile`, o fundo | 8 | `OPT_RES(30)`, `gOption->bg`, `Sprite_DrawPicture` (`Option_Draw`) |
| 7 | sem leitor, 128 bytes; nenhuma peça a converte inteira | | ninguém: bytes |
| 8 | filme Flash | | `Flash_Create(&gOption->flash[0], MPACK_AT(gOption->res, 8), gOption->tex)` |
| 31 | JanelaMsg | | `MsgWin_Init(MPACK_AT(gOption->res, 31), gOption->msgText, ...)` |
| 32 | JanelaIcone | | `IconWin_Init(MPACK_AT(gOption->res, 32), res)` |
| 33 | tabela de textos | | `gOption->msgText` |
| 35 | arquivo de diálogo | | `Dialog_Init(MPACK_AT(gOption->res, 35), NULL, 0)` (o diálogo do cartão) |
| 36 | LipPack | | `gOption->subtitles` |
| 37 | ListaIds de músicas, contagem pelo menos 11, ids até 0x19 | | `gOption->bgmIds`, `bgmCount` (`res[res[37] >> 2]`), `BgmList_ApplyUnlocks` |
| 38 | tabela de textos | | `gOption->dialogMsg` |

Escritas do jogo: a relocação e o `BgmList_ApplyUnlocks` na 37.

## 6. Imagens únicas (`menu_texfile`, `menu_fundo`)

O arquivo inteiro é um contêiner BPE (`menu_bpe` no arquivo, `menus.md` 3) e o conteúdo, um `TexFile` só, relocado
com a base nele mesmo. O buffer de saída é fixo e reaproveitado: cada `Sprite_Unpack` reescreve o conteúdo inteiro,
então converter depois de cada um não converte duas vezes.

| Formato | Arquivos (índices) | Chamadas | Mínimo |
|---|---|---|---|
| `menu_texfile` | retratos 0x2F9 + personagem (760..923) | `CharSel_UpdateFaceLoad` menu_c_e.c:226, `CharSel_Init` :746; `TeamSel_UpdateFaceLoad` menu_e_b.c:290, `TeamSel_Init` :771 | 1 (`MTEX(res, 0)`) |
| `menu_fundo` | estágios 0x39D + estágio (924..959) | `CharSel_UpdateStageLoad` menu_c_e.c:300, `CharSel_Init` :759; `TeamSel_UpdateStageLoad` menu_e_b.c:367, `TeamSel_Init` :784 | 8 (`Sprite_DrawPicture`) |
| `menu_texfile` | páginas do Treino 0x402 + página (1025..1422) | `Train_UpdateImage` menu_h_d.c:1221 | 1 |

As faixas (o que era hipótese no inventário, `menus-modos.md` 1 e 8):

- **Retratos 760..923 inteiros.** Todo leitor de retrato soma 0x2F9 a um personagem (`CHARSEL_FACE_FILE`,
  `TS_FACE_FILE`, `CHARREF_IMAGE_FILE` e o literal nos outros modos) e passa o arquivo ao `Sprite_Unpack` com o
  conteúdo lido como `TexFile`; nenhum outro código calcula um id nessa faixa. Os três últimos (0xA1..0xA3, os
  marcadores aleatório, travado e custom) são `TexFile` como os outros no disco (`--disco`), então entram: se o cursor
  os carrega ou não deixa de importar. Nenhum dos onze roteiros os lê.
- **Estágios 924..959.** A grade só para em estágio desbloqueado (`StgGrid_IsSelectable` e `StgGrid_Move*`: id menor
  que `STGGRID_ID_LOCKED`), e o CharSel do port leva um id além de 0x23 a um estágio do disco
  (`CHARSEL_STAGE_FILE_ID`, include/menu/menu_d.h). Os marcadores 0x24 e 0x25 do PS2 dariam 0x3C1 e 0x3C2, as folhas
  de carregamento (`folha_bpe`), que ficam como estão. **Hipótese** que continua: o TeamSel soma `TS_STAGE_FILE` direto
  (sem o ajuste do port), então um estágio acrescentado de fora do disco (`BT3_EXTRA_STAGES`, só no PC) cairia fora da
  faixa; a guarda o nomeia ("contêiner BPE cru") em vez de cair.
- **Páginas 1025..1422.** As aulas (4.2) vão da página 0 à 397, e a lista de páginas fora das listas (seção 24) também
  termina em 397: o teto sai do dado, não do código. É também o último id antes dos slots de lutador (0x590).

Antes de entrar no `formato_de`, os `lidos.txt` dos onze roteiros: os arquivos dessas faixas só aparecem lidos pelos
menus (as chamadas acima), e nenhum outro formato os lê.

## 7. Os ganchos

Um por chamada, em `#ifdef PORT_BIG_ENDIAN`, logo depois do `Sprite_Unpack` (`menus.md` 4; `unpacks.py` confere o
buffer, o tamanho e o formato): `Port_ConvertMenuPack(buffer, ((s32 *)origem)[0], "formato")`. Nas chamadas em laço
(os dois retratos do `CharSel_Init` e do `TeamSel_Init`) o gancho fica dentro do laço, antes do `Res_RelocateOffsets`.
São as 15 pendentes do essencial; com elas, `unpacks.py` dá 21 com gancho e 61 pendentes, todas dos outros modos.

## 8. Cobertura conferida

### 8.1 Contra o disco (`conv_tool --disco`)

2.984 arquivos sem erro (eram 2.386: entram os 164 retratos, os 36 estágios e as 398 páginas do Treino como
`menu_bpe`) e 611 conteúdos descomprimidos sem erro (eram 8: entram 448 s1 e s2, 453 s1, 461 s1 e s2 e as 598 imagens
únicas), todos com 100% de cobertura. Censo dos menus: 6 de 30 arquivos de menu e 10 de 49 contêineres com conversor;
retratos, estágios e páginas do Treino inteiros (164, 36 e 398 de cada). O `--pecas` continua sem erro (76 usos, com as
aulas do Treino).

### 8.2 Contra o rastreio (`conv_tool --mapa`, rodada inteira dos quatro roteiros)

Nenhuma posição com outra largura nem não trocada; as cadeias de texto lidas por inteiro são rótulos do Flash
(`menus-pecas.md` 7.3).

| Conteúdo | duelo | duelo_time | treino | opcoes |
|---|---|---|---|---|
| `menu_duelo` (448 s1) | 5.704 | 6.399 | | |
| `menu_selecao` (448 s2; 461 s2 no Treino) | 23.009 | 26.361 | 23.012 | |
| `menu_treino` (461 s1) | | | 13.895 | |
| `menu_opcoes` (453 s1) | | | | 17.063 |
| retratos (5 por roteiro), estágios (3), páginas do Treino (5) | 50, 198 | 50, 198 | 50, 198, 50 | |

Na primeira rodada, 484 mil posições da seleção saíram "não trocadas": as letras dos nomes de estágio e de música do
port (`port/src/gs/namefont.c`, só do PC) copiam os pixels e a paleta das folhas em blocos de 4 bytes, e a origem dessas
cópias ia para a pilha. O namefont agora compila sem a instrumentação na x86r (guia, seção 6).

### 8.3 A PPC sem mapa contra a x86 (o critério da etapa 5)

Os quatro roteiros chegam ao passo `fim` na PPC sem mapa, só com os conversores, com o rastro, as leituras de arquivo
e a luta iguais aos da x86 (`plano/F4/05.md`, etapa 5b). A luta: os 122 vblanks de luta do Duelo e do Time e os 120 do Treino com
vida, posição, relógio e gerador iguais (as duas builds sem janela usam o float do PS2 por software, então a igualdade
é exata; no console a luta se separa no primeiro quadro pelo float do 360, decisão 17).

### 8.4 A memória no último vblank antes do `fim`

`memdiff.py --so-em-uso --marcas-vivas` e `roteiros.py memoria`: o heap em uso no lugar (180 blocos nas lutas, 133 nas
Opções), cada bloco vivo com arquivo ou conteúdo convertido conforme número a número (16 ou 17 nas lutas: os quatro
arquivos comuns, a HUD, o cenário, os três arquivos de cada lutador, os rostos, a folha de carregamento; 6 nas Opções,
com o menu principal), e cada palavra "diferente" fora deles explicada:

| Onde | O que é | Por quê |
|---|---|---|
| heap#0 +0x290 (`SndMgr.iopBuf`) | ponteiro do host | `calloc` em 32 bits, outro em cada build |
| heap#21 +0x34 (`SaveData.unk0`, `unk4`) | a soma do save | `McCard_Checksum` sobre os bytes de números nativos |
| `gColWalkBox`, `gColWalkCtx` | ponteiros para a pilha do host | `ColMesh_WalkBox` guarda o endereço de locais (gravado, nunca lido) |
| `gPortDataStripped` | estado das tabelas do executável | a PPC liga o esqueleto (decisão 13) |
| `gSndRpcBuf` +0 | a metade alta do count u32 da fila de som sob o s16 do RPC 0xF | união s32/s16; ninguém lê |
| `gPortMoviePath`, `gPortAdxView`, `sMc` | caminhos do host | `BT3_DATA` e `BT3_SAVES` diferentes nas duas rodadas |
| heap#24 e #25 (pacotes da GPU), só no Time | `uv0.pad`, `uv1.pad` e `nop` do `GfxPostSpritePkt` | o pacote de sprite é um local copiado inteiro (`StgPanBlur_DrawView`) com esses campos nunca iniciados: lixo da pilha em bits que o GS ignora; sem nenhuma palavra das classes de troca de ordem de um pacote |

Fora da lista e sem ser diferença: `misto` (u8, u8 e u16 na mesma palavra: o `FontIconDef` e o `FlashTl` que a F3
já explicou pelo `campos.py`, e nos objetos e pools da luta; 38 a 64), `float-proximo` (8, os `cosf`/`sinf` do `EftGlow_BuildTables`: a libm do host, F2) e as classes de
largura que a F1 e a F3 já explicaram.

Duas diferenças reais que essa comparação achou, corrigidas na 5b: o `McCardIconSys.titleBreak` (o `McFlowReq` guarda
um s32 e o `McCardFile` lê u16; no big-endian vinha 0: `PS2_LOW` em `src/battle/stgm_a_b.c`) e o `mark` que a HUD grava
na folha do rosto do lutador (entrada 1 do file[0], vista `HudCTex`; `personagem.md` 4.4).

Visto falhando: numa cópia das saídas do Duelo com um u32 do 00001 sem troca e uma palavra do `BtlObjWork` estragada
no dump da PPC, o `roteiros.sh` (`REUSA=1`) sai com 1 ("fora-do-formato 1", "heap#14+0x100 diferente e não explicado",
"heap#31+0x202c ..."); o `roteiros.py memoria` com saídas forjadas acusa marca fora do formato, bloco fora do lugar,
memdiff com erro ou cortado, deslocamento ou global fora da lista, trecho não listado e pacote com palavra `u64`. Os
conversores das telas, sobre cópias estragadas dos conteúdos no WSL: N errado, filme sem "FOD", 10 estágios, 170
células, aula sem `TRAIN_LESSON_LAST`, página fora dos arquivos, fundo com 3 registros, retrato como fundo, as Opções
como Duelo: todos saem com erro e o nome da seção.

## 9. Hipóteses e o que fica aberto

- O conteúdo das seções sem leitor que ficam em bytes (seleção 40 e 52, Opções 7) e o porquê das que têm forma
  conhecida sem leitor (seleção 41, Treino 21, 23, 25): variantes de tabela que nenhum código desta versão lê.
- O TeamSel com um estágio de fora do disco (6).
- Os conteúdos que moram dentro de um buffer compartilhado (as animações descomprimidas do lutador) não se acham pelo
  começo de um bloco no `--marcas-vivas` e ficam na comparação por palavra; a F1 os conferiu na luta da demo.
