# F5: inventário das cores e dos personagens da luta

[Escopo da F5](00-escopo.md) | [Tarefa](../../tarefas/f5-inventario.md) | decisão 24

Leitura de 16/10/2026, base `8a881b6263d5bf867fa7d3cc9c1168a51794473f`, branch
`codex/f5-inventario`. Só documentos, fontes e dados originais do usuário, com scripts de leitura de até poucos
segundos. Nenhuma build, execução do jogo, emulador, depurador, captura ou conferência oficial nesta tarefa.
Os números de linha abaixo são dessa base; as propostas de correção não foram implementadas.

**Resultados:** 148 atribuições a bytes de `rgba[4]` e mais 244 a campos equivalentes `r/g/b/a` de pacotes
precisam de correção. Os 392 pontos são sintáticos, incluindo corpos que o port habilita a partir de `#if 0` e
uma definição de macro, não uma contagem de primitivas em execução. Goku (Early), id 0, não tem transformação
nem fusão. Raditz, id 77, e Nappa, id 80, têm transformação condicionada à lua do cenário. Portanto, os dados
não permitem fechar um adversário sem transformação entre os dois candidatos da decisão 24. A pergunta ao Lucas
está na seção 2.4; nenhum personagem, traje ou estágio novo foi escolhido aqui.

## 1. Cores dos efeitos e do desenho

### 1.1 Contrato e propostas de correção

O [contrato da GPU](../../formatos/gpu.md), seções 1, 2 e 6, exige palavras de 32 bits na ordem do host e, em
RGBAQ, R no byte de menor peso da primeira palavra. No big-endian, a memória dessa palavra deve ser A, B, G, R.
Q continua sendo um `f32` independente, na palavra seguinte. A inversão é **dentro de quatro bytes**, nunca dos
oito bytes de RGBAQ. Campos XYZF2 e as duas metades de valores `u64` continuam com as regras já existentes.

Classificação usada nas tabelas:

- **CPU:** componente semântico em vetor, parâmetro, estado ou vértice de recorte. A CPU ainda lê R por índice 0
  ou pelo nome `r`, antes de empacotar. Não inverter a origem.
- **Pacote:** representação já destinada ao GIF/GS, mesmo quando ainda está numa variável local da pilha e será
  copiada. Precisa respeitar o contrato antes da cópia.
- **Cópia de pacote:** origem e destino já no contrato de pacote. Preservar os bytes, sem segunda conversão.
- **Imagem:** texels/CLUT enviados por IMAGE, que conservam os bytes do PS2. É uma exceção distinta de RGBAQ,
  explicitada para não aplicar a correção de pacotes a uma paleta.

**Proposta A, em cada gravação de vetor de pacote:** trocar apenas o índice do destino no ramo big-endian,
`0 -> 3`, `1 -> 2`, `2 -> 1`, `3 -> 0`. Manter expressão, conversão numérica e ordem original das atribuições.
Exemplo concreto de `src/battle/eft_g.c:415..418`:

```c
#ifdef PORT_BIG_ENDIAN
    p->v[0].rgba[3] = (u32)col0[0];
    p->v[0].rgba[2] = (u32)col0[1];
    p->v[0].rgba[1] = (u32)col0[2];
    p->v[0].rgba[0] = (u32)col0[3];
#else
    p->v[0].rgba[0] = (u32)col0[0];
    p->v[0].rgba[1] = (u32)col0[1];
    p->v[0].rgba[2] = (u32)col0[2];
    p->v[0].rgba[3] = (u32)col0[3];
#endif
```

O mesmo padrão se aplica aos demais vértices. No `EFT_STORM_SPRITE`, o `#ifdef` deve envolver duas definições
completas da macro: a big-endian com os destinos permutados e, no `#else`, a definição original inteira.
Uma diretiva no meio da continuação da macro não serve. Não criar uma macro que altere as linhas originais fora
do condicional. A declaração duplicada de `EftSteamPkt` em `eft_d_b.c` não contém gravações adicionais.

**Proposta B, campos com nome:** permutar a declaração do quarteto das 11 structs da seção 1.3, como já se faz
com `EftTriVtx`, `EftNVtx` e `EftSprPkt`. Todas as atribuições listadas passam a escrever na posição certa sem
mudar a leitura das cores da CPU. O original fica integralmente no `#else`, inclusive quando declara cada canal
numa linha (`EftYRgbaq`):

```c
#ifdef PORT_BIG_ENDIAN
    u8 a, b, g, r;
#else
    u8 r, g, b, a;
#endif
    f32 q;
```

Registrar as 11 permutações em `layout.PERMUTADAS` na implementação, com motivo e arquivo, preservando tamanho,
alinhamento e deslocamento de Q. Não aplicar B a `EftOColor`, `EftRay`, HUD ou cores de trabalho. Não combinar A
e B sobre o mesmo quarteto. Cópias mantêm o original. Gravações numéricas já protegidas por `PS2_PKT64` também.

### 1.2 Inventário A: todos os destinos `rgba[4]` de pacote

Cada intervalo de quatro linhas significa, em ordem, as gravações de R, G, B e A nos índices 0, 1, 2 e 3.
Os intervalos separados por ponto e vírgula correspondem aos vértices 0, 1, 2 e 3 indicados. Assim cada
atribuição tem uma linha identificável; as contagens não incluem Q, ST nem XYZF2. Todos os destinos desta tabela
são **pacote**, recebem A; as fontes mencionadas são **CPU** e mantêm sua ordem.

