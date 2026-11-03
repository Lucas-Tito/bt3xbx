# Mapa de larguras do save

Para o conversor de save (PS2 little-endian para 360 big-endian, decisão 22 de `docs/x360/decisoes.md`): a largura
de cada número, em cada posição, dos três arquivos da pasta `BASLUS-21678DBZT3`. **A fonte única é a tabela em C**
(`port/x360/conv/save_larguras.c` e `.h`, C89, compilada no console e no PC); este documento a explica e dá a
evidência. Ferramentas Python leem a tabela pelo `port/x360/tools/save_larguras.py`, que compila e roda
`save_larguras_tool.c`; nada é duplicado.

Método: varredura de todos os acessos a `gSaveData` em `src/`, `include/` e `port/src/` (544 linhas em 75 arquivos).
O que a varredura mostrou, e que muda como se lê o `save.h`: cada tela do menu e cada módulo declara **o seu próprio
view** de `gSaveData` (`NSave`, `UoSave`, `PSave`, `UbSave`/`QSave`, `SimSave`, `SSave`, `TSave`, `MSave`, `LSave`, `VSave`,
`USave`, `ZSave`, `TsSave`, `CharSelSave`, `TourSave`, `AdxSaveView`, `BObjSave`, `FxSave`, `McFlowSave` e outros), e esses
views resolvem os blocos `u8 unkXXX[...]` do `save.h`. A largura de uma posição é o tipo com que o código a lê ou grava.
Cópias em bloco (`*gSaveData = gMcFlow->saveBuf`, `memset`, `ZSaveRec rec = *(ZSaveRec *)...`, `TourItemSet` de 16 bytes) não
dizem a largura e não contam: vale quem lê depois.

## Os arquivos

| Arquivo | Bytes | Mapa | Quem grava |
|---|---|---|---|
| `BASLUS-21678DBZT3` (dados) | 0x4000 | seção 2 | `McCard_WriteSave(0, gSaveData, 0x4000, ...)`, src/sys/mcflow_a.c:460 e :1408 |
| `icon.sys` | 964 (0x3C4) | seção 3 | `McCard_CreateSave`, src/battle/stgm_a_b.c:262..279 (`McCardIconSys`) |
| `dbzsm.ico` | 40.090 (0x9C9A) | seção 4: tudo byte | cópia do disco, `McFlow_SetupRequest`, src/sys/mcflow_a.c:160..161 |

## 1. Quanto cada categoria cobre

Arquivo de dados (0x4000 = 16.384 bytes):

| Categoria | Bytes | Parte | O que é |
|---|---|---|---|
| **tipado** pelo `save.h` | 6.322 | 38,6% | campos com tipo no `save.h` (inclui os `unkXXX` com tipo, como `unk208`), mais `item[350]` em bytes |
| **resolvido pela varredura** | 1.773 | 10,8% | blocos opacos do `save.h` cujo tipo veio dos views do menu: 748 bytes em números de 4, 224 em números de 2, 801 em bytes acessados |
| **sem acesso** | 8.289 | 50,6% | nenhum código acessa: largura 1, não troca. 7.734 em 8 blocos opacos inteiros mais 555 de enchimento dentro de registros |

`icon.sys`: 964 bytes, todos tipados por `McCardIconSys` (184 em números de 4, 4 em números de 2, 776 em bytes).
`dbzsm.ico`: 40.090 bytes, todos bytes copiados do disco.

Quando a tabela diz "largura 1" não há troca, então **o que importa para o conversor são só as faixas de largura 2, 4 e 8**:
1.588 bytes em números de 4, 5.324 em números de 2 (quase tudo `custom[]`, 97 x 48 bytes de itens), 32 em números de 8.

## 2. Arquivo de dados

Faixas na ordem do arquivo. "Tip." é tipado pelo `save.h`; "Var." é resolvido pela varredura; "S/A" é sem acesso.
Confiança: **alta** quando o código acessa o número com esse tipo e os views concordam; **média** onde um view declara outra
coisa e a escolha está na seção 5.

