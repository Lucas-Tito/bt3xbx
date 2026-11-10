# F8: a escala 1,4 e a primeira otimização de GPU para ela

[Índice do plano](../../plano.md) · [pendências da F8](pendencias.md), item 14 · [F5, etapa 8: 720p](../F5/08-720p.md) ·
decisões 20 e 27

Branch `claude/f8-escala` (base `main` `cf6e8aa2`), 30/10/2026. Convenção: **[medido]**, **[lido]** e **[estimado]**.
**A rodada da seção 6 rodou em 30/10** (`build/sessao_escala/coleta/`): na luta, a mediana da folga da GPU foi 8,5 ms na 1,33,
7,6 na 1,4, 10,2 na 1,4 com a `BT3_OTIM_SEMTEX` e 7,8 na 1,5 com ela; as capturas da `e14` e da `e14s` têm os 15 FNV comuns
iguais. A tabela completa, com o MSAA nas mesmas escalas, e as decisões do mantenedor estão em `10-msaa.md`, seção 8.

## 1. O pedido e o resultado

O mantenedor acha o serrilhado incômodo (o FXAA de console está ligado por padrão desde 23/10, 0,8 a 0,9 ms) e quer subir a
resolução interna "nem que seja um pouco" acima da 1,33 do release (648 x 456 do GS em 864 x 608). A 1,5 e a 1,6 não
couberam em 23/10 (35,6 e 40,5 ms de GPU na pausa com as otimizações); a 1,33 mediu 26,6 ms na pausa e tem uns 2 ms de
folga nos quadros pesados do Duelo de teste (item 14 das pendências).

- **A 1,4 entrou** pela chave, com as mesmas garantias das outras: `BT3_ESCALA=1.4` com `BT3_ALVO_AREA=640x480` (896 x
  672 na escala). O padrão continua 1,33.
- **A otimização feita não é o `BT3_ESCALA_FX`:** pelo perfil fino de 28/10, o desenho sem textura pelo shader geral pesa
  três vezes o que os efeitos de tela na escala 1 tirariam, e a mudança é só de shader, sem mexer na imagem. Ficou sob
  `BT3_OTIM_SEMTEX=1`, desligada por padrão (seção 4).
- **A rodada de console** mede a 1,33, a 1,4 sem e com a chave, e a 1,5 com a chave, pela tela por imagem (seção 6).

| Commit | O quê |
|---|---|
| `64d1920b` | `escala.h`: a 1,4 = 7/5 no fim da lista; `teste_escala.c` com a área, as recusas, as bordas, a redução e a menor área das vizinhas; `teste_afiado.c` com a 1,4 |
| `e3dcbfbd` | `BT3_OTIM_SEMTEX`: `ps_gs_semt` e `ps_gsz_semt` no `gs_d3d.hlsl`, a escolha por desenho no `ps_escolhe`, a classe no perfil e a linha no log |

## 2. As contas da 1,4 [lido no `escala.h`, medido no teste]

**A área.** O `esc_monta` exige a área na escala inteira e múltipla de 32 nos dois lados (o pitch exato da textura tiled
que o autoteste da profundidade confere; também cobre o 8 do Resolve). Com 7/5, `7 x L / 5` múltiplo de 32 pede `L`
múltiplo de 160 (7 e 160 não têm fator comum): a largura mínima de 640 serve, e a altura, que precisa de 448 ou mais, vai
a **480**. A 1,4 na área da 1,33 (648 x 456) dá 907,2 x 638,4 e é recusada. **Cuidado com a chave sozinha:** sem
`BT3_ALVO_AREA`, o `area_init` tenta a área padrão (648 x 456) na escala pedida e, como ela também não vale na escala 1
(648 não é múltiplo de 32), recusa a área e cai em 1024 x 1024 na escala 1 (`BT3_ALVO_AREA=648x456 recusada: não inteira na
escala; fica 1024x1024` e `BT3_ESCALA=1.4 recusada com a área de 1024x1024 ...; fica 1`), o caminho de antes da etapa 2 e
mais lento que a 1,33. Já era assim com a 1,5 sozinha. Os ambientes da rodada dão as duas chaves.

