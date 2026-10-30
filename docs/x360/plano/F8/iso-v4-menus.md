# ISO versão 4: pacotes de menu antecipados

[Decisão 36](../../decisoes.md#36-a-conversão-continua-antecipada-e-os-pacotes-de-menu-entram-nela-29102026)

## Contrato

Os 81 sítios de `Port_ConvertMenuPack` vêm de arquivos da ISO. Nenhum pacote
nessa lista é montado apenas na memória. Os 78 sítios em `src/menu/`, os dois
em `src/sys/late_a*.c` (Shenlong) e o de `src/sys/mcflow_a.c` usam a mesma
API. A lista abaixo liga cada sítio à entrada e à seção pelo inventário da F4
(`unpacks.py` e `roteiros.py CONTEUDO/FAIXAS`), conferido contra os leitores.
As chamadas repetidas são aberturas distintas do mesmo tipo de arquivo.

`Conv_Arquivo` converte a estrutura externa pelo formato registrado e chama
`Conv_MenusISO`: descomprime cada conteúdo conhecido, aplica seu conversor
com cobertura integral, recomprime BPE e confere a descompressão resultante
contra o conteúdo esperado. Mantém posição e tamanho de cada entrada e das
seções externas. A ausência de formato ou de espaço recusa a instalação.
O núcleo é C89 e é compartilhado pelos instaladores PC e console.

`Port_ConvertMenuPack` só libera a guarda do `Sprite_Unpack` e verifica tamanho.
Assim a PPC e o 360 usam o conteúdo convertido uma única vez. `mods/` passam
por `Conv_Arquivo` ao abrir e seguem a mesma regra. A x86 e o PC permanecem
inalterados: o gancho já era exclusivo de `PORT_BIG_ENDIAN`.

## Conversão que permanece na carga

Nenhum `Port_ConvertMenuPack` permanece convertendo na carga. Os únicos
conteúdos comprimidos fora deste escopo são as animações de lutador
(`Port_ConvertAnim`) e as três folhas de carregamento (`Port_ConvertLoadSheet`).
Eles conservam o contrato anterior. Não são pacotes construídos em memória.

## Inventário completo

Os índices nos caminhos são zero-based, o id do jogo é índice + 1.

| Sítio | Origem da ISO | Formato do conteúdo |
|---|---|---|
| `src/menu/menu_a.c:55 MainMenu_Init` | `pzs3us1/00449.bin`, seção 2 | `menu_principal` |
| `src/menu/menu_a_c.c:56 Title_Init` | `pzs3us1/00449.bin`, seção 1 | `menu_titulo` |
| `src/menu/menu_a_d.c:76 ModeMenu_UpdateImage` | `pzs3us1/00973..01020.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_a_d.c:132 ModeMenu_Init` | `pzs3us1/00460.bin`, seção 2 | `menu_hist_lista` |
| `src/menu/menu_a_d.c:140 ModeMenu_Init` | `pzs3us1/00464..00471.bin`, BPE inteiro | `menu_saga` |
| `src/menu/menu_b_c.c:79 HistOutro_Init` | `pzs3us1/00460.bin`, seção 2 | `menu_hist_lista` |
| `src/menu/menu_b_c.c:113 HistOutro_Init` | `pzs3us1/00464..00471.bin`, BPE inteiro | `menu_saga` |
| `src/menu/menu_b_d.c:147 HistSel_Init` | `pzs3us1/00460.bin`, seção 1 | `menu_hist_sagas` |
| `src/menu/menu_c_c.c:180 HistResult_Init` | `pzs3us1/00460.bin`, seção 3 | `menu_hist_resultado` |
| `src/menu/menu_c_c.c:188 HistResult_Init` | `pzs3us1/00464..00471.bin`, BPE inteiro | `menu_saga` |
| `src/menu/menu_c_c.c:203 HistResult_Init` | `pzs3us1/01021..01024.bin`, BPE inteiro | `menu_hist_fundo` |
| `src/menu/menu_c_d.c:25 HistSave_Init` | `pzs3us1/00460.bin`, seção 4 | `menu_hist_save` |
| `src/menu/menu_c_e.c:226 CharSel_UpdateFaceLoad` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_c_e.c:300 CharSel_UpdateStageLoad` | `pzs3us1/00924..00959.bin`, BPE inteiro | `menu_fundo` |
| `src/menu/menu_c_e.c:400 CharSel_Init` | `pzs3us1/00461.bin`, seção 2 | `menu_selecao` |
| `src/menu/menu_c_e.c:409 CharSel_Init` | `pzs3us1/00448.bin`, seção 2 | `menu_selecao` |
| `src/menu/menu_c_e.c:746 CharSel_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_c_e.c:759 CharSel_Init` | `pzs3us1/00924..00959.bin`, BPE inteiro | `menu_fundo` |
| `src/menu/menu_e_b.c:290 TeamSel_UpdateFaceLoad` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_e_b.c:367 TeamSel_UpdateStageLoad` | `pzs3us1/00924..00959.bin`, BPE inteiro | `menu_fundo` |
| `src/menu/menu_e_b.c:469 TeamSel_Init` | `pzs3us1/00448.bin`, seção 2 | `menu_selecao` |
| `src/menu/menu_e_b.c:771 TeamSel_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_e_b.c:784 TeamSel_Init` | `pzs3us1/00924..00959.bin`, BPE inteiro | `menu_fundo` |
| `src/menu/menu_g_c.c:195 DuelMenu_Init` | `pzs3us1/00448.bin`, seção 1 | `menu_duelo` |
| `src/menu/menu_h_b.c:49 CharRef_Init` | `pzs3us1/00463.bin`, seção 1 | `menu_charref` |
| `src/menu/menu_h_b.c:125 CharRef_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_h_b.c:861 CharRef_UpdateImage` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_h_d.c:650 Train_Init` | `pzs3us1/00461.bin`, seção 1 | `menu_treino` |
| `src/menu/menu_h_d.c:1221 Train_UpdateImage` | `pzs3us1/01025..01422.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_i_b.c:24 BootCard_Init` | `pzs3us1/00473.bin`, seção 2 | `menu_cartao` |
| `src/menu/menu_i_c.c:173 FirstRun_Main` | `pzs3us1/00473.bin`, seção 3 | `menu_logos` |
| `src/menu/menu_i_d.c:98 EntrySel_UpdateImage` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_i_d.c:172 EntrySel_Init` | `pzs3us1/00450.bin`, seção 2 | `menu_entrysel` |
| `src/menu/menu_i_d.c:346 EntrySel_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_j_b.c:59 TourMenu_Init` | `pzs3us1/00450.bin`, seção 1 | `menu_tourmenu` |
| `src/menu/menu_j_b.c:123 TourMenu_Init` | `pzs3us1/00963..00967.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_k_d.c:24 TourBg_Init` | `pzs3us1/00968..00972.bin`, BPE inteiro | `menu_tourbg` |
| `src/menu/menu_k_f.c:31 Bracket_Load` | `pzs3us1/00454..00458.bin`, BPE inteiro | `menu_bracket` |
| `src/menu/menu_k_f.c:811 Bracket_UpdateImages` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_k_f.c:818 Bracket_UpdateImages` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_k_f.c:848 Bracket_LoadImages` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_k_f.c:855 Bracket_LoadImages` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_l_b.c:83 SoloSel_UpdateImage` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_l_b.c:154 SoloSel_Init` | `pzs3us1/00451.bin`, seção 4 | `menu_ubsel` |
| `src/menu/menu_l_b.c:266 SoloSel_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_m_b.c:184 UbTeamSel_UpdateFaceLoad` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_m_b.c:272 UbTeamSel_Init` | `pzs3us1/00451.bin`, seção 4 | `menu_ubsel` |
| `src/menu/menu_m_b.c:433 UbTeamSel_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_n_b.c:98 UbzSel_Init` | `pzs3us1/00476..00476.bin`, BPE inteiro | `menu_ub_cursos` |
| `src/menu/menu_n_c.c:140 UbRank_Init` | `pzs3us1/00475..00475.bin`, BPE inteiro | `menu_ub_ranking` |
| `src/menu/menu_n_c.c:720 UbRank_UpdateFaceLoad` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_n_c.c:727 UbRank_UpdateFaceLoad` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_n_d.c:53 UbResult_Init` | `pzs3us1/00451.bin`, seção 3 | `menu_ub_resultado` |
| `src/menu/menu_o_b.c:63 DiscFusion_Init` | `pzs3us1/00451.bin`, seção 6 | `menu_discfusion` |
| `src/menu/menu_o_d.c:146 MisSel_Init` | `pzs3us1/00451.bin`, seção 2 | `menu_ub_selecao` |
| `src/menu/menu_p_b.c:67 MisResult_Init` | `pzs3us1/00451.bin`, seção 3 | `menu_ub_resultado` |
| `src/menu/menu_p_c.c:70 UbMenu_Init` | `pzs3us1/00451.bin`, seção 1 | `menu_ub_menu` |
| `src/menu/menu_q_b.c:899 SimDay_Init` | `pzs3us1/00474..00474.bin`, BPE inteiro | `menu_sim_dia` |
| `src/menu/menu_q_b.c:1976 SimDay_UpdateFaceLoad` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_q_b.c:1983 SimDay_UpdateFaceLoad` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_r_c.c:38 SimResult_Init` | `pzs3us1/00451.bin`, seção 3 | `menu_ub_resultado` |
| `src/menu/menu_r_d.c:53 SimTop_Init` | `pzs3us1/00451.bin`, seção 5 | `menu_sim_inicio` |
| `src/menu/menu_s_b.c:93 SurvSel_Init` | `pzs3us1/00451.bin`, seção 2 | `menu_ub_selecao` |
| `src/menu/menu_s_c.c:30 SurvResult_Init` | `pzs3us1/00451.bin`, seção 3 | `menu_ub_resultado` |
| `src/menu/menu_u_g.c:60 EvoZ_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_u_h.c:542 EvoZ_Load` | `pzs3us1/00452.bin`, seção 3 | `menu_evoz` |
| `src/menu/menu_u_h.c:550 EvoZ_Load` | `pzs3us1/00924..00959.bin`, BPE inteiro | `menu_fundo` |
| `src/menu/menu_v_b.c:759 EvoZ_UpdateFace` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_v_d.c:146 Shop_Init` | `pzs3us1/00452.bin`, seção 2 | `menu_loja` |
| `src/menu/menu_w_c.c:133 EvoTop_Init` | `pzs3us1/00452.bin`, seção 1 | `menu_evo_topo` |
| `src/menu/menu_x_c.c:39 Option_Init` | `pzs3us1/00453.bin`, seção 1 | `menu_opcoes` |
| `src/menu/menu_y_b.c:957 DcList_UpdateFace` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_y_b.c:1008 DcList_Init` | `pzs3us1/00459.bin`, seção 2 | `menu_dc_lista` |
| `src/menu/menu_y_b.c:1020 DcList_Init` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_z_c.c:211 DcMenu_Init` | `pzs3us1/00459.bin`, seção 1 | `menu_dc_topo` |
| `src/menu/menu_z_d.c:535 DcPass_UpdateFace` | `pzs3us1/00760..00923.bin`, BPE inteiro | `menu_texfile` |
| `src/menu/menu_z_d.c:958 DcPass_Init` | `pzs3us1/00459.bin`, seção 3 | `menu_dc_senha` |
| `src/menu/menu_za_d.c:254 ReplayMenu_Init` | `pzs3us1/00459.bin`, seção 4 | `menu_replay` |
| `src/sys/late_a.c:555 Shen_Init` | `pzs3us1/00472.bin`, seção 2 | `menu_shenlong` |
| `src/sys/late_a_c.c:52 ShenSave_Init` | `pzs3us1/00472.bin`, seção 2 | `menu_shenlong` |
| `src/sys/mcflow_a.c:2636 McFlow_Init` | `pzs3us1/00477..00477.bin`, BPE inteiro | `menu_mcflow` |

## Verificação

`teste_menus_iso.py` usa um decodificador BPE independente em Python e o
conversor de conteúdo anterior como referência. Exige equivalência após a
recompressão, inclusive deslocamentos externos e comprimento do arquivo.
As cópias com conteúdo cru, fluxo alterado ou cabeçalho impossível devem
reprovar. O `--disco` continua provando também a tipagem e a cobertura dos
conteúdos por formato, separadas da recompressão.

`conv_tool --marcas` grava `.convertido` ao lado de `.orig` para os arquivos
de menu. `memdiff.py` exige a saída antecipada exata onde a x86 conserva os
bytes originais, inclusive `packSize` e fluxo. A igualdade de bytes crus não
é aceita no lugar do fluxo novo. A relocação continua comparada pelas larguras
originais. Essas referências contêm dados do jogo e ficam fora do git.

[Medido] O ensaio integral reconheceu 690 arquivos com 709 conteúdos BPE.
Todos couberam e a descompressão independente coincidiu com a conversão de
conteúdo. As folhas de carregamento (três) ficam fora desses números.
Os estragos de fluxo cru, byte, truncamento e cabeçalho impossível reprovaram,
assim como os estragos na comparação de memória. A rodada oficial com as
builds finais e os roteiros ainda precisa concluir para provar a carga.

Os resultados da rodada final ficam no relatório `~/entrega-iso-v4.md`.
