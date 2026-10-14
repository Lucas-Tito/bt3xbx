# Menus dos modos: inventário das chamadas de `Sprite_Unpack` (F4, etapa 1)

Continuação de `menus.md` (seção 11): as 82 chamadas de `Sprite_Unpack` do código do jogo, de onde vem cada arquivo,
quem lê o conteúdo descomprimido, o formato proposto para o conversor, a tela e se ela está no caminho essencial da
decisão 22. É o mapa da etapa 5 (especificações e conversores) e da etapa 9 (os outros modos). Nenhum conversor novo
foi escrito aqui.

Fontes: o código da decomp (as linhas são as de hoje), lido por quatro agentes em paralelo, um lote de arquivos cada,
com conferência por amostra (`ModeMenu_Run` só no modo 7, `Option_Init` com `McFlow_Init` direto, `LipSync_Start`,
`Train_UpdateImage`); o censo do disco (`conv_tool --disco`, seção 6) no WSL, de onde só voltaram contagens; e o
verificador `port/x360/tools/unpacks.py` (seção 7). **Hipótese** marca o que o código não prova.

Convenções: índice = id - 1 na partição 1 (`pzs3us1/NNNNN.bin`, `comuns.md` 1.1); `baseFile` = 0x1C1 (`menus.md`
1.1); "arquivo inteiro" é o arquivo lido passado direto a `Sprite_Unpack` (um contêiner BPE no deslocamento 0, a forma
de `menu_bpe`, `menus.md` 3); "seção n" é `MPACK_AT(arquivo, n)` de um arquivo com tabela de seções (`menu_pacote`,
`menus.md` 2). Peças já especificadas: `TexFile` (`personagem.md` 4.4, `menus.md` 8), Flash (`menus.md` 7), arquivo de
diálogo (`menus.md` 6), tabela de textos u16 (`menus.md` 6.1).

## 1. Os arquivos que os menus pedem

| Arquivo | Id (como o código calcula) | Índices | Forma no disco | Quem |
|---|---|---|---|---|
| `gMenuArc5` | `baseFile` | 448 | pacote, 2 seções BPE | Duelo (`Duel_Main`, menu_g_b.c:28) |
| `gMenuArc1` | `baseFile + 1` | 449 | pacote, 2 | título, menu principal |
| `gMenuArc4` | `baseFile + 2` | 450 | pacote, 2 | Dragon World Tour (`Tour_Main`) |
| `gMenuArc3` | `baseFile + 3` | 451 | pacote, 6 | Ultimate Battle (`Ub_Main`, menu_o_c.c:63) |
| `gMenuArc7` | `baseFile + 4` | 452 | pacote, 3 | Evolution Z (menu_w_b.c:33) |
| `gMenuArc10` | `baseFile + 5` | 453 | pacote, 1 | Opções (menu_x_b.c:20) |
| chave do torneio | `baseFile + 6 + torneio` | 454..458 | BPE inteiro | `Bracket_Load` (menu_k_f.c:29) |
| `gMenuArc8` | `baseFile + 0xB` | 459 | pacote, 4 | Data Center (menu_z_b.c:26) |
| `gMenuArc2` | `baseFile + 0xC` | 460 | pacote, 4 | História (menu_b_c.c:827) |
| `gMenuArc6` | `baseFile + 13` | 461 | pacote, 2 | Treino (menu_h_c.c:67) |
| (sem leitor achado) | `baseFile + 0xE` | 462 | pacote sem seção BPE | ninguém, pelo código |
| `gMenuArc9` | `baseFile + 15` | 463 | pacote, 1 | referência de personagens (menu_h_c.c:29) |
| saga da História | `baseFile + subMenu + 0x10` | 464..471 | BPE inteiro | ModeMenu, HistOutro, HistResult |
| `gMenuArc12` | `baseFile + 0x18` | 472 | pacote, 2 | Shenlong (late_a.c:90) |
| `gMenuArc0` | `baseFile + 0x19` | 473 | pacote, 3 | cartão e logos (`menus.md` 5.1, 5.2) |
| tabuleiro do simulador | `baseFile + 0x1A` | 474 | BPE inteiro | `SimDay_Init` (menu_q_b.c:897) |
| ranking do Disc Fusion | `baseFile + 0x1B` | 475 | BPE inteiro | `UbRank_Init` (menu_n_c.c:139) |
| cursos do Disc Fusion | `baseFile + 0x1C` | 476 | BPE inteiro | `UbzSel_Init` (menu_n_b.c:97) |
| recursos do cartão | `fileBase + 0x1D` | 477 | BPE inteiro | `McFlow_Init` (`menus.md` 5.3) |
| retratos | `0x2F9 + personagem` | 760..923 | BPE inteiro | seleções, CharRef, torneio, UB, EvoZ, Data Center |
| imagens de estágio | `0x39D + estágio` (`CHARSEL_STAGE_FILE_ID`) | 924..959 | BPE inteiro | CharSel, TeamSel; 0x3C0 também é o fundo do EvoZ (menu_u_h.c:543) |
| folhas de carregamento | `0x3C1 + tipo` | 960..962 | BPE inteiro | já convertidas (`som-e-sprites.md` 2) |
| convites do torneio | `0x3C4 + convite` | 963..967 | BPE inteiro | `TourMenu_Init` (menu_j_b.c:116) |
| fundos do torneio | `0x3C9 + torneio` | 968..972 | BPE inteiro | `TourBg_Init` (via EntrySel e Bracket) |
| episódios da História | `imageBase + item`, `imageBase` de 0x3CE a 0x3FA por saga | 973..1020 | BPE inteiro | `ModeMenu_UpdateImage` (menu_a_d.c:67) |
| fundos do resultado da História | `0x3FE + bg` (bg 0..3) | 1021..1024 | BPE inteiro | `HistResult_Init` (menu_c_c.c:190) |
| páginas do Treino | `0x402 + página` | 1025.. (o fim é hipótese) | BPE inteiro | `Train_UpdateImage` (menu_h_d.c:1207) |