| | 1,33 (release) | **1,4** | 1,5 |
|---|---|---|---|
| Área do GS | 648 x 456 | **640 x 480** | 640 x 448 |
| Alvos e profundidades | 864 x 608 | **896 x 672** | 960 x 672 |
| A profundidade vista como cor | 880 | **960** | 960 |
| Tiles de uma superfície (80 x 16) | 11 x 38 = 418 | **12 x 42 = 504** | 12 x 42 = 504 |
| O par da EDRAM, de 2.048 | 836 | **1.008** | 1.008 |
| Residente: A + Z + B, e a sobra para as pequenas | 1.254, 794 | **1.512, 536** | 1.512, 536 |
| A cópia dos ids (640 x 448 na escala, em múltiplos de 8) | 856 x 600 | **896 x 632** (896 x 627) | 960 x 672 |
| A parte mostrada (512 x 448 do GS) | 683 x 597 | **717 x 627** | 768 x 672 |
| Pixels desenhados contra a 1,33 | 1 | **1,10** | 1,27 |
| Uma textura de alvo | 2.052 KB | **2.352 KB** | 2.520 KB |

O par cabe com folga sem tiling predicado, e os alvos residentes também (as pequenas da luta pedem uns 60 tiles, como na
1,5). A 1,4 usa os mesmos tiles da 1,5 e desenha 87% dos pixels dela: as 45 linhas de baixo da área (627 a 671) só existem
porque 448 não fecha. A memória dos alvos cresce uns 300 KB por textura contra a 1,33 (uns 4 MB nas 9 texturas, as 3
profundidades e a do FXAA) [estimado]; a livre mínima da 1,33 era 74 MB em 23/10.

**As vizinhas que não fecham** (a menor área de cada uma pelas mesmas regras, conferida no teste): a **1,375** (11/8) pede
largura e altura múltiplas de 256, então 768 x 512 do GS em 1056 x 704 (616 tiles: uma superfície 23% maior que a da 1,4
para uma escala menor, e 128 colunas do GS que ninguém desenha); a **1,45** (29/20) pede múltiplos de 640, então 640 x 640
em 928 x 928 (696 tiles; A + Z + B de 2.088 passa dos 2.048 e a residência se recusaria). A 1,4 é a escala entre 1,33 e
1,5 que fecha limpo, e com a largura de 640, a da imagem do jogo.

**As bordas, o meio pixel e as tesouras.** Nada novo no código: o `esc_px` (`ceil(x * s - 0,5)`, a regra do rasterizador
com os centros em .5) já é geral. Na 1,4 o pixel 0 do GS cobre o pixel de saída 0, o 1 os 1 e 2, o 2 o 3, o 3 os 4 e 5,
o 4 o 6 (1 e 2 alternados, um ciclo de 5 pixels do GS em 7); nenhum centro cai numa borda (`(2k + 1) x 5 / 14` nunca é
inteiro), ao contrário da 1,25. O `teste_escala.c` confere cada pixel de saída de 1.024 pixels do GS contra o rasterizador,
retângulos vizinhos de larguras 1 a 37 somando a largura inteira (sem fresta nem sobreposição) e a redução das capturas.

**A arte 2D com o afiado** (ligado por padrão): o modelo `teste_afiado.c` ganhou a 1,4. Na arte nítida, 0 colunas
perdidas e até 1 pixel misturado por borda, também no HUD estreitado do 16:9, que sem o afiado perderia 141 colunas nas
16 posições do modelo; a cópia para a tela (716,8 para 1280 e 627,2 para 720) com até 1 pixel misturado por borda.

## 3. Que otimização primeiro [medido no perfil fino de 28/10, seção 31 de `F5/08-gravacao.md`]

No quadro pesado do Duelo (o 6960, na 1,33), o comum líquido por classe:

