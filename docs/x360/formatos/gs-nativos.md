# Efeitos nativos, alfa e profundidade do GS: o que o PC faz e o caminho no 360

Especificação da **segunda parte da etapa 7 da F5** ([escopo, etapa 7](../plano/F5/00-escopo.md); decisão 24, resposta
6). Escrita por leitura (nada foi compilado nem rodado: nem build, nem emulador, nem console), para quem for implementar
no `port/x360/src/gs/gs_d3d.cpp` não ter de reler o backend do PC. A primeira parte (a profundidade persistente por
alvo) está no branch `f5-profundidade` (commits `27a98d8` e `b06a32e`, o `plano/F5/07.md` mora lá), **ainda não
medida no console** quando isto foi escrito: tudo o que depende dela abaixo parte do desenho dela, não de números dela.

Convenção de confiança, usada nas tabelas: **[código]** lido no fonte e conferido nas duas pontas; **[log]** tirado do
`etapa6_r2.log`; **[inferência]** deduzido da leitura, sem prova direta; **[estimativa]** ordem de grandeza sem medida.

## 0. Resumo, e três coisas que a leitura mudou

| Efeito | PC (`gs_gpu.c`) | 360 hoje | Falta |
|---|---|---|---|
| 1 contorno (`native 1`) | passada própria, `outline.frag`, lê a cópia dos ids | contado e pulado: 1 por quadro | tudo (seção 3.1) |
| 2 chave (`native 2`) | **morto no PC também** (seção 3.2) | contador sempre 0 | nada: a chave é uma passada de tabela |
| 3 tabela (`native 3`) | passada de tela, `dclut.frag`, lê z ou a cópia dos ids | contado e pulado: 5 a 6 por quadro | tudo (3.3) |
| 4 cópia de ids (`native 4`) | cópia do alvo auxiliar, no ponto da lista | contado e pulado: 1 por quadro | tudo (3.4); pré-requisito de 1 e 3 |
| DATE (`native 5` + `gs.frag`) | cópia do byte exato no começo de cada sequência | **já existe** (`ps_gs`, `sDate`), com o alfa saturado | provar, medir, 1 diferença de limiar (3.5) |
| FBA | bit 7 só no byte auxiliar | existe, mas **muda o alfa da mistura** | corrigir (3.6) |
| névoa, brilho | consequência das passadas 3 e 4 + sprites de alvo para alvo | os sprites já saem; a máscara (alfa) não | sai com o item 3 (3.7) |
| byte exato do alfa | segundo anexo `outAux` (R8) em todo desenho | não existe | só para ids acima de 0x7F (seção 4) |

1. **DATE e FBA não estão pulados no 360.** O enunciado da tarefa os lista entre o que o 360 pula; no `gs_d3d.cpp` e no
   `gs_d3d.hlsl` que li (os de `troca/main` e do `f5-profundidade`, iguais nesse ponto) os dois existem: o marcador 5
   resolve a cor do alvo para `sDate` (ou usa a textura do alvo fora da EDRAM) e o `ps_gs` faz `clip` pelo bit 7 do alfa
   lido, e liga o alfa a 1,0 com FBA. O que falta é prová-los, contá-los (o log não tem um contador de DATE) e corrigir
   dois detalhes (3.5 e 3.6).
2. **O FBA do 360 pode apagar a transparência do estágio.** `ps_gs` faz `a = 1.0` com FBA antes de devolver o alfa, e o
   alfa devolvido é também o fator `SRCALPHA` da mistura. O jogo liga FBA junto com DATE nos grupos transparentes do
   estágio (`StgModel_SetBlendEnv`, `src/battle/stgm_a.c`: `TEST_1 = 0x54000`, `FBA_1 = 1`) e a mistura que está valendo
   ali é `ALPHA_1 = 0x44`, por alfa de fonte (`StgModel_SetDrawEnv`). No PC o FBA só mexe no byte auxiliar. **[código]**,
   nunca visto no console. Ressalva: no estágio 5, o do Duelo fixo, o grupo A não é desenhado (`BtlStage_GetId() != 5`);
   os grupos B e C só se tiverem conteúdo (não conferi), então esse defeito pode não aparecer no caso fixo.
3. **O alfa saturado do 360 já carrega os ids de 0 a 0x80 sem perda** (conta na seção 4). O que se perde é o que passa
   de 0x80: o fundo 0xFF ("nada"), os ids da chave (0xF4 a 0xFE) e objetos com linha de alfa 9 ou mais. Por isso o
   caminho barato (tirar os ids da própria cor) cobre quase tudo, e o byte exato fica para o fim.

**Ordem recomendada** (do que mais muda a imagem para o que menos muda; detalhe na seção 6): primeiro a cópia de ids
e as passadas de tabela (lavado, borrado e névoa: itens 1, 2 e parte do 6 da triagem), depois o contorno, depois a
correção do FBA e a conferência do DATE, por último o byte exato.

## 1. O que o 360 tem hoje (a base da segunda parte)

Fonte: `port/x360/src/gs/gs_d3d.cpp` e `gs_d3d.hlsl` do `f5-profundidade` (os nomes abaixo são de lá).