| Faixa | Largura | Cat. | Campo | Evidência | Conf. |
|---|---|---|---|---|---|
| 0x0000, 2 números | 4 | Tip. | soma (`unk0`, `unk4`) | `McCard_Checksum` lê e grava `buf[0]`, `buf[1]` como `u32`: src/battle/stgm_a_b.c:939..982 | alta |
| 0x0008 | 4 | Tip. | `unlockFlags` | src/sys/save.c:34; src/menu/menu_b_d.c:266 | alta |
| 0x000C | 4 | Tip. | `unkC` (`MSaveBody.level`, dificuldade) | src/sys/save.c:68; src/menu/menu_b_d.c:588, :702 | alta |
| 0x0010..0x00A0, 36 | 4 | Tip. | `slot[9]`: `flags`, `val[3]` | src/sys/save.c:49..52, :69..71; src/menu/menu_b_d.c:264; src/sys/late_a.c:509 | alta |
| 0x00A0..0x0208 (0x168) | 1 | S/A | `unkA0` | nenhum view nem código | sem acesso |
| 0x0208, 0x020C | 4 | Tip. | `unk208` (bit 0x10: recompensa), `unk20C` (capacidade / `missionPages` / `TSave.unk20C`) | src/menu/menu_n_d.c:34; src/menu/menu_o_d.c:222; src/menu/menu_q_b.c:207; src/menu/menu_s_d.c:1529; src/sys/save.c:46, :137 | alta |
| 0x0210..0x0288, 10 x 12 | | Var. | `rank[10]` (ranking da escada do modo 22): `chara` s32, `score` s32, `cleared` u8, 3 de enchimento | src/menu/menu_p_d.c:235..250 (`UbScore_AddRanking`); src/menu/menu_r_d.c:253 | alta |
| `rank[i]` +0, +4 | 4 | Var. | `chara`, `score` | idem | alta |
| `rank[i]` +8 | 1 | Var. | `cleared` | idem :246 | alta |
| `rank[i]` +9..+11 | 1 | S/A | enchimento (`SimRankRec.unk9[3]`) | include/menu/menu_r.h:108 | sem acesso |
| 0x0288 | 1 | Var. | `simCleared` (u8) | src/menu/menu_r_c.c:140..141 | média (seção 5, C1) |
| 0x0289..0x028C | 1 | S/A | enchimento | `SimSave.unk289` | sem acesso |
| 0x028C..0x073C, 100 x 12 | | Var. | `mission[100]` (as missões) | | |
| `mission[i]` +0..+4 | 1 | Var. | `cleared`, `rank`, `hours`, `minutes`, `seconds` (5 u8) | src/menu/menu_p_d.c:256..262; src/menu/menu_o_d.c:277, :307..309; src/menu/menu_p_b.c:35, :39 | alta |
| `mission[i]` +5..+7 | 1 | S/A | enchimento (`unk5[3]`) | include/menu/menu_o.h:111 | sem acesso |
| `mission[i]` +8 | 4 | Var. | `score` / `total` (s32) | src/menu/menu_p_d.c:256..257; src/menu/menu_o_d.c:315 | alta |
| 0x073C..0x0778, 5 x 12 | | Var. | `bestB[5]` (cursos dos modos 17..19: a sobrevivência) | | |
| `bestB[i]` +0, +4 | 4 | Var. | `value`, `total` | src/menu/menu_p_d.c:271, :278; src/menu/menu_s_b.c:222 (`defeated`) | alta |
| `bestB[i]` +8..+11 | 1 | Var. | `rank`, `hours`, `minutes`, `seconds` | src/menu/menu_p_d.c:274..277; src/menu/menu_s_b.c:191 | alta |
| 0x0778..0x077C | 1 | S/A | `unk778[0]` (declarado s32 nos views Q e P) | nunca acessado | sem acesso (C2) |
| 0x077C | 4 | Tip. | `unk77C` (`rank`: posição na escada, 99 por padrão) | src/sys/save.c:138; src/menu/menu_n_c.c:81 | alta |
| 0x0780..0x0A08, 54 x 12 | | Var. | `bestC[54]` (modos 24..30; as 5 primeiras são `course[5]` de `NSave`) | | |
| `bestC[i]` +0..+4 | 1 | Var. | `cleared`, `rank`, `hours`, `minutes`, `seconds` | src/menu/menu_p_d.c:287..293; src/menu/menu_n_b.c:209, :233; src/menu/menu_n_d.c:28..30 | alta |
| `bestC[i]` +5..+7 | 1 | S/A | enchimento | include/menu/menu_n.h:98 | sem acesso |
| `bestC[i]` +8 | 4 | Var. | `total` / `score` | src/menu/menu_p_d.c:287; src/menu/menu_n_b.c:239 | alta |
| 0x0A08, 0x0A0C, 0x0A10 | 4 | Tip. | `unkA08` (flags do Tour), `unkA0C` (relógio 0..23), `unkA10` | src/menu/menu_j_b.c:23..28, :33; src/menu/menu_a.c:389; src/menu/menu_i_d.c:1147 | alta |
| 0x0A14..0x0C10 (0x1FC) | 1 | S/A | `unkA14` | nenhum | sem acesso |
| 0x0C10, 0x0C18, 0x0C20 | 8 | Tip. | `charaBits[3]` (u64) | src/sys/late_a.c:337; src/battle/battle_load.c:1450; src/battle/btl_seq.c:156 (`u64 *`); src/menu/menu_c_c.c:111 | alta |
| 0x0C28 | 8 | Tip. | `stageBits` (u64) | src/battle/view_a_e.c:916; src/sys/late_a.c:335 | alta |
| 0x0C30 | 4 | Tip. | `bgmBits` (u32) | src/battle/view_a_e.c:954; src/menu/menu_b_c.c:936 | alta |
| 0x0C34..0x0C4C, 6 | 4 | Tip. | `rule[6]` | src/sys/save.c:195..200; src/menu/menu_g_c.c:138..174; src/menu/menu_c_e.c:1985; src/menu/menu_e_b.c:2166 | alta |
| 0x0C4C..0x0E0C (0x1C0) | 1 | S/A | `unkC4C` (antes) | nenhum | sem acesso |
| 0x0E0C..0x0E18, 3 | 4 | Var. | `trainClear[3]` (bits de lição, `TRAIN_CLASS_NUM` = 3) | src/menu/menu_h_d.c:17, :1129; include/menu/menu_i.h:191..195 | alta |
| 0x0E18..0x1008 (0x1F0) | 1 | S/A | `unkC4C` (depois) | nenhum | sem acesso |
| 0x1008, 0x100C | 4 | Tip. | `unk1008` (bits), `unk100C` (contador da loja) | src/menu/menu_b_c.c:64..65; src/menu/menu_v_d.c:19..40, :289 | alta |
| 0x1010..0x1208 (0x1F8) | 1 | S/A | `unk1010` | nenhum | sem acesso |
| 0x1208 | 4 | Tip. | `unk1208` (`ZSave.dcFlags`) | src/menu/menu_z_c.c:147, :149 | alta |
| 0x120C..0x1608 (0x3FC) | 1 | S/A | `unk120C` | nenhum | sem acesso |
| 0x1608..0x16B0, 42 | 4 | Tip. | `flags`, `key[2][8]`, `keyEdit[2][8]`, `unk168C[2]`, `unk1694`, `unk1698`, `screenX`, `screenY`, `soundMode`, `bgmVolume`, `seVolume` | src/menu/menu_x_c.c:456..493, :914..921; src/battle/btl_input.c:496; src/sys/adx.c:39, :306; src/sys/gfx.c:125..128; src/battle/view_b_c.c:59 | alta |
| 0x16B0..0x1808 (0x158) | 1 | S/A | `unk16B0` | nenhum | sem acesso |
| 0x1808.., 97 x 0x38 | | Tip. | `custom[97]` (`SaveCustom`) | | |
| `custom[i]` +0x00..+0x30 | 2 | Tip. | `item[3][8]` (u16) | src/menu/menu_u_h.c:211; src/menu/menu_c_e.c:1727; src/menu/menu_i_d.c:873 | alta |
| `custom[i]` +0x30 | 4 | Tip. | `unk30` (`exp`, s32) | src/menu/menu_u_g.c:167..172 | alta |
| `custom[i]` +0x34, +0x36 | 2 | Tip. | `level` (u16), `unk36` | src/menu/menu_v_b.c:693; src/menu/menu_u_g.c:183; src/battle/view_b_b.c:47, :59 | alta (`unk36` só tipado) |
| 0x2D40.., 14 x 0x1C | | | `rec[14]` (`SaveRec`) | | |
| `rec[i]` +0x00..+0x10 | 2 | Var. | `item[8]` (u16: o `u8 unk0[0x14]` do `save.h`) | src/menu/menu_z_d.c:501..503; src/menu/menu_y_b.c:714; src/menu/menu_g.c:302; include/menu/menu_z.h:81..87 | alta |
| `rec[i]` +0x10..+0x14 | 1 | S/A | `unk10` (declarado s32 em `ZSaveRec`, `TsSaveRec`, `TourSaveRec`) | nunca acessado | sem acesso (C3) |
| `rec[i]` +0x14, +0x16 | 2 | Tip. | `level`, `unk16` | src/battle/view_b_b.c:45; src/menu/menu_z_d.c:505 | alta |
| `rec[i]` +0x18 | 4 | Tip. | `chara` (s32, -1 = vazio) | src/battle/view_a_e.c:877; src/menu/menu_z_d.c:421; src/menu/menu_y_b.c:889 | alta |
| 0x2EC8..0x3026, 350 | 1 | Tip. | `item[350]` (u8, bits `OWNED`/`NEW`) | src/sys/save.c:177..179; src/menu/menu_u_h.c:316; src/menu/menu_v_b.c:278 | alta |
| 0x3026..0x3028 | 1 | S/A | `unk3026` | nenhum | sem acesso |
| 0x3028 | 4 | Tip. | `money` | src/sys/save.c:185..189; src/menu/menu_u_g.c:166 | alta |
| 0x302C..0x4000 (0xFD4) | 1 | S/A | `unk302C` | nenhum (`Uo`/`P`/`S` declaram só `unk..[]` até 0x3028) | sem acesso |