- **Retratos:** o código não limita o personagem. Os seletores usam 0..0xA0 (161 personagens, `SAVE_CHARA_COUNT`) e os
  marcadores 0xA1..0xA3 (aleatório, travado, custom), que dariam 921..923; 0xA4 é "vazio" e o cursor o pula. O censo
  achou os 164 arquivos 760..923 todos como BPE inteiro. Se o cursor carrega de fato 921..923 é hipótese.
- **Estágios:** 36 (0..0x23); 0x24 e 0x25 (travado, vazio) dariam 0x3C1 e 0x3C2, as folhas de carregamento, e o
  código não os carrega (hipótese: não conferido em todos os seletores).
- **Páginas do Treino:** os ids vêm de `TrainLesson.firstPage` e `pageNum` (dado, seção 22 do pacote do Treino); o
  teto da faixa não sai do código.

## 2. As peças novas (a especificar uma vez, na etapa 5)

Achadas nos leitores; os nomes são os deste inventário.

| Peça | Leitor | Forma |
|---|---|---|
| **JanelaMsg** | `MsgWin_Init(pack, text, side)` (src/battle/view_a_b.c:30) | subpacote: seção 1 `TexFile` (2 texturas), seção 2 Flash. `text` é uma tabela de textos (6.1), lida por `TextBox_AttachLine` (src/battle/view_b.c:232) |
| **JanelaIcone** | `IconWin_Init(pack, icons)` (src/battle/view_a_c.c:24) | subpacote: seção 1 `TexFile` (3 texturas), seção 2 Flash. `icons` é um `TexFile` do chamador, já relocado |
| **LipPack** (legendas e boca) | `Voice_PlayWithSubtitle` (src/battle/view_b_c.c:36), `LipSync_Start`/`LipSync_Update` (:284, :303) | tabela de pares u32 por fala (`pack[line*2+1]`, `pack[line*2+2]`, as duas línguas, `>> 2 << 2`); cada bloco: +0x08 contagem **s32** (`Mem_ReadS32`, uma leitura nativa alinhada), chaves de 4 bytes a partir de +0x10 (u16 quadro, u16 boca). Ninguém lê +0x00..+0x07 nem +0x0C..+0x0F. Não é o `BObjLipData` da luta (`personagem.md` 6.6) |
| **GetWin** | `GetWin_Init(pack, lang)` (src/battle/view_a.c:42) | seções 1..4 `TexFile`, 5 ou 6 Flash (pela língua), 7, 8, 9 e 11 textos, 10 tabela `GetWinItem` de 0x28 bytes (só o u8 `type` é lido) |
| **ItemPanel** | `ItemPanel_Init` (src/menu/menu_g.c:39) | 1, 2, 3, 6..12, 16 `TexFile`; 4 e 5 Flash; 13..15 textos |
| **ItemHelp** | `ItemHelp_Init` (src/menu/menu_v_c.c:14) | 1 e 3 `TexFile`, 2 Flash, 5 e 6 textos; a 4 sem leitor |
| **PassWin**, **PassChk**, **ShenCfm** | src/menu/menu_za_b.c:46, menu_za_c.c:23, src/sys/late_a_b.c:56 | subpacotes do Data Center e do Shenlong; o interior do PassChk não foi levantado (hipótese) |
| **ListaIds** | `MPACK_WORD`, `BgmList_ApplyUnlocks`, `StgGrid_ApplyUnlocks` | s32 contagem em +0, ids s32 a partir de +0x10; +4..+0xF sem leitor. O jogo **grava** ids nela depois da conversão (desbloqueios) |
| **Células** | `ChrGrid_Build` (src/battle/view_a_e.c:776) | s32 contagem em +0, células de 0x24 bytes a partir de +0x10 (`MChrCell`/`LChrCell`/`ESelCell`/`UChrCell`: s32 id, s32 formCount, s32 form[7]) |
| **SubpacoteTex** | laços de `Res_RelocateOffsets` sobre `MPACK_AT(seção, k)` | uma seção que é ela mesma uma tabela de seções, cada uma um `TexFile` (165 fichas de personagem; 38 de estágio) |
| tabelas de registros | cada tela | `ModeMenuDescr` (0x20, s32), `NCourse`, `NFoe`, `NRankRule`, `UbScorePrice`, `MisSelDef`, `SurvCourse`, `SimRound`..., quase todas s32; `TrainLesson` (0x10: u16, u8, u8, s32, s32, u8 x4), `LTourCpu` (s32, u16 x8) e `ZItemEntry` (0x28, bytes e campos desconhecidos) têm larguras misturadas |