| Peça | O que é | Para a segunda parte |
|---|---|---|
| Alvos | uma EDRAM só: cor A8R8G8B8 na base 0 e D24S8 depois (832 + 832 de 2.048 tiles, 1024 x 1024); cada alvo do GS tem uma textura tiled com a cor (`sAlvos[i].tex`, 4 MiB) | a cor é o único anexo; o alfa dela é `min(A / 0x80, 1)` |
| `sai_da_edram` / `entra_na_edram` | Resolve do retângulo sujo da cor para a textura do alvo; a entrada limpa a EDRAM e redesenha a textura de volta | cada passada nativa é uma "visita" a um alvo: entra por aqui |
| Profundidade (1ª parte) | `sProf[i].tex` (D24S8 tiled, 4 MiB, só nos alvos em que algum desenho escreve z), `sZDono`, `sSujoZ`, `prof_entrega`, modo 1 (restauro como cor) ou 2 (oDepth), escolhido por autoteste no boot; `BT3_Z=0` desliga | a passada de tabela por névoa lê `sProf[alvo].tex` |
| `targetAux`, `targetDepth` | `target_none`: devolvem 0 | o PC os usa para a cópia e a tabela; no 360 as passadas lêem `sAlvos`/`sProf` direto |
| Nativos | o `frame_end` conta `d->native & 7` em `sConta.semNativo[]` e pula (1 a 4); os ≥ 100 (imagem direto no quadro) em `semFb`; o 5 é o DATE | linha do log: `nativos A/B/C/D (contorno, chave, tabela, ids)` |
| DATE | marcador 5: Resolve de `sSujo` (união com o conteúdo) do alvo residente para `sDate` (4 MiB); fora da EDRAM, a textura do alvo; `prepara` põe em `s1`; `ps_gs` faz `clip` pelo alfa lido `>= 254.5/255` | existe; sem contador |
| FBA | `ps_gs`: `a = 1.0` | diverge do PC (3.6) |
| `quad()` / `copia()` | um retângulo do alvo atual com uma textura e um pixel shader (`vs_copia`); `esquece_estado()` depois | a base de toda passada de tela nova |
| `aplica_pipe` | decodifica a chave do pipeline; **para a chave de passada de tabela (bit 22) deixa `ZENABLE` ligado e o `ZFUNC` pela chave** (a chave de tabela tem `ztst` 0, ou seja, NEVER) | tem de ligar `ZENABLE = FALSE` nas passadas de tela |

Números da luta (Duelo, Goku (Early) contra Kuririn, estágio 5; `etapa6_r2.log`, 46 trechos de 60 quadros com algum
nativo, o primeiro é a entrada na luta) **[log]**:

- por quadro: contorno 1,0; ids 1,0; **tabela 5,0 a 6,0** (360 em 60 quadros na maior parte dos trechos); chave 0;
- 2.012 a 2.339 desenhos e 114 a 130 mil vértices por quadro no estágio 5; 1.262 a 1.952 desenhos no segundo trecho de luta (quadros
  9239 a 9479, 8 alvos, tabela 5,0 por quadro);
- **42,0 trocas de alvo por quadro**, 7 alvos (8 no segundo trecho);
- quadro inteiro 57 a 110 ms (a CPU da VU1 domina; a etapa 8 trata disso), memória livre mínima vista 30 MB com o piso
  em 32, texturas 101 a 112 MB do teto de 128, alvos 36 MiB, buffers 77,6 MiB (79.488 KB).

O menu não emite nenhum (`0/0/0/0`). O que o `ChrView` e o Shenlong da etapa 9 emitem não foi medido.

## 2. O modelo do PC, em uma página

O jogo guarda **ids de objeto no alfa do quadro**: cada material de cada modelo tem um id (`alphaRow * 15 + slot`, com
`alphaRow` o bit livre de `BtlObjVis_AllocFreeBit`: os dois lutadores pegam as primeiras linhas, ids 0 a 29; tiros e
modelos de efeito pegam as seguintes). O fundo vale 0xFF ("nada", segundo `docs/port/README.md`; **não conferi quem
escreve esse 0xFF**). No PS2 esse byte vive na parte alta da página de profundidade (PSMT8H, 8 bits, lido por uma
tabela de 256 cores, a CLUT); o jogo desenha esse byte pela CLUT por cima da tela em 16 faixas de 32 x 448. O PC não tem essa memória, então:

- todo alvo tem **três anexos** (`sTgCol` RGBA8, `sTgAux` R8, `sTgDep` D32F); `gs.frag` escreve `outColor` (alfa
  `min(A/0x80, 1)`) e `outAux.r` = o byte exato, sem mistura, sob a máscara de alfa do FBMSK;
- as passadas que só fazem sentido na memória do PS2 são **descartadas** em `draw_state` (`gsSkipped`) e substituídas
  por nativas, que entram na lista como `GsDraw` com `native != 0` e rodam no replay, cada uma terminando a passada de
  desenho atual (`cur = -1`).

A ordem do jogo em `BtlObj_BeginDraw` (`src/battle/btl_obj.c`, **[código]**), que é a ordem dos nativos:

1. `GfxPost_CopyAlphaToDepth` -> `native 4` (a cópia dos ids);
2. `GfxAlphaKey_Draw` -> marcador 2 (ignorado) e uma tabela, CLUT 0x3E94 (a chave);
3. `ObjOutline_Draw` -> marcador 1 (`native 1`, o contorno); as três passadas de tabela que ele faz com a CLUT 0x3E8C
   são descartadas por `depth_clut` (`cbp == 0x3E8C`);
4. `ObjGlow_Draw` -> tabela, CLUT 0x3E90, ids para alfa (só alfa), e `GfxPost_DrawGlow` (sprites de alvo para alvo).

E em `StgFx_DrawPre` (`src/battle/stg_c.c`): `GfxDepthFog_Draw` (a névoa em si é descartada, mas marca que o byte da
profundidade agora é névoa), `StgDepthTint_Draw` (tabela, CLUT 0x3E64, aditiva `0x48` ou por mistura `0x44`),
`StgGlare_Draw` (tabela, CLUT 0x3E88, só alfa, e `DrawGlow`), `StgFog_Draw` (feature 3), `StgPanBlur_Draw` (feature 0xD,
duas tabelas, CLUT 0x3E98) e `StgHaze_DrawAll` (feature 9, CLUT 0x3E9C). **Não conferi a ordem relativa entre
`BtlObj_BeginDraw` e `StgFx_DrawPre` no quadro**, nem quais features o estágio 5 liga; as seis tabelas por quadro do
log são, por essa leitura, a chave + o brilho de objeto + a tinta de profundidade + o brilho do céu + mais duas do
estágio **[inferência]**. Para separá-las sem adivinhar, o log novo conta as tabelas por (fonte, chave do pipeline),
seção 7.