| Arquivo, função e linhas das gravações | Struct de destino, campo | Fonte e quantidade |
|---|---|---|
| `src/battle/eft_g.c`, `EftUtil_DrawTri`, 415..418; 420..423; 425..428 | `EftTriPkt.v[0..2]`, `EftGsVtx.rgba` | `col0/col1/col2`, vetores de float; 12 |
| `src/battle/eft_g.c`, macro `EFT_STORM_SPRITE`, 526..529, usada em `EftStorm_DrawBolts:611,613,614` | `EftStripPkt.rgba` | brilho `col` e vida da descarga; 4 na definição |
| `src/battle/eft_g.c`, `EftStorm_DrawRainLines`, **742,743,744,739** na ordem RGBA | `EftLinePkt.rgba` | constantes RGB e alfa; 4 |
| `src/battle/eft_e.c`, `EftSteam_DrawQuad`, 547..550 | `EftSteamPkt.rgba` | `color` e `tint`; 4 |
| `src/battle/eft_e.c`, `EftWater_DrawBillboard`, 2645..2648 | `EftWaterSpritePkt.rgba` | argumentos `r/g/b/a`; 4 |
| `src/battle/eft_e.c`, `EftWater_DrawSprayQuad`, 2811..2814; 2816..2819; 2821..2824; 2826..2829 | `EftWaterQuadPkt.v[0..3]`, `EftWaterGsVtx.rgba` | argumentos `r/g/b/a`; 16 |
| `src/battle/eft_e.c`, `EftWater_QueueTri`, 2928..2931; 2933..2936; 2938..2941 | `EftWaterTriPkt.v[0..2]`, `EftWaterGsVtx.rgba` | `c0/c1/c2`; 12 |
| `src/battle/eft_ae.c`, `EftSprAnim_QueueTri`, 904..907; 909..912; 914..917 | `EftAeTriPkt.v[0..2]`, `EftAeGsVtx.rgba` | `c0/c1/c2`; 12 |
| `src/battle/eft_p_b.c`, `EftGlow_DrawParts`, 1071..1074; 1076..1079; 1081..1084; 1086..1089 | `EftPQuadPkt.v[0..3]`, `EftPGsVtx.rgba` | `col[0..2]` local e `a`; 16 |
| `src/battle/eft_s.c`, `EftChain_DrawStrand`, 903..906; 908..911; 913..916; 918..921 | `EftArcPkt.v[0..3]`, `EftSGsVtx.rgba` | `colA` nos dois primeiros, `colB` nos outros; 16 |
| `src/battle/eft_s.c`, `EftRay_DrawQuad2D`, 2451..2454; 2456..2459; 2461..2464; 2466..2469 | `EftTQuadPkt.v[0..3]`, `EftTGsVtx.rgba` | `r0/g0/b0/aCur`, `r1/g1/b1/aNext`; 16 |
| `src/battle/eft_t_b.c`, `EftStreak_DrawScreen`, 641..644; 646..649; 651..654; 656..659 | `EftTQuadPkt.v[0..3]`, `EftTGsVtx.rgba` | `col.x/y/z/w`; 16 |
| `src/battle/eft_t_c.c`, `EftShotFx_DrawQuad`, 571..574; 576..579; 581..584; 586..589 | `EftUQuadPkt.v[0..3]`, `EftUGsVtx.rgba` | argumentos `r/g/b/a`; 16 |
| **Total** | 13 grupos, 11 tipos de quarteto | **148 atribuições** |

Declarações, para repetir a busca por tipo e não só pelo nome do campo:

| Tipo | Declaração |
|---|---|
| `EftGsVtx`, `EftStripPkt`, `EftLinePkt` | `src/battle/eft_g.c:169,192,212` |
| `EftSteamPkt` | `src/battle/eft_e.c:197`, duplicada em `src/battle/eft_d_b.c:1265` (sem usuário nesse arquivo) |
| `EftWaterGsVtx`, `EftWaterSpritePkt` | `src/battle/eft_e.c:1375,1400` |
| `EftAeGsVtx` | `src/battle/eft_ae.c:71` |
| `EftPGsVtx` | `src/battle/eft_p_b.c:86` |
| `EftSGsVtx` | `src/battle/eft_s.c:770` |
| `EftTGsVtx` | `include/battle/eft_t.h:235` |
| `EftUGsVtx` | `src/battle/eft_t_c.c:151` |

O comentário de `EftLinePkt` diz PACKED, mas o valor de `gif0` em `eft_g.c:737`, `0x4400000000008001`,
seleciona REGLIST (FLG = 1). A classificação acima segue a etiqueta e os campos efetivamente emitidos.

### 1.3 Inventário B: campos equivalentes com nome ainda sem permutação

Todos são **pacote**, inclusive `pkt` local de `EftMesh_SendTri`. A correção B se faz na declaração de cada
quarteto, com o original no `#else`; a última coluna enumera **todas** as gravações beneficiadas. Como na tabela A,
cada intervalo de quatro linhas é R, G, B, A. Onde o alfa está afastado, as quatro linhas estão explícitas.

