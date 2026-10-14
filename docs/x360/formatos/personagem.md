# Arquivos de personagem da luta: especificação para a conversão de ordem de bytes

Especificação, por formato e por tipo, dos três arquivos que formam o slot de recurso de um lutador
(decisão 8). Para cada entrada: o formato, a struct da decomp que o lê, o layout com a largura de cada
número, contadores e deslocamentos internos, o que vira ponteiro na carga, e como delimitar as partes
opacas. Regra da conversão: cada número de 2, 4 ou 8 bytes trocado de ordem, bytes e strings intactos,
nada muda de tamanho nem de lugar.

Fontes: o código da decomp (header e linha citados em cada seção) e, como conferência, o rastreio da luta
de demonstração no WSL (`port/build/tipos.tsv`, `port/build/mapa.tsv`, `port/build/mapa.tsv.conflitos`)
com os arquivos `pzs3us1/01473.bin`, `01481.bin`, `01482.bin` (lado 0) e `01793.bin`, `01801.bin`,
`01802.bin` (lado 1). Nenhum byte do jogo está neste documento: só contagens, deslocamentos e conclusões.

"Não determinado" quer dizer que o código não deixa claro e que não foi inventado um tipo.

## 1. O slot e os três arquivos

Quem carrega: `BtlObj_RequestCharaModel(side, chara, costume, variant)` (src/battle/btl_obj.c:513),
chamada por `battle_load.c:535` e `:536` para os dois lados, pede um slot fixo a `BtlRes_Request(1, ...)`
(btl_obj.c:1242) com três ids:

| Arquivo | Id | Buffer fixo (`BtlRes_Init`, btl_obj.c:1175) | Na demo | Conteúdo |
|---|---|---|---|---|
| file[0] | `chara*10 + costume + 0x590` (`+ 0x594` com variant) | `BTL_RES_MODEL_SIZE` 0xCE000 (buffers de reserva trocados na mudança de personagem) | 01473, 01793 | modelo, texturas, corpo, parâmetros de jogo, câmeras, texto, lábio |
| file[1] | `chara*10 + 0x598` | `BTL_RES_ANIM0_SIZE` 0x160800 | 01481, 01801 | animações comprimidas em BPE |
| file[2] | `chara*10 + 0x599` | `BTL_RES_ANIM1_SIZE` 0xCE800 | 01482, 01802 | pacotes de efeito, de câmera e de som do personagem |

Na demo, file[1] e file[2] estão 8 e 9 números depois de file[0], o que corresponde a costume 0 sem
variant. A relação exata entre o id e o número do arquivo no AFS não é tratada aqui.

`BtlRes_LoadFile` (btl_obj.c:1151) lê cada arquivo inteiro com `File_Request3` para o buffer do slot
(`BtlResFile.buf`, struct `BObjFile`/`BtlResFile`, include/battle/bobj_b.h:21 e
include/battle/btl_obj.h:192). Não há cópia nem descompressão na carga: os dados são usados no próprio
buffer. Depois `BtlObj_CreateChara` (btl_obj.c:493, chamado por src/battle/battle_work.c:106) ou
`BtlObj_Rebind` (troca de personagem) chamam `BtlObj_Setup` → `BtlObjMdl_Create` → `BtlObjMdl_Init`
(src/sys/gfxm_e_b.c:228), que roda `BtlObj_BindTables` (src/battle/bobj_a.c:2295) e
`BtlObjMdl_BindFiles` (gfxm_e_b.c:166). São elas que guardam ponteiros para as entradas no objeto
(`obj + 0x18 + campo de BObjMdl`, include/battle/bobj_b.h:236) e que fazem as primeiras relocações. Outros
módulos leem as entradas pelo objeto (parâmetros, técnicas, IA, câmera, efeitos).

Consequência para a conversão: o ponto natural é logo depois da leitura do arquivo, antes de
`BtlObjMdl_Init` (que já grava no buffer). Um arquivo de modelo ligado uma vez fica marcado (bit 31 de
flags do cabeçalho do modelo) e não é religado: a conversão tem de acontecer uma vez por leitura.

## 2. A tabela de deslocamentos (os três arquivos)

Lida por `BObjFile_GetEntry` (src/battle/bobj_a.c:2272) e pelos laços equivalentes de
`BtlObj_BindCommonTables` (bobj_a.c:2445) e `BtlScene_GetPackEntry`:

| Palavra | Tipo | Significado |
|---|---|---|
| `buf[0]` | u32 | N = número de entradas (0xFC no file[0], 0x1B8 no file[1], 6 no file[2], nos dois lados da demo) |
| `buf[n]`, n = 1..N | u32 | deslocamento em bytes da entrada n a partir do início do arquivo |
| `buf[N+1]` | u32 | tamanho do arquivo (fim da última entrada) |
| até o início da entrada 1 | u32 | zeros de enchimento (o início da entrada 1 é 0x400, 0x700 e 0x40) |

A entrada n ocupa `[buf[n], buf[n+1])` e é vazia quando os dois são iguais (`GetEntry` devolve NULL). O
código faz `buf[n] >> 2 << 2`, mas nenhum deslocamento dos seis arquivos tem os dois bits baixos ligados
(todas as entradas são alinhadas em 0x40). A entrada 0 nunca é pedida (`GetEntry(0)` daria `buf[0]`, que
é a contagem).

Como o número de entradas é conhecido: **o jogo não lê `buf[0]`**; ele pede índices fixos e confia que
existem. Para o conversor, N = `buf[0]`, e a validação é `buf[N+1]` = tamanho do arquivo, a sequência não
decrescente e a entrada 1 começando logo depois da tabela alinhada. Isso vale nos seis arquivos da demo.
O valor de N é o que o código precisa: no file[0], 0xFC = 0x35 + 200 - 1 (a última faixa de lábio da
segunda língua); no file[1], o código pede até 0x19E e as entradas 0x19F..0x1B8 existem sem leitor; no
file[2], o código pede 1..6.

Conversão: a tabela inteira, de `buf[0]` até o início da entrada 1, é u32 (as palavras de enchimento são
zero). O rastreio confirma: em 01473 e 01793, 147 palavras lidas como u32 (`BObjFile_GetEntry`), e em
01481/01801, 415.

## 3. Tabela de entradas

"Objeto" é o campo do objeto onde `BtlObj_BindTables` guarda o ponteiro (`obj + 0x18 + campo de
BObjMdl`). "Demo" diz se a luta de demonstração lê a entrada (conferido no rastreio), com o estado nos
dois arquivos quando difere. A coluna de seção aponta para o layout.

### 3.1 file[0] (01473, 01793): N = 0xFC

| Índice | Nome / uso | Formato | Struct (header) | Quem lê | Objeto | Demo | Seção |
|---|---|---|---|---|---|---|---|
| 0 | contagem N | u32 | | ninguém | | | 2 |
| 1 (0x01) | arquivo de textura extra (2 texturas) | `TexFile` | `TexFile`/`TexEntry` (include/sys/gfxm_b_b.h:47, :29) | `Res_RelocateOffsets` em `BtlObj_BindTables`; usuário de obj + 0x50 não identificado | +0x50 | só a relocação | 4.4 |
| 2 (0x02) | volumes do corpo | registros de 0x1D0 | `BobjBodyData`/`BobjBodyPart` (include/battle/bobj_a.h:225, :213) | `BtlObjBody_*`, colisão, `EftDet_*` | +0x54 | sim | 4.3 |
| 3 (0x03) | modelo: cabeçalho, lista de malhas com pacotes VIF, costura | variável | `ObjMdlFile` (include/sys/gfxm_e.h:30) e visões | `BtlObjMdl_*`, `MdlTex_RebaseChain`, `ObjDraw_*`, `BtlObj_FindBound`, cadeias, pose | +0x18, +0x40, +0x44 | sim | 4.1, 4.2 |
| 4..11 (0x04..0x0B) | geometria da boca, formas 1..8 (substitui o nó 0x30) | pacote DMA/VIF | `ObjMdl.extra` (gfxm_e.h:71) | `MdlTex_RebaseChain`, `ObjDraw_DrawParts` | +0x5C..+0x78 | 4..8 em 01473 e 4..7 em 01793 presentes e lidas pelo rebase; 9..11 vazias | 4.5 |
| 12 (0x0C) | texturas do modelo | `TexFile` | idem | `BtlObjMdl_BindFiles`, `ObjDraw_UploadTextures`, `BtlObjMdl_InitAlphaSlots` | +0x1C, +0x48 | sim | 4.4 |
| 13 (0x0D) | texturas dos olhos (piscar) | `TexFile` | `BObjTexTable` (include/battle/bobj_b.h:73) | `BtlObjMdl_CopyTexAlpha`, `BtlObj_GetFaceTexture` | +0x4C | sim | 4.4 |
| 14..17 (0x0E..0x11) | texturas da boca, formas 5..8 | `TexFile` | idem | `BObjFace_InitMouth`, `BtlObj_GetFaceTexture` | +0x8C..+0x98 | vazias | 4.4 |
| 18 (0x12) | parâmetros gerais do personagem | registro de 0xCC | `BtlParam` (include/battle/btl_tech_a.h:107) e visões | `BtlParam_*`, `BtlCharApi_*`, `BtlObjBody_Init`, `BtlObj_InitFlags` | +0x91C | sim | 5.2 |
| 19 (0x13) | golpes corpo a corpo | registros de 0x30 | `BtlAtkRecord` (btl_tech_a.h:59) | `BtlAtk_*` | +0x920 | sim | 5.3 |
| 20 (0x14) | rajadas de ki | 13 registros de 0x34 | `BtlKiBlastData` (include/battle/btl_tech_b.h:71) | `BtlKiBlast_*`, `BtlCharApi_*` | +0x924 | sim | 5.4 |
| 21 (0x15) | movimento | registro de 0xA0 | `BtlMoveData` (btl_tech_b.h:98) | `BtlMoveParam_*` | +0x928 | sim | 5.5 |
| 22 (0x16) | falas de entrada e vitória por adversário | 4 bytes por personagem | `talkTbl` (src/battle/btl_seq.c:550) | `BtlSeqIntroTalk_Setup`, `BtlSeqWinTalk_Setup` | +0x93C | não | 5.6 |
| 23 (0x17) | cadeias secundárias e limites da cabeça | registro de 0x355 | `BObjChainParam` (bobj_b.h:176) + `BtlCtlParam` (include/battle/btl_char_ctl.h:123) | `BObjChainA/B_*`, `BtlChar_UpdateLook` | +0x940 | sim | 5.7 |
| 24 (0x18) | técnicas dos espaços 2..4 | registro de 0x244 | `BtlSuperData` (btl_tech_b.h:110) + `EftSuperSrc` (include/battle/eft_h.h:181) | `BtlSuper_*`, `EftShot_BuildParam`, IA | +0x92C | sim | 5.8 |
| 25 (0x19) | habilidades dos espaços 0..1 | registro de 0xC8 | `BtlSkillData` (btl_tech_b.h:166) + `EftSkillSrc` (eft_h.h:155) | `BtlSkill_*`, `EftShot_BuildParam`, IA | +0x930 | sim | 5.9 |
| 26 (0x1A) | não ligada | não determinado | | ninguém | | vazia | 9 |
| 27 (0x1B) | parâmetros da IA | bytes | sem struct (src/battle/btl_ai_act.c, btl_ai_cond.c) | `BtlAiAct*`, `BtlAiCond*`, `AiThink_*` | +0x934 | sim | 5.10 |
| 28 (0x1C) | cortes de câmera do personagem | registros de 0x24 | `ChrCamCutDef` (include/battle/btl_char_cam.h:129) | `ChrCam_StartCut` | +0x938 | sim | 5.11 |
| 29..36 (0x1D..0x24) | animações do próprio modelo | `BobjAnim` sem compressão | `BobjAnim` (bobj_a.h:89) | `BtlObjAnim_PlayModel`, `BtlObjAnim_Load` | +0x738.. e +0x8E8.. | 29..35 vazias; 36 presente e não lida | 6.4 |
| 37..40 (0x25..0x28) | animações de câmera do personagem | `DemoCamAnim` | `DemoCamAnim` (include/battle/btl_demo_cam.h:52) | `DemoCam_PlayCharAnim0..2`, `ShenScene` | +0x908..+0x914 | vazias | 6.5 |
| 41..43 (0x29..0x2B) | animações de câmera de entrada e vitória | `DemoCamAnim` | idem | `DemoCam_PlayObjAnim` | +0xB0, +0xB4, +0xB8 | presentes, não lidas | 5.12, 6.5 |
| 44 (0x2C) | provável roteiro da lista de golpes num nono idioma | u16 (inferido) | nenhuma | nenhum leitor achado | | não | 5.13 |
| 45..52 (0x2D..0x34) | roteiro da lista de golpes do menu de pausa, um por idioma | texto u16 | `u16 *` (src/battle/hud_0.c:50) | `BtlText_*` (src/battle/btl_seq.c:202, :420) | +0xBC (só `0x2D + gProgress->unk00`) | não (46 vazia em 01473) | 5.13 |
| 53..152 (0x35..0x98) | faixas de lábio, primeira língua | `BObjLipData` | `BObjLipData` (bobj_b.h:85) | `BtlObj_SetSubState`, `BObjFace_StepLipData` | +0x758.. (com `gSaveData->flags & 1` = 0) | não | 6.6 |
| 153..252 (0x99..0xFC) | faixas de lábio, segunda língua | `BObjLipData` | idem | idem | +0x758.. (com o bit ligado) | sim (210..213, 231..233, 242..243 em 01473) | 6.6 |

### 3.2 file[1] (01481, 01801): N = 0x1B8

| Índice | Nome / uso | Formato | Struct (header) | Quem lê | Objeto | Demo | Seção |
|---|---|---|---|---|---|---|---|
| 0 | contagem N | u32 | | ninguém | | | 2 |
| 1..414 (0x001..0x19E) | movimentos do personagem (id = índice menos 1) | contêiner BPE com `BobjAnim` | `BpeHeader` (include/sys/bpe.h:7), `BobjAnim` (bobj_a.h:89) | `BtlObjAnim_Load` → `Bpe_Decode` | +0xC0.. (`anims[0x19E]`) | 68 (01481) e 83 (01801) decodificadas; há 60 (01481) e 62 (01801) "BPE vazio" nessa faixa, nenhuma decodificada | 6.1..6.3 |
| 415..440 (0x19F..0x1B8) | reserva | "BPE vazio" de 0x40 bytes | `BpeHeader` | ninguém | | não | 6.1 |

### 3.3 file[2] (01482, 01802): N = 6

| Índice | Nome / uso | Formato | Quem lê | Objeto | Demo | Seção |
|---|---|---|---|---|---|---|
| 0 | contagem N | u32 | ninguém | | | 2 |
| 1 | pacote de efeito do espaço de técnica 0 | pacote de técnica | `EftShot_InitSlot` | +0x9C | 01482: nada lido; 01802: só a carga | 7.4 |
| 2 | espaço 1 | pacote de técnica | idem | +0xA0 | só a carga | 7.4 |
| 3 | espaço 2 | pacote de técnica (sub 3 = posicionamento) | idem + `BtlChar_BindObject` | +0xA4 | 01482: só a carga; 01802: usada | 7.4 |
| 4 | espaço 3 | idem | idem | +0xA8 | usada | 7.4 |
| 5 | espaço 4 (especial) | idem | idem | +0xAC | só a carga | 7.4 |
| 6 | pacote do personagem (carga de ki, rajadas, onda de choque, rush, câmeras das técnicas, CharaFx, absorção) | pacote com 11 subentradas | `BtlScene_GetCharPackEntry`, gerenciadores de efeito | +0x58 | usada | 7.5 |

## 4. file[0]: modelo, texturas e corpo

### 4.1 Modelo (entrada 3)

Ligado por `BtlObj_BindTables` (src/battle/bobj_a.c:2304) em `BObjMdl.unk00` (objeto + 0x18) e por
`BtlObjMdl_BindFiles` (src/sys/gfxm_e_b.c:166) em `model` (objeto + 0x40) e `meshes`/`bounds`/`parts`
(objeto + 0x44 = entrada + `meshOfs`). O mesmo cabeçalho tem várias visões parciais na decomp:
`ObjMdlFile` (include/sys/gfxm_e.h:30), `ObjDrawModel` (include/sys/gfxm_d_c.h:51), `BtlObjModel`
(include/battle/btl_obj.h:88), `BobjModelHdr` (include/battle/bobj_a.h:278), `BObjModel` e
`BObjModelBytes` (include/battle/bobj_b.h:34 e :41). A entrada tem três partes: cabeçalho (0x90 bytes
nos dois arquivos da demo), lista de malhas a partir de `meshOfs`, e seção de costura opcional a partir
de `seamOfs`.

#### 4.1.1 Cabeçalho do modelo

Deslocamentos a partir do início da entrada.