## 3. Efeito a efeito

### 3.1 Contorno (`native 1`)

**O que faz na imagem.** Uma linha escura de um pixel em volta de cada lutador e entre partes do corpo com ids de
valor de tabela diferente. Sem ele o lutador fica sem a linha de desenho animado (hoje falta, `06.md`).

**Quem emite e quanto.** `ObjOutline_Draw` por quadro de luta (1,0 por quadro **[log]**), só na luta.

**PC.** Marcador 1 -> `GsGpu_Native(1)` (`gs_draw.c`) grava um `GsDraw` `native = 1` com a tesoura de `gGs.scissor[0]`
(pulado com `BT3_FX_OFF & 1`). No replay (`gs_gpu.c`, `if (d->native)`): passada própria no `sTgCol` do alvo (carga, sem
profundidade, sem aux), pipeline `sOutlinePipe` (mistura `ONE`/`ONE`, `REVERSE_SUBTRACT` na cor, máscara RGB, alfa
intocado), um triângulo que cobre o alvo, lendo `sAuxCopy` (a cópia dos ids do `native 4`, **não** o alfa vivo, que as
passadas seguintes sobrescrevem). `outline.frag`:

```
id(uv)   = round(aux(uv).r * 255)
tabela   = id == 255 ? 0 : ((id << 3) & 0xFF) == 0 ? 0x80 : (id << 3) & 0xFF      // ObjOutline_BuildClut
aresta   = tabela(aqui) > tabela(direita) || > (baixo) || > (esquerda) || > (cima)  // um pixel do GS de passo
saída    = aresta ? 0x64/255 : 0     (a mistura subtrai da cor)
```

O PC não reproduz o PS2 exato (a espessura e a força da linha: o PS2 desenha a imagem de arestas três vezes com alfas
diferentes, `README.md` do PC); o alvo da segunda parte é o **PC**, com a referência do rasterizador de software
(que executa as passadas do PS2 de verdade) só como comparação visual.

**360 hoje.** Contado e pulado.

**Caminho no 360.** Uma passada de tela a mais no `frame_end`, no lugar do `continue` do marcador 1:

1. o alvo do marcador tem de estar na EDRAM (`entra_na_edram(alvo, 0)`; sem z, a profundidade do dono fica);
2. um `quad()` da tesoura do marcador com o `ps_contorno` (rascunho abaixo), lendo a textura de ids (3.4), mistura
   `REVSUBTRACT` `ONE`/`ONE` (já existe em `aplica_blend`, chave `k[0] == 3`), `COLORWRITEENABLE` só RGB, `ZENABLE`
   falso;
3. contar em `sConta` (um contador de contorno feito, em vez de `semNativo[1]`).

```hlsl
// RASCUNHO, não compilado. ids vem da textura da cópia (3.4), alfa saturado: byte = a8 * 128/255 (exato até 0x80).
float id_de(float2 uv) {
    float a8 = tex2D(idsTex, uv).a * 255.0f;
    return a8 >= 254.5f ? 255.0f : floor(a8 * 128.0f / 255.0f + 0.5f);   // todo o "acima de 0x7F" vira 255 (seção 4)
}
float tabela(float2 uv) {
    float id = id_de(uv);
    if (id >= 255.0f) return 0.0f;
    float c = fmod(id * 8.0f, 256.0f);        // sem operador de bits no SM3 do Xenos
    return c == 0.0f ? 128.0f : c;
}
float4 ps_contorno(float2 uv : TEXCOORD0) : COLOR0 {
    float q = tabela(uv);
    bool aresta = q > tabela(uv + float2(passo.x, 0)) || q > tabela(uv + float2(0, passo.y)) ||
                  q > tabela(uv - float2(passo.x, 0)) || q > tabela(uv - float2(0, passo.y));
    return float4(aresta ? 100.0f / 255.0f : 0.0f, aresta ? 100.0f / 255.0f : 0.0f, aresta ? 100.0f / 255.0f : 0.0f, 0.0f);
}
```

Cuidados do Xenos: amostragem no ponto e presa na borda (`aplica_sampler(estágio, 6)`: ponto, `CLAMP` em u e v); o passo
é `1 / TAM` (SCALE = 1); `HALFPIXELOFFSET` ligado, como na cópia (que funciona), então `uv` do `quad()` cai no centro.

**Custo [estimativa].** 1 passada de até 640 x 448 com 5 leituras: 0,3 a 0,6 ms de GPU por quadro; o `Resolve` da
cópia de ids é do 3.4. Nenhuma RAM além da textura de ids.

### 3.2 Chave (`native 2`): não existe mais

`GfxAlphaKey_Draw` emite o marcador 2 e depois a passada de tabela da CLUT 0x3E94. `GsGpu_Native` só aceita
`effect == 1` (`if (effect != 1 || ...) return;`, **[código]**), então **nunca grava um `native 2`**: o ramo
`d->native == 2 ? sKeyPipe` do replay e o `alphakey.frag` são código morto; a chave sai pela passada de tabela
(`depth_clut`, que já usa a tabela do jogo). O `0` do meio do log `nativos 60/0/360/60` é, portanto, **o esperado**, não
uma lacuna: a chave do 360 é uma das tabelas do contador 3, e `sConta.semNativo[2]` deve sair do log (ou ficar como
verificação de que continua 0).

A chave tem outra dependência: os ids dela são 0xF4 a 0xFE (acima de 0x80), o que só o byte exato dá (seção 4). Sem ele,
a passada de chave do 360 vê todo o "acima de 0x7F" como 255 e **não desenha nada**: o efeito (lutador visto por
transparência quando a câmera o cobre) some, e só nessa situação.

### 3.3 Tabela de profundidade / de ids (`native 3`)

