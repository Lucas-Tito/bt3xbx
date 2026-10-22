# F5, etapa 8: para onde vai o tempo da thread de gravação

Depois dos degraus 1 e 2 (`08-threads.md`, `08-degrau2.md`), a luta no console está em **38,4 ms** de mediana (desenho na
thread de hardware 3) e o período é o da **thread de gravação**: andar a cadeia de DMA e o VIF, o GIF e os envios de textura,
o lado portável do GS (`gs_core.c`, `gs_draw.c`), a gravação da VU1 (`GsVu1_Call`) e a entrega do retrato. Esta nota é só o
inventário: **nada foi otimizado**. Diz quanto cada família pesa (o que o log do console mede e o que vem da proporção da x86) e
dá a lista ordenada do que cortar. Meta: 33,37 ms (30 fps), com margem; faltam uns 5 ms.

Marcas: **[medido]** sai do log do console ou do perfil; **[estimado]** é conta sobre medidas, com a conta escrita.

## 1. O que o log do console mede (conferido no código)

O log é o da abertura 1 do degrau 2 (`.xex` do `c79548b`, desenho na thread 3, 21/10/2026).

| Linha do log | O que mede | Onde |
|---|---|---|
| `quadro inteiro` | com a divisão, o período do `frame_end` na thread de **desenho**, de ponta a ponta; em regime é o período do pipeline | `gs_d3d.cpp:3319-3328` |
| `fim do quadro` | o trabalho do desenho no quadro | idem |
| `jogo e gravação` | `quadro inteiro` menos `fim do quadro`: com a divisão é **a espera do desenho pelo retrato seguinte**, não o tempo da gravação | `gs_d3d.cpp:2520` |
| `VU1, lotes ... gravação X ms` | a soma, por quadro, do tempo dentro do `GsVu1_Call` (tique antes e depois), levada no retrato (`vu1Ms`). Inclui o que ele chama: `hle_program*`, `GsGpu_DrawVu0/4/6`, `draw_state`, `texture_get`, o `Gs_PageHash` e a criação de textura nova. **Não** inclui o `GsVu1_Unpack` (fora do `GsVu1_Call`) | `gs_vu1.c:722-739`, `gs_draw.c:3826` |
| `gs: divisão: ... entrega média` | a média **desde o boot** do tempo do `desenho_entrega`, contado depois da espera pelo desenho: as cinco cópias das listas para o retrato e a entrega do backend (`vu_plano`, `prof_garante`, `alvos_sem_textura`, `mem_mede`, `sincroniza_alvos`). O `pior` é do trecho | `gs_draw.c:3763-3862`, `gs_d3d.cpp:3419-3441` |

Duas armadilhas: a **entrega média é cumulativa** (o valor do trecho sai da diferença das somas: média × retratos) e o log do
`gs_d3d` **fica mudo durante a janela do `passos`** (`log_quadros` volta cedo com `Passos_Medindo()`): a janela da luta que deu
os 38,4 ms (vblank 3984 a 11438, 3.727 passos) não tem trecho de 60 quadros. Os trechos com lutadores estão antes de a janela
abrir e depois de ela fechar (a pausa).

## 2. O rateio no console

### 2.1 Os trechos com lutadores [medido]

Trechos de 60 quadros com mais de 20 mil desenhos de lutador (`vu0`); `entrega` do trecho pela diferença das somas; `resto` =
`quadro inteiro` − VU1 − entrega (andar a cadeia, VIF, GIF e envios, `GsVu1_Unpack`, os desenhos do GIF, e espera, se houver).
Os valores são médias do trecho (com os picos), não medianas.

| Trecho (quadro) | Lutadores por quadro | Período (média) | Gravação da VU1 | Entrega | Resto |
|---|---:|---:|---:|---:|---:|
| 2759 | 1.661 | 47,8 | 21,1 | 4,7 | 22,0 |
| 2939 | 2.152 | 47,5 | 23,1 (pior 23,3) | 5,8 | 18,6 |
| 2999 | 1.582 | 47,4 | 17,2 | 4,3 | 25,9 |
| 3179 | 1.981 | 41,3 | 17,3 | 4,7 | 19,3 |
| **a janela da luta (3.780 retratos)** | | **38,4 (mediana)** | sem log | **4,81** | |
| 6959 e 7019 (depois da janela: a pausa) | 2.236 | 58,2 e 58,4 | 23,0 e 23,0 | 7,1 (o 7019) | 28 a 30 |

A entrega da janela inteira é a única medida de dentro dela: **4,81 ms por quadro**, a média de 3.780 retratos (a janela e uns
50 quadros de pausa). É cara: na x86 a mesma entrega é uns 9% da gravação (seção 3), no console é 12,5% do período.

### 2.2 Os 38,4 ms por família

Nos trechos que mais se parecem com a luta (2759, 2939, 3179) a VU1 é de 42 a 49% do período e o resto de 39 a 47%. Aplicado à
mediana da janela:

| Família | ms por quadro | Como |
|---|---:|---|
| Gravação da VU1 (`GsVu1_Call` com o que chama) | **16 a 19** | 42 a 49% de 38,4, a proporção dos trechos vizinhos [estimado sobre medido] |
| Entrega do retrato | **4,8** | média da janela [medido] |
| O resto (cadeia, VIF, GIF, envios, `Unpack`, desenhos do GIF) | **14,6 a 17,6** | a diferença [estimado sobre medido] |

Dentro de cada parcela medida, a divisão vem da proporção da x86 (seção 3), **dentro da mesma parcela** (a VU1 pela proporção
dentro da VU1 na x86, o resto pela proporção dentro do resto). Assim um erro da x86 sobre o Xenon numa família não vaza para a
outra. Tudo abaixo é **[estimado]**:

| Família | x86, % da gravação | Console, ms por quadro | Onde |
|---|---:|---:|---|
| **Envios de textura** (`transfer_data`, o laço do PSMCT32) | 19,9 | **5,2 a 6,3** | `gs_core.c:774-779` |
| **Hash de página** (`Gs_PageHash`, o recálculo de 8 KB) | 9,7 (8,2 na VU1, 1,5 no GIF) | **4,4 a 5,3** | `gs_core.c:276-283`, pedido em `gs_draw.c:266-268` |
| **Entrega** | 8,9 | **4,8** [medido] | `gs_draw.c:3790-3796` e `gs_d3d.cpp:3419` |
| **Paletas e decodificação** (`vram_rw` por `Gs_VramRead`) | 13,4 | **4,1 a 5,0** | ver abaixo |
| . o `clut_texture` da tabela pela névoa e pelos ids | 6,1 | 1,6 a 1,9 | `gs_draw.c:464-474`, de `depth_clut` (`:619`) |
| . o `clut_hash` (a paleta de cada textura de 4 e 8 bits) | 3,9 | 1,3 a 1,6 | `gs_draw.c:132-150` |
| . a decodificação de textura nova | 3,4 | 0,9 a 1,1 | `gs_draw.c:376-383` |
| **Montagem dos desenhos da VU1** (`GsGpu_DrawVu0/4`, a cópia dos vértices, `hle_program*`, `memcmp` das constantes) | 14,6 | **7,1 a 8,5** | `gs_draw.c:1132-1191`, `gs_vu1.c:558-720` |
| . os índices da faixa (`gsVuIdx[...]`) e o teste do `layer 4` | 2,3 | 1,1 a 1,3 | `gs_draw.c:1164-1175` |
| . a cópia dos vértices para `gsVuVerts` | 5,9 | 2,9 a 3,4 | `gs_draw.c:1177` |
| **Estado do desenho** (`draw_state`, `texture_get` sem hash e sem decodificação, `pipeline_get`, `same_state`, `GsDraw_TargetGet`) | 9,5 (5,7 na VU1, 3,8 no GIF) | **3,8 a 4,5** | `gs_draw.c:240-785` |
| **`GsVu1_Unpack`** (dois terços são a cópia `V4-32` para a memória da VU) | 8,7 | **2,3 a 2,7** | `gs_vu1.c:130-249`, a cópia em `:146` |
| **GIF: etiquetas, registradores e primitivas** (`Gs_Gif` fora do envio, `reg_write`, `vertex`, `GsGpu_Draw`) | 6,7 (mais 1,3 de GIF dentro da VU1) | **2,4 a 2,9** | `gs_core.c:589-706`, `:822-950` |
| **Andar a cadeia e o VIF** (`run_chain`, `vif`, `GsVu1_UnpackWords`) | 4,0 | **1,0 a 1,3** | `gs_core.c:951-1035`, `:1763` |
| Só do backend falso (`bytes_walk` e seu `memset`, `fake_*`) | 2,9 | 0 (não existe no 360) | `gs_core.c:1712`, `gpu_falso.c` |

(As linhas com "." são parte da linha de cima. Os centros das faixas somam uns 38,2.)

**Esperas.** Na x86 a thread de gravação ficou 86% do tempo na CPU (14% esperando o jogo, que lá corre quase no mesmo passo). No
console a gravação é o estágio mais lento (o jogo tem uns 16 a 20 ms, o desenho uns 15), então em regime ela não espera ninguém,
salvo a espera da entrega pelo desenho, que fica fora do tempo da entrega e não aparece no log: com 15 ms de desenho contra 38 de
período, deve ser perto de zero [estimado].

## 3. O perfil da x86 [medido]

**Como.** Main em `91d076f` levada ao WSL por bundle, `headless.py --alvo x86 --gpu --vu1` (a `x86gv`). Roteiro `f5_duelo` com
`BT3_GS_THREAD=1 BT3_GS_DESENHO=1` e o ambiente do `vu1_perfil.sh`, sob `perf record -e cpu-clock` ligado **só na luta**: um
leitor da saída (com `stdbuf -oL`) manda `enable` ao `--control` do perf na linha `roteiro: seq 3` (o combate controlável, quadro
4672) e `disable` na `roteiro: seq 4` (o fim, quadro 7757). São 3.085 quadros, uns 67 deles de pausa (98% de luta). Duas rodadas,
uma de cada vez:

1. **A plana** (`x86gv`, 4 kHz, sem pilha): 28.800 amostras da thread de gravação. É dela toda porcentagem própria desta nota.
2. **A com pilha** (uma cópia da `x86gv` com `-fno-omit-frame-pointer`, 1,5 kHz, `--call-graph fp`): 13.075 amostras, usada só
   para repartir o que é compartilhado (o `memcpy`, o `draw_state`, o `Gs_PageHash`, o `vram_rw`) entre VU1 e GIF. O hash do jogo
   saiu igual nas duas builds (`08a98b6d05b8775a`).