| Quarteto e declaração que recebe B | Pacote e função que grava | Arquivo e linhas de cada quarteto RGBA |
|---|---|---|
| `EftGfxRgbaq`, `src/battle/eft_a.c:1721` | `EftGfxStripPkt.rgbaq0..3`, `EftGfx_DrawSprite` | `src/battle/eft_a.c:1865..1868;1870..1873;1875..1878;1880..1883` (16) |
| `EftBRgbaq`, `src/battle/eft_b.c:202` | `EftBQuadPkt.rgbaq`, `EftPrim_DrawBillboard` | `src/battle/eft_b.c:384,385,386,405` (4) |
| idem | `EftBTriPkt.rgbaq0..2`, `EftPrim_DrawTriangle` | `src/battle/eft_b.c:556..559;561..564;566..569` (12) |
| idem | `EftBQuadPkt.rgbaq`, `EftBubble_DrawOne` | `src/battle/eft_b.c:1183..1186` (4) |
| idem | `EftBStripPkt.rgbaq0..3`, `EftGeyser_DrawColumn` | `src/battle/eft_b.c:1703..1706;1708..1711;1713..1716;1718..1721` (16) |
| `EftSurfRgbaq`, `src/battle/eft_c.c:1699` | `EftSurfTriPkt.rgbaq0..2`, `EftSurf_QueueTri` | `src/battle/eft_c.c:1756..1759;1761..1764;1766..1769` (12) |
| `EftAdRgbaq`, `src/battle/eft_ad_b.c:50` | `EftMeshTexTriPkt.v[0..2].rgbaq`, ramo com textura de `EftMesh_QueueTri` | `src/battle/eft_ad_b.c:881..884;886..889;891..894` (12) |
| idem | `EftMeshTriPkt.v[0..2].rgbaq`, ramo sem textura de `EftMesh_QueueTri` | `src/battle/eft_ad_b.c:941..944;946..949;951..954` (12) |
| idem | `EftMeshTexTriPkt.v[0..2].rgbaq`, `EftMesh_SendTri`, pacote local | `src/battle/eft_ad_b.c:1007..1010;1012..1015;1017..1020` (12) |
| `EftQRgbaq`, `include/battle/eft_q.h:287` | `EftQSpritePkt.rgbaq`, `EftFlash_Draw` | `src/battle/eft_q.c:1176..1179` (4) |
| idem | `EftQQuadPkt.rgbaq`, `EftTrail_DrawSprite` | `src/battle/eft_q.c:1682..1685` (4) |
| `EftVRgbaq`, `include/battle/eft_v.h:92` | `EftVStripPkt.rgbaq0..3`, `EftPtcl_DrawAxisQuads` | `src/battle/eft_u_b.c:1018..1021;1023..1026;1028..1031;1033..1036` (16) |
| `EftWRgbaq`, `src/battle/eft_w.c:45` | `EftWStripPkt.v[0..3].rgbaq`, `EftLink_DrawQuad` | `src/battle/eft_w.c:160..163;165..168;170..173;175..178` (16) |
| idem | `EftWQuadPkt.rgbaq`, `EftLink_DrawBillboard` | `src/battle/eft_w.c:861,862,863,881` (4) |
| `EftXRgbaq`, `src/battle/eft_x.c:50` | `EftXStripPkt.v[0..3].rgbaq`, `EftPart10_DrawQuad` | `src/battle/eft_x.c:466..469;471..474;476..479;481..484` (16) |
| idem | `EftXQuadPkt.rgbaq`, `EftPart10_DrawBillboard` | `src/battle/eft_x.c:715,716,717,735` (4) |
| `EftYRgbaq`, `include/battle/eft_y.h:390` | `EftYStripPkt.v[0..3].rgbaq`, `EftQuad_DrawSprite` | `src/battle/eft_y.c:640..643;645..648;650..653;655..658` (16) |
| idem | `EftYStripPkt.v[0..3].rgbaq`, `EftLine_DrawSprite` | `src/battle/eft_z.c:266..269;271..274;276..279;281..284` (16) |
| `EftAaRgbaq`, `include/battle/eft_aa.h:60` | `EftGroundQuadPkt.v[0..3].rgbaq`, `EftGndDust_DrawQuad` | `src/battle/eft_aa.c:505..508;510..513;515..518;520..523` (16) |
| idem | `EftBladeTriPkt.v[0..2].rgbaq`, `EftBlade_Draw` | `src/battle/eft_aa.c:1140..1143;1145..1148;1150..1153` (12) |
| idem | `EftBlindPkt.rgbaq`, `EftBlind_DrawOverlay` | `src/battle/eft_aa.c:1367..1370` (4) |
| `EftAcRgbaq`, `include/battle/eft_ac.h:65` | `EftAcQuadPkt.v[0..3].rgbaq`, `EftZap_DrawQuad` | `src/battle/eft_ab_c.c:2258..2261;2263..2266;2268..2271;2273..2276` (16) |
| **Total** | **22 grupos, 11 tipos de quarteto** | **244 atribuições** |

Não descartar os corpos em `#if 0`: `port/tools/portsrc.py:20..42` habilita tentativas de C que têm
`INCLUDE_ASM`, incluindo `EftLine_DrawSprite` e `EftStorm_DrawBolts`. O inventário lê os fontes originais para não
depender dos arquivos gerados da rodada da F4. `gOtCurRead`, em `eft_u_b.c:915,979`, é um alias de `gOtCur`, não
um buffer só da CPU.

### 1.4 Pacotes já corrigidos, cópias e cores da CPU