**O que faz na imagem.** É o bloco de construção dos efeitos de tela do jogo: o jogo desenha um byte (ou a névoa de
profundidade, ou os ids) por cima da tela como textura de 8 bits lida por uma tabela de 256 cores; o resultado entra na
cor (tinta, chave) ou **só no alfa** (a máscara que alimenta o brilho do céu, o brilho de objeto e o borrão das coisas
distantes). Sem as passadas, as passadas seguintes (sprites comuns, que o 360 já desenha) somam a imagem reduzida e
borrada **sem máscara**: é o lavado (+18% no chão branco) e o borrado do cenário (itens 1 e 2 da triagem, `06.md`).

**Quem emite e quanto.** Pelo código (seção 2): chave, brilho de objeto, tinta de profundidade, brilho do céu, e, por
estágio, pan blur (2), neblina e haze. No log: 5 a 6 por quadro **[log]**. Só na luta.

**PC.** `depth_clut` (`gs_draw.c`) é chamado de `draw_state` quando o jogo desenha, em sprites, a página de profundidade
como PSMT8H (`tme && sprite && tpsm == 0x1B && tbp/32 == zbp`); as 16 faixas viram **um** `GsDraw` (`native = 3`; a faixa
seguinte igual é descartada): `src` 0 = névoa pela profundidade (`gsDepthByteIsFog`, marcado pela passada de névoa), 1 =
cópia dos ids; `tex` = a CLUT como textura 256 x 1 RGBA8 (`clut_texture`, alfa como gravado); `blendc`, `scissor`,
`misc[0]` = o máximo do z pela `ZBUF.PSM` do contexto (4294967295 / 16777215 / 65535), e a chave do pipeline tem o
bit 22 (só mistura e máscara de escrita). `gsFxOff` pode pular as que colorem (bit 4, só `src == 0` com máscara
diferente de "só alfa") e a tinta da chave (bit 2, CLUT 0x3E94). No replay (`gs_gpu.c`, `native == 3`): passada de duas
cores (`sTgCol` + `sTgAux`, carga), sem profundidade, `dclut.frag`:

```
idx   = src == 0 ? (z = depth(uv) * zmax;  z >= 65448 ? 0 : clamp(255 - int(z / 256), 0, 255))
                 : round(aux_copia(uv).r * 255)
c     = clut[idx]
cor   = c.rgb;   alfa = clamp(c.a * 255 / 128, 0, 1)        // 1,0 = 0x80, a convenção da cor
aux   = c.a                                                  // o byte exato (só o PC guarda)
```

A fórmula da névoa (255 - z/256, zero para z >= 0xFFA8) foi **lida do código do jogo e nunca confirmada** (o README do PC
a registra com ±24 em faixas no meio da imagem contra o rasterizador de software); o 360 herda a mesma dúvida.
**Não conferi** em que escala de `ZBUF.PSM` as passadas de névoa chegam (o `65448` é um valor de 16 bits).

**360 hoje.** Contado e pulado.

**Caminho no 360.**

- **A fonte `src == 1`** (chave, brilho de objeto) só precisa da cópia dos ids (3.4) e da CLUT: não depende da
  profundidade. Implementar primeiro.
- **A fonte `src == 0`** (tinta, brilho do céu, as de estágio) precisa do z do alvo como textura: a `sProf[alvo].tex`
  (D24S8) da primeira parte, **em dia**. O z mora na EDRAM (`sZDono == alvo`) até outro alvo com z entrar; então, antes da
  passada, **resolver o z sujo para a textura sem entregar a posse**: uma `prof_sincroniza()` nova = o corpo de
  `prof_entrega` sem o `sZDono = -1` (resolve `alinha80(sSujoZ)` para `z->tex`, soma em `z->conteudo`, zera `sSujoZ`).
  Depois da primeira sincronização do quadro, as outras passadas não pagam nada (nada escreve z entre elas, **a conferir**:
  os 16 strips que "limpam a profundidade entre o estágio e os lutadores" escrevem z).
- Leitura do z: `tex2D(sProf.tex).x` devolve a profundidade normalizada (é o que o `ps_zprof` já usa; o autoteste do boot
  mede se o modo 2 é exato). Se o autoteste só aceitar o modo 1, ler pelo cabeçalho `Prof.cor` (A8B8G8R8) e remontar os 24
  bits dos canais, desfazendo a troca de metades de 40 colunas por tile de 80 pixels (a mesma do `ps_zcor`). Com
  `sModoZ == 0` (nenhum modo exato), pular as de `src == 0` e contá-las (o que o log já faz), sem inventar névoa.
  `z = floor(profundidade * misc[0] + 0.5)`.
- **O pixel shader** (`ps_tabela`): o `dclut.frag` com a conta do byte do alfa saturado:

```hlsl
// RASCUNHO, não compilado. s0 = z do alvo (D24S8), s1 = cópia dos ids (3.4), s2 = a CLUT (256 x 1, ponto, presa).
float4 par : register(c4); // x: fonte (0 névoa, 1 ids), y: máximo do z
float4 ps_tabela(float2 uv : TEXCOORD0) : COLOR0 {
    float idx;
    if (par.x < 0.5f) {
        float z = floor(tex2D(zTex, uv).x * par.y + 0.5f);
        idx = z >= 65448.0f ? 0.0f : clamp(255.0f - floor(z / 256.0f), 0.0f, 255.0f);
    } else {
        idx = id_de(uv);      // a mesma conta do contorno (3.1)
    }
    float4 c = tex2Dlod(clutTex, float4((idx + 0.5f) / 256.0f, 0.5f, 0.0f, 0.0f));
    return float4(c.rgb, saturate(c.a * 255.0f / 128.0f));
}
```

  (`tex2D` com gradiente fora de desvio dinâmico, como o `ps_gs` já faz; sem `discard`.) A textura da CLUT é a de
  `clut_texture`, que o `tex_create` do 360 faz `D3DFMT_LIN_A8B8G8R8` de 256 x 1: lida com coordenada normalizada, ponto,
  presa na borda (`aplica_sampler(estágio, 6)`).