| Classe | ms | Fragmentos a 4/3 | ns por fragmento |
|---|---:|---:|---:|
| `sem`: sem textura pelo `ps_gs` geral (17 desenhos; 5,32 ms deles no alvo 0, o quadro) | **5,76** | 4.720.113 | 1,22 |
| `alvo`: alvo como textura (38 desenhos) | 3,44 | 2.692.113 | 1,28 |
| `rapido textura`: `ps_gs_tex` | 1,77 | 3.781.437 | 0,47 |
| `afiado` | 1,18 | 261.161 | |
| `rapido sem`: `ps_gs_sem` | 0,14 | 142.883 | |

**O `BT3_ESCALA_FX`** desenharia na escala 1 os alvos de trabalho dos efeitos (3, 4, 5 e 6: o brilho e o desfoque). O que
eles custam no 6960, somando as classes `alvo` e `sem` neles: 3 (0,94 + 0,43), 4 (0,30), 5 (0,21) e 6 (0,15), **2,02 ms**.
Na 1,4 seriam 2,23 ms; na escala 1, 1,14: o teto do ganho é **1,1 ms**, mais uns 0,2 a 0,3 dos Resolves deles, e menos o
custo fixo por desenho dos alvos de 64 x 64. A estimativa de 3 a 5 ms de 22/10 vinha da GPU grossa de antes do perfil
fino. Além disso ele muda a imagem (o brilho borra conforme a escala, ver `05-brilho.md`), mexe nas vagas da EDRAM, no DATE
e nos Resolves de cada alvo, e contraria a escala uniforme da decisão de 22/10.

**O sem textura pelo geral** é a maior classe: com modo 0, o `ps_gs` lia a textura branca com os gradientes do
`tex2Dgrad` e jogava a leitura fora. Esses desenhos ficavam fora do especializado (`BT3_OTIM_PS`) por terem DATE, teste de
alfa ou arte 2D. Por fragmento custam 2,6 vezes o especializado com textura, que ainda lê uma textura de verdade.

| | Ganho estimado na 1,4, quadro pesado | Muda a imagem | Onde mexe |
|---|---:|---|---|
| `BT3_ESCALA_FX` | 0,6 a 1,3 ms | sim (o brilho mais largo, como na escala 1) | as vagas, os Resolves, o DATE e o uv de cada alvo |
| **`BT3_OTIM_SEMTEX`** | **1,6 a 3,9 ms** | não | o `ps_escolhe` e dois shaders |

A faixa da chave [estimado]: os 5,76 ms viram 6,35 na 1,4; se o custo seguir os ciclos do `fxc` (51 contra 75 a 80, seção
4), sai um terço (2,1 ms); se os 5,2 milhões de fragmentos da 1,4 passarem a custar o do especializado com textura (0,47
ns), 3,9 ms, o teto; a ponta de baixo, um quarto, conta os desenhos com DATE, que ainda leem a cópia do DATE, e a mistura,
que não muda. A rodada de 23/10 ensinou que o
`fxc` não prevê o ganho (o especializado tirou um quinto do teto do trivial): só o console diz.

## 4. `BT3_OTIM_SEMTEX`

**O que faz** (`ps_escolhe`, só com a chave e só nos desenhos com `mode[0] == 0`, depois do trivial e do afiado):

- sem DATE e sem teste de alfa, com ou sem arte 2D: o `ps_gs_sem` (ou `ps_gsz_sem`) que já existe, se o `BT3_OTIM_PS`
  estiver ligado (o padrão). O `misc.w` da arte 2D só mexe no uv da textura, que o modo 0 não usa;
- os outros (DATE, teste de alfa, ou todos sem o `BT3_OTIM_PS`): o **`ps_gs_semt`** (`ps_gsz_semt` na VU1 com o recorte
  pelo w), o `gs_pixel` com `semTex` verdadeiro: a leitura da textura vira zero e o uv e os gradientes, sem uso, o `fxc`
  tira. As entradas são as do `ps_gs` e do `ps_gsz` (o `stq` chega e não é lido: o aviso X3596 do `fxc`, como nos
  especializados).

A chave fica **fora do `BT3_OTIM`** (que é ligado por padrão) e lê `BT3_OTIM_SEMTEX` com padrão 0. O boot diz
`F8, desenhos sem textura sem a leitura da branca (BT3_OTIM_SEMTEX=1): ...` e cada trecho de 60 quadros
`F8, sem textura, por quadro: N pelo ps_gs_semt (N com DATE, N com teste de alfa), N com arte 2D pelo especializado`. No
perfil (`BT3_COMUM_GPU`), o que vai ao `ps_gs_semt` continua na classe `sem` e o que vai ao especializado, na `rapido sem`.