| Classe | Arquivo, função e gravações | Struct, origem/destino e tratamento proposto |
|---|---|---|
| Pacote, corrigido por layout | `src/battle/eft_d.c:121..124;126..129;131..134`, macro `EFT_SURF_FILL_PKT`; `:378..381;383..386;388..391`, `EftSurf_DrawTriDirect` | `EftTriVtx` em `EftTriPkt` e `EftTriGs`; 24 atribuições. Layout permutado em `:58..66`. Manter os destinos com nome e as origens `Vec4`/`EftIVec` |
| Pacote, corrigido por layout | `src/battle/eft_n.c:371..374;376..379;381..384;386..389`, `EftAura_DrawFlames`; `:1542..1545;1547..1550;1552..1555;1557..1560`, `EftBolt_Draw` | `EftNVtx` em `EftNQuadPkt`; 32 atribuições. Layout em `:52..60`. Não permutar `rgb`, `al`, `col0`, `col1` da CPU |
| Pacote, corrigido por layout | `src/battle/eft_ad_c.c:130..133`, `EftSpr_DrawFlat`; `:275..278`, `EftSpr_DrawRot` | `EftSprPkt`; 8 atribuições. Layout em `:48..53`. Manter |
| Pacote local, corrigido por layout | `src/sys/gfxm_a.c:555..558`, `StgPanBlur_DrawView` | `GfxPostSpritePkt.rgbaq`; 4 atribuições, layout em `:65..73`. Mesmo sendo quatro constantes iguais, classificar como pacote |
| Cópia de pacote | `src/sys/gfxm_a.c:576`, `StgPanBlur_DrawView` | `memcpy(p, &pkt.gifTag, ...)`, pacote local já permutado para DMA. Preservar |
| Cópia de pacote | `src/battle/eft_ad_b.c:1041`, `EftMesh_SendTri` | `memcpy(dst, &pkt.gifTag, ...)`. Corrigir o `EftAdRgbaq` local pela proposta B, preservar a cópia |
| Cópia de pacote | `src/sys/gfx_ot.c:187`, `Ot_Send` | `memcpy(dst, prim + 1, prim->qwc << 4)`. Origem na tabela de ordenação, destino DMA. Corrigir os emissores, preservar a cópia |
| CPU para número de pacote | `src/battle/eft_d_b.c:653..656`, `EftBurst_DrawModel`; empacotamentos `:657..658,681..682,688..689,692`; cópias `:710,714,719,738,742,747` | `rgba` é `EftIVec` de inteiros, não `u8[4]`. `rgbaq[3][2]` local já contém `PS2_PKT64`; as seis cópias são de pacote. Manter |
| CPU | `src/sys/gfxm_c.c:936..939;970..973;975..977;980`, `FlashDraw_TransformQuad`; `:1428..1431`, `FlashShape_Draw` | `FlashQuad.rgba` é cor de CPU, empacotada por `FLASH_RGBA` em `:701` e `PS2_PKT64` em `:714,782`. Não aplicar A ao Flash |
| CPU | `src/battle/eft_n.c:351..355`; `src/battle/eft_p_b.c:1055..1057` | `rgb/al` locais e `col` de `EftGlow_DrawParts`; fontes dos pacotes, preservar |
| CPU | `src/battle/stg_b.c:356..359;363..366;371..374;378..380;390..391;403..405` | `BtlStage_GetAmbient`/`BtlStage_SetAmbient`: `rgba` e `col` são vetores de `s32`, copiados como pares de 64 bits, não RGBAQ; `c[0..2]` atualiza os bytes de ambiente |
| CPU | `src/battle/eft_ad_b.c:166..169;254..257` | `EftMesh.r/g/b/a` (`EftObjRes`), multiplicador lido no cálculo dos vértices; não confundir com `EftAdRgbaq` de pacote |
| CPU | `src/battle/eft_aa.c:156..172`; `src/battle/eft_z.c:1200..1209;1227..1234;1304..1305` | `EftGroundRgba`, `EftGndDustStage`, `EftGndDustArg/Part.colA/colB`, inclusive cópias `EftZCol`; entradas para interpolação, preservar |
| CPU | `src/battle/eft_c.c:335..338;706..708`; `src/battle/view_b.c:188..208`; `src/sys/color_fade.c:64..66;76..78` | `EftWeather.color`, `TextBoxDraw.color/shadow`, `ColorFade`; preservar a ordem semântica. O destino GPU é montado como número por seus desenhadores |
| CPU | `src/sys/gfxm_a.c:1853..1856;1881..1884` | `GfxPostQuad.r/g/b/a` são `s32`, usados para montar números, não o quarteto de `GfxPostSpritePkt` |
| CPU | `HudSprite` e as visões `HudBSprite`, `HudCSprite`, `HudDSprite`, `HudESprite`, por exemplo `src/battle/hud_c_c.c:74..77;84..87` | Estado do sprite, lido canal por canal para formar valores de pacote. Não permutar o layout de trabalho |

As 68 atribuições dos quatro primeiros grupos já têm o tratamento de layout. Junto das 392 pendentes, são
460 gravações escalares de cor em structs de pacote identificadas. As cópias não entram nessa soma. As origens
CPU da tabela não são uma nova lista de erros: delimitam por que uma substituição textual de todo `rgba[4]`
produziria erros. Cores `Vec4`/`EftYCol`, chaves `EftOColor`, `EftRay` e os argumentos dos emissores conservam sua
ordem; o limite de conversão é a gravação no pacote.

**Texels e CLUT não recebem A nem B.** São bytes IMAGE, conforme `gpu.md`, seções 1 e 4. Gravações encontradas
na busca ampliada, para exclusão explícita e repetível:

| Arquivo e linhas | Função e destino |
|---|---|
| `src/battle/eft_a.c:1081..1083,1091` | `EftGfx_LightClutDiffuse`, paleta `u8 *dst` |
| `src/battle/eft_a.c:1137..1140;1161..1164` | `EftGfx_LightClutSpecular` e `EftGfx_LerpClut`, paleta `u8 *dst` |
| `src/sys/gfxm_a.c:194..196,199,201` | `StgPanBlur_BuildClut`, entrada `e[0..3]` |
| `src/sys/gfxm_a.c:1149..1152;1159..1162` | `ObjOutline_BuildClut`, `clut[k+0..3]` |
| `src/sys/gfxm_a.c:1468..1471;1473..1476` | `StgGlare_BuildClut`, `clut[k+0..3]` |
| `src/sys/gfxm_a.c:1612..1615;1623,1625,1627,1628;1637..1640;1645..1648` | `StgDepthTint_BuildClut`, `clut[k+0..3]` |
| `src/sys/gfxm_a.c:1793..1796` | `ObjGlow_BuildClut`, entrada `e[0..3]` |
| `src/sys/gfxm_b.c:778..781;785..788;790..793;1156..1159` | `GfxAlphaKey_BuildClut` e `GfxDepthFog_BuildClut`, `clut` |
| `src/battle/stg_c.c:314..317` | `StgHaze_BuildClut`, `clut[k+0..3]` |
| `src/battle/stg_b.c:1587..1590;1593..1596` | `StgFog_BuildClut`, `clut` |
| `src/battle/eft_c.c:1915..1919,1926,1929,1932` | `EftSurf_RenderPalettes`, cópia/limpeza das paletas e chamadas aos iluminadores |