- **A mistura e a máscara** vêm da chave do pipeline (`d->pipeline`, bit 22 + mistura + máscara de escrita): `aplica_pipe`
  já as decodifica, **mas** tem de ligar `ZENABLE = FALSE` (hoje o bit 22 cai no `ZFUNC` NEVER) e usar a tesoura de
  `d->scissor` como `prepara` faz. Duas das passadas escrevem **só o alfa** (brilho de objeto e brilho do céu): é a
  máscara `COLORWRITEENABLE = ALPHA`, que o `aplica_pipe` já trata.
- O alvo da passada é `d->target` (o quadro do contexto, quase sempre o principal): `entra_na_edram(alvo, 0)`.
- **Falhas para o log:** CLUT nula (`d->tex == 0`), alvo sem `Prof.tex` numa passada de névoa, alvo não `cleared`
  (o PC pula igual).

**Custo [estimativa].** 5 a 6 passadas por quadro, cada uma de até 640 x 448 (a tesoura limita) com 2 a 3 leituras e
mistura: 0,3 a 0,6 ms de GPU cada, **2 a 4 ms por quadro**; mais um Resolve de z (0,1 a 0,3 ms) na primeira passada de
névoa. CPU: estado e um `DrawPrimitiveUP` de 4 vértices por passada, décimos de ms. Sem RAM nova além das texturas dos
itens 3.1 e 3.4. Comparado ao quadro de 57 a 110 ms do log, é ruído; vira custo de verdade só depois da etapa 8.

### 3.4 Cópia de ids (`native 4`)

**O que faz.** `GfxPost_CopyAlphaToDepth` copia o alfa do quadro (os ids) para o byte de reserva da profundidade. Os
efeitos seguintes lêem a **cópia** dos ids, porque as passadas que escrevem alfa (as de tabela de só alfa) sobrescrevem o
alfa vivo.

**Quem emite e quanto.** 1,0 por quadro de luta **[log]**.

**PC.** `draw_state`, no ramo `fbp == zbp && tme` com `tpsm != 0x1B`: grava um `GsDraw` `native = 4` com o alvo da
textura de origem (`tbp / 32`), um só por vez. No replay: `SDL_CopyGPUTextureToTexture` de `sTgAux[alvo]` para
`sAuxCopy` (R8, do tamanho de um alvo), se o alvo está `cleared`. As passadas 1 e 3 lêem `sAuxCopy`.

**360 hoje.** Contado e pulado.

**Caminho no 360.** É um **snapshot da cor do alvo**, exatamente o que o DATE já faz:

1. `sIds`, uma textura tiled A8R8G8B8 (como `sDate`; de 640 x 448 bastam 1,09 MiB, ver o orçamento da seção 5);
2. no marcador: se o alvo está na EDRAM, `Resolve(D3DRESOLVE_RENDERTARGET0, ret, sIds, ...)` do retângulo sujo mais o
   conteúdo (alinhado a 8), como o `native 5` faz com `sDate`; fora da EDRAM, **copiar** a textura do alvo (um `copia()`
   para `sIds`), não referenciá-la, porque a leitura é adiada até as passadas 1 e 3 e a textura do alvo muda depois;
3. `SetTexture(estágio, NULL)` antes do Resolve (o destino não pode estar ligado), como em `sDate`;
4. `sIds` é **separada** de `sDate`: o DATE se refaz no meio (no PC são `sAuxCopy` e `sDateCopy`).

O que a cópia contém é o alfa **saturado** (seção 4); o contorno e a tabela decodificam com `id_de`.

**Custo [estimativa].** 1 Resolve de até 640 x 448 por quadro: 0,1 a 0,3 ms de GPU; 1,09 MiB de RAM.

### 3.5 DATE (teste de alfa do destino)

**O que faz.** `TEST.DATE`: o GS olha o bit 7 do alfa **já gravado** no pixel e só desenha onde ele é 0 (`DATM` 0) ou 1
(`DATM` 1). O jogo o usa em três lugares que importam: as barras do HUD (o enchimento cortado por uma máscara), as faixas
de texto (o anúncio READY/FIGHT) e, com FBA, **os grupos transparentes do estágio e a sombra plana** (`TEST_1 = 0x54000`:
DATE 1, DATM 0, `ZTE`, `GEQUAL`, em `StgModel_SetBlendEnv` e `ObjGs_AddFlatShadowEnv`): cada pixel recebe a transparência
**uma vez**, porque o primeiro fragmento liga o bit 7 (FBA) e os seguintes falham no DATE. É isso que impede as faces
sobrepostas da silhueta de somar e escurecer demais (o item 5 da triagem, "sombra preta demais", é esse mecanismo, não a
profundidade). Quantos por quadro: **sem contador no 360**; no PC, "2.479 triângulos no começo do quadro" num estágio de
teste e 1.092 corridas do programa 2b da sombra por quadro numa medição antiga (`README.md` do PC).

**PC.** `date_snapshot` (chamado de todo ponto de entrada de desenho) grava um `GsDraw` `native = 5` no começo de cada
**sequência** de desenhos com DATE do mesmo alvo; o replay copia `sTgAux[alvo]` (o byte exato) para `sDateCopy`; o
`gs.frag` lê `sDateCopy` (`texelFetch`, `>= 127.5/255` = bit 7 ligado) e dá `discard` se `ligado != (misc.y > 1.5)`.
**Limite do PC (e do 360): a cópia vale para a sequência inteira.** O bit 7 que o FBA liga dentro da sequência **não** é
visto pelos fragmentos seguintes da mesma sequência. Ou seja, nem o PC faz o "uma vez por pixel" entre faces da mesma
sequência: a sombra plana do PC pode somar nas sobreposições. **[inferência]**, sem imagem que mostre.

**360 hoje.** Existe e é equivalente ao do PC, **com duas diferenças**:

