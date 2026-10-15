# Torneio (F4, tarefa 9c-3)

Continuação de `menus-dc.md`, `menus-pecas.md` e do inventário `menus-modos.md`.
Fontes: `TourMenu_Init` (src/menu/menu_j_b.c), `EntrySel_Init` e `EntrySel_UpdateImage`
(menu_i_d.c), `TourBg_Init` (menu_k_d.c), `Bracket_Load`, `Bracket_LoadImages` e
`Bracket_UpdateImages` (menu_k_f.c), com os leitores da chave em menu_k_b.c, menu_k_c.c
e menu_k_e.c. Conversores em `port/x360/conv/menus_torneio.c`.

O `gMenuArc4`, id `baseFile + 2` = 451, índice 450 da partição 1, é `menu_pacote`:
a seção 1 é `menu_tourmenu` (modo 33), a 2 é `menu_entrysel` (34). Os cinco arquivos
454..458 são contêineres `menu_bpe` com conteúdo `menu_bracket` (35); 963..967 são
os convites (`menu_texfile`), 968..972 os fundos (`menu_tourbg`). Os retratos já são
`menu_texfile`, índices 760..923. O gancho fica imediatamente após cada descompressão,
antes do primeiro leitor e antes da chamada seguinte.

As peças seguem `menus.md`, `menus-pecas.md` e `menus-dc.md`: tabelas com N exato,
limites, ordem e fim conferidos, números pela largura dos leitores, bytes de pixels
e texto do Flash intactos, enchimento zero. `TexFile` exige pelo menos 1 + o maior
`MTEX` acessado; um fundo de `Sprite_DrawPicture` exige 8. Textos exigem a maior linha
alcançada + 1 e legendas duas entradas por fala. As seções sem leitor ficam em bytes,
mesmo quando parecem uma peça conhecida. O conversor cobre todos os caminhos do pacote.

## 1. Menu: `menu_tourmenu`, 450 s1, N = 27

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile, fundo | 8 | `Sprite_DrawPicture` |
| 2, 3 | TexFile | 4 cada | `TM_RES`, MTEX até 3 |
| 4 | TexFile | 3 | MTEX 0..2 |
| 5 | TexFile | 2 | MTEX 0..1 |
| 6, 7 | TexFile | 1 cada | MTEX 0 |
| 8 | TexFile | 3 | MTEX 0..2 |
| 9, 10, 11 | TexFile | 1 cada | MTEX 0 |
| 12 | TexFile | 3 | MTEX 1..2 |
| 13 | TexFile | 2 | MTEX 0..1 |
| 14 | TexFile | 1 | MTEX 0 |
| 15, 18 | Flash | | `Flash_Create`, os dois filmes |
| 16, 17 | TexFile | 2 cada | `texB`, mesmo sem convite o pacote inteiro se converte |
| 19 | Textos, falas | 34 | `MsgWin_Draw`, `voiceLine` até 0x21 |
| 20 | JanelaMsg | | `MsgWin_Init` |
| 21 | JanelaIcone | | `IconWin_Init` |
| 22 | TexFile, ícones | 2 | registros 0 e 1 de `IconWin_Init` |
| 23 | LipPack | 34 falas | `TM_SAY`, `Voice_PlayWithSubtitle` |
| 24 | sem leitor | | bytes preservados |
| 25 | TexFile | 3 | MTEX 0..2 |
| 26 | TexFile | 2 | MTEX 0..1 |
| 27 | TourInfo | 5 registros de 0x64 | `info[tour].prize[2][3]`, torneio 0..4 |

A seção 27 é uma tabela de `TourInfo` (include/menu/menu_j.h), só s32, 25 por
registro. Convertem-se todos os registros inteiros que cabem, exigindo os cinco
alcançados pelo leitor e zeros depois deles. No disco são 10 registros e 24 bytes
zero. A mesma tabela aparece como `LTourPrize` na seção 41 da chave (seção 4).