| Desl. | Tipo | Largura | Campo e significado | Quem lê |
|---|---|---|---|---|
| 0x00 | ? | | 12 bytes não lidos por nenhuma visão: não determinado | |
| 0x0C | u32 | 4 | flags: bit 0 desvanece com a distância (`BOBJ_MODEL_FADE`); 0x10000/0x20000/0x40000 classes de partes; 0xC00000 boca com mandíbula; 0x1000000 cor A / luz da câmera; bit 31 `bound` (o jogo grava 1 na primeira ligação) | `BtlObj_InitFlags`, `BtlObjMdl_BindFiles`, `BtlObjMdl_HasJaw`, `BtlObj_IsNodeShown`, `ObjDraw_*` |
| 0x0F | (byte de flags) | | `BObjModelBytes.flagsTop`: o código original lê o byte alto de flags como char (bit 0 = bit 24). É uma visão de outra largura (classe 4 do guia), não um campo próprio: a conversão trata 0x0C como u32 e o código precisa de correção (`BtlObj_InitFlags` e `BtlObj_SetColorPreset`, src/battle/bobj_a.c:2389 e :2401; ainda sem `PS2_LOW` nem entrada em visoes_resolvidas.tsv) | idem |
| 0x10 | f32 | 4 | fadeNear (0 = 40.0) | `BtlObj_UpdateView` (src/battle/btl_obj.c:829) |
| 0x14 | f32 | 4 | fadeFar (0 = 120.0) | idem |
| 0x18 | f32[4] | 4 | size[0..3]: copiados para `BobjBody.size` (objeto + 0xFF4); size[0] é a escala do corpo, size[3] serve de raio quando o bloco de parâmetros não dá um | `BtlObjBody_Init` (bobj_a.c:1141), `ObjShadow_*`, `BtlCharApi_*` |
| 0x28 | s32 | 4 | alphaRow: gravado na ligação (`BtlObjVis_AllocFreeBit`) | `BtlObjMdl_Destroy`, `BtlObj_FillAlphaRow` |
| 0x2C | u8[16] | 1 | alpha base por material: gravado por `BtlObjMdl_InitAlphaSlots` | `BtlObj_UpdateAlpha` |
| 0x3C | ? | | 16 bytes não lidos: não determinado | |
| 0x4C | u8 | 1 | faceTex: textura substituída pelo olho ou pela boca | `ObjDraw_UploadTextures`, `BtlObjMdl_CopyTexAlpha` |
| 0x4D | u8 | 1 | fadeTex | `BtlObjDraw_DrawModelFade` |
| 0x4E | u8 | 1 | texFirst | `ObjDraw_UploadTextures`, `BtlObjMdl_InitAlphaSlots` |
| 0x4F | u8 | 1 | texCount | idem |
| 0x50 | u8 | 1 | clutFirst | `ObjDraw_UploadTextures` |
| 0x51 | u8 | 1 | clutCount | idem |
| 0x52 | u8 | 1 | fadeFirst | `BtlObjDraw_DrawModelFade` |
| 0x53 | u8 | 1 | fadeCount | idem |
| 0x54 | u8 | 1 | texNoAlpha (0 = nenhuma) | `BtlObjMdl_InitAlphaSlots` |
| 0x55 | ? | | 3 bytes não lidos: não determinado | |
| 0x58 | s32 | 4 | tbp: gravado na ligação (0x3480) | `ObjDraw_*`, `ObjSeam_Draw` |
| 0x5C | s32 | 4 | cbp: gravado (0x3C00) | idem |
| 0x60 | s32 | 4 | clutTbp / cbp2: gravado (0x3D00) | idem |
| 0x64 | s32 | 4 | fadeTbp / tbp2: gravado (0x3D40) | idem |
| 0x68 | s32 | 4 | fadeCbp / unk68: gravado (0x3E40) | idem |
| 0x6C | s32 | 4 | meshOfs: deslocamento em bytes da lista de malhas, a partir do início da entrada (0x90 na demo) | `BtlObjMdl_BindFiles` |
| 0x70 | s32 | 4 | seamOfs: deslocamento em bytes da seção de costura a partir do início da entrada; 0 = não há (0 nos dois arquivos da demo) | `ObjSeam_Transform`, `ObjSeam_Draw` (src/sys/gfxm_d_c.c:73 e :95) |
| 0x74 | ? | | 0x1C bytes até `meshOfs`, não lidos: não determinado | |

Os campos gravados na ligação (0x28, 0x2C..0x3B, 0x58..0x68, bit 31 de 0x0C) são escritos antes de
qualquer leitura, mas o bit `bound` precisa começar em 0, o que vale nas duas ordens. Converter pela
largura do tipo é inofensivo. O que o arquivo traz nos campos não determinados não foi conferido byte a
byte; se forem zeros, a largura é indiferente.

### 4.2 Lista de malhas (registros de nó)

Registros de tamanho variável, encadeados por deslocamento, um por nó do esqueleto em ordem
pai-primeiro. Visões: `ObjMdlMesh` (gfxm_e.h:52), `ObjDrawPart` (gfxm_d_c.h:74), `BtlObjBound`
(btl_obj.h:73), `BObjBound` (bobj_b.h:54), `BobjBone` (bobj_a.h:111). O laço termina no registro com
`last` diferente de zero (66 registros em 01473, 60 em 01793).

| Desl. | Tipo | Largura | Campo e significado |
|---|---|---|---|
| 0x00 | s32 | 4 | next: deslocamento em bytes até o próximo registro, a partir deste (não vira ponteiro; é somado a cada passo) |
| 0x04 | u16 | 2 | pop: quantos níveis a hierarquia sobe depois deste nó |
| 0x06 | u16 | 2 | last: diferente de zero no último registro |
| 0x08 | u16 | 2 | enabled: 0 = registro sem malha (pulado no desenho, nos limites e no rebase de texturas) |
| 0x0A | u16 | 2 | node: id do nó do modelo |
| 0x0C | u16 | 2 | flags (`ObjDrawPart`): bit 0 = desenhado pela passada de desvanecimento. `BObjBound`/`BobjBone` declaram aqui um s32 sem uso; vale a visão que lê (u16) |
| 0x0E | u16 | 2 | unk0E: não lido |
| 0x10 | f32[4] | 4 | ofsA / origin: posição do nó no espaço do modelo |
| 0x20 | f32[4] | 4 | ofsB / unk20 |
| 0x30 | f32[4] | 4 | rest: translação a partir do pai na pose de repouso |
| 0x40 | f32[4] | 4 | center (só registros habilitados) |
| 0x50 | f32[4] | 4 | extent: meias-dimensões; w (+0x5C) é o "fator de tamanho" de `BtlCharApi_GetPartUnk5C` |
| 0x60 | fluxo | | só registros habilitados: pacote DMA/VIF (abaixo) |

Delimitação: registro desabilitado tem 0x40 bytes (next = 0x40; não tem center, extent nem fluxo).
Registro habilitado vai até o fim do pacote DMA mais 16 bytes (a etiqueta de fim, abaixo); em todos os
habilitados da demo, next = (fim do pacote + 16) menos o início do registro.

O último registro (`last` diferente de zero) é desabilitado e tem next = 0x40 em todos os 698 modelos do
disco (os file[0] com tabela de 0xFC entradas). O que vem depois dele depende de `seamOfs` (cabeçalho +
0x70), e é esse o critério do conversor:

| `seamOfs` | Depois dos 0x40 bytes do último registro | Modelos no disco | Conversão |
|---|---|---|---|
| 0 | 0x40 bytes até o fim da entrada; em todos os 625 modelos há palavras não nulas entre +0x08 e +0x3F desse trecho (não têm a forma de uma seção de costura); ninguém lê (o laço das malhas para no registro com `last`, e nenhum leitor soma `next` ao último) | 625 | bytes, sem exigir zero (trecho comprovadamente não lido) |
| diferente de 0 | a seção de costura (4.2.2), que começa exatamente aí: `seamOfs` = início do último registro + 0x40 menos o início da entrada, nos 73 casos | 73 (nenhum na demo) | 4.2.2; depois do último triângulo, zeros até o fim da entrada (exigir zero) |

Era isso que o conversor não previa: ele marcava como bytes tudo o que vinha depois do último registro e
só então convertia a costura, que está nesse mesmo trecho ("convertido duas vezes (marca 1, agora 4)",
os 73 erros da entrada 3). A ordem certa é: percorrer as malhas, guardar o fim do último registro; se
`seamOfs` = 0, o resto é bytes; senão, conferir `seamOfs` = fim do último registro, converter a costura e
exigir zero no que sobrar.

#### 4.2.1 Pacote DMA/VIF de malha (registro + 0x60, e entradas 4..11)

Formato verificado nos 46 + 44 registros habilitados e nas 9 cadeias de boca da demo:

1. Uma etiqueta DMA de 16 bytes. Palavra 0 (u32): `qwc` nos bits 0..15 e id = 6 (`ret`) nos bits 28..30;
   palavra 1 (u32): endereço, 0. Palavras 2 e 3 são os dois primeiros códigos VIF do fluxo (a DMA do
   VIF1 corre com TTE ligado: `Vu1Pkt_CallProg0`/`1`/`2` chamam a cadeia com uma etiqueta `call`,
   src/sys/vu1_packet.c:84).
2. Fluxo VIF de (etiqueta + 8) até (etiqueta + 16 + qwc x 16): repetição de lotes
   `UNPACK V4-32 num=5 addr=0 TOPS` (palavra 0x6C058000), `UNPACK V4-32 num=3n addr=5 TOPS`, `MSCNT`,
   com códigos `NOP` de enchimento. Na demo: 679 + 429 lotes nas malhas e 138 + 133 nas cadeias de boca,
   todos assim; nenhum outro formato de UNPACK, nenhum `DIRECT`, `MPG`, `STROW` ou `STMASK`.
3. Logo depois do pacote, 16 bytes que começam com a palavra 0x70000000 (etiqueta DMA `end`, qwc 0). É o
   sentinela que `MdlTex_RebaseChain` procura (src/sys/gfxm_e_c.c:31); em todos os fluxos da demo a
   primeira ocorrência de 0x70000000 é exatamente essa, e 0x6C058000 não aparece dentro de dados.

Conteúdo de um lote (layout lido pelo programa 0/1 da VU1, src/vu1/prog0.vsm e
docs/systems/vu1/prog0_prog1.md):

| Quadword | Conteúdo | Números |
|---|---|---|
| +0 | etiqueta GIF "um registro A+D" | 2 x u64 |
| +1 | A+D: TEX0_1 da textura (palavras 5 e 6 do lote contando do código UNPACK) | u64 dado + u64 endereço de registro |
| +2 | A+D: TEX0_2 da rampa de sombreamento (palavras 9 e 10) | u64 + u64 |
| +3 | etiqueta GIF da faixa da camada 0; NLOOP (bits 0..14) = número de vértices n | 2 x u64 |
| +4 | etiqueta GIF da faixa da camada 1 (mesmo NLOOP) | 2 x u64 |
| segundo UNPACK, 3 por vértice | posição x, y, z (f32) e w = peso do osso A (f32) | 4 x f32 |
| | normal x, y, z (f32) e 0 | 4 x 32 bits |
| | s, t, q (f32) e 0 | 4 x 32 bits |

Conferido na demo: num do segundo UNPACK = 3 x NLOOP em todos os 1.379 lotes; NLOOP igual nas duas
etiquetas; w da posição é float ou zero, w da normal e a quarta palavra de st sempre zero.

Regra de conversão: **toda palavra do pacote é invertida como u32**, inclusive as etiquetas GIF e os
valores A+D. É assim que o código C os lê: `MdlTex_RebaseChain` percorre o fluxo palavra a palavra
(u32) e reescreve TEX0 pelas metades (palavras 5, 6, 9, 10); o rastreio confirma (171.872 + 4.074
leituras de 4 bytes no fluxo do 01473, nenhuma de outra largura). Consequência: depois da conversão,
cada valor de 64 bits fica como duas palavras na ordem do PS2 (metade baixa no endereço menor); quem o
ler no 360 (o renderizador da F3) tem de montá-lo como `lo | hi << 32`, nunca como um u64 nativo. Se um
modelo de outro personagem trouxer UNPACK de outro formato, a regra muda por formato: V4-32/V3-32/
V2-32/S-32 são palavras de 32 bits, V4-16/V3-16/V2-16/S-16 são u16, V4-8/V3-8/V2-8/S-8 são bytes, V4-5
é u16; os códigos VIF, as palavras de STMASK/STROW/STCOL e as etiquetas DMA são u32; o dado de
`DIRECT` é pacote GIF (etiquetas e A+D como u64, imagem como bytes). Isso não aparece na demo.

O jogo grava no pacote: `MdlTex_RebaseChain` (chamado por `BtlObjMdl_BindFiles` na primeira ligação)
muda TBP0/CBP das palavras 5, 6, 9 e 10 de cada lote. A conversão tem de vir antes.

#### 4.2.2 Seção de costura (entrada 3 + seamOfs)

Não existe na demo (seamOfs = 0 nos dois modelos), mas existe em 73 file[0] do disco (por exemplo 01454,
01458, 01464; 23 personagens). Leitores: `ObjSeam_Transform` e
`ObjSeam_Draw` (src/sys/gfxm_d_c.c:73 e :95), structs em include/sys/gfxm_d_c.h:20..48. A rotina VU0
`ObjSeam_TransformVtx` (em assembly, antes de gfxm_d_c.c) lê posição, normal e uv de cada vértice e grava
xyz, stBase e stShade.

Arranjo, conferido nos 73 (sem trazer bytes): seção em `seamOfs` = fim do último registro de malha;
vértices logo depois do cabeçalho da seção (`vtxOfs` = `seamOfs` + 0x10); triângulos logo depois dos
vértices (`triOfs` = `vtxOfs` + vtxCount x 0x70); depois do último triângulo, zeros até o fim da entrada.
vtxCount vai de 12 a 120 e triCount = vtxCount / 3 em todos (3.186 vértices, 1.062 triângulos); todo
índice de triângulo é menor que vtxCount.

`ObjSeamSec` (16 bytes): s32 vtxCount, s32 triCount, s32 vtxOfs, s32 triOfs. Os deslocamentos são em
bytes **a partir do início da entrada (cabeçalho do modelo)**, somados a cada uso, sem virar ponteiro.

`ObjSeamVtx` (0x70 bytes, vtxCount registros):