- *Limiar.* O PC lê o byte exato (`round(a * 128) >= 128`, ou seja, `a >= 0,99609`); o 360 lê o alfa de 8 bits da cor,
  onde `a` entre 0,99412 e 0,99804 grava 254, e o limiar `254.5/255` o trata como desligado. Só difere para alfas
  fracionários (interpolação de Gouraud, produto textura x vértice) que cruzam 0x7F/0x80; para alfas inteiros (as barras
  do HUD, os 0x80 opacos, FBA) é exato (0x7F grava 253, 0x80 grava 255). **[código]**, conta de ponta a ponta.
- *Custo e contagem.* Cada sequência faz um Resolve da área inteira com conteúdo (`sSujo` unido ao `conteudo`), em
  qualquer alvo. Com dezenas de sequências por quadro (principalmente na página da sombra, de 256 x 256, onde é barato) o
  custo pode importar no alvo principal. O log não diz quantas.

**Caminho no 360.**

1. **Contar** (seção 7): marcadores 5 por quadro, quantos viraram Resolve, a área, o tempo; desenhos com DATE por alvo.
2. **Provar** com um quadro do HUD (barra de vida meio cheia) e outro da sombra, contra o PC.
3. Só se a conta do limiar importar numa imagem: o byte exato (seção 4).
4. **Opcional, com a opção de Z da primeira parte já ligada: stencil para o "uma vez por pixel".** O Xenos tem 8 bits de
   stencil na D24S8 que a profundidade persistente já preserva (modo 1 copia os 32 bits). `DATE` 1, `DATM` 0, com `FBA`
   ligado e a máscara de alfa ligada, poderia ser: teste de stencil "igual a 0", passe = substitui por 1, limpando o
   stencil no começo da sequência. Isso faz o que o GS faz e que a cópia não faz. **Não verificado** no console (o
   limite é que só vale quando o FBA e o DATE andam juntos numa sequência, e que a sequência não pode misturar
   desenhos com e sem FBA); deixar como experimento depois de ver imagem de sombra com a cópia.

### 3.6 FBA

**O que faz.** Todo pixel gravado sai com o bit 7 do alfa ligado: a marca que o DATE (3.5) lê. Usos: máscara do HUD
(`hud_a_d.c`: só alfa, com FBA), grupos transparentes do estágio e sombra plana.

**PC.** `gs.frag`: `ab = (ab_inteiro | 128) / 255` **só no byte auxiliar**; o alfa da cor (`outColor.a`) fica como o
cálculo deu, e é ele o fator da mistura.

**360 hoje.** `ps_gs`: `if (misc.z != 0) a = 1.0;` antes de `return float4(saturate(rgb), saturate(a))`. O alfa devolvido
é o da mistura (`SRCALPHA`/`INVSRCALPHA`, `aplica_blend`) **e** o gravado (`SRCBLENDALPHA = ONE`). Quando um desenho com
FBA mistura por alfa de fonte, a mistura passa a ser opaca. Os desenhos de HUD (RGB mascarado, a mistura não aparece)
e a sombra plana (alfa fixo `0x46`, `BLENDFACTOR`) não são afetados; **os grupos transparentes do estágio são** (os
três vêm depois de um `Dma_AddFillRect` de tela cheia que zera o alfa, para o DATE começar de bit 7 limpo; no estágio 5 o
grupo A não é desenhado).
**[código]**, nunca visto no console.

**Caminho no 360.** Duas opções, a primeira é a conservadora:

1. **Duas passadas só para o desenho que tem FBA e mistura por `SRCALPHA`**: a primeira, a de hoje com o `a = 1.0`
   tirado (cor RGB, `COLORWRITEENABLE` RGB, mistura como está), a segunda só alfa (`COLORWRITEENABLE` ALPHA, mistura
   desligada, um modo do `ps_gs` que devolve `a = 1.0` depois do mesmo `clip` de teste de alfa e de DATE). A segunda
   precisa do mesmo teste de profundidade (`GEQUAL` empata, passa; `GREATER` não: se a primeira escreveu z, trocar a ordem
   ou desligar a escrita; nos grupos do estágio `ZMSK` é 1, não escreve). Custo: só nos desenhos FBA + mistura (um
   subconjunto pequeno; **contar quantos**, seção 7).
2. Aceitar a divergência: FBA muda só o alfa e a mistura sai por `a` original. Perde o bit 7 nesses pixels, e o DATE dos
   desenhos seguintes que dependem dele passa onde o PS2 não passaria.

### 3.7 Névoa e brilho

Não são efeitos separados no backend do PC: **não existem no `gs.frag` nem nos três `.vert`** (conferido na triagem).
Saem do resto:

- **Névoa** (`StgFog_Draw`, `GfxDepthFog_Draw`, a tinta de profundidade): passadas de tabela com `src 0` (3.3) mais sprites
  comuns. Sai com 3.3.
- **Brilho** (`GfxPost_DrawGlow`: reduz a imagem pelo alfa para o alvo de 256 x 256, desce a 64 x 64, borra entre dois
  alvos, soma de volta com `ALPHA 0x68`; e o borrão por alfa do destino, `ALPHA 0x54`): **sprites de alvo para alvo, que o
  360 já desenha** (`draw_state` os grava como sprites comuns). O que falta é a **máscara de alfa** que as passadas de
  tabela de só alfa (brilho de objeto, brilho do céu) escrevem; sem ela a mistura soma tudo. Sai com 3.3, não pede passada
  própria. A cor de alfa que esses sprites lêem é a **saturada** da cor, no PC também (um alvo lido como textura usa o
  alfa já reescalado), então não depende do byte exato.

A conferência de que a hipótese da triagem vale é o `BT3_FX_OFF` das aberturas 2 a 4 da rodada da primeira parte
(24 / 16 / 8). Se elas mostrarem que nada muda, os itens 1 e 2 da triagem voltam para a etapa 6 e a ordem da seção 6
deixa de valer.

## 4. O byte exato do alfa: o que exige

**O que o 360 guarda.** A cor tem 8 bits de alfa com a convenção `a8 = round(min(A / 128, 1) * 255)`, A em 0..255.