O perf deste WSL não tem unwinder (sem pilha `dwarf`) e não resolve os símbolos do binário ligado em `0x20000000`; os endereços
foram resolvidos pelo `addr2line` (o caminho está no guia de port, em "O perfil da thread de gravação no WSL").

**Tempos da janela** (`/proc/PID/task/*/stat` nas duas pontas): 8,47 s de parede, **2,75 ms por quadro**; CPU da gravação 7,25 s
(86%, **2,35 ms por quadro**), do jogo 7,37 s (87%), do desenho 3,54 s (42%, quase todo o hash de lista do falso). A razão
console sobre x86 na gravação é de uns 16 vezes; por família: a VU1 de 20 a 26 vezes, a entrega 23, o resto de 11 a 13. **O
Xenon perde mais na VU1 e na cópia do que no resto**, e por isso o rateio da seção 2 escala cada parcela à parte.

**Por função (a plana), % da thread de gravação:**

| Função (o `transfer_data`, a `texture_get` e o `clut_texture` em linha, separados pela faixa de linhas) | % |
|---|---:|
| `memcpy` (de onde, abaixo) | 21,2 |
| `transfer_data`, em linha no `Gs_Gif` (`gs_core.c:777` 10,9 e `:778` 8,3) | 19,9 |
| `Gs_PageHash` (`gs_core.c:281` 6,9, `:282` 1,7) | 9,7 |
| `vram_rw` | 8,3 |
| `GsGpu_DrawVu0` (`gs_draw.c:1173` 1,6) | 5,2 |
| `draw_state` | 4,1 |
| `vif` | 3,6 |
| `Gs_Gif` (fora do envio) | 3,1 |
| `Gs_VramRead` | 3,0 |
| `texture_get` (em linha no `draw_state`) | 2,1 |
| `GsVu1_Unpack` | 1,8 |
| `pipeline_get` 1,3; `vertex` 1,2; `clut_texture` 1,2; `reg_write` 1,0; `GsGpu_DrawVu4` 1,0 | 5,7 |
| o resto (abaixo de 1% cada; o falso: `fake_pipe_get` 0,9, `fake_tex_upload` 0,7, `bytes_walk` 0,6, `memset` 0,5) | 12,3 |

**De onde vem o que é compartilhado (a com pilha):**

- **`memcpy`**: 40% é a **entrega** (o retorno cai no `run_chain:1821`, a chamada do `GsGpu_FrameEnd`, onde o `desenho_entrega`
  está em linha), 28% é o **`GsVu1_Unpack`** (`vif:979`; a cópia `V4-32` de `gs_vu1.c:146`), 28% são os **vértices da VU1**
  (`hle_program0/2/4` e `GsVu1_Call`: o `memcpy` do `GsGpu_DrawVu0`), 4% outros. O `memcpy` não monta quadro, então a pilha
  mostra o chamador do chamador; a linha do retorno diz qual chamada era.
- **`Gs_PageHash`**: todo pelo `texture_get` (`gs_draw.c:267`); 85% dentro da VU1, 15% nos desenhos do GIF.
- **`vram_rw` e `Gs_VramRead`**: 43% do `clut_texture` (`gs_draw.c:472`), 29% do `clut_hash` (`:141`), 26% da decodificação
  (`:376` e vizinhas); 21% dentro da VU1, 79% no GIF.
- **`draw_state`** (com o que tem em linha): 61% dentro da VU1, 39% no GIF.
- **`GsVu1_Call` com tudo o que chama**: 33% na build com pilha; na plana, somando as partes, uns 32,5%.

**O `transfer_data` é quase todo o PSMCT32** (linhas 777 e 778: uma consulta de tabela e uma cópia de 4 bytes por pixel); os
caminhos de 8, 4 e 16 bits e o genérico somam menos de 0,5%. O jogo reenvia uns 2,27 MB de imagem por quadro de luta
(`08-threads.md`, degrau 1, 1a).

## 4. O que a x86 distorce em relação ao Xenon

1. **Memória.** A x86 do WSL tem cache grande e execução fora de ordem: a VRAM de 4 MB, as listas e os retratos cabem quase
   inteiros no L3. O Xenon tem 1 MB de L2 para os três núcleos (o jogo e o desenho disputam), é em ordem e para em cada falta de
   cache. Tudo o que anda megabytes por quadro pesa **mais** no console do que a x86 diz: a entrega (4,4 MB copiados), o envio
   (2,27 MB espalhados pela VRAM), o hash (2,8 MB lidos da VRAM), a cópia dos vértices. A razão da entrega (23 vezes contra 16 da
   média) já mostra isso.
2. **Em ordem, sem renomear.** Laços curtos com dependência (o índice do `transfer_data`, a cadeia de multiplicações do hash) e
   desvios mal previstos (o `draw_state`, o `same_state`, o `switch` do `vif`) custam mais no Xenon. Os carregamentos logo depois
   de uma gravação no mesmo endereço (load-hit-store) e as trocas de inteiro para ponto flutuante pela memória (`(float)` das
   constantes no `GsGpu_DrawVu0`) também, e não aparecem na x86.
3. **A multiplicação de 64 bits.** A x86 de teste é de 32 bits (`clang -m32`): o `Gs_PageHash` usa o mesmo laço do `#else` que o
   `cl` do 360 (sem `__int128`), mas cada multiplicação de 64 bits vira três de 32. No Xenon é uma `mulld`, nativa porém lenta e
   pouco encadeada. O peso relativo do hash pode ir para qualquer lado.
4. **Sem SMT.** No console a gravação está na thread de hardware 0, com o diário na irmã (leve); o jogo divide o núcleo com o
   desenho. Na x86 cada thread tem um núcleo.
5. **O que só existe de um lado.** No 360 a entrega do backend (`vu_plano` lendo os 128 mil índices, `GlobalMemoryStatus`,
   `prof_garante`, `sincroniza_alvos`) e a criação de textura pelo XG (no `texture_get` da gravação) não estão no perfil; na x86,
   o falso (2,9%) e o `bytes_walk` não existem no 360.
6. **A cena.** O `f5_duelo` (1P contra COM pelo roteiro) não é a luta jogada à mão no console; o número de desenhos por quadro
   pode diferir. A proporção entre famílias é mais estável que o total.
7. **A build com pilha** infla o `transfer_data` (19,9% para 30,6%: o i386 perde um registrador no laço quente); por isso ela só
   reparte, não mede.

## 5. Os candidatos a corte, em ordem

O ganho é sobre a faixa da seção 2.2. "Prova" segue o padrão das otimizações anteriores: a lista igual quadro a quadro na x86 e na
PPC (`vu1_perfil.sh lista` e `compara`; `divergencias.py --normal` quando a mudança mexe em quantas texturas se decodificam), o
verificador visto falhando com a mudança estragada de propósito, a `.text` da x86 plana igual (a mudança em
`#if defined(PORT_BIG_ENDIAN) || defined(BT3_TESTE_VU1)`, medida na `x86gv`) e, no fim, a linha do log do console.

### 1. A entrega sem cópia (listas em dois buffers)

- **O que é:** `desenho_entrega` (`gs_draw.c:3790-3796`) copia as cinco listas (uns 4,4 MB por quadro de luta: 53.618 vértices
  da VU de 48 bytes, 128.772 índices, 2.215 constantes de 416 bytes, desenhos e vértices de CPU) para o retrato. Com dois
  conjuntos de listas que se alternam, a entrega troca os ponteiros em vez de copiar; o "não coube" (o caminho em série que já
  existe) continua para o quadro que passar da capacidade.
- **Ganho no 360:** **3,5 a 4,5 ms**. A entrega mede 4,8 ms; na x86, 94% dela é a cópia. A parte de cima supõe que quase tudo é
  cópia também no Xenon; a de baixo deixa até 1,3 ms para a entrega do backend (`vu_plano` e companhia), que não se mediu à parte.
- **Risco para a lista:** baixo. Os bytes que o desenho lê são os mesmos; muda de quem é cada buffer. O risco real é de
  **memória**: um segundo conjunto do tamanho do retrato custa 10,3 MB, e a luta já anda no piso (54 MB livres, mínimo 31, 43
  pressões na abertura 1). O caminho neutro é encolher as listas vivas para a capacidade do retrato (hoje elas têm a capacidade
  máxima, dezenas de MB) e usar dois conjuntos desses: a memória cai em vez de subir. É a decisão de capacidade que a
  recomendação do `08-threads.md` já deixou para o degrau 2.
- **Esforço:** 1,5 a 2,5 dias (as listas são globais do `gs_draw.c`; o falso e o `gs_d3d.cpp` leem pelo retrato e não mudam).
- **Prova:** lista igual quadro a quadro (as seções e o hash normal), as contagens de "não couberam" iguais, e no console a
  `entrega média` do trecho caindo para perto de zero. Antes, uma linha a mais no log separando a cópia da entrega do backend diz
  quanto da faixa é de fato da cópia.

### 2. O envio PSMCT32 por grupos de 8 pixels

- **O que é:** `transfer_data`, `gs_core.c:776-779`: hoje cada pixel faz `sIn32[y & 31][x & 63]`, o deslocamento da página e uma
  cópia de 4 bytes. Numa linha de bloco, 8 pixels seguidos (x múltiplo de 8) caem em `o, o+1, o+4, o+5, o+8, o+9, o+12, o+13`
  palavras (`kCol32`, `gs_core.c:73`): **quatro pares contíguos de 8 bytes**. O laço por grupos faz uma consulta e quatro cópias de
  8 bytes por 8 pixels, com as pontas fora do grupo pelo laço de hoje. A origem (o pacote do jogo, alinhado a 16 bytes) fica
  alinhada a 8 quando o grupo começa num pixel par.
- **Ganho no 360:** **2 a 3,5 ms** (de 40 a 60% dos 5,2 a 6,3 ms; as instruções caem umas 3 a 4 vezes, o tráfego de memória não
  muda, e no Xenon parte do custo é escrita espalhada pela VRAM).
- **Risco para a lista:** nenhum se a VRAM sair igual byte a byte; nada na lista depende de como o envio escreve.
- **Esforço:** 0,5 a 1 dia.
- **Prova:** a lista e o hash do jogo iguais na x86 e na PPC; um teste à parte (headless) que compara a VRAM depois de cada envio
  com o laço antigo e com o novo, em todos os envios de um roteiro, visto falhando com um par trocado.

### 3. O hash de página pela origem do envio