**Por que não muda a imagem.** No `gs_pixel`, com `modo.x` 0, a leitura `t` só entra no ramo `if (modo.x != 0)`, que não
é tomado: a cor sai `cor.rgb`, o alfa `cor.a * k`, depois o teste de alfa e o FBA. No `ps_gs_semt` o DATE, o teste de
alfa e o FBA são o mesmo texto. **O ramo da função da textura ficou de propósito:** na primeira versão, sem ele, o `fxc`
juntou o `a = cor.a * k` com o `a * 128 + 0,5` do teste de alfa num `mad` só (`cor.a * 255 + 0,5`), outra ordem de contas
(igual em aritmética exata, mas não garantida no float se o `mad` do Xenos arredondar uma vez só). Com o ramo, a listagem
do `ps_gs_semt` tem o `mul r0.x, r1.w, 1,9921875` e o `mad r1.y, r0.x, 128, 0,5` do geral, e a leitura do DATE com as
mesmas contas (`floor`, `+ 0,5`, `* alvo.xy`, `tfetch2D`, `>= 0,998`, `kill`). Só o console prova que o Xenos dá o mesmo
byte: a abertura opcional `e14`/`e14s` da seção 6 compara os FNV-1a das capturas.

**A estimativa do `fxc`** (ciclos por vetor de 64 pixels):

| Shader | Ciclos | Instruções | Leituras de textura |
|---|---:|---:|---:|
| `ps_gs` (geral) | 74,7 a 80 | 56 a 60 | 2 (a textura e o DATE), 20 ciclos |
| **`ps_gs_semt`** | **50,7** | 38 | 1 (o DATE), 4 ciclos |
| `ps_gsz` | 77,3 a 82,7 | 58 a 62 | 2 |
| **`ps_gsz_semt`** | **53,3** | 40 | 1 |
| `ps_gs_sem` | 16 (o interpolador) | 5 | 0 |

## 5. As provas [medido]

- **Build do 360** (`build_f2.sh` no `e3dcbfbd`, carimbo `e3dcbfbd diff:da39a3ee5e`): 317 de 317 fontes sem erro, os
  avisos iguais aos da main por fonte (985 nos do jogo, 8 nos do 360, nenhum no `gs_d3d.cpp`), os mesmos 103 stubs
  (`stubs.txt` igual), `default.dat` `4fe9d0495c144043fdccda41281655fa` e `default.swap`
  `2e97a3e533accf0ae852e4d25df5283a` iguais aos da main; **`default.xex` `5e6741db7b983fa25b5cca14b96b8919`** (1.511.424
  bytes, 4 KB a mais: os dois shaders), `default.map` `7decb14fe608f9f42ed2bbb3e4cfa789`. A main (`cf6e8aa2`), na mesma
  máquina: `default.xex` `1bb4032e7c8f0ae338b87b21925a3b89`.
- **Os shaders de antes saem iguais:** os 25 headers do `fxc` da main são byte a byte os do branch (o mesmo caminho de
  saída, que o header traz no comentário), inclusive o `ps_gs`, o `ps_gsz` e o `ps_gs_af`, que agora passam `false` ao
  `semTex`. Sem a chave a GPU roda o mesmo microcódigo. **O controle:** a primeira versão, com o bloco da textura dentro de
  um `if (!semTex)`, mudou o `ps_gs`, o `ps_gsz` e o `ps_gs_af` (um `mul` virou `mulsc`, uma instrução saiu do par): a
  comparação enxerga a diferença, e foi por ela que a leitura virou um `?:` na própria linha.