## 3. Formatos propostos, por grupo

77 chamadas pendentes e 5 com gancho (mais o `Bpe_Decode` direto das animações). "Ess." = essencial pela decisão 22.

### 3.1 Já convertidos

| Formato | Chamada | Conteúdo |
|---|---|---|
| `menu_titulo` | `Title_Init`, src/menu/menu_a_c.c:56 | 449, seção 1 (`menus.md` 5.4) |
| `menu_cartao` | `BootCard_Init`, src/menu/menu_i_b.c:24 | 473, seção 2 (5.1) |
| `menu_logos` | `FirstRun_Main`, src/menu/menu_i_c.c:173 | 473, seção 3 (5.2) |
| `menu_mcflow` | `McFlow_Init`, src/sys/mcflow_a.c:2636 | 477 inteiro (5.3) |
| `menu_principal` (etapa 5a) | `MainMenu_Init`, src/menu/menu_a.c:55 | 449, seção 2 (`menus-pecas.md` 6) |
| `menu_duelo` (etapa 5b) | `DuelMenu_Init`, src/menu/menu_g_c.c:195 | 448, seção 1 (`menus-telas.md` 2) |
| `menu_selecao` (etapa 5b) | `CharSel_Init` menu_c_e.c:400 e :409, `TeamSel_Init` menu_e_b.c:469 | 448 e 461, seção 2 (`menus-telas.md` 3) |
| `menu_treino` (etapa 5b) | `Train_Init`, src/menu/menu_h_d.c:650 | 461, seção 1 (`menus-telas.md` 4) |
| `menu_opcoes` (etapa 5b) | `Option_Init`, src/menu/menu_x_c.c:39 | 453, seção 1 (`menus-telas.md` 5) |
| `menu_texfile`, `menu_fundo` (etapa 5b) | os retratos, os estágios e as páginas do Treino do essencial (as 9 chamadas de 3.2 marcadas "sim") | 760..923, 924..959 (fundo), 1025..1422 (`menus-telas.md` 6) |
| `menu_hist_sagas`, `menu_hist_lista`, `menu_hist_resultado`, `menu_hist_save`, `menu_saga`, `menu_texfile`, `menu_hist_fundo` (etapa 9a) | as 10 chamadas da História: `HistSel_Init`, `ModeMenu_Init` (2), `ModeMenu_UpdateImage`, `HistOutro_Init` (2), `HistResult_Init` (3), `HistSave_Init` | 460 s1..s4, as sagas 464..471, os episódios 973..1020 e os fundos do resultado 1021..1024 (`menus-historia.md`) |
| `menu_ub_menu`, `menu_ub_selecao`, `menu_ub_resultado`, `menu_ubsel`, `menu_sim_inicio`, `menu_discfusion`, `menu_sim_dia`, `menu_ub_ranking`, `menu_ub_cursos`, `menu_texfile` (etapa 9b) | as 22 chamadas do Ultimate Battle: `UbMenu_Init`, `MisSel_Init`, `SurvSel_Init`, os quatro resultados, `SoloSel_*` (3), `UbTeamSel_*` (3), `SimTop_Init`, `DiscFusion_Init`, `SimDay_*` (3), `UbRank_*` (3), `UbzSel_Init` | 451 s1..s6, 474, 475 e 476 inteiros e os retratos (`menus-ub.md`) |
| `folha` (`Port_ConvertLoadSheet`) | `Load_InitScreen`, src/sys/loading.c:166 | 960..962 inteiro |
| `animacao` (`Port_ConvertAnim`) | `BtlObjAnim_Load`, src/battle/bobj_a.c:368 (`Bpe_Decode` direto) | animação do lutador |