**Perda.** Decodificar: `A = round(a8 * 128 / 255)`. Para A de 0 a 0x80 a ida e a volta é exata (o erro máximo é
0,25 unidade, antes do arredondamento). Para A > 0x80, `a8 = 255` e a volta dá 0x80: **0x80 a 0xFF são uma classe só**.
Consequência: o "nada" (0xFF), a chave (0xF4 a 0xFE) e objetos de linha 9 ou mais (`9 * 15 = 135 = 0x87`) se confundem.
A conversão `id_de` dos rascunhos acima manda a classe para 255.

| Consumidor | Com o alfa saturado | Perde |
|---|---|---|
| contorno | ids 0..0x7F exatos; fundo 0xFF e ids altos viram "nada" | linha em volta de objetos de linha >= 9; a chave |
| tabela, brilho de objeto (CLUT 0x3E90) | ids < 0x80 exatos | o brilho dos objetos de linha >= 9 (a tabela dá 0 no 255) |
| tabela, chave (CLUT 0x3E94) | todos os ids da chave viram 255: **a passada não desenha** | a transparência do lutador coberto |
| DATE | exato para alfas inteiros | limiar para alfas fracionários (3.5) |
| FBA | depende da correção 3.6 | |

**A leitura de volta.** Não há leitura para a CPU: tudo fica na GPU. "Ler de volta" é (a) o Resolve da cor do alvo para
uma textura (`sIds`, `sDate`) e (b) a amostragem no ponto dela no pixel shader; a CPU só lê a cor para as capturas
(`captura()`). O Resolve preserva os 8 bits do alfa (o DATE de hoje depende disso e a primeira parte também). O `id_de`
faz `floor(a8 * 128 / 255 + 0.5)`; a amostragem de ponto devolve `a8 / 255` exato o suficiente para isso.

**Como ter os bits acima de 0x80**, em ordem de preferência (todas **[inferência]**, nenhuma testada no Xenos):

1. **Não ter** (hoje). Cobre os dois lutadores e quase todo o resto. Contar no log (seção 7) quantos desenhos que
   gravam alfa têm alfa de vértice acima de 0x80 e quantos objetos de linha >= 9 existem num quadro; só decidir pelos
   números.
2. **Stencil como byte, para o desenho cujo alfa é constante.** A D24S8 da primeira parte tem 8 bits de stencil que o
   restauro preserva. Se o alfa de um desenho é constante (cor de vértice constante por lote nos programas da VU1; sem
   modulação por alfa de textura), o byte exato é um `STENCILREF` por desenho com `STENCILFUNC` sempre, `STENCILPASS`
   substituir, que respeita profundidade e teste de alfa sem custo de EDRAM nem passada extra. **Limite:** não serve
   para alfa por pixel (textura x vértice, interpolação), e exige que o alvo tenha textura de profundidade. Dá o byte
   no stencil, que é lido como `Prof.cor` (o canal baixo ou alto, **qual dos dois não conferi**).
3. **Passada extra só para o que escreve ids:** o desenho duplicado com máscara só de alfa e mistura desligada, um `ps`
   que devolve o byte exato `byte / 255` para uma textura de ids própria. Exige distinguir a janela dos ids (do clear
   do fundo até o marcador 4); **não localizei o ponto exato dessa janela no código**.
4. **Segundo alvo de cor no mesmo desenho (MRT).** Não cabe do jeito atual: 832 + 832 + 832 = 2.496 de 2.048 tiles.
   Cabe se as superfícies encolherem (a opção b da pergunta 6): 1024 x 512 dá 416 tiles cada, 1.248 no total; 1024 x 704
   dá 572 cada, 1.716. Dois riscos que só o XDK ou o console dizem: o D3D9 do 360 exige o mesmo tamanho nos alvos
   (então todos encolhem) e **não sei se há mistura por alvo** (o byte auxiliar não pode misturar). Só depois da sonda
   da seção 5.

## 5. Orçamento: EDRAM, RAM, passadas

**EDRAM.** Nada muda com as opções 1 a 3 (nenhum tile a mais; a primeira parte já usa 832 + 832 de 2.048). Em tiles de
80 x 16 pixels a 32 bits: 1024 x 1024 = 13 x 64 = 832; 1024 x 512 = 416; 640 x 448 = 8 x 28 = 224.

**A sonda que decide a opção b** (pergunta 6, "só reorganizar a EDRAM se não couber"): hoje cada alvo é uma textura
de 1024 x 1024, mas só se usa uma parte. Registrar no log, por alvo e por trecho, a **união dos retângulos desenhados**
(`sSujo` / `conteudo` já a medem) e a maior tesoura. Se o principal não passar de 640 x 448 e os de trabalho (sombra,
brilho) ficarem em 256 x 256, os 1024 x 512 cabem com folga e o MRT (opção 4 acima) fica possível; se algum alvo passar de
512 de altura, a opção b morre sem custo de implementação. É um contador, de uma tarde, e vale antes de qualquer MRT.

**RAM (texturas tiled A8R8G8B8, tamanhos exatos do `tam_real`, múltiplos de 32 x 32; [estimativa] onde marcado):**

| Peça | Tamanho | Vive |
|---|---|---|
| `sIds` (cópia dos ids) | 640 x 448: 1,09 MiB; 1024 x 1024: 4 MiB | uma só |
| `sDate` (já existe, 4 MiB) | pode cair a 1,09 MiB pelo mesmo corte | uma só |
| texturas de profundidade (1ª parte) | 4 MiB por alvo que escreve z, esperados 2 a 4 | uma por alvo |
| aux persistente por alvo (só se a opção 2, 3 ou 4 entrar) | 1,09 MiB no principal, 4 MiB cheio | só o alvo principal |