Os desenhadores que montam uma cor como número (`p[n] = PS2_PKT64(...)`, `Dma_AddRgbaq` etc.) já pertencem ao
contrato e ao inventário de `pacotes64.py`; não são gravações byte a byte pendentes. Exemplos: `fade.c:26..27,40`,
`gfxm_b.c:285..286`, `gfxm_d_c.c:485`. O backend lê o pacote numérico, e o formato de vértice do backend tem seu
próprio contrato (`gs_draw.h`, `gs_d3d.cpp`); não aplicar a permutação de pacote aos vértices já decodificados.

### 1.5 Busca repetível e requisito para o verificador futuro

`rg` não está instalado neste WSL; usei `grep`, leitura das declarações/usos e scripts Python pequenos sobre
texto. O conjunto foi `src/` e `include/`, com atenção a `src/battle`, `src/sys`, seus headers, `port/src/gs` e
`port/x360/src`. Não li árvores de dados por busca recursiva nem escrevi em diretórios de dados ou build.

Comandos de descoberta, a partir da raiz, independentes de build:

```sh
grep -RInE '\b(u8|unsigned char|uint8_t)[[:space:]]+[^;]*;' src include
grep -RInE 'rgba|rgbaq|rgb|color|colA|colB' src/battle src/sys include/battle include/sys
grep -RInE '(->|\.)[rgba][[:space:]]*=' src/battle src/sys
grep -RInE 'memcpy|memmove|gOtCur|Dma_BeginDirect|Dma_AddData' src/battle src/sys
grep -RInE 'rgba|rgbaq|uint8_t.*(col|rgb)' port/src/gs port/x360/src
```

O primeiro comando é deliberadamente mais amplo que nomes de cor: permite descobrir um novo quarteto com
outro nome. Para cada declaração de cor, busque todos os usos do tipo (incluindo structs que o contêm), siga a
alocação e a ligação a `gOtCur`, aliases ou `Dma_BeginDirect`, e classifique os escritores e as cópias. Os
quartetos com nome da seção 1.3 foram encontrados assim, não pela lista original de 11 vetores de `gpu.md`.
Não eliminar macros ou corpos em `#if 0`; não contar comentários, leituras e Q como gravação RGBA.

Este censo textual curto reproduz as contagens de A e B e lista as linhas, sem executar compilador ou jogo:

```sh
python3 - <<'PY'
from pathlib import Path
import re
padroes = {
    'A': re.compile(r'(?:->|\.)rgba\[[^]]+\]\s*=(?!=)'),
    'B': re.compile(r'rgbaq\d*\.[rgba]\s*=(?!=)'),
}
for classe, padrao in padroes.items():
    total = 0
    for arquivo in sorted(Path('src/battle').glob('*.c')):
        linhas = [n for n, texto in enumerate(arquivo.read_text().splitlines(), 1)
                  if padrao.search(texto) and not texto.lstrip().startswith(('/', '*'))]
        if linhas:
            print(classe, arquivo, len(linhas), ','.join(map(str, linhas)))
            total += len(linhas)
    print(classe, 'total', total)
PY
```

Saída na base: A = 148, B = 244. **Isso é ferramenta de levantamento, não verificador de cobertura:** alguém
pode adicionar um campo com outro nome, um cast ou uma gravação por ponteiro que não case com as expressões.
A busca ampla e a revisão por tipo complementam o censo. As 26 primitivas da [F4, etapa 6](../F4/06.md) são o
sintoma conhecido: 24 com textura em 0x2A20, duas em 0x2B00, cor `3c3c3c16` contra `163c3c3c`. A documentação
não liga essas linhas de dump a um emissor C específico. Nenhuma execução nova foi feita para atribuí-las;
não alegar que os 13 ou 22 grupos foram alcançados por aquele quadro.

Na etapa 4, o verificador deverá ter uma lista por **arquivo, função/macro, tipo do destino e campo**, expandindo
os intervalos deste inventário, e classificar todos os acessos de escrita, não só aceitar uma soma. Deve:

1. Descobrir quartetos de bytes nas structs e seguir seus tipos que os contêm até pacote, CPU ou IMAGE;
   recusar tipo/gravação/cópia nova sem classificação. Incluir inicializadores, casts, aliases, gravação composta,
   macros e cópias de struct ou memória. Usar o modelo do AST de `pacotes64.py` e as localizações de expansão.
2. Em A, conferir que o ramo big-endian grava o canal lógico `i` em `3-i` e o ramo original permanece no `#else`;
   em B, exigir a permutação de layout, Q em +4 e cobertura de todos os usos. Não inferir correção só pelo nome.
3. Conferir que CPU/IMAGE e cópias de pacote não recebem segunda troca, e que um pacote local é corrigido antes
   de copiar. Não depender exclusivamente do nome `Pkt` (há vértices e tipos locais com o mesmo nome em arquivos
   diferentes, por exemplo os dois `EftTriPkt`).