### 3.2 `menu_texfile`: imagem única (34 chamadas, 9 essenciais)

O arquivo inteiro é um contêiner BPE e o conteúdo, um `TexFile` só: `Res_RelocateOffsets(&res, res, res)` e
`MTEX(res, 0)` (às vezes 0 e 1) para a tabela de texturas de um filme, ou o fundo por `Sprite_DrawPicture`. O buffer de
saída é fixo e reaproveitado (0x20800 nos retratos, 0x43000 nos estágios, 0x10800 nas páginas do Treino, 0x41000 nos
convites): cada `Sprite_Unpack` reescreve tudo, então converter depois de cada um não converte duas vezes.

| Arquivos | Chamadas | Ess. |
|---|---|---|
| retratos 760..923 | `CharSel_UpdateFaceLoad` menu_c_e.c:226, `CharSel_Init` :722, `TeamSel_UpdateFaceLoad` menu_e_b.c:290, `TeamSel_Init` :753 | sim |
| | `CharRef_Init` menu_h_b.c:119, `CharRef_UpdateImage` :849; `EntrySel_UpdateImage` menu_i_d.c:98, `EntrySel_Init` :334; `Bracket_UpdateImages` menu_k_f.c:805, :806, `Bracket_LoadImages` :830, :831; `SoloSel_UpdateImage` menu_l_b.c:83, `SoloSel_Init` :254; `UbTeamSel_UpdateFaceLoad` menu_m_b.c:184, `UbTeamSel_Init` :421; `UbRank_UpdateFaceLoad` menu_n_c.c:714, :715; `SimDay_UpdateFaceLoad` menu_q_b.c:1970, :1971; `EvoZ_Init` menu_u_g.c:60, `EvoZ_UpdateFace` menu_v_b.c:759; `DcList_UpdateFace` menu_y_b.c:957, `DcList_Init` :1008; `DcPass_UpdateFace` menu_z_d.c:535 | não |
| estágios 924..959 | `CharSel_UpdateStageLoad` menu_c_e.c:294, `CharSel_Init` :729, `TeamSel_UpdateStageLoad` menu_e_b.c:361, `TeamSel_Init` :760 | sim |
| 959 (fundo do EvoZ) | `EvoZ_Load` menu_u_h.c:544 | não |
| páginas do Treino 1025.. | `Train_UpdateImage` menu_h_d.c:1215 | sim |
| convites 963..967 | `TourMenu_Init` menu_j_b.c:117 | não |
| episódios 973..1020 | `ModeMenu_UpdateImage` menu_a_d.c:76 | não (seção 5, achado 1); convertido na etapa 9a |
| fundos do resultado 1021..1024 | `HistResult_Init` menu_c_c.c:203 (era :191 antes dos ganchos) | não; convertido na etapa 9a (`menu_hist_fundo`) |

`DcList_Init` :1008 descomprime um retrato que ninguém lê (`DcList_UpdateFace` descomprime de novo por cima antes de
ler): o gancho vai mesmo assim, para a regra de um gancho por chamada e para a guarda.

### 3.3 Pacotes de tela do caminho essencial (6 chamadas)