## 2. Seleção: `menu_entrysel`, 450 s2, N = 39

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile | 1 | `ES_RES`, MTEX 0 |
| 2 | TexFile | 3 | MTEX 0..2 |
| 3, 10, 16 | Flash | | filmes 0, 1 e 2 |
| 4 | TexFile | 1 | MTEX 0 |
| 5 | TexFile | 3 | MTEX 0, 2 |
| 6 | TexFile | 2 | MTEX 0..1 |
| 7 | TexFile | 5 | MTEX 1..4 |
| 8 | TexFile | 1 | MTEX 0 |
| 9, 11, 12, 13, 14, 15 | TexFile | 2 cada | MTEX 0..1 |
| 17, 23 | TexFile | 2 cada | guias World e Otherworld, MTEX 0..1 |
| 18, 20, 22, 24, 25 | Flash | | filme 3 dos cinco torneios |
| 19, 21, 26, 28 | TexFile | 4 cada | guias Big, Cell e Yamcha, MTEX 0, 1, 3 |
| 27 | sem leitor | | bytes preservados |
| 29 | Células | | `ChrGrid_Build`, cabeçalho e registros de 9 s32 |
| 30, 31 | Textos | 161 cada | nomes e formas, `TextBox_AttachLine` |
| 32 | SubpacoteTex | 165 subseções exatas | laço `i < ESEL_CELL_MAX` |
| 33 | Textos, falas | 205 | `MsgWin_Draw`, até 0xCC, Yamcha |
| 34 | JanelaMsg | | `MsgWin_Init` |
| 35 | ItemHelp | | `ItemHelp_Init` |
| 36 | LipPack | 205 falas | `EntrySel_UpdateEnd`, maior fala 0xCC |
| 37, 38 | sem leitor | | bytes preservados |
| 39 | ItemPanel | | `ItemPanel_Init` |

Todos os cinco guias são convertidos, não só o World que o roteiro visita.
A imagem inicial e a atualização do cursor descomprimem o retrato para o buffer
0x20800, por arquivo `0x2F9 + personagem`, com registro 0 como leitor.

## 3. Fundos: `menu_tourbg`, 968..972, N = 3

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile, fundo | 8 | `TourBg_Draw`, `Sprite_DrawPicture` |
| 2 | TexFile | 3 ou 7 | `TourBg_Init`, conforme abaixo |
| 3 | Flash | | `Flash_Create` |

O Big usa os registros 0..6, os demais 0..2. A contagem da seção 2 tem de ser
3 ou 7, os dois layouts do formato, conferida antes do `TexFile`. Texturas adicionais
do chamador vão para `gTourBg->tex`, não vêm deste arquivo.

## 4. Chave: `menu_bracket`, 454..458

N = 43 (World, Otherworld), 44 (Big, Cell) ou 45 (Yamcha). A contagem define o
layout do guia: seção 43 com mínimo 2 nos de 43 seções, 4 nos outros; seção 44
com mínimo 4 quando presente; seção 45 com mínimo 4 só no Yamcha. Os tamanhos
exatos de cada variante são conferidos pela tabela, não pelo torneio em memória.

| Seção | Formato | Mínimo | Leitor |
|---|---|---|---|
| 1 | TexFile | 1 | `BK_RES`, MTEX 0 |
| 2 | TexFile | 6 | MTEX 0..5 |
| 3 | TexFile | 4 | MTEX 0..3 e texturas do fundo |
| 4, 10, 13, 14, 16, 29 | Flash | | seis filmes de `Bracket_Load` |
| 5 | TexFile | 1 | MTEX 0 |
| 6 | TexFile | 2 | MTEX 0..1 |
| 7 | TexFile | 1 | MTEX 0 |
| 8, 9, 11 | TexFile | 2 cada | MTEX 0..1 |
| 12 | TexFile | 1 | MTEX 0 |
| 15 | TexFile | 2 | MTEX 0..1 |
| 17 | TexFile | 1 | MTEX 0 |
| 18 | TexFile | 4 | MTEX 0..3 |
| 19 | TexFile | 2 | MTEX 0..1 |
| 20 | TexFile | 1 | MTEX 0 |
| 21, 22 | Textos | 161 cada | nomes e formas |
| 23 | SubpacoteTex | 165 subseções exatas | laço `i < LCHR_CELL_MAX` |
| 24 | Células | | `ChrGrid_Build` |
| 25 | Textos, falas | 246 | `Bracket_UpdateSeq`, até 0xF5 (Yamcha) |
| 26 | JanelaMsg | | `MsgWin_Init` |
| 27 | TexFile | 2 | MTEX 0..1 |
| 28 | TexFile | 9 | MTEX 0..8 e textura do fundo |
| 30 | TexFile | 4 | MTEX 0..3 |
| 31 | LTourCpu | 15 registros de 0x14 | nível 0..2, rodada 0..4 |
| 32 | TexFile | 1 | MTEX 0 |
| 33 | sem leitor | | bytes preservados |
| 34 | TexFile | 4 | MTEX 0..3 |
| 35 | TexFile | 2 | MTEX 0..1 |
| 36 | Diálogo | | `Dialog_Init`, textos internos |
| 37 | TexFile | 1 | MTEX 0 |
| 38 | sem leitor | | bytes preservados |
| 39 | LipPack | 246 falas | `BRK_SAY` |
| 40 | sem leitor | | bytes preservados |
| 41 | LTourPrize | 5 registros de 0x64 | `Bracket_GivePrizes`, torneio 0..4 |
| 42 | GetWin | | `GetWin_Init`, tipo 1 |
| 43, 44, 45 | TexFile | conforme variante | `Bracket_Load` e `Bracket_SetTalker` |