- **Os objetos** (`xdk_objetos.py` do branch `claude/f5-lotes`, a desmontagem do `link /dump /disasm` normalizada): os 317
  `obj/*.obj` e os 30 `x360/*.obj` além do `gs_d3d.obj` com a mesma desmontagem da main; o `gs_d3d.obj` diferente, como
  esperado (o primeiro ponto é o `cmpwi cr6,r3,6` da lista das escalas, que virou 7), e no `.map` uma função só nova,
  `comum_classe`, que deixou de ser inline. **O controle:** `--mutantes` recusa a instrução trocada, o registrador trocado,
  a instrução a menos, o rótulo falso e a função a mais no `.map`; e sem `--espera-diferente` a comparação sai com 1
  apontando o `gs_d3d.obj`. **Nada portável mudou** (`git diff --name-only main` só tem `port/x360/` e `docs/x360/`): a
  regra do upstream (`PORT_X360` com o original no `#else`) não teve onde entrar.
- **Sem a chave, o `gs_d3d.obj` faz o de antes** [lido]: o `sSemTex` só vale 1 com `BT3_OTIM_SEMTEX` diferente de 0; os
  três lugares novos (`ps_escolhe`, `comum_classe` e o `else if` do `prepara`) começam por ele, e os shaders novos só são
  criados com ele. A 1,4 só muda a leitura de `BT3_ESCALA` com o texto `1.4` ou `1,4`, que antes era recusado.
- **O teste das contas** (`gcc -std=c89 -pedantic -Wall -Wextra -Werror`): **86 casos, 0 falhas** (eram 72). Novos: 640 x
  480 na 1,4 em 896 x 672 com 504 tiles, 800 x 640 em 1120 x 896 com 784; as recusas (640 x 448, 648 x 456 e 640 x 512 na
  1,4; 960 x 800, que não cabe na EDRAM; 1,375 e 1,45 fora da lista); a leitura de `1.4` e `1,4` e a recusa de `1.40`,
  `1.45`, `1.375`, `1.4 ` e `1.`; as bordas 1, 3, 4, 6 e 7; o `esc_gs`; cada pixel de saída contra o rasterizador; os
  retângulos vizinhos; a redução; a menor área da 1,4, da 1,375 e da 1,45. **Os controles saem com 1:** `--estraga` (61 de
  86 falham); uma cópia do `escala.h` com a regra `floor` (`x * num / den`): 12 falhas, 3 delas na 1,4; com o tile de 64
  colunas: 15, 3 na 1,4; com a 1,4 trocada por 7/4 na lista: 4, todas da 1,4; com a borda `+ den` no lugar de `+ den - 1`
  (o caso do centro na borda): 7, nenhuma na 1,4, que não tem centro na borda (o que se espera dela).
- **As escalas de antes têm os mesmos números:** um programa que imprime o `esc_monta` (áreas de 600 a 1.100 em passos de
  8), o `esc_px` e o `esc_gs` (0 a 4.096), o `esc_nome` e a leitura de 17 textos nas seis escalas antigas sai igual com o
  `escala.h` da main e com o novo (57.869 linhas); o controle, uma cópia com a borda trocada, sai diferente.
- **O modelo do afiado** (`teste_afiado.c`): 163.200 casos, 0 falhas (eram 129.719); o `--estraga` sai com 1 (64.653
  falhas).

## 6. A rodada no console (preparada)

Em `build/sessao_escala/` do repositório principal: `default.xex`, `.dat`, `.swap` e `.map` do `e3dcbfbd`, `f5_duelo.txt`
e `boot.txt` (cópias dos de `build/sessao_2810/gpu`, iguais aos de `port/x360/roteiros/`), um `ambiente_<nome>.txt` por
abertura e o `ordem.txt`. Cada ambiente é o `ambiente_base1.txt` de `build/sessao_2810/gpu` (o release com o `f5_duelo`
pelo roteiro, 16:9, FXAA, `BT3_PICOS=36`, `BT3_PICOS_TELA=1`, `BT3_TEMPOS=0`; todas as chaves dele existem na main), mais
`BT3_ISO=game:\bt3-x360.iso`, a escala, a chave e saves próprios (`game:\sv_esc_<nome>`), com o que mede no comentário do
topo. O `environment.txt` é a cópia do `ambiente_a133.txt`. Uns 4 a 5 minutos por abertura.