| Formato | Chamada | Conteúdo | Seções, pelo leitor |
|---|---|---|---|
| `menu_principal` | `MainMenu_Init` menu_a.c:55 (modo 4) | 449, seção 2 | 28, todas lidas: 1 `TexFile` (fundo); 2..19, 23..26 `TexFile` (texturas do filme, 23 os ícones do IconWin); 20 Flash; 21 JanelaMsg; 22 JanelaIcone; 27 LipPack; 28 textos. Primeiro leitor: `Res_RelocateOffsets` da 1 |
| `menu_duelo` | `DuelMenu_Init` menu_g_c.c:195 (modo 38) | 448, seção 1 | 1..18 e 24 `TexFile`; 19 Flash; 21 e 27 textos; 22 JanelaMsg; 23 JanelaIcone (`icons` = 24); 25 diálogo (msgTbl = 27); 26 LipPack; 20 sem leitor. N = 27 é hipótese |
| `menu_selecao` | `CharSel_Init` menu_c_e.c:391 (modo 39) e `TeamSel_Init` menu_e_b.c:457 (modo 40), sobre 448 seção 2; `CharSel_Init` :388 (modo 45, Treino), sobre 461 seção 2 | 448 s2, 461 s2 | união das duas telas: `TexFile` 1..14, 16..23, 26..31, 34..36, 47..50, 55; Flash 15, 24, 25, 32, 33, 37, 38; ListaIds 39 (estágios) e 56 (músicas); Células 42; textos 43, 44; SubpacoteTex 45 (165 fichas) e 46 (38 estágios); ItemHelp 51; ItemPanel 53; JanelaIcone 54. Sem leitor: 40, 41, 52. Que 461 s2 tenha o layout de 448 s2 é hipótese (o código lê as duas iguais) |
| `menu_treino` | `Train_Init` menu_h_d.c:650 (modo 44) | 461, seção 1 | 2..4, 6..9 `TexFile` (2 o fundo); 5 ícones; 1 Flash; 10 JanelaIcone; 11 JanelaMsg; 14..17 textos; 12, 13 LipPack; 22 `TrainLesson` (larguras misturadas; o leitor é a versão C de `Train_BuildLists` que o port compila); 24 e 26 ListaIds. Sem leitor: 18..21, 23, 25 |
| `menu_opcoes` | `Option_Init` menu_x_c.c:39 (modo 62) | 453, seção 1 | N = 38 (comentário do header, hipótese): 27 `TexFile` + 30 (fundo) + 18 (ícones); 8 Flash; 31 JanelaMsg; 32 JanelaIcone; 33 e 38 textos; 35 diálogo; 36 LipPack; 37 ListaIds de músicas (contagem s32 em +0, ids em +0x10; +4..+0xF sem leitor). Sem leitor: 7 |

### 3.4 Os outros pacotes de tela (36 chamadas, etapa 9)