- **O que é:** o resto do hash de página (`Gs_PageHash`, `gs_core.c:276-283`, pedido pela `texture_get` em `gs_draw.c:266-268`):
  351 páginas de 8 KB relidas da VRAM por quadro de luta, 79% delas com conteúdo novo de fato (a textura seguinte do ciclo do
  estágio). A chave pela origem: quando um envio cobre páginas inteiras, o `transfer_data` calcula o hash da página **enquanto
  escreve** (os dados já estão no registrador) e o grava com a geração nova; o `Gs_PageHash` só refaz o laço de 8 KB nas páginas
  que um envio cobriu em parte, ou que o GS escreveu como alvo. O hash pela ordem do envio difere do hash pela ordem da VRAM, mas
  só precisa ser **sempre o mesmo para o mesmo conteúdo chegado pelo mesmo caminho**; um conteúdo igual que chega pelo outro
  caminho dá uma decodificação a mais, nunca uma textura errada.
- **Ganho no 360:** **2 a 4 ms** (de 50 a 75% dos 4,4 a 5,3 ms: some a releitura de 2,8 MB e fica o custo das contas no envio,
  com os dados quentes).
- **Risco para a lista:** médio. A geração do `texture_get` muda (o `gen`), então o número de decodificações pode mudar; a prova
  tem de ser pelo hash normal. O risco de verdade é uma página escrita por outro caminho (alvo, `vram_rw` de escrita, envio
  parcial) ficar com o hash da origem: cada caminho de escrita tem de invalidar.
- **Esforço:** 2 a 3 dias.
- **Prova:** `divergencias.py --normal` igual quadro a quadro (cada desenho amostra os mesmos pixels), contagem de recálculos e de
  decodificações antes e depois, o verificador visto falhando com um caminho de escrita sem invalidar.
- **Alternativa barata, menor:** `dcbt` adiante no laço do `Gs_PageHash` e um hash sem multiplicação de 64 bits: de 0,5 a 1,5 ms,
  só medível no console (o WSL não vê a `mulld`), risco de colisão a avaliar. O `08-threads.md` (degrau 1, parte 2) já deixou
  essa fora até a medida do console; esta nota não muda isso.

### 4. A memória do `clut_texture`

- **O que é:** `clut_texture` (`gs_draw.c:464-507`), chamado pelo `depth_clut` (`:619`) a cada primitiva das passadas de tabela
  pela névoa e pelos ids: lê a paleta de 256 cores por 256 `Gs_VramRead` e faz o hash dela **toda vez**, sem memória, para no fim
  achar a mesma textura no `sCluts`. Uma memória pelo `cbp` e pela geração das páginas da paleta (a mesma regra do `clut_hash`,
  `:132-150`) devolve a textura sem ler nada enquanto a paleta não for reescrita.
- **Ganho no 360:** **1 a 1,8 ms** (de 60 a 95% de 1,6 a 1,9 ms; a paleta da tabela muda no máximo algumas vezes por quadro).
- **Risco para a lista:** baixo: a textura devolvida é a mesma (a lista igual prova).
- **Esforço:** 0,5 dia.
- **Prova:** lista igual quadro a quadro; o verificador visto falhando com a memória que ignora a geração.

### 5. Os índices da VU0 sem gravar índice por triângulo

- **O que é:** `GsGpu_DrawVu0`, `gs_draw.c:1164-1175`: a faixa vira triângulos gravando 3 índices por triângulo (128 mil índices,
  0,5 MB por quadro, que depois a entrega copia e o desenho copia de novo). Os índices são sempre `base + i, i+1, i+2`, salvo os
  triângulos que o `layer 4` (os destroços) pula. Um buffer de índices fixo no desenho com o vértice de base, e só os desenhos de
  `layer 4` com índices próprios.
- **Ganho no 360:** **1 a 1,5 ms** na gravação, mais uns 0,5 MB a menos na entrega (se o item 1 não vier antes) e na cópia da VU1
  do desenho.
- **Risco para a lista:** médio: a seção dos índices muda de forma (a prova tem de ser pela geometria resultante, não pelo hash
  da seção), e o desenho do 360 muda (`vu_plano`, `vu_copia`).
- **Esforço:** 1,5 a 2 dias, com uma rodada de console.
- **Prova:** o falso expande os índices dos dois jeitos e compara triângulo a triângulo em todo quadro; imagem no console.

### 6. O `clut_hash` lendo a paleta direto

- **O que é:** `clut_hash` (`gs_draw.c:140-142`) lê 16 ou 256 cores por `Gs_VramRead` (cada uma com o `switch` de formato e o
  endereço por tabela no `vram_rw`). A memória que existe acerta pouco porque o jogo repinta paletas todo quadro. Uma leitura
  direta para o caso comum (paleta PSMCT32 e PSMCT16, `bw` 1, as mesmas tabelas) dá o mesmo valor com uma fração das instruções.
- **Ganho no 360:** **0,6 a 1 ms** (de 1,3 a 1,6 ms, uns 60%).
- **Risco para a lista:** nenhum se o hash sair o mesmo (é o mesmo valor, pela mesma ordem).
- **Esforço:** 0,5 dia. **Prova:** lista igual; um `assert` de teste comparando os dois caminhos em toda chamada de um roteiro.

### 7. O `Unpack` sem a cópia para a memória da VU nos lotes reconhecidos

- **O que é:** `GsVu1_Unpack`, `gs_vu1.c:146`: o `V4-32` copia os vértices do pacote para `vu.mem`, de onde o `hle_program0`
  os passa ao `GsGpu_DrawVu0`, que os copia de novo para `gsVuVerts` (`gs_draw.c:1177`). Nos lotes que o HLE reconhece, os
  vértices poderiam ser lidos do pacote, e só o que o programa precisa (cabeçalho, constantes) iria para `vu.mem`.
- **Ganho no 360:** **1 a 2 ms** (a cópia do `Unpack` é uns dois terços dos 2,3 a 2,7 ms).
- **Risco para a lista:** médio a alto: o HLE e o interpretador leem `vu.mem` de formas diferentes, e um lote que caia no
  interpretador depois de um `Unpack` sem cópia leria lixo. Precisa de uma regra clara de quando a cópia pode faltar.
- **Esforço:** 2 a 3 dias. **Prova:** seções da VU1 iguais quadro a quadro na x86 e na PPC, `f5_duelo` e `f5_treino`.

### 8. A decodificação por blocos

- **O que é:** `gs_draw.c:376-383`, um `Gs_VramRead` por texel. Caminhos por bloco para os formatos comuns.
- **Ganho no 360:** **0,3 a 0,8 ms** em regime (mais nas cargas e nas trocas de cena, onde a decodificação domina; no 360 a
  criação pelo XG soma, e não se mediu).
- **Risco:** nenhum para a lista se os pixels saírem iguais (o hash normal prova). **Esforço:** 1 dia.

### 9. O `Gs_Gif` e o `vertex`

- **O que é:** a leitura das etiquetas e dos registradores do GIF (`gs_core.c:822-950`) e a montagem das primitivas de CPU
  (`vertex`, `:589-706`). Nada isolado passa de 1% na x86.
- **Ganho no 360:** **0,4 a 1 ms**, espalhado. **Risco:** baixo a médio (muitos registradores). **Esforço:** 1 a 2 dias.
  Só depois dos outros.

### Fora: a interpretação da VU1

Na luta **nenhum lote vai pelo interpretador**: o log dá `vu0 134352/0/0, vu4 144024/0/0, vu6 324/0/0` (shader, interpretador,
não executados) e a x86 dá 0,8% ao `hle_program*` e ao `GsVu1_Call` próprios. O custo da "VU1" é a gravação (a montagem, o estado,
o hash, as cópias), já repartido nos itens acima. Mexer no interpretador não rende nada na luta.

### A soma

Os quatro primeiros somam **de 8,5 a 13,8 ms** [estimado], o que levaria a gravação de 38,4 para uns **25 a 30 ms**: os 30 fps
com margem, se as faixas se confirmarem. Abaixo de uns 20 ms o período passa a ser o do jogo (16 a 20 ms com o desenho na irmã)
ou o do desenho (15 ms mais a cópia da VU1).

## 6. O que ficou em aberto

- **As famílias do resto não foram medidas no console**, só rateadas pela x86. Antes (ou junto) do primeiro corte, vale uma
  rodada com tempos por família na gravação, como já existe para a VU1 (`gGsVu1Ms`): o `transfer_data`, o recálculo do
  `Gs_PageHash`, o `GsVu1_Unpack`, e a entrega dividida em cópia e backend; e que essas linhas saiam também **dentro** da janela
  do `passos` (hoje o log fica mudo nela). É uma rodada de console e acerta a ordem dos itens 1 a 4 sem adivinhar.
  **Feito no código** (seção 7, a linha `gs: gravação por família`); falta a rodada no console.
- **A VU1 dentro da janela** saiu da proporção dos trechos vizinhos (42 a 49%), não de medida dentro da luta.
- **A PPC não foi usada.** A x86 bastou para ordenar, e a dúvida que sobra é de memória e de execução em ordem, que a contagem
  de instruções do `qemu-ppc` também não vê. A medida certa é a do console.
- **A memória do item 1** depende da decisão de capacidade das listas vivas.

## 7. Os cortes 2 e 4 e a medição por família (21/10/2026)

Branch `f5-envio`, sobre a main `c2606a9`. Tudo em `BT3_GS_ESPERAS` (a condição `PORT_BIG_ENDIAN || BT3_TESTE_VU1`), com o
código de antes no `#else`: a `.text` da x86 plana segue `cc46fa0752f092fca305bc8bf2e2e54390763af0`.

### O que mudou