`LTourCpu` (include/menu/menu_l.h) é um s32 `cpuLevel` e um `LItemSet` de oito
u16. Convertem-se os 15 registros alcançados por `[level * 5 + round]`, exigindo
zero no restante (no disco, 20 bytes). `LTourPrize` tem 25 s32, igual a `TourInfo`:
os 10 registros inteiros e os 24 bytes de zeros, com mínimo 5. Não se infere
largura dos registros de itens pela aparência de palavras u32.

`Bracket_UpdateImages` lê assincronamente os dois retratos do confronto;
`Bracket_LoadImages` os relê na volta da luta. Cada descompressão recebe seu
próprio gancho `menu_texfile`, antes da próxima. Os buffers são 0x16800 para o
arquivo e 0x20800 para o conteúdo.

## 5. Disco inteiro e recusas deliberadas

O `conv_tool --disco gamedata` passa pelas duas representações das marcas (bytes
e bits, como no jogo), sem divergência e a 100% nas faixas novas: o pacote 450,
as duas seções, as cinco chaves, os cinco convites e os cinco fundos. As marcas
por conteúdo, no disco desta tarefa:

| Conteúdo | Tamanho | Bytes em números trocados | Cobertura |
|---|---:|---:|---:|
| 450 s1 | 2.366.784 | 105.110 | 100% |
| 450 s2 | 4.152.128 | 339.088 | 100% |
| 454 e 457 | 4.880.000 cada | 311.460 cada | 100% |
| 455 e 456 | 4.961.856 cada | 314.040 cada | 100% |
| 458 | 5.068.544 | 328.806 | 100% |
| 963..967 | 264.864 cada | 644 cada | 100% |
| 968, 970, 972 | 670.912 cada | 3.936 cada | 100% |
| 969 | 730.624 | 12.712 | 100% |
| 971 | 412.672 | 15.166 | 100% |

Os dados e o script ficam em `~/9c-torneio/estragos.py`, fora do git. São 50
casos de `conv_tool --formato`, 7 intactos aceitos com saída 0 e 43 cópias
estragadas recusadas com saída 1 e indicação da seção:

| Formato | Estragos recusados |
|---|---|
| menu | N = 26; seções 16 e 17 com uma textura; 33 textos; 66 entradas de legendas; prêmios com quatro registros; um u32 na cauda dos prêmios |
| seleção | N = 38; uma textura em cada guia (17, 19, 21, 23, 26, 28); 164 fichas; 204 textos; 408 legendas; 9.999 células; N = 15 no ItemPanel |
| fundo | N = 2; 2 ou 6 texturas móveis; 7 texturas no fundo; assinatura do filme zerada |
| chave | N = 42 nas três variantes; guia 43 curto nas três; guia 44 curto nas duas que o têm; guia 45 curto no Yamcha; CPU com 14 registros ou cauda não nula; prêmios curtos ou cauda não nula; 490 legendas; 245 textos; 164 fichas; N errado no GetWin e no diálogo |
| convite (`menu_texfile`) | contagem de texturas zerada |