| Formato | Chamadas | Conteúdo | Tela |
|---|---|---|---|
| `menu_hist_sagas` | `HistSel_Init` menu_b_d.c:147 | 460 s1 | modo 6, seleção de saga (N = 40: `TexFile`, Flash, JanelaIcone, JanelaMsg, 9 textos, 9 LipPack) |
| `menu_hist_lista` | `ModeMenu_Init` menu_a_d.c:126, `HistOutro_Init` menu_b_c.c:79 | 460 s2 | modos 7 e 9 |
| `menu_hist_resultado` | `HistResult_Init` menu_c_c.c:180 | 460 s3 | modo 8 (`TexFile`, Flash, GetWin) |
| `menu_dialogo` (a forma de `menu_cartao`) | `HistSave_Init` menu_c_d.c:25 | 460 s4 | modo 10, save da História (só a seção 1, diálogo) |
| `menu_saga` | `ModeMenu_Init` menu_a_d.c:128, `HistOutro_Init` menu_b_c.c:107, `HistResult_Init` menu_c_c.c:182 | 464..471 inteiro | modos 7..9 (`TexFile`, textos, LipPack, `ModeMenuDescr`, Flash) |
| `menu_charref` | `CharRef_Init` menu_h_b.c:49 | 463 s1 | modo 60 (N = 28, com tabelas s32 de 161 entradas) |
| `menu_tourmenu` | `TourMenu_Init` menu_j_b.c:59 | 450 s1 | modo 33 (com `TourInfo` s32) |
| `menu_entrysel` | `EntrySel_Init` menu_i_d.c:166 | 450 s2 | modo 34 (Células, SubpacoteTex, ItemPanel, ItemHelp; as seções dos cinco torneios) |
| `menu_tourbg` | `TourBg_Init` menu_k_d.c:24 | 968..972 inteiro | modos 34 e 35 (2 `TexFile`, 1 Flash) |
| `menu_bracket` | `Bracket_Load` menu_k_f.c:31 | 454..458 inteiro | modo 35 (GetWin, `LTourCpu`, `LTourPrize`, diálogo) |
| `menu_ub_menu` | `UbMenu_Init` menu_p_c.c:70 | 451 s1 | modo 13 |
| `menu_ub_selecao` | `MisSel_Init` menu_o_d.c:146, `SurvSel_Init` menu_s_b.c:93 | 451 s2 | modos 14 e 17 |
| `menu_ub_resultado` | `UbResult_Init` menu_n_d.c:53, `MisResult_Init` menu_p_b.c:67, `SimResult_Init` menu_r_c.c:38, `SurvResult_Init` menu_s_c.c:30 | 451 s3 | modos 16, 19, 23, 27, 30 (cada tela lê um subconjunto; o conversor cobre a união) |
| `menu_ubsel` | `SoloSel_Init` menu_l_b.c:148, `UbTeamSel_Init` menu_m_b.c:266 | 451 s4 | modos 15, 18, 21, 25, 29 |
| `menu_sim_inicio` | `SimTop_Init` menu_r_d.c:53 | 451 s5 | modo 20 |
| `menu_discfusion` | `DiscFusion_Init` menu_o_b.c:63 | 451 s6 | modo 24 |
| `menu_sim_dia` | `SimDay_Init` menu_q_b.c:899 | 474 inteiro | modo 22 (33 seções) |
| `menu_ub_ranking` | `UbRank_Init` menu_n_c.c:140 | 475 inteiro | modo 26 |
| `menu_ub_cursos` | `UbzSel_Init` menu_n_b.c:98 | 476 inteiro | modo 28 |
| `menu_evo_topo` | `EvoTop_Init` menu_w_c.c:133 | 452 s1 | modo 48 (a seção 11 é o diálogo do DcSave) |
| `menu_loja` | `Shop_Init` menu_v_d.c:146 | 452 s2 | modo 50 |
| `menu_evoz` | `EvoZ_Load` menu_u_h.c:542 | 452 s3 | modo 49 (N = 39, hipótese) |
| `menu_dc_topo` | `DcMenu_Init` menu_z_c.c:211 | 459 s1 | modo 53 (a seção 12 é o diálogo do DcSave) |
| `menu_dc_lista` | `DcList_Init` menu_y_b.c:1002 | 459 s2 | modo 55 (`ZItemEntry`) |
| `menu_dc_senha` | `DcPass_Init` menu_z_d.c:952 | 459 s3 | modo 54 (PassChk, grade) |
| `menu_replay` | `ReplayMenu_Init` menu_za_d.c:254 | 459 s4 | modo 56 |
| `menu_shenlong` | `Shen_Init` src/sys/late_a.c:555, `ShenSave_Init` src/sys/late_a_c.c:52 | 472 s2 | modos 70 e 71 (o leitor da seção 2 é assembly, `Shen_BuildList`: o layout é hipótese) |

Seções sem leitor achado (o conversor da etapa 5 decide pela forma, como em `menus.md` 5.2): Opções 7, EvoZ 1, Loja 23,
Shenlong 12, Replay 9 (só atribuída), Duelo 20, seleção 40/41/52, Treino 18..21/23/25, CharRef 6, UB seleção 6 e 16,
UB resultado 10, MisSel 2/9/12..14, TourMenu 24, EntrySel 27/37/38, Bracket 33/38/40, SimDay 29.

## 4. Totais

| | Chamadas | Essenciais | Não essenciais |
|---|---|---|---|
| já com gancho (`menu_titulo`, `menu_cartao`, `menu_logos`, `menu_mcflow`, `folha`) | 5 | 5 (boot e save) | 0 |
| `menu_texfile` | 34 | 9 | 25 |
| pacotes de tela essenciais (`menu_principal`, `menu_duelo`, `menu_selecao` x3, `menu_treino`, `menu_opcoes`) | 7 | 7 | 0 |
| outros pacotes de tela (27 formatos) | 36 | 0 | 36 |
| **total de `Sprite_Unpack`** | **82** | **21** | **61** |

Pendentes: 77 (16 essenciais, 61 não); desde a etapa 5a, 76 (15 essenciais), com o `menu_principal`; desde a etapa
5b, 61 (nenhuma essencial), com o resto do essencial (`menus-telas.md`); desde a etapa 9a, 51, com as 10 da História
(`menus-historia.md`); desde a etapa 9b, 29, com as 22 do Ultimate Battle (`menus-ub.md`). A seção 11 de `menus.md` dizia 76, mas a lista dela tem 77 linhas, as mesmas
que o `unpacks.py` acha. Formatos novos: 33 (`menu_texfile` e 32 pacotes de tela); 6 deles no essencial
(`menu_texfile`, `menu_principal`, `menu_duelo`, `menu_selecao`, `menu_treino`, `menu_opcoes`).