| # | Arquivo | Escala (área) | Chave | `BT3_TEMPOS` | Para quê |
|---|---|---|---|---|---|
| a | `ambiente_a133.txt` | 1,33 (padrão) | | 0 | a referência |
| b | `ambiente_b14.txt` | 1,4 (640x480) | | 0 | o custo da escala |
| c | `ambiente_c14s.txt` | 1,4 (640x480) | `BT3_OTIM_SEMTEX=1` | 0 | **o critério da 1,4** |
| d | `ambiente_d15s.txt` | 1,5 (640x448) | `BT3_OTIM_SEMTEX=1` | 0 | só se a c passar com folga |
| opcional | `ambiente_bt14.txt`, `ambiente_ct14s.txt` | 1,4 | sem e com | 1 | a GPU por classe nos três trechos |
| opcional | `ambiente_e14.txt`, `ambiente_e14s.txt` | 1,4 | sem e com, `BT3_SHOT=600` | 0 | as imagens iguais |

```
python3 port/x360/tools/subir.py build/sessao_escala/default.xex build/sessao_escala/default.dat \
    build/sessao_escala/default.swap build/sessao_escala/environment.txt build/sessao_escala/f5_duelo.txt \
    build/sessao_escala/boot.txt build/sessao_escala/ambiente_*.txt
# abertura a; depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log --para build/sessao_escala/a133
python3 port/x360/tools/subir.py --nome environment.txt build/sessao_escala/ambiente_b14.txt
# abertura b; baixar o log para build/sessao_escala/b14; o mesmo com cada linha do ordem.txt
```

As capturas da `e14` e da `e14s` ficam no console (`game:\shots`): só o FNV-1a de cada uma vem no log, e nenhuma imagem do
jogo vem para cá.

**O que olhar no boot** (b, c e as outras da 1,4): `área dos alvos 640x480 do GS (BT3_ALVO_AREA), escala 1,4: alvos e
profundidades de 896x672, a profundidade vista como cor com 960 de largura, a cópia dos ids de 896x632`; `pronto; alvo
896x672, EDRAM: cor 504 + profundidade 504`; o autoteste da profundidade no modo 1 exato com 602.112 pixels; as nativas
65.536 de 65.536; `EDRAM residente ... 896x672 e 504 tiles cada; as pequenas de 1512 a 2048 (536 tiles)`; na c, a linha
`F8, desenhos sem textura sem a leitura da branca (BT3_OTIM_SEMTEX=1)`. Na d, o mesmo com 960x672 e 645.120 pixels. Em
todas: `roteiro: resultado estado 4 vencedor 0x2 razao 0x1 vida 0 27160` (o de 23/10), nenhum `roteiro: FALHA`, `vigia:`
nem `CRASH`, e `além da área ...: 0 desenhos e 0 nativas cortados` em todos os trechos.

**Como ler a GPU por quadro** (com `BT3_TEMPOS=0`, sem as marcas que atrasam a tela):

- **`picos: tela, janela: ...`**, uma por janela do `passos` (duas na luta): `imagens de fato por vblanks na tela 1 N, 2 N,
  3 N, 4 ou mais N` e a `folga até o prazo ... da GPU acabando o quadro: mínimo, p1, p10, mediana`. A folga é quanto antes
  do segundo vblank depois da imagem anterior a GPU acabou o quadro seguinte: negativa, a imagem fica 3 vblanks. Contra a
  a133, a queda da mediana é o custo da escala na GPU (o que a CPU leva não muda).
- **`picos: tela: a imagem do quadro N ficou K vblanks ...`**, uma por imagem que não ficou 2 vblanks: o `a GPU acabou X
  ms` (+ antes do prazo, - depois) e o Present. Uma imagem longa com a GPU negativa e o Present positivo é da GPU.
- **`picos: quadro N ...`**, uma por quadro com a apresentação acima de 36 ms: `GPU: fim X depois da CPU, período Y`. Com
  a GPU limitando, o período é o tempo de GPU daquele quadro.