### A soma (primeiros 8 bytes)

`McCard_Checksum` (src/battle/stgm_a_b.c:939) guarda a soma em duas palavras `u32`: a palavra 0 com o byte alto da soma, a 1
com o baixo (cada uma abaixo de 0x100). A soma é calculada **sobre os bytes** depois dos 8 primeiros, pares uma vez e ímpares
vezes 256. Trocar a ordem dos números muda os bytes e portanto a soma: depois de `SaveLarg_Inverte` o conversor tem de
**regravar as duas palavras** (recalcular sobre o arquivo já convertido) e não só trocá-las. O `McCard_Checksum` do 360 ainda
recusa palavra acima de 0xFF (decisão 22, etapa 8).

## 3. icon.sys

`McCardIconSys` (include/battle/stgm_a_b.h:64..79); montado em src/battle/stgm_a_b.c:262..279 com `memset` a zero e gravado
inteiro (964 bytes) por `sceMcWrite`. Toda a estrutura é tipada, não há opaco a resolver (o `unk1C4[0x200]` é zero).

| Faixa | Largura | Campo | Confiança |
|---|---|---|---|
| 0x000..0x004 | 1 | `head` "PS2D" | alta |
| 0x004 | 2 | `unk4` (u16, atributos, zero) | alta |
| 0x006 | 2 | `titleBreak` (u16; no 360 `PS2_LOW(u16, ...)`, stgm_a_b.c:267) | alta |
| 0x008, 0x00C | 4 | `unk8` (zero), `transRate` (0x60) | alta |
| 0x010..0x0C0, 44 | 4 | `bgColor[4][4]` s32, `lightDir`, `lightColor`, `ambient` f32 (troca-se os 4 bytes do float como número) | alta |
| 0x0C0..0x3C4 (0x304) | 1 | `title` (Shift-JIS, 0x44), `viewName`, `copyName`, `delName` (0x40 cada), `unk1C4` (0x200) | alta |