| Commit | O quê |
|---|---|
| `7ac8ee5` | **A medição por família.** A gravação soma o próprio tempo em `gGsFamTiques` (`gs_internal.h`): o envio (`transfer_data`, depois do teste de envio ativo), o recálculo do `Gs_PageHash` (só quando refaz o laço; a parte pedida de dentro da VU1 soma também à parte), o `GsVu1_Call`, o `GsVu1_Unpack` (o corpo virou `vu1_unpack`, com o tempo em volta) e a entrega em três pedaços: a espera pelo desenho, a cópia (as cinco listas e a tabela de alvos) e o gancho `GsBackend.entrega`. O `Gs_FamTrecho` (`gs_draw.c`, chamado no começo do `GsGpu_FrameEnd`, na thread de gravação) escreve uma linha a cada 60 fins de quadro, **também dentro da janela do `passos`** (ele não consulta o `Passos_Medindo` para calar, só para contar os quadros da janela; no 360 a linha vai pelo diário, que grava noutra thread) |
| `b1cb898` | **Corte 2, o envio PSMCT32 em grupos de 8 pixels** (`gs_core.c`, `transfer_data`): com x múltiplo de 8 os 8 pixels caem no mesmo bloco, nas palavras o, o+1, o+4, o+5, o+8, o+9, o+12 e o+13; uma consulta de `sIn32` e um cálculo de endereço por grupo no lugar de 8, as pontas pelo laço de antes. As cópias seguem de 4 bytes (ver abaixo) |
| `fc980e7` | **Corte 4, a memória do `clut_texture`** (`gs_draw.c`): o hash da tabela de 256 cores lembrado por `cbp` e pela geração das duas páginas que ela ocupa (a regra do `clut_hash`; 64 vagas); as 256 leituras da VRAM só quando a memória não vale, ou para criar a textura. O `sCluts`, a escolha da vaga e o `last` seguem iguais |
| `123ba2a` | No 360 o relógio das famílias é o `__mftb` declarado como no `ppcintrinsics.h` (o `cl` o emite em linha, um `mftb`, sem chamada; o `default.map` não tem o símbolo), calibrado a cada trecho contra o `gpu_now` |
| `b04ecd3` | Fora do 360 a medição só liga com `BT3_TEMPOS=1` (na PPC do `qemu` a leitura do relógio é uma chamada de sistema emulada: a rodada das listas da `ppcg` foi de 121 para 232 s); no 360 vem ligada, e `BT3_TEMPOS=0` desliga, a mesma chave dos tempos do backend |

**Por que as cópias do envio seguem de 4 bytes.** A primeira versão copiava os quatro pares de 8 bytes; foi trocada antes das
provas. A origem é o pacote do jogo, alinhada a 16, mas o grupo começa no pixel `i + j`, que só é par quando a largura e o
começo da linha ajudam: uma cópia de 8 bytes alinhada só a 4 no Xenon pode cair numa exceção de alinhamento (e o `cl` pode
gerar `ld`/`std` para um `memcpy` de 8). O ganho do corte é a consulta e o endereço por pixel, não a largura da cópia.

### A linha nova do log

```
gs: gravação por família, quadro Q (60 quadros, K na janela): parede P ms por quadro; envio E (N vezes, B KB), hash H (R
recálculos; HV dentro da VU1), VU1 V (L lotes), Unpack U (n), entrega: espera W, cópia C, backend K2; soma S, fora delas F;
CPU da thread T
```

Tudo em ms por quadro, médias do trecho. `K na janela` diz quantos dos 60 quadros a janela do `passos` cobriu (os trechos da
luta têm 60). A **parede** é o período da gravação no trecho (do `gpu_now`, de um fim de quadro ao do trecho seguinte). A
**soma** conta o hash só uma vez (o hash total menos o de dentro da VU1, mais a VU1). **Fora delas** é a parede menos a soma:
andar a cadeia de DMA e o VIF, o GIF fora do envio e os desenhos de CPU. A **CPU da thread** só existe fora do 360 (`-1` no
console). O `espera` é o tempo da gravação esperando o desenho largar o retrato anterior (no x86 é zero; no console, se passar
de zero, o desenho virou o gargalo).

### As provas no WSL [medido]

Antes: `main-local` (`91d076f`, o mesmo código da main), script `~/envio-antes.sh`, resumo `~/envio/antes.resumo`, logs e as
builds em `~/envio/antes`. Depois: o `f5-envio` em `fc980e7`, script `~/envio-depois.sh`, resumo `~/envio/depois.resumo`, logs
em `~/envio/depois`. Uma rodada de emulador por vez; a pasta de dados de cada roteiro pelo passo do `roteiros.sh` que a gera
(como no `~/provas-degrau2-e3.sh`). Os dois commits do relógio (`123ba2a`, `b04ecd3`) vieram depois: o primeiro não muda o
código pré-processado fora do 360 (conferido com `gcc -E` nas três fontes, nas configurações da `x86gv` e da `ppcg`), o
segundo só muda o padrão da chave, e a rodada do critério refaz as listas da `x86gv` com ele (abaixo).

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `cc46fa0752f092fca305bc8bf2e2e54390763af0`, antes e depois |
| `x86gv`, `f5_treino` (7.386 quadros) e `f5_duelo` (8.380), sem chaves e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1`, antes contra depois | as quatro com `divergencias.py` saindo 0: nenhum quadro com a lista diferente, hashes `f76f0f6849b64629` e `08a98b6d05b8775a` |
| `ppcg`, os mesmos quatro casos | os quatro com 0: hashes `f76f0f6849b64629` e `994627a2aecd429a` (o da PPC no `f5_duelo`, o mesmo de antes) |
| Texturas decodificadas | as mesmas (1.248 e 2.431): nenhum corte muda a ordem de criação, e o hash de sempre basta (o normal não foi preciso) |
| Sabotagem S1: o envio com dois pixels do grupo trocados | `f5_treino` 255 quadros diferentes, `f5_duelo` 544 (o primeiro no 22), hash do jogo diferente: `divergencias.py` sai 1 |
| Sabotagem S2: a memória do `clut_texture` sem a geração das páginas | `f5_duelo` 3.416 quadros diferentes (do 4.905 em diante), sai 1; **`f5_treino` 0**: lá nenhuma tabela é reescrita no mesmo `cbp`, então só o `f5_duelo` vê esse erro |
| Sabotagem S3: 5 us de espera ativa em cada envio | o envio da linha nova sobe 1,33 ms por quadro com 274 envios por quadro (o esperado, 274 x 5 us, é 1,37): a medição mede o que se põe nela |
| O git do WSL depois das sabotagens | 0 alterados (cada uma desfeita por `git checkout`) |

**O ganho na x86.** `x86gv`, `f5_duelo` com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1`, as builds M (`7ac8ee5`, só a medição), E
(`b1cb898`, mais o envio) e C (`fc980e7`, mais o `clut_texture`), alternadas três vezes; médias dos 49 trechos inteiros dentro da
janela da luta, em ms por quadro:

| Build | Envio | Hash | VU1 | Unpack | Cópia | CPU da gravação | Parede |
|---|---:|---:|---:|---:|---:|---:|---:|
| M | 0,569 | 0,230 | 1,038 | 0,356 | 0,214 | **3,024** | 3,254 |
| E | 0,353 | 0,230 | 1,046 | 0,353 | 0,200 | **2,798** | 3,046 |
| C | 0,355 | 0,231 | 1,043 | 0,357 | 0,198 | **2,675** | 2,962 |