4. Ser visto falhando em cópias: retirar uma gravação do inventário mantendo o fonte, acrescentar escrita nova,
   retirar uma inversão de índice, retirar uma permutação de struct, inverter uma origem CPU/CLUT e trocar uma
   cópia de pacote duas vezes. Falha de análise ou ausência de fonte não pode virar "ok".

Esse verificador **não foi criado nem executado** nesta tarefa documental. Na implementação, além dele, continuam
necessários objetos x86 sem depuração iguais à base, memória viva sem exceção de cor, `IMG=luta` e composições do
Shenlong idênticas; as 26 primitivas não limitam o escopo da auditoria.

## 2. Ids, trajes e disponibilidade de transformação/fusão

### 2.1 Nome ligado ao id pelos dados

`Duel_Main` carrega o id global `0x1C1` e chama `CharSel_Run(2)` (`src/menu/menu_g_b.c:28,50`). Pela tradução de
`src/sys/file.c:437..440` e `include/sys/file.h:11`, o id global positivo i vira `pzs3us1/(i-1).bin`.
Assim, o arquivo é `gamedata/pzs3us1/00448.bin`, seção 2 comprimida em BPE. SHA1 do arquivo original:
`430371353266e0c1b33ccec10da29ee1e3ac0a47`, tamanho 3.693.312 bytes.

`CharSel_Init` liga `nameText` à seção 43 e `formText` à 44 do conteúdo descomprimido
(`src/menu/menu_c_e.c:549..550`). Os dois textos são indexados pelo **id do personagem** em
`TextBox_AttachLine` (`:903..905`); o leitor usa o deslocamento `text[line+1]`, alinhado a quatro, para uma
string u16 (`src/battle/view_b.c:244`). Não há inferência por posição do cursor.

Uma expansão BPE em memória, seguindo `src/sys/bpe.c`, devolveu 7.074.880 bytes e 56 seções. Cada tabela de
texto citada tem 168 entradas e 11.456 bytes. As seguintes entradas identificam os ids (BOM inicial omitido;
o marcador final não alfabético nos nomes 77 e 80 foi omitido da apresentação):

| Id decimal / hexadecimal | Linha da seção 43 | Linha da seção 44 | Conclusão |
|---|---|---|---|
| 0 / `0x00` | Goku (Early) | vazia | Goku escolhido pela decisão 24 |
| 77 / `0x4D` | Raditz | vazia | primeiro candidato |
| 78 / `0x4E` | Great Ape Raditz | vazia | alvo encontrado nos parâmetros do id 77 |
| 80 / `0x50` | Nappa | vazia | candidato de reserva |
| 81 / `0x51` | Great Ape Nappa | vazia | alvo encontrado nos parâmetros do id 80 |

### 2.2 Trajes e arquivos que realmente fornecem os parâmetros

O id global 4, `gamedata/pzs3us1/00003.bin`, é lido em `gCommonRes->data[2]` (`src/sys/common.c:24..25,36`).
SHA1 `54a813cede98b5498c3eec76f800d948424bec39`, tamanho 23.744. A seção 1 começa em `0x40`.
`ChrTblEntry` tem passo `0x3C` e `costumes` em `+0x0A` (`include/battle/view_b.h:93..105`);
`ChrTbl_WrapCostume` lê essa contagem e reduz uma escolha fora dela para 0 (`src/battle/view_b_b.c:20..28`).

| Personagem | Registro em 00003, deslocamento absoluto | Contagem | Ids de traje válidos |
|---|---|---|---|
| Goku (Early), 0 | `0x40` | 4 | 0, 1, 2, 3 |
| Raditz, 77 | `0x124C` | 2 | 0, 1 |
| Nappa, 80 | `0x1300` | 2 | 0, 1 |

São os ids internos, não os rótulos visuais das roupas. Esta tarefa não capturou a aparência de cada um nem
escolheu o traje do roteiro. `CharSel_Run` passa `colorPlate` como traje e variante 0 a `BattleSetup_SetMember`
(`src/menu/menu_c_e.c:2029..2030`). Variante é outro eixo, não outro traje selecionável.

`BtlObj_RequestCharaModel` (`src/battle/btl_obj.c:513..521`) pede modelo com id
`chara*10 + costume + 0x590`, ou `+0x594` quando variante é diferente de zero. Os outros dois arquivos são
`chara*10 + 0x598` e `+0x599`. Subtrair 1 dá o índice do arquivo em `pzs3us1`. A carga inicial usa os dois
lados em `battle_load.c:535..536`; a recarga preserva a mesma fórmula em `:400..409`.

A entrada `0x12` do modelo é ligada a `obj+0x91C` em `src/battle/bobj_a.c:2361` (a visão do modelo começa em
`obj+0x18`). `BtlParam`, `include/battle/btl_tech_a.h:107`, define os campos lidos por `btl_act_j.c`.
Lidos todos os trajes válidos, tanto a variante normal quanto a alternativa, para não extrapolar uma roupa às
outras. A tabela abaixo registra o intervalo absoluto da entrada, cujo comprimento alinhado é `0x100`;
o registro de parâmetros documentado ocupa `0xCC` antes do enchimento.