O título em Shift-JIS e os nomes são texto em bytes: o arquivo do PS2 guarda os caracteres de dois bytes na ordem do código,
não como `u16` little-endian, então **não se troca**. Não há soma no `icon.sys`.

## 4. dbzsm.ico

Os 40.090 bytes (0x9C9A) são a seção 1 do pacote 00477 do disco (`gMcFlow->res[1]`), gravados sem mexer: `McFlow_SetupRequest`
(src/sys/mcflow_a.c:160..161, `iconSize = 0x9C9A`) entrega o ponteiro e o tamanho, e `McCard_CreateSave` (src/battle/stgm_a_b.c:328)
e `McCard_WriteSave` (:499) só os escrevem com `sceMcWrite(fd, file->icon, file->iconSize)`. Nenhum código do jogo monta número nele. O conversor da ISO trata esse trecho como
bytes (`Conv_MenuMcFlow`, port/x360/conv/menus.c:446..462; docs/x360/formatos/menus.md 5.3), então **o arquivo é idêntico
no PS2 e no 360 e o mapa é "tudo byte"** (uma faixa de 40.090 bytes, largura 1, categoria `disco`). Os números do formato de
ícone do PS2 que há dentro dele continuam little-endian nos dois lados, como o navegador do PS2 espera. (O ícone do replay,
`dbzsmr.ico`, 0xBE8A bytes, é do save de replay e fica fora deste mapa.)