O envio caiu **38%** (0,216 ms); o `clut_texture` tirou mais 0,123 ms (ele não é família própria: aparece na CPU, e no "fora
delas", porque roda nas passadas de tabela, fora da VU1). Os dois juntos: **11,5% da CPU da gravação** (3,02 para 2,68 ms). As
três rodadas de cada build ficaram dentro de 0,08 ms. Com a medição desligada (`BT3_TEMPOS=0`, o padrão de agora fora do 360) a
rodada inteira leva uns 1,5 s a menos que com ela (12,2 contra 13,7 s): o relógio da x86 custa uns 0,18 ms por quadro com
umas 24 mil leituras (por isso o 360 usa o `mftb` em linha).

**A estimativa no console [estimado].** Na mesma proporção: o envio de 5,2 a 6,3 ms para uns 3,2 a 3,9 (de **2 a 2,4 ms** a
menos; abaixo dos 2 a 3,5 da seção 5, que contava com cópias de 8 bytes), e o `clut_texture` de 1,6 a 1,9 para uns 0,5 (de
**1,1 a 1,3 ms** a menos: na x86 o corte tirou uns dois terços dos 6,1% que ele tinha). Juntos, uns **3 a 3,7 ms** dos 38,4: a luta perto de 35 ms,
ainda acima dos 33,37. O que confirma é a linha nova no console.

### A rodada do critério (serviço `claude-envio` no WSL, disparado sem esperar)

O script `~/rodada-envio.sh` (usuário `codex`) refaz, no HEAD do `f5-envio`: a `x86gv` e as listas dela nos dois roteiros com a
divisão (a medição no padrão novo, desligada), com o `divergencias.py` contra as de antes; depois as três builds do critério
(`headless.py --alvo x86`, `--alvo x86 --rastreio`, `--alvo ppc`, nessa ordem), a `.text` da x86 plana, e em série `REF=1
SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino` e `SEMMAPA=1 IMG=1 roteiros.sh duelo`. Saída do serviço em
`~/rodada-envio.log`, logs em `~/rodada-envio-*.log`, resumo em `~/rodada-envio.resumo`: uma linha por passo com `RC=`, a linha
da `.text` ("a mesma" ou "DIFERENTE"), o git no fim e `FIM`. Vale com todos os `RC=0`, as duas listas com o `divergencias.py`
saindo 0, a `.text` a mesma e o git sem alterados.

### A rodada no console: o que olhar

A build: `build/x360/f2/default.xex` do worktree, com o `ambiente.txt` de lá (`BT3_GS_THREAD=1`, `BT3_GS_DESENHO=1`,
`BT3_VU1=1`, `BT3_VU_VERTS=98304`, piso de 32 MB, teto de textura de 128 MB, `BT3_SHOT=600`, a ISO convertida e os saves
`saves19`). O roteiro: START pula o filme; Duelo, Goku contra Kuririn, estágio 5; 60 s de luta; a pausa com uns 3 s de espera e a
Guide.

1. **As linhas `gs: gravação por família` com `60 na janela`**: é a primeira medida de dentro da luta. Para cada família, a
   mediana dos trechos da janela, contra o rateio da seção 2.2: envio (era 5,2 a 6,3 estimado; o esperado agora é de 3 a 4),
   hash (4,4 a 5,3), VU1 (16 a 19; ela inclui o hash de dentro da VU1, que a linha mostra à parte), Unpack (2,3 a 2,7), a cópia
   da entrega (a parte maior dos 4,8) e o backend, e o "fora delas" (a cadeia, o VIF, o GIF e os desenhos de CPU, sem medida
   até aqui). É essa tabela que reordena os itens 1, 3, 5 a 9 da seção 5.
2. **A parede** das linhas da janela contra a mediana dos passos (`passos: janela`): a parede é o período da gravação; se ela
   ficar abaixo do período dos passos, o gargalo passou para o jogo ou para o desenho (e o `espera` sobe).
3. **O período da luta**: a mediana dos passos contra os 38,4 ms da abertura 1 do degrau 2 (a estimativa é de uns 35).
4. O resto como no degrau 2: a linha `gs: divisão` (0 sem cópia), as capturas `BT3_SHOT=600` sem textura errada (em especial a
   névoa e os ids das passadas de tabela, que o corte 4 toca), a Guide no menu, na carga e na pausa, nenhum `vigia:` novo.
5. O custo da própria medida: são umas 24 mil leituras do `mftb` por quadro. Se a parede da janela parecer alta, uma abertura
   a mais com `BT3_TEMPOS=0` no `ambiente.txt` (sem as linhas, e sem os tempos do backend) dá o período sem ela.

### A rodada do critério e o console (21/10/2026) [medido]

**WSL**, serviço `claude-envio`, HEAD `3a074d9`, de 09:55 a 12:39: as listas da `x86gv` iguais às de antes nos dois roteiros, as três
builds do HEAD, a `.text` da x86 `cc46fa0752f092fca305bc8bf2e2e54390763af0` (a mesma), `f5_duelo` e `f5_treino` com "tudo certo"
(11 de 11 imagens idênticas) e `duelo` com "tudo certo" (9 de 9), o git limpo no fim.

**Console**, `.xex` `b13d0a5c239bbe201480a429dd075df6`, `BT3_GS_THREAD=1` e `BT3_GS_DESENHO=1`, Duelo, Goku contra Kuririn,
estágio 5, sem queda, sem textura branca:

| Abertura | Luta, mediana | p95 | p99 |
|---|---:|---:|---:|
| Antes dos cortes (degrau 2, `BT3_TEMPOS` ligado) | 38,4 ms | 59,5 | 78,6 |
| Com os cortes e a medição por família ligada | 39,3 ms | 61,8 | 91,9 |
| **Com os cortes, `BT3_TEMPOS=0`** | **36,2 ms** | 58,5 | 95,0 |

- **A medição por família custa uns 3 ms no 360** (39,3 contra 36,2): é de diagnóstico. Rodada de desempenho se compara sempre com
  `BT3_TEMPOS=0` dos dois lados; a referência de agora é **36,2 ms**.
- **Os cortes renderam no máximo 2,2 ms** (a base de 38,4 tinha a medição do backend ligada), menos que os 3 a 3,7 estimados.
- **O rateio de dentro da luta** (as linhas `gs: gravação por família`, com a medição ligada, por quadro): VU1 22 a 25 ms (com 2,1 de
  hash dentro), envio 6,4 ms (288 envios, 2,5 MB), `GsVu1_Unpack` 3,4 ms, entrega 2,7 de cópia e 2,4 do gancho do backend, hash fora
  da VU1 uns 0,4 ms, o resto (andar a cadeia, a montagem, o GIF) 10 a 12 ms. O envio anda a uns 390 MB/s: no Xenon ele espera a
  memória (escrita espalhada na VRAM do GS), e por isso o corte 2, que tirou contas, quase não rendeu lá. O hash fora da VU1 é
  pequeno no console: o candidato 3 perde força. Faltam uns 3 ms para os 33,37: a VU1, a cópia da entrega e o `Unpack` são os alvos.

## 8. A entrega sem cópia: dois jogos de listas (candidato 1, 21/10/2026)

Branch `f5-pingpong`, sobre a main `024cff3`. O corte 1 da seção 5: a cópia da entrega custava 2,7 ms por quadro de luta no
360, na thread de gravação, a mais lenta (seção 7).

### O que mudou

| Commit | O quê |
|---|---|
| `c9f05ae` | **Dois jogos de listas que se alternam** (`gs_draw.c`, `sJogos`, `jogo_aloca`, `jogo_usa`). `gsDraws`, `gsVerts`, `gsVuVerts`, `gsVuIdx` e `gsVuUni` apontam para o jogo em que a gravação grava. Na entrega (`desenho_entrega`), com a divisão, o retrato aponta para esse jogo como está e a gravação passa para o outro (`jogo_usa(sJogo ^ 1)`, no fim da entrega). Um quadro em voo só, como antes: a entrega começa esperando o desenho largar o retrato anterior, e o jogo para onde a gravação passa é justamente o desse retrato. O retrato copiado (`sRetDraws` e companhia, 10,06 MiB) e o caminho em série do quadro que não cabia nele deixaram de existir. Os campos pequenos (a tabela de alvos, `gGsFrame`, `gGs.frame[0..1]`, `GS_FBP_QUADRO`, `gsAnchor`, `gsTexCount`, `gsSkipped`, `gsNative`, `gGsVu1Ms`, a fila de soltar) seguem copiados como antes. Sem a divisão só se aloca o primeiro jogo. **As capacidades** (`gs_draw.h`, só com `BT3_GS_ESPERAS`): 470.000 vértices de CPU (eram 1.048.576), 260.000 da VU (524.288), 524.288 índices (1.048.576), 8.192 constantes (as mesmas) e 16.384 desenhos (65.536); a x86 plana segue com as de antes. O `gs_d3d.cpp` conta os dois jogos na linha `buffers do gs_draw.c`. `BT3_GS_CAP_DIV=N`, só nas builds `--gpu` (`BT3_GPU_FALSO`), divide as cinco capacidades por N, para a prova do que não cabe |

**A memória (360, ponteiros de 4 bytes).**

| | Antes | Depois |
|---|---:|---:|
| Listas | 66,38 MiB (1 jogo, capacidade cheia) | 61,65 MiB (2 jogos de 30,83 MiB) |
| Retrato | 10,06 MiB | 0 |
| **Total** | **76,44 MiB** | **61,65 MiB** (14,78 MiB a menos) |
| Sem a divisão | 66,38 MiB | 30,83 MiB |

Mais 576 KB a menos nas tabelas do `gs_d3d.cpp` medidas por `MAX_DRAWS` (`sVuIni`, `sVuLo`, `sVuNv`). Por jogo, contra o
maior quadro medido: vértices de CPU 470.000 contra 13.182 (35 vezes), da VU 260.000 contra 53.618 (4,8), índices 524.288
contra 128.907 (4,1), constantes 8.192 contra 2.253 (3,6), desenhos 16.384 contra 2.517 (6,5). Nenhuma precisou de potência
de dois: o código só compara contagens com a capacidade.

**O que acontece quando uma lista enche (conferido no código).** Não há saída nova. Cada um dos caminhos que gravam na lista
testa a capacidade antes de gravar e, se não cabe, volta sem gravar nada, sem contar: `GsGpu_Draw` (vértices de CPU mais 6, e
desenhos), os três de desenho da VU (`count` vértices, `(count - 2) * 3` índices, um desenho e uma constante), `depth_clut`,
a cópia dos ids e o `date_snapshot` (desenhos), `GsGpu_FbUpload` e `GsGpu_Native` (desenhos). Nenhum incremento escapa do teste,
então a contagem nunca passa da capacidade (e o `BT3_GS_CAP_DIV` pode usar os mesmos testes de igualdade). O caminho é o mesmo
com a divisão ligada ou desligada: os testes são da gravação, antes da entrega. **O "sem espaço" do log do `gs_d3d` não é
esse caso**: ele conta os desenhos da VU1 que não couberam nos buffers do próprio `gs_d3d` (`BT3_VU_VERTS`), e a brancura
"sem espaço" é do cache de texturas. O sinal de lista cheia no console é o `CHEIO` na linha `buffers, o maior quadro do
trecho` (vértices de CPU podem parar até 5 abaixo da capacidade sem o `CHEIO`, pelo teste de 6 de folga).

**O backend.** O `gs_d3d.cpp` guarda um ponteiro só, o `sQ` (o quadro que o `frame_end` desenha, posto no começo dele); tudo o
que lê das listas é por ele ou pelo `q` da entrega (`vu_plano`, `vu_copia`, `mem_buffers`, os nativos, os desenhos de CPU por
`DrawPrimitiveUP`). Fora do `frame_end` o `sQ` só é lido pelas profundidades e pelos alvos (`sQ->targets`, a cópia da
tabela) e pela captura (`sQ->quadro`); o `GsD3d_Repete` usa a textura do último alvo mostrado, não as listas, e não corre
durante a entrega (`sEntregando`). O falso (`gpu_falso.c`) lê o quadro pelo `gsQuadro` no `fake_frame_end` e pelo `q` no
`fake_entrega`, sem guardar ponteiro. Nenhum dos dois mudou, fora a conta dos buffers.

### As provas no WSL [medido]

Serviço `claude-pingpong-provas` (script `~/provas-pingpong.sh` do `codex`, resumo `~/provas-pingpong.resumo`, logs em
`~/pingpong/`), de 13:02 a 14:53, em série. Antes: `3a074d9` (o código da main). Depois: `c9f05ae`. A pasta de dados dos dois
roteiros refeita pelo passo do `roteiros.sh` que a gera (a rodada do critério anterior a deixou vazia).

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `cc46fa0752f092fca305bc8bf2e2e54390763af0`, antes e depois |
| `x86gv`, `f5_treino` (7.386 quadros) e `f5_duelo` (8.380): sem chaves, com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1`, com o desenho atrasado (`BT3_GS_DESENHO_ATRASO_US=20000`) e com a gravação atrasada (`BT3_GS_ATRASO_US=20000`), antes contra depois | as oito com `divergencias.py` saindo 0, hashes `f76f0f6849b64629` e `08a98b6d05b8775a`, 0 desenhos com textura já solta |
| `ppcg`, os mesmos oito casos | os oito com 0, hashes `f76f0f6849b64629` e `994627a2aecd429a`; de 165 a 323 s por rodada |
| O maior quadro com a divisão (`gs: divisão`) | `f5_duelo` 2.473 desenhos, 13.125 vértices de CPU, 53.618 da VU, 128.907 índices, 2.253 constantes, como antes |
| **O que não cabe**, `BT3_GS_CAP_DIV=40`, sem a divisão contra a divisão e contra a divisão com o desenho atrasado 20 ms | 0 nos quatro pares (os dois roteiros); o maior quadro do `f5_duelo` com 11.745 vértices de CPU (de 11.750: cheia), 13.107 índices (de 13.107: cheia), 5.783 da VU, 153 constantes, 303 desenhos |
| O mesmo com `BT3_GS_CAP_DIV=8` e `=5` (`f5_duelo`, sem contra com a divisão) | 0 nos dois; com 8, 1.024 constantes de 1.024 e 65.535 índices de 65.536; com 5, 1.638 constantes de 1.638 |
| Verificador visto falhando: a capacidade dividida contra a normal | sai 1: 3.316 quadros diferentes no `f5_treino` e 4.244 no `f5_duelo` (/40), 4.197 (/8 e /5) |
| Sabotagem N1: a gravação não troca de jogo na entrega (volta ao que o desenho ainda tem), com o desenho atrasado 20 ms | sai 1: 7.382 de 7.386 quadros diferentes no `f5_treino`, 8.375 de 8.380 no `f5_duelo` |
| A mesma sem atraso | sai 1 também: 1.582 quadros diferentes no `f5_treino` (a gravação zera e reescreve o jogo logo depois de acordar o desenho) |
| O git do WSL no fim | 0 alterados (a sabotagem desfeita por `git checkout`, com `assert` de que casou) |

Os vértices da VU e os desenhos não chegam ao teto com um divisor só: os índices e as constantes enchem antes, e o desenho da VU
que não cabe para no mesmo teste (`count` vértices, os índices, um desenho e uma constante numa condição só). O teste dos
desenhos é o mesmo `gsDrawCount == CAP_DRAWS` em todos os caminhos.

**A entrega na x86** (`f5_duelo` com a divisão e `BT3_TEMPOS=1`, antes e depois alternados três vezes, médias das 147 linhas
com 60 quadros na janela): a cópia de **0,159 para 0,000 ms** por quadro; a parede da gravação de 2,856 para 2,691 ms e a CPU da
thread de 2,551 para 2,401 (uns 0,15 ms a menos, a cópia que saiu). A `entrega média` da linha `gs: divisão` caiu de 0,079 para
0,003 ms na x86 e de 0,435 para 0,016 ms na PPC do `qemu`.

**A estimativa no console [estimado].** A cópia media 2,7 ms por quadro de luta (seção 7). Sem ela, a família "cópia" fica
só com a tabela de alvos (24 entradas): perto de zero. A luta de 36,2 ms (`BT3_TEMPOS=0`) deve ir para perto de **33,5 ms**,
se a gravação seguir como gargalo; abaixo dos 33,37 só com a VU1 ou o `Unpack`.

### A build do 360

`build_f2.sh` no `c9f05ae`: 317 de 317 fontes sem erro, avisos por fonte iguais aos da main (os dois C4090 de sempre no
`gs_draw.c`), os mesmos 103 stubs, `default.dat` `6d30baaf87b1177fb2359a855f8befc0` e `default.swap`
`a7cf723fbfaea860fcc3707ad288e755`, iguais; `default.xex` `f33df29754d2e25b90dcf074dc27dae0`.

### A rodada do critério (serviço `claude-pingpong` no WSL, disparado sem esperar)

O script `~/rodada-pingpong.sh` (usuário `codex`) espera o serviço das provas acabar e refaz, no HEAD do `f5-pingpong`, as três
builds do critério (`headless.py --alvo x86`, `--alvo x86 --rastreio`, `--alvo ppc`, nessa ordem), a `.text` da x86 plana, e
em série `REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino` e `SEMMAPA=1 IMG=1 roteiros.sh duelo`. Saída em
`~/rodada-pingpong.log`, logs em `~/rodada-pingpong-*.log`, resumo em `~/rodada-pingpong.resumo` (uma linha por passo com
`RC=`, a linha da `.text`, o git no fim e `FIM`). Vale com todos os `RC=0`, a `.text` a mesma e o git sem alterados.

### A rodada no console: o que olhar

A build: `build/x360/f2/default.xex` do worktree `bt3xbx-a` (`f33df29754d2e25b90dcf074dc27dae0`), em duas aberturas: o
`ambiente_ab1.txt` (com `BT3_TEMPOS=0`, o desempenho) e o `ambiente_ab2.txt` (sem ele, o rateio por família), copiado como
`ambiente.txt`. Os dois com `-BT3_DEMO`, a ISO convertida, `BT3_GS=gpu`, os saves `saves19`, piso de 32 MB, teto de textura de
128 MB, `BT3_VU1=1`, `BT3_VU_VERTS=98304`, `BT3_SHOT=600`, `BT3_GS_THREAD=1` e `BT3_GS_DESENHO=1`. O roteiro: START pula o
filme; Duelo, Goku contra Kuririn, estágio 5; 60 s de luta; a pausa com uns 3 s de espera e a Guide.

1. **A mediana da luta na abertura 1** (`passos: janela`) contra os **36,2 ms** de `BT3_TEMPOS=0` da seção 7; o esperado é
   perto de 33,5. O p95 e o p99 contra 58,5 e 95,0.
2. **A cópia na abertura 2**: nas linhas `gs: gravação por família` com `60 na janela`, a `cópia` da entrega perto de zero
   (era 2,7); a `espera` subir mostra o desenho virando o gargalo.
3. **A linha `gs: BT3_GS_DESENHO=1`**: `dois jogos de listas de 31567 KB`; e a `gs: divisão` com `(os dois jogos de listas, sem
   cópia)`, a `entrega média` menor que antes.
4. **A memória** (`gs_d3d:   memória`): `buffers 63134 KB` (eram 67.968 das listas, sem contar os 10.304 do retrato), e a
   **memória livre mínima** (`mínimo visto`) contra os 31 MB do degrau 2: uns 14 MB a mais de folga.
5. **O que não cabe**: na linha `gs_d3d:   buffers, o maior quadro do trecho`, nenhum `CHEIO` (os maiores perto de 13 mil
   vértices, 54 mil da VU, 129 mil índices, 2,3 mil constantes e 2,5 mil desenhos, contra 470 mil, 260 mil, 524 mil, 8.192 e
   16.384); e o `sem espaço` da linha da VU1 (os buffers do próprio `gs_d3d`) e o das brancas em zero, como antes.
6. O resto como no degrau 2: as capturas `BT3_SHOT=600` sem textura errada, a Guide no menu, na carga e na pausa, nenhum
   `vigia:` novo.

### A rodada no console da entrega sem cópia (21/10/2026) [medido]

`.xex` `f33df29754d2e25b90dcf074dc27dae0`, `BT3_GS_THREAD=1` e `BT3_GS_DESENHO=1`, Duelo, Goku contra Kuririn, estágio 5, sem
queda, sem textura branca, nenhum `CHEIO` nas duas aberturas.

| | Antes (cortes 2 e 4) | Com os dois jogos de listas |
|---|---:|---:|
| Luta, mediana, `BT3_TEMPOS=0` | 36,2 ms | **34,9 ms** |
| p95 e p99, `BT3_TEMPOS=0` | 58,5 e 95,0 ms | **45,8 e 72,5 ms** |
| Luta, mediana, com a medição | 39,3 ms | 37,5 ms |
| Cópia da entrega (por família) | 2,7 ms | 0,00 ms |
| Memória livre mínima | 31 MB | 38 MB |

A cópia sumiu, mas a mediana caiu só 1,3 ms dos 2,7 dela; os picos caíram mais (p95 menos 12,7 ms). No rateio com a medição: VU1
22 a 25 ms, envio 6,4, `Unpack` 3,3 a 3,8, o gancho do backend na entrega 2,0 a 2,1, hash fora da VU1 uns 0,3, o resto 9,5 a 11,4.
Faltam uns 1,5 ms para os 33,37.

### Pendente (para depois do desempenho): o embaçado no voo

O Lucas nota, desde antes da etapa 8, as texturas embaçadas em movimento (o voo para frente com o A). Provavelmente é o efeito do
próprio jogo, o desfoque de movimento do cenário (`StgPanBlur`, `src/sys/gfxm_a.c`, ligado em `src/battle/stg_c.c:629`), que mistura
o quadro anterior quando a câmera anda rápido. A dúvida é a intensidade contra o PS2: a imagem de 640x448 ampliada para 960x720
suaviza, e há uma passada de borrão pelo alfa do destino (`BT3_FX_OFF=16`) parecida com o "borrado" que a etapa 7 investigou. Para
conferir, sem código: comparar com o original (PS2 ou PCSX2) voando com o A, e uma abertura com `BT3_FX_OFF=16` para ver se o
embaçado do voo some.

## 9. O gancho do backend e o `Unpack` por dentro (21/10/2026)

Branch `f5-perfil`, sobre o `f5-pingpong`. Faltam uns 1,5 ms para os 33,37; o gancho `backend` da entrega (2,0 a 2,1 ms) e o
`GsVu1_Unpack` (3,3 a 3,8 ms) são dois alvos sem divisão interna. Esta medição os divide, no mesmo mecanismo da seção 7 (só com
`gGsFam`, o `mftb` em linha no 360; fora do 360 só com `BT3_TEMPOS=1`; tudo em `BT3_GS_ESPERAS`, a `.text` da x86 plana não muda).

### O que cada um faz, só lendo

- **O gancho** (`port/x360/src/gs/gs_d3d.cpp`, `entrega`): o `grav_junta` e o `GsDraw_TargetGet` (fixos); o `vu_plano`
  (`:2179`), que passa duas vezes pelos desenhos e, para cada desenho da VU1, por **todos os índices** dele atrás do menor e do
  maior (uns 129 mil índices, 516 KB, por quadro de luta), mais a regra do cleared, outra passada pelos desenhos; o `prof_garante`
  (`:1345`), outra passada pelos desenhos com o `sPipes[d->pipeline]` de cada um; o `alvos_sem_textura` (24 vagas, só cria se
  faltar); o `mem_mede` (`:318`), um `GlobalMemoryStatus`, chamada ao sistema; o `sincroniza_alvos` (24 vagas). Proporcional aos
  índices da VU1 (o `vu_plano`) e aos desenhos (quatro passadas por uns 2,5 mil `GsDraw` de 128 bytes, uns 320 KB cada).
- **O `Unpack`** (`port/src/gs/gs_vu1.c`, chamado pelo `vif` em `port/src/gs/gs_core.c:1058` e `:1077`): proporcional aos
  vetores; três caminhos. A cópia `V4-32` (um `memcpy` do pacote do jogo para a `vu.mem`, a origem fria); o laço rápido dos
  outros formatos sem máscara (no 360 cada elemento de 8 ou 16 bits passa pelo `unpack_elem`, um `memcpy` de 4 bytes e um
  deslocamento); o laço geral (máscara, `mode`, `cl != wl`, `V4-5`), com ramos por componente.

### A linha nova

Logo depois da `gs: gravação por família`, também dentro da janela do `passos`:

```
gs: gravação por família, por dentro, quadro Q: backend B: plano da VU1 P, alvos e profundidades A, memória M, o resto R;
Unpack U: cópia V4-32 C (n vezes, v vetores), rápido X (n vezes, v vetores), geral G (n vezes, v vetores)
```

Em ms por quadro, médias do trecho. O **plano da VU1** é o `vu_plano` mais a regra do cleared; **alvos e profundidades**, o
`prof_garante` e o `alvos_sem_textura`; **memória**, o `mem_mede`; **o resto** é o gancho menos os três (o `grav_junta`, o last
do alvo mostrado, o `sincroniza_alvos`, a fila das profundidades). O gancho ganha três leituras do relógio por quadro; o `Unpack`
nenhuma (o caminho sai de uma variável que o corpo marca). As novas famílias ficam fora da `soma` da linha de cima.

### A build e a rodada no console

`build/x360/f2/default.xex` do worktree `bt3xbx-p` (`4afb25bc5fee6881e71c05973bdfda8a`): 317 de 317, os mesmos 993 avisos e
os mesmos 103 stubs da main, `.dat` e `.swap` iguais. O `ambiente.txt` é o da abertura 2 da seção 8 (a medição ligada). O
roteiro de sempre (Duelo, Goku contra Kuririn, estágio 5, 60 s de luta); olhar as linhas `por dentro` com `60 na janela` na
linha de cima. A aposta antes de medir: o plano da VU1 leva a maior parte do gancho (o laço por índice), a memória quase nada;
no `Unpack`, a cópia `V4-32` leva a maior parte, presa à leitura fria do pacote, e o laço rápido custa mais por vetor que ela.

## 10. O `vu_plano` sem reler os índices (21/10/2026)

Branch `f5-vuplano`, sobre o `f5-perfil`. Medido no console (seção 9): o gancho `entrega` leva 1,8 a 2,1 ms por quadro de luta,
e 95% disso é o `vu_plano`, que relia todos os índices da VU1 (uns 129 mil por quadro) só para achar o menor e o maior índice de
cada desenho.

### O que mudou

| Commit | O quê |
|---|---|
| `7f0dade` | **A faixa de cada desenho guardada na gravação** (`gs_draw.c`, `vu_faixa`). Cada jogo de listas ganha a `vuFaixa`, dois `uint32_t` por desenho (o menor e o maior índice), e o `GsQuadro` a leva (`q->vuFaixa`, posta no `backend_frame_end` e na entrega, do jogo que vai). Os três `GsGpu_DrawVu*` geram os índices de um trecho em ordem crescente (`base + i + k`), então o menor é o primeiro índice do trecho e o maior o último; quando o trecho se junta ao desenho anterior (`same_state`), a faixa é a união das duas; o desenho sem índices (o `DrawVu0` das lascas com todos os triângulos marcados) fica com a faixa vazia, que não muda a união. O `vu_plano` (`gs_d3d.cpp`) passa uma vez só pelos desenhos e lê a faixa: o resultado (`sVuIni`, `sVuLo`, `sVuNv`, `sVuCabem`, os maiores pedidos do log e, por eles, a regra do cleared da entrega) só depende da faixa, da contagem, do `vuVertCount` e da capacidade do par, e sai igual. O `GsDraw` segue com 128 bytes (o `gs_d3d.cpp` confere o layout) e tudo fica em `BT3_GS_ESPERAS`. A conta dos buffers (`JOGO_BYTES`, `sMem.buffers`) soma os 128 KB da tabela por jogo. **Verificador de teste**, só nas builds `--gpu`: `BT3_VU_PLANO_CONFERE=1` relê no fim de cada quadro os índices de cada desenho da VU1, como o `vu_plano` fazia, e conta as faixas diferentes (uma linha a cada 1.000 quadros, e uma por diferença, até 20). O falso não tem `vu_plano`; a conferência fica no `gs_draw.c`, que as duas builds compartilham |
| `dfb5c84` | O `vu1_perfil.sh` cria a pasta das rodadas (`~/perfil`): no Deck ela não existia e os roteiros paravam no boot (guia, seção 6) |

**A entrega sem cópia não estraga a faixa.** A tabela é do jogo, como as listas: na entrega o retrato aponta para a do jogo que
vai, o `vu_plano` a lê ali mesmo, na gravação, antes do `jogo_usa(sJogo ^ 1)`, e a gravação passa a escrever na do outro. O
desenho não lê a faixa (o `vu_copia` usa o `sVuIni`, o `sVuLo` e o `sVuNv` que o plano deixou). Sem a divisão, o `frame_end` lê a
do jogo único, viva. A interpolação (`interp_counts_restore`) só trunca as contagens; nada fora dos três `DrawVu*` escreve índices
ou muda o `first` e o `count` de um desenho da VU1 (conferido por `grep` no `gs_draw.c`).

### As provas no Deck [medido]

Script `~/provas-vuplano.sh`, resumo `~/vuplano/provas.resumo`, listas e logs em `~/vuplano/`, em série. Antes: `f5-perfil`
(`4dbedd3`). Depois: `dfb5c84` (o código de `7f0dade`), as rodadas do depois com `BT3_VU_PLANO_CONFERE=1`. A pasta de dados dos
dois roteiros já estava no Deck.

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `cc46fa0752f092fca305bc8bf2e2e54390763af0` |
| `x86gv`, `f5_treino` (7.386 quadros) e `f5_duelo` (8.380), sem chaves e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1`, antes contra depois | os quatro com `divergencias.py` saindo 0, hashes `f76f0f6849b64629` e `08a98b6d05b8775a` |
| `ppcg`, os mesmos quatro casos | os quatro com 0, hashes `f76f0f6849b64629` e `994627a2aecd429a`; de 208 a 452 s por rodada |
| A conferência, nas oito rodadas do depois | 0 diferentes: 6.375.164 desenhos da VU1 conferidos nos primeiros 7.000 quadros do `f5_treino`, 7.422.923 nos primeiros 8.000 do `f5_duelo` (uns 900 por quadro), nenhuma linha de diferença em quadro nenhum |
| Sabotagem: o maior guardado com 1 a menos (`x86gv`, `f5_duelo` com a divisão) | 7.422.923 diferentes de 7.422.923; a primeira no quadro 4.136, desenho 1 (`vu 2`, 129 índices): relido 0 a 48, guardado 0 a 47 |
| O git do Deck no fim | 0 alterados (a sabotagem desfeita por `git checkout`, com `assert` de que casou, e a `x86gv` refeita) |

### A build do 360

`build_f2.sh` no `7f0dade`: 317 de 317 fontes sem erro, avisos por fonte iguais aos da main, os mesmos 103 stubs, `default.dat`
`6d30baaf87b1177fb2359a855f8befc0` e `default.swap` `a7cf723fbfaea860fcc3707ad288e755`, iguais; `default.xex`
`3b8f679dd133e4820f9e0b261be34800`. O `ambiente.txt` do worktree `bt3xbx-v` tem a medição desligada (`BT3_TEMPOS=0`).

### A rodada do critério (no Deck, disparada sem esperar)

O script `~/rodada-vuplano.sh` do Deck (dentro do contêiner) traz o HEAD do `f5-vuplano`, refaz as três builds do critério
(`headless.py --alvo x86`, `--alvo x86 --rastreio`, `--alvo ppc`, nessa ordem), confere a `.text` da x86 plana e roda em série
`REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino` e `SEMMAPA=1 IMG=1 roteiros.sh duelo`. Saída em `~/rodada-vuplano.log`,
logs por passo em `~/rodada-vuplano-*.log`, resumo em `~/rodada-vuplano.resumo` (uma linha por passo com `RC=`, a `.text`, o
git no fim e `FIM`). As referências dos roteiros do Deck são as mesmas do WSL (o mesmo sha1). Vale com todos os `RC=0`, os dois
`roteiros.sh` com "tudo certo", a `.text` a mesma e o git sem alterados.

### A rodada no console: o que olhar

A build: `build/x360/f2/default.xex` do worktree `bt3xbx-v` (`3b8f679dd133e4820f9e0b261be34800`), com o `ambiente.txt` de lá
(`-BT3_DEMO`, a ISO convertida, `BT3_GS=gpu`, os saves `saves19`, piso de 32 MB, teto de textura de 128 MB, `BT3_VU1=1`,
`BT3_VU_VERTS=98304`, `BT3_SHOT=600`, `BT3_GS_THREAD=1`, `BT3_GS_DESENHO=1`, `BT3_TEMPOS=0`). O roteiro de sempre: START pula o
filme; Duelo, Goku contra Kuririn, estágio 5; 60 s de luta; a pausa com uns 3 s de espera e a Guide.

1. **A mediana da luta** (`passos: janela`) contra os **34,9 ms** da seção 8 (`BT3_TEMPOS=0`); a meta é 33,37. O p95 e o p99
   contra 45,8 e 72,5. A estimativa [estimado]: o plano passa de uns 1,7 a 2,0 ms para uma passada pelos uns 2,5 mil desenhos
   (perto de 0,1 a 0,2 ms); se a gravação seguir como gargalo, a luta perto de 33,3 a 33,6 ms, mas a cópia da seção 8 rendeu só
   metade do que media, então de 34 para baixo já confirma o corte.
2. As capturas `BT3_SHOT=600` iguais às de antes (um desenho da VU1 com a faixa errada sai sem parte dos vértices ou é pulado
   como "sem espaço"), e o `sem espaço` da linha da VU1 em zero, como antes.
3. A linha `gs_d3d:   memória` com `buffers` 256 KB acima da de antes (a tabela dos dois jogos), nenhum `CHEIO`.
4. Para ver o gancho por dentro, uma abertura a mais sem `BT3_TEMPOS=0`: na linha `por dentro`, o `plano da VU1` perto de zero.

### A rodada no console do vu_plano sem reler os índices (22/10/2026) [medido]

`.xex` `3b8f679dd133e4820f9e0b261be34800`, `BT3_GS_THREAD=1`, `BT3_GS_DESENHO=1`, `BT3_TEMPOS=0`, Duelo, Goku contra Kuririn,
estágio 5 (931 passos de luta). Critério no Deck com tudo certo (21/10).

| | Entrega sem cópia | Com o vu_plano sem reler |
|---|---:|---:|
| Luta, mediana | 34,9 ms | **33,8 ms** |
| p95 e p99 | 45,8 e 72,5 ms | 45,5 e 66,4 ms |

Sem queda, sem `CHEIO`, sem textura branca, as 32 texturas pretas de sempre, memória livre mínima 38 MB. A luta ficou a 0,4 ms
dos 33,37: a meta ainda não fecha com margem, e o p95 (45 ms) mostra os picos. O próximo alvo é o `GsVu1_Unpack` (3 a 4 ms, a cópia
V4-32 presa à memória, seção 9).

## 11. O registro dos quadros lentos, `BT3_PICOS` (22/10/2026)

Branch `f5-picos`, sobre a main `1ac0a42`. A luta tem mediana de 33,8 ms (seção 10), mas p95 de 45,5 e p99 de 66,4, com
máximos de centenas de ms: os picos são o que se sente. A medição por família (`BT3_TEMPOS`) custa uns 3 ms (24 mil leituras do
relógio por quadro) e mede médias de 60 quadros; os picos pedem o contrário: o quadro lento sozinho, com quase nenhum custo.
Só mede, não muda nada no caminho do quadro.

### O que se mede

Só no 360 (`PORT_X360`; nas outras builds o código pré-processado não muda). Ligado por `BT3_PICOS=L` no `ambiente.txt`, o limiar
em ms; ausente ou 0, desligado (os contadores inteiros andam sempre, os relógios e as linhas só com o limiar). Por quadro, cada
thread escreve o seu lado na fronteira dela, numa vaga de um anel de 16 pelo número do quadro do GS:

| Lado | Fronteira | O que guarda | Relógio |
|---|---|---|---|
| Jogo (hardware 2) | a entrega da lista (`Port_GsVif1Chain`, antes do `render_post`): `Picos_Jogo` | o período desde a entrega anterior; dele, a espera pela gravação (`render_wait`: o `Port_GsEspera` de todos os pontos, a espera da lista anterior e os envios pelo canal da GIF) e quantas; a espera de vblank (o `gPortSleptNs` que o próprio `Port_GsVif1Chain` lê e zera); os vblanks no período e o `gPortVBlanks`; as leituras do disco (o `ler` do `iso360.c`: quantas, KB, ms) e as linhas do diário desde a entrega anterior (de qualquer thread), com o tempo da escritora no disco (lotes e flushes) | 2 `mftb`, mais 2 por espera de fato |
| Gravação (hardware 0) | a entrega do retrato (`desenho_entrega`): `Picos_Gravou` | o período desde a entrega anterior; dele, a espera pelo desenho (largar o retrato anterior), o tempo sem trabalho (esperando a lista do jogo, no laço da `render_thread`) e o resto, **ocupada**; dentro de ocupada, a espera por memória (`espera_desenho`, o pedido ao desenho para soltar); o que fez: texturas decodificadas, os pixels e o tempo da decodificação; envios (`transfer_data`) e KB; recálculos do `Gs_PageHash`; lotes da VU1 (o `gGsVu1Lotes` que o `grav_junta` junta); desenhos, vértices de CPU, da VU e índices; do descarte, pressões, texturas descartadas e esperas; as medidas da memória (`GlobalMemoryStatus`); o XG: texturas, alvos e profundidades criados, os KB e o tempo do `XPhysicalAlloc`, e as solturas (`XPhysicalFree`) | 4 `mftb` |
| Desenho (hardware 3) | o fim do `frame_end` | o período entre duas apresentações de quadro novo (do `mftb` logo depois do `Present`); o `frame_end` inteiro na CPU e o `Present` (os mesmos números do log de 60 quadros); a espera da GPU soltar um recurso da cova (`BlockUntilNotBusy`); as apresentações repetidas (`GsD3d_Repete`) entre dois quadros; se o quadro teve captura | 2 `mftb` |
| GPU | uma marca antes do `Present` | `InsertCallback(D3DCALLBACK_IDLE \| D3DCALLBACK_CPU1)`, como as marcas do `BT3_TEMPOS`: o callback roda quando a GPU acabou tudo o que veio antes e guarda o `mftb`. Daí o **fim da GPU depois da CPU** (quanto depois de a CPU pôr a marca a GPU terminou o quadro) e o **período da GPU** (entre os fins de dois quadros) | 1 marca por quadro |

**Por que a marca da GPU e não o `Present` sozinho.** O `Present` do 360 só bloqueia quando a fila de comandos está cheia ou a GPU
está atrasada; o tempo dele mistura as duas coisas. A marca diz quando a GPU acabou o quadro, sem parar a CPU. O custo: a GPU
esvazia o pipeline uma vez por quadro, no fim dele, onde já está acabando. As cercas comuns (`InsertFence`) só dizem "passou" ou
"não passou", sem o instante.

O lado do jogo fica na vaga do quadro do jogo (`gGsFrameJogo`); a gravação recebe esse número com a lista (`sJobJogo`, lido ao
pegar o trabalho) e o guarda, e a linha junta os dois por ele, não por contarem igual. O desenho do quadro N escreve a linha do quadro **N - 2**: os três lados dele já passaram e a GPU já chegou à marca (se não chegou,
`-1`, e o resumo conta). O relógio do `mftb` é calibrado a cada resumo contra o `QueryPerformanceCounter`. As linhas vão pelo diário
(um anel em memória; a escritora grava noutra thread, na de hardware 1), **também dentro da janela do `passos`**: o mudo de lá é
só do resumo de 60 quadros do `gs_d3d` (`log_quadros` consulta o `Passos_Medindo`), e o registro não consulta. O custo de uma linha
é um `_vsnprintf` de uns 600 bytes na thread de desenho, depois do `Present` e só nos quadros lentos.

### As linhas

```
gs_d3d: picos: o registro dos quadros lentos ligado, limiar 40 ms (BT3_PICOS): ...
picos: quadro Q (vblank V)[, CAPTURA][, na janela]: apresentação A ms; jogo J (espera da gravação E em n, vblank B, v vblanks);
  gravação G (ocupada O, dela memória M; esperando o desenho D, sem trabalho S); desenho: frame_end F, Present P, cova C, r
  repetidas; GPU: fim X depois da CPU, período Y
picos:   quadro Q, o que teve: texturas t (p mil pixels, T ms), envios e (K KB), hash h, VU1 l lotes, desenhos d, vértices v de
  CPU e u da VU (i índices); descarte: p pressões, x texturas, w esperas; memória medida m vezes; XG: a texturas, b alvos, c
  profundidades (K KB, T ms), s soltas; disco n leituras (K KB, T ms); diário n linhas, escritora T ms
picos: resumo, quadro Q: 60 quadros (K na janela), N acima de 40 ms (j pelo jogo, c com captura); apresentação até 36,7 ms a,
  até 45 b, até 58 c, até 83 d, acima e; pior P ms no quadro X; GPU sem chegar à marca em dois quadros g
```

Tudo em ms, do quadro. As duas primeiras saem juntas quando a **apresentação** (o que se vê) ou o **período do jogo** passa do
limiar. O resumo sai a cada 60 quadros desenhados; as faixas do histograma são 33 (até 36,7: dois vblanks), 40 (até 45), 50 (até 58:
três vblanks), 66 (até 83: quatro) e 100 ou mais. Um lado que não chegou à vaga (o jogo fora da thread de gravação, um quadro
refeito) sai com `-1` nos tempos.

### Como ler

- **Qual thread puxou.** No quadro lento, as três têm o mesmo período (uma espera a outra); a que tem o tempo **próprio** alto é
  a culpada: o jogo com `J - E - B` alto (o código do jogo, a carga, o disco), a gravação com `ocupada` alta, o desenho com
  `frame_end` alto, a GPU com `fim X depois da CPU` alto (ou período da GPU acima do da apresentação). Se a gravação passa o
  tempo `esperando o desenho`, o desenho é o gargalo; se passa `sem trabalho`, o jogo é.
- **O que veio junto.** A segunda linha diz o que o quadro teve a mais que um quadro comum da luta (perto de 288 envios, 2,5 MB,
  2,5 mil desenhos, 13 mil vértices de CPU, 54 mil da VU, 129 mil índices; seções 7 e 8): texturas novas (e o tempo de
  decodificá-las), pressões e esperas do descarte, criações pelo XG (o `XPhysicalAlloc` pode demorar), leituras do disco, um surto
  de linhas no diário. Um pico com `texturas 40 (… 9 ms)` é decodificação; com `descarte: … 3 esperas` e `memória` alta, é a
  gravação esperando o desenho soltar memória; com `disco 2 leituras (300 KB, 25 ms)` e o jogo alto, é carga.
- **A captura.** O `BT3_SHOT=600` captura um quadro a cada 600 (uns 20 s), e cada captura custa de 100 a 200 ms no desenho: um
  pico artificial. A linha diz `CAPTURA` e o resumo conta à parte; para os percentis do `passos` sem elas, tirar o `BT3_SHOT` numa
  abertura a mais.
- **O vblank.** O `vblank V` (o `gPortVBlanks` na entrega) junta a linha ao resto do log; `v vblanks` é quantos o período do jogo
  levou (2 na luta em dia).

### As provas

- `build_f2.sh` no `2e9df0f` (o código): 317 de 317 fontes sem erro, avisos por fonte iguais aos da main (os do jogo e os do 360),
  os mesmos 103 stubs, `default.dat` `6d30baaf87b1177fb2359a855f8befc0` e `default.swap` `a7cf723fbfaea860fcc3707ad288e755`,
  iguais; `default.xex` `5aa16a31153a2b7d36e4bfb4ac0998d7`. No `default.map` não há `__mftb` (em linha) e estão os `Picos_*`.
- **Fora do 360 nada muda.** O `gcc -E -P` do `gs_core.c`, do `gs_draw.c` e do `gs_vu1.c` nas configurações da x86 plana, da
  `x86gv` (`-DBT3_TESTE_VU1 -DBT3_GPU_FALSO`) e da `ppcg` (`-DPORT_BIG_ENDIAN -DBT3_GPU_FALSO`) é o mesmo antes (`1ac0a42`) e
  depois; o mesmo com `-DPORT_X360` sai diferente (o verificador vê a mudança). A `.text` da x86 plana no WSL ficou para depois:
  o WSL não respondia (sem rota para a máquina) durante o trabalho.

### A build e a rodada no console

`build/x360/f2/default.xex` do worktree `bt3xbx-k`, com o `ambiente.txt` de lá: o da seção 10 (`-BT3_DEMO`, a ISO convertida,
`BT3_GS=gpu`, os saves `saves19`, piso de 32 MB, teto de textura de 128 MB, `BT3_VU1=1`, `BT3_VU_VERTS=98304`, `BT3_SHOT=600`,
`BT3_GS_THREAD=1`, `BT3_GS_DESENHO=1`, `BT3_TEMPOS=0`) mais `BT3_PICOS=40`. O roteiro: START pula o filme; Duelo, Goku contra
Kuririn, estágio 5; uns 90 s de luta, com golpes especiais e o voo para frente com o A; a pausa com uns 3 s de espera e a Guide.

O que olhar: (1) a linha `gs_d3d: picos: ... ligado`; (2) a mediana do `passos: janela` contra os 33,8 ms da seção 10 (o custo do
registro tem de ficar dentro do ruído, uns 0,3 ms); (3) os resumos `picos: resumo` com `60 na janela`, somados, contra o p95 e o
p99 do `passos`; (4) para cada linha `picos: quadro` na janela, sem `CAPTURA`, qual lado tem o tempo próprio alto e o que o quadro
teve, agrupando por causa.