| Personagem | Traje | Variante | Modelo em `gamedata/pzs3us1/` | Intervalo da entrada 0x12, fim exclusivo | Resultado dos campos |
|---|---|---|---|---|---|
| 0 | 0 | 0 | `01423.bin` | `0x992C0..0x993C0` | G |
| 0 | 1 | 0 | `01424.bin` | `0x992C0..0x993C0` | G |
| 0 | 2 | 0 | `01425.bin` | `0x992C0..0x993C0` | G |
| 0 | 3 | 0 | `01426.bin` | `0x95640..0x95740` | G |
| 0 | 0 | 1 | `01427.bin` | `0x98B00..0x98C00` | G |
| 0 | 1 | 1 | `01428.bin` | `0x98B00..0x98C00` | G |
| 0 | 2 | 1 | `01429.bin` | `0x98B00..0x98C00` | G |
| 0 | 3 | 1 | `01430.bin` | `0xA8600..0xA8700` | G |
| 77 | 0 | 0 | `02193.bin` | `0x8CFC0..0x8D0C0` | R |
| 77 | 1 | 0 | `02194.bin` | `0x8CFC0..0x8D0C0` | R |
| 77 | 0 | 1 | `02197.bin` | `0x8B900..0x8BA00` | R |
| 77 | 1 | 1 | `02198.bin` | `0x8B900..0x8BA00` | R |
| 80 | 0 | 0 | `02223.bin` | `0x91540..0x91640` | N |
| 80 | 1 | 0 | `02224.bin` | `0x83880..0x83980` | N |
| 80 | 0 | 1 | `02227.bin` | `0x8F740..0x8F840` | N |
| 80 | 1 | 1 | `02228.bin` | `0x83880..0x83980` | N |

Valores semânticos dos campos, relativos à entrada 0x12 (não são dumps de arquivos):

| Campo | G, id 0 | R, id 77 | N, id 80 |
|---|---|---|---|
| `transTarget[4]`, `+0x98` | quatro sentinelas `0xFF` | alvo 78 no slot 0; demais `0xFF` | alvo 81 no slot 0; demais `0xFF` |
| `transDefault`, `+0xAC`, signed | -1 | 0 | 0 |
| `transKind[4]`, `+0xA4` | 1 em todos | 3 no slot 0, 1 nos demais | 3 no slot 0, 1 nos demais |
| `fusionResult[3]`, `+0xB4` | todos `0xFF` | todos `0xFF` | todos `0xFF` |
| `fusionPartner[3][4]`, `+0xBA` | todos `0xFF` | todos `0xFF` | todos `0xFF` |

### 2.3 O que o código permite concluir

`BtlParam_GetSlotId` lê `transTarget[slot]` (`btl_tech_a.c:1354..1356`), `BtlParam_CountSlots` conta alvos
não `0xFF` (`:1384..1395`), e `BtlParam_GetUnkB4` lê `fusionResult[n]` (`:1403..1405`). Não se usou nome,
raça ou parentesco do personagem para inferir capacidade.

- **Goku (Early): sem transformação e sem fusão pelos parâmetros**, nos oito modelos. Em
  `BtlAct_CanTransform` (`src/battle/btl_act_j.c:194..230`), todo slot 0..3 retorna 0 pelo alvo `0xFF` antes
  da condição de lua ou de custo. A contagem é zero e o padrão neutro é -1. Em `BtlAct_CanFuse` (`:328..373`),
  todo slot 0..2 retorna 0 por resultado `0xFF`, antes de procurar parceiro.
- **Raditz: uma transformação de dados, nenhuma fusão**, nos quatro modelos. Slot 0 aponta para id 78, com
  tipo 3 e padrão 0. `BtlAct_CanTransform:223` recusa esse tipo se `BtlStage_HasMoon()` for falso;
  `src/battle/stg_b.c:336..347` testa o bit `STG_FLAG_MOON = 0x10` dos dados do cenário. Se houver lua,
  a transformação ainda exige o alvo nos bits utilizáveis do lado, permissão e recursos suficientes,
  sem os bloqueios das habilidades/flags. Não existe veto universal equivalente ao de Goku.
- **Nappa: uma transformação de dados, nenhuma fusão**, nos quatro modelos. A mesma prova, mas slot 0 aponta
  para id 81, também tipo 3 e padrão 0. Trocar Raditz por Nappa não resolve sozinho o requisito.

As entradas de controle para transformação são 99..103 (`btl_act_j.c:236..265`); para fusão 104..106
(`:376..396`). A disponibilidade dos alvos também é testada por `BattleSide_IsCharaUsable`
(`battle_load.c:1993..1997`): o Duelo cria os lados com `bits = NULL` (`menu_c_e.c:2016..2017`), e
`BattleSetup_InitCharaBits` copia os desbloqueios do save (`battle_load.c:1445..1454`). Portanto, alvo bloqueado
no save também pode impedir a transformação, mas isso não prova que o personagem não a tenha. Um roteiro que
desbloqueia tudo não pode usar esse bloqueio como garantia. Não se escolheu save, cenário ou opção para forçar
uma conclusão favorável.

A prova é sobre disponibilidade normal de Duelo/Treino, modos de luta 0 e 6 (`menu_c_e.c:1965,1979`), não sobre
chamadas arbitrárias às filas. `BtlAct_CheckStoryForced` (`btl_act_j.c:1104` em diante) só opera no modo 1 e pode
chamar `QueueTransform/QueueFusion` sem consultar `CanTransform/CanFuse`. História, flags injetadas e alterações
de tabelas ficam fora da alegação. Nenhuma disponibilidade em execução foi medida aqui.

### 2.4 Perguntas para o Lucas e limite aplicado

1. **Raditz e Nappa têm transformação para Ozaru nos dados.** O Lucas prefere manter Raditz e restringir o caso
   a um cenário comprovadamente sem lua, ou escolher outro adversário sem alvo de transformação? Se quiser
   Nappa por outro motivo, ele exige a mesma restrição de lua. Não há escolha aplicada: o adversário/cenário da
   etapa 3 fica pendente, sem apresentar a decisão 24 como já satisfeita.
2. **Quais ids de traje usar?** Goku permite 0..3, Raditz e Nappa 0..1. A aparência não foi identificada nesta
   tarefa sem jogo; a escolha e sua conferência visual ficam para o Lucas/etapa 3. Nenhum traje foi fixado por
   conveniência do script de leitura.