| Desl. | Tipo | Largura | Significado | Nos 3.186 vértices do disco |
|---|---|---|---|---|
| 0x00 | f32[4] | 4 | posição; w = peso do primeiro nó | float ou zero |
| 0x10 | f32[3] | 4 | normal | float ou zero |
| 0x1C | s32 | 4 | node (argumento de `BtlObj_FindBound` / `BtlObj_GetNode`) | inteiro pequeno (até 87) |
| 0x20 | f32[4] | 4 | uv da textura base | x, y, z float; w zero |
| 0x30 | s32[4] | 4 | xyz: gravado pela rotina VU0 (12.4; bit 15 da palavra 3 = não desenhar) | zero |
| 0x40 | ? | | 16 bytes não lidos: não determinado | zero |
| 0x50 | u128 | | stBase: gravado (uv / w') | zero |
| 0x60 | u128 | | stShade: gravado | zero |

`ObjSeamTri` (0x20 bytes, triCount registros): s32 idx[3], s32 pad (zero nos 1.062), u64 tex0Base, u64
tex0Shade. Aqui o TEX0 é lido como **u64 inteiro** (`tri->tex0Base`, gfxm_d_c.c:127; só o primeiro
triângulo tem o TEX0 usado, os outros têm o mesmo tipo), ao contrário do fluxo VIF: inverter como u64. As
áreas gravadas (xyz, stBase, stShade) são escritas antes da leitura e chegam zeradas no arquivo; como os
quadwords são copiados para o pacote GIF, convém tratá-los como 4 x 32 bits. O trecho 0x40..0x4F, sem
leitor e zero, pode ir como 4 x u32 ou como bytes (indiferente).

Critério do conversor: `seamOfs` diferente de zero; conferir `seamOfs` = fim do último registro,
`vtxOfs` e `triOfs` dentro da entrada; converter vértices e triângulos como acima e exigir zero no resto
(4.2, tabela do fim do último registro).

### 4.3 Corpo (entrada 2)

`BObjMdl.unk3C` = `BobjMdl.body` (objeto + 0x54), struct `BobjBodyData` (include/battle/bobj_a.h:225)
com registros `BobjBodyPart` (bobj_a.h:213) de 0x1D0 bytes. Leitores: `BtlObjBody_Init` (bobj_a.c:1141),
`BtlObjBody_UpdateParts` (bobj_a.c:1108), `BtlObjObb_SetMtx`, `ColBounds_AddSphere`,
`ColSphere_*`, `EftDet_*VsFighter`, `BtlCharApi_GetRadius`.

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | s32 | 4 | unk00: não lido |
| 0x04 | f32 | 4 | unk04: copiado para `BobjBody.unkA0` |
| 0x08 | ? | | 8 bytes não lidos: não determinado |
| 0x10 | `BobjBodyPart`[] | | partes até a que tem flags bit 0 |

`BobjBodyPart` (0x1D0 bytes):

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x000 | u32 | 4 | flags: bit 0 = última parte |
| 0x004 | s32 | 4 | node |
| 0x008 | ponteiro | 4 | bone: gravado por `BtlObjBody_Init` |
| 0x00C | ponteiro | 4 | part: gravado por `BtlObjBody_Init` |
| 0x010 | f32[4] | 4 | offset: centro no espaço do modelo (x, y, z lidos) |
| 0x020 | f32[4] | 4 | center: gravado a cada quadro |
| 0x030 | f32 | 4 | raio (lido como `CpSphere.radius` sobre center) |
| 0x034 | ? | | 12 bytes não lidos: não determinado |
| 0x040 | `BobjObb` (0x190) | 4 | caixa orientada, toda em f32: center, unk10, axis[3], mtx (gravados por `BtlObjObb_SetMtx`), local[8] em +0x0D0 (cantos no espaço da caixa, **lidos do arquivo**), world[8] (gravados) |

Contagem: sem contador; o laço para na parte com flags bit 0 (22 partes nos dois arquivos da demo). A
entrada termina com enchimento até 0x40. Ponteiros 0x008/0x00C são gravados na carga (32 bits no PS2 e
no 360).

### 4.4 Arquivos de textura (entradas 1, 12, 13, 14..17)

Formato `TexFile`/`TexEntry` (include/sys/gfxm_b_b.h:29 e :47). `Res_RelocateOffsets`
(src/sys/gfxm_b_b.c:244) transforma os deslocamentos em ponteiros **no próprio arquivo**; base = início
da entrada.

| Entrada | Uso | Relocação | Na demo |
|---|---|---|---|
| 1 | `BObjMdl.unk38` (objeto + 0x50): 2 texturas; é a folha do rosto que a HUD desenha (`BtlObj.faceRes`, `HudGauge_DrawFace`) com a vista `HudCTex`: +0x28 é o s32 `mark` gravado a cada quadro e +0x2C o s32 `unk2C`, convertidos como nas folhas da HUD (`comuns.md` 6.3; achado na F4, etapa 5b) | `BtlObj_BindTables` (bobj_a.c:2352) | presente |
| 12 | texturas do modelo: `ObjMdl.texFile` (objeto + 0x1C) e `tex` (objeto + 0x48) | `BtlObjMdl_BindFiles` (gfxm_e_b.c:188 e :192) | 23 texturas (01473), 17 (01793) |
| 13 | olhos: `BObjMdl.eyes` (objeto + 0x4C), uma textura por quadro de piscar | `BtlObjMdl_CopyTexAlpha` (bobj_a.c:1565) | 2 (01473), 1 (01793) |
| 14..17 | formas 5..8 da boca: `mouthB[4..7]` (objeto + 0x8C..0x98) | `BtlObjMdl_CopyTexAlpha` via `BObjFace_InitMouth` (bobj_a.c:1927) | vazias |

`TexFile` (cabeçalho):

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | u32 | 4 | count |
| 0x04 | u32 | 4 | entOfs: tabela de registros, **em palavras de 4 bytes** a partir do início da entrada (0x20 bytes na demo) |
| 0x08 | ? | | 8 bytes não lidos (não zero nos dados): não determinado; provavelmente 2 x u32 |
| 0x10 | ponteiro | 4 | ent: gravado por `Res_RelocateOffsets` |

`TexEntry` (0x40 bytes, count registros):

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | u32 | 4 | pixOfs: pacote de pixels, em palavras a partir do início da entrada; 0 = não há |
| 0x04 | u32 | 4 | clutOfs: pacote da paleta, idem |
| 0x08 | s32 | 4 | pixSize: bytes do pacote |
| 0x0C | s32 | 4 | clutSize |
| 0x10 | s32 | 4 | tbpStep |
| 0x14 | s32 | 4 | cbpStep |
| 0x18 | u32 | 4 | pixBlt: bits 48..63 de BITBLTBUF (DBW, DPSM) |
| 0x1C | u32 | 4 | clutBlt |
| 0x20 | s32 | 4 | tbpOfs |
| 0x24 | s32 | 4 | cbpOfs |
| 0x28 | u8[8] | 1 | [4] = posição de material, [5] = alfa (`BtlObjMdl_InitAlphaSlots`); os outros zero na demo |
| 0x30 | u64 | 8 | tex0: TEX0 sem os ponteiros de bloco; nenhuma leitura no código de personagem (o TEX0 usado vem do fluxo VIF) |
| 0x38 | ponteiro | 4 | pix: gravado por `Res_RelocateOffsets` |
| 0x3C | ponteiro | 4 | clut: gravado |

Pacote de pixels e de paleta (opaco, mandado à GS por DMA com `Dma_AddRef`, src/sys/gfxm_b_b.c:264 e
gfxm_d_c.c:291). Delimitação: `[início da entrada + pixOfs x 4, + pixSize)` e o mesmo com clutOfs e
clutSize. Estrutura verificada nos 47 pacotes de pixels e 33 de paleta da demo:

| Desl. | Conteúdo | Números |
|---|---|---|
| 0x00 | três `NOP` e um `DIRECT` cujo imediato = tamanho/16 menos 1 | 4 x u32 (códigos VIF) |
| 0x10 | etiqueta GIF PACKED, NLOOP 3, EOP, um registro A+D | 2 x u64 |
| 0x20 | A+D TRXPOS (0x51) | u64 + u64 |
| 0x30 | A+D TRXREG (0x52) | u64 + u64 |
| 0x40 | A+D TRXDIR (0x53) | u64 + u64 |
| 0x50 | etiqueta GIF IMAGE, EOP, NLOOP = quadwords de imagem | 2 x u64 |
| 0x60 | imagem: NLOOP x 16 bytes | ver abaixo |
| fim menos 0x20 | etiqueta GIF A+D (NLOOP 1, EOP) e A+D TEXFLUSH (0x3F) | 4 x u64 |

A imagem é sempre enviada como PSMCT32 (DPSM = 0 em todos os pixBlt/clutBlt da demo), mas o formato real
está no TEX0: PSMCT32 (cores RGBA de 8 bits, bytes), PSMT8 (índices de 8 bits, bytes) ou PSMT4 (índices de
4 bits); na demo, 14 texturas CT32, 4 T8 e 29 T4. A paleta são cores RGBA de 4 bytes (bytes; o jogo grava
o byte de alfa em +0x60 + j x 4 + 3, `BtlObjMdl_InitAlphaSlots` e `BtlObjMdl_CopyTexAlpha`). Regra: a
imagem e a paleta ficam intactas (bytes). Nenhum código C lê o cabeçalho de 0x60 bytes nem o fim do
pacote; eles só seguem para a GS. Como números são: códigos VIF u32, etiquetas GIF e A+D u64. A F3
substitui esse caminho; se o conversor os inverter, deve ser como u64 (e então a F3 os lê como u64
nativos), ou deixá-los intactos para a F3 decodificar como dados do PS2. Escolha não determinada pelo
código; o rastreio não vê leituras aí (a DMA não passa pela instrumentação).

### 4.5 Cadeias da boca (entradas 4..11)

`BObjMdl.mouthA[0..7]` = `ObjMdl.extra[0..7]` = `ObjDrawMdl.chains[0..7]` (objeto + 0x5C..0x78). Uma cadeia
por forma de boca 1..8: quando `BtlObj_GetSubState` dá uma forma, `ObjDraw_DrawParts` desenha a cadeia
dela no lugar da do nó 0x30 (src/sys/gfxm_d_c.c:543). Formato: o mesmo pacote DMA/VIF do registro de
malha + 0x60, começando no início da entrada, seguido da etiqueta `end` de 16 bytes e de enchimento até
0x40. `MdlTex_RebaseChain(1, ...)` reescreve o TEX0 dos lotes na primeira ligação. Na demo: 4..8
presentes em 01473 (4..7 em 01793), lidas por `MdlTex_RebaseChain`; nenhuma forma de boca chegou a ser
desenhada como substituta no rastreio (só há leituras do rebase).

## 5. file[0]: parâmetros de jogo, IA, câmera e texto

Todas as entradas desta seção são lidas no lugar, pelo ponteiro que `BtlObj_BindTables`
(`src/battle/bobj_a.c:2295`) guarda no objeto. O deslocamento no objeto é `obj + 0x18 + campo de BObjMdl`
(`include/battle/bobj_b.h`). Nenhuma delas tem deslocamento interno transformado em ponteiro na carga, exceto
as animações de câmera (0x29..0x2B), que `DemoCam_FixupAnim` corrige na primeira vez que são tocadas.
Tamanhos abaixo são os da tabela de deslocamentos (iguais em 01473 e 01793, salvo onde dito); toda entrada é
múltipla de 0x40 e o que sobra depois do último registro é preenchimento com zero (conferido no WSL).

Convenção das larguras: "1" é byte (s8/u8, não inverte), "2" é u16/s16, "4" é u32/s32/f32. Um array `T x[n]`
é `n` números de largura `sizeof(T)` seguidos.

### 5.1 Visão geral

| Entrada | Objeto | Conteúdo | Struct da decomp | Contagem | Tamanho | Demo lê |
|---|---|---|---|---|---|---|
| 0x12 (18) | +0x91C | parâmetros gerais do personagem | `BtlParam` (`include/battle/btl_tech_a.h:107`) + `BtlTechBParam` (`include/battle/btl_tech_b.h:52`) | 1 registro de 0xCC | 0x100 | sim |
| 0x13 (19) | +0x920 | golpes corpo a corpo | `BtlAtkRecord` (`include/battle/btl_tech_a.h:59`) | 177 de 0x30 (pelo tamanho) | 0x2140 | sim |
| 0x14 (20) | +0x924 | rajadas de ki | `BtlKiBlastData` (`include/battle/btl_tech_b.h:71`) | 13 de 0x34 (fixo) | 0x2C0 | sim |
| 0x15 (21) | +0x928 | movimento | `BtlMoveData` (`include/battle/btl_tech_b.h:98`) | 1 de 0xA0 | 0xC0 | sim |
| 0x16 (22) | +0x93C | falas de entrada e vitória por adversário | `BtlSeqObj.talkTbl` (`src/battle/btl_seq.c:550`), só bytes | 176 de 4 (pelo tamanho) | 0x2C0 | não |
| 0x17 (23) | +0x940 | cadeias secundárias (cabelo, cauda, capa) e limites da cabeça | `BObjChainParam` (`include/battle/bobj_b.h:176`) + `BtlCtlParam` (`include/battle/btl_char_ctl.h:123`) | 1 de 0x355 | 0x380 | sim |
| 0x18 (24) | +0x92C | técnicas dos slots 2..4 | `BtlSuperData` (`include/battle/btl_tech_b.h:110`) + `EftSuperSrc` (`include/battle/eft_h.h:181`) | 1 de 0x244 | 0x280 | sim |
| 0x19 (25) | +0x930 | habilidades dos slots 0..1 | `BtlSkillData` (`include/battle/btl_tech_b.h:166`) + `EftSkillSrc` (`include/battle/eft_h.h:155`) | 1 de 0xC8 | 0x100 | sim |
| 0x1A (26) | nenhum | não ligada por `BindTables` | nenhuma | vazia nos dois arquivos | 0 | não |
| 0x1B (27) | +0x934 | parâmetros da IA (CPU) | sem struct; acessos `(s8 *)param + k` em `src/battle/btl_ai_act.c` e `btl_ai_cond.c` | 2 metades de 0xF8 | 0x200 | sim |
| 0x1C (28) | +0x938 | cortes de câmera do personagem | `ChrCamCutDef` (`include/battle/btl_char_cam.h:129`) | 10 de 0x24 (pelo tamanho) | 0x180 | sim |
| 0x29 (41) | +0xB0 | animação de câmera 0 (entrada) | `DemoCamAnim` (`include/battle/btl_demo_cam.h:52`) | cabeçalho | 0x3C0 | não |
| 0x2A (42) | +0xB4 | animação de câmera 1 (vitória) | `DemoCamAnim` | cabeçalho | 0x300 | não |
| 0x2B (43) | +0xB8 | animação de câmera 2 (vitória) | `DemoCamAnim` | cabeçalho | 0x300 | não |
| 0x2C (44) | nenhum | provável roteiro da lista de golpes num nono idioma | nenhuma (sem leitor) | terminador | 0x1C80 | não |
| 0x2D..0x34 (45..52) | +0xBC (uma delas) | roteiro da lista de golpes do menu de pausa, uma por idioma | `u16 *` (`src/battle/hud_0.c:50`, lido em `src/battle/btl_seq.c:202` e `:429`) | terminador | 0x1B40..0x2500 | não |

Notas de ligação:
- `obj + 0xB4` (0x2A) é sobrescrito por `BtlObj_BindCommonTables` (`src/battle/bobj_a.c:2445`) com a entrada
  12 do arquivo comum 3, mas essa função só é chamada na troca de membro com `BtlChar_TestMemberUnk70`
  (`src/battle/btl_char_mgr.c:427` e `:492`). Na luta normal 0x2A fica em uso.
- 0x2D..0x34: `BindTables` liga só `0x2D + gProgress->unk00`, e `gProgress->unk00` é o idioma do texto
  (`src/battle/btl_script_cmd.c:463`). As outras sete ficam no arquivo sem uso nesta carga, mas têm o mesmo
  formato. Em 01473 a entrada 46 (0x2E) é vazia; em 01793 não.
- As entradas 0x25..0x28 (37..40, `obj + 0x908..0x914`, `BObjMdl.unk8F0`) são também `DemoCamAnim`
  (`DemoCam_PlayCharAnim0..2`, `src/battle/btl_demo_cam.c:465` em diante), mas estão vazias nos dois arquivos da
  demo; a seção delas é a mesma de 0x29..0x2B.

### 5.2 Entrada 0x12: parâmetros gerais (`BtlParam`), 0xCC bytes

Cinco visões da decomp leem o mesmo bloco: `BtlParam` (`btl_tech_a.h:107`), `BtlTechBParam` (`btl_tech_b.h:52`),
`BtlCapiParam` (`include/battle/btl_capi_a.h:75`), `BtlCapiBParam` (`include/battle/btl_capi_b.h:117`) e
`BobjParam` (`include/battle/bobj_a.h:288`). O layout abaixo é a união delas; onde divergem, vale quem lê.

| Desl. | Tipo | Larg. | Significado (leitor) |
|---|---|---|---|
| 0x00 | u16 | 2 | bits de caráter: 0x4, 0x8, 0x80 testados (`BtlParam_GetUnk0`, `BtlCharApi_ObjGetParamFlags0`) |
| 0x02 | s8 | 1 | 4 isenta de arremessos e investidas (`BtlParam_GetUnk2`) |
| 0x03 | s8 | 1 | tipo de aura (`BtlParam_GetAuraKind`); também a linha do preset de cor (`BtlObj_InitFlags`, `bobj_a.c:2389`) e somado ao alfa (`ObjDraw_GetDimAlpha`) |
| 0x04 | f32 | 4 | altura, escala do movimento vertical na reprojeção de animação (`BobjParam.height`, `BtlObjAnim_Retarget`; `btl_demo_cam.c:254`) |
| 0x08 | f32 | 4 | raio da esfera do corpo; <= 0 usa 2 x tamanho do modelo (`BobjParam.radius`, `BtlObjBody_Init`) |
| 0x0C | f32 | 4 | raio usado pela IA; <= 0 usa obj+0xFF0 x 1,8 (`BtlCapiParam.radius`, `BtlCharApi_GetRadius`) |
| 0x10 | u32 | 4 | flags (`BtlParam_GetFlags`) |
| 0x14 | u32 | 4 | flags2: bits de habilidade (`BtlParam_GetFlags2`) |
| 0x18 | u32 | 4 | flags3 (`BtlParam_GetFlags3`) |
| 0x1C..0x24 | s32 x3 | 4 | `unk1C`, `unk20`, `unk24` (lidos por acessores sem chamador conhecido) |
| 0x28 | s32 | 4 | `kiRegenLimit` |
| 0x2C | s32 | 4 | `kiRecoverGoal` |
| 0x30..0x3C | s32 x4 | 4 | `kiCharge`, `kiChargeWater`, `kiRegen`, `kiRecover` (por segundo) |
| 0x40 | s32 | 4 | `unk40` |
| 0x44 | s32 | 4 | `blastGain` |
| 0x48 | s32 | 4 | `recoverKiCost` |
| 0x4C | s32 | 4 | `kiDrain67` |
| 0x50 | f32 | 4 | `maxPowerChargeTime` (s) |
| 0x54 | f32 | 4 | `maxPowerTime` (s) |
| 0x58 | s8[4] | 1 | `searchType` por traje |
| 0x5C..0x6C | f32 x5 | 4 | `unk5C`, `hitReactScale`, `gauge99CTime`, `unk68`, `unk6C` |
| 0x70 | s16 | 2 | `unk70` |
| 0x72 | s16 | 2 | `unk72` |
| 0x74 | f32 | 4 | `chargeRateA` |
| 0x78 | f32 | 4 | `chargeRateB` |
| 0x7C | f32 | 4 | `damageTaken` |
| 0x80 | s16 | 2 | `blastLimitA` |
| 0x82 | s16 | 2 | `blastLimitB` |
| 0x84..0xC7 | bytes | 1 | `unk84[4]`, `unk88[2]`, `unk8A[5]`, `unk8F[9]` (indexado), transformações `0x98..0xAC` (`transTarget`, `transCost`, `transSeq`, `transKind`, `transA8`, `transDefault`), `unkAD`, fusões `0xAE..0xC5`, `unkC6`, `unkC7` (percentual) |
| 0xC8 | f32 | 4 | `unkC8` (`BtlParam_GetUnkC8`, `btl_capi_a.c:177`) |

Divergência das visões: `BobjParam.unk00` diz f32 em 0x00, mas ninguém o lê; os leitores de 0x00..0x03 leem
u16, s8, s8. `BtlParam` declara 0x04..0x0F como bytes (`unk4`), mas os únicos leitores (`BobjParam`,
`BtlCapiParam`) leem f32. Não há contador: o bloco é um registro só.

### 5.3 Entrada 0x13: golpes (`BtlAtkRecord`), registros de 0x30

Indexado pelo id do golpe (`BtlAtk_GetId`), sem contador no arquivo. O código limita o id a 0..0xA2
(`BTL_ATK_COUNT`, `btl_tech_a.h:21`), mas a entrada tem 0x2140 bytes = 177 registros de 0x30 + 0x10 de
preenchimento, e o último byte não nulo cai no registro 176 (nos dois arquivos). O conversor deve tratar a
entrada inteira como `tamanho / 0x30` registros.

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | u32 | 4 | `flags` |
| 0x04 | u16 | 2 | `damage` |
| 0x06 | u16 | 2 | `guardDamage` |
| 0x08 | u16 | 2 | `guardKi` |
| 0x0A | u16 | 2 | `kiGain` |
| 0x0C | u16 | 2 | `throwC` |
| 0x0E | u16 | 2 | `throwE` |
| 0x10 | u16 | 2 | `unk10` |
| 0x12..0x2C | u8/s8 | 1 | empurrões, lançamento, tremor, reações, efeito, som, voz, prioridade, defesa, `armorIgnore` |
| 0x2D..0x2F | u8[3] | 1? | `unk2D`, sem leitor: não determinado (tratado como bytes) |

### 5.4 Entrada 0x14: rajadas de ki (`BtlKiBlastData`), 13 registros de 0x34

13 é fixo (`BTL_KIBLAST_COUNT`, `btl_tech_b.h:42`); 13 x 0x34 = 0x2A4, entrada 0x2C0.
`BtlCapiKiBlast` (`btl_capi_a.h:87`) é outra visão do mesmo registro (só `type`).

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | u16 | 2 | `flags` |
| 0x02 | s8 | 1 | `type` |
| 0x03 | s8 | 1 | `unk3` |
| 0x04 | s32 | 4 | `damage` |
| 0x08 | s32 | 4 | `guardDamage` |
| 0x0C | s32 | 4 | `ki` |
| 0x10..0x20 | f32 x5 | 4 | `speed`, `turn`, `time`, `push`, `guardPush` |
| 0x24..0x27 | s8 x4 | 1 | `react`, `reactS800`, `reactS4000`, `reactS200` |
| 0x28 | f32 | 4 | `unk28` |
| 0x2C | f32 | 4 | `unk2C` |
| 0x30 | s8 | 1 | `hits` |
| 0x31 | s8 | 1 | `unk31` |
| 0x32 | u8[2] | 1? | sem leitor: não determinado (tratado como bytes) |

### 5.5 Entrada 0x15: movimento (`BtlMoveData`), 0xA0 bytes

Só números de 4 bytes: `speed` f32[20] (0x00), `accel` f32[2] (0x50), `angle58` f32 (0x58), `height` f32[2]
(0x5C), `kiCost` s32[4] (0x64), `turnRate` f32[3] (0x74), `turnAccel` f32[4] (0x80), `turnMax` f32[4] (0x90).
Os acessores (`BtlMoveParam_*`) leem por índice; o rastreio não tem nome de campo (`?`), mas todas as leituras
são de 4 bytes, alinhadas.

### 5.6 Entrada 0x16: tabela de falas (`talkTbl`), 4 bytes por adversário

Indexada pelo id do personagem adversário (`chara * 4`); só bytes: +0 fala de entrada (0xFF = nenhuma), +1 fala
em segundo, +2 fala de vitória (0xFF = nenhuma), +3 sem leitor. Leitores: `BtlSeqIntroTalk_Setup` e
`BtlSeqWinTalk_Setup` (`src/battle/btl_seq.c:812`, `:970`). 0x2C0 / 4 = 176 posições; o último byte não nulo
está na posição 161. Nada a inverter.

### 5.7 Entrada 0x17: cadeias secundárias (`BObjChainParam`) e limites da cabeça (`BtlCtlParam`), 0x355 bytes

`BObjChainParam` (`bobj_b.h:176`) ocupa 0x000..0x34F; `BtlCtlParam` (`btl_char_ctl.h:123`) é o mesmo ponteiro
visto até 0x354. Estrutura de arrays, 16 posições do tipo A e 8 do tipo B:

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x000 | u8[16] | 1 | `nodeA` |
| 0x010 | f32[16] x3 | 4 | `axisX`, `axisY`, `axisZ` |
| 0x0D0 | u8[16] | 1 | `hinge` |
| 0x0E0 | s16[16] | 2 | `hingeOfs` |
| 0x100 | u8[16] | 1 | `hingeLimit` |
| 0x110 | f32[16] x2 | 4 | `inertia`, `damping` |
| 0x190 | s8[16] | 1 | `speed` |
| 0x1A0 | s16[16] | 2 | `phase` |
| 0x1C0 | u8[16] | 1 | `swingMax` |
| 0x1D0 | f32[16] x2 | 4 | `swing`, `follow` |
| 0x250 | u8[16] | 1 | `limit` |
| 0x260 | f32[16] x2 | 4 | `rest`, `gravity` |
| 0x2E0 | u8[16] | 1 | `stiff` |
| 0x2F0 | u8[8] | 1 | `nodeB` |
| 0x2F8 | s8[8] | 1 | `yaw` |
| 0x300 | f32[8] x2 | 4 | `gravityB`, `swingB` |
| 0x340 | s8[8] x2 | 1 | `yawMax`, `yawMin` |
| 0x350..0x354 | u8 x3, s8 x2 | 1 | `headYawMax`, `headUpMax`, `headDownMax`, `neckPitch`, `headPitch` (`BtlChar_UpdateLook`) |

O último byte não nulo é 0x354. Algumas leituras de f32 vêm em vetores de 16 bytes (`Vec4_Add`, `wide4`): os
elementos continuam f32.

### 5.8 Entrada 0x18: técnicas dos espaços 2..4 (`BtlSuperData`), 0x244 bytes

Estrutura de arrays, cada campo com 3 elementos (slot - 2). `BtlSuperData` (`btl_tech_b.h:110`) deixa trechos
como `unk`; `EftSuperSrc` (`eft_h.h:181`, a visão de `EftShot_BuildParam`) tipa esses trechos. União:

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x000 | u32[3] | 4 | `flagsA` |
| 0x00C | u32[3] | 4 | `flags` |
| 0x018 | s16[3] | 2 | `id` |
| 0x01E | s16[3] | 2 | `unk1E` |
| 0x024..0x08F | f32[3] x9 | 4 | `shotTime`, `unk30`, `unk3C`, `unk48`, `shotSpeed`, `shotTurn`, `shotUnk6C`, `unk78`, `unk84` |
| 0x090..0x0D3 | bytes | 1 | `unk90`, `shots`, `hitsB`, `unk99`, `hitsC`, `unk9F`, `unkA2`, `unkA5`, `unkA8[6][3]`, `unkBA[8][3]`, `unkD2[2]` |
| 0x0D4 | f32[8][3] | 4 | `unkD4` |
| 0x134..0x16B | bytes | 1 | `unk134`, `unk137`, tipo de efeito (0x138), `hitDirKind` (0x13B), `type` (0x13E), reações, `guardKind`, `hitSound[2][3]`, `steps`, 0x15C..0x164, `unk165`, `lastStep`, `unk16B` |
| 0x16C..0x207 | f32/s32 [3] | 4 | `push`, `guardPush` (f32); `damage`, `guardDamage`, `ki`, `recoil`, `drain` (s32); `cooldown`, `chargeLimit`, `chargeScale`, `throwAngleA`, `throwAngleB`, `rushSpeed` (f32) |
| 0x208 | s16[3] | 2 | `throwChara` |
| 0x20E | s16[3] | 2 | `throwCharaAlt` |
| 0x214 | f32[3] | 4 | `unk214` (só `EftSuperSrc`) |
| 0x220..0x22B | bytes | 1 | `unk220`, `unk223`, `unk226`, `promptRow` |
| 0x22C | f32[3] | 4 | `time22C` |
| 0x238 | f32[3] | 4 | `time238` |

Último byte não nulo: 0x243. O rastreio também atribui leituras desta entrada a `EftKDef`, `EftOwnerParam` e
`EftShotParam`: são leituras da cópia montada por `EftShot_BuildParam` (`src/battle/eft_h.c:132`), não do
arquivo; a origem é que aponta para cá. Os comentários de `EftShotParam` citam fontes (0x141, 0x162) que não
batem com o código de `EftShot_BuildParam` (0x13B, 0x138): vale o código.

### 5.9 Entrada 0x19: habilidades dos espaços 0..1 (`BtlSkillData`), 0xC8 bytes

Estrutura de arrays de 2 elementos. União de `BtlSkillData` (`btl_tech_b.h:166`) e `EftSkillSrc`
(`eft_h.h:155`):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | u32[2] | 4 | `flagsA` |
| 0x08 | u32[2] | 4 | `flags` |
| 0x10 | s16[2] | 2 | `id` |
| 0x14 | s16[2] | 2 | `unk14` |
| 0x18..0x37 | f32[2] x4 | 4 | `shotTime`, `shotSpeed`, `shotTurn`, `shotUnk30` |
| 0x38..0x73 | bytes | 1 | contagens de tiro, `unk48[6][2]`, `unk54[6][2]`, tipo de efeito (0x60), `hitDirKind`, `sequence`, `statKind`, `react`, `reactS800`, `hitSound`, `unk6E`, `unk70`, 0x72..0x73 (sem leitor) |
| 0x74 | f32[2] | 4 | `push` |
| 0x7C | f32[2] | 4 | `guardPush` |
| 0x84 | s32[2] | 4 | `damage` |
| 0x8C | s32[2] | 4 | `guardDamage` |
| 0x94..0x9F | s8[2] x6 | 1 | `stocks`, `statLevel0/2/1/3`, `unk9E` |
| 0xA0 | f32[2] | 4 | `time` |
| 0xA8..0xC7 | s32[2] x4 | 4 | `healthPct`, `ki`, `valF`, `valE` |

`EftShot_BuildParam` lê `unk54[k][n]` como byte e estende o sinal para s16 na cópia: no arquivo é byte.

### 5.10 Entrada 0x1B: parâmetros da IA, 0x1F0 bytes úteis, só bytes

Sem struct na decomp. Dois blocos de 0xF8 (mínimo e máximo), interpolados pelo nível da CPU com
`BtlAi_ScaleByLevel(nivel, lo[k], hi[k])` (pares 0x08/0x100, 0x0F/0x107, 0x14/0x10C, 0x44/0x13C, 0x48/0x140,
0x50/0x148, 0x80/0x178, 0x88/0x180, 0xB8/0x1B0 em `btl_ai_act.c` e `btl_ai_cond.c`); byte 0 tem bits 0 e 1
(`btl_ai_act.c:344`, `:784`); `param + tiros * 2` (`btl_ai_act.c:792`). O ponteiro é o do arquivo (`ai->own`
zerado em `AiThink_ResetSide`, `btl_ai_cond.c:2301`), não uma cópia. Todo acesso encontrado é `s8`/`u8`:
nada a inverter. Em 01793 há bytes não nulos em 0x1F0..0x1F1, depois das duas metades, sem leitor
conhecido: não determinado (tratado como bytes).

### 5.11 Entrada 0x1C: cortes de câmera (`ChrCamCutDef`), registros de 0x24

Indexado por `chr->cut.unk4` (`src/battle/btl_char_cam_modes.c:730`), sem contador; 0x180 / 0x24 = 10
registros + 0x18 de preenchimento; o último byte não nulo está no registro 9.

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00..0x03 | u8 x4 | 1 | `nodeA`, `nodeA2`, `nodeC`, `nodeC2` |
| 0x04..0x1C | f32 x7 | 4 | `yaw`, `yawDelta`, `pitch`, `pitchDelta`, `dist`, `distDelta`, `seconds` |
| 0x20 | u16 | 2 | `cutFlags` |
| 0x22 | u16 | 2 | `flags` |

### 5.12 Entradas 0x29..0x2B (e 0x25..0x28): animação de câmera (`DemoCamAnim`)

Lida por `DemoCam_PlayObjAnim(obj, 0..2)` (`src/battle/btl_demo_cam.c:452`) para 0x29..0x2B e por
`DemoCam_PlayCharAnim0..2` para 0x25..0x27. `DemoCam_SetAnim` chama `DemoCam_FixupAnim`
(`btl_demo_cam.c:31`), que troca no próprio buffer os deslocamentos (relativos ao início da entrada) por
ponteiros e zera `needsFixup`. O conversor inverte os deslocamentos como u32 antes; a soma do código depois
funciona igual.

Cabeçalho (0x24 bytes):

| Desl. | Tipo | Larg. | Campo |
|---|---|---|---|
| 0x00 | 4 caracteres | 1 | `unk0` na decomp (s32), ninguém lê; nos 6 casos da demo é a mesma assinatura ASCII de 4 letras: tratar como bytes |
| 0x04 | s32 | 4 | `needsFixup` (não zero no arquivo) |
| 0x08 | u32 | 4 | `channels`: deslocamento da lista de canais |
| 0x0C | s32 | 4 | `channelCount` |
| 0x10 | s32 | 4 | `unk10` |
| 0x14 | f32 | 4 | `length` |
| 0x18 | u32 | 4 | `defaults`: deslocamento da pose inicial |
| 0x1C | u32 | 4 | `unk1C`: deslocamento de um bloco opcional; vira NULL se `version < 2` |
| 0x20 | s32 | 4 | `version` |

Canal (`DemoCamChannel`, 0x10, `channelCount` deles): `index` s32, `keys` u32 (deslocamento), `keyCount` s32,
`unkC` s32. Chave (`DemoCamKey`, 0x20, `keyCount` por canal): `flags` u32, `time` f32, `value` f32, e
0x0C..0x1F (`unkC[0x14]`) que o código não lê. Pose inicial (`DemoCamPose`, 0x20): `pos` f32[3], `rot` f32[3],
`frame` s32, `unk1C` f32.

Delimitação conferida no WSL (sem trazer bytes): nas 6 ocorrências o arranjo é cabeçalho em 0x00, zeros em
0x24..0x2F, pose em 0x30, 6 canais em 0x50, chaves contíguas a partir de 0xB0 (4 por canal em 0x29, 3 em 0x2A e
0x2B), e o bloco de `unk1C` nos últimos 0x10 bytes, todo zero, com `version` 0 (o jogo o ignora). Toda a entrada
fica coberta. As palavras 0x0C..0x1B das chaves têm cara de f32 e 0x1C é zero: sem leitor, a largura é "não
determinada pelo código", mas tudo ali é palavra de 4 bytes, então inverter como u32 é seguro.

### 5.13 Entradas 0x2D..0x34 (e 0x2C): roteiro da lista de golpes (texto u16)

`u16 *`: o leitor pula a primeira palavra e percorre palavras u16 até uma palavra 0 ou a marca `'@'`
(`BtlText_CountEntries`, `src/battle/btl_seq.c:420`; desenho em `:202`), com marcas `'$'` (página) e `'*'`
(entrada) e as funções `BtlText_CheckLineMask` / `BtlText_CheckUnlock` / `BtlText_NextLine`, todas sobre
`u16 *`. Delimitação: a entrada inteira é uma sequência de u16 (tamanho par, terminada antes do fim; no WSL o
`'@'` e o 0 final aparecem perto do fim de cada entrada). Inverter cada u16.

A entrada 0x2C não é ligada por ninguém na decomp (busca por índice 0x2C e por `obj + 0xB0..0xBC` sem achado).
No WSL ela tem a mesma estrutura das 0x2D..0x34 (mesmo número de marcas `'$'`, `'*'` e um `'@'`), mas com
metade das palavras acima de 0xFF, como a 0x34: provavelmente o mesmo roteiro num idioma de escrita não
latina. O uso é não determinado; o formato provável é o mesmo (u16).

## 6. Animações: file[1] e as entradas de animação do file[0]

### 6.1 file[1] (01481, 01801)

#### 6.1.1 Tabela e conteúdo das entradas

Mesmo esquema dos outros arquivos do slot: `buf[0]` = número de entradas N (0x1B8 = 440 nos dois
arquivos da demo), `buf[1..N]` = deslocamento em bytes de cada entrada a partir do início do arquivo,
`buf[N+1]` = tamanho do arquivo, zeros até 0x700 (onde começa a entrada 1). Todos u32. As entradas são
alinhadas em 0x40 e as que existem ocupam de `buf[i]` até `buf[i+1]`.

Quem lê: só `BtlObj_BindTables` (src/battle/bobj_a.c:2307), que guarda as entradas 1..0x19E em
`mdl.anims[0..0x19D]` (include/battle/bobj_b.h:254, objeto + 0xC0). No C da decomp ninguém mais lê o
file[1]. No rastreio: `BObjFile_GetEntry` leu `buf[1..415]` (828 leituras = 414 entradas x 2), e o mapa
inverte exatamente essas 415 palavras; `buf[0]` e `buf[416..441]` não são lidos na demo (mas são u32 e
devem ser invertidos pela especificação).

Conteúdo das entradas (verificado nos dois arquivos):

| Entradas | Conteúdo |
|---|---|
| 1..0x19E (1..414), menos as reservas abaixo | animação comprimida em BPE (354 em 01481, 352 em 01801) |
| reservas, comuns aos dois: 154..156, 266..267, 273..274, 280..281, 283..295, 300..301, 307..308, 314..315, 317..318, 323, 328..329, 334..335, 341..342, 348..349, 351..363, 396..397; só em 01481: 304..306, 311..313; só em 01801: 319..322, 324..327 | "BPE vazio" de 0x40 bytes: rawSize = 0, packSize = 6, um bloco de 6 bytes que decodifica em 0 bytes, resto zero (86 em 01481, 88 em 01801, contando 415..440) |
| 0x19F..0x1B8 (415..440) | todas "BPE vazio" de 0x40 bytes; **ninguém lê** (BindTables para em 0x19E) |

Observação de comportamento (não é de ordem de bytes): tocar uma entrada "BPE vazio" faz `Bpe_Decode`
escrever 0 bytes e `BtlObjAnim_Load` devolver o buffer da camada com o que já estava nele. A demo não
decodificou nenhuma delas.

### 6.2 Contêiner BPE (cada entrada comprimida)

Struct: `BpeHeader` (include/sys/bpe.h:7), lida por `Bpe_Decode` (src/sys/bpe.c).

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | s32 | 4 | rawSize: tamanho depois de decodificar (máximo na demo 0x52A0; o destino é a camada de 0xC000 bytes de `BobjWork`) |
| 0x04 | s32 | 4 | packSize: bytes do fluxo comprimido que segue |
| 0x08 | u8[packSize] | 1 | fluxo comprimido (Gage "expand"): sequência de blocos |
| 0x08+packSize | | | zeros até a próxima fronteira de 0x40 (fim da entrada) |

Delimitação: o fluxo é exatamente `[entrada + 8, entrada + 8 + packSize)`; o laço do decodificador
termina quando consumiu packSize bytes. Dentro dele **tudo é byte**: cada bloco é uma tabela de pares
(contadores e símbolos u8), depois o tamanho do bloco em 2 bytes lidos um a um, alto primeiro
(`hi = *in++; blockSize = *in++ + (hi << 8)`, já independente da ordem da máquina), depois os bytes
codificados. Nenhum número de 2, 4 ou 8 bytes lá dentro.

Regra para a conversão: só rawSize e packSize são u32 invertíveis. **Inverter qualquer coisa dentro do
fluxo comprimido corrompe a animação**, e os números da animação só existem depois de `Bpe_Decode`. Logo
a conversão in-place do arquivo não alcança o conteúdo das animações. Caminhos possíveis (decisão do
Lucas, não tomada aqui):
1. converter o buffer decodificado logo depois de `Bpe_Decode` em `BtlObjAnim_Load`
   (src/battle/bobj_a.c:360); é o que a build de teste da F1 já faz, em `__wrap_Bpe_Decode`
   (port/x360/headless/inverter.c:151), aplicando o mapa ao `#bpe`. Cada decodificação gera um buffer
   novo, então a conversão roda a cada `BtlObjAnim_Play`/`PlayFrom` e não há risco de inverter duas vezes.
   O gancho tem de ser no caminho das animações, não em `Bpe_Decode` em geral: `Sprite_Unpack`
   (src/sys/sprite.c:11) usa o mesmo decodificador para pacotes de menu de outro formato;
2. o instalador decodifica, converte e recomprime (o tamanho da entrada muda, o que foge da regra
   "nada muda de tamanho" e exige refazer a tabela);
3. o instalador guarda a animação já decodificada e convertida (também muda tamanhos e a carga deixa de
   chamar `Bpe_Decode`).

### 6.3 Animação decodificada (`BobjAnim`)

Structs: `BobjAnim` (include/battle/bobj_a.h:89), `BobjAnimEvent` (:81), `BobjTrack` (:72),
`BobjKey` (:64), `BobjPackedQuat` (:58), `BobjAnimShape` (:99); descrição em bobj_a.h:13..37.
Leitores: `BtlObjAnim_SamplePose`, `_SampleRot`, `_SamplePosRot`, `_UpdateEvents`, `_QueryEvent`,
`_Play`, `_PlayFrom`, e os que alteram a animação decodificada no lugar (`_Retarget`, `_ZeroRootAxes`,
`_RebaseRoot`, `_ClearNodeRot`), todos em src/battle/bobj_a.c.

Ordem das partes no buffer (conferida nas 706 animações de 01481 e 01801): cabeçalho, eventos,
volumes, trilhas, e no fim até 2 bytes de enchimento (o rawSize é sempre múltiplo de 4).

Cabeçalho (0x94 bytes):

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | u8 | 1 | unk00: o mesmo valor em todas as 706; ninguém lê |
| 0x01 | u8 | 1 | eventCount: número de eventos (máximo 34 na demo) |
| 0x02 | u16 | 2 | length: último quadro |
| 0x04 | u16 | 2 | unk04: zero em todas; ninguém lê |
| 0x06 | u16[71] | 2 | track[id]: por id de nó do modelo, deslocamento da trilha **em unidades de 4 bytes a partir do início da animação**; 0 = nó sem trilha |

Eventos: `eventCount` registros de 0x10 bytes a partir de 0x94:

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | u32 | 4 | attrLo: bits 0..31 do atributo |
| 0x04 | u32 | 4 | attrHi: bits 32..63 |
| 0x08 | u16 | 2 | frame |
| 0x0A | u16 | 2 | shape: deslocamento do volume, **em unidades de 4 bytes a partir do início da animação** (só vale com attr bit 0) |
| 0x0C | u32 | 4 | arg (máscara de nós de ataque, ou argumento do evento) |

O atributo de 64 bits é montado pelo código como `attrLo + (attrHi << 32)` a partir de **duas leituras
u32**: inverter como dois u32, nunca como um u64 (isso trocaria as metades).

Volumes (registro apontado por `shape` nos eventos com attr bit 0): ficam entre o fim dos eventos e a
primeira trilha, com passo de 0x20 bytes (80 volumes nas animações da demo, todos nessa posição):

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | f32[3] | 4 | x, y, z: centro no espaço do nó |
| 0x0C | f32 | 4 | unk0C, não lido |
| 0x10 | f32[3] | 4 | r0, r1, r2: raio da esfera (r0) ou meias-dimensões da caixa |
| 0x1C | 4 bytes | | resto do passo de 0x20; não lido. Largura não determinada pelo código (provavelmente enchimento) |

Trilha (`BobjTrack`), no deslocamento `track[id] * 4`:

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | u16 | 2 | flags: bit 0 = só rotação (na demo: 36.894 trilhas com 1, 1.275 com 0) |
| 0x02 | u16 | 2 | count: número de chaves |
| 0x04 | | | chaves, conforme o bit 0 |

Só rotação: `count` x `BobjPackedQuat` (2 x u32, lo e hi, 8 bytes cada), depois `count` x u16 com o
quadro de cada chave, depois até 2 bytes de enchimento (se count for ímpar) até múltiplo de 4.

Completa: `count` x `BobjKey` de 0x18 bytes:

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | f32 | 4 | x |
| 0x04 | f32 | 4 | y |
| 0x08 | f32 | 4 | z |
| 0x0C | u32 | 4 | frame |
| 0x10 | u32 | 4 | rot.lo |
| 0x14 | u32 | 4 | rot.hi |

O quaternion empacotado também é lido como `lo + (hi << 32)` (macro `QUAT64`, bobj_a.c:104) a partir
de dois u32: converter como dois u32. Nada disso vira ponteiro: os deslocamentos ficam u16 e são somados
a cada uso (`TRACK_AT`, bobj_a.c:103; `(u32 *)data + shape`, bobj_a.c:652).

Conferência contra o rastreio (posições lidas dentro das animações decodificadas `01481.bin@0x...#bpe`
e `01801.bin@0x...#bpe`; 68 animações de 01481 e 83 de 01801 foram decodificadas na demo, todas entre
as entradas 1..0x19E):

| Parte | tipos.tsv 01481 | tipos.tsv 01801 | mapa.tsv 01481 | mapa.tsv 01801 |
|---|---|---|---|---|
| eventCount | 177 | 163 | (byte) | (byte) |
| length | 81 | 84 | 68 | 83 |
| track[] | 3.965 | 4.648 | 3.686 | 4.565 |
| eventos | 605 | 225 | 336 | 123 |
| cabeçalho de trilha completa | 238 | 474 | 192 | 396 |
| chaves completas | 13.040 | 11.456 | 5.743 | 5.515 |
| cabeçalho de trilha de rotação | 10.819 | 12.896 | 7.045 | 8.568 |
| quaternions de rotação | 62.660 | 55.168 | 33.204 | 30.816 |
| quadros u16 de rotação | 39.515 | 33.018 | 17.092 | 15.780 |

Em todas essas posições a largura do rastreio é a da especificação (0 divergências em tipos.tsv e em
mapa.tsv), nenhuma leitura cai fora das partes descritas, e nenhuma cai em volume (nenhuma janela de
golpe ficou aberta num quadro lido) nem em unk00/unk04. Leitores fora de bobj_a.c que aparecem lendo
estes dados (`BtlSuper_MeasureRushStep`, `BtlMove_*`, `Quat_Mul`, `ChrCam_IsCutActive` etc.) leem
cópias de campos (frame, attr, chaves) e com a mesma largura.

No arquivo comprimido em si: `Bpe_Decode` leu rawSize/packSize como u32 (68 + 83 entradas, o mapa
inverte esses 2 u32 em cada uma) e o fluxo byte a byte (212.179 e 186.129 posições, máscara 1).

#### 6.3.1 O conflito 01801.bin 737884 / 737890

As duas posições ficam na entrada 299 do 01801 (0xB41C0..0xB4600), dentro do **fluxo comprimido**.
737890 nem é múltiplo de 4. As "leituras como número" são de `EftQuad_StepAll` (src/battle/eft_y.c:66,
a variável local `step` da pilha) e de `Vec3_Normalize`/`Vec3_Copy` (src/port/vu0_b.c), com o tipo
`EftYVec`. Não há leitura real do arquivo aí: `Bpe_Decode` copia bytes do fluxo para os vetores locais
`left`/`right`/`stack` (256 bytes cada, na pilha), e a sombra do rastreio guarda essa origem; depois
o quadro de pilha de `EftQuad_StepAll` reaproveita o endereço e a leitura herda a origem antiga
(inferência pelo local e pelo desalinhamento, não verificada passo a passo). Conclusão: artefato de
pilha do rastreio; a especificação mantém o fluxo como bytes e o conflito corretamente não inverte.

### 6.4 Entradas 0x1D..0x24 do file[0] (29..36): animações do próprio modelo

`BtlObj_BindTables` (bobj_a.c:2312 e :2330) guarda as mesmas 8 entradas em dois lugares:
`mdl.unk720[8]` = `BobjMdl.modelAnims` (include/battle/bobj_a.h:322, objeto + 0x738) e
`mdl.unk8D0[8]` (include/battle/bobj_b.h:257, objeto + 0x8E8).

Formato: `BobjAnim` **sem compressão**, idêntico ao layout decodificado acima (cabeçalho 0x94, eventos,
volumes, trilhas), com os deslocamentos u16 em unidades de 4 bytes contados do início da entrada. Em
todos os 742 arquivos com tabela de 0xFC entradas no disco, 991 destas entradas existem e todas fecham
com esse layout (sobra menor que 0x40 até o fim da entrada, que é o alinhamento).

Leitores: `BtlObjAnim_PlayModel` (bobj_a.c:824, modelAnims[0..7]; chamado por view_a_d.c, view_b_d.c,
battle_load.c, eft_ad_b.c e btl_char_fx_c.c) e `BtlObjAnim_Load` com id >= 0x19E (bobj_a.c:365: o índice
corre de `anims[]` para `modelAnims[]` e, acima de 0x1A5, para a tabela de lábio). Esses são usados
**no lugar**, sem cópia: a conversão in-place do file[0] tem de convertê-las uma vez na carga. Cuidado:
`BtlObjAnim_Play` com `work->retarget` chama `BtlObjAnim_Retarget`, que altera as chaves no lugar
(comportamento original; só soma floats, não muda a ordem dos bytes).

Quem lê `mdl.unk8D0` (objeto + 0x8E8..0x904): não determinado. Nenhum leitor no C da decomp; pode haver
em funções ainda em assembly.

Na demo: em 01473 e 01793 só a entrada 36 (0x24, modelAnims[7]) existe e ninguém a lê (zero posições
em tipos.tsv e mapa.tsv). Não usada na demo.

### 6.5 Entradas 0x25..0x28 do file[0] (37..40): animações de câmera (formato `DemoCamAnim`)

`mdl.unk8F0[4]` (include/battle/bobj_b.h:258) = objeto + 0x908..0x914. Leitores:
`DemoCam_PlayCharAnim0/1/2` (src/battle/btl_demo_cam.c:464..500, objeto + 0x908, + 0x90C, + 0x910) e
`ShenScene` (src/battle/view_b_d.c:109, `SHEN_OBJ_CAMS`, as quatro). Formato: `DemoCamAnim`
(include/battle/btl_demo_cam.h:52), o mesmo das câmeras do file[2] e do cenário.

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | 4 bytes | 4 | unk0: igual nos 168 casos do disco, 4 caracteres ASCII (assinatura); ninguém lê; tratar como bytes |
| 0x04 | s32 | 4 | needsFixup: não zero enquanto os campos abaixo são deslocamentos (não zero em todos os 168) |
| 0x08 | s32 | 4 | channels: deslocamento em bytes **a partir do início da animação**; vira ponteiro em `DemoCam_FixupAnim` |
| 0x0C | s32 | 4 | channelCount |
| 0x10 | 4 bytes | 4 | unk10: ninguém lê; nos dados é zero ou um float plausível |
| 0x14 | f32 | 4 | length |
| 0x18 | s32 | 4 | defaults: deslocamento em bytes da pose inicial; vira ponteiro |
| 0x1C | s32 | 4 | unk1C: deslocamento de um bloco opcional; vira ponteiro e é zerado se version < 2 |
| 0x20 | s32 | 4 | version (0 nos 168) |

`DemoCamChannel` (btl_demo_cam.h:33), 0x10 bytes cada: s32 index (0..5 nos dados), s32 keys
(deslocamento em bytes a partir do início da animação, vira ponteiro), s32 keyCount, s32 unkC (zero).
`DemoCamKey` (btl_demo_cam.h:25), 0x20 bytes: u32 flags (bit 0x10 = degrau), f32 time, f32 value,
mais 0x14 bytes `unkC` não lidos; nos dados são palavras de 4 bytes com cara de float ou zero
(largura não determinada pelo código; 4 é o provável). `DemoCamPose` (btl_demo_cam.h:41), 0x20 bytes:
8 palavras de 4 bytes (pos[3], rot[3] f32, frame s32, unk1C f32).

Ordem nos dados: cabeçalho, pose, canais, chaves, bloco unk1C (16 ou 48 bytes até o fim da entrada,
incluindo o alinhamento). O bloco unk1C nunca é lido (version 0 em todos): conteúdo não determinado.

Ponteiros na carga: `DemoCam_FixupAnim` (btl_demo_cam.c:31) soma o endereço da animação a channels,
defaults, unk1C e a cada `keys`, e zera needsFixup **no próprio arquivo**, na primeira vez que a câmera
toca. A conversão de ordem de bytes tem de acontecer antes disso (na carga), senão soma um deslocamento
invertido.

Na demo: 37..40 vazias em 01473 e 01793. Não usadas na demo.

### 6.6 Entradas 0x35..0xFC do file[0] (53..252): faixas de lábio

Duas tabelas de 100: entradas 0x35..0x98 (53..152) quando `gSaveData->flags & 1` é 0, e 0x99..0xFC
(153..252) quando é 1 (bobj_a.c:2337..2343; o mesmo bit escolhe a segunda língua das vozes em
src/battle/view_b_c.c:37, `VOICE_LANG2_OFFSET`). Daí sai que N = 0xFC = 0x35 + 200 - 1.
Guardadas em `mdl.lip[100]` (include/battle/bobj_b.h:256, objeto + 0x758).

Struct: `BObjLipData` (include/battle/bobj_b.h:85) e `BObjLipKey` (:80).

| Desl. | Tipo | Largura | Significado |
|---|---|---|---|
| 0x00 | 8 bytes | | assinatura: idêntica nas 139.600 faixas do disco (4 caracteres ASCII e mais 4 bytes); ninguém lê; tratar como bytes. Largura dos 4 últimos não determinada |
| 0x08 | u16 | 2 | count: número de chaves |
| 0x0A | 6 bytes | | zero em todas; ninguém lê |
| 0x10 | `BObjLipKey`[count] | | u16 frame, u16 code (0 = boca fechada, outro = falando). Nos dados: frames crescentes e code 0 ou 1 |
| depois | | | zeros até 0x40 (em 922 faixas sobra mais que 0x40, sempre zerada) |

Leitores: `BtlObj_SetSubState` (modo `BOBJ_MOUTH_LIP`: lê count e o frame da última chave),
`BObjFace_StepLipData`, `BtlObjMdl_HasLipTrack` (só testa o ponteiro), todos em bobj_a.c. Nada vira
ponteiro além do próprio `mdl.lip[i]`; `face->lip` aponta para as chaves.

`BtlObj_BindCommonTables` (bobj_a.c:2445) sobrescreve `mdl.lip[]` com as faixas do arquivo comum 3,
mas só é chamada quando `BtlChar_TestMemberUnk70(chr)` (src/battle/btl_char_mgr.c:426). Na demo as
faixas do personagem **são usadas**: o rastreio leu as entradas 210..213, 231..233, 242..243 de 01473 e
206, 207, 210, 211, 231..233, 240, 242 de 01793, todas na faixa 153..252, ou seja, com o bit 0 de
`gSaveData->flags` ligado (segunda língua).

Conferência: em 01473, tipos.tsv tem 9 leituras de count (u16, `BObjFace_StepLipData` e
`BtlObjFace_Reset` sem nome de campo, porque leem a cópia em `face->lipCount`), 27 de frame e 9 de code,
todas de 2 bytes; mapa.tsv inverte 9 counts, 18 frames e 9 codes, todos de 2 bytes. Em 01793: 9 counts,
61 frames e 26 codes em tipos.tsv; o mapa inverte 9 + 35 + 26, todos de 2 bytes. Nenhuma divergência.
As entradas 29..40 não têm nenhuma posição em tipos.tsv nem em mapa.tsv nos dois arquivos.

## 7. file[2]: o pacote de efeitos do personagem (01482, 01802)

### 7.1 Quem carrega e quem liga

- Id do arquivo: `chara * 10 + 0x599` (`BtlObj_RequestCharaModel`, `src/battle/btl_obj.c:513`), lido por
  `BtlRes_Request` / `BtlRes_LoadFile` para `slot->file[2].buf` (nos dois slots fixos, num buffer de
  `BTL_RES_ANIM1_SIZE` = 0xCE800 bytes). Na luta de demonstração: 01482 (lutador 0) e 01802 (lutador 1).
- `BtlObj_BindTables` (`src/battle/bobj_a.c:2295`) liga as entradas 1..5 a `mdl.unk84[0..4]` (objeto + 0x9C..0xAC)
  e a entrada 6 a `mdl.unk40` (objeto + 0x58). Leitores:
  - `BtlCharApi_GetPlayerObjUnk9C(player, n)` (`src/battle/btl_capi_a.c:288`) devolve `unk84[n]`, n = 0..4: é o
    pacote de efeito do **espaço de técnica n** (0, 1 habilidades; 2, 3 técnicas; 4 especial, ver `include/battle/eft_h.h:12`).
    `EftShotChar_Init` (`src/battle/eft_g.c:1710`) chama `EftShot_InitSlot(chr, n, pacote)` para os cinco.
  - `BtlChar_BindObject` (`src/battle/btl_char_mgr.c:243`) lê `BtlMgrObj.file[0..2]` (objeto + 0xA4..0xAC, isto é
    `unk84[2..4]`, entradas 3, 4, 5) e guarda `FILE_TABLE(pacote, 3)` em `chr->objTbl[i]` = `BtlTechBChr.place[i]`
    (`include/battle/btl_tech_b.h:284`): a subentrada 3 do pacote de técnica é a tabela de posicionamento.
  - `BtlCharApi_GetPlayerObjUnk58` (`btl_capi_a.c:336`) devolve `unk40`; `BtlScene_GetCharPackEntry(side, idx)`
    (`src/battle/btl_scene.c:253`) indexa esse pacote do personagem.

### 7.2 F2.1 Pacote (tabela de deslocamentos), o mesmo formato em todos os níveis

O arquivo inteiro, cada uma das 6 entradas e várias subentradas são "pacotes" no mesmo formato da tabela do file[0]:

| Desl. | Tipo | Larg. | Significado |
|---|---|---|---|
| 0x00 | u32 | 4 | N, número de entradas (no topo: 6) |
| 0x04 + 4*(i-1) | u32 | 4 | deslocamento em BYTES da entrada i (1..N), relativo ao início DESTE pacote |
| 0x04 + 4*N | u32 | 4 | fim da última entrada (= tamanho do pacote) |
| até o 1º deslocamento | u32 | 4 | zeros de preenchimento |

- Leitura: `BtlScene_GetPackEntry(base, i)` = `base + (base[i] >> 2 << 2)` (`btl_scene.c:231`); tamanho =
  `base[i+1] - base[i]` (`BtlScene_GetPackEntrySize`); vazia quando os dois são iguais (só alguns chamadores testam).
  Os deslocamentos NUNCA viram ponteiro: são somados a cada leitura.
- Conferido nos dois arquivos (13 tabelas em cada): os bits baixos são sempre 0, todo deslocamento é múltiplo de
  0x40, o preenchimento é todo zero e a tabela termina num múltiplo de 0x40.
- Conversão: todas as palavras da tabela são u32 (inclusive as de preenchimento, que são zero).

### 7.3 Entradas do topo

| Índice | Uso | Formato | Quem lê | Demo 01482 / 01802 |
|---|---|---|---|---|
| 0 | contagem N = 6 (não é entrada) | u32 | | |
| 1 | pacote de efeito do espaço 0 (habilidade 1) | F2.2 pacote de técnica | `EftShot_InitSlot` e a classe do tipo de efeito | 01482: só a tabela e um cabeçalho vazio, nada lido; 01802: só a carga (cabeçalho e texturas) |
| 2 | espaço 1 (habilidade 2) | F2.2 | idem | só a carga, nas duas |
| 3 | espaço 2 (técnica 1) | F2.2; subentrada 3 = F2.9 | idem + `BtlChar_BindObject` | 01482: só a carga; 01802: usada (técnica disparada) |
| 4 | espaço 3 (técnica 2) | F2.2; subentrada 3 = F2.9 | idem | 01482: usada (posicionamento lido); 01802: usada |
| 5 | espaço 4 (especial) | F2.2; subentrada 3 = F2.9 | idem | só a carga, nas duas |
| 6 | pacote do personagem (efeitos que não são de técnica) | F2.3 | `BtlScene_GetCharPackEntry` | usada nas duas |

### 7.4 F2.2 Pacote de técnica (entradas 1..5)

É um pacote F2.1 (01482: N = 4, 22, 45, 4, 76; 01802: N = 24, 20, 64, 46, 36) com subentradas de papel fixo e um
conjunto de emissores a partir da 4:

| Sub | Papel | Formato | Quem lê |
|---|---|---|---|
| 1 | opcional; o formato depende do tipo de efeito da técnica, das suas flags e do id (7.4.1): modelo no formato do file[0], pacote de modelos por traje, anel (malha F2.12 + texturas) ou conjunto de texturas F2.5 | 7.4.1 | `eft_h.c`, `eft_j.c`, `eft_k.c` |
| 2 | não determinado (nenhum leitor com índice constante 2 num pacote de técnica); vazia nos 805 pacotes de técnica do disco | | |
| 3 | tabela de posicionamento da cena da técnica (só espaços 2..4) | F2.9 | `BtlSuper_GetStagePlacement` (`src/battle/btl_tech_b.c:1438`) |
| 4 | cabeçalho do conjunto de emissores | F2.4 | `EftEmit_LoadSet(..., pacote, 0, 4)` (`src/battle/eft_h.c:928`) |
| 5.. | recursos dos grupos, na ordem que F2.4 define | F2.5 / F2.6 / F2.8 | `EftEmit_LoadSet` e módulos de partícula |

Nos dois arquivos da demo as subentradas 1 e 2 estão vazias em todos os pacotes de técnica, e a 3 só existe em
01482 entrada 4 (0xB00 bytes). `BtlChar_BindObject` calcula `FILE_TABLE(pacote, 3)` sem testar se está vazia.

#### 7.4.1 Subentrada 1: as variantes, por tipo de efeito

Quem lê a subentrada 1 é o "gerenciador" da técnica: `EftShot_CreateSlotTask` (`src/battle/eft_h.c:302`)
cria a tarefa da classe `gEftShotClass[tipo + 1]` (tipo -1 usa a linha 0), e o init dessa classe pega
`BtlScene_GetPackEntry(pacote, 1)` conforme o tipo, as flags e o id da técnica. Esses três valores vêm do
file[0] do mesmo personagem, copiados por `EftShot_BuildParam` (`src/battle/eft_h.c:132`) para
`EftShotParam`:

| Espaço (entrada do topo) | Fonte no file[0] | tipo (u8) | flags (u32) | id (u16) |
|---|---|---|---|---|
| 0, 1 (entradas 1, 2), n = espaço | entrada 0x19, `EftSkillSrc` (`include/battle/eft_h.h:155`) | 0x60 + n | 0x00 + 4n (`flagsA` de 5.9) | 0x10 + 2n |
| 2..4 (entradas 3..5), n = espaço - 2 | entrada 0x18, `EftSuperSrc` (`include/battle/eft_h.h:181`) | 0x138 + n | 0x000 + 4n (`flagsA` de 5.8) | 0x018 + 2n |

No disco os três valores são iguais em todos os trajes de cada personagem (conferido nos file[0] de
trajes 0..3 e variantes 4..7 que existem).

| Tipo | Módulo (init do gerenciador) | Condição | Formato da sub 1 | Quem usa | No disco |
|---|---|---|---|---|---|
| -1 | nenhum (`EftShotNull*`, `eft_h.c`) | | não lida | | sempre vazia (68) |
| 0 | `EftBlastMgr_Init` (`src/battle/eft_j.c:1484`) | | não lida | | sempre vazia (480) |
| 1 | `EftVolleyGroup_Init` (`src/battle/eft_h.c:873`) | flags & 0x20000 | modelo no formato do file[0] (`set->extra`) | `EftBlastObj_CreateWithModel` (`eft_h.c:536`) → `EftObj_Create` (`src/battle/eft_o_b.c:187`) | 2 com a flag (técnicas 0x26E, 0x26F: 01922 e 02592, entrada 5), modelo; 59 sem a flag, vazia |
| 2 | `EftShotTechMgr_Init` (`src/battle/eft_k.c:618`) | flags & 0x20000 | modelo (`modelPack`) | `EftShotTech_InitModel` (`src/battle/eft_j.c:1510`) → `EftObj_Create` | 1 com a flag (02912, entrada 5), modelo; 126 sem, vazia |
| 3 | `EftSweepGroup_Init` (`src/battle/eft_i.c:1706`) | | não lida | | sempre vazia (11) |
| 4 | `EftRingShotMgr_Init` (`src/battle/eft_l_b.c:897`) | | não lida | | sempre vazia (9) |
| 5 | `EftMultiMgr_Init` (`src/battle/eft_j.c:608`) | id = 0x158 ou 0x202 | conjunto de texturas F2.5 (`EftTexSet_Load32`) | o próprio init | 2 (02392 entrada 4, 03002 entrada 3; 2 texturas cada); as 6 de outro id, vazias |
| 6 | `EftPropShotMgr_Init` (`src/battle/eft_j.c:1089`) | sempre; id = 0x19A muda o formato | id 0x19A: pacote F2.1 de modelos por traje (entrada traje + 1, traje = `BtlCharApi_GetCostume`); outros ids: modelo | `EftPropShot` (`eft_j.c:686`) → `EftObj_Create` | 12: 11 modelos e 1 pacote por traje (02432 entrada 3, N = 2, dois modelos) |
| 7 | `EftFollowGroup_Init` (`src/battle/eft_j.c:201`) | | não lida | | sempre vazia (11) |
| 8 | `EftObjTechMgr_Init` (`src/battle/eft_k.c:1718`) | flags & 0x20000: modelo; flags & 0x40000: anel | 0x20000: modelo (`modelPack`); 0x40000: pacote F2.1 com sub 1 = malha de efeito F2.12 (`EftMesh_Init`) e sub 2 = conjunto de texturas F2.5 (`EftTexSet_Load32`) | `EftObjTech_CreateModel` (`eft_k.c:1106`) → `EftObj_Create`; o anel em `EftObjTech_DrawRings` (`eft_k.c:1206`) | 3 com 0x20000 (modelo), 2 com 0x40000 (01922 entrada 4, 02582 entrada 3: anel de 1 grupo, 96 triângulos, 192 vértices, 1 textura), 1 sem flag (vazia); nenhuma com as duas flags |
| 9 | `EftRushShotMgr_Init` (`src/battle/eft_k.c:2467`) | flags & 0x20000 | modelo (`sub[0]`) | `EftRushShot_CreateModels` (`eft_k.c:1798`) → `EftObj_Create` | 1 com a flag (02372 entrada 4, técnica 0x19D), modelo; 11 sem, vazia |

"Modelo" é um arquivo inteiro no formato do file[0] (tabela de 0xFC entradas, 3.1), entregue a
`BtlObj_Create(1, res, 1)` por `EftObj_Create` (`src/battle/eft_ad_b.c:752`, `EftObjRes` em
`include/battle/eft_ad.h:304`, só o file[0] preenchido). Esse objeto passa por `BtlObj_BindTables` e
`BtlObjMdl_BindFiles` como um lutador: os deslocamentos da tabela são relativos ao início da subentrada,
as entradas são as do file[0] (seções 4 a 6) e as mesmas escritas na ligação acontecem no buffer do
file[2] (8). Nos 25 modelos que o file[2] traz no disco (18 subentradas 1 de técnica, 2 dentro do pacote
por traje, 5 no pacote do personagem, 7.5.1), as entradas presentes são só 3 e 12 (23 modelos) ou 3, 12 e
29..33 (os dois modelos do tipo 1, com animações do próprio modelo tocadas por `EftObj_PlayAnim`); nenhum
tem costura. O conversor do file[0] passa neles com 100% de cobertura.

Concordância: nos 805 pacotes de técnica do disco, a subentrada 1 existe exatamente quando a tabela acima
diz que alguém a lê (23 casos), e o formato encontrado é o que a tabela diz em todos os 23.

**Como o conversor decide.** Há dois critérios, que dão o mesmo resultado no disco inteiro:

1. Pelo código (exato): ler tipo, flags e id do file[0] do personagem (qualquer traje) e aplicar a tabela.
   Exige que o conversor do file[2] receba o file[0].
2. Pela forma (o que o teste usou; só o próprio file[2]), com w0 e w1 as duas primeiras palavras u32 da
   subentrada 1 lidas em little-endian:

   | Teste | Formato |
   |---|---|
   | w0 = 0xFC e w1 = 0x400 (tabela de 0xFC entradas com a entrada 1 em 0x400) | modelo |
   | w1 = 8 (`TexFile.entOfs` em palavras; w0 = número de texturas) | conjunto de texturas F2.5 |
   | senão: pacote F2.1 (w0 = N, w1 = 0x40) cuja entrada 1 começa com 0xFC, 0x400 | modelos por traje (cada entrada não vazia é um modelo) |
   | senão: pacote F2.1 com N = 2 | anel: entrada 1 malha F2.12, entrada 2 conjunto de texturas |
   | qualquer outra coisa | erro (formato não especificado) |

   No disco: 18 modelos, 2 conjuntos de texturas, 1 pacote por traje e 2 anéis. Os testes não se
   confundem: um pacote F2.1 tem w1 = (N + 2) x 4 arredondado a 0x40, nunca 8; um `TexFile` tem w1 = 8.

### 7.5 F2.3 Pacote do personagem (entrada 6)

Pacote F2.1 com N = 11 (idêntico em estrutura nos dois arquivos, exceto a 8):

| Sub | Uso | Formato | Quem lê | Demo |
|---|---|---|---|---|
| 1 | bits de "tem efeito" por tipo de efeito do lutador | F2.10 | `BtlScene_TestCharPackBit` (`btl_scene.c:244`), chamado por `eft_q.c:789` | sim |
| 2 | conjunto de emissores da carga de ki | F2.1 com cabeçalho F2.4 na sub 1 | `EftChargeMgr_Init` (`eft_r.c:932`): `EftEmit_LoadSet(..., pacote, 2, 1)` | sim |
| 3 | rajadas de ki (tipo 1) | conjunto (F2.1 + F2.4 na sub 1) ou, se o lutador tem rajada de tipo 2, pacote {modelo, texturas} (7.5.1) | `EftKiBlastMgr_Init` (`eft_r.c:1288`, idx 3) ou `EftKiObjMgr_Init` (`eft_s.c:111`) | sim (como conjunto) |
| 4 | rajadas de ki (outro tipo) / carga de rajada | conjunto ou, se o lutador tem rajada de tipo 3, pacote {modelo} (7.5.1) | `EftKiBlastMgr_Init` (idx 4) e `EftBlastChargeMgr_Init` (`eft_r.c:1468`), ou `EftKiBombMgr_Init` (`eft_r.c:1664`) | sim (como conjunto) |
| 5 | não determinado (nenhum leitor com índice constante 5) | | | vazia nas duas |
| 6 | onda de choque: sub 1 = pacote de sprites "V000" (F2.8), sub 2 = pacote de texturas (F2.5) | F2.1 | `EftShockMgr_Init` (`eft_ad.c:491`) | sim |
| 7 | conjunto de emissores da explosão do rush | F2.1 + F2.4 na sub 1 | `EftRushBurstMgr_Init` (`eft_q.c:636`) | sim |
| 8 | animações de câmera das técnicas: sub k (k = 1..3) = espaço k+1 | F2.1 de F2.11 | `EftShotTechMgr_Init` (`eft_k.c:618`), `EftRushShot` (`eft_k.c:2482`), `EftCam_Start` (`eft_a.c:988`) | 01482: sub 2 usada; 01802: as quatro vazias |
| 9 | conjunto de emissores do "CharaFx" | F2.1 + F2.4 na sub 1 | `EftCharaFxMgr_Init` (`eft_q.c:1901`) | vazia nas duas |
| 10 | não determinado (nenhum leitor com índice constante 10) | | | vazia |
| 11 | conjunto de emissores da absorção | F2.1 + F2.4 na sub 1 | `EftAbsorbMgr_Init` (`eft_l_c.c:225`) | vazia |

A sub 4 da 8 nunca é lida (o laço vai de 1 a 3). Os conjuntos do pacote do personagem usam `common = 2` e `idx = 1`:
cabeçalho na sub 1 e recursos a partir da 2, na mesma ordem de F2.4.

#### 7.5.1 Subentradas 3 e 4: conjunto de emissores ou modelo

`EftChar_Init` (`src/battle/eft_q.c:776`) cria, para cada "tipo" k de efeito do lutador (k = 0..10), o
gerenciador da classe `gEftCharKindClass[k]` quando o bit do bloco F2.10 está ligado; os tipos 1 e 2 leem
as subentradas 3 e 4. Duas exceções trocam a classe, "seja qual for o pacote":

| Condição (file[0] do lutador) | Efeito | Leitor | Formato da subentrada |
|---|---|---|---|
| alguma das 13 rajadas da entrada 0x14 tem `type` (+0x02, s8) = 2: `BtlCharApi_HasKiBlastType2` (`src/battle/btl_capi_a.c:197`) | o tipo 1 vira a classe `D_002C3E00` (`EftChar_CreateKind`, `eft_q.c:842`), mesmo com o bit desligado | `EftKiObjMgr_Init` (`src/battle/eft_s.c:111`) | sub 3 = pacote F2.1: entrada 1 modelo no formato do file[0] (`res->model`, ligado por `EftObj_Create`), entrada 2 conjunto de texturas F2.5 (`EftTexSet_Load4`) |
| alguma rajada com `type` = 3: `BtlCharApi_HasKiBlastType3` (`btl_capi_a.c:218`) | o tipo 2 vira `gEftKiBombMgrClass` e o tipo 3 (carga de rajada) é desligado | `EftKiBombMgr_Init` (`src/battle/eft_r.c:1664`) | sub 4 = pacote F2.1: entrada 1 modelo |
| nenhuma das duas | tipos 1, 2 e 3 normais | `EftKiBlastMgr_Init` (`eft_r.c:1288`, idx 3 para o tipo 1 e 4 para o 2), `EftBlastChargeMgr_Init` (`eft_r.c:1468`, idx 4) | conjunto de emissores (F2.1 com F2.4 na entrada 1) |

No disco (161 pacotes do personagem): rajada de tipo 2 em 3 lutadores (file[2] 01552, 01982, 02032), e
nos três a sub 3 é {modelo, texturas}; rajada de tipo 3 só em 01982, e a sub 4 dele é {modelo} (N = 1).
Nas outras 158 subs 3 e 159 subs 4, conjunto de emissores.

Exceção não explicada: em 01552 a sub 4 também é {modelo, texturas} (N = 2), mas o lutador não tem rajada
de tipo 3 e o efeito 3 do bloco F2.10 está ligado (bit 2 da palavra, o que `EftChar_Init` testa para os tipos 2 e 3). Pelo código, quem a lê são `EftKiBlastMgr_Init` (idx 4) e
`EftBlastChargeMgr_Init`, como conjunto de emissores: a tabela do modelo seria tomada por cabeçalho F2.4
(máscara 0xFC, `nGroups` = byte 4 = 0, nenhum recurso consumido), e `EftKiBlast_SpawnParts`
(`eft_r.c:1090`) percorreria os bits 2..7 da máscara com grupos que `EftEmit_LoadSet` nunca preencheu.
Isso sugere dado sem uso (nenhuma rajada desse tipo disparada por esse lutador), mas não foi provado. Se
o conversor tratar essa sub como modelo (critério pela forma, abaixo), só os bytes do cabeçalho
0x04..0x07 e 0x20..0x27 ficariam numa ordem diferente da que o PS2 vê, caso sejam lidos: não determinado.

**Como o conversor decide.** Pelo código (exato): os tipos das 13 rajadas do file[0] (no disco, a condição
dá o mesmo resultado em todos os trajes de cada lutador). Pela forma (só o file[2], o que o teste usou): se a entrada 1 do pacote da sub 3
ou 4 começa com as palavras 0xFC e 0x400 (tabela de file[0]), a sub é {modelo, e texturas se houver
entrada 2}; senão é conjunto de emissores. As máscaras de cabeçalho de emissores do disco só usam os bits
0, 2, 5, 9, 10, 14..18 (75 máscaras diferentes): 0xFC (bits 2..7) nunca é máscara, e a entrada 1 de um
cabeçalho F2.4 nunca começa com 0xFC. Os dois critérios concordam em todos os casos, menos a sub 4 de 01552
(acima).

### 7.6 F2.4 Conjunto de emissores (cabeçalho de efeito)

`EftEmit_LoadSet` (`src/battle/eft_h.c:928`) lê o cabeçalho da entrada `idx` e consome as entradas seguintes do
MESMO pacote, em ordem, sem tabela própria. Três views do mesmo dado: `EftSetHead` / `EftSetGroupDef` /
`EftSetDef` (`include/battle/eft_h.h:225`, `:251`, `:259`) e `EftEmitHdr` / `EftEmitDef` (`include/battle/eft_i.h:135`, `:53`).
Onde discordam vale a de `eft_i.h` (código casado) e o rastreio confirma.

Cabeçalho (0x40 bytes):

| Desl. | Tipo | Larg. | Significado |
|---|---|---|---|
| 0x00 | u32 | 4 | `mask`: bit por tipo de grupo presente (19 tipos) |
| 0x04 | u8 | 1 | `nGroups` |
| 0x05 | u8 | 1 | `nParts` (total de partes) |
| 0x06 | u8 | 1 | tipo de mira (`EftEmit_GetAimKind`) |
| 0x07 | u8 | 1 | não determinado |
| 0x08..0x1F | f32 x6 | 4 | animação da largura do rastro (`width0..2`, `widthTime`, `widthSplit`, `widthStep`) |
| 0x20..0x27 | u8 x8 | 1 | `unk20` (quadros), `flags` (0x21), 0x22 (tipo), 0x23..0x24, 0x25, 0x26, `phaseMask` (0x27) |
| 0x28..0x3F | f32 x6 | 4 | 0x28, segunda animação `b0..b2`, `bTime`, `bSplit` |

`EftEmitHdr.unk4` declara 0x04 como s32, mas o rastreio só lê bytes ali: são quatro u8.

Depois do cabeçalho, `nGroups` registros de grupo (0x20 bytes, `EftSetGroupDef`): 0x00 u8 `kind` (0..18),
0x01 u8 `nParts`, 0x02 u8 `nRes`, 0x03..0x1F não determinado (nenhuma leitura; tratar como bytes).
Depois, `nParts` registros de parte (0x40 bytes, `EftEmitDef`): 0x00..0x0B u8 (`flags`, `tex`, parâmetros,
`kind` em 0x08, 0x09, `node` 0x0A, `dirMode` 0x0B); 0x0C..0x33 f32 x10 (`spread`, `offset`, `offset2`, `rate`,
`scale0..2`, `scaleTime`, `scaleSplit`, `scaleStep`); 0x34..0x37 u8 (`flags2`, `cond`, `altNode`, `scaleMode`);
0x38..0x3F não determinado (nenhuma leitura). `EftSetDef` (`eft_h.h:259`) trata 0x0C..0x0F e 0x20..0x33 como
bytes; está errado (o código casado e o rastreio leem f32). O registro de cabeçalho ocupa
0x40 + 0x20*nGroups + 0x40*nParts bytes, arredondado a 0x40 no pacote.

**Ordem dos recursos (delimitação):** a partir de `idx + 1`, para cada grupo em ordem, se `kind != 0`:
primeiro `nRes` entradas de pacote de texturas (F2.5, carregadas por `EftTexSet_Load32/16/8/4` conforme
`EftEmit_TypeToResKind`: tipo 2 → Load4; 9 → Load8; 5, 10, 12, 15, 16, 17, 18 → Load16; demais → Load32), depois,
para cada uma das `nParts` partes, a entrada **a** e, salvo nos tipos 2 e 14, a entrada **b** (F2.6). Grupos de
tipo 0 (luz) não consomem entradas. Conferido: em todos os 9 conjuntos de cada arquivo esse percurso consome
exatamente as N entradas do pacote, sem sobra.

### 7.7 F2.5 Pacote de texturas (`TexFile`)

Mesmo formato das texturas do modelo: `TexFile` / `TexEntry` (`include/sys/gfxm_b_b.h:47`, `:29`); views
`EftTexPack`/`EftTexPackImage`/`EftVramImage` (`include/battle/eft_det_a.h:289`, `:296`) e `EftAeTexPack`/`EftAeImage`
(`include/battle/eft_ae.h:82`). `Res_RelocateOffsets(&hdr, pack, pack)` (`src/sys/gfxm_b_b.c:244`) roda na carga
(dentro de cada `EftTexSet_Load*`) e ESCREVE no próprio arquivo.

Cabeçalho: 0x00 u32 `count`; 0x04 u32 `entOfs` (em PALAVRAS a partir do início do pacote; observado sempre 8, isto
é, entradas em +0x20); 0x08..0x0F não determinado (não zero em todos os pacotes, nunca lido); 0x10 u32 `ent` (sobrescrito com ponteiro na carga).

Entrada (0x40 bytes, `count` delas): 0x00 u32 `pixOfs` e 0x04 u32 `clutOfs` (em PALAVRAS do início do pacote, 0 =
nenhum); 0x08 s32 `pixSize`, 0x0C s32 `clutSize` (bytes); 0x10, 0x14 s32 blocos; 0x18, 0x1C u32 (DBW/DPSM do
BITBLTBUF); 0x20, 0x24 s32; 0x28..0x2F u8 x8 (`unk28`; [4] e [5] lidos como byte no modelo); 0x30 u64 `tex0`
(lido como 8 bytes por `EftVram_AddImage`/`AddTex` e `EftTexSet_Load*`); 0x38 `pix`, 0x3C `clut` (ponteiros
escritos na carga).

**Bloco de imagem e bloco de CLUT** (`pixOfs*4` por `pixSize` bytes, `clutOfs*4` por `clutSize` bytes), enviados
por `EftVram_Upload` (`src/battle/eft_det_a.c:240`) com DMA REF, sem a CPU ler o conteúdo. Layout conferido em
todas as 74 (01482) e 92 (01802) texturas, imagem e CLUT:

| Desl. | Conteúdo | Números |
|---|---|---|
| 0x00 | 4 palavras VIF: três NOP e DIRECT (0x50) com IMMEDIATE = (tamanho / 16) - 1 | u32 x4 |
| 0x10 | GIFtag PACKED, NLOOP 3, EOP, NREG 1, REGS = A+D | u64 x2 |
| 0x20, 0x30, 0x40 | A+D: TRXPOS (0x51), TRXREG (0x52), TRXDIR (0x53) | u64 dado + u64 endereço |
| 0x50 | GIFtag IMAGE (FLG 2), NLOOP = quadwords de imagem, EOP | u64 x2 |
| 0x60 | NLOOP * 16 bytes de imagem | bytes (ver abaixo) |
| fim - 0x20 | GIFtag NLOOP 1, EOP, A+D e A+D TEXFLUSH (0x3F) com dado 0 | u64 x4 |

Delimitação: `tamanho = 0x60 + NLOOP*16 + 0x20` (conferido em todas). Formatos encontrados: PSM 0x13 (PSMT8, 160
texturas) e 0x14 (PSMT4, 7), todas com CLUT PSMCT32 (CPSM 0). Imagem = índices de 8 ou 4 bits (bytes), CLUT =
RGBA de 8 bits por canal (bytes): nenhum número de 2/4 bytes dentro dos dados. A F3 vai recodificar tudo isso; para
a ordem de bytes, os cabeçalhos/rodapés GIF/VIF são u32/u64 e o miolo fica intacto.

### 7.8 F2.6 Dados das partes (entradas a e b), por tipo de grupo

Tipo do grupo → módulo (`EftEmit_Spawn`, `src/battle/eft_i.c:596`). Na tabela, "f32" vale para cada palavra do
intervalo; os tamanhos das entradas no pacote são arredondados a 0x40.

| Tipo | Módulo | a (struct, header) | Layout de a | b (struct) | Layout de b | Na demo |
|---|---|---|---|---|---|---|
| 0 | luz | sem entradas | | | | |
| 2 | raios (`EftRays`) | `EftRaysDef` (`include/battle/eft_o.h:166`), 10 x `EftRaysRayDef` (`:151`, 0x78) + 0x4B0 s32 `count`, 0x4B4 s32 `flags` | por raio: 0x00..0x2F 3 x {f32, f32, f32, u8[4] cor}; 0x30..0x6F f32; 0x70 u16 `delay`; 0x72 s8; 0x73..0x77 u8 | não tem | | presente, nunca lido |
| 5 | partículas (`EftPtcl`) | `EftUPtclDef` (`include/battle/eft_u.h:172`; view menor `EftPtclDef`, `eft_v.h:137`) | 0x00 s32 `flags`; 0x04..0x0F u8; 0x10..0x1B f32; **0x1C..0x27 s16 x6** (`count[3]`, `countRange[3]`); 0x28..0x2AF f32 | `EftUPtclDef2` (`eft_u.h:160`) | 0x00..0x113 f32 (cores Vec4 x12, pulsos, tempos) | lido |
| 9 | quads (`EftQuad`) | `EftQuadDef` (`include/battle/eft_y.h:100`) e `EftPart9Def` (`include/battle/eft_x.h:374`) | 0x000..0x233 f32; 0x234 s32 `flags`; 0x238..0x242 u8; **0x243 até o fim da entrada: sem leitor** (nota abaixo) | `EftQuadDef2` (`eft_y.h:140`) | 0x00..0x12B f32 | lido |
| 10 | partículas em anéis (`EftPart10`, `src/battle/eft_w.c` e `eft_x.c`) | `EftPart10Def` (`include/battle/eft_x.h:104`; view parcial `include/battle/eft_w.h:254`), 0x20E bytes | 0x000..0x1F3 f32 (chaves de tamanho, ângulo, giro, velocidade, vida, escala, contagem, tempos); 0x1F4 s32 `flags`; 0x1F8..0x1FB u8 (`texCols`, `texRows`, `texStep`, `layer`/`blend`); 0x1FC..0x20B f32 (`ringFrom`, `ringTo`, `yaw`, `pitch`); 0x20C u8 `drawMode`; 0x20D u8 (`eft_x.c:300` compara com 1 e 2) | `EftPart10Def2` (`eft_x.h:161`), 0x12C | 0x000..0x12B f32 (cores Vec4 x12, estiramento, tempos) | 83 partes no disco, nenhuma na demo |
| 12 | "zap": linhas (`EftZap`, `src/battle/eft_ab_c.c`, `src/battle/eft_ad.c`) | `EftZapDef` (`include/battle/eft_ac.h:222`; view `EftZapPrm`, `include/battle/eft_ad.h:93`, só `flags`), 0xB8 bytes | 0x00..0x9F f32; 0xA0 s32 `ptsMin`; 0xA4 s32 `ptsRange`; 0xA8..0xB3 u8 (`blend`, `facing`, seis modos, `interval`, `count`, 2 sem leitor); 0xB4 s32 `flags` | `EftZapKeyTbl` (`eft_ac.h:266`; view `EftZapKeys`, `eft_ad.h:99`), 0x12C | 0x000..0x12B f32 | **ausente do disco** (nenhum grupo de tipo 12 e nenhuma máscara com o bit 12): layout só pelo código, não conferido com dados |
| 15 | corrente de sprites entre dois pontos (`EftLink`, `src/battle/eft_v_c.c` e `eft_w.c`) | `EftLinkDef` (`include/battle/eft_v.h:361`) e `EftWLinkDef` (`include/battle/eft_w.h:65`), 0x1A4 bytes | 0x000..0x19B f32; 0x19C..0x19F u8 (`cols`, `rows`, `texStep`, `layer`); 0x1A0 s32 `flags` | `EftLinkDef2` (`eft_v.h:402`) e `EftWLinkKey` (`eft_w.h:101`), 0x12C | 0x000..0x12B f32 | 37 partes no disco, nenhuma na demo |
| 14 | objeto animado | pacote de sprites "V000" (F2.8) | ver F2.8 | não tem | | lido |
| 16 | sprite (`EftBill`) e linha (`EftLine`) | `EftBillDef` (`include/battle/eft_z.h:46`) e `EftLineDef` (`eft_y.h:430`) | 0x00 s32 `flags`; 0x04..0x07 u8; 0x08..0x0B ver abaixo; 0x0C..0x8F f32 | `EftLineDef2` (`eft_y.h:452`) / `EftVec` | 0x00..0x83 f32 | lido |
| 17 | fita (`EftRibbon`) | `EftRibbonPrm` (`include/battle/eft_ac.h:108`) e `EftRbnPrm` (`include/battle/eft_ab.h:356`) | 0x00 s32; 0x04..0x07 u8; 0x08..0x5F f32 | `EftRbnAnim` (`eft_ab.h:374`) | 0x00..0xB3 f32 | lido |
| 18 | arco/corrente (`EftChain`) | `EftArcParam` (`include/battle/eft_s.h:148`, 0x1E8) | 0x00 u32; 0x04..0x0B u8; 0x0C..0x1E7 f32 | `EftArcKeys` (`eft_s.h:202`) | 0x00..0x113 f32 | lido |

- Tipo 9: `EftQuadDef` declara 0x224..0x233 como `u8[0x10]`, mas `EftPart9Def` e o rastreio leem f32 em 0x224,
  0x228, 0x22C, 0x230: são f32. Os bytes 0x238..0x242 têm nomes diferentes nas duas views (`interval[3]`/`count[3]`
  contra `interval` em 0x23A e `count` em 0x23D); para a conversão tanto faz, são todos u8.
- Tipo 16, 0x08..0x0B: `EftBillDef` diz f32 `frameTime`, `EftLineDef` diz bytes `unk5[7]`. O rastreio não leu essa
  posição. Não determinado; f32 é o mais provável (a view do sprite é a que lê o campo seguinte como f32 de tempo).
- Tipo 5: os conflitos de `mapa.tsv.conflitos` (01482 +0x6329C e +0x7A49C; 01802 +0x41D9C, +0x5D4DC, +0x955DC,
  +0xAC7DC) caem todos em `EftUPtclDef` + 0x19C (`radiusRange[0..2]` e `ring[0]`): `Port_FastMtxApply` lê 16 bytes
  de uma vez e outras leituras pegam f32 dentro. Pelo layout são quatro f32: inverter de 4 em 4.
- **Tipo 9, entrada a maior que a struct** (a causa dos 60 erros "parte de efeito: byte N não reconhecido" do
  disco; o arquivo para no primeiro erro, por isso eles aparecem em entradas 1..5 e nas subs 2, 3, 4 do pacote do
  personagem). `EftQuadDef`/`EftPart9Def` terminam em 0x242 e nenhum código lê além disso: o módulo de quads
  inteiro está em C casado (`src/battle/eft_x_c.c`, `src/battle/eft_y.c`, sem `INCLUDE_ASM`) e só lê a
  definição pelos campos nomeados. Nos dados, das 1.250 partes de tipo 9 do disco, 196 têm bytes não nulos
  depois de 0x242 (o último em até 0x25B): o byte 0x243 é sempre zero e as palavras 0x244..0x258 são f32
  plausíveis ou zero (nenhum inteiro pequeno). Parece uma versão maior da struct com campos que este
  executável não usa. Largura não determinada pelo código; como ninguém lê, o conversor deixa 0x243 até o fim
  da entrada como bytes, sem exigir zero (inverter como u32 seria igualmente inofensivo). Nenhum outro tipo
  passa do tamanho da struct: o último byte não nulo fica dentro dela em todas as partes do disco (tipo 2:
  0x4B4; 5: 0x2AF e 0x113; 10: 0x20D e 0x12B; 14: dentro do pacote "V000"; 15: 0x1A1, dentro de `flags`, e
  0x12B; 16: 0x8F e 0x83; 17: 0x5F e 0xB3; 18: 0x1E7 e 0x113; e a entrada b do tipo 9: 0x12B).
- Tipos 10 e 15: onde as views discordam, todas leem a mesma largura. Tipo 10: `eft_w.h` deixa
  0x000..0x17F, 0x184..0x1CB e 0x1D0..0x1E7 como `u8[]` sem leitura, `eft_x.h` os tipa como f32 e o código de
  `eft_x.c` os lê como f32. Tipo 15: `EftLinkDef` declara 0x164..0x16F como `u8[0xC]`, mas `EftWLinkDef` lê
  f32 em 0x164 (`fadeIn`), 0x168 (`fadeOut`) e 0x16C (`sizeSplit`) (`src/battle/eft_w.c:439`, `:466`): são f32.
  Nos dados, o último byte não nulo da entrada a do tipo 15 é 0x1A1 nas 37 partes, o segundo byte de `flags`
  (bits 0x200 e 0x400 citados no header): confirma `flags` como s32.
- Critério do conversor: o tipo do grupo (byte 0 do registro de grupo, F2.4) escolhe o layout da entrada a e
  da b; com os layouts desta tabela, toda entrada a/b do disco fecha (resto zero até o alinhamento, salvo o
  tipo 9 acima).

### 7.9 F2.8 Pacote de sprites "V000" (tipo 14 e pacote do personagem 6.1)

`EftSprPack` / `EftSprLayerDef` / `EftSprKey` (`include/battle/eft_ae.h:188`, `:166`, `:145`).

| Desl. | Tipo | Larg. | Significado |
|---|---|---|---|
| 0x00 | char[4] | 1 | "V000" (a checagem foi retirada do executável; nunca lido): bytes, não inverter |
| 0x04 | u8 | 1 | `layerCount` |
| 0x05 | u8 | 1 | `loop` |
| 0x06 | u8 x2 | 1 | não determinado |
| 0x08 | s32 | 4 | `keysOfs`: deslocamento em bytes, relativo ao início do pacote, das chaves da 1ª camada |
| 0x0C..0x1F | | | não determinado (nunca lido) |
| 0x20 + 0x20*i | `EftSprLayerDef` | | 0x00..0x07 u8; **0x08 s16 `length`, 0x0A s16 `jitterPeriod`**; 0x0C..0x0F s8; 0x10 u32 `flags`; 0x14 `keys` (escrito com ponteiro na carga); 0x18 s32 `fade`; **0x1C s16 `jitterX`, 0x1E s16 `jitterY`** |
| `keysOfs` | `EftSprKey` x soma de `keyCount` | | por chave (0x40): 0x00..0x2F f32 (pos, rot, escala Vec4); 0x30..0x33 u8 `color[4]`; 0x34 s32 `frame`; 0x38 s32 x2 |

`EftSprPack_Bind` (`src/battle/eft_ae.c:1087`) escreve em cada camada `keys = pacote + keysOfs + 0x40 * (soma dos
keyCount anteriores)`: as chaves das camadas são contíguas, na ordem das camadas.

### 7.10 F2.9 Tabela de posicionamento (subentrada 3 dos pacotes de técnica 3..5)

`BtlTechPlace` / `BtlTechPlaceTbl` (`include/battle/btl_tech_b.h:240`, `:245`): registros de 0x10 bytes
{f32 x, y, z, yaw em graus}, indexados `[cenário * 5 + n]`. Tudo f32. Tamanho em 01482 entrada 4: 0xB00 (176
registros). O número de cenários não está no arquivo (vem de `Battle_GetStage()`).

### 7.11 F2.10 Bloco de bits do personagem (pacote do personagem, sub 1, 0x40 bytes)

`BtlScene_TestCharPackBit` faz `GetPackEntry(sub1, 1)` e testa um bit da palavra apontada: na demo a palavra 1 vale
0, então a palavra testada é a própria palavra 0. 0x00 u32 bits (bit k-1 = efeito k ligado); 0x04 u32 deslocamento
(lido como deslocamento de pacote); 0x08..0x3F não determinado (nunca lido).

### 7.12 F2.11 Animação de câmera (pacote do personagem 8, sub 1..3)

`DemoCamAnim` / `DemoCamChannel` / `DemoCamKey` / `DemoCamPose` (`include/battle/btl_demo_cam.h:52`, `:33`, `:25`, `:41`).

| Desl. | Tipo | Larg. | Significado |
|---|---|---|---|
| 0x00 | 4 caracteres | 1 | assinatura ASCII, a mesma das câmeras do file[0] (conferido na única câmera da demo, 01482); ninguém lê: tratar como bytes (6.5) |
| 0x04 | s32 | 4 | `needsFixup` (não zero no arquivo; zerado pela carga) |
| 0x08 | u32 | 4 | `channels`: deslocamento em bytes relativo ao início da animação; vira ponteiro |
| 0x0C | s32 | 4 | `channelCount` |
| 0x10 | s32 | 4 | não determinado |
| 0x14 | f32 | 4 | `length` |
| 0x18 | u32 | 4 | `defaults`: deslocamento; vira ponteiro |
| 0x1C | u32 | 4 | `unk1C`: deslocamento ou 0; vira ponteiro; anulado se `version < 2` (na demo: 0) |
| 0x20 | s32 | 4 | `version` |
| `channels` | 0x10 x `channelCount` | 4 | s32 `index` (palavra da pose), u32 `keys` (deslocamento; vira ponteiro), s32 `keyCount`, s32 |
| `defaults` | `DemoCamPose` 0x20 | 4 | f32 x6 (pos, rot), s32 `frame`, f32 |
| `keys` de cada canal | 0x20 x `keyCount` | | 0x00 u32 `flags`, 0x04 f32 `time`, 0x08 f32 `value`, 0x0C..0x1F não determinado (nunca lido) |
| `unk1C` | | | não determinado (anulado na demo) |

`DemoCam_FixupAnim` (`src/battle/btl_demo_cam.c:31`) soma o endereço da animação a `channels`, `defaults`,
`unk1C` e a cada `keys`, IN-PLACE, e zera `needsFixup`. A conversão de ordem de bytes tem de acontecer antes.

### 7.13 Escritas no file[2] na carga

`Res_RelocateOffsets` (TexFile +0x10, TexEntry +0x38 e +0x3C), `EftSprPack_Bind` (camada +0x14),
`DemoCam_FixupAnim` (+0x04, +0x08, +0x18, +0x1C, canais +0x04). Os modelos no formato do file[0] que o file[2]
traz (7.4.1, 7.5.1) são ligados no próprio buffer por `EftObj_Create` → `BtlObj_Create`, com as mesmas escritas do
file[0] (seção 8: bit `bound`, TBP/CBP do fluxo VIF por `MdlTex_RebaseChain`, ponteiros de textura). Como esses
campos são lidos como deslocamento antes de virar ponteiro, a conversão tem de rodar uma vez, antes de qualquer
ligação, e nunca sobre um buffer já ligado.

### 7.14 F2.12 Malha de efeito (`EftMeshData`; sub 1 do anel do tipo de efeito 8)

Structs `EftMeshData` / `EftMeshGroup` / `EftMeshTri` / `EftMeshVtx` (`include/battle/eft_ad.h:248`, `:230`,
`:237`, `:242`). `EftMesh_Init` (`src/battle/eft_ad_b.c:160`) guarda ponteiros para os grupos, os triângulos e os
vértices **na instância** (`EftMesh`), sem escrever no arquivo; `EftMesh_DrawClip`/`DrawPlain`/`DrawNow*`
(`eft_ad_b.c:291` em diante) leem. O mesmo formato serve a `EftDisc` (`src/battle/eft_o_c.c:313`) e ao cenário
(`src/battle/eft_b.c:1382`), fora do slot do lutador.

| Desl. | Tipo | Larg. | Significado |
|---|---|---|---|
| 0x00 | 4 caracteres | 1 | `unk0`: assinatura ASCII de 4 letras (a mesma nos dois anéis do disco); ninguém lê: bytes |
| 0x04 | s32 | 4 | `numGroups` |
| 0x08 | s32 | 4 | `trisOfs`: deslocamento em bytes dos triângulos, a partir do início da malha (= 0x10 + 0x10 x numGroups) |
| 0x0C | s32 | 4 | `vtxOfs`: deslocamento em bytes dos vértices |
| 0x10 | `EftMeshGroup` x numGroups | 4 | s32 `numTris`, s32 `unk4` (sem leitor), s32 `firstTri`, s32 `unkC` (sem leitor) |
| `trisOfs` | `EftMeshTri` (0x10) | 4 | s32 `v[3]` (índices de vértice), s32 `tex` (< 0 = sem textura) |
| `vtxOfs` | `EftMeshVtx` (0x30) | 4 | f32 x4 `pos`, f32 x4 `color`, f32 x4 `uv` |

Delimitação: triângulos = maior `firstTri + numTris` dos grupos; vértices = maior índice + 1 (o arquivo não diz
a contagem). Nos dois anéis do disco (01922 e 02582): 1 grupo, 96 triângulos que enchem exatamente
`[trisOfs, vtxOfs)`, 192 vértices, e zeros depois até o fim da entrada. Tudo palavra de 4 bytes, salvo a
assinatura.

## 8. O que o jogo grava nos arquivos, e quando converter

O jogo usa os três arquivos no próprio buffer e escreve neles na ligação. Todos esses campos são lidos
como número (deslocamento, contagem, palavra de comando) antes de serem sobrescritos, então a conversão
de ordem de bytes tem de rodar **uma vez, logo depois da leitura do arquivo e antes de qualquer
ligação**, e nunca sobre um buffer já ligado:

| Arquivo | Quem escreve | O quê |
|---|---|---|
| file[0] entrada 3 | `BtlObjMdl_BindFiles` (gfxm_e_b.c:166) | bit 31 de flags, alphaRow, alpha[16], tbp..fadeCbp (cabeçalho 0x28..0x3B e 0x58..0x68) |
| file[0] entradas 3 e 4..11 | `MdlTex_RebaseChain` (gfxm_e_c.c:25) | TBP0/CBP nas palavras 5, 6, 9 e 10 de cada lote VIF |
| file[0] entradas 1, 12, 13, 14..17 | `Res_RelocateOffsets` (gfxm_b_b.c:244) | `TexFile.ent` (+0x10), `TexEntry.pix`/`clut` (+0x38, +0x3C) |
| file[0] entradas 12, 13, 14..17 | `BtlObjMdl_InitAlphaSlots`, `BtlObjMdl_CopyTexAlpha` | bytes de alfa da paleta (bytes, sem efeito na ordem) |
| file[0] entrada 2 | `BtlObjBody_Init`, `BtlObjBody_UpdateParts` | ponteiros bone/part e as áreas de trabalho de cada parte |
| file[0] entradas 29..36 | `BtlObjAnim_Retarget` | chaves alteradas no lugar (floats) |
| file[0] entradas 37..43 e file[2] entrada 6 sub 8 | `DemoCam_FixupAnim` (btl_demo_cam.c:31) | deslocamentos viram ponteiros e `needsFixup` zera, na primeira vez que a câmera toca |
| file[2] | `Res_RelocateOffsets`, `EftSprPack_Bind` | ponteiros de textura e de chaves de sprite |
| file[2], modelos (sub 1 de técnica, subs 3 e 4 do pacote do personagem; 7.4.1, 7.5.1) | `EftObj_Create` → `BtlObj_Create` → `BtlObjMdl_BindFiles`, `MdlTex_RebaseChain`, `Res_RelocateOffsets` | as mesmas escritas do file[0] entradas 3 e 12, no buffer do file[2], quando o efeito cria o objeto |

Pontos que a conversão in-place sozinha não resolve:

1. **Animações comprimidas (file[1]).** O contêiner BPE só tem dois números (rawSize, packSize); o fluxo
   comprimido é todo byte e os números das animações só existem depois de `Bpe_Decode`. Os caminhos
   possíveis estão em 6.2; a escolha não é tomada aqui.
2. **Visão do byte alto de flags do modelo** (`BObjModelBytes.flagsTop`, cabeçalho + 0x0F): é correção de
   código (classe 4 do guia), não do conversor.
3. **Valores de 64 bits lidos como dois u32.** O atributo dos eventos e o quaternion empacotado das
   animações, e as etiquetas GIF e valores A+D dos pacotes VIF de malha, são lidos pelo código como duas
   palavras de 32 bits: convertem-se como dois u32. Já `TexEntry.tex0` e `ObjSeamTri.tex0Base/tex0Shade`
   são lidos como u64 inteiros: convertem-se como u64.
4. **Pacotes de imagem e paleta das texturas.** Imagem e paleta ficam intactas. Os cabeçalhos de 0x60
   bytes e o fim de 0x20 bytes não são lidos por código C; convertê-los (VIF u32, GIF e A+D u64) ou
   deixá-los para a F3 decodificar é escolha em aberto.

## 9. Não determinado

file[0], gráficos:
- Cabeçalho do modelo: 0x00..0x0B, 0x3C..0x4B, 0x55..0x57 e 0x74..0x8F nunca são lidos.
- Registro de malha + 0x0E (u16 sem leitor, pela visão `ObjDrawPart`).
- Seção de costura: não existe nos dois modelos da demo (existe em 73 file[0] do disco, 4.2.2: layout
  conferido nos dados, mas nenhuma leitura no rastreio); +0x40..0x4F de cada vértice não é lido (zero nos
  dados).
- Os 0x40 bytes depois do último registro de malha nos 625 modelos sem costura: não nulos entre +0x08 e
  +0x3F, sem leitor; conteúdo não determinado (tratado como bytes).
- Corpo: cabeçalho 0x00 (s32 sem leitor), 0x08..0x0F, e 0x034..0x03F de cada parte.
- `TexFile` 0x08..0x0F (não zero nos dados, nunca lido; provavelmente 2 x u32).
- Quem usa a entrada 1 (textura em objeto + 0x50) além da relocação.
- Se os cabeçalhos e fins dos pacotes de imagem são convertidos ou deixados para a F3 (8, item 4).

file[0], jogo (seção 5):
- Entrada 0x1A: vazia nos dois arquivos e sem ligação; formato desconhecido.
- Entrada 0x2C: sem leitor; formato inferido (u16), não provado pelo código.
- Bytes sem leitor tratados como bytes: `BtlAtkRecord` 0x2D..0x2F, `BtlKiBlastData` 0x32..0x33,
  `BtlSkillData` 0x72..0x73, `talkTbl` +3, IA 0x1F0..0x1FF. Se algum for número, o rastreio da demo não viu.
- Quantos registros de `BtlAtkRecord` o jogo usa de fato (o código limita a 0xA3; o arquivo tem 177).

Animações (seção 6):
- Os 4 bytes finais de cada volume de golpe (passo 0x20).
- `DemoCamKey` 0x0C..0x1F, `DemoCamAnim` 0x10 e o bloco `unk1C` (só existe com version >= 2; version é 0
  em todos os casos vistos). Nos dados são palavras de 4 bytes com cara de f32 ou zero.
- Os 4 últimos bytes da assinatura das faixas de lábio (constantes, não lidos).
- Quem lê `mdl.unk8D0` (segunda cópia das animações do modelo, objeto + 0x8E8).
- O significado do byte constante unk00 do cabeçalho de animação.

file[2] (seção 7):
- Subentrada 2 dos pacotes de técnica (vazia nos 805 do disco) e subentradas 5 e 10 do pacote do
  personagem: nenhum leitor com índice constante; vazias na demo.
- Subentrada 4 do pacote do personagem em 01552: traz {modelo, texturas} sem rajada de tipo 3; pelo código
  só seria lida como conjunto de emissores (7.5.1). Se o jogo chega a ler os bytes 0x04..0x07 e 0x20..0x27
  desse "cabeçalho": não determinado.
- Tipo de grupo 9, entrada a, de 0x243 até o fim: sem leitor; 196 das 1.250 partes do disco têm palavras com
  cara de f32 em 0x244..0x258 (7.8). Largura não determinada.
- Tipo de grupo 12: ausente do disco; layout só pelo código (7.8).
- `EftMeshGroup` +0x04 e +0x0C e a assinatura de `EftMeshData`: sem leitor (7.14).
- Tipo 16, bytes 0x08..0x0B (f32 numa visão, bytes na outra; sem leitura no rastreio).
- Bytes sem leitura: `EftSetGroupDef` 0x03..0x1F, `EftEmitDef` 0x38..0x3F, cabeçalho de emissores 0x07,
  "V000" 0x06..0x07 e 0x0C..0x1F, bloco de bits 0x08..0x3F.
- Tipos de grupo 10 e 15 não aparecem nos dois arquivos da demo (só no disco: 83 e 37 partes) e o 12 em
  arquivo nenhum; os tipos que o despachante não trata (1, 3, 4, 6, 7, 8, 11, 13) não aparecem em máscara
  nenhuma do disco.

Também fica fora deste documento: a relação entre o id do arquivo e o número no AFS, e os erros de visão
nos headers da decomp que a parte de efeitos encontrou (`EftSetDef` 0x0C..0x0F e 0x20..0x33 são f32;
`EftEmitHdr.unk4` são quatro u8; `EftQuadDef.unk224` são f32). O código não foi alterado.

## 10. Cobertura conferida contra o rastreio

Método: no WSL, cada arquivo foi percorrido como o jogo percorre (tabelas, lista de malhas, pacotes VIF,
texturas por `entOfs`, partes do corpo, registros de jogo, animações decodificadas, pacotes de efeito na
ordem de `EftEmit_LoadSet`), cada posição ganhou a largura desta especificação, e cada posição de
`tipos.tsv` e de `mapa.tsv` (expandida pelas repetições) foi comparada com ela. Leituras de 16 bytes
(vetores) contam como compatíveis com f32.

### 10.1 01473 (file[0] do lado 0), por entrada

Posições distintas em `tipos.tsv` (T) e posições de número em `mapa.tsv` (M):

| Entrada | T | M (largura: quantidade) | Largura igual à da especificação |
|---|---|---|---|
| tabela | 147 | 4: 147 | todas |
| 1 | 6 | 4: 6 | todas |
| 2 | 305 | 4: 859 | todas |
| 3 | 87.066 | 4: 86.861; 2: 264 | todas, exceto 1 leitura de byte em +0x0F (a visão `flagsTop`, item 2 da seção 8). É o conflito 22220/22223 de `mapa.tsv.conflitos`: por causa dele o mapa deixa a palavra de flags sem inverter; pela especificação ela é u32, e a leitura de byte é que precisa de correção no código |
| 4 | 6.437 | 4: 6.437 | todas (fluxo VIF) |
| 5 | 6.493 | 4: 6.493 | todas |
| 6 | 6.493 | 4: 6.493 | todas |
| 7 | 6.625 | 4: 6.625 | todas |
| 8 | 6.481 | 4: 6.481 | todas |
| 12 | 128 | 4: 159 | todas |
| 13 | 9 | 4: 10 | todas |
| 18 (0x12) | 32 | 2: 3; 4: 14 | todas |
| 19 (0x13) | 2 | 4: 1 | todas |
| 20 (0x14) | 41 | 2: 2; 4: 23 | todas |
| 21 (0x15) | 23 | 4: 23 | todas |
| 23 (0x17) | 199 | 2: 20; 4: 90 | todas |
| 24 (0x18) | 48 | 2: 8; 4: 68 | todas |
| 25 (0x19) | 12 | 2: 4; 4: 10 | todas |
| 27 (0x1B) | 35 | nenhuma | todas (só bytes) |
| 28 (0x1C) | 13 | 2: 2; 4: 7 | todas |
| 210..213, 231..233, 242..243 (lábio) | 36 | 2: 36 | todas |
| demais | 0 | 0 | sem leitura na demo |

Na entrada 3: 46 registros habilitados e 20 desabilitados, 679 lotes VIF; quase todas as posições são
palavras dos fluxos, lidas como u32 por `MdlTex_RebaseChain` nas suas duas passadas (171.872 linhas de
`tipos.tsv`), e as 264 de 2 bytes são os campos u16 dos registros (+0x04..+0x0A, 66 registros). Na entrada 2: as leituras caem em
flags, node, offset, raio e nos cantos `local[8]` da caixa de cada uma das 22 partes.

### 10.2 Outros arquivos

- **01793** (file[0] do lado 1): o mesmo resultado; entrada 3 com 66.651 posições em T e 66.716 em M, todas
  iguais, exceto a mesma leitura de byte em +0x0F; entradas 4..7, 12, 13, 18..28 e as faixas de lábio
  206, 207, 210, 211, 231..233, 240, 242 todas iguais. Tabela por entrada das seções de jogo:

  | Entrada | 01793 T | 01793 M |
  |---|---|---|
  | 0x12 | 33 | 20 |
  | 0x13 | 2 | 1 |
  | 0x14 | 43 | 27 |
  | 0x15 | 17 | 17 |
  | 0x17 | 103 | 55 |
  | 0x18 | 76 | 72 |
  | 0x19 | 15 | 15 |
  | 0x1B | 47 | 0 |
  | 0x1C | 25 | 18 |

- **01481 e 01801** (file[1]): no arquivo em si, 415 palavras da tabela (u32) e, em cada entrada
  decodificada, rawSize e packSize (u32) e o fluxo lido byte a byte por `Bpe_Decode`. Nas animações
  decodificadas (pseudoarquivos `01481.bin@0x...#bpe`), as contagens por parte estão em 6.3: zero
  divergências de largura em `tipos.tsv` e `mapa.tsv`, nenhuma leitura fora das partes descritas. O
  conflito 737884/737890 de 01801 é artefato de pilha do rastreio (6.3.1).
- **01482 e 01802** (file[2]): todas as posições batem com a largura da especificação, depois de corrigir
  a visão `EftQuadDef` (0x224..0x233 são f32, não bytes: eram 8 posições em 01482 e 16 em 01802). Os seis
  conflitos de `mapa.tsv.conflitos` caem em `EftUPtclDef` + 0x19C (quatro f32 lidos de uma vez por
  `Port_FastMtxApply`). Posições de `mapa.tsv` por entrada do topo:

  | Entrada | 01482 | 01802 |
  |---|---|---|
  | tabela do topo | 4: 7 | 4: 7 |
  | 1 | nenhuma | 4: 49; 8: 9 |
  | 2 | 4: 39; 8: 7 | 4: 43; 8: 8 |
  | 3 | 4: 95; 8: 18 | 2: 68; 4: 3.461; 8: 26 |
  | 4 | 4: 21 | 2: 50; 4: 2.745; 8: 15 |
  | 5 | 4: 142; 8: 23 | 4: 58; 8: 8 |
  | 6 | 2: 42; 4: 3.796; 8: 26 | 2: 42; 4: 2.677; 8: 26 |

  Por papel, 01482 (`mapa.tsv`): tabelas 254, cabeçalhos de emissores 214, texturas 310 (u32) + 74 (u64
  `tex0`), tipo 5 852 + 30 (s16) + 345 (b), tipo 9 184 + 98, tipo 14 333 + 11 (s16), tipo 16 138 + 121,
  tipo 17 7 + 11, tipo 18 40 + 23, "V000" 42 + 1 (s16), câmera 1.114, posicionamento 12, bits 2. 01802:
  tabelas 289, emissores 557, texturas 594 + 92, tipo 5 3.302 + 114 + 1.311, tipo 9 286 + 144, tipo 14
  1.722 + 45, tipo 16 236 + 198, tipo 17 14 + 22, tipo 18 206 + 115, "V000" 42 + 1, bits 2.

### 10.3 O que a demo não exercita

- file[0]: entradas 9..11, 14..17, 26, 29..35, 37..40 vazias; 22, 36, 41..52 e as faixas de lábio da
  primeira língua (53..152) presentes e não lidas; a seção de costura não existe; nenhuma forma de boca foi
  desenhada no lugar do nó 0x30 (as cadeias 4..8 só foram lidas pelo rebase de texturas); imagem e paleta
  das texturas nunca passam pela instrumentação (vão por DMA).
- file[1]: 346 de 414 movimentos em 01481 (331 em 01801) não foram decodificados; as entradas 415..440 e
  todos os "BPE vazio".
- file[2]: em 01482, a entrada 1 inteira e todas as partes das entradas 2, 3 e 5; em 01802, todas as
  partes das entradas 1, 2 e 5 e a câmera (sub 8 vazia); nos dois, no pacote do personagem, 9 partes de
  tipo 5, 5 de tipo 14, 3 de tipo 16, 2 de tipo 18 e 1 de tipo 9 (com suas entradas b), e as subentradas
  5, 9, 10 e 11 (vazias); os tipos 2 (raios) e 17 só existem em partes não lidas de 01482.

### 10.4 O disco inteiro (`conv_tool --disco`)

Os 1.610 arquivos de slot dos 161 lutadores (file[0], file[1] e file[2]) convertidos sem gravar, no WSL.
Com o conversor de `port/x360/conv/` antes desta revisão: 1.412 sem erro e 198 com erro, todos de formato
não especificado (o conversor para no primeiro erro de cada arquivo):

| Erro | Arquivos | Causa | Seção |
|---|---|---|---|
| "convertido duas vezes (marca 1, agora 4) [entrada 3]" | 73 file[0] | seção de costura logo depois do último registro de malha, já marcada como bytes | 4.2, 4.2.2 |
| "tipo de grupo 10 não especificado" | 38 file[2] | layout do tipo 10 | 7.8 |
| "tipo de grupo 15 não especificado" | 15 file[2] | layout do tipo 15 | 7.8 |
| "parte de efeito: byte N não reconhecido" | 60 file[2] | entrada a do tipo 9 maior que a struct | 7.8 |
| "técnica: subentrada 1 não suportada" | 12 file[2] | variantes da sub 1 | 7.4.1 |

Escondidos atrás desses (aparecem só depois de resolvê-los): a sub 3 de 01552, 01982 e 02032 e a sub 4 de
01552 e 01982 do pacote do personagem, que são modelos (7.5.1).

Teste da especificação: uma cópia do conversor no /tmp do WSL, com as regras desta revisão (costura depois do
percurso das malhas, tipos 10, 12 e 15, tipo 9 com o fim sem leitor em bytes, sub 1 de técnica e subs 3 e 4
do pacote do personagem pelo critério da forma, malha F2.12). Resultado:

- `--disco`: 1.610 sem erro, 0 com erro; os 198 resolvidos, nenhum dos 1.412 que passavam passou a falhar;
  cobertura de 100% dos bytes em todos os 1.610;
- `--mapa port/build/mapa.tsv` nos seis arquivos da demo (01473, 01481, 01482, 01793, 01801, 01802): todas as
  posições iguais (121.146, 551, 4.216, 100.922, 581 e 9.292), 0 com outra largura, 0 não trocadas.

O que esse teste não prova: nenhuma posição das partes novas (costura, tipos 10 e 15, sub 1 de técnica,
modelos do pacote do personagem, malha) foi lida pela luta de demonstração; a largura vem só do código e da
forma dos dados.