A segunda parte, com o corte para a área usada, soma **cerca de 1 MiB** (e economiza 3 MiB cortando o `sDate`); sem o
corte, 4 MiB. O orçamento da etapa 1 tem **30 a 31 MB livres no mínimo contra um piso de 32** na luta **[log]**, e a
primeira parte já pede 8 a 16 MiB; então a classe nova (`CL_IDS`, como `CL_PROF`) tem de entrar na conta e na linha
`memória:` do log, e as texturas abrem espaço pelo descarte (`GsDraw_X360Abre`) como os alvos. Se o piso apertar, baixar
`BT3_MEM_TEX_MB` é o botão (113 MB vivos hoje contra o teto de 128).

**Passadas extras por quadro [estimativa]:** 1 Resolve de ids + 1 Resolve de z (a primeira vez) + 5 a 6 tabelas + 1 contorno
= 8 a 9 passadas de tela e 2 Resolves, 3 a 5 ms de GPU; a CPU soma décimos de ms. Sem a persistência de z (modo 0), a
parte de névoa não roda.

## 6. Ordem de implementação recomendada

Do que mais muda a imagem para o que menos muda, respeitando as dependências:

| # | O quê | Muda | Depende de | Prova |
|---|---|---|---|---|
| 0 | Contadores e a sonda de extensão (seção 7); `ZENABLE` falso na chave de tela | nada | nada | o log novo |
| 1 | **3.4 cópia de ids** e **3.3 tabela `src == 1`** (brilho de objeto, chave) | a máscara do brilho de objeto: corta o lavado do contorno do lutador; a chave só com o byte exato | `sIds`, `ps_tabela` | autoteste do boot + captura contra o PC |
| 2 | **3.3 tabela `src == 0`** (tinta, brilho do céu, neblina, haze) | **o maior**: lavado (+18% no chão branco), borrado, parte do HUD/escuro | `prof_sincroniza`, o modo de z da 1ª parte | `ppm_stats.py` nas caixas do `06.md`; `BT3_FX_OFF` 8/16/24 ligado e desligado |
| 3 | **3.1 contorno** | a linha dos lutadores (pequena em pixels, grande à vista) | 1 (a cópia) | captura lado a lado |
| 4 | **3.6 FBA** (as duas passadas) e **3.5 DATE** (conta + prova) | a transparência do estágio e a sombra; o resto é preciso mas quase sem imagem | `COLORWRITEENABLE` por desenho | quadros de estágio com água/vidro e de sombra; contra o PC |
| 5 | **4 byte exato** (opção 2 ou 3, ou b com MRT) | a chave (transparência do lutador coberto) e ids altos | a sonda | só se os contadores da seção 7 mostrarem ids acima de 0x80 |

Se a abertura com `BT3_FX_OFF=24` mostrar que os itens 1 e 2 da triagem **não** são da máscara, o 2 cai e o 3 sobe.
Os itens 1 e 3 compartilham a cópia, então fazer o 1 já valida o contorno de graça (só falta o `ps_contorno`).

Cada passo tem de entrar com um **verificador visto falhando de propósito** (regra do projeto). O de menor custo, e que não
depende de captura: um **autoteste no boot** como o `prof_autoteste`: escrever um padrão de alfa conhecido numa área do
alvo, rodar a passada, resolver e comparar palavra por palavra contra uma função em C que repete a conta do
`outline.frag` e do `dclut.frag` (a mesma tabela, a mesma ordem), mais um controle com a CLUT trocada que **tem de dar
diferente**. O que o autoteste não prova (a mistura do jogo, as escalas do z) fica para as capturas.

## 7. Diagnósticos a acrescentar ao log antes de implementar (passo 0)

No `log_quadros` (trecho de 60 quadros), em `port/x360/` apenas, sem tocar o `gs_draw.c`:

- **Tabelas por (fonte, chave do pipeline):** `d->src`, `blend` (bits 0..8 da chave) e a máscara (bits 16..19) separam a
  chave, o brilho de objeto, a tinta e o brilho do céu sem precisar do `cbp`.
- **DATE:** marcadores 5 por quadro; quantos viraram Resolve (alvo residente) e quantos usaram a textura do alvo; a área
  e o tempo; desenhos com `misc[1] != 0` por alvo. (O log de hoje não os tem.)
- **FBA:** desenhos com `misc[2] != 0`, e destes quantos misturam por `SRCALPHA` (`blend` com `C == 0`): o tamanho do
  conserto 3.6.
- **Alfa alto:** num quadro por trecho, vértices de desenhos com máscara de alfa ligada e alfa de vértice acima de 0x80
  (laço sobre `gsVerts`; para a VU1, o `color0` do `Vu0Uniform`): o tamanho do byte exato.
- **A sonda de extensão** da seção 5: a união dos retângulos desenhados e a maior tesoura por alvo.
- **Por tabela:** `CLUT nula`, `sem Prof.tex`, `não cleared`.

## 8. O que esta especificação não provou

- Nada foi compilado nem rodado; os rascunhos de HLSL não passaram pelo `fxc`. As funções que o Xenos pode recusar
  (`tex2Dlod` com a CLUT, `fmod`) são os primeiros a conferir.
- A **primeira parte não foi medida**: o modo de z (1, 2 ou 0), o custo dos Resolves e se `tex2D` de uma D24S8 dá o
  inteiro certo são premissas do 3.3.
- A composição das cinco ou seis tabelas por quadro (seção 2) e a ordem relativa entre `BtlObj_BeginDraw` e
  `StgFx_DrawPre`; quem escreve o 0xFF do fundo; a escala de `ZBUF.PSM` das passadas de névoa; em que byte do stencil o
  restauro do z entrega o valor (opção 2 da seção 4).
- Que o FBA derruba a transparência do estágio (3.6) é leitura do código, não imagem; a sombra "preta demais" ser o
  DATE por sequência (3.5) é inferência.
- As estimativas de GPU (0,1 a 0,6 ms por passada, 3 a 5 ms por quadro) são ordem de grandeza da taxa de preenchimento e
  de Resolve do Xenos, as mesmas premissas do 07.md da primeira parte; só o console mede.
- O `ChrView` e o Shenlong não foram olhados: se emitem nativos, a lista da seção 1 cresce.