O essencial pede também, no `formato_de`, os arquivos externos: 448, 453 e 461 como `menu_pacote` (449 já é), e os
retratos 760..923, os estágios 924..959 e as páginas do Treino como `menu_bpe`. Sem isso o arquivo chega cru e o jogo
cai antes do gancho (`MPACK_AT` sobre uma tabela crua, ou `Bpe_Decode` sobre um cabeçalho cru; a guarda recusa este
último com o nome de quem chamou, seção 7.2).

## 5. Achados que mudam o plano (para o Lucas)

1. **O "submenu dos modos" não é o `ModeMenu`.** `ModeMenu_Run` só roda no modo 7 de `Hist_Main` (src/menu/menu_b_c.c:861):
   é a lista de episódios de uma saga da História, e o `subMenu` vem da seleção de saga (`HistSel`). O submenu do
   Duelo é o `DuelMenu` (modo 38), e ele está no essencial aqui. Se a decisão 22 queria o `ModeMenu`, entram também
   `HistSel` (único caminho até ele), `menu_hist_lista`, `menu_saga` e as imagens 973..1020. **O Lucas confirmou o
   `DuelMenu` no essencial; o `ModeMenu` fica para a etapa 9.**
2. **O DcSave não está no caminho essencial.** `DcSave_Init` (src/menu/menu_za_e.c:19) não descomprime nada: recebe o
   arquivo de diálogo de `EvoTop_Init` (452 s1, seção 11) ou de `DcMenu_Init` (459 s1, seção 12), os dois fora do
   essencial. As Opções salvam e carregam pelo `McFlow` direto (`McFlow_Init(1)`, menu_x_c.c:139; `McFlow_Start(0)`,
   `(1)` e `(4)`), sobre o diálogo da seção 35 delas, e o `menu_mcflow` já tem gancho. Para o DcSave funcionar no 360
   é preciso converter o Data Center (`menu_dc_topo`) ou o Evolution Z (`menu_evo_topo`). **O Lucas escolheu o save das
   Opções como o essencial; o DcSave fica para a etapa 9.**
3. **Chamadas em sequência:** `ModeMenu_Init` (:126, :128), `HistOutro_Init` (:79, :107), `HistResult_Init` (:180,
   :182, :191) e os laços de retrato do `CharSel_Init` e do `TeamSel_Init` (dois retratos e um estágio seguidos). Cada
   uma precisa do gancho logo depois dela, antes da seguinte: a guarda acusa no `Sprite_Unpack` seguinte.
4. **O jogo grava nos buffers convertidos:** ListaIds 39 e 56 da seleção (`StgGrid_ApplyUnlocks`,
   `BgmList_ApplyUnlocks`, `bgmIds[n-1] = 0x18`), a lista de músicas das Opções, `descr[0].count = 2`
   (menu_a_d.c:222). Grava números nativos depois da conversão: não atrapalha, mas a comparação de memória x86 contra
   PPC tem de saber.
5. **Um conversor por pacote cobre todas as variantes,** não só o caso que a tela mostra: os cinco torneios no
   EntrySel, as seções 16 e 17 do TourMenu sem convite, as 3..5 e 18..21 do pacote de SoloSel e UbTeamSel, as 13, 14 e
   16 da seleção só no DP.

## 6. Censo do disco (`conv_tool --disco`, 13/10/2026)

O `--disco` termina com o censo por faixa (`port/x360/conv/conv_tool.c`, `censo`): para cada arquivo da faixa, se o
`formato_de` o conhece e, pela estrutura (tabela de seções que fecha no tamanho, ou contêiner BPE inteiro), quantos
contêineres BPE ele tem e quantos têm formato de conteúdo na tabela única. Só contagens voltaram do WSL.

| Faixa | Arquivos | Forma | Arquivo sem conversor | Contêineres BPE | Conteúdo sem conversor |
|---|---|---|---|---|---|
| menus 448..477 (`baseFile` + 0..0x1D) | 30 | 13 pacotes, 17 BPE inteiros | 27 (só 449, 473 e 477 têm) | 49 | 45 (só 449 s1, 473 s2 e s3, 477) |
| retratos 760..923 | 164 | 164 BPE inteiros | 164 | 164 | 164 |
| estágios 924..959 | 36 | 36 BPE inteiros | 36 | 36 | 36 |
| convites e fundos do torneio 963..972 | 10 | 10 BPE inteiros | 10 | 10 | 10 |
| imagens da História 973..1024 | 52 | 52 BPE inteiros | 52 | 52 | 52 |
| páginas do Treino 1025..1422 (o fim é hipótese) | 398 | 398 BPE inteiros | 398 | 398 | 398 |