## 5. Conflitos e escolhas

Só houve três posições com declarações diferentes entre views; nenhuma com acessos de larguras diferentes. Em todas valeu o
acesso de fato, e o que ninguém acessa ficou sem acesso:

- **C1, 0x288:** `QSaveBody`/`UbSaveBody` (menu_q.h:101, menu_p.h:215) declaram `s32 unk288`, `SimSave` declara `u8 simCleared`
  (menu_r.h:109). Só o `u8` é acessado (menu_r_c.c:140..141); o `s32` nunca. Escolhido: **1 byte** em 0x288, os outros 3 sem
  acesso. Se um dia um código ler esse `s32`, 0x288..0x28C teria de virar um número de 4. Confiança média.
- **C2, 0x778:** `unk778[2]` (s32 x 2) nos views Q e P (o S o tem como bytes) cobre 0x778 e 0x77C. O de 0x77C é tipado e acessado (`unk77C` = 99
  por padrão, `rank` em menu_n_c.c). O de 0x778 nunca é acessado. Escolhido: **sem acesso, 4 bytes de largura 1**.
- **C3, rec +0x10..0x1C:** `TsSaveRec` e `TourSaveRec` (menu_f.h:73..76, menu_j.h:91..94) declaram `s32 unk10[3]` (0x10, 0x14, 0x18), o
  `ZSaveRec` declara `s32 unk10` e depois `u16 level`, `u16 unk16`, `s32 chara`. Os acessos reais são só `level` (u16, 0x14),
  `chara` (s32, 0x18) e as 8 `item` u16. Valeu o `ZSaveRec`/`save.h`: 0x14 e 0x16 são u16, 0x18 é s32, e 0x10..0x14 sem
  acesso (largura 1). O mesmo vale em `custom[i]` +0x34: `TsSaveCustom`, `UbSaveCustom` declaram `s32 unk30[2]`, mas o
  acesso real é `level` u16 em +0x34 (menu_v_b.c:693, menu_u_g.c:183, view_b_b.c:47).

Os views de `SaveSlot` e das opções (`unk1694`, `unk1698`) concordam em s32 em todo lugar. `charaBits` e `stageBits` são `u64`
em todos os acessos (inclusive o view `u64 *` de btl_seq.c:23).

## 6. O que a varredura não prova

- "Sem acesso" quer dizer: nenhum acesso no código C da decomp e dos views dos cabeçalhos (`src/`, `include/`, `port/src/`).
  Uma função que a decomp ainda não trouxe, ou um dado que o jogo original grava e nunca lê (é o provável para os 4 KB do
  fim), não aparece. Para o conversor isso é inócuo: o que ninguém lê como número não precisa de troca, e a largura 1 devolve
  os mesmos bytes na ida e na volta.
- Larguras de 1 byte **acessadas** (801 bytes "resolvidos" e `item[]`) são por definição sem troca; estão no mapa para que a
  cobertura diga "visto", não "esquecido".
- Os blocos de enchimento dentro de registros (3 bytes após cinco `u8`, após o `cleared` do `rank`) podem esconder dados
  que uma cópia de struct leva junto; como larguras de 1 byte, passam intactos.
- A evidência vem do código do jogo; **nenhum arquivo de save real foi lido** (o mapa é feito sem dados do jogo). Uma
  conferência com um save real (por exemplo, o `.psu` convertido e carregado no 360) é o teste final.

## 7. Testes

`python3 port/x360/tools/teste_save_larguras.py` (sem dados do jogo): `save_larguras_tool confere` (cobertura exata sem
buraco nem sobreposição, larguras 1/2/4/8, alinhamento natural, troca de ordem em bytes sintéticos: duas vezes devolve o
original, uma vez inverte cada número na largura do mapa, tamanho errado recusado), a comparação número a número com
`campos_save()` e `campos_iconsys()` de `save_entre.py` (o que o `save.h` tipa é tipado no mapa e nada tipado cai nos opacos),
e nove mutantes (faixa sobreposta, buraco, larguras erradas em campos tipados, largura 3, número desalinhado, opaco marcado como
tipado, ícone com um byte a menos, `titleBreak` quebrado) que têm de sair com 1 a cada execução.