Se o Lucas autorizar um cenário sem lua, a etapa 3 precisa fixar seu id, provar nos dados que o bit `0x10` é
zero e conferir que nenhuma mudança de cenário da sequência o habilita. Essa é uma possibilidade a decidir,
não uma restrição imposta por este documento. Se escolher outro personagem, repetir nome, contagem de trajes,
modelos e as duas tabelas antes de congelar o roteiro.

### 2.5 Reprodução das leituras de dados

A leitura usa inteiros little-endian da mídia original, não saídas convertidas da rodada em andamento. A tabela
de entradas tem `u32` de deslocamento em `4*n`, com os dois bits baixos descartados pelo leitor do jogo.
Para reproduzir a contagem de trajes e todos os parâmetros sem extrair ou executar nada:

```sh
python3 - <<'PY'
from pathlib import Path
from struct import unpack_from
from hashlib import sha1
raiz = Path('gamedata/pzs3us1')
def le32(b, n):
    return unpack_from('<I', b, n)[0]
comum = (raiz / '00003.bin').read_bytes()
inicio = le32(comum, 4) & ~3
print('comum', len(comum), sha1(comum).hexdigest(), hex(inicio))
for personagem in (0, 77, 80):
    registro = inicio + personagem * 0x3c
    trajes = unpack_from('<H', comum, registro + 0x0a)[0]
    print('personagem', personagem, 'registro', hex(registro), 'trajes', trajes)
    for variante in (0, 1):
        for traje in range(trajes):
            indice = personagem * 10 + traje + 0x590 + variante * 4 - 1
            nome = f'{indice:05d}.bin'
            b = (raiz / nome).read_bytes()
            ini = le32(b, 0x12 * 4) & ~3
            fim = le32(b, 0x13 * 4) & ~3
            p = b[ini:fim]
            print(nome, 'traje', traje, 'variante', variante, hex(ini), hex(fim),
                  'alvos', list(p[0x98:0x9c]),
                  'padrao', unpack_from('b', p, 0xac)[0],
                  'tipos', list(p[0xa4:0xa8]),
                  'fusoes', list(p[0xb4:0xb7]),
                  'parceiros', list(p[0xba:0xc6]), 'sha1', sha1(b).hexdigest())
PY
```

Para repetir a ligação nome/id, expandir **somente** a seção 2 do `00448.bin` em memória pelo algoritmo de
`src/sys/bpe.c`, obter as seções 43 e 44, e nelas as entradas `id+1`, lidas como u16 little-endian até zero.
Esta tradução de leitura reproduz a consulta em cerca de um segundo; só guarda o resultado em memória:

```sh
python3 - <<'PY'
from pathlib import Path
from struct import unpack_from

def entrada(b, n):
    ini = unpack_from('<I', b, 4*n)[0] & ~3
    fim = unpack_from('<I', b, 4*(n+1))[0] & ~3
    return b[ini:fim]

def expande(b):
    tamanho, comprimido = unpack_from('<II', b)
    b = b[8:8+comprimido]
    pos = 0
    saida = bytearray()
    while pos < len(b):
        esquerda = list(range(256))
        direita = [0]*256
        simbolo = 0
        conta = b[pos]
        pos += 1
        while True:
            if conta > 127:
                simbolo += conta-127
                conta = 0
            if simbolo == 256:
                break
            for _ in range(conta+1):
                esquerda[simbolo] = b[pos]
                pos += 1
                if simbolo != esquerda[simbolo]:
                    direita[simbolo] = b[pos]
                    pos += 1
                simbolo += 1
            if simbolo == 256:
                break
            conta = b[pos]
            pos += 1
        quantidade = (b[pos] << 8) + b[pos+1]
        pos += 2
        for valor in b[pos:pos+quantidade]:
            pilha = [valor]
            while pilha:
                valor = pilha.pop()
                if valor == esquerda[valor]:
                    saida.append(valor)
                else:
                    pilha.extend([direita[valor], esquerda[valor]])
        pos += quantidade
    print('BPE: tamanho declarado e expandido', tamanho, len(saida))
    return bytes(saida)

b = Path('gamedata/pzs3us1/00448.bin').read_bytes()
tela = expande(entrada(b, 2))
for personagem in (0, 77, 78, 80, 81):
    for secao in (43, 44):
        texto = entrada(entrada(tela, secao), personagem+1)
        nome = texto.decode('utf-16-le').split('\0')[0].lstrip('\ufeff')
        print(personagem, secao, repr(nome))
PY
```

Não chamar `Sprite_Unpack` ou o executável do jogo para essa consulta; não usar a ordem da grade do menu.
O código acima é uma leitura do formato da mídia original, não um conversor nem um verificador de instalação.

## 3. O que fica para as etapas de implementação

O inventário cobre escritores por leitura estática, não sua frequência nem alcance. Nenhuma correção visual,
igualdade de imagens, ausência de queda, desempenho, build do 360 ou cobertura oficial da F4 foi provada aqui.
O SHA1 documental de referência da `.text` x86 continua `e0af7e67cfc7048af372b16eff0707be72390d35`
([escopo da F5](00-escopo.md), referência da 9c-4), sem recalcular ou substituir executáveis em uso.
Confirmar a referência entregue pela rodada completa da F4 antes de implementar.

A etapa 4 tem agora 35 grupos pendentes (13 A, 22 B), as 11 declarações adicionais para B, 68 gravações já
tratadas e as exclusões de CPU/IMAGE/cópia. A etapa 3 tem ids e trajes comprovados, mas depende da resposta do
Lucas para fechar o adversário e o cenário sem transformação disponível, além dos trajes e das ações do roteiro.