Por arquivo, na faixa dos menus (contêineres BPE, sem conversor): 448: 2, 2; 449: 2, 1; 450: 2, 2; 451: 6, 6; 452: 3, 3;
453: 1, 1; 454..458: BPE inteiro, 1, 1 cada; 459: 4, 4; 460: 4, 4; 461: 2, 2; 462: pacote sem nenhuma seção BPE (sem
leitor achado no código); 463: 1, 1; 464..471: BPE inteiro, 1, 1 cada; 472: 2, 2; 473: 3, 1; 474..476: BPE inteiro, 1,
1 cada; 477: BPE inteiro, 1, 0.

O disco confirma a forma que o código sugere em cada faixa (pacote onde há `MPACK_AT`, BPE inteiro onde o arquivo vai
direto ao `Sprite_Unpack`). O número de seções BPE de cada pacote bate com a maior seção que o código pede: 451 tem 6
(UbMenu 1 .. DiscFusion 6), 459 tem 4 (Data Center 1..4), 460 tem 4 (História 1..4), 452 tem 3.

## 7. As redes de cobertura no código

### 7.1 `port/x360/tools/unpacks.py`

Lê `src/` pelo texto (sem build) e confere, para cada `Sprite_Unpack`: o bloco `#ifdef PORT_BIG_ENDIAN` logo depois do
comando com `Port_ConvertMenuPack(buffer, ((s32 *)origem)[0], "formato")`, `Port_ConvertLoadSheet` ou a marca
`Port_UnpackSemNumeros(buffer)`; o buffer do gancho igual ao resultado do `Sprite_Unpack`; o tamanho igual ao
`rawSize` da origem; o formato presente em `sFormatos` (conv.c); e a tabela `CHAMADORES` da guarda igual às funções que
chamam `Sprite_Unpack`. Na etapa 1: `82 chamadas de Sprite_Unpack em 62 funções: bpe-direto 1, com gancho 5, pendente 77; 0
com erro` (sai com 1 enquanto houver pendente); depois da etapa 5b, `com gancho 21, pendente 61`, nenhuma do essencial; depois da etapa 9a, `com gancho 31, pendente
51` (as 10 da História com gancho); depois da etapa 9b, `com gancho 53, pendente 29` (as 22 do Ultimate Battle). Os ganchos mudam as linhas das chamadas seguintes do mesmo arquivo: `ModeMenu_Init`
:132 e :140 (eram :126 e :128), `HistOutro_Init` :113 (era :107), `HistResult_Init` :188 e :203 (eram :182 e :191).

### 7.2 A guarda em tempo de jogo

`Sprite_Unpack` (src/sys/sprite.c, só em `PORT_BIG_ENDIAN`) anota o buffer e o endereço de retorno; o gancho limpa;
`Port_UnpackCheck` (port/x360/conv/conv_jogo.c) para o jogo com o nome de quem chamou no `Sprite_Unpack` seguinte, no
`Res_RelocateOffsets` (o primeiro leitor de quase toda tela) e no vblank. Antes de descomprimir, um cabeçalho BPE
impossível (tamanho negativo ou acima de 16 MB) também para, com o nome: é arquivo que chegou sem conversor. No 360,
uma queda com anotação pendente põe o nome no log (`AoMorrer`, main360.cpp). Detalhes em `menus.md` 4.1.

## 8. Hipóteses e o que não foi levantado

- N de cada pacote quando o código não lê a última seção (EvoZ 39, saga, Shenlong); o conversor da etapa 5 confere
  pela tabela do disco. Decididos na 5b: Duelo 27, Opções 38, seleção 56 (`menus-telas.md`).
- O conteúdo das seções sem leitor (seção 3.4) e das palavras não lidas das ListaIds (+4..+0xF).
- Que 461 seção 2 tenha o layout de 448 seção 2 (confirmado na 5b: o mesmo conversor converte as duas inteiras).
- O teto das faixas: decidido na 5b (`menus-telas.md` 6): os retratos 760..923 inteiros, as páginas 1025..1422 pelas
  aulas; os estágios 0x24 e 0x25 nunca se carregam pela grade (o cursor só para em estágio desbloqueado).
- O interior do PassChk, de `ZItemEntry`, de `DcGridCell` e da seção 2 do Shenlong (leitor em assembly).
- Os campos não nomeados dos registros de larguras misturadas (`TrainLesson`, `LTourCpu`, `ZItemEntry`).