- **A GPU dos trechos** (só nas opcionais com `BT3_TEMPOS=1`): na luta há três relatórios de 60 quadros (o começo, perto do
  quadro 4.619; a pausa, perto do 5.399, o mais pesado; o fim, perto do 7.799). Em cada um, `tempos, GPU por quadro (...):
  ... soma das classes S` é a GPU sem as marcas (os 26,6 ms da pausa na 1,33 da decisão de 23/10), e `tempos, GPU por
  classe` dá o `comum sem` e o `comum rapido sem` que a chave move. A `ct14s` contra a `bt14`, trecho a trecho (os
  desenhos por quadro iguais confirmam o mesmo conteúdo), é o ganho medido da chave.

**A referência que já existe** (as três aberturas `base` de 28/10, a 1,33 do release, `build/sessao_2810/coleta/gc_base*`)
[medido]: na janela 1, 684 imagens, todas de 2 vblanks, folga da GPU mínimo 4,6, p1 5,4, p10 6,0, mediana 8,4 ms; na
janela 2, 2.335 imagens, **2 de 3 vblanks** (a troca 6959 para 6960, o problema conhecido da F5, seção 37 de
`08-gravacao.md`), mínimo -9,8, p1 2,3, p10 6,3, mediana 8,4. As três repetições batem em 0,1 ms. **A 1,33 já não cumpre o
zero da decisão 27 na janela 2**, por um quadro que nenhuma escala cura.

**O critério:** a escala passa se a **janela 1 fica com zero imagens de 3 vblanks e a janela 2 com as mesmas 2 da a133, e
nenhuma a mais** (decisão 27, descontado o 6960 que a escala não causa), com o **p1 da folga da GPU positivo nas duas**; e,
nas opcionais, **a soma das classes da pausa abaixo de 33,4 ms** (com uns 2 ms de margem para os quadros mais pesados que
a média do trecho). A queda da mediana da folga contra a a133 é o custo da escala na GPU: a estimativa para a 1,4 sem a
chave é de uns 2,5 ms [estimado: 10% mais pixels sobre uns 25 ms de GPU], o que leva o p1 da janela 2 para perto de 0 e
deve criar imagens longas; com a chave, 1,6 a 3,9 ms de volta. A b diz se a 1,4 já cabe sozinha; a c, se cabe com a chave;
a d, se a chave leva também a 1,5 (que mediu 35,6 ms na pausa em 23/10, antes da residência e do FXAA: improvável). Nas imagens: as linhas `captura gpu_NNNNN.ppm: fnv ...` da `e14` e da `e14s` iguais nos
quadros que as duas têm (`grep 'captura gpu_' | sort`); se alguma diferir, conferir a fase do quadro antes de culpar a
chave, e o `compara_quadros.py` no WSL diz onde.

**O que olhar na tela:** na 1,4 o 3D um pouco mais nítido que na 1,33 e o serrilhado menor depois do FXAA; o texto e o HUD
pelo afiado como na 1,33; o brilho e o desfoque sem caixa nem borda nova.

### A ISO

A main lê a marca da ISO convertida na versão **2** (`ISO_MARCA_VERSAO` em `port/x360/src/iso360.h`); a versão 3 só está no
`14ea9f97`, fora da main. Com a ISO versão 3 do console, esta build recusa a `game:\bt3-x360.iso` (`iso: conversão versão
3, esperada 2: recusada como não convertida`): **é preciso ajustar antes da rodada** (a ISO versão 2 no console, ou a
versão 3 trazida para o branch e uma build nova).

## 7. O que fica para depois

- **O alvo como textura especializado** (a classe `alvo`, 3,44 ms no 6960): o mesmo argumento do especializado de 23/10
  (`tex2D` no lugar do `tex2Dgrad`, um nível, sem filtro de mip), mas a leitura continua; o ganho é menor e mais incerto.
- **O `BT3_ESCALA_FX`**, se a 1,4 couber e a 1,5 ficar perto: até 1,3 ms, com a imagem do brilho mudando e uma decisão do
  o mantenedor sobre a escala uniforme.
- **A escala padrão:** só com a rodada. Se a c passar, a 1,4 com a chave pode virar o padrão (as duas linhas no
  `area_init` e o padrão da chave), com o FNV das capturas iguais e uma decisão nova.
