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

## 12. O custo do `GsVu1_Unpack`: o estudo e o protótipo da pré-carga (22/10/2026)

Branch `f5-unpack`, sobre a main `1ac0a42`. Depois do `vu_plano` (seção 10) a luta está em 33,8 ms; o `GsVu1_Unpack` leva de
3,4 a 4,1 ms por quadro de luta pesado, todo no caminho da cópia `V4-32`. Convenção: **[medido]**, **[lido]** (no código ou no
binário) e **[estimado]**.

### O caminho, ponta a ponta [lido]

- **De onde vêm os dados.** O `run_chain` (`gs_core.c`) anda a cadeia do quadro e passa os dados de cada etiqueta ao `vif`, que
  chama o `GsVu1_Unpack` com o ponteiro no próprio pacote quando os dados do comando estão todos na etiqueta (o caso comum; senão
  junta no `buf`). Os vértices vêm das cadeias dos modelos, fora dos dois buffers de lista: são as `CNT`/`CALL` externas da
  parte 1a do degrau 1 (`08-threads.md`), uns 1,8 MB por quadro de luta no `f5_duelo` (máximo 2,4), **dados persistentes dos
  modelos**, não montados por quadro. A gravação os lê uma vez por quadro; com os 2,2 MB de textura que passam pelos `REF` no mesmo
  quadro, não cabem na L2 de 1 MB (compartilhada pelos três núcleos): chegam frios. Ficam no heap do jogo, abaixo de `0x40000000`,
  em **páginas de 4 KB** (decisões: leitura aleatória a 345 ns, contra 120 com páginas de 64 KB).
- **Para onde vão.** `vu.mem`, os 16 KB da memória de dados da VU1, no `static struct vu` do `gs_vu1.c`. O endereço é `imm` mais o
  `tops` quando o bit 15 está ligado: o double buffer do `BASE`/`OFFSET` (o `tops` alterna a cada `MSCAL`/`MSCNT`, `vu1_call`).
- **Quem lê.** No caminho do shader (os `hle_program0`, `1`, `2`, `4`, `6`, `78`), o cabeçalho do lote em `top`, os vértices de
  `top + 3` ou `top + 5` e as constantes do começo da memória; os `GsGpu_DrawVu0/4` copiam os vértices inteiros (48 bytes cada)
  para o `gsVuVerts`, e os programas 0 e 1 fazem isso duas vezes, uma por camada; o `GsGpu_DrawVu6` lê dois dos quatro vetores de
  cada vértice. O interpretador (`run`, `kick`) lê o que o programa pedir e é o único que **escreve** na `vu.mem`; o caminho do shader
  nunca escreve nela. O `GsVu1_StateKeep` copia a estrutura inteira.
- **Quantas vezes, de que tamanho** [medido, log do console da seção 9]: de 7,6 a 9,3 mil cópias `V4-32` por quadro de luta pesado,
  de 133 a 153 mil vetores, uns **16,5 vetores (264 bytes, duas ou três linhas de 128) por chamada**, e de 3,4 a 4,1 ms: **uns 440 ns
  por chamada**, 27 ns por vetor. É perto de uma falta de memória com página de 4 KB por chamada (345 ns): o custo é **a primeira
  linha de cada cópia esperando a memória**, não o volume.
- **O `memcpy` do CRT do XDK** [lido, `dumpbin /disasm` do `default.exe`]: alinha o destino a 8 copiando de 1 a 7 bytes, e escolhe
  pelo alinhamento da origem: `ld`/`std` de 8 bytes se ela também ficou alinhada a 8, `lwz`/`stw` de 4 se ficou a 4, byte a byte
  nos outros casos. A partir de 128 bytes ele já pede com `dcbt` até 8 linhas adiante, mas só dentro da própria cópia: a primeira
  linha sempre espera, e uma cópia de 264 bytes quase não tem o que adiantar. Este XDK não tem `XMemCpy` (o `memcpy` é o que faz
  esse papel). **A `vu.mem` está desalinhada:** ela fica no deslocamento 668 da estrutura, e a estrutura começa em `0x82973770`
  (alinhada a 16; lido no `addi r26` do `vu1_unpack`), então a `vu.mem` cai em 12 módulo 16, 4 módulo 8. Com a origem alinhada a
  16 (os dados de um `UNPACK` começam numa palavra de quatro), o `memcpy` copia 4 bytes, fica com a origem em 4 módulo 8 e anda de 4
  em 4 bytes. O mesmo vale para a cópia dos vértices da `vu.mem` para o `gsVuVerts` nos `DrawVu*`.

### As opções

| | O que muda | Ganho no Xenon | Risco | Como se prova |
|---|---|---|---|---|
| **(b) pré-carga** (recomendada) | `dcbt` na origem inteira do `Unpack` e D bytes depois dela (o `Unpack` seguinte vem logo atrás na mesma cadeia), e as duas primeiras linhas da etiqueta seguinte no `run_chain` antes de passar a atual ao `vif` (o `CALL` para a cadeia de outro modelo cai longe). Só no 360 | **1,5 a 3 ms** [estimado]: a espera por chamada sai do caminho se a memória atender as faltas em paralelo; o que sobra é a cópia com os dados na cache. Incerto: se o `dcbt` sobrevive a uma falta de TLB com páginas de 4 KB (se o Xenon o descarta, a primeira linha de cada página nova ainda espera: uma espera a cada 4 KB, não a cada 264 bytes) | Nenhum para a lista: o `dcbt` é um aviso, não falha nem muda o que se copia. O custo de emitir: uns 16 mil `dcbt` por quadro, a maioria de linhas já pedidas | A cópia `V4-32` da linha `por dentro`, em ns por chamada, com a chave ligada e desligada, na mesma build; a mediana depois |
| (d1) `vu.mem` alinhada a 128 | No 360, a `vu.mem` no começo da estrutura, com a estrutura alinhada a 128 (`__declspec(align(128))`): o `memcpy` passa a `ld`/`std` no `Unpack` e na cópia dos vértices dos `DrawVu*` | 0,1 a 0,4 ms [estimado]: metade das cargas e gravações da cópia, com os dados já na cache; soma com a (b) | Nenhum para a lista (só a disposição na memória; o `StateKeep` copia a estrutura inteira). Não cabe numa chave de partida: é de compilação | Duas builds, a cópia `V4-32` e a `VU1` da linha por família |
| (a) sem cópia | Os `hle_program*` leem os vértices direto do pacote (um ponteiro por região da `vu.mem`) | Pequeno, uns 0,3 a 0,5 ms [estimado]: a leitura fria **não some, muda de lugar** (para o `memcpy` do `DrawVu*`, que espera a mesma memória); some só a passagem quente pela `vu.mem` | Médio a alto (seção 5, candidato 7): o interpretador, o `kick`, as constantes do `MSCAL`, o `tops` do double buffer e o `StateKeep` leem a `vu.mem`; cada região teria de virar ponteiro ou ser materializada antes de qualquer leitura fora do caminho do shader | Seções da VU1 iguais quadro a quadro na `x86gv` e na `ppcg` |
| (c) pular o não lido | Copiar só o que o caminho do shader lê | Quase nada [estimado]: o shader lê os vértices inteiros nos programas 0, 1, 2, 4, 7 e 8; o que sobra (dois vetores de cada quatro no programa 6, o cabeçalho de objeto do programa 4, o leque do 6) cai nas **mesmas linhas de 128 bytes** do que se lê, e a espera é por linha | Médio: a mesma regra de quando a cópia pode faltar da (a) | A contagem abaixo |

**Outra, fora do `Unpack` (d2):** no 360 o `getenv` do `gs_internal.h` é o `Port_GetEnv`, que procura o nome numa tabela por
comparação de ponteiros a cada chamada, e o `vu1_call` faz três por chamada (`BT3_VU_DUMP`, `BT3_VU_CALLS` e o `BT3_VU_INTERP` do
programa), com uns 5 mil lotes por quadro de luta [medido, seção 9]. Guardar as três respostas em variáveis estáticas, como o
`BT3_VU1` já faz, tira de 0,2 a 0,8 ms da família da VU1 [estimado, depende do tamanho da tabela]. Barato, e a mesma prova da (d1).

### A contagem no WSL (pendente)

`BT3_UNPACK_CONTA=1`, só na `x86gv` (`headless.py --alvo x86 --gpu --vu1`, condição `BT3_TESTE_VU1`; nem a x86 plana nem o 360 a
têm), escreve por quadro `vu1 unpack: quadro Q: v4 V em C chamadas, lidos L, perdidos P, pendentes no fim E, origem repetida R,
outros caminhos O`: cada vetor que a cópia `V4-32` põe na `vu.mem` fica pendente até uma leitura do caminho do shader (as regiões
dos `hle_program*` e dos `DrawVu*`) ou até um lote que roda o interpretador (que conta a memória toda como lida: o limite de cima);
um pendente que outro `Unpack` sobrescreve não foi lido nunca; as origens (ponteiro, vetores) repetidas no quadro dizem se valeria
lembrar uma cópia entre lotes. A rodada (`X86=x86gv BT3_UNPACK_CONTA=1 vu1_perfil.sh lista x86 f5_duelo LOG`, mais a `.text` da x86
plana) não rodou: o WSL estava fora da rede (`No route to host`) durante todo o estudo. O que ela decide é pouco: a (c) já perde
pela linha de cache e a (a) pela leitura fria que muda de lugar; o que o WSL não mostra de jeito nenhum é a espera de memória do
Xenon (a x86 tem pré-carga em hardware e cache grande), e é isso que decide a (b).

### O protótipo (só no 360, sob chave)

| Commit | O quê |
|---|---|
| `fb1075d` | A contagem acima (`BT3_TESTE_VU1`) e **`BT3_UNPACK_PRE=N`** no `vu1_unpack`: antes da cópia `V4-32`, um `dcbt` por linha de 128 bytes da origem inteira e de mais D bytes depois dela. N = 1 dá D = 1024; N >= 128 é o próprio D; sem a chave ou com 0, a cópia de sempre |
| `e21e93f` | Com a chave, o `run_chain` pede também as duas primeiras linhas da etiqueta seguinte antes de passar ao `vif` os dados da atual |
| `a320928` | O `if` vazio que sobrava da contagem no `hle_program78` sai: o pré-processado da x86 plana do `gs_vu1.c` e do `gs_core.c` fica igual ao da main token a token (`gcc -E -P`, sem espaços) |

O `__dcbt` é declarado como no `ppcintrinsics.h` (o `cl` o emite em linha; o `default.map` não tem o símbolo). Nenhuma linha nova
fora de `PORT_X360` e `BT3_TESTE_VU1`: a `.text` da x86 plana tem de seguir `cc46fa0752f092fca305bc8bf2e2e54390763af0` (a
conferir no WSL com o resto do critério), e as listas da `x86gv` e da `ppcg` não podem mudar porque nenhuma das duas compila a
pré-carga (a `ppcg` não tem `PORT_X360`).

**A build do 360** (`build_f2.sh` no `a320928`, `trocas.tsv` do `bt3xbx-d1`): 317 de 317 fontes sem erro, os mesmos avisos por
fonte que a build da seção 10, os mesmos 103 stubs, `default.dat` `6d30baaf87b1177fb2359a855f8befc0` e `default.swap`
`a7cf723fbfaea860fcc3707ad288e755` iguais; `default.xex` `f824d2ed29df866443b6b18869a1b200`. Três ambientes na pasta da build,
todos com a medição ligada (sem `BT3_TEMPOS=0`) e o resto igual ao da seção 10: `ambiente.txt` com `BT3_UNPACK_PRE=1`,
`ambiente-sem.txt` com `BT3_UNPACK_PRE=0` e `ambiente-4k.txt` com `BT3_UNPACK_PRE=4096`.

### A rodada no console: o que olhar

O roteiro de sempre (START pula o filme; Duelo, Goku contra Kuririn, estágio 5; 60 s de luta), uma abertura por ambiente, na
ordem sem, 1, 4k (o `ambiente.txt` de cada vez renomeado no console). Em cada uma, as linhas `gs: gravação por família, por dentro`
dos trechos de luta:

1. **A cópia `V4-32` em ns por chamada** (ms vezes 10^6 dividido pelas vezes): uns 440 sem a chave. A aposta: com a chave, abaixo
   de 250. Se não cair, o `dcbt` está sendo descartado (a suspeita é a falta de TLB das páginas de 4 KB) e a (b) não rende sem mudar
   a página do heap, o que é outra decisão.
2. A linha `gravação por família` de cima: a `VU1` não pode subir (a cópia dos vértices do `DrawVu*` passa a achar as linhas na
   cache, se subir é concorrência pela L1 da thread vizinha), nem o resto.
3. As capturas `BT3_SHOT=600` iguais às de antes, nenhum `CHEIO`, sem queda.
4. Com o vencedor, uma abertura com `BT3_TEMPOS=0` para a mediana (`passos: janela`) contra os 33,8 ms da seção 10 e o p95 contra
   45,5.

### A recomendação, em ordem

1. **(b), a pré-carga**, se o console confirmar o item 1: a chave vira o padrão no 360 (`BT3_UNPACK_PRE=0` desliga), com a
   distância que der melhor entre 1 KB e 4 KB.
2. **(d2), o `getenv` por lote guardado** e **(d1), a `vu.mem` alinhada a 128**: baratos e sem risco para a lista; uma build com os
   dois, comparada pela linha por família.
3. **(a), sem cópia**, só se depois disso o `Unpack` ainda pesar: rende pouco, porque a leitura fria só muda de lugar, e custa a
   regra de quando a cópia pode faltar.
4. **(c)** não vale: o que não se lê divide as linhas de cache com o que se lê.

**O plano da (b) depois do console:** um commit que tira a chave de teste (a distância vencedora fixa, a chave só para desligar),
no `gs_vu1.c` e no `gs_core.c`, tudo em `PORT_X360`. Provas: o pré-processado da x86 plana igual e a `.text`
`cc46fa0752f092fca305bc8bf2e2e54390763af0` no WSL; as listas da `x86gv` e da `ppcg` com o `divergencias.py` saindo 0 no
`f5_duelo` e no `f5_treino`, com e sem `BT3_GS_THREAD=1 BT3_GS_DESENHO=1` (a prova de que nada fora do 360 mudou, como na seção
10); o critério dos roteiros (`REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino` e `SEMMAPA=1 IMG=1 roteiros.sh duelo`, com
tudo certo e o git sem alterados); e a rodada do console com a mediana, o p95, as capturas e a linha `por dentro`.

## 13. As rodadas no console dos picos e da pré-carga (22/10/2026) [medido]

**Os picos** (`BT3_PICOS=40`, `.xex` `5aa16a31153a2b7d36e4bfb4ac0998d7`, 90 s de luta com especiais e o voo com o A; 2.951 passos,
mediana 38,9 ms com o registro ligado): 1.249 quadros acima de 40 ms. A grande maioria é a **gravação ocupada de 40 a 46 ms** em
cena pesada: carga sustentada, não engasgo isolado. As outras causas: texturas novas decodificadas (mais de 5 ms no quadro, uns 13%
dos lentos), o vblank perdido (o quadro que passa um pouco dos 33,4 ms espera mais um vblank e vira 50 ms, uns 14%) e a lógica do
jogo acima de 25 ms (uns 8%, nos especiais). GPU (uns 10 ms), desenho (uns 17 ms), descarte, disco e capturas quase não aparecem.
Atacar a gravação ataca a média e os picos juntos; as texturas novas são um alvo à parte.

**A pré-carga do Unpack** (`.xex` `f824d2ed29df866443b6b18869a1b200`, medição ligada), contra a rodada da seção 9:

| | Sem pré-carga | 1 KB | 4 KB |
|---|---:|---:|---:|
| Cópia V4-32, por chamada | 439 ns | **360 ns** | 538 ns |
| Unpack por quadro | 2,44 ms | **1,94 ms** | 3,50 ms |
| VU1 por quadro | 16,8 ms | 16,5 ms | 20,1 ms |

A de 4 KB tira do cache o que a VU1 usa. A de 1 KB ficou ligada por padrão (`62da7ba`, `.xex` `0048b67d3a1b086adca787a043cbc7a2`);
a abertura final, sem medição, deu 33,9 ms de mediana numa luta de 2,5 min até o KO, contra 33,8 numa de 31 s: o ganho de uns 0,5 ms
fica abaixo da variação entre lutas jogadas à mão. **Daqui em diante as rodadas de desempenho no console usam o roteiro de controle
(`BT3_ROTEIRO`, o `f5_duelo`), a mesma luta toda vez** (decisão do Lucas, 22/10).


## 14. Os cortes (d1) e (d2), a VU1 por dentro e a rodada pelo roteiro (22/10/2026)

Branch `f5-vu1-dentro`, sobre a main `9cbe3dc`. Tudo em `PORT_X360`: nem a x86 plana, nem a `x86gv`, nem a `ppcg` compilam uma
linha nova. Convenção: **[medido]**, **[lido]** e **[estimado]**.

| Commit | O quê |
|---|---|
| `514cb02` | **(d1)** No 360 o campo `mem` da `static struct vu` (`gs_vu1.c`) leva `__attribute__((aligned(128)))` (o `c89.py` o passa a `__declspec(align(128))`). [lido no `dumpbin`] A estrutura passou de `0x82973790` para `0x82973880` (0 módulo 128) e a `vu.mem` do deslocamento 668 para o 768 (o `tops` do `ori` do `vu1_unpack` foi de 33440 para 33540): a `vu.mem` fica numa linha de 128 bytes, e o `memcpy` do CRT, que escolhe a largura pelo alinhamento, passa de 4 para 8 bytes no `Unpack` e na cópia dos vértices para o `gsVuVerts` |
| `0170b97` | **(d2)** Os três `getenv` por lote do `vu1_call` (`BT3_VU_DUMP`, `BT3_VU_CALLS`, `BT3_VU_INTERP`; no 360 cada um é uma busca do `Port_GetEnv`) lidos uma vez, junto com o `BT3_VU1`. Fora do 360 os macros `VU_ENV_*` expandem no `getenv` de antes (que lá já se lembra da resposta em cada lugar) |
| `ae55fa7` | **A VU1 por dentro** (abaixo) |

### A VU1 por dentro

Num lote em cada quatro (`GS_VU_AMOSTRA`, `gs_internal.h`), com a medição por família ligada, o `GsVu1_Call` se divide em partes
**encadeadas**: cada marca (`GS_VU_MARCA`, o `mftb` em linha) fecha a parte anterior e abre a seguinte, e a última, no fim do lote,
fecha o programa. A soma das partes é o tempo dos lotes da amostra; a linha leva cada parte à VU1 inteira pela proporção.

| Parte | O que leva |
|---|---|
| programa | o `hle_program*` (o cabeçalho, as etiquetas, o `Gs_Gif` dos programas 2, 4, 7 e 8, as constantes do programa 2 e do 78), o `vu1_call` fora do resto, e o que nenhuma outra parte leva |
| estado | o `draw_state` e o `date_snapshot` dos `GsGpu_DrawVu0/4/6` (`gs_draw.c`): a textura, o alvo e o hash de página de dentro da VU1 (o `HV` da linha de cima) |
| constantes | o bloco uniforme (`Vu0Uniform`) |
| índices | a faixa virando triângulos (`gsVuIdx`) |
| cópia | os vértices para o `gsVuVerts`, a primeira cópia do lote (a camada 0, ou a única) |
| cópia da segunda camada | a cópia repetida dos programas 0 e 1 (camadas 1 e 5): os mesmos vértices outra vez |
| fecho | juntar ao desenho anterior (`same_state` e o `memcmp` do uniforme) ou abrir outro, e o `vu_faixa` |
| interpretador | o `run` (os lotes que o caminho do shader não toma, e o `run(0)` de preparação) |

```
gs: gravação por família, VU1 por dentro, quadro Q: VU1 V (L lotes; amostra de A, U us por lote): programa P, estado E,
constantes K, índices I, cópia C (n vezes, v vértices), cópia da segunda camada C2 (n vezes, v vértices), fecho F, interpretador T
```

Em ms por quadro (as vezes e os vértices também levados ao total de lotes); `U` é o custo médio de um lote da amostra, com as
marcas. Escrita logo depois da `por dentro`, também dentro da janela do `passos`. O custo: um lote do programa 0 tem duas chamadas
ao `DrawVu0`, seis marcas cada, mais a do fim: 13 leituras do relógio por lote medido, umas **3 por lote em média** com a amostra
de 1 em 4 (uns 16 mil `mftb` a mais por quadro de luta) [estimado]. Os lotes fora da amostra pagam um teste de `gGsVuMede` por
marca.

### As provas [medido]

- **Build do 360** (`build_f2.sh` no `ae55fa7`, carimbo sem diferença): 317 de 317 fontes sem erro, os avisos por fonte iguais aos da
  main (985 nos fontes do jogo, 8 nos do 360), os mesmos 103 stubs (`diff` com o `stubs.txt` da main), `default.dat`
  `6d30baaf87b1177fb2359a855f8befc0` e `default.swap` `a7cf723fbfaea860fcc3707ad288e755` iguais; **`default.xex`
  `4560d2c731fb902f08300bd07450518d`**. O alinhamento, no `dumpbin` (acima).
- **O pré-processado** (`gcc -E -P`, sem espaços) do `gs_vu1.c`, `gs_draw.c` e `gs_core.c` igual ao da main nas três configurações de
  fora do 360 (a x86 plana, `BT3_TESTE_VU1` da `x86gv`, `PORT_BIG_ENDIAN` da `ppcg`); com `PORT_X360` os três diferem (a comparação
  vê a mudança onde ela existe).
- **WSL**: a `.text` da x86 plana no `ae55fa7` é `cc46fa0752f092fca305bc8bf2e2e54390763af0`, a mesma.
- **A rodada do critério no WSL fica dispensada:** nenhuma das três builds do critério compila uma linha nova (o pré-processado é o
  mesmo), então as listas da `x86gv` e da `ppcg` e os roteiros não podem mudar; o `roteiro.c` e o `passos.c` não foram tocados.

### A rodada no console pelo roteiro de controle

**O que o `f5_duelo` precisa no console** [lido]:

- **O roteiro e o `boot.txt`** na mesma pasta (o `inclui` é relativo à pasta do arquivo que inclui, com `\` ou `/`). O
  `subir_f2.sh` põe os extras na raiz do jogo, então `BT3_ROTEIRO=game:\f5_duelo.txt` e o `game:\boot.txt`.
- **Uma pasta de saves vazia a cada abertura.** O `boot.txt` faz o New Game, responde "sim" ao "criar o save?" e roda o `desbloqueia`,
  que só aceita o cartão vazio no começo (o `roteiro.c` procura `<BT3_SAVES>\card1\BASLUS-21678DBZT3\BASLUS-21678DBZT3`) e o
  `BT3_DESBLOQUEIO=1`. O `plat_mc.c` cria a pasta. Com o `saves19`, ou numa segunda abertura com a mesma pasta, o roteiro para em
  vermelho no boot. Por isso cada ambiente tem a sua pasta (`saves_rsem`, `saves_rmed`); para repetir uma abertura, troque o nome
  da pasta no ambiente (`saves_rsem2`), em vez de apagar no console.
- **`-BT3_DEMO`** (sem ela, a luta de demonstração no lugar dos menus) e **`BT3_NOMOVIE=1`**: as builds sem janela não tocam os
  filmes (o `plat_movie.c` só toca com a GPU), e o `boot.txt` foi feito assim; no console o filme de abertura tocaria antes do título.
- Com o roteiro ligado o `pad360.c` não lê o XInput (os dois controles contam como ligados); depois do `fim` o controle fica parado no
  menu principal, e a saída é pela Guide.

**Com as threads** [lido]: o roteiro anda na leitura do controle do jogador 1 (`Port_PadRead`, na thread do jogo), conta quadros
por essas leituras (`sQuadro`) e lê só o estado do jogo (`gProgress`, `gMcFlow`, `gBtlSeq`, os lutadores); o `passos.c` fecha um
passo na mesma leitura. Nenhum dos dois lê o `gGsFrame` da thread de gravação: com `BT3_GS_THREAD=1` e `BT3_GS_DESENHO=1` o
roteiro vê o mesmo jogo, e o lado do jogo tem o seu contador (`gGsFrameJogo`, do degrau 1) para o que precisar do quadro dele (os
picos). Na `x86gv` o `f5_duelo` já rodou com as duas threads (seção 7). O `Passos_Apresentou` vem da thread de desenho, como nas
rodadas das seções 7 a 13.

**O que o log dá no fim** (`game:\bt3xbx.log`, refeito a cada abertura: baixar entre uma e outra): com o `rastreia luta`, `roteiro:
janela K passos N vblanks V ... fecha MOTIVO`, o `roteiro: resultado ...` e `roteiro: f5_duelo.txt:99 fim vblank V quadro Q`; e para
cada janela a linha `passos: janela K (...): N passos, parede, esperado ...; trabalho p50 p95 p99 máximo ...`. A luta tem **duas
janelas**: a 1 até a pausa do roteiro e a 2 da volta da pausa ao fim do combate. Na referência do WSL (`~/referencias/f5_duelo.telas`)
são **684 e 2.334 passos**, o resultado `vencedor 0x2 razao 0x1 vida 0 27160` (o COM vence por KO) no quadro 7.757 e o `fim` no
quadro 8.378. **Os passos por janela e o resultado iguais aos do WSL provam que o console jogou a mesma luta**; os vblanks não
precisam bater. Com o `ambiente_med.txt`, uns 50 trechos de `gravação por família` com `60 na janela`, cada um com a linha `VU1 por
dentro`.

**Os arquivos** (em `build/x360/f2/` do worktree `bt3xbx-w`): `default.xex`/`.dat`/`.swap`, `f5_duelo.txt` e `boot.txt` (cópias de
`port/x360/roteiros/`), `ambiente_sem.txt` (`BT3_TEMPOS=0`, a mediana) e `ambiente_med.txt` (a medição ligada), os dois com
`-BT3_DEMO`, a ISO convertida, `BT3_GS=gpu`, piso de 32 MB, teto de textura de 128 MB, `BT3_VU1=1`, `BT3_VU_VERTS=98304`,
`BT3_GS_THREAD=1`, `BT3_GS_DESENHO=1`, `BT3_NOMOVIE=1`, `BT3_ROTEIRO=game:\f5_duelo.txt`, `BT3_DESBLOQUEIO=1` e a sua pasta de
saves; sem `BT3_SHOT` (as capturas são picos artificiais) e sem `BT3_PICOS`. O `ambiente.txt` é a cópia do `ambiente_sem.txt`.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_sem.txt build/x360/f2/ambiente_med.txt
# abertura 1 (sem medição); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_med.txt
# abertura 2 (medição ligada); baixar o log de novo
```

**O que olhar:**

1. **A mediana pelo roteiro** (abertura sem medição): o p50 das duas janelas, pesado pelos passos (a 2 domina), contra os 33,9 ms da
   seção 13 (luta à mão, com a pré-carga e sem (d1) e (d2)). É a primeira medida com a mesma luta: ela vira a referência das
   próximas. O ganho esperado de (d1) e (d2) juntos é de 0,3 a 1,2 ms [estimado, seção 12]: some na variação entre lutas à mão, e
   é para isso que a luta passa a ser sempre a mesma.
2. **A VU1 por dentro** (abertura com medição), as medianas dos trechos com `60 na janela`: onde estão os 16 a 25 ms. A aposta antes
   de medir: o estado (a textura e o hash de dentro, uns 2 ms só do hash) e o programa levam a maior parte; as duas cópias, já com
   a `vu.mem` alinhada e os dados recém-desempacotados na cache, ficam abaixo de 1 ms cada; o interpretador só pesa se algum programa
   cair nele (os lotes `[1]` do backend). Se a cópia da segunda camada pesar, o candidato é os programas 0 e 1 escreverem os vértices
   uma vez e as duas camadas os indexarem (o mesmo `first`, outro uniforme).
3. A `VU1` e o `Unpack` da linha de cima contra a seção 13 (16,8 e 1,94 ms com a pré-carga): (d1) mexe nas duas, (d2) só na VU1.
4. Nenhum `roteiro: FALHA` (tela vermelha), nenhum `vigia:`, os passos por janela iguais aos do WSL.

**Fica fora desta rodada, para a próxima:** o `GsVu1_Call` do 360 ainda chama o `QueryPerformanceCounter` duas vezes por lote (o
`gGsVu1Ms` do log do backend, `gs_d3d.cpp`), com a medição ligada ou não, uns 10 mil por quadro de luta, fora da família da VU1 (no
"fora delas"). Trocar pelo `mftb` em linha, ou só com `BT3_TEMPOS`, é um corte barato a medir.

## 15. O estado e o fecho dos lotes da VU1 (22/10/2026)

Branch `f5-estado`, sobre a main `34d07fb`. Medido no console pelo roteiro (seção 14, `f5_duelo`, 33,9 ms de mediana, p95 40,8):
a VU1 leva 16,6 ms por quadro, e por dentro o **estado** 6,2 ms (com uns 2,1 de hash de página) e o **fecho** 4,6, contra programa
2,1, índices 2,0, constantes 1,0 e cópias 1,0, em uns 4.500 lotes. Convenção: **[medido]**, **[lido]** e **[estimado]**.

### O que as duas marcas cobrem [lido, na main]

- **Estado** (do `GS_VU_MARCA(GS_FAM_V_PROG)` da entrada dos `GsGpu_DrawVu0/4/6`, `gs_draw.c:1151`, `1247`, `1328`, ao
  `GS_FAM_V_ESTADO`): os testes de capacidade, o `draw_state` (`:762`) e o `date_snapshot` (`:936`). O `draw_state` lê uns dez
  registradores do contexto, anda a tabela de alvos duas vezes (`GsDraw_TargetGet`, `:673`, uma busca linear em até 24 vagas, para o
  alvo e para a vaga em `tbp / 32`), faz o `memset` dos 128 bytes do `GsDraw`, chama o `texture_get` (`:321`: a tabela `memo[16]` por
  TEX0 e época das páginas, o `sLast`; quando a época mudou, o hash das páginas, que é o `HV` da linha da família) e o
  `pipeline_get` (`:651`), cujo `pipeGet` do backend (`gs_d3d.cpp:1399`) é uma busca linear nos pipelines (29 numa luta), e
  converte uns seis inteiros para float (no Xenon cada conversão passa pela memória). **Proporcional ao número de chamadas**, com o
  hash proporcional às páginas reenviadas desde a última vez.
- **Fecho** (do fim da cópia ao `GS_FAM_V_FECHO`, `:1398` a `:1419` no `DrawVu0`, igual nos outros dois): o `draws++` e o `gen` do
  alvo, o `same_state` (`:923`, uns 15 campos e quatro `memcmp` de 16 bytes), e então ou o `memcmp` do bloco uniforme inteiro contra
  o do desenho anterior (304 bytes; só quando o `same_state` passa) e a junção com a `vu_faixa`, ou a cópia do bloco de 304 bytes da
  pilha para o `gsVuUni`, a `vu_faixa` e a cópia dos 128 bytes do `GsDraw`. **Proporcional às chamadas**, com 304 bytes lidos duas
  vezes em cada junção e 432 bytes copiados em cada desenho novo.

### O que se repete [medido na `x86gv`]

A chave de teste `BT3_VU_ESTADO_CONFERE=1` (abaixo) conta, nos 4.000 quadros a partir do primeiro desenho da VU1 do `f5_duelo` (com
a divisão), **15.416.833 chamadas** dos `DrawVu*` (uns 3.850 por quadro):

| | f5_duelo | f5_treino |
|---|---:|---:|
| chamadas | 15.416.833 | 10.919.766 |
| a mesma entrada da chamada anterior (uma vaga só) | 7.599.400 (49,3%) | 4.165.937 (38,2%) |
| a mesma entrada da última chamada do mesmo contexto (uma vaga por contexto) | **13.268.076 (86,1%)** | **9.113.099 (83,5%)** |
| desses, com a época das páginas mudada (o `texture_get` chamado de novo) | 49.392 | 19.137 |
| o fecho: o desenho anterior é o do lote anterior | 15.390.852 (99,8%) | 10.899.229 |
| o fecho: o mesmo estado (`same_state`) | 8.206.506 (53,2%) | 4.720.889 |
| o fecho: junta (o bloco uniforme igual, os 304 bytes comparados) | 7.819.830 (50,7%) | 4.391.792 (40,2%) |
| o fecho: desenho novo (bloco e desenho copiados) | 7.597.003 (49,3%) | 6.527.974 |

Com uma vaga só, 88% das chaves erradas eram **só o contexto**: as duas camadas dos programas 0 e 1 se alternam entre o contexto 0
e o 1 a cada chamada. Com uma vaga por contexto, o que sobra de erro é o TEX0 (2.120.257 das 2.148.757 no `f5_duelo`: o lote seguinte
com outra textura). Por programa, no `f5_duelo`: o 0 (lutadores) 9.341.804 acertos de 11.358.472, o 4 (cenário) 3.926.272 de
4.054.707, o 6 (sombra) nenhum de 3.654 (todos deram certo sem entrar na memória: a sombra lê a página da sombra, um alvo como textura).
No fecho, a junção custa a comparação inteira (em média 304 bytes, uns 594 KB por quadro) e a diferença, quando o estado é o
mesmo, aparece nos primeiros bytes (5 em média).

### O que mudou

| Commit | O quê |
|---|---|
| `2641cf5` | **A memória do estado** (`draw_state_vu`, `gs_draw.c`). O resultado de um `draw_state` que deu certo fica guardado com a entrada que ele leu: PRIM, TEX0, TEST, CLAMP, SCISSOR, FRAME, ZBUF, ALPHA, FBA e TEX1 do contexto, o TEXA, o contador `sAlvosMuda` (sobe a cada vaga de alvo que muda de dono e na troca de escala), o quadro, a textura branca e a chave dos pacotes de textura. Com a mesma entrada o resultado é o mesmo, fora do que depende da memória do GS: o alvo como textura nunca entra na memória, e a condição que o escolheria (o `cleared` e o `gen` da vaga em `tbp / 32`) é refeita em todo acerto; a textura vale se a época das páginas é a mesma e a entrada do cache segue no lugar com o mesmo TEX0, TEXA e `gen` (é ela que o `texture_get` acharia), senão o `texture_get` é chamado e, sendo uma entrada sem substituição de pacote (ou a branca da falta de espaço), o resultado é o da memória com a textura nova. Os efeitos do `draw_state` (o `last` do alvo, da vaga lida e da textura, todos iguais ao quadro, que está na chave) já valem num acerto; o `sLast` é posto de novo. Um `draw_state` que recusou o desenho nunca entra. **A chave de teste** `BT3_VU_ESTADO_CONFERE=1`, só nas builds `--gpu`: em todo acerto o `draw_state` completo roda também e é o resultado dele que vale; as diferenças (o `GsDraw` byte a byte, o `us`, o `vs` e o `sLast`) são contadas, com as contagens acima numa linha a cada 1.000 quadros |
| `0ca281a` | A conferência conta por programa e pelo campo da chave que mudou (de onde saiu a vaga por contexto) |
| `3cf2843` | **Uma vaga por contexto** |
| `0804ac3` | **O fecho** (`vu_fecho`): o bloco uniforme é montado direto na vaga seguinte do `gsVuUni` (`pu`; o macro `VU_U` é `u` no `#else`), então o desenho novo não copia mais os 304 bytes da pilha, e a junção deixa a vaga para o próximo (nada lê o `gsVuUni` além da contagem); o `same_state` e a comparação do bloco viram comparações de palavras de 32 bits em linha (`same_state_vu`, `iguais32`), com o mesmo resultado do `memcmp` (igual a 0 é todas as palavras iguais; os `float` do `misc` e do `blendc` seguem comparados como `float`) |
| `f1626b0` | **O `QueryPerformanceCounter` do `GsVu1_Call`** (`gs_vu1.c`, só no 360): as duas leituras por lote do `gGsVu1Ms` só com a medição ligada (`gGsFam`); com `BT3_TEMPOS=0` o tempo da VU1 do log do backend fica zero |

Tudo em `BT3_GS_ESPERAS`, com o código de antes no `#else`; o pré-processado da x86 plana é o mesmo da main (`gcc -E -P`), e o
360 só compila o `vu_fecho`, a memória e a leitura do relógio novas. Não muda a ordem de nada que o backend vê: os pedidos de
textura e de alvo novos acontecem nos mesmos lotes e na mesma ordem.

### As provas no Deck [medido]

Script `~/provas-estado.sh`, resumo `~/estado/provas.resumo`, listas e logs em `~/estado/`, em série. Antes: `34d07fb` (a main).
Depois: `f1626b0`. A pasta de dados dos dois roteiros feita pelo `roteiros.sh f5_treino f5_duelo` sem PPC (abaixo).

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `cc46fa0752f092fca305bc8bf2e2e54390763af0`, a mesma |
| `x86gv`, `f5_treino` (7.386 quadros) e `f5_duelo` (8.380), sem chaves e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1`, antes contra depois | os quatro com `divergencias.py` saindo 0, hashes `f76f0f6849b64629` e `08a98b6d05b8775a` |
| `ppcg`, os mesmos quatro casos | os quatro com 0 (hash `994627a2aecd429a` no `f5_duelo`, o de sempre); de 242 a 470 s por rodada, 5 a 10% acima das de antes (213 a 430 s): no `qemu` o tempo não diz nada do Xenon |
| A conferência, `x86gv`, os dois roteiros sem e com a divisão, e `ppcg` no `f5_duelo` com a divisão | **0 diferentes** em todas: 13.268.076 acertos conferidos no `f5_duelo` (os mesmos na `ppcg`), 9.113.099 no `f5_treino` |
| Sabotagem: o TEX0 fora da chave (`x86gv`, `f5_duelo` com a divisão) | com a conferência, **1.335.426 diferentes**, a primeira no quadro 4.136 (`vu 1`, textura 204 no completo, 203 na memória); sem ela, a lista diferente em 4.197 quadros (do 4.136 ao 8.379), hash `9852d2727d8a5df3`, `divergencias.py` sai 1 |
| O git do Deck no fim | 0 alterados (a sabotagem desfeita por `git checkout`, com `assert` de que casou, e a `x86gv` refeita) |

### A build do 360

`build_f2.sh` no `f1626b0`: 317 de 317 fontes sem erro, os avisos por fonte iguais aos da main (985 nos fontes do jogo, 8 nos do
360), os mesmos 103 stubs (`diff` com o `stubs.txt` da main), `default.dat` `6d30baaf87b1177fb2359a855f8befc0` e `default.swap`
`a7cf723fbfaea860fcc3707ad288e755`, iguais; **`default.xex` `c432303484a0d6b4c0e6d260e259811e`**.

### A estimativa [estimado]

O estado fora do hash (uns 4,1 ms) passa a ser pago por inteiro em 14% das chamadas; um acerto custa as 17 comparações da chave, a
conferência da textura e a cópia dos 128 bytes, perto de um décimo de um `draw_state`: de 4,1 para perto de 1 ms, **uns 3 ms a
menos**. O hash (2,1) fica: ele é o trabalho de reconhecer as páginas reenviadas. O fecho perde as cópias do bloco nos desenhos
novos e as chamadas ao `memcmp`: **de 0,5 a 1,5 ms**, se o custo for de instruções; se for a escrita em linhas que não estão na cache
(os desenhos novos escrevem uns 0,8 MB por quadro no `gsDraws` e no `gsVuUni`), o ganho é menor e o próximo corte é zerar a linha antes de escrever
(`__dcbz128`, com as listas alinhadas a 128). O `QueryPerformanceCounter` tirado (uns 9 mil por quadro) fica fora da família da VU1,
no "fora delas". Se a gravação seguir sendo o gargalo, a luta de 33,9 para perto de 30 a 31 ms.

### A rodada do critério (no Deck, disparada sem esperar)

O script `~/rodada-estado.sh` do Deck traz o HEAD do `f5-estado`, refaz as três builds do critério (`headless.py --alvo x86`,
`--alvo x86 --rastreio`, `--alvo ppc`), confere a `.text` da x86 plana e roda em série `REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo
f5_treino` e `SEMMAPA=1 IMG=1 roteiros.sh duelo`. Saída em `~/rodada-estado.log`, logs por passo em `~/rodada-estado-*.log`, resumo
em `~/rodada-estado.resumo` (uma linha por passo com `RC=`, a `.text`, o git no fim e `FIM`). Vale com todos os `RC=0`, os dois
`roteiros.sh` com "tudo certo", a `.text` a mesma e o git sem alterados.

### A rodada no console pelo roteiro

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-e`: `default.xex`/`.dat`/`.swap`, `f5_duelo.txt` e `boot.txt` (cópias de
`port/x360/roteiros/`), `ambiente_sem.txt` (`BT3_TEMPOS=0`, saves `saves_e1`) e `ambiente_med.txt` (a medição ligada, saves
`saves_e2`), o resto como na seção 14; o `ambiente.txt` é a cópia do `ambiente_sem.txt`. Duas pastas de saves novas, porque o roteiro
só aceita o cartão vazio.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_sem.txt build/x360/f2/ambiente_med.txt
# abertura 1 (sem medição); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_med.txt
# abertura 2 (medição ligada); baixar o log de novo
```

**O que olhar:**

1. **A mediana pelo roteiro** (abertura sem medição): o p50 das duas janelas, pesado pelos passos, contra os **33,9 ms** da seção 14;
   o p95 contra 40,8. Os passos por janela (684 e 2.334) e o resultado iguais aos da referência.
2. **A VU1 por dentro** (abertura com medição): o estado perto de 3 ms (o hash dentro dele, uns 2), o fecho abaixo de 4,6, e a VU1
   inteira contra os 16,6. Se o fecho cair pouco, o custo é a escrita em linhas novas, e o próximo corte é o `__dcbz128`.
3. O "fora delas" da linha da família contra o da seção 14: o `QueryPerformanceCounter` tirado sai dali, mas só na abertura sem
   medição (com a medição ligada ele continua sendo lido).
4. Nenhum `roteiro: FALHA`, nenhum `vigia:`, nenhuma textura branca nova.

**Fica para a próxima:** os **índices** (2,0 ms): o laço `gsVuIdx[gsVuIdxCount++] = gsVuVertCount + i + k` escreve por um
`uint32_t *` e lê e escreve dois `uint32_t` globais; o `cl` não pode supor que não se sobrepõem e relê e regrava os contadores a
cada índice (no Xenon, uma leitura logo depois da escrita no mesmo endereço espera a escrita). Com os contadores em variáveis locais o
laço fica só com a escrita do índice [estimado: de 1 a 1,5 ms].

## 16. Os índices da VU1 com o contador em locais (22/10/2026)

Branch `f5-indices`, sobre o `f5-estado` (`646e8ca`). Medido no console pelo roteiro com o `f5-estado` (o `f5_duelo`): a luta em
**33,2 ms de mediana** (meta 33,37), p95 34,9, p99 40,3; por dentro da VU1 os **índices** levam 1,9 ms por quadro. Convenção:
**[medido]**, **[lido]** e **[estimado]**.

### O corte

| Commit | O quê |
|---|---|
| `e7d5307` | Nos três `GsGpu_DrawVu*` (`gs_draw.c`: `DrawVu6` na `:1585`, `DrawVu4` na `:1715`, `DrawVu0` na `:1838`) o laço dos índices passa a guardar o ponteiro (`idx`), a base (`base`, o `gsVuVertCount`) e a posição (`n`, o `gsVuIdxCount`) em locais, escreve os três índices do triângulo (`idx[n]`, `idx[n + 1]`, `idx[n + 2]`, os mesmos `base + i + k`, na mesma ordem) e grava o contador de volta uma vez, no fim do laço. O `k` some das declarações (`uint32_t i, flag;` e `uint32_t i;`) para não sobrar variável sem uso (o C4101 mudaria os avisos). Em `BT3_GS_ESPERAS`, com o laço de antes no `#else` |

**Por que pesava** [lido no `link /dump /disasm` do `gs_gs_draw.obj`, `DrawVu4`]: antes, cada iteração tinha **30 instruções**,
com as três escritas do índice (`stwx`), **nove leituras de globais** (o `gsVuIdx`, o `gsVuVertCount` e o `gsVuIdxCount`, relidos
depois de cada escrita, porque o `uint32_t *` pode apontar para eles) e **três escritas do contador** (`stw r10,0(r31)`), cada uma
seguida pela leitura do mesmo endereço na escrita seguinte: no Xenon, uma leitura logo depois de uma escrita no mesmo endereço
espera a escrita chegar à cache (o *load-hit-store*). Depois, **12 instruções** por iteração: três `stw`/`stwu` em `idx` e as
somas, nenhuma leitura nem escrita de global; o contador é gravado uma vez depois do laço (`stw r9,0(r23)` no `0x2C4`). No `DrawVu0` e
no `DrawVu6` o laço novo é o mesmo, com o teste da marca do vértice.

**O resultado é o mesmo byte a byte**: os índices são os mesmos e saem na mesma ordem; o `d.first` é lido antes, o `d.count` depois
da gravação de volta, como antes; o `vu_faixa` lê o `gsVuIdx` já escrito.

### A estimativa [estimado]

Até uns 43 mil triângulos por quadro de luta (o maior quadro tem 128.907 índices, seção 8): antes, três esperas de escrita e leitura por triângulo
(129 mil por quadro, umas dezenas de ciclos cada) mais as 30 instruções; depois, 12 instruções sem espera. De **1,9 para uns 0,3 a
0,5 ms**: **de 1,4 a 1,6 ms a menos**, a luta de 33,2 para perto de 31,7 ms, se a gravação seguir sendo o gargalo.

### As provas no WSL [medido]

Serviço `claude-indices`, script `~/indices/rodada-indices.sh`, resumo `~/indices/provas.resumo`, listas e logs em `~/indices/`, num
worktree próprio do WSL (`~/bt3xbx-i`), cada passo pesado dentro do `flock ~/wsl.lock`; o clone `~/bt3xbx` ficou em `f5-pingpong`,
sem alterados. Antes: `646e8ca` (o `f5-estado`). Depois: `e7d5307`. A pasta de dados dos dois roteiros feita pelo `roteiros.sh
f5_treino f5_duelo` sem PPC no `e7d5307` ("tudo certo").

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `cc46fa0752f092fca305bc8bf2e2e54390763af0` antes e depois, a mesma (a referência do `f5-estado`; a main já tem a `ee54716`, da F6) |
| O pré-processado (`gcc -E -P`, sem espaços) do `gs_draw.c` | igual ao do `f5-estado` na x86 plana; diferente com `BT3_TESTE_VU1` e com `PORT_BIG_ENDIAN` (a comparação vê a mudança onde ela existe) |
| `x86gv`, `f5_treino` (7.386 quadros) e `f5_duelo` (8.380), sem chaves e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1`, antes contra depois | os dois com `divergencias.py` saindo 0, hashes `f76f0f6849b64629` e `08a98b6d05b8775a`, contadores do jogo e do desenho iguais, 0 desenhos com textura já solta |
| `ppcg`, os mesmos dois roteiros | os dois com 0, hashes `f76f0f6849b64629` e `994627a2aecd429a` (os de sempre) |
| Sabotagem: o `DrawVu6` sem gravar o contador de volta (o erro que este corte poderia ter), `x86gv`, `f5_duelo` | a lista diferente em **2.186 quadros** (do 4.136 ao 8.229; contagens, desenhos, vértices e índices da VU1 e constantes), hash `e6ef73eebea0ccbc`, `divergencias.py` sai 1 |
| O git do worktree no fim | 0 alterados (a sabotagem com `assert` de que casou, desfeita por `git checkout`, e a `x86gv` refeita) |

### A build do 360

`build_f2.sh` no `e7d5307`: 317 de 317 fontes sem erro, os avisos por fonte iguais aos do `f5-estado` (985 nos fontes do jogo, 8 nos
do 360, arquivo por arquivo), os mesmos 103 stubs (`diff` com o `stubs.txt` do `f5-estado`), `default.dat`
`6d30baaf87b1177fb2359a855f8befc0` e `default.swap` `a7cf723fbfaea860fcc3707ad288e755`, iguais; **`default.xex`
`73b2d65ba52f291a3cbc9e613daac94f`**. O laço novo no `link /dump /disasm` do `.obj` (acima).

### A rodada do critério (no WSL, disparada sem esperar)

O serviço `claude-indices` (`systemd-run --uid=codex ... MemoryMax=5G`), script `~/indices/rodada-indices.sh`, num worktree próprio
(`~/bt3xbx-i`, destacado, com os links dos dados e uma cópia do `port/build/gen`), cada passo pesado dentro do `flock ~/wsl.lock`.
Depois das provas acima, refaz as três builds do critério no HEAD (`headless.py --alvo x86`, `--alvo x86 --rastreio`, `--alvo ppc`),
confere a `.text` da x86 plana e roda em série `REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino` e `SEMMAPA=1 IMG=1 roteiros.sh
duelo`. Logs em `~/rodada-indices-f5.log` e `~/rodada-indices-duelo.log`, resumo em `~/rodada-indices.resumo` (uma linha por passo
com `RC=`, a `.text`, o git do worktree e do clone no fim e `FIM`). Vale com todos os `RC=0`, os dois `roteiros.sh` com "tudo certo",
a `.text` a mesma e o git sem alterados.

### A rodada no console pelo roteiro

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-i`: `default.xex`/`.dat`/`.swap`, `f5_duelo.txt` e `boot.txt` (cópias de
`port/x360/roteiros/`), `ambiente_sem.txt` (`BT3_TEMPOS=0`, saves `saves_i1`) e `ambiente_med.txt` (a medição ligada, saves
`saves_i2`), o resto como na seção 14; o `ambiente.txt` é a cópia do `ambiente_sem.txt`.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_sem.txt build/x360/f2/ambiente_med.txt
# abertura 1 (sem medição); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_med.txt
# abertura 2 (medição ligada); baixar o log de novo
```

**O que olhar:**

1. **A mediana pelo roteiro** (abertura sem medição): o p50 das duas janelas, pesado pelos passos, contra os **33,2 ms** do
   `f5-estado` (meta 33,37); o p95 contra 34,9 e o p99 contra 40,3. Os passos por janela (684 e 2.334) e o resultado iguais aos da
   referência.
2. **A VU1 por dentro** (abertura com medição): os índices perto de 0,3 a 0,5 ms (eram 1,9), o resto das partes como no `f5-estado`.
3. Nenhum `roteiro: FALHA`, nenhum `vigia:`.

## 17. As texturas novas fora do caminho da gravação: o estudo, a medida e a decodificação por linha (22/10/2026)

Branch `f5-texturas`, sobre a main `193821f`. Os picos da seção 13: uns 13% dos quadros lentos da luta tinham mais de 5 ms de
texturas novas decodificadas, nos especiais e nas trocas de câmera. Pedido do Lucas: tirar esses engasgos sem nenhum quadro a mais
de atraso entre o controle e a tela. Convenção: **[medido]**, **[lido]** e **[estimado]**.

### O caminho da textura nova [lido]

1. **O `texture_get` (`gs_draw.c`)** procura a textura por `TEX0`, `TEXA` e `gen`. O `gen` é o conteúdo: um hash das páginas de 8 KB
   que a textura ocupa (`Gs_PageHash`, refeito só depois de um envio tocar a página; o resultado do laço fica numa tabela por `TEX0`
   enquanto a época das páginas for a mesma) e, com paleta, o `clut_hash` das 16 ou 256 entradas. Acerto: a mesma entrada do
   desenho anterior (`sLast`) ou o balde. Erro: textura nova.
2. **O espaço** (só no 360): `GsDraw_X360Abre` pergunta ao backend quanto falta (piso de memória livre, teto das texturas) e só
   descarta quando falta, as usadas há mais tempo, nunca as dos dois últimos quadros.
3. **A decodificação**: `malloc` de `w x h x 4` e, **por pixel**, um `vram_rw` da textura (o `switch` do formato, as tabelas de
   bloco e coluna, o `GS_LE32`), mais um `vram_rw` da paleta (8 e 4 bits) e um `Gs_Expand` (que relê o `TEXA` a cada chamada).
4. **A criação no backend** (`gs_d3d.cpp`): `tex_create` pelo XG (`XGSetTextureHeader` e `XPhysicalAlloc` de escrita combinada) e
   `tex_upload`, a cópia linha a linha para a memória da textura e a varredura das pretas (para na primeira cor); `free`.

**O custo no console** [medido, o log dos picos da seção 13, `BT3_PICOS=40`]: a decodificação leva **110 ns por pixel** (quartis 108
e 114 nos 367 quadros com texturas, muito estável: o custo é por pixel). Nos quadros com mais de 5 ms de texturas, a mediana é 9
texturas, 77 mil pixels e 8,3 ms; o XG (o `XPhysicalAlloc`) leva 1,4 ms em média nesses quadros, uns 5% da decodificação. Na x86 o
mesmo laço leva 12,2 ns por pixel dentro do jogo [medido]: o Xenon paga 9 vezes mais pelas duas chamadas e os acessos por pixel.

### A medida no WSL [medido]

A contagem `BT3_TEX_CONTA=arquivo` (`aa9f404`, só nas builds `--gpu` do WSL) escreve uma linha por textura decodificada: o quadro, o
tamanho, os formatos, a chave do cache, um hash do conteúdo decodificado, o tempo e a conferência da decodificação nova (abaixo).
O `port/x360/tools/tex_conta.py` separa cada decodificação em: **primeira** (o conteúdo nunca visto), **descartada** (a mesma chave
já decodificada: a entrada saiu do cache e voltou), **página** (o mesmo `TEX0` e `TEXA`, o mesmo conteúdo, outro `gen`: uma página
que a textura ocupa mudou sem mudar a textura) e **outro TEX0** (o mesmo conteúdo, já decodificado noutro endereço ou com outra
paleta igual). O custo no console é estimado pelos pixels a 110 ns.

**`f5_duelo`, `x86gv`, 4:3** (8.380 quadros; a luta, do combate controlável ao KO, nos quadros 4.673 a 7.758 do GS):

| | O roteiro inteiro | A luta |
|---|---:|---:|
| Texturas decodificadas | 2.424 | 1.798 |
| Pixels | 37,8 M | 19,2 M |
| Quadros com decodificação | 546 | 403 de 3.085 |
| Primeira vez (dos pixels) | 55,1% | **19,0%** |
| Página mudou, textura igual | 28,1% | **49,2%** |
| Outro `TEX0`, textura igual | 16,8% | **31,8%** |
| Descartada e decodificada de novo | 0 | 0 |

Formatos: 8 bits 1.897 texturas (23,8 M pixels), 4 bits 521 (13,9 M), 32 bits 6 (0,2 M); nenhum de 16 bits nem dos outros.
Dos "outro `TEX0`", 606 de 699 mudam o `TBP` e o `CBP` juntos: a mesma imagem enviada outra vez a outro endereço, com a paleta.

**Os quadros da luta pela estimativa no console** (quantos quadros em cada faixa de ms de decodificação):

| Faixa | Hoje (110 ns por pixel) | Com a decodificação por linha (uns 10 ns, [estimado]) | Só a primeira vez, hoje |
|---|---:|---:|---:|
| até 1 ms | 112 | 384 | 23 (e 345 sem nada) |
| 1 a 5 ms | 173 | 16 | 22 |
| 5 a 10 ms | 98 | 2 | 2 |
| 10 a 20 ms | 3 | 0 | 0 |
| 20 ms ou mais | 17 | 1 | 11 |

**Onde caem as rajadas** (5 ms ou mais no roteiro inteiro, 177 quadros): 83 no laço de golpes e técnicas depois da pausa, 26 na
tela de escolha dos lutadores, 12 nos primeiros golpes, 8 na técnica com câmera (o Blast 2), o resto em menus e na carga. O maior da
luta (4:3) é o **quadro 5.338, o Blast 2**: 65 texturas, **3,2 M pixels (uns 350 ms no console), todas já decodificadas antes com outro
`TEX0`** (o jogo reenvia o conjunto a outro endereço); depois os quadros 7.015 a 7.017 (557, 282 e 504 mil pixels, todos de página
mudada) e os 4.976 a 4.980 (291 mil pixels de primeira vez em cada quadro: um efeito que muda a cada quadro).

**Leitura.** A rajada não vem do descarte por uso (zero), e no console o descarte só tira texturas sem uso há milhares de quadros
(as pressões da seção 13). Quatro quintos dos pixels da luta são imagens que o cache já tinha, com outra chave; o resto, de
primeira vez, é pouco por quadro, exceto num efeito de cinco quadros. E o custo por pixel do laço é o que transforma qualquer
rajada em engasgo.

**Em 16:9** (`BT3_WIDE=1`, decisão 26; o mesmo roteiro, a câmera mais aberta) [medido]: a câmera aberta **não traz mais
texturas**. O roteiro inteiro decodifica 2.410 texturas (contra 2.424 em 4:3); a luta, 1.785 texturas e 18,8 M pixels em 400
quadros (contra 1.798, 19,2 M e 403), com as mesmas proporções: primeira vez 19,3%, página 48,4%, outro `TEX0` 32,3%, descartada 0.

| Faixa (a luta, 16:9) | Hoje (110 ns por pixel) | Com a decodificação por linha (uns 10 ns, [estimado]) | Só a primeira vez, hoje |
|---|---:|---:|---:|
| até 1 ms | 111 | 383 | 23 |
| 1 a 5 ms | 174 | 15 | 22 |
| 5 a 10 ms | 97 | 0 | 2 |
| 10 a 20 ms | 3 | 1 | 0 |
| 20 ms ou mais | 15 | 1 | 11 |

A diferença está nos quadros da página mudada perto do fim da luta: em 16:9 eles se juntam num só (o 7.017, 18 texturas e 1,08 M
pixels, uns 120 ms hoje e uns 11 com a decodificação por linha [estimado]), em 4:3 se espalham por três. O Blast 2 é o mesmo
(quadro 5.338, 65 texturas, 3,2 M pixels de outro `TEX0`).

### As opções

**(d) A decodificação por linha (feita, abaixo).** O laço por pixel trocado por um por linha, com as tabelas do envio (`sIn8`,
`sIn4`, `sIn32`, `sIn16`: as mesmas contas do `vram_rw`, em páginas) e a paleta lida e expandida uma vez (256 ou 16 entradas). Os
mesmos pixels. Ganho: todas as decodificações, primeira vez ou não; na x86, de 12,2 para 1,19 ns por pixel dentro do jogo
[medido], 10 vezes. No `dumpbin` do `.exe` do 360 o laço dos 8 bits tem 12 instruções por pixel, três leituras (a tabela, o byte
da textura, a paleta na pilha) e uma escrita, sem chamada [lido]; no Xenon, de 110 para uns 8 a 15 ns por pixel com a escrita e a
cópia para a memória da textura [estimado]: as rajadas de 5 a 10 ms viram menos de 1 ms, e o Blast 2 de uns 350 para uns 30 ms.
Risco: baixo (a conferência pixel a pixel em cada textura dos roteiros, nas duas ordens de byte). Não muda nada fora do
`texture_get` nem o momento em que a textura fica pronta: nenhum atraso.

**(a) Um cache pelo conteúdo da origem** (o próximo passo, se a rodada do console mostrar que vale). Hoje a chave é `TEX0` + `gen`
das páginas; a proposta é, no erro, calcular uma segunda chave só do que a decodificação lê (os blocos da textura, não as páginas
inteiras; o `clut_hash` da paleta; o formato, o tamanho e o `TEXA`) e, se ela já existe, **reusar o `GsTex`** sem decodificar nem
criar. Pega a "página" (49%) e o "outro `TEX0`" (32%) da luta, inclusive o Blast 2 inteiro. Custo: um hash dos bytes da textura no
erro (64 KB num 256 x 256 de 8 bits, perto de 1 ns por byte [estimado], contra 7 ms de decodificação hoje). Risco: médio. Duas
entradas apontando o mesmo recurso pedem contagem de referências no descarte por uso (`GsDraw_X360Abre`), na varredura do cache
cheio e na fila de liberação do desenho; e as listas do `gpu_falso` mudam de propósito (o número de texturas novas e os índices
dos recursos entram no hash), então a prova é a lista normalizada (`BT3_GPU_NORMAL`, que tira o que depende de quantas texturas se
decodificaram) mais o hash do conteúdo de cada desenho. Com (d) feita, o que sobra para (a) são uns 30 ms no Blast 2 e uns 5 ms
nos quadros 7.015 a 7.017 [estimado].

**(b) Decodificar adiantado.** No envio (`transfer_data`) não serve: o envio não sabe o formato, a paleta nem o tamanho com que a
página será lida (vêm no `TEX0` do desenho), a mesma página serve a várias texturas, e o jogo reenvia uns 2,5 MB por quadro de
luta, quase tudo igual ao que já estava (seções 7 e 8): decodificar no envio custaria mais que hoje. Numa thread com folga (a de
hardware 1, a do diário, quase parada) daria sem atraso: a gravação só precisa do `GsTex` para gravar o desenho, e os pixels só
na thread de desenho, que desenha a lista depois de a gravação acabar. A gravação copiaria os blocos da origem (a VRAM muda com os
envios seguintes do mesmo quadro), criaria o recurso e entregaria a decodificação; o desenho esperaria a decodificação daquela
textura antes do primeiro desenho com ela. Ganho: o que (d) deixar, sem passar do quadro. Risco: alto (uma thread, a cópia, a
espera do desenho, a posse do recurso no descarte). Só se (d) e (a) não bastarem.

**(c) Espalhar** o que não fica pronto (desenhar um quadro sem a textura, ou com a anterior) muda a imagem: não serve.

### O protótipo: a decodificação por linha sob `BT3_TEX_RAPIDA`

| Commit | O quê |
|---|---|
| `5d504a9` | `Gs_TexDecode` (`gs_core.c`, em `BT3_GS_ESPERAS`): a textura inteira uma linha por vez com as tabelas do envio e a paleta expandida uma vez; 8 e 4 bits com qualquer paleta, 32 e 16 bits (o `Gs_Expand` dos 16 bits com o `TEXA` lido uma vez); os outros formatos devolvem 0 e ficam com o laço. No `texture_get`, `BT3_TEX_RAPIDA=1` (lida uma vez) a usa; **desligada por padrão**; o laço de antes inteiro no `#else` (o `TEX_LACO` vira `!replaced`, o mesmo texto de antes) |
| `aa9f404` | A contagem `BT3_TEX_CONTA` e o `tex_conta.py` (acima). A conferência: com a chave desligada, cada textura decodificada pelo laço é decodificada outra vez pelo `Gs_TexDecode` noutro buffer e comparada byte a byte |

**As provas no WSL** [medido]. Serviço `claude-tex`, script `~/tex/rodada-tex.sh`, resumo `~/tex/provas.resumo`, num worktree
próprio (`~/bt3xbx-tex`, do pacote `tex.bundle`), cada passo pesado dentro do `flock ~/wsl.lock`; o clone ficou em `f5-pingpong`,
sem alterados.

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `ee54716ba7d86744152b9b71e6ac8e500f985d97` na main (`193821f`) e no `aa9f404`, a mesma (a referência da main) |
| Teste de unidade (VRAM aleatória, 1.500 texturas de 1 a 256 de lado, os cinco formatos, `TBW` 0 a 16, `TEXA` e paleta sorteados): o `Gs_TexDecode` contra o laço | 0 diferentes na x86 e **0 na PPC de verdade** (`powerpc-linux-gnu-gcc -static` no `qemu-ppc`). Na x86 com `-DPORT_BIG_ENDIAN` dá 1.179 diferentes: ali o `GS_LE32` troca os bytes mas a memória segue little-endian, e o caminho por byte dos 8 e 4 bits (o mesmo do envio) não é simulado; só a PPC prova a ordem dos bytes |
| A conferência em cada textura dos roteiros (chave desligada) | `f5_duelo` 2.424 de 2.424 iguais e `f5_treino` 1.242 de 1.242, na `x86gv` e na `ppcg`, sem e com as duas threads |
| Listas, `BT3_TEX_RAPIDA=0` contra `1`, `x86gv` e `ppcg`, `f5_duelo` (8.380 quadros) e `f5_treino` (7.386), sem e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1` | os 8 pares com `divergencias.py` saindo 0; hashes `08a98b6d05b8775a` e `f76f0f6849b64629` na `x86gv`, `994627a2aecd429a` e `f76f0f6849b64629` na `ppcg` (os de sempre); 0 desenhos com textura já solta |
| O mesmo em 16:9 (`BT3_WIDE=1`): os 8 pares, e a conferência | os 8 pares com `divergencias.py` saindo 0; hashes `9f85962173d0bd7f` (`x86gv`) e `53f776da4b2d1e3f` (`ppcg`) no `f5_duelo`, `e74bbb9ad3627bdb` nas duas no `f5_treino`; conferência 2.410 de 2.410 e 1.191 de 1.191 iguais nas duas builds. (A lista de 16:9 difere da de 4:3, como deve: a projeção muda) |
| Sabotagem: os meios bytes trocados nos 4 bits do `Gs_TexDecode`, `x86gv`, `f5_duelo` | a conferência acusa **510 texturas** diferentes (as de 4 bits); com a chave ligada, a lista difere em **141 quadros** (a seção 0, a das texturas: o `fake_tex_upload` do `gpu_falso` hasheia os pixels enviados), hash `25b75fa7a48d391a`, `divergencias.py` sai 1 |
| O git do worktree no fim | 0 alterados (a sabotagem desfeita e a `x86gv` refeita) |

O `f5_treino`, para comparar [medido]: 1.242 texturas, 28,4 M pixels em 257 quadros; primeira vez 62,1% dos pixels, página 24,0%,
outro `TEX0` 13,9%, descartada 0; 95 quadros com 5 ms ou mais estimados, 30 deles depois da técnica.

**A build do 360** (`build_f2.sh` no `aa9f404`, carimbo sem diferença): 317 de 317 fontes sem erro, os avisos por fonte iguais aos
da main (993, arquivo por arquivo), os mesmos 103 stubs, `default.dat` `fb489b2f6e00d0e4de7e394f7037540c` e `default.swap`
`abbd7589c8425797c4a2639068c9bf54` iguais; **`default.xex` `d18e4dcd2541dc984dae9454ad85eb9e`**. O `Gs_TexDecode` em
`0x8217f720` no `default.map`; o laço dos 8 bits no `dumpbin` (acima). A contagem não entra no 360 (só `BT3_GPU_FALSO`).

### A rodada no console proposta

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-tex`: `default.xex`/`.dat`/`.swap`, `f5_duelo.txt` e `boot.txt` (cópias de
`port/x360/roteiros/`), `ambiente_t0.txt` (`BT3_TEX_RAPIDA=0`, saves `saves_tx0`) e `ambiente_t1.txt` (`BT3_TEX_RAPIDA=1`, saves
`saves_tx1`), os dois como o `ambiente_picos.txt` da main (o `f5_duelo` pelo roteiro, `BT3_TEMPOS=0`, `BT3_PICOS=36`,
`BT3_WIDE=1`, decisão 26); o `ambiente.txt` é a cópia do `ambiente_t0.txt`.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_t0.txt build/x360/f2/ambiente_t1.txt
# abertura 1 (chave desligada); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_t1.txt
# abertura 2 (chave ligada); baixar o log de novo
```

**O que olhar:**

1. **Os nanossegundos por pixel** nas linhas `picos:   quadro ..., texturas t (p mil pixels, T ms)`: uns 110 na abertura 1, de 8 a
   15 na 2 [estimado]. Se ficar acima de 30, o custo é a escrita no `malloc` novo ou a cópia para a memória da textura, não o laço.
2. **O p99 e o pior quadro** do `passos: janela` da luta (as duas janelas) contra a abertura 1; a mediana não deve mudar (as texturas
   pesam nos picos). Pelo WSL, das rajadas de 5 ms ou mais da luta (uns 118 quadros) devem sobrar umas 3 [estimado].
3. Nos picos, os quadros que ainda tiverem `texturas` acima de 5 ms: o Blast 2 (65 texturas, uns 3,2 M pixels) é o candidato a (a).
4. Os passos por janela e o resultado iguais aos da referência; nenhum `roteiro: FALHA`, nenhum `vigia:`; a imagem igual (a
   conferência do WSL já prova os pixels).

**Recomendação.** Ligar a decodificação por linha por padrão se a rodada confirmar o custo por pixel (o que se espera: o mesmo
resultado em menos instruções, sem nada novo entre as threads). Depois, se o Blast 2 e os quadros de página mudada ainda passarem
de 5 ms, o cache pelo conteúdo da origem (a). A thread auxiliar (b) só se as duas não bastarem.

### A rodada no console (22/10/2026) [medido]

Três aberturas do `f5_duelo` pelo roteiro, `BT3_PICOS=36`, `BT3_TEMPOS=0`, `BT3_WIDE=1`, saves novos em cada uma; as duas
primeiras com a build da `main` `db768f7` (`9fb6d6b9…`), a terceira com a do `f6-e45` juntado à main (`24413b34…`, o ADX lido em
blocos e `BT3_SOM=1`). O resultado da luta igual nas três (vencedor 0x2 KO, vida 0 27160, vblank 11478). A conta por pixel saiu
das linhas `picos:   quadro ..., texturas` (todas as da luta, do quadro 4672 em diante).

| | t0: `BT3_TEX_RAPIDA=0` | t1: `=1` | t1 com o som das etapas 4 e 5 |
|---|---|---|---|
| Custo por pixel na luta | 112,9 ns | **10,4 ns** | 10,7 ns |
| Tempo convertendo texturas na luta | 1.596 ms | 85 ms | 95 ms |
| Quadros com mais de 10 ms de texturas | 19 (pior 370,7 ms) | 3 (pior 33,3 ms) | 3 (pior 33,8 ms) |
| Combate, trabalho p50 / p95 / p99 / máximo | 33,24 / 34,90 / 39,11 / 159,6 ms | 33,25 / 34,00 / 35,00 / 116,0 | 33,12 / 34,84 / 35,11 / **38,3** |
| Combate, intervalos acima de 34,37 / de 50,05 ms | 6,98% / 6 | 6,26% / 2 | 11,53% / 2 (máximo 58,3) |
| Abertura da luta, trabalho p99 / máximo | 66,6 / 433,0 ms | 36,2 / 80,6 | 36,3 / 85,3 |
| Abertura da luta, intervalos acima de 50,05 ms | 11 | 1 | 1 |

- **Os 10,4 ns por pixel ficaram dentro dos 8 a 15 estimados:** a decodificação ligada por padrão no commit seguinte
  (`BT3_TEX_RAPIDA=0` volta ao laço). O Blast 2 caiu de uns 350 ms para 33 ms; o cache pela origem (a) não é preciso agora.
- **O que sobrou na t1 era leitura de disco na thread do jogo:** os três piores passos do combate (quadros 5825, 6359 e 7864,
  98, 65 e 40 ms) não tinham textura nenhuma e tinham `disco 1 leituras` com esse tempo, o ADX lido inteiro no início do fluxo.
  Com o ADX em blocos (F6, etapa 5, `F6/05.md`) o pior passo do combate foi a 38,3 ms.
- **Com o som ligado, mais passos logo acima do período** (11,5% dos intervalos acima de 34,37 ms, contra 6,3%; p95 34,84
  contra 34,00): o custo do som na thread do jogo (RPC, início de fluxo) ou na memória, a medir à parte. Fica para o corte
  seguinte da etapa 8.

## 18. O envio de imagens para a VRAM: a contagem por formato e o PSMCT32 por linhas de cache (22/10/2026)

Branch `f5-envio-rapido` (o nome `f5-envio` já era o da seção 7, juntado à main), sobre a main `64353a4`. O passo (c) da
investigação de hoje: nos quadros pesados da luta o envio de imagens para a VRAM custa uns 6,5 ms da gravação (298 envios,
2,5 MB) [medido no console], e a gravação ocupada fica em 29 a 30 ms contra o período de 33,37. A hipótese de partida era que
os envios de 8 e de 4 bits iam pixel a pixel (o de 4 bits relendo e regravando um byte por pixel). Convenção: **[medido]**,
**[lido]** e **[estimado]**.

| Commit | O quê |
|---|---|
| `6315fd0` | A contagem `BT3_ENVIO_CONTA=arquivo` (só nas builds `--gpu` do WSL): uma linha por chamada do `transfer_data` (quadro, formato, `dbp`, `dbw`, retângulo, posição, bytes, ns, pixels pelo caminho novo) e o `port/x360/tools/envio_conta.py`, que soma por formato no roteiro e no combate (de `roteiro: seq 3` a `seq 4`) e nos 10% de quadros com mais bytes |
| `b9c1896` | O envio de 8 e de 4 bits por colunas do GS (cada palavra da coluna montada das 4 linhas e gravada uma vez), sob `BT3_ENVIO_RAPIDO`, com o `teste_envio.c` e o `teste_envio.sh`. Certo (0 diferenças na x86 e na PPC de verdade, os dois mutantes acusados), mas **o jogo não envia nada nesses formatos** (abaixo): trocado no commit seguinte |
| `e49c962` | O envio de 32 bits por linhas de cache (`envio_faixa32`), sob `BT3_ENVIO_RAPIDO`: 1 com `dcbt` e `dcbz128`, 2 só com `dcbt`, 0 ou ausente o laço por grupos de antes (o padrão). A `sVram` alinhada a 128 no 360 |
| `b2e1629` | As cópias do caminho novo como `uint32_t`, não `memcpy` de 4 bytes (o `cl` fazia byte a byte) |

### A contagem [medido]

`x86gv`, chave desligada, sem as threads (`BT3_ENVIO_CONTA`, `envio_conta.py`), no `b9c1896` (o envio é o mesmo da main):

| | `f5_duelo`, combate (quadros 4.673 a 7.757) | `f5_treino`, combate (4.153 a 7.385) |
|---|---:|---:|
| Envios por quadro | 260,8 | 246,8 |
| KB por quadro | 2.209 | 2.097 |
| PSMCT32 (dos bytes) | **100%** | **100%** |
| PSMT8, PSMT4, PSMCT16, PSMCT16S, PSMT8H, PSMT4HL, PSMT4HH | 0 | 0 |
| ns por byte na x86 | 0,128 | 0,129 |
| Os 10% de quadros com mais bytes | 363,8 envios, 2.648 KB, 0,350 ms | 253,8 envios, 2.849 KB, 0,341 ms |

- **O jogo envia tudo em PSMCT32**: as texturas de 8 e de 4 bits vão como imagens de 32 bits com os bytes já arrumados para os
  formatos indexados (o comentário do começo do `gs_core.c` já dizia). Os caminhos por pixel de 8 e de 4 bits do `transfer_data`
  não rodam nos roteiros; a hipótese de partida veio da leitura do código, não da contagem.
- Fora do combate, no `f5_duelo`, há 348 envios **PSMCT24** de 512 x 224 (`dbp` 0x1C00 e 0x2300, `dbw` 8), dois por quadro nos
  quadros 4.235 a 4.344, antes do combate: 0,9% dos bytes do roteiro e 16,6% do tempo de envio na x86 (2,8 ns por byte, 1 ms por
  envio na x86), porque o 24 bits vai pelo `vram_rw` por pixel. No console seriam uns 10 a 20 ms por envio [estimado, pela razão
  de 9 a 20 vezes]: um candidato para os picos da abertura da luta, fora desta etapa.
- **O alinhamento** (`f5_duelo`, combate): 99,8% dos bytes PSMCT32 em retângulos alinhados a blocos de 8 x 8 pixels, todos em
  0,0; os tamanhos 128 x 64 (20,3% dos bytes), 128 x 128 (19,6%), 256 x 256 e 512 x 128 (11,6% cada), 256 x 128 (9,9%), 64 x 32
  (9,7%); `dbw` 2 (44,8%), 4 (21,9%), 1 (14,2%), 8 (11,6%), 0 (7,5%).

**Leitura.** O formato que pesa é o PSMCT32, que já ia em grupos de 8 pixels (seção 7). No console ele custa uns 2,6 ns por byte
(6,5 ms por 2,5 MB), umas **20 vezes** a x86; a decodificação das texturas, presa às contas, custava 9 vezes. No `dumpbin` do laço
por grupos [lido]: umas 34 instruções por 8 pixels (4,3 por pixel), contra uns 33 ciclos por pixel medidos (10,4 ns a 3,2 GHz).
O tempo é da memória: a origem (as texturas do jogo, frias) e o destino (a VRAM de 4 MB, maior que os 1 MB do L2; o L1 do Xenon
grava direto no L2, que lê a linha antes de escrevê-la), umas 20 mil linhas de 128 bytes de cada lado por quadro pesado.

### O envio por linhas de cache (`e49c962`, `b2e1629`)

Uma linha de cache de 128 bytes da VRAM no PSMCT32 é meio bloco: 8 pixels de largura por 4 linhas alinhadas a 4 (as colunas 0 e
1, ou 2 e 3, do bloco). Numa faixa de 4 linhas inteiras do retângulo (o retângulo com `tx` e `tw` múltiplos de 8, a faixa
começando numa linha múltipla de 4, inteira no pedaço IMAGE), cada grupo de 8 pixels enche uma linha inteira: a linha r da faixa
vai às palavras `(r / 2) * 16 + (r % 2) * 2 + {0, 1, 4, 5, 8, 9, 12, 13}` (o `kCol32`). Com `BT3_ENVIO_RAPIDO=1`, no 360:

- a linha de destino é zerada com `dcbz128` antes de ser escrita: o L2 a cria sem lê-la da memória;
- a origem da faixa seguinte (4 linhas de `tw` pixels, seguidas no pacote) é pedida com `dcbt` antes de copiar a atual (na
  primeira faixa de um pedaço, a atual também), para as faltas correrem juntas;
- as cópias são de uma palavra (`uint32_t`): a origem é o IMAGE de um pacote da GIF, em quadwords, e a faixa começa num pixel
  inteiro. No `cl`, o `memcpy` de 4 bytes de um `uint8_t *` saía como quatro `lbz` e quatro `stb` (no `e49c962`; também na
  primeira cópia de cada grupo do laço da seção 7).

Com `=2`, o mesmo sem o `dcbz128` (para separar os dois efeitos no console). Fora do 360 o `dcbt` e o `dcbz128` não existem: o
caminho é o mesmo, sem eles (no teste de unidade, com `-DBT3_TESTE_ENVIO`, o `dcbz128` vira um `memset` da linha, para uma linha
zerada e não escrita inteira aparecer como diferença). A `sVram` é `__attribute__((aligned(128)))` só no 360 (o `c89.py` a passa a
`__declspec(align(128))`), e o `transfer_data` cai para `=2` se ela não estiver alinhada.

**No `dumpbin` do `default.exe` (`b2e1629`)** [lido]: o `envio_faixa32` em `0x8217e868`; por grupo, a consulta da tabela, um
`dcbz128` e quatro vezes 8 `lwz` e 8 `stw` (umas 115 instruções por 8 pixels com o laço das 4 linhas, contra umas 34 do laço
por grupos: a aposta é que as instruções não são o gargalo); o `dcbt` num laço de uma instrução por linha de 128 bytes. A `sVram` em `0x823ADA00` (o `lis`/`addi` do começo da
função), múltiplo de 128.

## 19. A espera de vblank medida certo e a espera pelo vblank de hardware (22/10/2026)

Branch `f5-vblank`, sobre a main `64353a4`. Pedido do Lucas: os passos (a) e (b) da investigação feita sobre os logs do console do
`f5_duelo` (só leitura, o relatório abaixo), cada um com commit próprio, sem nenhum quadro a mais de atraso entre o controle e a
tela. Convenção: **[medido]**, **[lido]** e **[estimado]**.

### O que a investigação achou [lido nos fontes, medido nos logs]

1. **O `passos` não descontava a espera de vblank.** O `gs_core.c` (`Port_GsVif1Chain`, `:1840-1843` na main) lê o `gPortSleptNs`
   e o zera a cada entrega da lista; o `passos.c` (`Passos_Leitura`) o lia como um acumulado e tirava a diferença entre duas
   leituras do controle. As janelas diziam "espera de vblank 0.0 ms", e o "trabalho" p50/p95/p99 era o intervalo inteiro entre
   duas leituras do controle, com média 33,37 ms por construção. **Os números de "trabalho" de todas as rodadas da F5 até aqui são
   esse intervalo**, não o trabalho.
2. **Os intervalos acima de 34,37 ms são medidos na CPU logo depois do `Present`** (`Passos_Apresentou`, chamado pelo
   `frame_end` do `gs_d3d.cpp`); a tela troca no vblank de hardware (`D3DPRESENT_INTERVAL_ONE`, `main360.cpp`), e o jogo marcava
   o tempo pelo `Sleep` em milissegundos inteiros (`nanosleep` do `posix360.c` dentro do `vblank_wait` do `plat_stub.c`), sem
   fase com o vídeo. Em 24 de 70 quadros lentos o jogo acordou 1,4 ms ou mais atrasado (até 2,5), e a entrega soma uns 0,7 ms
   em média.

### (a) A medida certa

| Commit | O quê |
|---|---|
| `8bd7db3` (a) | **A espera acumulada própria**: `gPortVBlankEsperaNs` no `plat_stub.c` (só cresce), com as esperas de fato, quantas acabaram no vblank de hardware, quantas acordaram 1 ms ou mais depois do momento pedido e o pior atraso ao acordar desde a última leitura do `passos`. O `passos.c` desconta dela (o `gPortSleptNs` segue como era para o `gs_core.c`). **A tela**: o `gs_d3d.cpp` instala no init (pela dona do dispositivo, a thread de desenho com `BT3_GS_DESENHO=1`) uma callback de vblank (`SetVerticalBlankCallback`), que numa DPC guarda o `mftb`, conta quantos vblanks cada imagem ficou na tela (pelo `Swap` mostrado do `D3DVBLANKDATA`, que muda quando uma imagem nova entra), as trocas perdidas (`D3DVBLANK_SWAPMISSED`) e as imagens puladas, e sinaliza um evento. A callback sai no `GsD3d_Soltar` e a main a tira de novo quando toma o dispositivo de volta. A linha nova `passos: janela K tela:` e o critério do `passos.py --console` pela tela. Tudo em `PORT_X360`: a x86 e a PPC compilam o mesmo texto de antes |

**A callback** [lido no XDK: `D3DVBLANKCALLBACK`, `D3DVBLANKDATA`, `SetVerticalBlankCallback` e o exemplo `AsyncSwaps.cpp:1281`]:
roda numa DPC, onde só valem os `Interlocked*`, o `SetEvent` e afins com um handle válido, sem float e sem espera. Ela só lê o
`mftb`, soma inteiros e sinaliza o evento, que nasce uma vez e nunca se fecha (segue válido na saída e numa queda). Os contadores
só crescem, cada um com um escritor só (a DPC), e quem lê (o `passos`, por diferença entre o começo e o fim da janela) lê palavras
inteiras.

**A linha nova** (só no 360, entre a `apresentação` e os `descartes`):

```
passos: janela K tela: N imagens na tela, por vblanks na tela 1 a, 2 b, 3 c, 4 ou mais d (3 ou mais P%); trocas perdidas p,
imagens puladas u, vblanks V; esperas de vblank E (H pelo vblank de hardware), atraso ao acordar por passo p50 x p99 y máximo
z ms, esperas com 1 ms ou mais w
```

Sem a callback na janela inteira: `passos: janela K tela: sem a medida (sem a callback de vblank na janela inteira)`. O atraso ao
acordar é o pior das esperas de cada passo: com o vblank de hardware, de o vblank (o `mftb` da DPC) até a thread do jogo acordar;
no `Sleep`, de o momento pedido pela grade até acordar.

**O critério do `passos.py --console`** passa a ser a tela: em cada janela de pelo menos 2.700 passos, **no máximo 1% das imagens
com 3 vblanks ou mais na tela e nenhuma com 4 ou mais** (o "99% até um período mais 1 ms, nenhum acima de três vblanks" de antes,
contado onde a imagem de fato fica); mais a parede dentro de 1%, uma imagem nova por passo e zero descartes, como antes. O trabalho
(agora com a espera certa) e os intervalos medidos na CPU depois do `Present` se listam, sem aceite. Sem a linha `tela` a janela
falha no `--console`; fora dele a linha é opcional (a x86 e a PPC não a escrevem).

**O verificador visto falhando de propósito** [medido]: um banco de teste do `passos.c` com `PORT_X360` compilado pelo `gcc` daqui
(o relógio trocado por um falso, três janelas de 3.001 passos, o `Passos_Tela` e os contadores do `plat_stub.c` simulados) escreve as
linhas como o console; o `passos.py --console` diz "tudo certo" no log bom e com 30 imagens de 3 vblanks em 3.000 (1,00%), e sai
com 1 em cada estrago: 31 de 3.000 (1,03%), uma imagem de 4 vblanks, a janela sem a medida da tela, e o formato da linha mudado
numa cópia do `passos.c` (`4+` no lugar de `4 ou mais`, com `assert` de que a cópia mudou: a linha some para o `passos.py` e a
janela falha por falta da medida). A mesma conta da porcentagem sai das contagens, não do número arredondado da linha. O
`passos.c` sem `PORT_X360` (como a x86) segue sem a linha, e o `passos.py` sem `--console` aceita o log.

### O critério do escopo que usa o "trabalho" (proposta, o escopo não mudou)

Em `docs/x360/plano/F5/00-escopo.md`, seção "Tempo, memória e uso do console", a tabela tem a linha **"Trabalho por passo | p99
até 30 ms; nenhum passo acima de um período de luta, aproximadamente 33,37 ms, em cada uma de três janelas de pelo menos 2.700
passos | CPU e conclusão da GPU, sem a espera deliberada de vblank; ..."**, e a linha **"Apresentação | Em cada janela, pelo menos
99% dos intervalos entre imagens novas dentro de um período mais 1 ms; nenhum acima de três vblanks, aproximadamente 50,05 ms |
..."**. O número que as rodadas leram como trabalho era o intervalo inteiro (item 1 acima), e o intervalo entre imagens era o da
CPU depois do `Present`, não o da tela. Texto proposto, para o Lucas decidir:

| Medida | Aceite proposto | Como medir |
|---|---|---|
| Trabalho por passo | Informativo: p50, p95 e p99 do intervalo entre duas leituras do controle menos a espera de vblank do passo; acompanha a folga, sem aceite próprio (com as threads da etapa 8 o trabalho de um passo se sobrepõe ao do anterior, e o que a pessoa vê é a tela) | A espera de vblank de um acumulado próprio, que ninguém zera (`gPortVBlankEsperaNs`), descontada por passo; distinguir sobreposição para não somar tempos concorrentes como se fossem seriais |
| Apresentação | Em cada janela, no máximo 1% das imagens novas 3 vblanks ou mais na tela; nenhuma 4 ou mais | Contado no vblank de hardware (a callback de vblank do D3D: quantos vblanks cada imagem mostrada ficou na tela, pela troca mostrada); o intervalo medido na CPU depois do `Present` fica como diagnóstico |

E no parágrafo logo abaixo da tabela, "Não usar os 16,7 ms dos menus como duração do passo da luta, nem contar uma espera de vsync
como CPU trabalhando" segue valendo; a frase "Reservar os cerca de 3,37 ms entre p99 de 30 e o período como margem proposta para
a F6" perde a base (o p99 de 30 deixa de ser aceite) e vira: "a margem para a F6 se lê no trabalho informativo e nas imagens de 3
vblanks com o som ligado".

### (b) A espera pelo vblank de hardware

| Commit | O quê |
|---|---|
| `3ae20e6` (b) | No 360, o `vblank_wait` do `plat_stub.c` pergunta primeiro ao `gs_d3d.cpp` (`GsD3d_VblankEspera`): a thread do jogo espera o evento que a callback de vblank sinaliza, em vez do `Sleep`. `BT3_VBLANK_HW=1` é o padrão; `BT3_VBLANK_HW=0` volta ao `Sleep` (para comparar na mesma build). O `Sleep` fica de reserva: antes da callback (os menus antes do init do backend), depois do `GsD3d_Soltar` (a saída), sem vblank há 50 ms ou um evento que não veio em 50 ms (a Guide, o dispositivo parado); cada passagem é dita no log (`gs_d3d: vblank: a espera no Sleep (motivo; ...)` e `... de volta ao vblank de hardware`, até 20 vezes). Em `PORT_X360` |

**A conta é a mesma do `vblank_wait`** [lido]: um alvo (o número do vblank de hardware que acaba a próxima espera) que anda um por
chamada; o vblank que já passou não se espera (o passo da luta que passou de um vblank não perde a vez: a segunda espera dele acaba
no tempo); mais de seis vblanks atrás (100 ms), um alvo novo a partir de agora, sem esperar, como a grade nova do `Sleep`. O
evento é de reinício automático e pode ser de um vblank já contado: a volta confere pelo número, não pelo evento. O tempo esperado
entra no `gPortSleptNs` (o `gs_core.c` o desconta do quadro, como antes) e no acumulado do `passos`; o `sPaceNext` passa a ser o
próximo vblank de hardware (o `Port_VBlankUsLeft` e a reserva continuam com a grade certa).

**Nenhum quadro a mais de atraso** [lido e simulado]: o número de esperas por passo é o mesmo, e cada uma acaba no vblank de
hardware (mais a volta da DPC à thread, dezenas de microssegundos [estimado]) em vez de num momento da grade do `Sleep`, que não
tinha fase com o vídeo e acordava até 2,5 ms tarde. Uma simulação das duas contas (`scratchpad`, fora do repositório: 20 mil passos
de luta com trabalho de 20 a 60 ms, três fases do vídeo) deu o mesmo número de esperas de fato (7.886 nas duas) e o mesmo período
médio (33,369 contra 33,368 ms); no modo de hardware todo passo que começa depois de uma espera começa no vblank (fase 0), no `Sleep`
numa fase qualquer. O que muda é só a fase: o caminho do controle à tela fica igual em média e deixa de variar de um passo a outro.

**Os cuidados** [lido]: a callback só sinaliza (DPC); quem espera é a thread do jogo, que nunca toca o dispositivo (a dona, com
`BT3_GS_DESENHO=1`, é a de desenho, que instala e tira a callback); antes do init não há evento e a espera é o `Sleep`; na saída o
`GsD3d_Soltar` tira a callback e marca a espera desligada antes de soltar o dispositivo, e a main tira de novo se tomar o
dispositivo de uma thread que caiu; numa queda o evento segue válido (nunca se fecha) e a espera que estiver em curso volta em até
50 ms para o `Sleep`.

### As provas [medido]

| Prova | Resultado |
|---|---|
| O pré-processado (`gcc -E -P`, sem linhas em branco) do `plat_stub.c` e do `passos.c`, main contra `3ae20e6` | igual nas definições da x86 plana, da `x86gv` (`BT3_TESTE_VU1`) e da PPC (`PORT_BIG_ENDIAN`); diferente com as do 360 (`PORT_X360`): a comparação vê a mudança onde ela existe. O `gs_d3d.cpp` e o `main360.cpp` só entram na build do 360 |
| `.text` da x86 plana (WSL, serviço `claude-vblank`, worktree `~/bt3xbx-vb`) | `43444ea4d03a6d448aa1ae74a631e35b356a7caa` na main `64353a4` e no `3ae20e6`: a referência |
| `.text` da `x86gv` (`--gpu --vu1`), a mesma rodada | `eead10fc381957a692618088926fb09fdeda8aea` antes e depois, a mesma: as listas da `x86gv` não podem mudar |
| Listas da `x86gv`, `f5_duelo` e `f5_treino`, sem e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1`, main contra `3ae20e6` (`divergencias.py`) | os quatro pares com `divergencias.py` saindo 0, hashes `08a98b6d05b8775a` (`f5_duelo`, 8.380 quadros) e `f76f0f6849b64629` (`f5_treino`, 7.386), os de sempre; contadores do jogo e do desenho iguais, 0 desenhos com textura já solta, 0 `FALHA`; o git do worktree e do clone sem alterados no fim (serviço `claude-vblank`, terminado com `FIM`; as listas apagadas depois de conferidas) |
| `ppcg` | não refeita: o pré-processado com as definições da PPC é o mesmo (acima), então os objetos são os mesmos (guia, "Uma mudança só no 360 depois das provas do WSL") |
| O verificador do `passos.py` | visto falhando de propósito (acima, em (a)) |
| A conta da espera | a simulação das duas contas (acima, em (b)): as mesmas 7.886 esperas e o mesmo período |
| A callback no `dumpbin` do `.exe` (`vblank_cb`, `0x82277a10` no `default.map` do `3ae20e6`) | só inteiros (`lwz`, `stw`, `std`, `mftb`, `lwsync`) e a chamada ao `SetEvent`: nenhuma instrução de float nem de vetor na DPC |

### A build do 360

`build_f2.sh` em cada commit, carimbo sem diferença: 317 de 317 fontes sem erro, 985 avisos nos fontes do jogo (os mesmos da build da
main), 0 no `gs_d3d.cpp`, no `main360.cpp`, no `plat_stub.c` e no `passos.c`, os mesmos 103 stubs (`diff` com o `stubs.txt` da main),
`default.dat` `fb489b2f6e00d0e4de7e394f7037540c` e `default.swap` `abbd7589c8425797c4a2639068c9bf54` iguais. **`default.xex`** do
`8bd7db3` (a): `bf6d8814210c5bd2b623357128b7d07b`; do `3ae20e6` (b): **`806fe3c1fc3bb46dfe23f2bb151c2ce3`**. No `default.map` do (b):
`vblank_cb` em `0x82277a10`, `GsD3d_VblankEspera` em `0x82277d58`, `Passos_Tela` em `0x82277f00`.

### A rodada no console proposta

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-vblank` (a build do `3ae20e6`): `default.xex`/`.dat`/`.swap`, `f5_duelo.txt` e
`boot.txt` (cópias de `port/x360/roteiros/`), `ambiente_vb0.txt` (`BT3_VBLANK_HW=0`, o `Sleep`, saves `saves_vb0`) e
`ambiente_vb1.txt` (`BT3_VBLANK_HW=1`, o vblank de hardware, saves `saves_vb1`), os dois como o `ambiente_sx1.txt` da última rodada
da seção 17 (o `f5_duelo` pelo roteiro, `BT3_TEMPOS=0`, `BT3_PICOS=36`, `BT3_WIDE=1`, decisão 26, e `BT3_SOM=1`, o som ligado); o
`ambiente.txt` é a cópia do `ambiente_vb0.txt`.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_vb0.txt build/x360/f2/ambiente_vb1.txt
# abertura 1 (o Sleep); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_vb1.txt
# abertura 2 (o vblank de hardware); baixar o log de novo
# cada log à parte (as duas janelas do combate, 684 e 2.334 passos na referência):
python3 port/x360/tools/passos.py bt3xbx_vb1.log --console --janelas 2 --minimo 600 --carimbo "3ae20e6 diff:da39a3ee5e"
```

**O que olhar:**

1. **A linha do init** em cada abertura: `gs_d3d: vblank: a callback do vblank de hardware instalada ...; a espera de vblank pelo
   Sleep (BT3_VBLANK_HW=0)` e `... pelo vblank de hardware, o Sleep de reserva (BT3_VBLANK_HW=1)`. Na abertura 2, nenhum
   `gs_d3d: vblank: a espera no Sleep` durante a luta (no boot e nas cargas pode haver).
2. **As imagens de 3 vblanks na tela**, na linha `passos: janela K tela:` das duas janelas do combate (684 e 2.334 passos na
   referência): a porcentagem de "3 ou mais" e os "4 ou mais". Na abertura 1 deve aparecer o engasgo que a CPU via (11,5% dos
   intervalos acima de 34,37 ms com o som); na 2, bem menos [estimado: o que sobrar é passo que de fato passou do período]. Se a
   linha disser **0 imagens na tela** ou muito menos imagens que passos, o `Swap` do `D3DVBLANKDATA` não anda com as trocas
   síncronas: a medida da tela precisa de outro sinal (o `QuerySwapStatus` ou o `D3DVBLANK_SWAPMISSED`, que a linha também
   conta), e a espera do (b) não depende disso.
3. **O atraso ao acordar**, na mesma linha: na abertura 1, p99 perto de 1,4 a 2,5 ms e muitas esperas com 1 ms ou mais (a
   investigação viu 24 de 70 quadros lentos); na 2, p99 bem abaixo de 1 ms (a volta da DPC à thread, dezenas de microssegundos
   [estimado]) e as esperas todas "pelo vblank de hardware".
4. **O p95 e o p99 do intervalo**: os da `apresentação` (o intervalo na CPU depois do `Present`) e o do `trabalho`, que agora
   desconta a espera de verdade (a `espera de vblank` da linha de tempo deixa de ser 0,0 ms; o trabalho p50 deixa de ser 33,37).
5. A parede de cada janela dentro de 1% nas duas aberturas (a espera nova não pode mudar o ritmo), os passos por janela e o
   resultado da luta iguais aos da referência (vencedor 0x2 KO, vida 0 27160, vblank 11478); nenhum `roteiro: FALHA`, nenhum
   `vigia:`, nenhum `CRASH`.

**Recomendação.** O padrão do branch já é `BT3_VBLANK_HW=1`. Se a abertura 2 mostrar menos imagens de 3 vblanks e o atraso ao
acordar abaixo de 1 ms, sem nada novo, fica assim; se não, o padrão volta a 0 num commit e o `Sleep` segue como era. Depois, o
critério do escopo pela tela (a proposta acima) é decisão do Lucas.

## 20. A rodada do console (22/10/2026) [medido]

A build do `3ae20e6` (`default.xex` `806fe3c1...`, `passos: origem 3ae20e6 diff:da39a3ee5e` nos dois logs), `f5_duelo` pelo
roteiro, `BT3_SOM=1`, `BT3_PICOS=36`, `BT3_WIDE=1`; o ambiente das duas aberturas difere só no `BT3_VBLANK_HW` (e na pasta dos
saves). Logs em `build/sessao_2210/vb0/` (`=0`, o `Sleep`) e `vb1/` (`=1`, o vblank de hardware), fora do repositório. A prova do
WSL (`~/vblank/provas.resumo`) fica **pendente de conferência** por esta análise: o WSL e o Deck estavam fora do ar; a tabela "As
provas" da seção 18 é a do agente que a rodou.

| Medida (janela 1: 684 passos, fim pausa; janela 2: 2.334, fim combate) | `Sleep` (vb0) | vblank de hardware (vb1) |
|---|---|---|
| Linha do init | `... pelo Sleep (BT3_VBLANK_HW=0)` | `... pelo vblank de hardware, o Sleep de reserva (BT3_VBLANK_HW=1)`; nenhum `a espera no Sleep` no log inteiro |
| Tela: imagens, por vblanks na tela 1/2/3/4+ (3 ou mais) | j1 685: 0/683/2/0 (0,29%); j2 2.335: 13/2.302/20/0 (0,86%) | j1 685: 0/685/0/0 (0,00%); j2 2.335: 7/2.325/3/0 (0,13%) |
| Trocas perdidas, imagens puladas | 0, 0 | 0, 0 |
| Esperas de vblank (pelo hardware) | j1 431 (0); j2 1.797 (0) | j1 **0** (0); j2 1.761 (1.761) |
| Atraso ao acordar por passo p99, máximo; esperas com 1 ms ou mais | j1 1,00, 1,73 ms, 4; j2 0,95, 1,50 ms, 13 | j1 sem esperas; j2 **0,06, 0,07 ms, 0** |
| Espera de vblank na linha de tempo | j1 1.830,5 ms; j2 16.123,5 ms | j1 **0,0 ms**; j2 15.776,4 ms |
| "Trabalho" p50 / p95 / p99 / máximo | j1 30,10 / 33,38 / 36,07 / 81,27; j2 28,02 / 33,34 / 33,52 / 39,08 | j1 33,27 / 33,49 / 36,09 / 84,06; j2 27,86 / 33,34 / 33,54 / 39,28 |
| Apresentação na CPU: p99, máximo; acima de 34,37 ms; acima de 50,05 ms | j1 36,20, 82,24, 21 (3,07%), 1; j2 37,02, 48,64, 241 (10,33%), 0 | j1 33,74, 69,20, 4 (0,59%), 1; j2 36,18, 52,26, 67 (2,87%), 1 |
| Parede (razão) | j1 1,0004; j2 0,9991 | j1 0,9988; j2 0,9968 |
| Menus depois da luta (`picos_log.py`, trechos de 60 quadros) | 16,64 a 16,76 ms, pior por trecho até ~20 ms | 16,64 ms, pior 16,65 em quase todos os trechos |
| Resultado da luta | estado 4 vencedor 0x2 razão 0x1 vida 0 27160 vblank 11478 quadro 7757 | o mesmo |
| `FALHA`, `vigia:`, `CRASH`, `ERRO` | 0 | 0 |
| `passos.py --console --janelas 2 --minimo 600` | "tudo certo", sai 0 | "tudo certo", sai 0 |

**O que os números dizem.**

1. **A medida da tela funciona**: 685 e 2.335 imagens para 684 e 2.334 passos (uma por passo, mais a que já estava), 0 trocas
   perdidas e 0 puladas; o `Swap` do `D3DVBLANKDATA` anda com as trocas síncronas. Não precisa de outro sinal.
2. **A CPU depois do `Present` exagerava o engasgo**: no `Sleep`, 10,33% dos intervalos acima de 34,37 ms na CPU contra 0,86% de
   imagens com 3 vblanks na tela (janela 2). O que a pessoa via já era quase todo de 2 vblanks.
3. **O vblank de hardware melhora a tela e acaba o atraso ao acordar**: 3 ou mais vblanks de 0,29%/0,86% para 0,00%/0,13%,
   imagens de 1 vblank de 13 para 7, atraso ao acordar p99 de ~1 ms para 0,06 ms (todas as 1.761 esperas da janela 2 pelo
   hardware), os menus exatamente em um vblank. O intervalo na CPU acima de 34,37 ms cai de 10,33% para 2,87%. O p99 do
   "trabalho" não muda (33,5 ms): a mudança é de fase, não de custo, como a seção 18 previa.
4. **A janela 1 do vb1 não teve espera nenhuma** (0 esperas, espera 0,0 ms, "trabalho" p50 33,27 = o intervalo), sem nenhuma
   passagem ao `Sleep` no log. [lido no código, deduzido dos contadores] Toda chamada do `GsD3d_VblankEspera` achou o alvo já
   passado (devolveu 1): na cena anterior à janela (quadros 4.600 a 4.617, passos de ~45 ms pela gravação) o alvo ficou de 1 a 5
   vblanks atrás do atual, menos que os 6 que o recomeçam, e na janela cada passo durou exatamente 2 vblanks pela fila da gravação
   (`picos`: espera da gravação 21,8 ms em média, vblank 0,0), então o alvo andou 2 por passo como o vblank e a dívida nunca
   se pagou. A tela ficou perfeita (685 imagens de 2 vblanks), mas o ritmo ali veio da fila da gravação e do `Present`, não da
   espera, e a medida do trabalho ficou cega na janela. A conta do `Sleep` tem a mesma regra, mas a grade de 16,683 ms anda
   mais devagar que o vblank de 16,64 e a dívida se desfaz sozinha (o vb0 teve 431 esperas na janela 1).
   [estimado] Com o jogo adiantado na fila, a leitura do controle pode vir até um passo antes do que viria com a espera: o
   "nenhum quadro a mais de atraso" da seção 18 não se prova nessa janela. Correção a estudar (não feita): um teto na dívida não
   basta (um vblank de dívida já trava assim, com o passo de exatamente 2 vblanks); o que resolve é recomeçar o alvo quando a
   dívida não se paga em alguns passos seguidos (todas as chamadas com o vblank já passado), como os 6 vblanks já fazem, o que
   não custa ritmo onde a fila da gravação é que manda. Antes, contar na linha `tela` as chamadas com o vblank já passado.
5. **O vblank de hardware do console dura ~16,64 ms** pelo `QueryPerformanceCounter` (4.667 vblanks em 77.628 ms; apresentação
   p50 33,27 nas duas aberturas), não os 16,683 da grade de 59,94 Hz. Com o vblank de hardware o jogo segue a frequência
   efetiva do vídeo (o "usar 2 / frequência como período" do escopo): a parede fica ~0,3% abaixo da conta de 33,37 ms
   (razão 0,9968), dentro de 1%.

**O critério proposto para o escopo (seção 18) se sustenta**, com uma ressalva. Pela tela, as duas aberturas passam (0,86% e
0,13% no pior, nenhuma de 4 vblanks), enquanto pelo critério de antes (99% dos intervalos na CPU até 34,37 ms) as duas falhariam
na janela 2 (10,33% e 2,87%) por um engasgo que a tela não mostrou. O "trabalho" como informativo se confirma: na janela 1 do vb1
ele virou o intervalo inteiro sem que nada estivesse errado na tela. A ressalva: o vb0 passa a 0,86%, perto do limite, e o
critério não vê as imagens de 1 vblank (13 no vb0), que também são um tranco na luta; "no máximo 1% das imagens fora de 2
vblanks" separaria as duas aberturas (vb0 1,41%, falha; vb1 0,43%, passa). Fica para o Lucas.

**Recomendação.** Levar à main com `BT3_VBLANK_HW=1` por padrão: a tela melhora, o atraso ao acordar some, nada novo no log, o
resultado da luta é o mesmo. Antes ou junto: a correção da dívida do item 4 (com uma rodada curta do console), e a conferência da
prova do WSL quando a máquina voltar.

## 21. A dívida parada: o alvo recomeça e a espera é já (23/10/2026)

O item 4 da seção 19: na janela 1 do vb1 a espera pelo vblank de hardware sumiu (0 esperas) porque o alvo ficou atrás numa cena
lenta e a fila da gravação e do desenho segurou cada passo em exatamente dois vblanks. Pedido: recomeçar o alvo quando o atraso
persiste por alguns passos e contar no log as chamadas com o vblank já passado.

| Commit | O quê |
|---|---|
| `67f97a0` | **A conta no `vblank_alvo.h`** (C89, sem o XDK, como o `escala.h`): o `GsD3d_VblankEspera` chama `vb_decide` e `vb_esperou` em vez da conta em linha. O mesmo de antes (um alvo por chamada; o vblank já passado não se espera; 6 vblanks atrás, alvo novo) mais **a dívida parada**: numa sequência de chamadas com o alvo já passado, a soma dos atrasos de cada bloco de 8 chamadas (4 passos) se compara com a menor de um bloco anterior da sequência; dois blocos seguidos sem ficar abaixo dela (24 chamadas, 12 passos, 0,4 s) recomeçam o alvo no próximo vblank **e esperam por ele já**. A linha `passos: janela K tela:` ganha no fim `; vblank já passado em P chamadas, alvo recomeçado R vezes pela dívida parada e L por estar 100 ms atrás`; o `passos.py` a lê (opcional: os logs de antes seguem valendo) e, no `--console`, recusa um fim de linha que não conhece. **`port/x360/tools/teste_vblank.c`**: a simulação |

**Por que a soma e por que esperar já** [simulado]. O atraso anda em vblanks inteiros: um passo de 1,9 vblank paga um vblank a
cada dez passos, e o maior atraso de um bloco fica igual por vários blocos enquanto a dívida está sendo paga; a soma cai, e os
três blocos que o recomeço pede deixam essa dívida se pagar como antes (cena C abaixo). Recomeçar o alvo sem esperar não basta:
na trava o jogo está um passo à frente da tela, parado na entrega à gravação, e volta à fila na chamada seguinte (a variante
"sem esperar já" teve 52 a 61 recomeços e 7 esperas na janela). Esperando o próximo vblank já, a parada na entrega vira uma espera
no vblank, a fila esvazia um passo (a imagem que já estava nela cobre a vez) e o controle passa a ser lido um passo mais perto
da tela.

**A simulação** (`teste_vblank.c`, só tempo simulado): o vblank de 16,635 ms (o medido na seção 19); o jogo trabalha, entrega à
gravação (espera se ela ainda não tomou a anterior) e espera dois vblanks pela conta; a gravação entrega ao desenho; o Present
põe a imagem no primeiro vblank seguinte e **segura o desenho até ela entrar na tela** (sem isso o modelo nunca reproduz a trava:
o desenho adianta a seguinte, sai uma imagem de 1 vblank e ela paga a dívida). A conta nova vem do próprio `vblank_alvo.h`; a de
antes (`3ae20e6`) está copiada no teste. `gcc -std=c89 -pedantic -Wall -Wextra -Werror`, 69 conferências, 0 falhas:

| Cena | Conta de antes | Conta nova |
|---|---|---|
| A. A janela 1 do vb1 (dez fases do vídeo): 20 passos de ~45 ms pela gravação, a fila do desenho em dois vblanks (o desenho de 16,8 a 18,5 ms), um pico da gravação de 60 a 87 ms no passo 120; 684 passos | **0 esperas** depois do pico nas 10 fases; tela 0/682/1/0; o controle à tela p50 99,81 ms (6 vblanks) | 652 de 654 passos com espera depois do pico, 2 recomeços; **a mesma tela**; o controle à tela p50 83,12 ms (5 vblanks), p99 e máximo iguais ou menores; parede igual até um passo |
| B. O passo comum, 20 mil passos (70% de 15 a 30 ms, 25% de 30 a 33, 5% de picos de 34 a 60), três fases | 18.853 a 18.929 esperas; período 33,2698 ms | as mesmas esperas até 0,08%; período até 0,02% maior; 1 a 5 recomeços; até 5 imagens de 3 vblanks a mais em 20 mil (0,025%); o controle à tela igual |
| C. A dívida paga devagar: um pico de 90 ms e 200 passos de 1,9 vblank, três fases | 199 esperas | **igual**, 0 recomeços |

**Os mutantes** [medido]: cada um é uma cópia do `vblank_alvo.h` mudada numa pasta à parte, posta antes no `-I`, e **todos saem
com 1**: sem o recomeço (`0 &&`; 20 falhas: A sem espera), recomeço sem esperar já (`return 1`; 20 falhas: A com 7 esperas),
recomeço dois vblanks à frente (`n + 2`; 4 falhas: uma imagem de 3 vblanks vira de 4 na tela e a parede passa de um passo), recomeço no primeiro bloco
parado (`VB_PARADOS 0`; 18 falhas, entre elas C recomeçando e B mudando o período), e "abaixo ou igual" no lugar de "abaixo"
(3 falhas: em três fases de A, só 528 de 654 passos com espera depois do pico).

**A linha do `passos`** [medido]: um banco de teste do `passos.c` com `PORT_X360` no `gcc` (o relógio trocado por um falso, três
janelas de 3.001 passos, os contadores do `gs_d3d.cpp` simulados) escreve a linha com o fim novo, e o `passos.py --console` diz
"tudo certo"; com uma cópia do `passos.c` em que o fim mudou (`divida` sem acento, conferido que a cópia mudou) sai com 1 ("a
linha tela com um fim que este leitor não conhece"). Os logs da seção 19 (sem o fim novo) seguem com "tudo certo".

### As provas

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `43444ea4d03a6d448aa1ae74a631e35b356a7caa` na main e no `b9c1896` (WSL) [medido]; no `b2e1629`, o `gcc -E -P` do `gs_core.c` na configuração da x86 plana é byte a byte o da main [medido] (o caminho novo e a contagem só existem com `BT3_GS_ESPERAS`) |
| Teste de unidade do `b9c1896` (as colunas de 8 e 4 bits): 1.500 envios sorteados contra o `vram_rw` | 0 diferentes na x86 e na PPC de verdade (`qemu-ppc`); os mutantes (os meios bytes trocados; as metades da coluna ímpar trocadas) acusados nas duas, só com a chave (205 e 189) [medido] |
| Listas do `b9c1896`, `x86gv`, chave 0 contra 1, `f5_duelo` e `f5_treino`, sem e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1` | os 4 pares com `divergencias.py` saindo 0 (hashes `08a98b6d05b8775a` e `f76f0f6849b64629`) [medido], **e a sabotagem dos meios bytes também 0 nos dois roteiros**: o caminho não rodava (0 pixels pelas colunas na contagem). Foi o que levou à troca |
| Teste de unidade do `b2e1629` (o PSMCT32 por linhas), x86 | 0 diferentes com a chave em 0, 1 e 2; os mutantes acusados só com a chave: as linhas 1 e 2 da faixa trocadas (356 com 1, 356 com 2) e a linha 3 da faixa não escrita (356 e 356) [medido, aqui] |
| Build do 360 (`build_f2.sh` no `b2e1629`) | 317 de 317 fontes sem erro, os avisos por fonte iguais aos da main (993, arquivo por arquivo), os mesmos 103 stubs, `default.dat` `fb489b2f6e00d0e4de7e394f7037540c` e `default.swap` `abbd7589c8425797c4a2639068c9bf54` iguais; **`default.xex` `419e94aafcad52b1b88f632528b3ec98`** [medido] |
| No WSL, o `b2e1629` (serviço `claude-envio-rapido`, `~/envio-rapido/servico.sh`, resumo `~/envio-rapido/provas.resumo`, worktree `~/bt3xbx-envio-rapido`) | **pendente** (disparado às 19:53 e parado na fila da `~/wsl.lock` atrás de quatro rodadas de outros agentes): a `.text`, o teste na PPC, as listas da `x86gv` com a chave 0 contra 1 nos dois roteiros, sem e com as threads, a contagem com a chave ligada (os ns por byte no jogo e os pixels pelo caminho novo), a sabotagem das linhas 1 e 2 trocadas nos dois roteiros (tem de dar `divergencias.py` 1), e as listas da `ppcg` |

**A medida na x86** [medido, `teste_envio --mede`, gcc `-O2` x86-64 daqui, PSMCT32 em 0,0 nos tamanhos do jogo]: de 0,63 a 0,73 ns
por byte com a chave 0, de 0,14 a 0,16 com a 1 (com o `memset` no lugar do `dcbz128`) e uns 0,06 com a 2. Dentro do jogo
(`x86gv`, clang `-m32`) o laço de antes já fazia 0,128 ns por byte: o número do jogo com a chave ligada sai da contagem do serviço.
Na x86 o ganho é de instruções; no 360 o que se espera é o de memória.

### A estimativa [estimado]

Se os 6,5 ms forem a memória (as duas metades acima), o `dcbz128` tira a leitura das umas 20 mil linhas de destino por quadro
pesado e o `dcbt` sobrepõe as faltas da origem: o envio de 6,5 para uns 2,5 a 4 ms, **2,5 a 4 ms a menos de gravação ocupada**
nos quadros pesados (de 29 a 30 para uns 26). Se for só a origem, o `=2` dá quase o mesmo que o `=1`; se for só o destino, o
`=2` fica perto do `=0`. Risco: um `dcbz128` numa linha não escrita inteira zeraria a VRAM (o teste de unidade com o `memset`
prova a cobertura; o alinhamento está no `dumpbin` e tem a guarda).

### A rodada no console proposta

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-envio`: `default.xex`/`.dat`/`.swap` do `b2e1629`, `f5_duelo.txt` e
`boot.txt` (cópias de `port/x360/roteiros/`), `ambiente_e0.txt`, `ambiente_e1.txt` e `ambiente_e2.txt` (`BT3_ENVIO_RAPIDO` 0, 1
e 2, saves `saves_ev0`, `saves_ev1` e `saves_ev2`), os três como o `ambiente_t1.txt` da seção 17 (o `f5_duelo` pelo roteiro,
`BT3_TEMPOS=0`, `BT3_PICOS=36`, `BT3_WIDE=1`, decisão 26); o `ambiente.txt` é a cópia do `ambiente_e0.txt`.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_e0.txt build/x360/f2/ambiente_e1.txt build/x360/f2/ambiente_e2.txt
# abertura 1 (chave 0); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_e1.txt
# abertura 2 (chave 1); baixar o log de novo
# (opcional) abertura 3 com o ambiente_e2.txt: separa o dcbt do dcbz128
```

**O que olhar:**

1. **A gravação ocupada nos quadros pesados**: nas linhas `picos: quadro ... gravação G (ocupada O, ...)` do combate (do quadro
   4.672 em diante), a mediana do `ocupada` dos quadros com `envios` acima de 2.400 KB na linha seguinte, abertura 1 contra 2:
   uns 2,5 a 4 ms a menos [estimado]. Os quadros com texturas, disco ou descarte à parte (a linha `o que teve` diz).
2. O p50, o p95 e o p99 do `passos: janela` do combate, e os intervalos acima de 34,37 ms, contra a abertura 1.
3. Os passos por janela e o `roteiro: resultado` iguais aos da referência (vencedor 0x2 KO, vida 0 27160); nenhum
   `roteiro: FALHA`, nenhum `vigia:`; a imagem sem lixo nas texturas (um `dcbz128` errado apareceria como blocos pretos).
4. Se a abertura 2 não mudar o `ocupada`, o envio não é memória: uma abertura com `BT3_TEMPOS=1` dá a linha por família (o `envio`
   direto, em ms por quadro) para conferir os 6,5 ms antes de outro corte.

## 22. A rodada do console do envio por linhas (22/10/2026) [medido]

Duas aberturas do `f5_duelo` pelo roteiro com o `.xex` do `b2e1629` (o log diz a build de "Oct 22 2026 22:52:32", a do
`default.xex` `419e94aa…` da seção 18), `BT3_PICOS=36`, `BT3_TEMPOS=0`, `BT3_WIDE=1`, sem som: e0 com `BT3_ENVIO_RAPIDO=0`
(`saves_ev0`), e1 com `=1` (`saves_ev1`). A abertura com `=2` não foi feita. Logs em `build/sessao_2210/e0` e `e1` (fora do git).

**O que o log deixa medir.** Com o `BT3_PICOS=36`, só os quadros com apresentação acima de 36 ms têm a linha `ocupada`, e as
linhas de 60 quadros do `gs_d3d` não saem dentro das janelas do roteiro (no combate, só a da pausa, quadro 5399): o quadro típico
do combate (abaixo de 36 ms) não tem medida de gravação. A comparação que vale é **por pares**: o mesmo quadro nas duas aberturas
(o roteiro é determinístico), com os mesmos KB de envios e sem texturas novas nem disco nos dois. A mediana de cada abertura em
separado engana: no combate, os quadros com mais de 2.400 KB que passaram de 36 ms foram 14 na e0 (`ocupada` mediana 28,6) e 30 na
e1 (25,0), conjuntos diferentes; o par dá 2,2. O tempo de envio por byte não sai do log (`BT3_TEMPOS=0`): sai da diferença por
par, dividida pelos bytes.

| | e0 (chave 0) | e1 (chave 1) |
|---|---:|---:|
| Pares (mesmo quadro, mesmos KB, sem texturas nem disco), todos | 103 quadros, 2.814 KB em média | |
| `ocupada` da gravação, mediana dos pares | 38,1 ms | **36,3 ms** |
| Diferença por par (e0 menos e1): mediana, média, faixa | **1,8 ms**, 1,87, de 1,3 a 3,1 | |
| Só os pares com mais de 2.400 KB (91): diferença mediana | 1,8 ms (0,61 ns por byte) | |
| Na abertura da luta (quadros 4136 a 4671, 58 pares de 2.991 KB) | 42,7 ms | 41,1 (1,8 a menos) |
| No combate (4672 a 7757, só 6 pares acima de 2.400 KB, 2.546 KB) | 28,4 ms | 26,3 (2,2 a menos) |
| Combate (janela 2): trabalho p50 / p95 / p99 / máximo | 33,26 / 34,79 / 34,93 / 38,24 | 33,25 / 34,84 / 34,94 / 36,28 |
| Combate: apresentação p99 / máximo | 36,94 / 47,41 | 37,31 / 54,88 |
| Combate: intervalos acima de 34,37 / de 50,05 ms | 218 (9,34%) / 0 | 223 (9,55%) / 1 |
| Abertura da luta (janela 1): trabalho p99 / máximo | 36,94 / 85,64 | 35,51 / 83,59 |
| Abertura da luta: apresentação p99 / intervalos acima de 34,37 / de 50,05 ms | 35,94 / 21 (3,07%) / 1 | **38,09 / 94 (13,76%)** / 1 |
| Quadros no `picos` entre 4672 e 7757 | 66 | 89 |
| Deles, gravação ocupada acima de 33,4 ms | 7 | 4 |
| Deles, o jogo esperando o vblank (mais de 5 ms) com a gravação parada (mais de 5 ms sem trabalho) | 42 (0 na janela 1) | 80 (27 na janela 1) |
| `roteiro: resultado` | vencedor 0x2 razão 0x1 vida 0 27160, vblank 11478 | igual |
| Passos por janela (684 e 2.334), `FALHA`, `vigia:`, `CRASH` | 684 e 2.334; 0, 0, 0 | iguais; 0, 0, 0 |
| `textura preta` (RGB zero na textura inteira) | 32 linhas | as mesmas 32 (mesmos quadros e tamanhos) |

**Leitura.**

- **O envio por linhas de cache tira 1,8 ms da gravação ocupada nos quadros pesados** [medido, por pares], com pouca dispersão (de
  1,3 a 3,1 ms em 103 pares): uns 0,6 ns por byte. É menos que os 2,5 a 4 ms estimados. Se o envio custava os 2,6 ns por byte da
  seção 18, agora custa uns 2,0 [estimado]: a memória pesa, mas não é tudo, ou o `dcbt` e o `dcbz128` só tiram uma parte das faltas.
  A abertura com `=2` diria qual das duas metades rendeu.
- **A apresentação do combate não melhorou**: o p99 e os intervalos acima de 34,37 ms ficaram onde estavam (9,34% e 9,55%). Os quadros
  lentos do combate já não são da gravação: na e1, 77 de 89 têm `ocupada` até 30 ms, e 80 são o jogo esperando o vblank (uns 10
  ms) enquanto a gravação fica parada (uns 10 ms sem trabalho), com a apresentação em 36 a 38 ms. O que segura esses quadros é o
  acerto de fase entre o jogo, a gravação e o vblank, não o trabalho de nenhuma das threads.
- **Na abertura da luta (janela 1) a e1 teve mais intervalos acima de 34,37 ms** (94 contra 21); dos 32 quadros dela acima de 36
  ms na e1, 27 são desse tipo (nenhum na e0). Com uma abertura de cada, não dá para separar efeito de variação: entre as aberturas da seção 17 o mesmo
  número do combate variou de 6,3% a 11,5%. Não há sinal de defeito (o resultado, os passos e a lista de texturas pretas iguais).
- **Nenhum bloco preto pelo log**: as 32 texturas inteiramente pretas são as mesmas, nos mesmos quadros, nas duas aberturas. O log
  não vê um bloco preto dentro de uma textura com outros texels; isso fica com as listas do WSL (a sabotagem das linhas) e com o
  olho na imagem.
- **A prova do WSL (`~/envio-rapido/provas.resumo` e `parte-a.resumo`) não foi conferida**: o WSL e o Deck estavam fora do ar
  (23/10). Pendente: a `.text`, o teste na PPC, as listas da `x86gv` com a chave 0 contra 1 nos dois roteiros, sem e com as
  threads, a contagem com a chave ligada, a sabotagem acusada nos dois roteiros e as listas da `ppcg`.

**Conclusão.** O corte é certo pelo console (nenhuma diferença visível no log) e rende 1,8 ms de gravação nos quadros pesados, sem
ganho visível na apresentação do combate, que agora é presa ao acerto de fase. Ligar por padrão depende de duas coisas: a prova do
WSL conferida e uma segunda dupla de aberturas (na ordem e1, e0, e de preferência a e2) para ver se os 94 intervalos da janela 1
se repetem. A frente seguinte da etapa 8 é a fase entre o jogo e a gravação nos quadros de 36 a 38 ms, não mais o envio.

| Pré-processado (`gcc -E -P`) do `passos.c` e do `plat_stub.c`, main (`75e99f5`) contra `67f97a0` | iguais nas definições da x86 plana, da `x86gv` (`BT3_TESTE_VU1`) e da PPC (`PORT_BIG_ENDIAN`); o `passos.c` muda com `PORT_X360`, onde a mudança está. O `plat_stub.c` não mudou neste commit; o `vblank_alvo.h` e o `gs_d3d.cpp` só entram na build do 360. Então os objetos da x86, da `x86gv` e da PPC são os mesmos e as listas não mudam (guia, "Uma mudança só no 360 depois das provas do WSL") |
| A prova do WSL da seção 18 (`~/vblank/provas.resumo`) | **pendente de conferência**: o WSL e o Deck estavam fora do ar |
| `teste_vblank.c` e os cinco mutantes | 0 falhas na conta do commit; os cinco mutantes saem com 1 (acima) |
| O banco do `passos.c` e o `passos.py` | a linha nova lida; o fim mudado recusado (acima) |

### A build do 360

`build_f2.sh` no `67f97a0`, carimbo `67f97a0 diff:da39a3ee5e`: 317 de 317 fontes sem erro, 985 avisos nos fontes do jogo (os
mesmos), 0 no `hl_passos` e no `gs_d3d`, os avisos dos outros fontes do 360 iguais aos da build do `3ae20e6`, os mesmos 103
stubs (`diff` com o `stubs.txt` anterior), `default.dat` `fb489b2f6e00d0e4de7e394f7037540c` e `default.swap`
`abbd7589c8425797c4a2639068c9bf54` iguais aos de referência. **`default.xex` `26591a584263234c079e8e65f784f00f`**. No
`default.map`: `vblank_cb` em `0x82277ba0` (a callback não mudou), `GsD3d_VblankEspera` em `0x82277f08`, `Passos_Tela` em
`0x82278088`.

### A rodada curta no console proposta

Uma abertura só, `BT3_VBLANK_HW=1` (o padrão), com saves novos. Os arquivos em `build/x360/f2/` do worktree `bt3xbx-vblank`:
`default.xex`/`.dat`/`.swap` do `67f97a0`, `f5_duelo.txt` e `boot.txt` (iguais aos de `port/x360/roteiros/`) e
`ambiente_vb2.txt` (o `ambiente_vb1.txt` com `BT3_SAVES=game:\saves_vb2`), do qual o `ambiente.txt` é a cópia.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt
# a abertura; depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/passos.py bt3xbx.log --console --janelas 2 --minimo 600 --carimbo "67f97a0 diff:da39a3ee5e"
```

**O que olhar**, contra o vb1 da seção 19:

1. **A janela 1** (684 passos, fim pausa): esperas de vblank bem acima de 0 (no vb1, 0), `alvo recomeçado R vezes pela dívida
   parada` com R pequeno (1 ou 2 [estimado pela cena A]), a espera de vblank da linha de tempo deixando de ser 0,0 e o trabalho
   p50 deixando de ser 33,27.
2. **A tela igual ou melhor**: 3 ou mais vblanks 0,00% e 0,13% no vb1, nenhuma de 4 ou mais; trocas perdidas e imagens puladas 0.
3. **A janela 2**: os recomeços pela dívida parada poucos (0 a uns 5 [estimado pela cena B]); o atraso ao acordar como no vb1
   (p99 0,06 ms); o `vblank já passado` dá a ideia de quantas vezes o passo passou de um vblank.
4. A parede dentro de 1% (no vb1, 0,9988 e 0,9968), o resultado da luta igual (vencedor 0x2, vida 0 27160, vblank 11478), nenhum
   `roteiro: FALHA`, `vigia:`, `CRASH` ou `gs_d3d: vblank: a espera no Sleep` durante a luta, e o `passos.py --console` com
   "tudo certo".

O caminho do controle à tela não se mede no console (não há marca do controle na imagem): a melhora de um vblank é da simulação
[simulado]. Se a janela 1 voltar a ficar sem espera, ou a tela piorar, o recomeço sai num commit e o padrão fica como no vb1.

## 23. A VU1 gravada direto nos buffers da GPU, `BT3_VU1_DIRETO` (22/10/2026)

Branch `f5-vu1copia`, sobre a main `64353a4`. O passo (e) da investigação de hoje: a thread de desenho (hardware 3, dona do D3D)
gasta no `frame_end` uns **3,2 a 3,7 ms por quadro pesado na cópia da VU1** [medido fora da janela], e o caminho da imagem é em
série (entrega do jogo, gravação ocupada, `frame_end`, GPU): o que estoura o intervalo é a variação dessa soma. Convenção:
**[medido]**, **[lido]** e **[estimado]**.

### O caminho de hoje [lido]

1. **A gravação** (`gs_draw.c`, os três `GsGpu_DrawVu*`) grava, por lote, os vértices (48 bytes cada, `memcpy` da memória da VU)
   em `gsVuVerts`, os índices da tira (`base + i`, três por triângulo) em `gsVuIdx`, um bloco de constantes (`Vu0Uniform`, 304
   bytes) em `gsVuUni` e a faixa de índices do desenho (`vu_faixa`). As listas são do jogo da gravação (`sJogos`, dois que se
   alternam na entrega, seção 8), em memória com cache (`malloc`).
2. **A entrega** (`entrega` do `gs_d3d.cpp`, na gravação, com o desenho parado) faz o `vu_plano`: para cada desenho da VU1, a
   faixa de vértices que os índices tocam e a posição no par de buffers da GPU (`sVuIni`, `sVuLo`, `sVuNv`).
3. **O `frame_end`** (desenho) faz o `vu_copia`: troca de par (dois, `VU_PARES`), `IsBusy`/`BlockUntilNotBusy` do par (o do quadro
   de dois atrás), `Lock` dos dois buffers (memória de escrita combinada, a do D3D sem `D3DUSAGE_CPU_CACHED_MEMORY`) e copia, por
   desenho, a faixa de vértices (`memcpy`) e cada índice rebaseado (`ib[ini + k] = idx[first + k] - lo + nv`): uns 54 mil
   vértices (2,5 MB) e 129 mil índices (0,5 MB) num quadro de luta (seção 8). É a "cópia da VU1".
4. **As constantes não se copiam**: o `desenha_vu` passa o `Vu0Uniform` da lista direto ao `SetVertexShaderConstantF`, que o põe
   no buffer de comandos. Já estão em memória com cache, como a proposta pede.

Como os desenhos da VU1 recebem vértices novos a cada lote, as faixas não se sobrepõem e saem em ordem: o par recebe a mesma
lista, na mesma posição. A cópia só move os bytes de um lugar para o outro.

### A proposta vale, com três jogos e não dois

**Vale**: a gravação já escreve esses mesmos bytes, uma vez, numa lista; escrevendo-os no buffer da GPU, a cópia some do desenho
inteira, e a gravação não escreve nada a mais (escreve em memória de escrita combinada em vez de memória com cache, sem a leitura
da linha antes de escrever). O desenho certo:

- **Três jogos de buffers, não o par das listas.** Os dois jogos de listas se alternam entre a gravação e o desenho, mas a GPU
  anda atrás do desenho: o quadro N é desenhado (a CPU) no `frame_end` N, e a GPU o lê depois, até uns 17 a 20 ms (`08-threads.md`, seção 2.1,
  a linha 7), enquanto o `frame_end` leva uns 12,5. A gravação começa o quadro N+2 no jogo de N logo que o `frame_end` N
  acaba (a entrega N+1 espera só isso): a GPU pode ainda estar lendo N. Com três jogos em rodízio (o quadro s no jogo s % 3), o
  jogo que a gravação começa é o de três quadros atrás, e o `frame_end` do de dois atrás já acabou: a GPU teve esse `frame_end`
  inteiro para acabar o de três atrás. É o mesmo instante e o mesmo quadro que o `vu_copia` de hoje espera com dois pares no
  começo do `frame_end` (`IsBusy`), que não esperou nenhuma vez na etapa 6 (`0 esperas pelo par`, `06.md`).
- **A gravação não chama o dispositivo** ("o jogo nunca pergunta à GPU, `IsBusy` é da dona", seção do filme no `gs_d3d.cpp`). O
  desenho põe, antes do `Present`, uma marca da GPU (`InsertCallback(D3DCALLBACK_IDLE | D3DCALLBACK_CPU1)`, a mesma dos picos e dos
  tempos) com o número do quadro; o callback roda quando a GPU acabou tudo o que veio antes e grava o número (`sVdFeito`). A
  gravação compara antes de regravar um jogo e, se a GPU não largou, espera ela mesma (100 µs por volta), **nunca o desenho**;
  passados 100 ms grava assim mesmo, contado (como as vagas do filme). Uma cerca comum (`InsertFence`) pediria o `IsFencePending`
  da thread da gravação; a marca custa à GPU um esvaziamento por quadro no fim dele, onde ela já está acabando.
- **Nada de reler a memória de escrita combinada.** A gravação lia dois índices por desenho de volta da lista (`vu_faixa`, o
  primeiro e o último índice, uns 5 mil desenhos por quadro). Em escrita combinada cada leitura vai à memória. A faixa passou a vir
  de quem gera os índices (commit `0109293`): a tira inteira nos programas 4 e 0, e nos que pulam triângulos marcados (o 6 e os
  destroços do 0) o primeiro e o último triângulo sem marca, lidos dos vértices de entrada (a memória da VU, com cache). Nenhum
  outro ponto da gravação lê `gsVuVerts` ou `gsVuIdx` (o `memcpy` e as escritas dos índices só escrevem).
- **A escrita sai do núcleo antes de a GPU ler.** A escrita combinada fica nos buffers de junção do núcleo da gravação; o `lwsync`
  da trava da entrega não a ordena. A entrega do backend faz um `__sync` quando o retrato é direto (o mesmo do filme, `filme360.c`).
- **Os buffers pelo XG**, como as texturas (`xg_cria`): cabeçalhos `XGSetVertexBufferHeader`/`XGSetIndexBufferHeader` sobre
  `XPhysicalAlloc` de escrita combinada, sem `Lock`. A ordem dos bytes dos vértices é a da declaração (`D3DDECLTYPE_FLOAT4`, 8 em
  32), a dos índices a do formato (`D3DFMT_INDEX32`), as mesmas de antes.
- **As constantes ficam onde estão**, nas listas (com cache).

### O que mudou

| Commit | O quê |
|---|---|
| `0109293` | A faixa de cada desenho da VU1 (`vu_faixa`) de quem gera os índices, sem reler a lista (`vu_faixa_marcas` para os triângulos marcados). O mesmo valor, nos dois modos; o `BT3_VU_PLANO_CONFERE` confere |
| `e562d0c` | `BT3_VU1_DIRETO=1` (desligado por padrão, só com a divisão): os ganchos `vuJogo` e `vuFeito` do `GsBackend`; no fim do `GsGpu_Init`, `vu_direto_liga` pega os três jogos, põe as capacidades da VU1 nas deles (se menores) e solta as listas da VU1 dos dois jogos; a cada entrega, depois de acordar o desenho, `vu_direto_proximo` passa ao jogo seguinte (esperando a GPU, se preciso); o retrato leva `vuJogo` e `vuSeq`. A espera conta na família `espera` da entrega. O falso (`gpu_falso.c`) com o mesmo arranjo e uma GPU falsa atrasada (abaixo) |
| `df50afc` | O 360: os três jogos no `vu_init` no lugar dos dois pares (sem memória, os pares), o `frame_end` desenha do jogo do retrato sem o `vu_copia`, a marca antes do `Present`, o `__sync` na entrega; o tempo da cópia no log de 60 quadros (`VU1, buffers: ...; gravados direto em 3 jogos de buffers ...`, `cópia X ms por quadro (pior Y)`) e na linha dos picos (`frame_end F (cópia da VU1 c)`) |

**A memória (360, `BT3_VU_VERTS=98304`).** Sem a chave: as listas da VU1 nos dois jogos (2 x 14,5 MB) e dois pares (11,8 MB),
40,8 MB. Com ela: três jogos de 5,9 MB, 17,7 MB: **uns 23 MB a mais livres**. A capacidade da VU1 na gravação passa de 260.000
vértices e 524.288 índices para 98.304 e 294.912, as do par de hoje (o maior quadro medido: 53.618 e 128.907). O que não cabe
passa a ser pulado na gravação (como os outros caminhos cheios, seção 8), não no `vu_plano`.

**A GPU falsa do WSL.** No falso, os três jogos são `malloc` com a capacidade das listas (a lista sai a mesma) e uma GPU falsa os
lê com atraso: o quadro desenhado agora é relido (o hash dos vértices e dos índices refeito sobre o jogo) `BT3_GPU_FALSO_ATRASO`
quadros depois (padrão 1; 0 a 2), e só então o `vuFeito` o dá por lido. Um jogo regravado antes da hora sai como `gpu_falso: VU1
direto: o jogo J do quadro de ordem S relido diferente` e no resumo (`N quadros relidos pela GPU falsa (atraso A), D
diferentes`). Com o atraso 2 a gravação precisa esperar a cada quadro (o jogo de três atrás é lido no fim do quadro seguinte ao
que ela acabou de entregar): é o que exercita a espera.

### A estimativa [estimado]

- **Desenho:** os 3,2 a 3,7 ms da cópia nos quadros pesados saem inteiros (fica a troca de ponteiro); o `frame_end` deles cai
  nisso. No caminho em série, uns 3 ms a menos onde a cópia pesava.
- **Gravação:** os mesmos bytes, uma vez, agora em escrita combinada (sem a leitura de cada linha de 128 bytes antes de escrever,
  e sem sujar o L2 com 3 MB por quadro): igual ou um pouco menos. A espera pela GPU deve ser zero (o mesmo instante em que o
  `vu_copia` não esperava). O risco é a junção da escrita combinada com três fluxos intercalados (vértices, índices, e as listas
  com cache no meio): o `ocupada` dos picos diz.
- **GPU:** uma marca por quadro (um esvaziamento no fim, como a dos picos, que a rodada já liga).

### As provas

**Feitas aqui [medido]:**

- **Fora das builds de teste e do 360 nada muda.** O `gcc -E -P` de cada `port/src/gs/*.c` na configuração da x86 plana (sem
  definições, com `-Iport/x360/headless -Iport/x360/src/gs/sdl`) sai igual na main (`64353a4`) e no branch; nas da `x86gv`
  (`-DBT3_TESTE_VU1 -DBT3_GPU_FALSO`) e da `ppcg` (`-DPORT_BIG_ENDIAN -DBT3_GPU_FALSO`) o `gs_draw.c` sai diferente, como deve. O
  `gs_draw.c` da x86 plana pré-processa inteiro (4.242 linhas, sem erro), e o controle (`gsScale = 2` trocado por `3` numa cópia)
  sai diferente. (O `gs_gpu.c` e o `gs_gl.c` param no `shaders.h` gerado, iguais dos dois lados; não entram nas builds sem
  janela.) Logo a `.text` da x86 plana é a da main, `43444ea4d03a6d448aa1ae74a631e35b356a7caa`; a rodada abaixo a mede.
- **A sintaxe** do `gs_draw.c` (as três configurações) e do `gpu_falso.c` no `gcc -fsyntax-only -Wall`: sem aviso.
- **A build do 360** (`build_f2.sh` no `df50afc`): 317 de 317 fontes sem erro, os avisos por fonte iguais aos da build da main
  (347 arquivos de saída do `cl`, nenhum diferente; o `gs_d3d.cpp` sem aviso), os mesmos 103 stubs (`stubs.txt` igual),
  `default.dat` `fb489b2f6e00d0e4de7e394f7037540c` e `default.swap` `abbd7589c8425797c4a2639068c9bf54`, iguais. No `default.map`:
  `vu_faixa_marcas`, `vu_direto_proximo`, `vu_direto_liga`, `vu_jogo`, `vu_feito`, `vu_cb`, e o `XGSetVertexBufferHeader` e o
  `XGSetIndexBufferHeader` do `xgraphics.lib`. O `.c` preparado pelo `c89.py` conferido no `vu_direto_proximo`.

**A rodada do WSL (serviço `claude-vu1copia`, disparado sem esperar).** O script `~/vu1copia/rodada.sh` (usuário `codex`, worktree
`~/bt3xbx-vu1copia` do `vu1copia.bundle`, cada passo pesado em `flock ~/wsl.lock`), resumo em `~/vu1copia/provas.resumo`, uma linha
por passo terminando em `FIM`. Todas as listas com as duas threads (`BT3_GS_THREAD=1 BT3_GS_DESENHO=1`) e `BT3_VU_PLANO_CONFERE=1`:

1. A `.text` da x86 plana na main e no branch (vale com `43444ea4…` nas duas).
2. `x86gv`, `f5_duelo` e `f5_treino`: a chave 0 contra 1; a chave 1 com o desenho atrasado 20 ms e a GPU falsa atrasada 2 quadros
   (a espera da gravação exercitada); a chave 1 com a gravação atrasada 20 ms; e a chave 1 sem a divisão (ignorada). Vale com o
   `divergencias.py` saindo 0 em todos contra a chave 0, os hashes de sempre (`08a98b6d05b8775a` e `f76f0f6849b64629`), `0
   diferentes` na linha `gpu_falso: VU1 direto: N quadros relidos` e no `BT3_VU_PLANO_CONFERE`, nenhuma espera além do limite
   (`passaram do limite` 0) e, com a GPU atrasada 2, esperas acima de zero.
3. **As sabotagens** (`x86gv`, `f5_duelo`, o desenho atrasado 20 ms; aplicadas por `python` com `assert` de que casaram, desfeitas
   por `git checkout` e a `x86gv` refeita): (1) **o par trocado**, a gravação regrava o jogo do retrato que o desenho ainda desenha,
   sem esperar: tem de sair `divergencias.py` 1 e linhas `relido diferente`; (2) **a cerca ignorada**, o rodízio certo sem esperar a
   GPU falsa atrasada 2: tem de sair `relido diferente` (a lista pode sair igual: o fim do quadro já tinha lido o jogo; é o que a
   releitura atrasada acrescenta à prova).
4. A `x86r`, a pasta de dados dos roteiros (`roteiros.sh f5_treino f5_duelo`), a `ppcg` e a chave 0 contra 1 nos dois roteiros
   (hashes `994627a2aecd429a` e `f76f0f6849b64629`).
5. O git do worktree e do clone sem alterados no fim.

Quando foi disparado, a trava estava com as rodadas de outras frentes (`bt3-f6trava`, `claude-vblank`, `claude-shen3`) e o
serviço ficou na fila: as provas da lista ficam para quem ler o resumo. Sem elas a chave não liga por padrão.

### A rodada no console: o que olhar

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-vu1copia`: `default.xex`/`.dat`/`.swap` (o `.xex` da build refeita no commit
desta nota; o md5 está no relatório da etapa), `f5_duelo.txt` e `boot.txt` (cópias de `port/x360/roteiros/`), `ambiente_d0.txt`
(`BT3_VU1_DIRETO=0`, saves `saves_vd0`) e `ambiente_d1.txt` (`=1`, saves `saves_vd1`), os dois como o `ambiente_picos.txt` da main
(`-BT3_DEMO`, o `f5_duelo` pelo roteiro, `BT3_TEMPOS=0`, `BT3_PICOS=36`, `BT3_WIDE=1`, decisão 26, as duas threads); o
`ambiente.txt` é a cópia do `ambiente_d0.txt`.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_d0.txt build/x360/f2/ambiente_d1.txt
# abertura 1 (chave desligada); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_d1.txt
# abertura 2 (chave ligada); baixar o log de novo
```

1. **A chave pegou** (abertura 2): `gs_d3d: VU1: 3 jogos de buffers em que a gravação grava direto (BT3_VU1_DIRETO=1), 98304
   vértices e 294912 índices cada (17280 KB ao todo ...)` e `gs: BT3_VU1_DIRETO=1: a VU1 gravada direto em 3 jogos de buffers do
   backend, 98304 vértices e 294912 índices ...`. Na abertura 1, a linha de sempre (`2 pares de buffers`).
2. **A cópia e o `frame_end` nos quadros pesados**: nas linhas `picos: quadro ...`, o `desenho: frame_end F (cópia da VU1 c)`. Na
   abertura 1, `c` de uns 3,2 a 3,7 nos quadros de luta pesados; na 2, perto de 0,0, e o `F` uns 3 ms menor nos mesmos tipos de
   quadro. Fora da janela, a linha `gs_d3d:   VU1, buffers: ... cópia X ms por quadro (pior Y)` dá a média de 60 quadros.
3. **A gravação não piorou**: o `gravação G (ocupada O ...)` das mesmas linhas, e a espera pela GPU: `gs: VU1 direto
   (BT3_VU1_DIRETO): N quadros em 3 jogos de buffers; a gravação esperou a GPU largar o jogo E vezes (0 passaram do limite de 100
   ms) ...` (esperado E perto de 0; um `passaram do limite` acima de 0 é um quadro que pode ter saído com vértices errados).
4. **O que se vê**: os lutadores, o estágio e as sombras no lugar (nada de vértice esticado, triângulo solto ou modelo piscando),
   sobretudo nos especiais e nas trocas de câmera. Um erro na ordem dos bytes dos buffers do XG daria a luta inteira quebrada logo
   no primeiro quadro.
5. **O resultado**: a mediana, o p95 e o p99 do `passos: janela` da luta e os intervalos acima de 34,37 ms contra a abertura 1; os
   passos por janela e o resultado iguais aos da referência (`vencedor 0x2 KO`); nenhum `roteiro: FALHA`, `vigia:` nem `CRASH`. A
   memória livre mínima (`gs_d3d:   memória`) uns 23 MB maior na 2.

## 24. A rodada do console da VU1 direto (22/10/2026) [medido]

O `.xex` do commit `0fa201a` (`passos: origem 0fa201a`), duas aberturas do `f5_duelo` pelo roteiro, sem som, `BT3_PICOS=36`,
`BT3_TEMPOS=0`: a abertura 1 com `BT3_VU1_DIRETO=0` (`build/sessao_2210/d0/bt3xbx.log` do clone principal) e a 2 com `=1`
(`d1/bt3xbx.log`). Lidos com o `passos.py --console`, a linha de 60 quadros do `gs_d3d` e as linhas `picos:` (scripts de uma vez no
scratchpad, só texto).

**A chave pegou.** Na 1, `2 pares de buffers ... (11520 KB)` e `copiados para 2 pares`; na 2, `3 jogos de buffers em que a gravação
grava direto (BT3_VU1_DIRETO=1) ... (17280 KB ao todo ...)`, `gs: BT3_VU1_DIRETO=1: ... (as listas da VU1 dos dois jogos soltas, 28471
KB)` e `gravados direto em 3 jogos de buffers`. O maior quadro coube nas duas (53.797 de 98.304 vértices), nenhum desenho da VU1
pulado.

| | chave 0 | chave 1 |
|---|---|---|
| cópia da VU1, média dos 13 trechos de 60 quadros com cópia de 3 ms ou mais (as cenas da luta antes e depois da janela) | 3,82 ms | 0,00 |
| fim do quadro, os mesmos 13 trechos | 11,05 ms | 6,22 ms (de 3,0 a 8,1 a menos em cada um) |
| desenho da VU1 sem a cópia, os mesmos trechos | 5,42 ms | 4,42 ms |
| quadro inteiro, os mesmos trechos | 34,24 ms | 34,24 ms |
| o trecho da pausa (quadro 5399): fim do quadro, cópia, desenho da VU1 com a cópia | 24,06; 6,40; 22,02 ms | 24,17; 0,00; 22,18 ms |
| picos na janela da luta (linhas `picos: ..., na janela`) | 72 | 96 |
| neles: `frame_end` mediana (p90) e a cópia mediana | 16,75 (20,50) e 5,30 ms | 9,90 (16,30) e 0,00 ms |
| neles: gravação `ocupada` mediana (p90, máximo) | 27,75 (33,10; 84,90) ms | 27,65 (31,80; 79,30) ms |
| neles: a gravação `esperando o desenho` acima de 1 ms | 3 quadros | 0 |
| neles: o jogo esperando a gravação, mediana | 8,10 ms | 9,45 ms |
| a gravação esperou a GPU largar o jogo | (não se aplica) | 0 vezes em 8.641 quadros, 0 além do limite |
| entrega média (linha `gs: divisão`, a sessão inteira) | 0,207 ms | 0,293 ms |
| memória livre: no começo; mínimo visto | 248; 53 MB | 270; 75 MB |
| janela 1 (684 passos), apresentação: p99, máximo, acima de 34,37, acima de 50,05 | 36,01; 85,73; 26 (3,81%); 1 | 37,24; 80,73; 33 (4,83%); 1 |
| janela 2 (2.334 passos), apresentação: p99, máximo, acima de 34,37, acima de 50,05 | 37,11; 52,21; 227 (9,73%); 1 | 38,92; 51,12; 256 (10,97%); 1 |
| janela 2, trabalho: p95, p99, máximo | 34,78; 34,96; 38,22 | 34,89; 34,94; 37,75 |
| resultado | `vencedor 0x2 razao 0x1 vida 0 27160` | o mesmo |

Nenhum `roteiro: FALHA`, `vigia:` nem `CRASH` nas duas; os passos por janela (684 e 2.334) e as matrizes de ações iguais. A
abertura 2 foi encerrada mais cedo depois do roteiro (8.640 retratos contra 13.920), o que não toca a janela. O que se vê na tela
não está no log: fica com quem abriu.

**A referência do ruído.** As outras aberturas de 22/10 do `f5_duelo` com `BT3_PICOS=36`, com ou sem som, com a chave da sua
rodada desligada ou sem mudança no caminho da luta (`e0`, `e1`, `trava`, `sx1`, `vb0` e a `d0`) deram na janela 2 de 218 a 269 intervalos acima de 34,37 ms e p99 da apresentação de 36,94 a 37,73.
A chave 1 (256; 38,92) fica dentro da faixa no primeiro número e um pouco acima no segundo: com uma abertura de cada lado, não
há ganho nem perda que se separe do ruído.

**Conclusão.** A cópia saiu inteira do desenho e o `frame_end` caiu uns 4,8 ms por quadro nas cenas pesadas, um pouco mais que a
cópia (o desenho da VU1 em si ficou 1 ms mais rápido, o que bate com o L2 sem os 3 MB por quadro) [medido]; a gravação não piorou,
nunca esperou a GPU, e sobraram 22 MB. Mas **o intervalo entre imagens não mudou**: o `quadro inteiro` dos mesmos trechos é igual
ao centésimo, e na janela da luta o que estoura é a soma do jogo e da gravação (`ocupada` perto de 28, o jogo esperando a gravação
uns 8 a 9 ms e o vblank uns 6,6), não o desenho, que nos picos só segurou a gravação em 3 de 72 quadros. Cortar o desenho tira a
folga dele, não o pico. No trecho da pausa o `frame_end` ficou igual (24 ms) com a cópia ou sem ela: o desenho da VU1 cresceu
exatamente o que a cópia deixou de custar, o que aponta para um trecho limitado pela GPU, em que a CPU do desenho só troca a
cópia por espera no anel de comandos [estimado].

**A prova do Deck** (`~/vu1copia/provas.resumo` no Deck, a rodada da seção 18 adaptada) **está pendente de conferência**: nesta
análise o Deck e o WSL estavam fora da rede e nenhum ssh foi feito. Os itens 1 a 5 da lista da seção 18 seguem sem leitura.

**Recomendação.** Pode ir à main desligada por padrão (fora das builds de teste e do 360 nada muda, e no 360 a chave 0 é o caminho
de antes). Ligar por padrão pede a prova do Deck conferida (sobretudo as sabotagens e a GPU falsa atrasada 2 sem `relido diferente`)
e a imagem vista nos especiais; o ganho que justifica ligar é a memória (22 MB) e a folga do desenho, não o tempo de quadro. O
próximo corte de tempo tem de vir da gravação ou do jogo (`ocupada` e a espera do jogo nos picos), não do desenho.

## 25. Rumo a nenhuma imagem longa na luta: o que os logs dizem e a tela por imagem (23/10/2026)

Branch `f5-zero`, sobre a main `5021312`. A meta do Lucas: **nenhum pico na luta** (fora dela não importa). Pedido: ligar cada
imagem que ficou 3 vblanks no combate ao quadro e à causa, com as aberturas de 23/10, e propor o caminho para zero; se o log não
bastar, uma chave barata que registre isso. Convenção: **[medido]**, **[lido]** e **[estimado]**.

### O que os logs de 23/10 dizem [medido]

Dez aberturas do `f5_duelo` pelo roteiro, todas a 1,33 (logs fora do git): `build/sessao_2310d/` `kbase`, `kenv` (envio por linhas),
`kvu` (VU1 direto), `kambos` (os dois), as quatro com `BT3_PICOS=36`; `x0`, `x1` (FXAA 0 e 1) e `x1m` (FXAA com `BT3_TEMPOS=1`);
`build/sessao_2310c/` `m133e0` e `m133e1`; `build/sessao_2310b/vb2` (com som e `BT3_PICOS=36`). As outras sem som.

| Janela (passos) | Imagens de 3 vblanks, nas 10 | De 1 vblank | De 4 ou mais | Maior intervalo na CPU depois do `Present` |
|---|---|---|---|---|
| 1, a abertura da luta (684) | 11 (de 0 a 3 por abertura) | 0 | 1 (só na `kambos`) | 64 a 75 ms em **todas** |
| 2, o combate (2.334) | 41 (de 2 a 7) | 44 (de 3 a 7) | 0 | 44 a 52 ms |

1. **As imagens de 3 e as de 1 vêm aos pares**, e os vblanks da janela 2 são exatamente 2 por passo nas 10 (4.668): cada imagem
   longa é um quadro que perdeu o vblank, e o seguinte entrou no tempo (a de 1). O ritmo não se perde; o tranco é de um quadro.
2. **A contagem da tela não vê as repetições.** No quadro 5338 (logo depois da ação 0x10f do roteiro, o Blast 2: 65 texturas
   novas, 3,2 M pixels, 33 ms de decodificação e 7 ms do XG, a gravação ocupada 77 a 80 ms) a tela ficou parada 64 a 75 ms em
   todas as aberturas. Em 4 das 5 com `picos` a linha diz `1 repetidas`: o `GsD3d_Repete` (50 ms sem quadro) apresentou a mesma
   imagem de novo, e a callback de vblank contou duas imagens de 2. Só na `kambos` (64 ms, sem repetição) ela aparece como "4 ou
   mais 1". **Há uma imagem de 4 vblanks por abertura, na luta, que o critério da seção 19 não vê.**
3. **O jogo não é a causa no combate**: nas linhas `picos` da janela 2 das quatro aberturas, o período do jogo fica entre 33,1 e
   33,5 ms, e os resumos dizem `0 pelo jogo`; o trabalho p99 é 30,4 a 33,3 ms.
4. **Dois quadros se repetem nas quatro aberturas com `picos`**, com o período da GPU (do fim de um quadro ao do anterior) de 44 a
   50 ms, que não cabe em 2 vblanks: **6960** (os desenhos caem de uns 2.400 para 1.400, uma troca de cena; o `frame_end` dura só
   6 a 9 ms, mas a gravação entregou uns 6,5 ms depois do período e a GPU acabou 16 ms depois da CPU, contra 10 a 14 nos outros)
   e **7017** (15 texturas novas, 1 M pixels, 10,6 ms na gravação: ocupada 30 a 32 ms, contra 17 a 19 nos vizinhos). Na `kenv` e
   na `kambos`, que tiveram 2 imagens de 3 no combate, são essas duas [estimado: o log não liga a imagem ao quadro].
5. **O resto varia de abertura a abertura com o mesmo roteiro** (de 0 a 5): quadros com o período da GPU de 36 a 39 ms (6087,
   6995, 7063, 6930, 6990) entram ou não no prazo conforme a folga de fase daquela abertura. Os cortes de CPU da etapa 8 (envio por
   linhas, VU1 direto) não separam do ruído: 5, 2, 3 e 2 com um cada.
6. **O sistema não aparece**: o quadro 4807 teve o mesmo intervalo (36,5 ms) nas quatro com `picos`, com a escritora do diário
   gravando 109 a 130 ms nele em três e parada na `kbase`; as aberturas sem som (`kbase` 5, `x0` 7) têm tantas quanto a com som
   (`vb2` 5).
7. **A GPU a 1,33** (`x1m`, as linhas de 60 quadros fora das janelas): 20 a 26 ms por quadro no combate (com uns 4 ms das marcas
   da medição) e 32,7 ms (pior 33,0) no trecho do Blast 2 e da pausa (quadros 5340 a 5399).

### Por que o log não liga a imagem ao quadro [lido]

- A callback de vblank (`vblank_cb`) só soma: quantas imagens ficaram 1, 2, 3 ou 4 vblanks, sem o número da troca nem o quadro.
- As linhas `picos` saem pelo intervalo na CPU depois do `Present` acima do limiar (36 ms); uma imagem que entrou um vblank tarde
  com o intervalo na CPU abaixo dele não tem linha, e um quadro com linha pode ter entrado no tempo.
- Os carimbos do `picos` são períodos (de uma entrega à anterior), não momentos: não dá para saber a que distância do vblank cada
  lado entregou.
- A repetição é uma troca nova com a mesma imagem.

O que falta registrar: **o número da troca de cada `Present`** (o `QuerySwapStatus` logo depois dele, que a amostra
`DualFrameAsyncSwaps` do XDK usa assim) e **o vblank em que cada troca entrou**, com o `mftb` dele (o `D3DVBLANKDATA.Swap` na
callback); **o `mftb` de cada lado do quadro** (a entrega do jogo, a da gravação, o `Present`, a GPU acabando); e, para cada
troca, **o prazo**: o segundo vblank depois de a imagem anterior entrar.

### A chave `BT3_PICOS_TELA=1` (commit `dac2466`)

Só no `gs_d3d.cpp` (360), desligada por padrão; nada portável muda.

- **A DPC** guarda, por troca, o vblank e o `mftb` em que ela entrou (um anel de 64, só inteiros: no `dumpbin` do `vblank_cb`,
  `lwz`, `stw`, `std`, `mftb`, `lwsync` e as chamadas, nenhuma instrução de float nem de vetor).
- **O desenho**, logo depois de cada `Present` (o do quadro e o do `GsD3d_Repete`), guarda por troca o quadro do GS, se é uma
  repetição e o `mftb`; e lê as trocas que entraram desde a última vez. Os carimbos dos três lados são os do `picos` (`PicoJ` e
  `PicoG` ganham o `mftb` da entrega); sem `BT3_PICOS`, a chave liga o `picos` com limiar de 100.000 ms (os carimbos sem as linhas).
- **Na janela do passos**, uma linha por imagem de fato (as repetições somadas à imagem que repetem) que não ficou 2 vblanks:

```
picos: tela: a imagem do quadro A ficou N vblanks na tela (K na última troca); a seguinte, do quadro B (troca S), entrou no
vblank V; até o prazo (+ antes, - depois): a entrega do jogo J, a da gravação G, o Present P, a GPU acabou U ms; o quadro: jogo
..., gravação ocupada ... (sem trabalho ...; texturas ..., ... ms), frame_end ...; a troca anterior: Present ..., GPU ... ms antes
do prazo dela
```

- **No fim de cada janela**, `picos: tela, janela:` com as trocas, as repetições, as trocas sem o quadro, as puladas, as imagens de
  fato por vblanks e a folga até o prazo de todas as trocas (mínimo, p1, p10 e mediana, da GPU acabando e do `Present`).
- **O teste** `port/x360/tools/teste_tela.sh` recorta do `gs_d3d.cpp` os anéis e o `tela_*` e os compila com o `teste_tela.cpp`
  (g++ daqui; o relógio, a DPC, o `QuerySwapStatus` e os carimbos simulados): uma cena com uma imagem de 3, uma de 1 e um engasgo
  de 4 escondido por uma repetição; "tudo certo" [medido]. **Os cinco mutantes saem com 1**: a repetição ignorada (`rep = 0`, 3
  falhas), a soma trocada por atribuição (1), o prazo a 1 vblank (1), só as imagens acima de 2 (1) e a repetição contada como
  imagem (2).
- **A build** (`build_f2.sh` no `dac2466`, carimbo `dac2466 diff:da39a3ee5e`): 317 de 317 fontes sem erro, 0 avisos no `gs_d3d`,
  os mesmos 103 stubs, `default.dat` `fb489b2f6e00d0e4de7e394f7037540c` e `default.swap` `abbd7589c8425797c4a2639068c9bf54`,
  **`default.xex` `7cbc25ea06341cbafc2b4e55417b4bd0`**.

### O caminho para zero (proposta) [estimado]

| Ordem | Correção | O que tira, por abertura | Custo |
|---|---|---|---|
| 1 | **A medida**: a linha `tela` do passos somando as repetições à imagem que repetem (o desenho passa à DPC o número das trocas repetidas) e, na luta, sem `GsD3d_Repete` enquanto a gravação trabalha num quadro | nada na tela; o critério passa a ver a imagem de 4 do Blast 2 | pequeno, só no 360 |
| 2 | **As texturas novas fora da gravação**: o cache pela origem (seção 17, (a): na luta, 81% dos pixels decodificados são conteúdo já visto) e, para a primeira vez (o Blast 2), a decodificação numa thread auxiliar (seção 17, (b)) | a imagem de 4 da janela 1 e a de 3 do 7017 (umas 1 + 1) | médio; a lista do WSL prova a imagem |
| 3a | **A folga de fase pela cadeia**: cortar a gravação, o desenho e a GPU do quadro (a GPU a 1,33 é a maior parcela) | as de 3 que passam do prazo por poucos ms (de 0 a 5) | depende do que a chave mostrar |
| 3b | **Um vblank de folga na luta**: `D3DRS_PRESENTINTERVAL` em `D3DPRESENT_INTERVAL_TWO` só na luta (o 360 muda por quadro; a amostra `MultiRateRender` do XDK faz isso) e o primeiro quadro da luta um vblank mais tarde | as de fase (6960 e as variáveis): todas as que atrasam menos de 16,6 ms | **16,6 ms a mais do controle à tela na luta**, contra o "nenhum quadro a mais de atraso" da seção 19: decisão do Lucas |

Com 1 e 2 sobram as de fase (de 1 a 5 por abertura); 3a ou 3b decide-se pela folga que a chave medir: se o p1 da folga da GPU for
de poucos ms e as imagens longas passarem do prazo por poucos ms, cortar a cadeia rende; se as longas passarem por 6 a 16 ms
(o 6960), só a folga de um vblank zera.

### A rodada no console proposta

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-zero`: `default.xex`/`.dat`/`.swap` do `dac2466`, `f5_duelo.txt` e `boot.txt`
(iguais aos de `port/x360/roteiros/`), `ambiente_z1.txt` e `ambiente_z2.txt` (o jogo como ele roda, com os padrões da main e o
som, mais `BT3_TEMPOS=0`, `BT3_PICOS=36`, `BT3_PICOS_TELA=1`; saves novos `saves_zero_z1` e `saves_zero_z2`); o `ambiente.txt` é a
cópia do `ambiente_z1.txt`. As duas aberturas iguais: o dobro de casos.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_z1.txt build/x360/f2/ambiente_z2.txt
# abertura 1; depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_z2.txt
# abertura 2; baixar o log de novo
python3 port/x360/tools/passos.py bt3xbx.log --console --janelas 2 --minimo 600 --carimbo "dac2466 diff:da39a3ee5e"
grep 'picos: tela' bt3xbx.log
```

**O que olhar:**

1. A linha do init `gs_d3d: picos: a tela por imagem ligada`. No resumo `picos: tela, janela:` de cada janela, **as trocas sem o
   quadro perto de 0** (se forem quase todas, o `Swap` do `QuerySwapStatus` não é o do `D3DVBLANKDATA`: a ligação está errada e
   as linhas não valem) e as repetições: na janela 1, 1 (o Blast 2) [estimado].
2. **As imagens de fato** do resumo contra a linha `passos: janela K tela:`: as mesmas, menos a repetição (na janela 1, a de 4).
3. **Cada linha `picos: tela:`** da janela 2: o quadro, e qual lado passou do prazo (a GPU acabou depois dele com o `Present`
   antes: a GPU; o `Present` depois dele: a gravação ou o desenho, pelo `ocupada`, `texturas` e `frame_end`). Esperado: o 6960 e o
   7017 nas duas aberturas.
4. **A folga da GPU até o prazo** (mínimo, p1, p10, mediana): decide entre 3a e 3b.
5. O resultado da luta igual (vencedor 0x2 razão 0x1 vida 0 27160), os passos por janela (684 e 2.334), nenhum `roteiro: FALHA`,
   `vigia:` ou `CRASH`, e as imagens longas da linha `tela` na faixa das aberturas de 23/10 (a chave não deve mudar o ritmo).

## 26. O cache de texturas pela origem, `BT3_TEX_ORIGEM` (23/10/2026)

Branch `f5-texcache`, sobre a main `5021312`, com o `f5-zero` juntado (a linha por imagem longa, `BT3_PICOS_TELA`, seção 25).
Pedido: o caminho 2 da seção 25, a opção (a) da seção 17. Os picos que se repetem na luta são de texturas novas na gravação: o
Blast 2 (quadro 5338, 65 texturas, 3,2 M pixels, 33 ms de decodificação e 7 ms do XG; a tela parada 64 a 75 ms em todas as
aberturas) e o 7017 (10,6 ms na gravação). Convenção: **[medido]**, **[lido]** e **[estimado]**.

### O que foi feito

| Commit | O quê |
|---|---|
| `f84354a` | O cache pela origem no `texture_get` (`gs_draw.c`) e o `Gs_TexOrigem` (`gs_core.c`), sob `BT3_TEX_ORIGEM=1` (lida uma vez), **desligado por padrão**; a linha "R" da contagem e o `tex_conta.py` com `--quadros` e `--quadro` |
| `a6c39b2` | Junta o `f5-zero` (só o `gs_d3d.cpp` do 360, o teste da tela e a nota) |
| `3a9256b`, `bbd4200` | `BT3_GPU_NORMAL=2` no `gpu_falso.c`: a lista normal com as junções pelo conteúdo (abaixo) |
| `bf7f96c` | A chave só com o que a decodificação lê da paleta e do `TEXA` (a primeira versão não pegava o Blast 2, abaixo) |

**A chave [lido].** O endereço de cada pixel no `vram_rw` é `tbp * 256` bytes mais uma conta que só depende de `tbw`, do formato,
de x e de y (o bloco é `tbp` mais um número da grade, com a volta nos 4 MB). O `Gs_TexOrigem` hasheia, em ordem de grade a partir
de `tbp`, os blocos de 256 bytes em que caem os pixels: a mesma imagem enviada a outro endereço dá o mesmo hash, e uma página
mudada fora dos blocos da textura não o muda. A paleta entra pelo que a decodificação lê: as 256 cores de 32 bits são os blocos
`cbp` a `cbp + 3` inteiros, as de 16 bits `cbp` e `cbp + 1`; as 16 cores (8 x 2) dividem o bloco com outras paletas e entram cor
a cor, pelo `vram_rw`. A chave completa: o hash, o formato, o `tbw`, o tamanho, o formato da paleta e o `TEXA` só quando o
`Gs_Expand` o lê (cores que não são de 32 bits). Quatro faixas de 64 bits, o laço sem `__int128` do `Gs_PageHash`.

**O reuso [lido].** Uma vaga por recurso decodificado (`sOrig`, até `MAX_TEX`), com a chave e quantas entradas do `sTex`
apontam para o recurso. No erro do `texture_get` (depois do descarte e da varredura do cache cheio, que podem soltar vagas), o hash
é feito; se a chave tem vaga, a entrada nova aponta para o mesmo recurso, sem decodificar nem criar (sem `malloc`, sem XG); o tempo
do hash conta como o da decodificação (`gPicosTexNs`), e só as decodificações contam em `texturas t` dos `picos`. O recurso só é
solto quando a última entrada sai (`tex_solta`, nas duas varreduras); o descarte do 360 (`GsDraw_X360Abre`) só soma os bytes de
um recurso de que saem todas as entradas. Com um pacote de texturas carregado, o cache fica de fora. Desligado, nenhuma entrada
tem vaga e o caminho é o de antes; tudo dentro de `BT3_GS_ESPERAS` (a x86 plana não muda).

**Por que a primeira versão não pegava o Blast 2 [medido].** Ela hasheava 1 KB a partir de `cbp` e o `TEXA` sempre. Na x86gv o
`f5_duelo` reusou 1.549 texturas, mas o 5338 seguiu com as 65 e os 7015 a 7017 com as suas. Pela contagem, as 65 do Blast 2 eram
o mesmo `TEX0` (o mesmo `tbp`, `tbw` e paleta) de texturas do quadro 4136, com o `TEXA` `8000008080` contra `8000000000`: a
expansão das cores de 32 bits não lê o `TEXA`. E nos 7015 a 7017 (4 bits) as 16 cores dividem os blocos com outras paletas que o
jogo reescreve. Com a chave exata (`bf7f96c`), os quatro quadros saem inteiros do cache.

**Achado lateral, sem correção [lido].** O `clut_hash` do `gen` (a chave de sempre) lê as 16 cores de 4 bits como `(i & 15, 0)`,
e a decodificação as lê como `(i & 7, i >> 3)`: as cores 8 a 15 (a segunda linha) não entram no `gen`. Uma mudança só nelas
deixaria a textura velha no cache. Corrigir muda as listas de sempre; fica anotado.

### A medida na x86gv [medido]

`BT3_TEX_CONTA` com a chave 0 e 1, sem as threads; o custo pela estimativa no console a 10,4 ns por pixel (seção 17, a rodada de
22/10). "Reusadas": as linhas "R", cada uma conferida pixel a pixel (abaixo).

| | `f5_duelo`, chave 0 | chave 1 | `f5_treino`, chave 0 | chave 1 |
|---|---:|---:|---:|---:|
| Texturas decodificadas | 2.424 | **687** | 1.242 | **564** |
| Pixels decodificados | 37,8 M | 20,9 M | 28,4 M | 17,7 M |
| Quadros com decodificação | 546 | 157 | 257 | 115 |
| Reusadas sem decodificar | | 1.737 (16,9 M pixels) | | 678 (10,7 M pixels) |
| Primeira vez, dos pixels decodificados | 55,1% | 99,7% | 62,1% | 99,6% |

**A luta** (`--quadros 4673:7758`, 4:3):

| | Chave 0 | Chave 1 |
|---|---:|---:|
| Texturas decodificadas | 1.805 (19,2 M pixels) | **217 (3,7 M pixels)**, todas de primeira vez |
| Reusadas | | 1.588 (15,5 M pixels) |
| Quadros com decodificação | 404 | 59 |
| Quadros de 5 ms ou mais (estimado) | 3: 5338 (33,4 ms), 7015 (5,8), 7017 (5,2) | **0** |
| O maior | 5338, 65 texturas, 3,2 M pixels | 4905, 15 texturas, 300 mil pixels, 3,1 ms |
| Quadro 5338 (Blast 2) | 65 decodificadas | **0**; 65 reusadas |
| Quadros 7015, 7016, 7017 | 5, 14 e 11 decodificadas | **0**; todas reusadas |

O que sobra na luta é a primeira vez: o efeito dos quadros 4976 a 4980 (291 mil pixels por quadro, uns 3 ms) e a entrada dos
lutadores (4767 a 4770). Fora da luta, os 3 quadros de 5 a 10 ms são da carga.

### As provas no WSL [medido]

Serviço `claude-texcache` (`systemd-run` de sistema, `--uid=codex`, `MemoryMax=5G`), script `~/texcache/rodada.sh`, resumo
`~/texcache/provas.resumo` (termina em `FIM`), worktree `~/bt3xbx-texcache` do pacote `tc.bundle`, cada passo pesado dentro do
`flock ~/wsl.lock`; o clone ficou em `f5-pingpong`, sem alterados.

| Prova | Resultado |
|---|---|
| `.text` da x86 plana | `43444ea4d03a6d448aa1ae74a631e35b356a7caa` na main `5021312` e no `bf7f96c` |
| Teste de unidade do `Gs_TexOrigem` (VRAM aleatória, 3.000 texturas, os 13 formatos com os de profundidade, `tbw` 0 a 16): (1) um pixel ou uma cor mudada que muda a decodificação muda o hash; (2) a VRAM girada de `d` blocos, a textura em `tbp + d`: a mesma decodificação e o mesmo hash; (3) blocos de fora mudados: o mesmo hash; (4) o resto do bloco de uma paleta de 16 cores mudado: o mesmo hash | "tudo certo" na x86 e na **PPC de verdade** (`qemu-ppc`): 0 falhas em 2.911 e 2.902 decodificações mudadas, 3.000 giradas, 3.000 e 1.400 de fora. Visto falhando aqui: sem a paleta (290 hashes iguais com a decodificação diferente), o bloco de 4 bits errado (54), o bloco sem `tbp` (2.108 e 2.899 giradas), sem as 16 cores e sem as 256 |
| A conferência de cada reuso (a decodificação refeita no reuso contra a cópia guardada dos pixels da vaga) | **1.737 de 1.737** iguais no `f5_duelo` e **678 de 678** no `f5_treino`, na `x86gv` e na `ppcg`, sem e com as duas threads; a conferência da decodificação por linha, 2.424 e 1.242 (chave 0), 687 e 564 (chave 1) iguais |
| Listas normais (`BT3_GPU_NORMAL=2`), chave 0 contra 1, `x86gv` e `ppcg`, `f5_duelo` (8.380 quadros) e `f5_treino` (7.386), sem e com `BT3_GS_THREAD=1 BT3_GS_DESENHO=1` | os **8 pares** com `divergencias.py --normal` saindo 0; 0 desenhos com textura já solta; o resultado do roteiro igual (vencedor 0x2 razão 0x1 vida 0 27160 no duelo); os hashes de sempre com a chave 0 (`08a98b6d05b8775a` e `f76f0f6849b64629` na `x86gv`, `994627a2aecd429a` e `f76f0f6849b64629` na `ppcg`) |
| As listas cruas, chave 0 contra 1 | diferentes (6.285 quadros no duelo, desde o 2095), como deve: o `gpu_falso` numera as texturas pela criação e hasheia cada envio; com 687 em vez de 2.424, os números e a seção 0 mudam |
| O cache cheio (`BT3_GS_TEX_MAX=256`, a varredura a cada poucos quadros: 5.144 decodificações com a chave 0), chave 0 contra 1, sem e com as threads | normais iguais, e iguais às do cache livre; 2.576 reusos, todos conferidos iguais; 0 desenhos com textura já solta: a contagem de referências solta o recurso só com a última entrada |
| **Sabotagem 1**: a chave sem a paleta (nem as 16 nem as 256 cores) | a conferência acusa **190 reusos diferentes** no duelo e **79** no treino; as listas normais diferem em 4.133 e 3.330 quadros; `divergencias.py` sai 1 |
| **Sabotagem 2**: a contagem de referências ignorada (o recurso solto na saída de qualquer entrada), cache cheio, threads | **760.046 desenhos com textura já solta**, `divergencias.py` sai 1 |
| O git do worktree no fim | 0 alterados (as sabotagens desfeitas e a `x86gv` refeita) |

**Por que a lista normal precisou de `=2`.** Com a chave ligada, duas texturas de mesmo conteúdo viram o mesmo recurso, e dois
desenhos seguidos que só diferiam no número da textura viram um (o `same_state` compara o número): com a primeira versão da chave,
21 quadros do duelo (240 desenhos a menos) e 12 do treino com a lista normal diferente, todos só pelo `drawCount`. É a mesma
imagem: o mesmo estado e os mesmos pixels num desenho só. O `BT3_GPU_NORMAL=2` conta como um só dois desenhos de primitivas
seguidos, com a faixa contígua, que a junção do `gs_draw.c` juntaria se a textura valesse pelo conteúdo, e refaz a seção das
contagens com esse número; com a chave 0 dá o mesmo que `=1` (o resultado das listas da chave 0 não mudou). A sabotagem 1 mostra
que ela continua vendo uma textura errada.

### A estimativa depois de (a) [estimado]

No Blast 2 não sobra decodificação (0 de 65), nem o XG (o reuso não cria recurso): saem uns 33 ms de decodificação e uns 7 ms do
XG, e entra o hash dos blocos, uns 2,5 MB (as 65 texturas em 8 e 4 bits), na faixa de 0,2 a 1 ns por byte no Xenon: de 0,5 a
2,5 ms. Pela estimativa do pedido (as decodificações que sobram vezes 10,4 ns por pixel), o Blast 2 fica em 0 ms de textura e
**a opção (b), a decodificação adiantada numa thread, não é precisa**: o maior quadro de textura da luta passa a ser o 4905, com
3,1 ms. O que sobra no 5338 não é textura: a gravação ocupada tinha 77 a 80 ms com 33 + 7 de textura (seção 25), então uns 37 a
40 ms são outro trabalho no mesmo quadro (o envio dos 3,2 MB ao novo endereço e os hashes das páginas, a cena da técnica). Se a
rodada mostrar o 5338 ainda acima do período, o próximo passo é medir esse resto, não a thread.

### A build do 360

`build_f2.sh` no `bf7f96c` (carimbo `bf7f96c diff:da39a3ee5e`): 317 de 317 fontes sem erro, 985 avisos no total (os mesmos do
`f5-zero`; 2 no `gs_draw`, 0 no `gs_core`), os mesmos **103 stubs**, `default.dat` `fb489b2f6e00d0e4de7e394f7037540c` e
`default.swap` `abbd7589c8425797c4a2639068c9bf54` iguais, **`default.xex` `9e480e206afd114d140cf5ded4630f82`**. O `Gs_TexOrigem` em
`0x8217ce78` no `default.map`; o `tex_solta` entrou em linha. A memória a mais: o `sOrig` (8.192 vagas de 32 bytes) e o balde
(64 KB), uns 320 KB.

### A rodada no console proposta

Os arquivos em `build/x360/f2/` do worktree `bt3xbx-texcache`: `default.xex`/`.dat`/`.swap` do `bf7f96c`, `f5_duelo.txt` e
`boot.txt` (iguais aos de `port/x360/roteiros/`), `ambiente_o0.txt` e `ambiente_o1.txt` (os `ambiente_z1.txt` da seção 25, o jogo
como ele roda com `BT3_TEMPOS=0`, `BT3_PICOS=36` e `BT3_PICOS_TELA=1`, mais `BT3_TEX_ORIGEM=0` e `=1`; saves novos `saves_tc_o0` e
`saves_tc_o1`); o `ambiente.txt` é a cópia do `ambiente_o0.txt`.

```
port/x360/tools/subir_f2.sh build/x360/f2/ambiente.txt build/x360/f2/f5_duelo.txt build/x360/f2/boot.txt \
    build/x360/f2/ambiente_o0.txt build/x360/f2/ambiente_o1.txt
# abertura 1 (chave 0); depois, com o console de volta na FreeStyle:
python3 port/x360/tools/subir.py --baixar bt3xbx.log
python3 port/x360/tools/subir.py --nome ambiente.txt build/x360/f2/ambiente_o1.txt
# abertura 2 (chave 1); baixar o log de novo
python3 port/x360/tools/passos.py bt3xbx.log --console --janelas 2 --minimo 600 --carimbo "bf7f96c diff:da39a3ee5e"
grep 'picos: tela' bt3xbx.log
```

**O que olhar:**

1. **O quadro 5338 (Blast 2)**: na abertura 2, sem linha `picos: tela:` (nenhuma imagem longa nele) e sem a repetição na janela 1
   (`picos: tela, janela:`); na linha `picos:   quadro 5338`, `texturas 0` e o tempo de textura só do hash (de 0,5 a 2,5 ms
   [estimado]), contra 65 texturas e uns 33 ms na abertura 1. E a **gravação ocupada** nele: de 77 a 80 ms para uns 37 a 42
   [estimado]; se ficar acima de 33,4 ms, o resto do quadro é a próxima medida (acima).
2. **O quadro 7017** (e os 7015, 7016): `texturas 0`, a gravação ocupada perto dos vizinhos (17 a 19 ms), e sem linha `tela`.
3. **O quadro 6960**, que não é de textura (seção 25): deve seguir igual nas duas; ele separa o que o cache tira do que é de fase.
4. As imagens de 3 vblanks do combate (`passos: janela 2 tela:` e as linhas `tela`) contra a abertura 1 e as de 23/10; o tempo de
   textura da luta inteira (uns 85 ms na rodada de 22/10) deve cair para uns 20%, mais o hash (3,7 M pixels contra 19,2 M).
5. O resultado da luta igual (vencedor 0x2 razão 0x1 vida 0 27160), os passos por janela (684 e 2.334), nenhum `roteiro: FALHA`,
   `vigia:` ou `CRASH`, e nenhuma linha `gs_d3d: branca` a mais (o descarte com recursos partilhados).

**Recomendação.** Se a rodada confirmar o 5338 e o 7017 sem imagem longa e nada pior no resto, ligar `BT3_TEX_ORIGEM` por padrão
(o mesmo conteúdo provado textura a textura no WSL; o risco que sobra é o descarte por memória do 360, que o WSL não exercita, e
um recurso que só sai com a última entrada fica mais tempo na memória).


## 27. A rodada de 23/10 com a tela por imagem e o cache pela origem [medido]

Análise no branch `codex/f5-picos`, base `troca/f5-texcache` (`07206c9c`). Só os textos já colhidos no console,
sem executar jogo, emulador ou build. Fontes fora do git: `~/sessoes/sessao_2310e/{z1,z2,o1,jogo}/bt3xbx.log`
e `~/sessoes/sessao_2310d/`. A meta continua **nenhuma imagem longa na luta, sem somar atraso de entrada**.
As propostas abaixo substituem a alternativa 3b da seção 25: nenhum vblank de folga.

### A ligação entre troca e quadro e a conferência final

**A guarda da seção 25 passou antes da análise:** nas seis janelas, **0 trocas sem quadro, 0 puladas e 0 sem a
marca da GPU**. Na z1 e na z2, carimbo `dac2466 diff:da39a3ee5e`; na o1, `bf7f96c diff:da39a3ee5e`, com
`BT3_TEX_ORIGEM=1` explícito. As três: `f5_duelo`, 864 x 608 (escala 1,33), som ligado, `BT3_TEMPOS=0`,
`BT3_PICOS=36` e `BT3_PICOS_TELA=1`.

| Rodada / janela | Trocas / repetições | Imagens de fato por vblanks: 1 / 2 / 3 / 4+ | GPU: folga mínima / p1 / p10 / mediana (ms) |
|---|---:|---|---|
| z1 / 1 | 685 / 1 | 0 / 683 / 0 / 1 | 4,7 / 5,4 / 6,1 / 8,4 |
| z2 / 1 | 685 / 1 | 0 / 683 / 0 / 1 | 4,6 / 5,4 / 6,1 / 8,4 |
| o1 / 1 | 684 / 0 | 0 / 684 / 0 / 0 | 4,7 / 5,4 / 6,1 / 8,4 |
| z1 / 2 | 2.334 / 0 | 3 / 2.328 / 3 / 0 | -9,4 / 2,5 / 6,5 / 9,5 |
| z2 / 2 | 2.334 / 0 | 3 / 2.328 / 3 / 0 | -9,3 / 2,6 / 6,5 / 9,5 |
| o1 / 2 | 2.335 / 0 | 3 / 2.330 / 2 / 0 | -8,8 / 3,0 / 6,4 / 8,8 |

A linha antiga `passos: ... tela` da janela 1 conta 685 imagens de 2 na z1 e na z2; a medida por imagem soma a
repetição e encontra a imagem de 4. Na o1, ambas contam 684 de 2. Na janela 2 da o1, o `passos` conta 2.334 imagens
(3 / 2.329 / 2 / 0), uma de 2 a menos que o resumo por imagem. É a fronteira de leitura entre os dois medidores
[sobre os fontes, `tela_apresentou` fecha ao observar a janela encerrada]; não há troca perdida ou sem associação.
Não exigir igualdade exata dessas duas amostras assíncronas.

| Conferência sobre os logs originais | z1 | z2 | o1 |
|---|---|---|---|
| `passos.py --console --janelas 2 --minimo 600`, com o carimbo de cada build | tudo certo, saída 0 | tudo certo, saída 0 | tudo certo, saída 0 |
| Passos, janelas 1 / 2; resultado | 684 / 2.334; vencedor 0x2, razão 0x1, vida 0 / 27160 | igual | igual |
| Parede, razão j1 / j2 | 0,9984 / 0,9972 | 0,9984 / 0,9972 | 0,9972 / 0,9972 |
| Trabalho p99 / máximo, j2 (ms) | 31,54 / 36,57 | 31,38 / 36,51 | 29,58 / 31,11 |
| Apresentação CPU p99 / máximo, j2 (ms) | 35,06 / 50,94 | 34,96 / 50,83 | 36,00 / 43,69 |
| FALHA do roteiro, CRASH, vigia de parada, branca, descartes VU1/nativos/textura recusada | todos 0 | todos 0 | todos 0 |

O resultado vem no vblank 11478, quadro 7757, nas três. O aceite antigo dá “tudo certo” até nas referências com o
Blast 2 escondido pela repetição: **isso não prova zero picos nem a F5 inteira**, e estas janelas são menores que
as três de 2.700 passos do critério completo. A apresentação da CPU acima de 34,37 ms na j2 até aumenta na o1
(46, contra 41/39); a tela melhora. Não decidir pela porcentagem da CPU.

### Todas as imagens diferentes de 2 vblanks no combate (janela 2)

Em cada linha, **A é a imagem que ficou na tela; B é o quadro seguinte**, cujos carimbos e trabalho aparecem.
J/G/P/U são as folgas até o prazo da entrega do jogo, da gravação, do `Present` e do fim da GPU: positivo antes,
negativo depois, em ms. G ocupada e F (`frame_end`) também são ms. Os números abaixo são transcritos, sem estimativa.

**z1**

| Imagem A → quadro B | Vblanks de A | J / G / P / U | G ocupada / F | Texturas de B (ms) |
|---|---:|---|---|---|
| 6911 → 6912 | 1 | 66,3 / 56,7 / 32,1 / 18,8 | 9,6 / 24,6 | 0 (0,0) |
| 6912 → 6913 | 1 | 49,7 / 40,7 / 32,3 / 18,7 | 9,0 / 8,4 | 0 (0,0) |
| 6929 → 6930 | 3 | 32,9 / 18,6 / 11,9 / -2,5 | 14,4 / 6,6 | 0 (0,0) |
| 6930 → 6931 | 1 | 49,6 / 35,3 / 32,2 / 17,7 | 14,4 / 3,2 | 0 (0,0) |
| 6959 → 6960 | 3 | 32,8 / 13,3 / 7,7 / -9,4 | 19,6 / 5,6 | 0 (0,0) |
| 7016 → 7017 | 3 | 49,5 / 18,8 / 10,5 / -0,7 | 30,8 / 8,3 | 15 (10,5) |

**z2**

| Imagem A → quadro B | Vblanks de A | J / G / P / U | G ocupada / F | Texturas de B (ms) |
|---|---:|---|---|---|
| 6911 → 6912 | 1 | 66,3 / 56,8 / 32,1 / 18,8 | 9,5 / 24,7 | 0 (0,0) |
| 6912 → 6913 | 1 | 49,7 / 40,7 / 32,3 / 18,8 | 9,0 / 8,4 | 0 (0,0) |
| 6929 → 6930 | 3 | 32,9 / 18,6 / 11,9 / -2,5 | 14,4 / 6,6 | 0 (0,0) |
| 6930 → 6931 | 1 | 49,6 / 35,4 / 32,2 / 17,8 | 14,2 / 3,2 | 0 (0,0) |
| 6959 → 6960 | 3 | 32,8 / 13,2 / 7,6 / -9,3 | 19,7 / 5,6 | 0 (0,0) |
| 7016 → 7017 | 3 | 49,5 / 18,8 / 10,5 / -0,7 | 30,8 / 8,3 | 15 (10,6) |

**o1**

| Imagem A → quadro B | Vblanks de A | J / G / P / U | G ocupada / F | Texturas de B (ms) |
|---|---:|---|---|---|
| 6911 → 6912 | 1 | 66,3 / 56,9 / 32,1 / 18,8 | 9,4 / 24,9 | 0 (0,0) |
| 6912 → 6913 | 1 | 49,7 / 40,8 / 32,3 / 18,7 | 8,9 / 8,6 | 0 (0,0) |
| 6929 → 6930 | 3 | 32,9 / 18,8 / 12,2 / -2,2 | 14,1 / 6,6 | 0 (0,0) |
| 6930 → 6931 | 1 | 49,6 / 35,7 / 32,2 / 17,8 | 13,9 / 3,5 | 0 (0,0) |
| 6959 → 6960 | 3 | 32,9 / 13,7 / 8,1 / -8,8 | 19,2 / 5,6 | 0 (0,0) |

**Por causa:** textura nova na chave de sempre, 7017 (imagem 7016, só z1/z2); troca de cena com atraso de fase,
6960 (imagem 6959, nas três); fase, 6930 (imagem 6929, nas três); outra, as imagens curtas 6911, 6912 e 6930,
nas três. As duas primeiras curtas adiantam a fase, e a última recupera o atraso do 6930. Elas terminam com a GPU
17,7 a 18,8 ms antes do prazo, sem textura nova na linha da tela. Para 6912/6913/6931 **não há par de linhas
`picos: quadro` e `o que teve`**, pois o filtro é outro; a linha da tela é a única medida individual disponível.
Não atribuir disco, descarte ou uma ação específica a esses quadros. Também não há uma curta imediatamente depois
de cada longa: os histogramas não demonstram pares consecutivos em todos os casos.

Os pares de linhas `picos` disponíveis para as longas dizem:

| Quadro B e causa | Conteúdo, igual nas referências e na o1 quando há linha | G ocupada z1 / z2 / o1; GPU após CPU z1 / z2 / o1 (ms) |
|---|---|---|
| 6930, fase: GPU 2,5 / 2,5 / 2,2 ms tarde | 0 texturas; 172 envios, 2.181 KB; 276 hashes; 2.276 lotes; 213 desenhos; vértices CPU/VU 2.247/17.358, 41.466 índices | 14,4 / 14,4 / 14,1; 14,5 / 14,4 / 14,4 |
| 6960, troca de cena e fase: GPU 9,4 / 9,3 / 8,8 ms tarde | 0 texturas; 245 envios, 2.290 KB; 293 hashes; 3.150 lotes; 1.408 desenhos; vértices CPU/VU 3.270/31.287, 74.841 índices | 19,6 / 19,7 / 19,2; 17,1 / 17,0 / 17,0 |
| 7017, textura: GPU 0,7 / 0,7 ms tarde, só z1/z2 | 15 texturas, 1.032 mil pixels, 10,5/10,6 ms; XG 15, 4.032 KB, 1,4 ms; 170 envios, 1.773 KB; 224 hashes; 1.885 lotes; 1.378 desenhos; vértices CPU/VU 2.796/23.118, 56.376 índices | 30,8 / 30,8 / sem medida; 11,3 / 11,3 / sem medida |

Nesses pares: disco 0, descarte 0, esperas de memória e pelo desenho 0, cova 0, repetidas 0, diário 0 linhas e
escritora 0 ms; memória medida uma vez. No 6930 e no 6960, XG também 0. O período do jogo é 33,3/33,4 ms, com
16 a 19 ms esperando vblank e gravação sem trabalho por 18,5 a 20,1 ms. O jogo entrega antes do prazo; o `Present`
também. **Falta acabar a GPU**, embora a gravação e o desenho possam adiantar esse fim. Os 14 a 17 ms “após CPU”
são a cauda até a marca, não o tempo total da GPU, que já trabalha enquanto a CPU envia comandos.

As sete rodadas anteriores, sem a chave por imagem, dão 5/2/3/2/7/3/4 imagens de 3 na j2
(kbase/kenv/kvu/kambos/x0/x1/x1m). Confirmam a variabilidade, mas não permitem identificar cada quadro pela tela.
Os três atrasos reproduzidos em z1/z2 agora têm associação direta; a antiga hipótese sobre 6960/7017 fica confirmada.

### O cache pela origem e o Blast 2

**Sim, 5338 e 7017 somem das imagens longas na o1.** O primeiro está na janela 1, que também é luta.

| Quadro | z1 / z2 [medido] | o1 [medido] |
|---|---|---|
| 5338, Blast 2 | Imagem 5337 repetida, 4 vblanks; CPU 68,03/67,99 ms; gravação ocupada 78,2/78,4 ms; 65 texturas, 3.215 mil pixels, 33,5 ms; XG 7,3 ms | Nenhuma linha por imagem fora de 2 na j1, 0 repetições; maior intervalo CPU da janela 36,48 ms; **tempo ocupado individual ausente** |
| 7017 | Imagem 7016 por 3 vblanks; gravação ocupada 30,8/30,8 ms, textura 10,5/10,6 ms mais XG 1,4 ms | Ausente das linhas da tela e de `picos`; **tempo ocupado individual ausente** |

No Blast 2: 362 envios, 2.639 KB, 328 hashes, 4.672 lotes, 2.393 desenhos, sem disco nem descarte.
O ganho medido é na tela e no máximo da janela, não uma subtração de gravação antes/depois: `BT3_PICOS=36` filtra
períodos de apresentação/jogo, **não** tempo ocupado. A falta da linha não dá sequer um teto de 36 ms para esse tempo.
Os 37 a 42 ms previstos para 5338 na seção 26 continuam estimativa, não resultado; a subtração simples de textura
mais XG daria 37,4/37,6 ms no 5338 e 18,9/18,8 ms no 7017, mas não mede os hashes nem a mudança do restante do caminho.
As 65 e as 15 criações ausentes na o1 não são observáveis individualmente neste log; o reuso já provado no WSL
na seção 26 sustenta a explicação, sem substituir a medição do console.

**Cuidado com o prazo do Blast 2:** a linha imprime J/G/P/U = 115,8/37,6/21,1/5,5 ms na z1
(115,8/37,5/21,1/5,5 na z2), apesar dos 4 vblanks. Lido no `tela_troca`: o prazo parte da **troca anterior**,
que aqui é a repetição, e não da primeira entrada da imagem 5337. Descontando os dois vblanks já gastos antes
da repetição (~33,27 ms), o `Present` fica ~12,2 ms e a GPU ~27,8 ms **depois do prazo original da imagem**
[calculado]. A contagem de 4 é válida; as folgas positivas e o mínimo positivo da j1 não excluem esse atraso.

**Recomendação: ligar `BT3_TEX_ORIGEM` por padrão**, como próximo passo de implementação, sem mudar código nesta análise.
Tira uma imagem de 4 na j1 e uma de 3 na j2, reproduzidas em ambas as referências. Restam duas de 3 na o1
(0,086% das 2.335 imagens de fato), contra três (0,129%) em cada referência. Não apareceu uma nova imagem longa.
O mínimo de memória livre observado sobe de 93 para 141 MB; no fim, texturas de 123.956 para 81.876 KB,
2.251 para 695 recursos contados. A o1 não exercitou pressão de memória: benefício local confirmado, segurança
em sessão longa ainda apoiada nas provas da seção 26, não demonstrada por esta única abertura.

### A partida livre: o alcance que o roteiro não teve

Build `c86082b diff:da39a3ee5e`, anterior ao cache pela origem e à chave da tela, som ligado, escala 1,33.
`BT3_PICOS` ausente. **`BT3_TEMPOS=1` está ligado por padrão e o init confirma**: esta sessão tem o custo das marcas
por passada, ao contrário de z1/z2/o1. Não comparar seus p99 como efeito exclusivo da maneira de jogar.
Não há nomes ou ids explícitos de personagens e estágios no rastro; as leituras aparecem só como números de arquivo.
As observações do Lucas em `observacoes.txt` descrevem o Shenlong (h8/glow), sem observação sobre estas lutas.

As 19 janelas somam **62.705 passos, 2.093,6 s (34,9 min) de combate medido**, em sete sequências que terminam em
`fim combate`, intercaladas por pausas. A tabela agrupa as janelas até cada fim de combate; não inclui o tempo parado
entre elas. “Tela 3/4+” é a callback antiga por troca, que pode esconder repetições da mesma imagem.

| Janelas | Passos | Tela: 3 / 4+ | Maior intervalo CPU (ms) |
|---|---:|---:|---:|
| 1 a 4 | 8.281 | 65 / 6 | 246,97 |
| 5 a 8 | 8.885 | 42 / 6 | 266,21 |
| 9 a 10 | 8.392 | 47 / 1 | 148,90 |
| 11 a 13 | 11.242 | 31 / 4 | 183,28 |
| 14 a 15 | 10.303 | 26 / 3 | 216,29 |
| 16 a 17 | 8.961 | 37 / 5 | 446,36 |
| 18 a 19 | 6.641 | 9 / 3 | 197,29 |

O período mediano da apresentação é 33,27 ms em todas as janelas; as razões parede/esperado vão de 0,9972 a 1,0040,
todas dentro de 1%. Nas 62.807 trocas contadas: **81 de 1, 62.441 de 2, 257 de 3 e 28 de 4+** (3+ = 0,454%).
Há **81 intervalos CPU acima de 50,05 ms**, até **446,36 ms** na janela 16; trabalho máximo **470,06 ms** nela.
Nas janelas longas, o p99 da apresentação vai de 34,21 a 39,48 ms e o de trabalho de 29,22 a 36,30 ms.
Sem descartes de VU1, nativos ou textura recusada nas 19; sem CRASH, branca ou vigia de parada no log.
Nas janelas longas, o atraso ao acordar p99 é 0,05 ms; mesmo nas curtas chega só a 0,09 ms:
não explica esses centésimos de segundo perdidos.

Duas coisas que o duelo curto não exercitou aparecem:

- **Pressão e reciclagem de texturas durante o combate.** No trecho de gravação com 60 quadros na janela,
  quadros 6903 a 6905 já descartam centenas de texturas por pedido. O total no fim é 482 pressões, 61.526 texturas,
  2.320 MB reciclados; 0 esperas pela GPU no contador de descarte. O mínimo livre é 79 MB, acima do piso de 32:
  é sobretudo o teto de texturas de 128 MB, não evidência de falta de RAM total. O cache pela origem é promissor
  aqui, mas seu efeito neste percurso não foi medido.
- **Leituras síncronas de conteúdo em pleno trecho de luta.** Entre os relatórios de gravação dos quadros 7020
  e 7080, ambos com 60 quadros na janela 2, há leituras de `pzs3us1/01431.bin` de 96,2 ms e `01423.bin` de 73,5 ms,
  mais duas de 6,6 e 10,1 ms. São eventos que o `f5_duelo` fixo não reproduz nesse trecho. Pré-carga de conteúdo
  reutilizável merece estudo separado; o log não liga essas leituras a um pico individual nem identifica uma técnica.

A gravação por família mede médias de 60, com a parede e as esperas misturadas; não dá o quadro responsável.
A GPU e o desenho por passada ficam mudos dentro das janelas. Portanto não atribuir os 446 ms ao descarte ou ao
disco, nem afirmar que o cache já eliminaria todos esses picos. O ritmo médio está preservado; **zero picos em
partida livre não está demonstrado e os 28 de 4+ já demonstram a falha da meta nesse percurso**.

### O que falta cortar, sem aumentar o atraso de entrada

O déficit medido na o1 é **2,2 ms no 6930 e 8,8 ms no 6960**. Nas referências, os máximos respectivos são 2,5 e 9,4.
O `Present` do 6960 ainda tem 8,1 ms de folga, mas a cauda GPU leva ~16,9 ms até o fim: só adiantar a chamada ao
`Present` ou mudar o limiar do log não basta. O jogo não pede espera adicional: entrega a ~32,9 ms do prazo.

| Trecho da cadeia, o1 | 6930 [medido] | 6960 [medido] | Exemplo de orçamento para 6960 [estimado] |
|---|---:|---:|---:|
| Gravação ocupada (hardware 0) | 14,1 ms | 19,2 ms | 14,5 ms: cortar 4,7 |
| Desenho, `frame_end` (hardware 3) | 6,6 ms | 5,6 ms | 4,0 ms: cortar 1,6 |
| Cauda entre `Present` e fim GPU | ~14,4 ms | ~16,9 ms | 13,0 ms: cortar 3,9 |

A cadeia do 6960 soma aproximadamente 41,7 ms desde a entrega do jogo até a marca, para 32,9 ms disponíveis.
O orçamento de 31,5 ms daria ~1,4 ms de margem, **se os cortes adiantarem de fato as fronteiras**. Para cobrir o
pior das referências com 1 ms de margem, buscar ao menos **10,4 ms** na cadeia. O trabalho do jogo, tiradas suas
esperas, fica ~14 a 17 ms nesses quadros; não há déficit medido nele a resolver primeiro. As três threads e a GPU
se sobrepõem: as parcelas de CPU são trabalho, a cauda GPU é diferença de carimbos, e ganhos isolados não se
somam automaticamente quando mudam as filas. Estes valores são orçamento, não promessa de ganho.

| Prioridade / lado | Corte a investigar, preservando conteúdo e ordem | Base observada e ganho proposto [estimado] |
|---|---|---|
| 1, gravação | Evitar releitura/hash de páginas por geração de escrita e origem do envio; invalidar em todo caminho que escreve. Depois, evitar cópia V4-32 em lotes reconhecidos quando a vida dos dados permitir | Na x1m, média dos 60 até 6960: envio 5,10 ms, hash 1,93, Unpack 1,01; alvos de 2 a 3 ms em envio/hash e 0,5 a 1 em Unpack. São médias de outra build, não rateio do 6960 |
| 2, gravação | No estado e no fecho da VU1, reduzir comparações e montagem ainda repetidas entre lotes, além das memórias já implantadas | No mesmo trecho: VU1 5,10 ms, estado 2,48, fecho 0,83; buscar 0,5 a 1,5 ms adicionais. Não contar de novo o cache de estado, os índices em locais, envio rápido ou VU1 direto já presentes |
| 3, desenho | Juntar apenas desenhos contíguos de estado e constantes equivalentes e reduzir invalidações completas ao voltar das passadas nativas; o cache `sCur*` já existe | `frame_end` 5,6/6,6 ms nos quadros alvo; buscar 1 a 2 ms. Medir por chamada antes: o log sem marcas não reparte esse tempo, e há só 213 desenhos no 6930 |
| 4, GPU | Reduzir área de passadas/Resolves pela área realmente lida e escrita, e especializar mais os shaders comuns nos estados usados. Respeitar alfa, DATE, FBA e dependências entre alvos | Cauda 14,4/16,9 ms, não GPU total. Buscar 3 a 5 ms na cadeia. As passadas nativas, área DATE e residência em EDRAM já estão ativas; não somar seus ganhos antigos outra vez |

Os três primeiros cortes têm soma de metas de ~4 a 7,5 ms de CPU; precisam render na cadeia, junto à redução GPU,
para cobrir o 6960 com margem. No 6930, um corte real de ~3,5 ms já cobriria a pior referência com 1 ms de margem.
Nenhuma dessas estimativas foi medida nesta tarefa. Para a GPU, a amostra da partida livre no quadro 97979,
**fora da janela 19**, dá apenas uma orientação: comum 7,64 ms, FBA 1,39, resolves de cor 1,12, FXAA 0,76,
com 2,53 ms das próprias marcas em 20,35 ms totais. Não transportar esse rateio para o 6960; desligar FXAA
sozinho nem alcançaria 1 ms nesse exemplo, além de mudar a imagem. Baixar a escala tampouco é prova de um corte
com a mesma qualidade. O próximo perfil deve cobrir os quadros alvo, sem cobrar marcas por desenho em toda a luta.

A ordem recomendada é o cache por padrão, os cortes medidos por quadro na cadeia e, em paralelo ao diagnóstico,
a cobertura de memória/disco da sessão longa. Não há justificativa para adiantar uma thread de decodificação de
textura só pelo Blast 2: a imagem longa dele já saiu com o cache. A aceitação final continua sendo a tela por
imagem sem repetição escondida e a latência de entrada preservada, não só p99 médio ou cada CPU abaixo de 33 ms.

### Reprodutibilidade e limites da entrega

A extração fica em `~/f5-picos/confere.py`, saída `imagens.json`; confere associação, marcas, conservação de
histogramas e todas as 17 linhas da janela 2 (6/6/5), além das duas da janela 1. Foi vista falhando com saída 1
em quatro cópias estragadas da z1: uma troca sem quadro, uma linha retirada, duração 3 trocada por 2 e formato
do carimbo GPU desconhecido. Os originais dão “tudo certo”. O `passos.py` existente foi reaplicado com os comandos
da seção 25/26, sem nova execução do jogo. Artefatos de análise em `~/f5-picos/`, relatório em `~/entrega-f5-picos.md`.
Só documentação muda neste branch; nenhum dado do jogo entra no git.

A `.text` do binário x86 preservado da prova do cache (`~/bt3xbx-texcache/port/build/x86/bt3_x86`), extraída novamente
por `objcopy`, tem SHA1 **`43444ea4d03a6d448aa1ae74a631e35b356a7caa`**, a referência da seção 26. Não é build nova
desta análise. O binário antigo do clone principal tem outro SHA1 (`ee54716ba7d86744152b9b71e6ac8e500f985d97`),
registrado sem tratá-lo como referência; ambos ficam intocados. Sem fontes ou objetos alterados, não houve build,
comparação nova de objetos, roteiro, save, emulador ou acesso ao console. Os ganhos futuros, o tempo individual
de gravação após o cache e sua sessão longa no 360 continuam sem prova nesta entrega de análise.

### CLUT16: as 16 cores na chave de sempre (27/10, `codex/f5-clut16`)

O `clut_hash` lia `(i & 15, i >> 4)` também com 16 entradas: lia x=8 a 15 da primeira linha, deixando fora a segunda
linha, as cores 8 a 15. Em CSM1 a CLUT16 é 8 x 2 em `CBP`, com o formato `CPSM`; `CSA` seleciona a posição no buffer
interno do GS, não desloca essas entradas na VRAM. O leitor e a decodificação atuais usam esse caminho CSM1, não implementam
CSM2; a correção acompanha suas 16 leituras `(i & 7, i >> 3)`. As 256 entradas e a memória por gerações ficam como antes.
Pela regra do upstream, a mudança fica no ramo `PORT_BIG_ENDIAN`, também exercitado pela `BT3_TESTE_VU1` da x86gv, com a
linha original no `#else`.

**Antes de corrigir [medido].** `clut16.py diagnostico` numa cópia: compara as 16 palavras exatas antes dos acertos do
cache, por `(TEX0, TEXA, gen)`, e conta transições com a mesma chave e só a metade alta diferente. A memória por época só
pula consultas à mesma VRAM; a tabela cheia é erro explícito. Na x86gv, `f5_duelo`, `f5_treino` e `shenlong` deram **0
colisões**, em 560.547, 430.109 e 239.176 leituras de paleta; nenhum CSM2. Não se espera mudança de imagem nesses roteiros.

**Prova sintética [medido].** `teste_clut16.sh`, x86 de 32 bits e PPC no qemu: 288 mudanças individuais nas 16 cores,
4.608 nas 256 cores, três CPSM (0, 2, 10), seis CBP incluindo limites de página e a volta da VRAM; chave corrigida com 0
falhas. Restaurar a chave antiga numa cópia acusa 144 colisões, todas nas cores 8 a 15, e o teste sai 1. O verificador do
log também recusa o resumo ausente e a contagem adulterada.

**Listas na x86gv [medido, `662a69b7`].** `divergencias.py --normal`: 0 quadros diferentes nos 8.380 do duelo, 7.386 do
treino e 5.818 do Shenlong. A correção também deixa de hashear cores de fora da paleta: as decodificações caíram de
2.431 para 2.388, de 1.248 para 1.157 e de 531 para 519. Por isso a lista crua e o acumulado chamado de "hash do jogo"
no `gpu_falso` mudam, apesar da lista normal igual. A `.text` plana do mesmo commit deu o SHA-1 exigido abaixo.

**Rodada final preparada.** `clut16_rodada.sh COMMIT BASE`, serviço `bt3-clut16`, resumo `~/f5-clut16/provas.resumo`,
terminando em `FIM`. Recompila base e commit final, compara objetos sem depuração e a `.text` plana com
`43444ea4d03a6d448aa1ae74a631e35b356a7caa`, compara as listas normais dos três roteiros na x86gv e ppcg com
`divergencias.py --normal`, e repete o diagnóstico e seus controles. A conclusão depende de `tudo certo` no resumo;
resultados ainda não conferidos nesta nota. Sem console.

## 28. O restante da gravação, os cortes de constantes e fecho e o perfil pontual da GPU (27/10/2026)

Branch `codex/f5-cortes`, base `1003b9f5` (`troca/codex/f5-picos`). Sem console nesta tarefa. A meta continua sendo
nenhum pico na luta sem acrescentar vblank nem atraso de entrada. Convenção: **[medido]**, **[lido]**, **[estimado]**.

### Medir antes de cortar

`cc4037a1`: `BT3_CORTES=A,B`, desligado por padrão, mede cada quadro de A até B, inclusive. `=1` escolhe 6900 a 7025.
A pilha divide exclusivamente DMA, VIF, GIF, desenhos de CPU, VU1, Unpack, envio, entrega, hash, constantes e fecho.
O GIF chamado pela VU1 entra no GIF, e o envio dele só no envio. A soma conserva o total; não se somam famílias
inclusivas à pilha. Só a thread de gravação escreve os contadores. A medição começa no `run_chain`, não inclui o
intervalo sem trabalho entre duas listas, e termina depois da entrega, antes da captura de software.

**[Medido, x86gv]** Duelo com `BT3_TEX_ORIGEM=1`, sem threads, `BT3_TEMPOS=0`, chaves dos cortes ainda ausentes.
`~/f5-cortes/diag-duelo.log`: 3.088 quadros, 4670 a 7757, soma conservada em todos por `cortes.py`; roteiro completo,
8.380 quadros. Tempos em ms, com as marcas por chamada:

| Quadro | Total | DMA | VIF | GIF | CPU | VU1 restante | Unpack | Envio | Entrega | Hash | Constantes | Fecho |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 6930 | 1,437 | 0,064 | 0,219 | 0,109 | 0,127 | 0,193 | 0,106 | 0,169 | 0,195 | 0,179 | 0,031 | 0,044 |
| 6960 | 2,832 | 0,089 | 0,349 | 0,206 | 0,162 | 0,512 | 0,240 | 0,200 | 0,695 | 0,184 | 0,086 | 0,109 |
| Mediana do combate | 4,635 | 0,137 | 0,567 | 0,285 | 0,152 | 0,897 | 0,407 | 0,285 | 1,240 | 0,260 | 0,155 | 0,193 |
| 4976, maior total | 6,481 | 0,149 | 0,630 | 0,461 | 0,146 | 2,185 | 0,453 | 0,436 | 1,352 | 0,276 | 0,170 | 0,224 |

O antigo resto tem trabalho identificável: no 6960, DMA/VIF/GIF/CPU somam **0,806 ms**; no 6930, **0,520 ms**.
Isso não transfere os cerca de 5 ms do Xenon diretamente para essas parcelas. O diagnóstico cobra relógios por chamada,
e a entrega inclui o hash e a leitura das listas pelo backend falso, sem rasterização. Com as threads ela também pode
esperar o desenho. A VU1 restante exclui constantes, fecho e quaisquer filhos medidos. Não é o número inclusivo da família
VU1 antiga. O DMA inclui a contagem dos bytes de teste e o `GsVu1_FrameEnd`.

**[Preparado, 360]** `BT3_CORTES_GPU=A,B` seleciona os quadros mesmo em `Passos_Medindo()`, independentemente de
`BT3_TEMPOS`; `=1` seleciona só 6960. Guarda o número emitido com cada buffer e drena os buffers fora da faixa.
Linhas `cortes: gpu quadro Q` mostram total, custo das marcas e, por classe, tempo bruto, líquido estimado, limite
da espera pela CPU e número de marcas. `cortes: cpu quadro Q` mostra o `frame_end` e as classes, com preparo,
plano da EDRAM e limpeza separados. O custo descontado é o trecho vazio do mesmo quadro, como no perfil existente.
As marcas esvaziam o pipeline: o perfil identifica passadas, mas não certifica fase nem ganho de apresentação.
Nenhum tempo individual de GPU do 6960 foi medido aqui; nenhum corte de shader, Resolve ou área foi aplicado sem ele.

### Os cortes, desligados por padrão

| Commit | Chave | Mudança [lido] |
|---|---|---|
| `5c240eaf` | `BT3_VU_FECHO=1` | A igualdade binária do uniforme passa de uma palavra por volta a quatro palavras com XOR/OR; conserva cada bit, inclusive NaNs e sinais de zero, e exige só alinhamento de quatro bytes |
| `5459444e` | `BT3_VU_CONSTANTES=1` | Nos programas 4 e 6, zera somente os campos que não recebem valor logo depois: elimina 76 e 152 bytes de escritas redundantes por bloco, respectivamente; o programa 0 fica com a montagem anterior |
| `ab252e4d` | `BT3_VU_CONSTANTES=2` | Acrescenta ao modo 1 a memória do uniforme do cenário: a chave compara os 64 bytes da matriz, XYOFFSET e ZBUF.PSM. Em acerto evita a montagem e as três conversões; o fecho copia a vaga estática ao abrir um desenho |
| `852d05c7` | Constantes 2 e `BT3_VU_FECHO=1` | A igualdade da cópia anterior é conhecida por endereço, quadro e versão; neste caso evita reler os 304 bytes. Qualquer outra origem ou versão volta à comparação binária |

A ordem dos desenhos, os índices, os uniformes completos e os estados continuam os mesmos. Não há nova espera,
fila ou recurso. As chaves não mudam padrões. O código novo entra no ramo da PPC e no teste explícito `--vu1`,
com os corpos originais da x86 plana preservados.

### Provas locais e alcance

**[Medido]** `teste_cortes.py` extrai o código real do rateio e do perfil de GPU e testa relógios e dispositivo simulados.
Passa, e acusa três mutantes: soma perdida, quadro leitor no lugar do emitido e ausência do dreno. O leitor do rateio
recusa, com saída 1, cópias com total adulterado, quadro alvo ausente e fim do roteiro retirado; o teste também tira uma parcela.

`teste_vu_cortes.sh`, x86 de 32 bits e PPC real no qemu: **512 blocos de constantes iguais byte a byte**, começando com todos
os 256 bytes de preenchimento possíveis nas duas famílias; **2.926 diferenças de palavras acusadas**, inclusive comprimentos
não múltiplos de quatro e origem deslocada quatro bytes. Os mutantes que aceitam tudo no fecho ou esquecem o `light2` falham
nas duas arquiteturas. A memória do cenário acrescenta 256 sequências com mudanças de matriz, XY e formato Z e 256
cópias adulteradas recusadas pelo oráculo completo; as chaves sem XY ou sem matriz também falham na x86 e na PPC. Os testes não dependem de um bloco estático inicialmente zerado.

**[Medido]** `~/f5-cortes/local.log`: as 16 comparações `divergencias.py --normal` da `x86gv` passaram, quatro combinações
00/10/01/11 nos dois roteiros e nas duas configurações de threads, contra o binário do diagnóstico anterior aos cortes.
A memória do cenário passou em mais 12 comparações locais (00/20/21, nos mesmos quatro casos). O último resumo do Duelo
tem 4.096.367 acertos e 3.633 montagens novas conferidas, nenhum diferente; no Treino, 1.097.255 e 2.745, respectivamente.
Cada roteiro acabou com os mesmos 8.380/7.386 quadros e sem `FALHA`, `CRASH` ou `race:`. Isso é prova local de desenvolvimento;
a matriz do commit final contra a base da tarefa, incluindo `ppcg`, fica na rodada abaixo.

### Ganho medido e estimativa separada

**[Medido, x86gv]** Em `~/f5-cortes/`, as séries `rateio-*`, `cache-rateio-*` e `identidade-rateio-*` conservam os
3.088 quadros do combate. As parcelas abaixo são medianas em ms, com marcas, comparadas dentro de cada série:

| Mudança | Parcela antes | Depois | Ganho na parcela |
|---|---:|---:|---:|
| Limpeza parcial, 00 contra 10 | constantes 0,1587 | 0,1536 | 0,0051 |
| Comparação em grupos, 00 contra 01 | fecho 0,1969 | 0,1800 | 0,0169 |
| Memória do cenário, série final 00 contra 20 | constantes 0,1572 | 0,1563 | 0,0009, abaixo da variação observada |
| Combinação final, 00 contra 21 | fecho 0,1957 | 0,1793 | 0,0164; não separa ganho adicional da identidade |

Na série final, sem marcas finas e com `BT3_GPU_LISTA`/`BT3_GPU_NORMAL` **ausentes**, três repetições do Duelo completo:
CPU mediana 20,56 s (00), 20,57 (20), 20,49 (21); parede 20,21 / 20,21 / 21,22 s. A economia de CPU da combinação é
**0,07 s, cerca de 0,34%**, aproximadamente 0,0084 ms por quadro do roteiro inteiro; a parede piora nessa amostra.
Não chamar isso de melhora do ritmo. As parcelas vêm de outra corrida, com marcas, não de uma subtração dessa CPU.
No 6930, fecho 0,0445 para 0,0384 ms, total 1,429 para 1,473; no 6960, fecho 0,1064 para 0,1027, total 2,757 para 2,767.
**Não há redução demonstrada do total dos quadros alvo na x86gv**, nem ganho isolado da memória ou da identidade no roteiro.

**[Estimado, Xenon, sem crédito no orçamento até o console]** Grupos do fecho: 0 a 0,15 ms; limpeza parcial: 0 a 0,10;
memória da VU4: 0 a 0,30; identidade: 0 a 0,25. As faixas são hipóteses, não somáveis: a memória tira conversões que a x86
faz barato e o atalho tira leituras do mesmo fecho que os grupos já encurtam. A combinação tem hipótese de 0 a 0,5 ms,
muito aquém dos 10,4 ms de corte com margem propostos para o 6960. Por isso todos continuam desligados. A decisão sobre
GPU/Resolves depende do novo perfil no console; nenhuma passagem adicional ao vblank foi introduzida.

**Provas finais curtas [medido]:** mais quatro comparações da identidade e duas da sua instrumentação passaram, totalizando
34 pares locais normais da x86gv. O contador do atalho conferiu o conteúdo completo em pelo menos **3.900.000 igualdades no
Duelo e 1.100.000 no Treino**, sem divergência. O teste extrai o oráculo real do fecho e o vê abortar com uma cópia adulterada.
Os mutantes sem quadro ou versão falham na x86 e na PPC; no jogo, a chave que aceita qualquer matriz foi acusada no quadro
4137, saída 134. A árvore foi restaurada e recompilada depois da sabotagem.

A `.text` plana recompilada após os cortes tem SHA1 **`43444ea4d03a6d448aa1ae74a631e35b356a7caa`**. Os objetos sem depuração
(`objcopy --strip-debug`, `cmp`) são iguais aos da base preservada do cache, salvo `x360_passos.o`, que contém o carimbo de teste.
O pré-processado plano dos três fontes continua igual ao anterior à memória; a explicitação das guardas em `ac219da3` também
conservou os nove pré-processados (três fontes, três configurações). Logs em `plana-final.log` e `guardas.log`.

### Rodada preparada no console

`cortes_console.py ~/f5-cortes/console --rodada cortes27` preparou os nove ambientes e os dois roteiros, sem build do 360
nem acesso ao console. Na revisão Linux, compilar o commit final e preparar novamente os ambientes ao lado da build.
Para repetir a rodada, escolher outro identificador, pois cada abertura precisa de saves vazios.

- `base`, `constantes`, `fecho`, `parcial_fecho`, `cache`, `ambos`: mesmos padrões, som, 16:9, escala 1,33, FXAA, envio por linhas, VU1 direto e
  cache pela origem; `BT3_PICOS=36`, `BT3_PICOS_TELA=1`, sem marcas finas. Variam só os modos dos cortes e a pasta dos saves. `ambos` usa constantes 2 e fecho 1.
- `gravacao`: cortes desligados, `BT3_CORTES=6900,7025`. Completa o rateio do resto no Xenon, sem marcas finas de GPU.
- `gpu6930` e `gpu6960`: cortes desligados, sem rateio fino da gravação, marcas só em 6929 a 6931 e 6959 a 6961.

Olhar as **imagens de 3 vblanks na janela 2**, sobretudo 6929 e 6959, e a folga da GPU até o prazo, mínimo/p1/p10/mediana.
Comparar gravação ocupada e `frame_end` por pares de quadros, com mesmos envios e sem texturas novas; não usar só p99 da CPU.
O resumo da tela deve manter perto de zero trocas sem quadro, sem repetição escondida. Conferir 684/2.334 passos por janela,
vencedor 0x2, razão 0x1, vida 0/27160, sem queda, vigia ou descartes novos. Aplicar `passos.py --console --janelas 2 --minimo 600`
com o carimbo da build efetivamente produzida. Esses dois trechos não substituem o critério completo de três janelas longas.

### Rodada final longa

`cortes_rodada.sh FINAL 1003b9f5` cria worktrees próprios em `~/f5-cortes/final/{base,final}`, compila as três builds em cada
lado e compara objetos sem depuração e SHA1 da `.text`. Faz 48 comparações de listas normais contra a base: seis combinações de chaves,
dois roteiros, duas configurações de threads, duas arquiteturas. Confere também a instrumentação contra a lista limpa,
as sabotagens, a medição sem listas, os roteiros oficiais com `SEMMAPA=1 IMG=1` e o `save_mc.sh` do commit final.
O serviço de usuário `bt3-cortes` recebe `MemoryMax=5G`; toda build e emulador usa `flock ~/wsl.lock`. O resumo em
`~/f5-cortes/final/provas.resumo` termina em `FIM`, com `tudo certo` ou `FALHA RC=...` antes dele. Foi visto recusar uma árvore
já existente e terminar o resumo de falha com `FIM`. A falha nunca é registrada como sucesso. A conferência adicional do save exige estados e dados não vazios, pois o script antigo só imprime diferenças. O serviço pode morrer se a sessão
WSL cair, conforme o guia. A tarefa pede dispará-lo e parar; a certificação longa continua pendente até ler seu resultado.

## 29. A rodada dos cortes de 27/10: duas imagens longas e a cadeia até o prazo

Branch `codex/f5-cortes2`, base `troca/main` `ccdd3b01`. **[Medido]** Análise dos cinco logs do Lucas em
`~/sessao_2710/cortes/{base,ambos,gpu6930,gpu6960,gravacao}/bt3xbx.log`, uma abertura de cada, saves novos
`sv_ct2710_NOME`. Todos os cálculos param na primeira linha `roteiro: f5_duelo.txt:99 fim`; sobretudo, os quadros
até 24059 da continuação de `gpu6930` não entram. Sem acesso ao console, alteração de código ou execução do jogo.
Convenção: **[medido]** para os logs e as contas sobre eles, **[lido]** para o código, **[estimado]** para ganhos futuros.

A build foi informada como main `16a29b9`, com o código do 360 da base desta tarefa. O carimbo **literal** nas cinco
é `ba63dab diff:4bcb7b67df`; conservá-lo na evidência, sem chamá-lo de build limpa `16a29b9` ou `ccdd3b01`.
Os ambientes registrados confirmam constantes/fecho 0/0 na base e nos diagnósticos, 2/1 em `ambos`, som, escala
1,33, FXAA, cache pela origem, envio rápido, VU1 direto, duas threads e `BT3_TEMPOS=0`. Os opcionais
`constantes`, `fecho`, `parcial_fecho` e `cache` não rodaram: nenhum resultado isolado é inferido deles.

### As imagens na luta [medido]

As duas janelas são combate: 684 passos até a pausa e 2.334 da retomada ao fim. Nas dez janelas: ritmo do roteiro
exatamente dois vblanks por passo, razão parede/esperado 0,9972, descartes requeridos zero. Resultado nas cinco:
vencedor 0x2, razão 0x1, vida 0/27160, vblank 11478, quadro 7757; fim do roteiro no quadro 8378. Até esse fim,
nenhum `FALHA`, `CRASH`, `ERRO` ou `vigia:`. Nos resumos por imagem: zero repetições, trocas sem quadro, puladas e
marcas GPU ausentes. A associação entre imagem, quadro seguinte e prazo vale nesta rodada.

| Abertura | Janela 1, 3 / 4+ | Janela 2, tela do passos: 1 / 2 / 3 / 4+ | Imagens longas A → quadro atrasado B, janela 2 | Folga GPU j2: mínimo / p1 / p10 / mediana (ms) |
|---|---|---|---|---|
| base | 0 / 0 | 3 / 2329 / 2 / 0 | 6929 → 6930; 6959 → 6960 | -9,6 / 2,5 / 6,3 / 8,5 |
| ambos | 0 / 0 | 3 / 2329 / 2 / 0 | 6929 → 6930; 6959 → 6960 | -9,2 / 2,8 / 6,4 / 8,6 |
| gpu6930 | 0 / 0 | 3 / 2329 / 2 / 0 | 6928 → 6929; 6959 → 6960 | -9,6 / 2,5 / 6,3 / 8,5 |
| gpu6960 | 0 / 0 | 3 / 2329 / 2 / 0 | 6929 → 6930; 6958 → 6959 | -2,5 / 2,5 / 6,3 / 8,5 |
| gravacao | 0 / 0 | 3 / 2328 / 3 / 0 | 6929 → 6930; 6959 → 6960; 6989 → 6990 | -11,5 / 1,5 / 6,5 / 9,5 |

Todas as **11 imagens longas duram três vblanks**; nenhuma quatro ou mais. A janela 1 tem 684 imagens de dois nas
cinco, incluindo o Blast 2. Na janela 2, a contagem por imagem soma uma imagem de dois a mais que o passos nas
quatro primeiras (2.335 trocas, histograma 3/2330/2/0); em `gravacao`, ambos contam 2.334. É a mesma fronteira
assíncrona da seção 27, sem associação perdida. Não somar as duas contagens como amostras independentes.

A tabela seguinte contém **cada** imagem longa. J/G/P/U são as folgas até o prazo da entrega do jogo, da gravação,
do Present e do fim GPU; negativo significa atraso. G ocupada e F (`frame_end`) são trabalho/intervalo da CPU,
não GPU total. Em todas as linhas: **zero texturas novas e 0,0 ms de decodificação**.

| Abertura | A → B | J / G / P / U (ms) | G ocupada / F (ms) |
|---|---|---|---|
| base | 6929 → 6930 | 32,9 / 18,4 / 11,8 / -2,6 | 14,5 / 6,6 |
| base | 6959 → 6960 | 32,8 / 13,0 / 7,4 / -9,6 | 19,9 / 5,5 |
| ambos | 6929 → 6930 | 32,9 / 18,8 / 12,2 / -2,2 | 14,1 / 6,7 |
| ambos | 6959 → 6960 | 32,8 / 13,4 / 7,8 / -9,2 | 19,5 / 5,5 |
| gpu6930 | 6928 → 6929 | 32,9 / 18,3 / 15,9 / -0,9 | 14,6 / 2,4 |
| gpu6930 | 6959 → 6960 | 32,8 / 13,0 / 7,5 / -9,6 | 19,9 / 5,5 |
| gpu6960 | 6929 → 6930 | 32,9 / 18,5 / 11,8 / -2,5 | 14,5 / 6,7 |
| gpu6960 | 6958 → 6959 | 33,0 / 19,2 / 16,4 / -0,4 | 13,8 / 2,8 |
| gravacao | 6929 → 6930 | 32,9 / 17,4 / 11,0 / -3,5 | 15,7 / 5,6 |
| gravacao | 6959 → 6960 | 32,8 / 11,1 / 5,6 / -11,5 | 21,9 / 5,5 |
| gravacao | 6989 → 6990 | 49,4 / 26,8 / 15,4 / -0,8 | 22,8 / 11,4 |

As linhas `picos: quadro B` e `o que teve` completam a leitura quando presentes:

| B | Conteúdo nas cinco aberturas | GPU depois da CPU / período GPU, base (ms) |
|---|---|---|
| 6930 | 172 envios/2181 KB, 276 hashes recalculados, 2276 lotes, 213 desenhos; 2247 vértices CPU, 17358 VU, 41466 índices | 14,4 / 38,2 |
| 6960 | 245 envios/2290 KB, 293 hashes recalculados, 3150 lotes, 1408 desenhos; 3270 vértices CPU, 31287 VU, 74841 índices | 17,0 / 45,5 |
| 6990 | 245 envios/2308 KB, 296 hashes recalculados, 3238 lotes, 1412 desenhos; 2937 vértices CPU, 31841 VU, 76059 índices | 17,6 / 38,2 |

Nesses três B, todas têm disco, XG, descarte, espera por memória/desenho e cova zero; texturas também zero.
O período do jogo fica em 33,3/33,4 ms. Em `gravacao`, 6990 tem gravação 22,8 e F 11,4 ms, contra 20,9 e 10,0 na
base; a cauda GPU é 16,2 contra 17,6. A escritora do diário trabalha 21,5 ms em paralelo nele e 26,1 no 6930;
isso não é espera medida da thread do jogo nem prova de causalidade exclusiva. A base já tem um pico CPU no
6990, mas sua imagem não é longa. Nos novos B 6929 (`gpu6930`) e 6959 (`gpu6960`), **não há** par individual
`picos: quadro/o que teve`; a linha da tela e o perfil pontual são a evidência disponível, sem atribuir disco ou
hash individual por ausência de uma linha filtrada.

**Base contra ambos:** mesmas duas imagens longas. G ocupada cai 0,4 ms em cada alvo, o Present adianta 0,4 e a GPU
termina 0,4 ms mais cedo em relação ao prazo; o déficit do 6960 continua 9,2 ms. Trabalho j2 p50/p99/máximo:
24,84/30,47/32,03 para 24,43/29,90/31,58 ms. A combinação tem sinal favorável na CPU desta amostra, mas **uma
abertura por condição não isola o ganho nem justifica ligar as chaves por padrão**. A apresentação não melhorou.

**As marcas alteram o experimento:** GPU mantém duas longas, deslocando a longa para o quadro anterior ao alvo
(A 6928 ou 6958); os alvos 6930/6960 deixam de ser os B longos nessas aberturas. Uma imagem anterior que dura
três muda a fase do prazo seguinte. Não creditar essa mudança a uma otimização, nem concluir que o 6960 já cabe
pela ausência de sua linha longa em `gpu6960`. O rateio da gravação aumenta a contagem de duas para três, acrescenta
6989 e piora os atrasos dos alvos em 0,9/1,9 ms. As diferenças de G ocupada contra base são +1,2/+2,0 ms;
são o efeito observado da abertura instrumentada, não uma calibração universal das marcas.

### A GPU no Xenos [medido]

Bruto e custo são impressos pelo diagnóstico. O líquido é **estimativa calibrada**: bruto menos um trecho vazio
por segmento; inclui o efeito residual de esvaziar o pipeline. Não é uma segunda corrida sem marcas.

| Q | Bruto (ms) | Marcas | Custo (ms) | Líquido (ms) | Limite de espera CPU (ms) |
|---|---|---|---|---|---|
| 6929 | 18,699 | 163 | 2,498 | 16,201 | 0,000 |
| 6930 | 18,356 | 163 | 2,692 | 15,664 | 0,000 |
| 6931 | 18,429 | 163 | 2,679 | 15,750 | 0,000 |
| 6959 | 19,005 | 155 | 2,476 | 16,528 | 0,000 |
| 6960 | 25,983 | 214 | 3,421 | 22,562 | 0,000 |
| 6961 | 26,024 | 214 | 3,587 | 22,437 | 0,000 |

| Classe, líquido estimado (ms) | 6929 | 6930 | 6931 | 6959 | 6960 | 6961 |
|---|---|---|---|---|---|---|
| comum | 9,008 | 8,719 | 8,841 | 8,538 | 12,123 | 12,158 |
| vu0 | 0,100 | 0,095 | 0,097 | 0,082 | 0,826 | 0,818 |
| vu4 | 1,910 | 1,761 | 1,773 | 2,790 | 2,809 | 2,785 |
| profundidade | 0,412 | 0,444 | 0,416 | 0,409 | 0,453 | 0,418 |
| contorno | 0,000 | 0,000 | 0,000 | 0,000 | 0,692 | 0,692 |
| tabela pelos ids | 0,000 | 0,000 | 0,000 | 0,000 | 0,404 | 0,401 |
| tabela pela profundidade | 0,998 | 0,995 | 0,995 | 0,996 | 0,996 | 0,994 |
| cópia dos ids | 0,000 | 0,000 | 0,000 | 0,000 | 0,154 | 0,154 |
| FBA | 1,111 | 1,060 | 1,059 | 1,109 | 1,115 | 1,096 |
| DATE | 0,228 | 0,214 | 0,214 | 0,219 | 0,223 | 0,212 |
| resolves de cor | 0,859 | 0,832 | 0,830 | 0,840 | 1,166 | 1,160 |
| entradas na EDRAM | 0,168 | 0,123 | 0,144 | 0,154 | 0,175 | 0,160 |
| volta da cor | 0,287 | 0,311 | 0,281 | 0,283 | 0,314 | 0,279 |
| tela | 0,324 | 0,326 | 0,324 | 0,324 | 0,324 | 0,326 |
| FXAA | 0,793 | 0,780 | 0,775 | 0,783 | 0,787 | 0,783 |

`vu6`, filme e medição líquidos são zero nas seis; outros fica entre 0,001 e 0,003 ms.

**[Lido]** `tempo_marca` insere callbacks `IDLE`; `tempo_poe` só marca a mudança de classe. As 44/64 marcas em
`comum` dos quadros 6930/6960 são segmentos entre classes, **não** número de desenhos. `desenha_comum` classifica
por `d->vu`, e FBA tem classe própria. O nome `comum` não identifica textura, shader, alvo ou área individual.
`tempo_le` limita a espera CPU pela comparação dos carimbos e estima o custo pelo trecho vazio do próprio quadro.
O limite zero nas seis não exclui bloqueio da CPU nas APIs, dependências internas da GPU ou efeitos das marcas.

**[Medido]** `comum` pesa 51,7% a 56,1% do líquido: 8,72 ms no 6930 e 12,12 no 6960. Do 6959 ao 6960, o líquido
sobe 6,03 ms: comum +3,58, VU0 +0,74 e contorno/tabela pelos ids/cópia dos ids +1,25, além dos demais deltas.
VU4 fica em ~2,8; FXAA em ~0,79. Resolves de cor custam 0,83/1,17 nos alvos, entradas na EDRAM 0,12/0,17 e volta
da cor 0,31/0,31: não há dez ms isolados em Resolve ou FXAA a recuperar. Não remover passadas requeridas.

A GPU sozinha **não excede dois vblanks** nos quadros alvo: mesmo o bruto é 18,36/25,98 ms, e o líquido
15,66/22,56, para ~33,27 ms disponíveis por período nessa sessão. Ela acaba tarde **em relação ao prazo da cadeia**:
na base, o Present ainda tem 11,8/7,4 ms, mas a cauda até a marca dura 14,4/17,0 ms. Não somar GPU total a essa
cauda, nem GPU total a todo `frame_end`: o envio de comandos já se sobrepõe à execução da GPU.

O perfil CPU reforça a necessidade de localizar as chamadas: F 2,413/6,846/2,993 nos 6929/6930/6931 e
2,776/5,772/5,998 nos 6959/6960/6961. No 6930, `outros` tem 5,416 ms; no 6960, VU0 tem 3,859, contra 0,826 de
GPU líquida. Preparo/plano/limpeza têm 0,001/0,071/0,003 no 6930 e 0,001/0,219/0,004 no 6960. Isso não permite
chamar os 5,4 ms de um shader nem supor que juntar constantes os tirará: as classes CPU acumulam tempo entre
mudanças de classe, incluindo chamadas e possíveis esperas. Separar esses trechos na próxima medida.

### A gravação no Xenon [medido]

126 quadros, **6900 a 7025**, soma exclusiva conservada em todos, com marcas por chamada. O total não inclui o
intervalo sem trabalho entre listas. A VU1 restante exclui seus filhos; envio e hash não se somam novamente ao GIF.
Tempos em ms; as cinco maiores somas são 6900, 6996, 6995, 6994 e 6999. A mediana é desta faixa, não do combate
inteiro. As medianas das parcelas não precisam somar a mediana do total.

| Q | Total | DMA | VIF | GIF | CPU | VU1 restante | Unpack | Envio | Entrega | Hash | Constantes | Fecho |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 6900 | 31,840 | 0,288 | 2,673 | 4,587 | 2,419 | 6,982 | 2,839 | 5,000 | 0,728 | 2,530 | 1,938 | 1,857 |
| 6930 | 15,570 | 0,191 | 1,429 | 1,295 | 1,800 | 2,588 | 1,196 | 3,463 | 0,068 | 2,023 | 0,567 | 0,950 |
| 6960 | 21,742 | 0,240 | 1,978 | 2,072 | 2,371 | 4,517 | 1,824 | 3,496 | 0,410 | 2,237 | 1,259 | 1,338 |
| 6994 | 25,868 | 0,275 | 2,516 | 2,099 | 2,214 | 5,666 | 2,504 | 4,561 | 0,408 | 2,349 | 1,503 | 1,771 |
| 6995 | 25,922 | 0,273 | 2,504 | 2,096 | 2,179 | 5,684 | 2,500 | 4,528 | 0,440 | 2,437 | 1,500 | 1,778 |
| 6996 | 26,017 | 0,272 | 2,506 | 2,084 | 2,203 | 5,674 | 2,522 | 4,705 | 0,408 | 2,377 | 1,496 | 1,769 |
| 6999 | 25,867 | 0,272 | 2,482 | 1,995 | 2,057 | 5,656 | 2,494 | 4,749 | 0,456 | 2,428 | 1,494 | 1,784 |
| Mediana | 16,587 | 0,190 | 1,422 | 1,755 | 2,106 | 3,269 | 1,246 | 3,284 | 0,407 | 2,039 | 0,929 | 0,939 |

Comparação com a x86gv histórica da seção 28 (`~/f5-cortes/diag-duelo.log`), sem threads e com backend falso:

| Parcela (ms) | Xenon 6930 | x86gv 6930 | Xenon 6960 | x86gv 6960 | Xenon mediana faixa | x86gv mediana faixa |
|---|---|---|---|---|---|---|
| Total | 15,570 | 1,437 | 21,742 | 2,832 | 16,587 | 2,633 |
| dma | 0,191 | 0,064 | 0,240 | 0,089 | 0,190 | 0,086 |
| vif | 1,429 | 0,219 | 1,978 | 0,349 | 1,422 | 0,311 |
| gif | 1,295 | 0,109 | 2,072 | 0,206 | 1,755 | 0,187 |
| cpu | 1,800 | 0,127 | 2,371 | 0,162 | 2,106 | 0,151 |
| vu1 | 2,588 | 0,193 | 4,517 | 0,512 | 3,269 | 0,462 |
| unpack | 1,196 | 0,106 | 1,824 | 0,240 | 1,246 | 0,205 |
| envio | 3,463 | 0,169 | 3,496 | 0,200 | 3,284 | 0,194 |
| entrega | 0,068 | 0,195 | 0,410 | 0,695 | 0,407 | 0,648 |
| hash | 2,023 | 0,179 | 2,237 | 0,184 | 2,039 | 0,177 |
| constantes | 0,567 | 0,031 | 1,259 | 0,086 | 0,929 | 0,078 |
| fecho | 0,950 | 0,044 | 1,338 | 0,109 | 0,939 | 0,097 |

No 6960, DMA/VIF/GIF/CPU somam **6,660 ms**, contra 0,806 na x86gv; no 6930, **4,715**, contra 0,520.
O resto antigo tem peso real no Xenon, além de VU1, envio e hash. No 6960, envio 3,496 e hash 2,237 somam 5,733;
constantes/fecho somam 2,598. Mesmo a eliminação hipotética dessas duas últimas parcelas **inteiras e instrumentadas**
não cobriria o déficit de 9,6 ms. Na x86gv, envio/hash são 0,200/0,184, e CPU 0,162 contra 2,371 no Xenon:
não transportar fatores de ganho da x86 para o console.

A mediana total da x86gv no combate inteiro da seção 28 era 4,635 ms; **na mesma faixa** 6900:7025 ela é 2,633,
contra 16,587 do Xenon. A entrega 0,407 do console contra 0,648 no falso também mostra que não há um multiplicador
único. A x86gv hasheia/lê as listas na entrega, não desenha no Xenos; o Xenon usa threads, VU1 direto, outro compilador,
outro float e relógios por chamada. São rateios de ambientes diferentes, não uma regressão de 6,3 vezes nem uma
previsão de ganho transferível. Os 31,840 ms do 6900 não geraram imagem longa: somas ocupadas maiores não determinam
o prazo de tela sem a fase de cada entrega. Os quadros pesados 6994 a 6999 tampouco são os B das longas desta abertura.

`cortes.py` exige o rodapé `gpu_falso` e, por isso, não aceita diretamente um log de console. A extração local aplica
às linhas do Xenon o mesmo formato e a mesma tolerância de conservação, sem inventar esse rodapé. O 404 de `hash`
no rateio do 6960 conta chamadas à parcela, incluindo acertos; o 293 dos picos conta recálculos de página. Não
interpretar contadores diferentes como perda de eventos. **[Lido]** `Gs_PageHash` já tem cache por geração da página:
propor simplesmente acrescentar esse cache contaria uma otimização existente de novo.

### Causa, próximo corte e o que medir

**[Medido]** Na base, a cadeia desde a entrega do jogo até o fim GPU dura aproximadamente **35,5 ms no 6930** e
**42,4 no 6960**, para 32,9/32,8 até o prazo. Ela se decompõe pelos carimbos em 14,5 + 6,6 + 14,4 e
19,9 + ~5,6 + 17,0 (arredondamento de F 5,5). O jogo chega no tempo, o desenho apresenta antes do prazo, mas a GPU
termina 2,6/9,6 ms depois. Não há textura nova nesses quadros. É uma cadeia tardia, com mudança de carga de desenhos
no 6960, não evidência de GPU isolada acima do período nem de falta de média de fps. Os cortes atuais adiantaram
~0,4 ms nesta amostra; **zero continua reprovado**. Nas cinco aberturas, o cache já evita a imagem longa do Blast 2;
essa ausência não quantifica o trabalho individual dele nem prova partida livre sem cargas.

**Decisão conservadora:** não há, neste rateio, **um corte pequeno identificado que demonstre zerar as duas**.
Nenhuma otimização nova, chave ou mudança de padrão foi implementada. Não mudar apresentação, filas, escala, FXAA,
conteúdo, vblank ou latência para produzir artificialmente o zero. A alternativa prevista na tarefa é esta análise
e a próxima medição; as provas x86/PPC, mutantes do corte, `divergencias.py` e novos ambientes de uma otimização
ficam condicionadas a existir esse corte. Não as declarar executadas para um código que não mudou.

**[Estimado]** Para o pior alvo **sem marcas**, o novo orçamento precisa recuperar **ao menos 10,6 ms na cadeia**
(9,6 medidos mais 1 de margem); para o 6930, 3,6. Um caminho a investigar é reduzir o trabalho comum da GPU pela
área efetivamente necessária e pelos shaders dos estados dominantes, junto de envios redundantes e do restante
da gravação, preservando ordem e conteúdo. Como metas de investigação, não ganhos: 2 a 3 ms em envio/hash,
1 a 2 no desenho e 4 a 6 na cauda GPU. A soma nominal 7 a 11 **não garante** o orçamento: apenas sua ponta superior
alcança 10,6, e há sobreposição entre CPU/GPU. Não creditar os 12,12 ms inteiros de `comum` como removíveis.
O perfil atual não identifica qual subpassada domina ou se sua área pode encolher sem mudar a imagem.

Próxima rodada, a preparar com a revisão Linux após instrumentar os trechos que faltam:

1. **Controle sem marcas:** pelo menos três pares alternados base/ambos, mesma combinação, som e roteiro, saves
   novos e únicos a cada abertura (`sv_c2b01`, `sv_c2a01`, depois 02/03). Publicar todas as imagens longas, folgas e
   ocupada por alvo; estimar a variação entre aberturas antes de atribuir os ~0,4 ms a uma chave. Se for necessário
   separar constantes/fecho/cache, rodar os opcionais em aberturas próprias, sem inferir o que ainda não rodou.
2. **GPU em aberturas separadas, só o alvo:** `BT3_CORTES_GPU=6930,6930` e `6960,6960`, com `BT3_CORTES=0` e saves
   `sv_c2g30_01`/`sv_c2g60_01`, depois novos sufixos. Não marcar o antecessor: nesta rodada foi ele que perdeu o prazo,
   mudando a referência do alvo. Continuar drenando os buffers. Comparar com base sem marcas, nunca aprovar a tela
   pelo diagnóstico. Repetir cada alvo para separar variação de custo de marca.
3. **Abrir `comum` sem marcas por desenho na luta inteira:** segmentos contíguos por fonte (`mode[0]`: sem textura,
   VRAM, alvo, pacote), shader escolhido, afiado e alvo de destino; contagem de desenhos, área da tesoura e leituras
   de alvo. Somar ao `comum` antigo, calibrar as marcas e registrar bruto/líquido. Distinguir cópias/efeitos sobre alvos
   dos desenhos comuns de geometria antes de propor redução de área ou shader. Identificar a parcela e a economia
   necessária em cada alvo, sem remover contorno, DATE, FBA ou tabela requerida.
4. **CPU e gravação em diagnósticos separados:** localizar chamadas/esperas dentro de `outros` no 6930 e VU0 no 6960;
   dividir envio por formato e bytes, recálculos/acertos de hash e escritos repetidos, VIF/GIF/CPU por leitor e VU1
   restante por família. O cache por geração já existe: medir a repetição e as invalidações antes de propor omitir
   escrita/hash. Reaplicar o rateio também com ambos, sem perfil GPU, pois o atual não isola cada corte.
5. Só então prototipar o candidato desligado por padrão. Exigir equivalência das listas x86gv/PPC sem mapa,
   controle negativo de cada verificador, objetos planos sem depuração iguais e `.text` preservada. O ganho só vale
   se adiantar a **GPU até o prazo sem marcas**, sem atraso de entrada, e zerar a tela. Repetir depois as três
   janelas longas fria/aquecidas, a sessão livre com cargas e os outros critérios da etapa 10. Este Duelo curto
   de 684/2.334 passos não fecha a F5.

### Conferência desta análise [medido]

Artefatos fora do git em `~/f5-cortes2/`: `confere.py`, `imagens.json`, `extracao.log`, logs recortados `NOME/ate-fim.log`
e saídas do passos. O extrator confere ambientes/saves, fim/resultado, associação, histogramas e linhas individuais,
classes e conservação bruto/líquido/custo da GPU, faixa integral e soma do rateio. Foi visto sair com **1** em nove
cópias: fim retirado, associação perdida, imagem longa omitida, duração trocada, classe GPU retirada, custo GPU
adulterado, total de gravação adulterado, parcela retirada e quadro rateado retirado. Originais: “tudo certo na
extração”, **não** “zero aprovado”. Os arquivos originais foram preservados.

| Rodada final da análise | Resultado |
|---|---|
| Extração dos cinco logs e controles negativos | saída 0 nos originais; nove cópias recusadas com saída 1 |
| `passos.py NOME/ate-fim.log` nas cinco | estrutura conforme, saída 0 |
| `passos.py NOME/ate-fim.log --console --janelas 2 --minimo 600` nas cinco | saída 1 em todas: j2 com 2/2/2/2/3 imagens longas; apenas j1 passa |
| GPU, seis quadros | classes completas; somas bruta, líquida, custo e marcas conservadas |
| Gravação, 126 quadros | faixa completa, parcelas exclusivas conservam o total |
| Roteiros, resultado e associação da rodada do Lucas | fim nas cinco, resultado e passos iguais; nenhum erro/associação perdida |

O `passos.py` do commit atual aplica a decisão 27, não o antigo 1%. O parâmetro 600 só deixa examinar estas duas
janelas curtas; não substitui três de 2.700. Seu “estrutura ok” imprime o aviso genérico sobre emulador mesmo ao
ler este log de console; aqui foi usado apenas para a estrutura. Não foi usado `--carimbo` para certificar uma
build limpa: a origem literal acima é a que foi efetivamente registrada.

A `.text` do binário x86 **preexistente** em `port/build/x86/bt3_x86` (carimbo `763e4b73 diff:da39a3ee5e`), extraída
novamente com `objcopy -O binary --only-section=.text`, deu SHA1 **`43444ea4d03a6d448aa1ae74a631e35b356a7caa`**.
Não é build nova deste branch. Só documentação muda: nenhum fonte, objeto ou binário foi alterado; não há nova
prova de execução x86/PPC ou build 360 nesta entrega. As provas anteriores da seção 28 não são reatribuídas a ela.

## 30. Abrir a passada comum: listas, cobertura e o perfil fino (28/10/2026)

Branch `codex/f5-gpu-comum`, base `4c85c039`. Sem console, sem alteração de padrão, apresentação, conteúdo ou entrada.
A meta da decisão 27 continua zero imagens de três vblanks no combate. Não há novo ganho de GPU medido nesta tarefa.

### O que foi contado e o que ainda não se mede

**[Medido, reconstrução x86gv]** `BT3_GPU_COMUM=pasta` com
`BT3_GPU_COMUM_QUADROS=6900,6929,6930,6931,6959,6960,6961` exporta JSONL por quadro, antes de o falso consumir a lista.
Inclui todos os estados, alvos e pipelines e as posições das primitivas comuns; não exporta pixels nem transforma VU1.
`BT3_SCALE=1` é obrigatório: o falso começa em 2 e o 360 força 1. `gpu_comum.py` faz o censo por shader, alvo e chave
completa de pipeline, com cobertura dos triângulos pela tesoura. Saídas em `~/f5-gpu-comum/`, fora do git.

**[Lido]** Com os padrões do console, as subclasses relevantes são `ps_gs` sem textura mas com arte 2D ou testes,
`ps_gs` lendo alvos, `ps_gs_af` para o HUD, `ps_gs_tex` para primitivas com textura sem testes e `ps_gs_sem` para as
sem textura que o predicado atual já especializa. VU0/4/6, nativas, a segunda passada FBA, DATE, Resolve e limpeza
não entram na área comum. A máscara dos principais pipelines é RGBA (`f`), só alfa (`8`) ou só RGB (`7`).
`0xf0445` e `0xf0545` misturam com Z sempre, `0xf0845`/`0xf0815` misturam com teste GEQUAL;
`0xf1400` escreve RGBA e Z sem mistura. O censo guarda as demais chaves, sem colapsá-las pela aparência.

A área abaixo é **fragmentos geométricos antes de alfa/DATE/Z**, somados por triângulo e por alvo, em uma chamada
comum. A coluna de 4/3 usa centros de pixels na grade de 854 x 598 que contém a área lógica de 640 x 448; a alocação
real do console é 864 x 608 (648 x 456 lógicos). Não é largura de viewport mudada para 854, nem rasterização capturada
do Xenos. A sobreposição é soma menos união de pixels por alvo; não significa trabalho removível.
Para as escritas, o mínimo conta somente máscara efetiva não vazia na primeira chamada, Z sempre e nenhum teste de alfa ou DATE; o máximo
admite todos os fragmentos. Não afirmar um total exato dos pixels escritos pelos desenhos com testes sem reproduzir
seus buffers e texturas. A quantização das bordas no Xenos também pode diferir desta reconstrução.

| Q | Todos os desenhos | Comuns | Vértices comuns | Fragmentos, escala 1 | Fragmentos, 4/3 | Escritas, intervalo 4/3 |
|---|---:|---:|---:|---:|---:|---:|
| 6900, referência de combate com muita geometria | 2.377 | 131 | 3.144 | 7.731.158 | 13.745.897 | 7.469.231 a 13.745.897 |
| 6929 | 214 | 95 | 2.235 | 4.809.417 | 8.548.658 | 4.715.065 a 8.548.658 |
| 6930 | 213 | 94 | 2.247 | 5.035.606 | 8.953.036 | 4.716.973 a 8.953.036 |
| 6931 | 213 | 94 | 2.238 | 4.609.900 | 8.194.995 | 4.719.501 a 8.194.995 |
| 6959 | 1.408 | 116 | 3.276 | 6.184.995 | 10.996.111 | 7.353.381 a 10.996.111 |
| 6960 | 1.408 | 116 | 3.270 | 6.523.730 | 11.597.707 | 7.353.381 a 11.597.707 |
| 6961 | 1.408 | 116 | 3.273 | 6.421.400 | 11.415.026 | 7.353.381 a 11.415.026 |
| 7000, referência de combate após a troca | 1.409 | 113 | 2.694 | 6.463.038 | 11.490.914 | 6.738.280 a 11.490.914 |

O 7000 tem 34 leituras de alvos: mantém as duas cadeias de brilho e perde quatro cópias do caminho
da sombra. Por shader, tem 17 desenhos GS sem textura, 34 de alvo, 42 afiados, 7 rápidos com textura e
13 rápidos sem textura. Sua cobertura é próxima da do 6960; a troca acrescenta trabalho e muda a fase da cadeia,
sem produzir uma carga de área exclusiva e inteiramente descartável no alvo problemático.

A união comum é 319.488 pixels a 1 e 568.406 a 4/3 nos sete, somando as superfícies distintas. No 6930,
8.384.630 dos 8.953.036 fragmentos a 4/3 se sobrepõem a outros no mesmo alvo; no 6960, 11.029.301 dos
11.597.707. Há mistura em 91 dos 94 desenhos comuns do 6930 e 112 dos 116 do 6960. Misturas, máscaras de alfa,
Z e leituras de resultados anteriores impedem tratar essa sobreposição como cobertura opaca descartável.

| Shader/estado, comum | Desenhos 6930 / 6960 | Vértices 6930 / 6960 | Fragmentos 6930 / 6960, a 4/3 |
|---|---:|---:|---:|
| `ps_gs`, sem textura, com arte/testes | 11 / 17 | 516 / 954 | 2.856.547 / 4.720.113 |
| `ps_gs`, alvo como textura | 22 / 38 | 690 / 984 | 1.906.115 / 2.692.113 |
| `ps_gs_af`, VRAM e arte no quadro | 44 / 42 | 498 / 498 | 273.279 / 261.161 |
| `ps_gs_tex`, VRAM sem arte/testes | 4 / 6 | 465 / 756 | 3.774.212 / 3.781.437 |
| `ps_gs_sem`, sem textura especializado | 13 / 13 | 78 / 78 | 142.883 / 142.883 |

**[Medido, listas]** No 6960 os alvos comuns são o quadro (0, FBP 0), com 79 desenhos/2.466 vértices e
10.187.755 fragmentos a 4/3; o 3 (FBP 336), 9/492 e 1.032.207; o 4 (368), 6/168 e 204.345;
o 5 (502), 12/84 e 101.150; o 6 (504), 10/60 e 72.250. O quadro alterna com o alvo 2 (FBP 112)
nos ímpares. Os alvos 1 (FBP 140) e os outros sem desenho não contam como área escrita.

### Onde dobra e quais trocas a lista permite contar

**[Medido, x86gv]** No 6960 há duas cadeias de brilho, nos desenhos **109 a 125** e **1261 a 1277**.
Cada uma tem a limpeza por primitivas e dezesseis desenhos que leem alvos, **901.776 fragmentos a 4/3**.
Elas passam do quadro a 256, 128, 64, alternam os alvos 5/6 dez vezes e ampliam de volta ao quadro.
Entre as duas há geometria de VU1; elas não leem a mesma imagem. Há ainda sombra, tabela, cópia dos ids e contorno.
A duplicação é desse percurso, não de todo o HUD nem de todos os pixels comuns: no 6930 há uma cadeia de brilho,
22 leituras de alvos no total; no 6960, duas e 38 leituras. A referência 6900 também tem duas e 38 leituras,
mas 2.377 desenhos totais; menos desenhos totais no 6960 não implica menor área comum.

**[Medido]** A segunda cadeia já aparece no **6959 da x86gv**; no console, a mudança do perfil ocorre no 6960.
O 6958 da x86gv ainda tem 210 desenhos, 94 comuns e 22 leituras. A lista do 6930 e a do 6960 têm as contagens
publicadas nos picos do console (213/2.247 e 1.408/3.270), mas isso não torna todos os seus vizinhos equivalentes.
Não atribuir os 8,538 ms do 6959 do console à lista mais pesada do 6959 da x86.

**[Medido, transições da lista inteira, incluindo VU/nativas]** Do 6930 ao 6960: alvo 21 a 39, pipeline 57 a 1.191,
textura 191 a 1.331, sampler 31 a 1.157, modo 65 a 1.194, misc 61 a 71, tesoura 31 a 42.
São diferenças entre registros consecutivos, não chamadas D3D: o cache de estado, as nativas e a residência de
EDRAM podem evitar ou acrescentar chamadas. O salto de pipeline/textura é sobretudo a geometria VU intercalada.
**[Medido, console anterior]** As marcas de Resolve de cor são 21/36 no 6930/6960 e custam 0,832/1,166 ms líquidos
estimados (seção 29). São segmentos, não contagem certificada de `Resolve`. **[Lido]** Uma leitura de alvo só resolve
se sua versão na EDRAM estiver suja; a cópia DATE e as nativas têm chamadas próprias. A lista não registra esses
comandos do backend 360. O perfil novo registra as chamadas efetivas para resolver essa lacuna na próxima rodada.

### O perfil fino preparado, desligado por padrão

**[Lido e testado por simulação]** `BT3_COMUM_GPU=A,B`, `=1` somente 6960, seleciona os quadros inclusive dentro da
janela de passos, como `BT3_CORTES_GPU`. Se ambas forem ligadas, a chave fina escolhe a faixa. Ausente ou 0 mantém
o perfil antigo. Divide somente a família comum por fonte/shader: sem textura, VRAM, alvo, pacote, afiado,
rápido sem textura, rápido com textura e trivial. Trocar alvo ou faixa absoluta de **128 desenhos** fecha um trecho,
mesmo mantendo o shader. Não põe marca em cada desenho. O limite continua 4.096 marcas por quadro; transbordo ou
captura invalidam a amostra explicitamente.

Linhas `cortes: gpu ... classe comum ...` são os agregados. `cortes: comum ... classe ... alvo ... desenhos I:F`
são os trechos que os compõem, com bruto, líquido calibrado, limite da espera CPU e marcas. Somar os trechos
por classe deve reproduzir o agregado, nunca acrescentá-los a ele. O desconto continua um trecho vazio por marca;
valores líquidos negativos não são truncados. FBA e as demais classes continuam separadas. Preserva a fronteira
antiga depois de `prepara`: parte do preparo seguinte é atribuída ao trecho anterior, logo isto não isola o shader.
Os metadados ficam nos mesmos dois buffers, com o quadro emitido, e são drenados fora da faixa.

A linha `cortes: comum quadro Q API` conta chamadas efetivas de Resolve, Clear, pixel shader, render target,
textura, tesoura, UP e índices, somente no quadro selecionado, incluindo outras famílias e as chamadas da limpeza
final. Conta comandos, não estados distintos ou custo. A DPC continua só com o relógio e a barreira; sem log,
float, desenho ou acesso ao dispositivo. Os metadados acrescentam 96 KB estáticos, sem alocação na luta.

`python3 port/x360/tools/comum_console.py ~/f5-gpu-comum/console --rodada gc2810a` preparou dez ambientes, `boot.txt`, `f5_duelo.txt`
e `ordem.txt`, sem build nem FTP: três pares alternados `base1/ambos1` a `base3/ambos3` sem marcas finas, depois
`g6930_1/g6960_1` e `g6930_2/g6960_2`, cada um marcando **só o alvo**, com constantes/fecho desligados.
Saves `sv_gc2810a_NOME`, todos distintos. Repetição exige outro identificador e diretório novo. Na revisão Linux,
compilar este commit e preparar os ambientes ao lado da build; conferir carimbo, recursos e ordem antes da rodada.

Aberturas com marcas localizam custo, não aprovam a tela. Em cada alvo conferir classes, trechos completos,
conservação bruto/custo/líquido, nenhuma amostra indisponível, quantidade de chamadas e o prazo medido sem marcas.
Nos controles, publicar todas as imagens longas, folga, jogo/gravação/desenho e resultado; aplicar
`passos.py LOG --console --janelas 2 --minimo 600 --carimbo CARIMBO_REAL`. Exigir associação completa,
684/2.334 passos, vencedor 0x2, razão 0x1 e vida 0/27160. Isso não substitui as três janelas longas da F5.

### Candidatos, ganhos ainda estimados no 6960

Nenhum corte foi implementado. A área não determina o tempo, o preparo contamina as classes e os resultados
intermediários do brilho mudam entre as cadeias. O perfil fino é necessário para escolher um candidato com ganho
atribuível. Não há demonstração de um corte que recupere os **10,6 ms na cadeia**. Não somar estimativas abaixo como
se fossem parcelas independentes: há sobreposição de execução e candidatos concorrentes sobre os mesmos fragmentos.

| Candidato [lido] | Ganho no 6960 [estimado, hipótese a testar] | Risco de imagem e prova necessária |
|---|---|---|
| Tesoura mais apertada, geometria fora de 640x448 e padding da alocação | 0 ms de área adicional demonstrável: nos sete quadros, a cobertura em 648x456 é igual à de 640x448. A tesoura já recusa a geometria excedente | Recortar a coluna que o bilinear, FXAA, ids ou uma redução lê muda a borda. Provar a região de leitura incluindo halos e comparar todos os intermediários e a captura final, nas duas escalas |
| Desenhos cobertos | 0 ms creditado; o teto de remover todos os 12,123 ms comuns não é realizável | A maioria mistura; há máscaras só alfa, DATE e dependência Z. Exigir cobertura opaca e prova de que nenhum leitor futuro usa a versão anterior; equivalência de listas normalizadas e imagens, com mutante que omita um leitor |
| Especializar mais `ps_gs`, sobretudo sem textura e com `misc.w` não nulo, e os estados de cópia de alvo | 0 a 3 ms como faixa de investigação, sem tempo fino disponível. O shader sem textura já existe, mas o predicado exclui arte 2D; o HUD afiado é só 0,261 M de 11,598 M fragmentos | Preservar DATE, alfa, FBA, UV preso, borda e gradientes. Provar predicado e saída de todos os estados elegíveis, mutantes, x86/PPC e capturas iguais; medir cada subclasse no Xenos antes/depois. Não desligar afiado nem usar o trivial como solução |
| Reduzir o alcance do brilho e das cópias de alvos | 0 a 2 ms, condicionado a regiões realmente vazias; duas cadeias somam 1,804 M fragmentos a 4/3, não todo o comum | Blur amplia suporte; o conteúdo entre as duas cadeias difere. Propagar região útil e halo pelas dez alternâncias, provar intermediários e finais idênticos; não omitir a segunda cadeia |
| Resolve/limpeza redundantes | 0 a 0,5 ms plausíveis, teto não realizável de 1,166 ms para toda a cor e 0,175 para entradas do perfil antigo | A residência, limpeza fora do conteúdo e cache por geração já existem. Exigir log de chamadas novas e versões sujas/lidas; um mutante que pule Resolve obrigatório deve falhar. Capturas dos intermediários mais o quadro final |
| Trabalho no quadro anterior | 0 ms creditado à GPU sem especulação. Não há texturas novas nesses alvos; a geometria e a segunda cadeia dependem do quadro corrente | Pré-alocar recursos pode adiantar CPU, mas não mover desenho/Resolve que lê o quadro atual. Provar dependências, ordem, fase e latência pelos carimbos sem marcas; não acrescentar fila nem vblank |

Para qualquer protótipo futuro, executar o conjunto da seção 28, incluindo `divergencias.py`, negativos, objetos
planos sem depuração e capturas, antes de medir o console. Nesta entrega as comparações de listas certificam o
exportador; não são prova de equivalência de um shader novo, pois o backend falso não rasteriza shaders D3D.

### Conferência local e limites

**[Medido]** Builds da base `4c85c039` e do código novo, com `BT3_SCALE=1`, `BT3_TEX_ORIGEM=1`, sem threads:

| Conferência | Resultado |
|---|---|
| x86gv, Duelo completo, base contra exportador ligado | 8.380 quadros, `divergencias.py --normal`: 0 diferentes; hash cru `fba506996b28cdae` igual |
| PPCg real no qemu, Duelo completo, `SEMMAPA=1`, mesmos dados convertidos da rodada anterior | 8.380 quadros, 0 diferentes; hash cru `807d95bd0dfc40ee` igual; nenhuma textura já solta |
| Censo x86gv contra PPCg, nove quadros, incluindo 6928 e 6958 | JSON de resultados inteiro igual byte a byte; ambas veem a segunda cadeia no 6959 |
| x86gv, Duelo completo, duas threads e exportação de 6800/7000/7100 | 8.380 quadros; listas normais iguais às da base sem threads |
| `teste_gpu_comum.py`, código real com dispositivo simulado | seleção, shader, alvo, faixa, VU, dreno e conservação passam; cinco mutantes recusados |
| Censo, entradas copiadas e estragadas | desenho ausente, posições truncadas, ordem e alvo inválidos recusados; um JSONL real truncado sai 1 |
| Censo, primeira chamada FBA com máscara só alfa | caso sintético de 32 pixels: mínimo 0 com as duas passadas e 32 sem FBA; números dos quadros analisados preservados |
| `teste_cortes.py` | diagnóstico anterior, dreno e seus controles negativos continuam passando |
| `divergencias.py` e `cmp`, cópias adulteradas | hash normal do 6960 e byte de objeto alterados: saída 1 nos dois |
| Ambientes | dez saves únicos; tentativa de sobrescrever diretório existente recusada |
| x86 plana, base contra final, `objcopy --strip-debug` e `cmp` | 396 de 397 objetos iguais; apenas `x360_passos.o`, o carimbo de teste, difere |
| `.text` das duas builds planas | SHA1 `43444ea4d03a6d448aa1ae74a631e35b356a7caa` igual |

Não há corte de execução nesta entrega; a matriz extensa da seção 28, `roteiros.sh`/`save_mc.sh` e novas capturas
não foram repetidos como prova de uma otimização inexistente. O roteiro completo aqui é o Duelo no backend falso,
com estados/listas comparados à base, nas duas arquiteturas. Não chamar o seu tempo de GPU.
Os artefatos, logs, builds e carimbos ficam em `~/f5-gpu-comum/`; a rodada do commit final e seus limites estão
no relatório `~/entrega-f5-gpu-comum.md`.
A build do XDK, as subclasses medidas no Xenos, a contagem efetiva de chamadas D3D e o zero de imagens longas
continuam pendentes da revisão e do console. Não houve console nesta tarefa.


## 31. O perfil fino de 28/10: o zero continua fora dos cortes pequenos sustentados pela medida

Branch `codex/f5-gpu-fina`, base `af015da4`. **Veredito:** os candidatos pequenos da seção 30 não têm ganho
medido que recupere os **10,6 ms na cadeia**. Mesmo sua soma otimista, ainda estimada e com sobreposição, não chega.
A decisão 27 continua reprovada; a decisão 28 exige a mesma disciplina para a tela dividida. Cabe ao Lucas escolher
entre um trabalho maior na cadeia e uma mudança explícita de imagem, latência ou aceite. Nenhuma dessas mudanças
foi aplicada. Isto delimita o que estes cortes permitem afirmar, não prova a impossibilidade de qualquer reprojeto.

### Primeiro, a variação entre aberturas iguais [medido]

Nos três controles `base`, a folga GPU do 6930 é -2,7/-2,7/-2,8 ms; no 6960, -9,8/-9,9/-9,9.
Em `ambos`, é -2,4 nas três aberturas do 6930 e -9,5 nas três do 6960. A amplitude entre aberturas iguais é
**0,1 ms na base e 0,0 ms em ambos**, na precisão publicada. A gravação ocupada varia 0,0/0,1 ms nos alvos da base;
em ambos, 0,0/0,0. Isso precede a atribuição do ganho: os três pares alternados repetem um adiantamento de
**0,3 a 0,4 ms no fim GPU**, maior que essa variação observada, mas conservam as mesmas duas imagens longas.
É um efeito da combinação constantes 2/fecho 1, sem isolar constantes, fecho, memória ou identidade.
Não é intervalo de confiança nem autorização para ligar as chaves por padrão.

Logs originais em `~/sessao_2810/gpu/NOME/bt3xbx.log`. O Lucas informou main `201ba92`; o carimbo literal nas dez
aberturas é **`c57e807 diff:da39a3ee5e`**, sem diferenças locais, coerente com o mesmo código 360 informado.
A análise termina no primeiro `roteiro: f5_duelo.txt:99 fim`, quadro 8378. Há continuação nos arquivos originais;
ela não é amostra adicional da luta. Os ambientes conferem os dez saves únicos `sv_gc2810a_NOME`, constantes/fecho
0/0 ou 2/1, som, duas threads, 16:9, escala 1,33, FXAA, envio rápido, VU1 direto, cache pela origem e tempos 0.
A ordem fornecida pelo Lucas é base1/ambos1, base2/ambos2, base3/ambos3, g6930_1/g6960_1, g6930_2/g6960_2.

Nos seis controles, j1 tem 684 passos e 684 imagens de dois vblanks, incluindo o Blast 2. J2 tem 2.334 passos;
todas as longas são **6929 → 6930** e **6959 → 6960**, com três vblanks cada. São 12 longas nos seis controles,
zero de quatro ou mais. Nas vinte janelas das dez aberturas: 684/2.334 passos, dois vblanks por passo sem exceção,
parede/esperado 0,9972, vencedor 0x2, razão 0x1, vida 0/27160, resultado no quadro 7757/vblank 11478.
Sem `FALHA`, `CRASH`, `ERRO`, vigia ou descartes requeridos. Associação completa: zero repetições, trocas sem quadro,
puladas ou marcas GPU ausentes. Nos resumos por imagem de j2 são 2.335 trocas: base 4/2329/2/0,
ambos 3/2330/2/0. A tabela usa a contagem do passos; a diferença de fronteira em ambos é uma imagem de dois,
não perda de associação. Os dois histogramas não são amostras independentes.

| Controle | J2, passos: 1 / 2 / 3 / 4+ | Folga GPU j2: mínimo / p1 / p10 / p50 (ms) | Trabalho j2: p50 / p99 / máximo (ms) |
|---|---|---|---|
| base1 | 4 / 2329 / 2 / 0 | -9,8 / 2,3 / 6,3 / 8,4 | 26,79 / 30,83 / 33,28 |
| ambos1 | 3 / 2329 / 2 / 0 | -9,5 / 2,6 / 6,3 / 8,4 | 24,86 / 30,33 / 31,96 |
| base2 | 4 / 2329 / 2 / 0 | -9,9 / 2,3 / 6,3 / 8,4 | 26,78 / 30,82 / 33,28 |
| ambos2 | 3 / 2329 / 2 / 0 | -9,5 / 2,6 / 6,3 / 8,4 | 24,82 / 30,36 / 32,03 |
| base3 | 4 / 2329 / 2 / 0 | -9,9 / 2,2 / 6,3 / 8,4 | 26,79 / 30,83 / 33,31 |
| ambos3 | 3 / 2329 / 2 / 0 | -9,5 / 2,6 / 6,3 / 8,4 | 24,85 / 30,37 / 31,91 |

Cada longa dos seis controles está abaixo. A é a imagem retida, B o quadro seguinte que perdeu o prazo.
J/G/P/U são folgas da entrega do jogo, entrega da gravação, Present e fim GPU, positivas antes do prazo.
Jogo é o período da CPU do jogo; G ocupada e F são CPU, não GPU. Em todas: zero texturas novas e decodificação 0,0 ms.

| Controle | A → B | J / G / P / U (ms) | Jogo / G ocupada / F (ms) |
|---|---|---|---|
| base1 | 6929 → 6930 | 32,9 / 18,3 / 11,6 / -2,7 | 33,3 / 14,7 / 6,7 |
| base1 | 6959 → 6960 | 32,8 / 12,7 / 6,9 / -9,8 | 33,4 / 20,1 / 5,8 |
| ambos1 | 6929 → 6930 | 32,9 / 18,6 / 12,0 / -2,4 | 33,3 / 14,3 / 6,7 |
| ambos1 | 6959 → 6960 | 32,8 / 13,1 / 7,3 / -9,5 | 33,4 / 19,8 / 5,8 |
| base2 | 6929 → 6930 | 32,9 / 18,3 / 11,6 / -2,7 | 33,3 / 14,7 / 6,7 |
| base2 | 6959 → 6960 | 32,8 / 12,7 / 6,9 / -9,9 | 33,4 / 20,2 / 5,8 |
| ambos2 | 6929 → 6930 | 32,9 / 18,7 / 12,0 / -2,4 | 33,3 / 14,3 / 6,7 |
| ambos2 | 6959 → 6960 | 32,8 / 13,1 / 7,2 / -9,5 | 33,4 / 19,8 / 5,9 |
| base3 | 6929 → 6930 | 32,9 / 18,3 / 11,6 / -2,8 | 33,3 / 14,7 / 6,7 |
| base3 | 6959 → 6960 | 32,8 / 12,7 / 6,8 / -9,9 | 33,4 / 20,2 / 5,9 |
| ambos3 | 6929 → 6930 | 32,9 / 18,6 / 12,0 / -2,4 | 33,3 / 14,3 / 6,7 |
| ambos3 | 6959 → 6960 | 32,8 / 13,1 / 7,2 / -9,5 | 33,4 / 19,8 / 5,8 |

A cadeia jogo entregue → fim GPU no pior alvo da base dura **42,6 a 42,7 ms**, com prazo a 32,8 ms:
20,1 a 20,2 de gravação ocupada, aproximadamente 5,8 a 5,9 até o Present e 16,7 a 16,8 de cauda GPU.
A soma usa carimbos arredondados; a subtração direta J-U é a referência. Em ambos, a cadeia fica em **42,3 ms**.
Os 10,6 ms da seção 29 eram 9,6 de atraso mais 1 de margem. Agora o pior atraso sem marcas é 9,9:
**[estimado, orçamento] 10,9 ms na base ou 10,5 ms já com ambos**, mantendo 1 ms de margem.
Não usar a queda do p50 de trabalho, cerca de 1,9 ms, como se adiantasse o pior alvo em 1,9 ms.

### Quais quadros têm perfil fino, e conservação

**[Lido nos ambientes e medido nos logs]** Cada abertura g selecionou somente um quadro: 6930 ou 6960.
Há 70/91 linhas `cortes: comum`, sendo uma API e **69/90 trechos**. As duas aberturas de cada alvo têm os mesmos
metadados shader/alvo/índices e contagens API. **Não há perfil fino de 6929, 6931, 6959 ou 6961 nestes arquivos.**
Para situar os vizinhos, os seis agregados abaixo são da rodada **anterior, 27/10**, conferidos nos logs originais
`~/sessao_2710/cortes/gpu6930` e `gpu6960`, carimbo `ba63dab diff:4bcb7b67df`, como na seção 29.
Não são novas medições nem subclasses atribuídas por interpolação; a lacuna fina dos quatro vizinhos permanece.

| Quadro, 27/10 | GPU bruta | Custo | Líquido calibrado | Comum líquido (ms) |
|---|---:|---:|---:|---:|
| 6929 | 18,699 | 2,498 | 16,201 | 9,008 |
| 6930 | 18,356 | 2,692 | 15,664 | 8,719 |
| 6931 | 18,429 | 2,679 | 15,750 | 8,841 |
| 6959 | 19,005 | 2,476 | 16,528 | 8,538 |
| 6960 | 25,983 | 3,421 | 22,562 | 12,123 |
| 6961 | 26,024 | 3,587 | 22,437 | 12,158 |

**[Medido, 28/10]** Bruto e custo do trecho vazio abaixo vêm do Xenos. **[Estimado, calibrado]** Líquido é bruto
menos esse custo por segmento. As marcas IDLE drenam o pipeline; descontá-las não reconstitui uma execução sem
marcas nem dá automaticamente o ganho na cadeia. O limite de espera CPU é 0 em todas as classes e trechos;
isso não exclui esperas nas APIs ou dependências da GPU. O preparo do desenho seguinte ainda pode cair no trecho
anterior, pois a fronteira foi preservada. Nenhuma coluna é tempo puro do shader.

| Abertura | Bruto | Marcas | Custo | Líquido | Comum líquido | CPU frame_end (ms) |
|---|---|---|---|---|---|---|
| g6930_1 | 18,935 | 188 | 3,093 | 15,842 | 8,842 | 6,809 |
| g6930_2 | 18,884 | 188 | 3,179 | 15,705 | 8,814 | 6,791 |
| g6960_1 | 26,545 | 240 | 3,738 | 22,807 | 12,300 | 6,066 |
| g6960_2 | 26,568 | 240 | 3,748 | 22,821 | 12,312 | 6,129 |

Conservação conferida a 0,00005 ms, antes de arredondar as tabelas: soma bruta das 27 classes = total;
soma líquida = total menos custo; soma bruto menos líquido = custo; soma das marcas de classe = marcas totais
menos uma. Cada classe conserva também bruto menos quantidade de segmentos vezes trecho vazio = líquido.
Os 69/90 trechos recompõem integralmente as oito subclasses comuns, em bruto, líquido, espera e marcas.
A classe `comum` antiga é zero: as subclasses a substituem. Não se somam detalhes e agregados de novo.
No comum, bruto/custo/líquido são 9,984/1,141/8,842 e 9,987/1,173/8,814 no 6930;
13,708/1,408/12,300 e 13,723/1,411/12,312 no 6960.

Aberturas 1 contra 2: no 6930, bruto muda -0,052 ms, custo +0,086 e líquido -0,138;
no 6960, +0,023/+0,010/+0,014. O comum líquido varia somente 0,028 e 0,012 ms.
Variação de custo de marca não é ganho de otimização.

As classes não nulas seguem em **bruto / custo / líquido, ms**, na ordem das duas aberturas de cada alvo.
`comum`, VU6, filme, comum VRAM, comum pacote e comum trivial são zero nas quatro.

| Classe | g6930_1 | g6930_2 | g6960_1 | g6960_2 |
|---|---|---|---|---|
| outros | 0,018 / 0,017 / 0,002 | 0,018 / 0,017 / 0,001 | 0,018 / 0,016 / 0,003 | 0,018 / 0,016 / 0,002 |
| vu0 | 0,113 / 0,017 / 0,096 | 0,111 / 0,017 / 0,094 | 0,876 / 0,047 / 0,829 | 0,877 / 0,047 / 0,830 |
| vu4 | 2,038 / 0,281 / 1,757 | 2,044 / 0,289 / 1,755 | 3,015 / 0,203 / 2,812 | 3,014 / 0,204 / 2,810 |
| profundidade | 0,503 / 0,050 / 0,454 | 0,492 / 0,051 / 0,441 | 0,495 / 0,047 / 0,448 | 0,499 / 0,047 / 0,452 |
| contorno | 0,000 / 0,000 / 0,000 | 0,000 / 0,000 / 0,000 | 0,708 / 0,016 / 0,692 | 0,708 / 0,016 / 0,693 |
| tabela pelos ids | 0,000 / 0,000 / 0,000 | 0,000 / 0,000 / 0,000 | 0,436 / 0,031 / 0,405 | 0,434 / 0,031 / 0,403 |
| tabela pela profundidade | 1,062 / 0,066 / 0,996 | 1,063 / 0,068 / 0,995 | 1,061 / 0,063 / 0,998 | 1,061 / 0,063 / 0,998 |
| cópia dos ids | 0,000 / 0,000 / 0,000 | 0,000 / 0,000 / 0,000 | 0,184 / 0,016 / 0,168 | 0,186 / 0,016 / 0,170 |
| FBA | 1,560 / 0,496 / 1,063 | 1,555 / 0,510 / 1,045 | 1,535 / 0,407 / 1,128 | 1,536 / 0,408 / 1,128 |
| DATE | 0,409 / 0,182 / 0,228 | 0,402 / 0,187 / 0,215 | 0,400 / 0,172 / 0,228 | 0,402 / 0,172 / 0,229 |
| resolves de cor | 1,204 / 0,347 / 0,857 | 1,178 / 0,357 / 0,821 | 1,750 / 0,563 / 1,187 | 1,747 / 0,564 / 1,182 |
| entradas na EDRAM | 0,490 / 0,364 / 0,126 | 0,487 / 0,374 / 0,113 | 0,812 / 0,626 / 0,186 | 0,814 / 0,627 / 0,187 |
| volta da cor | 0,396 / 0,083 / 0,313 | 0,394 / 0,085 / 0,309 | 0,394 / 0,078 / 0,316 | 0,394 / 0,078 / 0,315 |
| tela | 0,343 / 0,017 / 0,326 | 0,339 / 0,017 / 0,322 | 0,339 / 0,016 / 0,324 | 0,339 / 0,016 / 0,323 |
| FXAA | 0,800 / 0,017 / 0,784 | 0,796 / 0,017 / 0,779 | 0,800 / 0,016 / 0,785 | 0,801 / 0,016 / 0,785 |
| medição | 0,017 / 0,017 / 0,000 | 0,017 / 0,017 / 0,000 | 0,016 / 0,016 / 0,000 | 0,016 / 0,016 / 0,000 |
| comum sem | 3,649 / 0,182 / 3,467 | 3,655 / 0,187 / 3,468 | 6,008 / 0,250 / 5,757 | 6,013 / 0,251 / 5,762 |
| comum alvo | 2,725 / 0,347 / 2,378 | 2,727 / 0,357 / 2,370 | 4,022 / 0,579 / 3,443 | 4,038 / 0,580 / 3,457 |
| comum afiado | 1,571 / 0,364 / 1,207 | 1,574 / 0,374 / 1,200 | 1,529 / 0,344 / 1,185 | 1,523 / 0,345 / 1,178 |
| comum rapido sem | 0,345 / 0,215 / 0,130 | 0,338 / 0,221 / 0,117 | 0,344 / 0,203 / 0,140 | 0,345 / 0,204 / 0,141 |
| comum rapido textura | 1,693 / 0,033 / 1,660 | 1,693 / 0,034 / 1,659 | 1,805 / 0,031 / 1,774 | 1,805 / 0,031 / 1,774 |

**[Medido, efeito sobre a tela]** g6930_1/2 mantêm 6929 → 6930, pioram sua folga para -6,6/-6,5 ms e acrescentam
**6931 → 6932**, -0,6 ms nas duas. Também conservam 6959 → 6960, -10,0/-9,9. J2 fica com três longas,
histograma por imagem 5/2327/3/0. g6960_1/2 mantêm as duas longas: 6929 → 6930 a -2,8 e 6959 → 6960 a
**-14,7 ms**, histograma 4/2329/2/0. J1 continua limpa em todas. Só marcar o alvo evitou o deslocamento para seu
antecessor visto em 27/10, mas não tornou a instrumentação neutra. O atraso adicional de ~3,8 a 3,9/~4,8 a 4,9 ms
contra a base excede o desconto vazio de ~3,1/~3,7 ms: fase e esvaziamento residual também importam.

### Shader, alvo e faixa absoluta dos desenhos

**[Medido, líquido calibrado]** A faixa abaixo agrupa índices absolutos em blocos de 128, não desenhos novos nem
128 chamadas por linha. A quantidade é de segmentos, não de desenhos (94/116 comuns nas listas da seção 30).
Nenhum grupo com tempo foi omitido. Os **318 trechos individuais**, com início:fim efetivo, bruto, custo, líquido,
espera e marca, estão em `~/f5-gpu-fina/trechos.csv` e `rodada/dados.json`; não há pixels no git.
Os nomes de classe são os literais do log: `sem` usa `ps_gs`, `alvo` usa `ps_gs` lendo alvo, `afiado` usa
`ps_gs_af`, `rapido sem/textura` usa a especialização existente. O alvo 0 é o quadro; 3/4/5/6 são os intermediários.

**Quadro 6930.** Líquido em ms, aberturas 1 / 2.

| Classe comum | Alvo | Faixa | Trechos | Líquido 1 / 2 |
|---|---|---|---|---|
| alvo | 0 | 0..127 | 1 | 0,462 / 0,462 |
| rapido textura | 0 | 0..127 | 1 | 0,216 / 0,215 |
| sem | 0 | 0..127 | 3 | 1,474 / 1,476 |
| alvo | 3 | 0..127 | 2 | 0,300 / 0,300 |
| sem | 3 | 0..127 | 1 | 0,140 / 0,139 |
| alvo | 4 | 0..127 | 2 | 0,084 / 0,084 |
| alvo | 5 | 0..127 | 6 | 0,096 / 0,093 |
| alvo | 6 | 0..127 | 5 | 0,071 / 0,068 |
| afiado | 0 | 128..255 | 22 | 1,207 / 1,200 |
| alvo | 0 | 128..255 | 2 | 0,920 / 0,919 |
| rapido sem | 0 | 128..255 | 13 | 0,130 / 0,117 |
| rapido textura | 0 | 128..255 | 1 | 1,445 / 1,444 |
| sem | 0 | 128..255 | 7 | 1,853 / 1,853 |
| alvo | 3 | 128..255 | 2 | 0,324 / 0,322 |
| alvo | 4 | 128..255 | 1 | 0,123 / 0,122 |

**Quadro 6960.** Líquido em ms, aberturas 1 / 2.

| Classe comum | Alvo | Faixa | Trechos | Líquido 1 / 2 |
|---|---|---|---|---|
| alvo | 0 | 0..127 | 1 | 0,464 / 0,463 |
| rapido textura | 0 | 0..127 | 1 | 0,212 / 0,213 |
| sem | 0 | 0..127 | 3 | 1,477 / 1,481 |
| alvo | 3 | 0..127 | 3 | 0,465 / 0,463 |
| sem | 3 | 0..127 | 1 | 0,141 / 0,142 |
| alvo | 4 | 0..127 | 2 | 0,086 / 0,085 |
| alvo | 5 | 0..127 | 6 | 0,101 / 0,100 |
| alvo | 6 | 0..127 | 5 | 0,075 / 0,075 |
| alvo | 0 | 128..255 | 2 | 0,922 / 0,925 |
| sem | 0 | 128..255 | 1 | 0,499 / 0,502 |
| alvo | 3 | 128..255 | 1 | 0,164 / 0,165 |
| alvo | 4 | 128..255 | 1 | 0,126 / 0,130 |
| alvo | 0 | 1152..1279 | 1 | 0,465 / 0,466 |
| sem | 0 | 1152..1279 | 2 | 1,489 / 1,490 |
| alvo | 3 | 1152..1279 | 2 | 0,307 / 0,308 |
| sem | 3 | 1152..1279 | 2 | 0,292 / 0,291 |
| alvo | 4 | 1152..1279 | 2 | 0,088 / 0,090 |
| alvo | 5 | 1152..1279 | 6 | 0,105 / 0,106 |
| alvo | 6 | 1152..1279 | 5 | 0,074 / 0,082 |
| afiado | 0 | 1280..1407 | 22 | 1,185 / 1,178 |
| rapido sem | 0 | 1280..1407 | 13 | 0,140 / 0,141 |
| rapido textura | 0 | 1280..1407 | 1 | 1,562 / 1,561 |
| sem | 0 | 1280..1407 | 7 | 1,860 / 1,858 |

O peso real no 6960 é **sem textura geral 5,757 a 5,762 ms**, cerca de 47% do comum, seguido de
**alvo 3,443 a 3,457**, cerca de 28%; rápidos com textura 1,774, afiado 1,178 a 1,185 e rápidos sem textura
0,140 a 0,141. Dentro de sem textura, **5,324 a 5,330 ms estão no alvo 0**. Não é o HUD afiado que explica o salto.
Do 6930 ao 6960, o comum cresce 3,458/3,498 ms: sem textura +2,290/+2,294 e alvos +1,065/+1,087,
com rápidos com textura +0,114 e afiado ligeiramente menor. A GPU líquida inteira cresce 6,965/7,116:
VU4 acrescenta ~1,055, VU0 ~0,734 e contorno/tabela pelos ids/cópia dos ids ~1,265, além dos demais deltas.
Isso compara os alvos, não transforma a ausência de subclasses do 6959 em nova medida.

**[Medido]** No 6960, a primeira cadeia do brilho é 109..125 e a segunda **1261..1277**, com geometria VU entre elas.
O líquido comum é **1,170/1,167 ms na primeira** e **1,180/1,193 na segunda**: juntas 2,349/2,360.
No 6930 há uma, **111..127**, 1,152/1,146 ms. Os alvos passam 3 → 4 → 5/6, alternam dez vezes e voltam a 0.
A segunda acrescenta um percurso do brilho, não duplica o HUD nem todo o comum. Seu custo não inclui as classes
separadas Resolve, entrada/volta EDRAM, FBA ou DATE. **[Lido, seção 30]** As duas cadeias leem versões diferentes;
retirar uma não é eliminação de cópia redundante. Na x86 o segundo percurso já aparece no 6959, no console
anterior a mudança está no 6960: a fase entre arquiteturas continua sendo uma limitação.

**[Medido]** Chamadas efetivas, iguais nas duas repetições, de todo o quadro:

| Quadro | Resolve | Clear | Pixel shader | Alvo | Textura | Tesoura | UP | Índices |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 6930 | 36 | 4 | 79 | 31 | 284 | 24 | 121 | 119 |
| 6960 | 53 | 4 | 103 | 53 | 1.471 | 45 | 146 | 1.284 |

O salto principal de chamadas é da geometria VU intercalada e das mudanças de textura/índices, não um dobro
uniforme de todos os comandos. As 36/53 chamadas Resolve são contagem, não tempo removível; os 21/36 segmentos
de Resolve de cor do perfil antigo e do novo não eram número de chamadas. **[Medido, CPU instrumentada]** O
`frame_end` mantém `outros` 5,380 a 5,384 ms no 6930 e VU0 **4,167 a 4,238 ms** no 6960, contra GPU VU0 ~0,83.
O perfil não abriu as chamadas/esperas internas desses dois intervalos. Não atribuir esses ms a shader nem somá-los
a GPU inteira como se CPU e GPU não se sobrepusessem.

### Cada candidato da seção 30 contra a medida

A coluna ganho é economia **demonstrada**: localizar custo não demonstra que seja removível. As faixas continuam
hipóteses de investigação e não são independentes. Os tempos abaixo são líquidos calibrados do 6960.

| Candidato | Custo agora atribuível [medido, calibrado] | Ganho demonstrado na cadeia | Hipótese restante [estimado] e impedimento [lido] |
|---|---|---:|---|
| Tesoura/padding | Nenhuma área adicional eliminável no censo da seção 30; o perfil só conta 45 chamadas de tesoura | 0 ms | 0 ms sustentado; conservar a região lida e os halos de filtro, nas duas escalas |
| Desenhos cobertos | Todo o comum pesa 12,300 a 12,312; sem textura no alvo 0 pesa 5,324 a 5,330 | 0 ms | Nenhuma cobertura opaca descartável provada. Mistura, alfa, DATE, Z e versões lidas impedem apagar a sobreposição |
| Especializar `ps_gs` e cópias de alvos | Sem textura 5,757 a 5,762 e alvos 3,443 a 3,457, soma ~9,21; afiado só ~1,18 | 0 ms | 0 a 3 ms como antes; 9,21 é custo das classes inteiras, não ganho. Predicado atual exclui arte/testes; preservar DATE, alfa, FBA, UV e gradientes. Não usar trivial ou desligar afiado |
| Alcance do brilho/cópias | Duas cadeias comuns 2,349 a 2,360; segunda 1,180 a 1,193 | 0 ms | 0 a 2 ms era hipótese; nada mostra região vazia que a sustente. Mesmo apagar as duas integralmente não tiraria 10,6 ms; há conteúdo diferente e halo |
| Resolve/limpeza redundante | Cor 1,182 a 1,187, entradas EDRAM 0,186 a 0,187; CPU limpeza ~0,004, quatro Clear | 0 ms | 0 a 0,5 ms continua sem chamada redundante identificada. 1,37 ms é teto irreal de retirar toda cor/entrada, necessárias à residência; não é economia dos 53 Resolve |
| Trabalho no quadro anterior | Nenhuma textura nova; sem partição nova de dependências ou custo antecipável | 0 ms | 0 ms creditado. A geometria e a segunda cadeia usam o quadro atual; não mover o desenho sem provar dependências, fase e entrada |

**[Estimado, teste do orçamento]** Mesmo somando os máximos antigos de shader 3 + área 2 + redundâncias 0,5,
são **5,5 ms**, com sobreposição nas mesmas cópias e sem prova de regiões descartáveis. Somar o efeito observado
~0,4 de ambos dá **5,9 ms**, ainda **4,7 abaixo dos 10,6**, ou **5,0 abaixo dos 10,9 atualizados**.
Esse é um exercício otimista, não uma promessa de 5,9: nenhum dos novos candidatos tem ganho medido positivo.
Não há soma realista demonstrada que feche a cadeia. Nem a eliminação fictícia de todo sem textura + alvo (~9,21)
resolveria sozinha o orçamento; apagaria grande parte da imagem. O perfil localiza onde investigar, mas não
sustenta um corte pequeno concreto com imagem idêntica. Por isso não se implementou protótipo nesta entrega.

### As escolhas que voltam ao Lucas, decisões 27.3 e 28.1

A opção conservadora aplicada foi manter zero, imagem e latência, com F5 pendente e sem aprovar F7.
A análise não escolhe uma concessão por ele. Estas são alternativas para uma decisão explícita, com seus limites:

| Opção para o plano | Números e consequência |
|---|---|
| Manter todos os requisitos e reservar uma etapa maior para reprojetar a cadeia | Recuperar **10,9 ms na base ou 10,5 com ambos**, não apenas shader: gravação atual ~20,2 ms, `frame_end` ~5,9 e cauda ~16,7. Só CPU, mantendo o restante, exigiria adiantar ~10,9 ms, cerca de 54% da gravação ocupada; só GPU exigiria tirar ~65% da cauda. São metas, não ganhos possíveis já provados. A prioridade e o alcance maior voltam ao Lucas; os cortes pequenos desta nota não fecham F5 |
| Autorizar menos resolução interna | De 4/3 para 1 há **43,75% menos pixels**, conta geométrica. Se todo o comum escalasse com pixels, hipótese otimista daria ~5,38 ms; se toda a GPU líquida escalasse, ~9,98 ms. Não cobre sozinho os 10,9 e não prevê a cauda: há custos fixos/CPU. Muda imagem e exige nova medida; não foi aplicado |
| Autorizar menos efeitos | Omitir a segunda cadeia apagaria conteúdo e retiraria no máximo ~1,19 ms dos trechos comuns medidos; FXAA inteiro pesa ~0,785. Somados ficticiamente, ~1,98 ms, longe de 10,9; Resolve associado e efeito na cadeia não foram isolados. Desligar afiado também muda imagem, e seus ~1,18 ms inteiros não são ganho atribuível à troca de filtro. Não foi aplicado |
| Autorizar um vblank adicional de fila/prazo | **+16,68 ms** excede o atraso observado de 9,9 mais 1 de margem em ~5,8 ms, na aritmética desta amostra. Acrescenta latência, vedada nas decisões atuais; precisa decisão e prova da entrada/fase, não certifica o resto do jogo |
| Alterar o aceite de apresentação | Os seis controles têm **2/2.334 passos**, cerca de 0,086%, com imagens de três vblanks (~50 ms em vez de ~33), zero de quatro ou mais nesta amostra. Aceitar isso mudaria o zero; não prova limite nas três janelas longas nem no dividido. Nenhum percentual foi adotado |

**[Lido]** A decisão 28.1 leva o mesmo zero para duas vistas, sem perda de imagem ou atraso de entrada.
**[Não medido]** Nenhum log desta tarefa é de tela dividida; não dobrar os 22,8 ms como previsão da F7 nem dizer
que ela passou ou é impossível. O resultado que vale para ambas é que as concessões acima exigem a decisão do Lucas,
e que um orçamento só de shader/brilho não demonstrou fechar nem a vista única atual.

Se ele mantiver todos os requisitos, a tarefa seguinte deve localizar as chamadas/esperas de `outros`/VU0 da CPU
e o envio/hash/VIF/GIF antes de escolher um reprojeto. Para completar especificamente a lacuna fina dos vizinhos,
preparar outro identificador, por exemplo **`gf29b`**: controles base1/ambos1 a base3/ambos3 sem marcas e diagnósticos
**um quadro por abertura**, para Q em 6929,6930,6931,6959,6960,6961, duas repetições R=1/2, constantes/fecho 0/0,
`BT3_COMUM_GPU=Q,Q`, demais ambientes como nesta rodada. Saves novos `sv_gf29b_baseI`, `sv_gf29b_ambosI` e
`sv_gf29b_qQ_R`, todos únicos e inicialmente vazios; se o identificador já existir, escolher outro e não reutilizar.
Essa rodada é condicionada à escolha do plano, não foi disparada nem substitui as três janelas fria/aquecidas.

Para qualquer corte futuro, manter desligado por padrão e cumprir a seção 28: listas normais x86gv/PPCg
(`SEMMAPA=1`, nunca `PPC=1`) nos dois roteiros e modos de threads, `divergencias.py`, testes do predicado e mutantes,
objetos planos sem depuração por `objcopy --strip-debug`/`cmp`, SHA1 da `.text`, `roteiros.sh` e `save_mc.sh`.
Listas iguais no backend falso não provam shader D3D: exigir referência dos estados elegíveis, capturas de todos
os intermediários e final iguais no console, inclusive halos/bordas e ambas as escalas. Só depois comparar o prazo
sem marcas, com saves novos, sem fila extra e com entrada verificada. Tudo que exige execução fica para a próxima
tarefa autorizada; não houve emulador, build, FTP nem console nesta análise.

### Conferência final desta análise [medido]

Artefatos em `~/f5-gpu-fina/`: extrator `confere.py`, `originais.sha256`, `rodada/dados.json`, 318 trechos em CSV,
logs até o fim e saídas dos verificadores. `tabelas.py` e `nota31.py` recompõem as tabelas desta nota.
O extrator foi visto sair com **1 em quinze cópias estragadas**: fim retirado, associação ausente, imagem omitida,
duração trocada, vida diferente, save trocado, carimbo diferente, classe GPU retirada, custo e bruto adulterados,
trecho retirado, líquido adulterado, faixa fora de ordem, API retirada e quadro de trecho trocado.
Os originais permaneceram intocados. A tolerância de conservação é 0,00005 ms para a impressão decimal;
nenhum líquido negativo seria truncado. Os SHA256 são dos bytes dos logs originais, não do texto normalizado.

| Rodada final | Resultado |
|---|---|
| Extrator, dez logs e quinze negativos | originais: saída 0, tudo certo na extração; quinze cópias: saída 1 |
| `passos.py` estrutural com carimbo real, dez recortes | dez saídas 0 |
| `passos.py LOG --console --janelas 2 --minimo 600 --carimbo 'c57e807 diff:da39a3ee5e'`, seis controles originais | seis saídas **1**: j1 passa, j2 tem duas longas; zero reprovado |
| Mesmo aceite nos quatro diagnósticos até o fim | quatro saídas 1, longas j2 em 3/2/3/2; não são controles de apresentação |
| Quatro perfis finos, classes e 318 trechos | bruto/custo/líquido/marcas conservados; nenhuma amostra indisponível; metadados/API iguais entre repetições |
| Alterações de execução, novas provas de x86/PPC e objetos | nenhuma; só documentação mudou; provas antigas não são atribuídas a este branch |
| `.text` x86 preexistente, extraída novamente | SHA1 `43444ea4d03a6d448aa1ae74a631e35b356a7caa` |

A `.text` acima foi extraída de `~/f5-gpu-comum/base/port/build/x86/bt3_x86`, carimbo
`4c85c039 diff:da39a3ee5e`, para `~/f5-gpu-fina/x86-preexistente.text`. Não é build nova desta worktree,
que não tem `port/build`; nenhum fonte ou objeto foi alterado. Os verificadores são os do commit final,
e analisam as builds do console identificadas pelo carimbo real, não uma build fictícia do branch documental.
Não ficaram provados: equivalência ou ganho de um corte novo, subclasses dos quatro vizinhos, zero nas três
janelas longas, sessão livre com cargas ou desempenho da tela dividida. As seis reprovações oficiais fazem parte
da entrega; **não se declara a F5 pronta**.

## 32. Reprojeto da cadeia: mapa, orçamento e próxima medida (28/10/2026)

Branch `codex/f5-reprojeto`, base `bbc07118`. Decisão 29, opção A: manter zero, imagem e entrada,
sem regressão; favorecer a resolução maior. **Recomendação:** medir as lacunas de CPU e preparar o envio
progressivo de prefixos imutáveis do mesmo quadro, combinado com retirada comprovada de trabalho repetido.
Mais trabalhadores de gravação ficam como alternativa posterior. Não há ganho novo demonstrado nem autorização
para uma exceção. Esta primeira parte entrega o mapa, o projeto, o diagnóstico desligado e os ambientes.

### Mapa do caminho crítico, sem confundir quadros ou relógios

**[Medido]** Reextração nesta worktree dos quinze logs originais de `~/sessao_2810/gpu/` e
`~/sessao_2710/cortes/`, parando no primeiro `f5_duelo.txt:99 fim`. Carimbos literais:
`c57e807 diff:da39a3ee5e` em 28/10 e `ba63dab diff:4bcb7b67df` em 27/10. Não são builds deste branch.
As seções 29 e 31 conservam as tabelas completas; dados e verificações novos ficam em `~/f5-reprojeto/`.

**[Lido]** O caminho atual é este. Os nomes se referem aos fontes desta base, não às linhas do estudo antigo.

| Ordem e dono | Trabalho e dependência | Evidência e limite |
|---|---|---|
| Jogo, hardware 2 | `Port_PadRead`/roteiro, lógica de `Battle_Loop`, `Battle_Draw`, montagem de DMA e REF/CALL; `Gfx_EndFrame(2)` espera GS e vblank; `Dma_Flush` chega a `Port_GsVif1Chain` | [lido] `gs_input.c`, `src/battle/battle.c`, `src/sys/gfx.c`, `src/sys/dma.c`. O controle usa o contador do jogo; não mover sua leitura nem usar o progresso de trabalhadores para escolher botões |
| Jogo entrega J | Espera a lista anterior, copia REF pequenos (`ref_copia`), captura FBP e número do quadro, `Picos_Jogo`, `render_post`; avança `gGsFrameJogo` | [lido] `gs_core.c`. J é entrega, não instante do botão. Os logs não medem a latência física botão/fóton nem repartem a lógica do pior alvo |
| Gravação, hardware 0 | `render_thread` pega o trabalho; `run_chain` percorre DMA, `vif` mantém DIRECT/UNPACK pendentes, `GsVu1_Unpack` escreve a memória VU; MSCAL/MSCNT chamam `GsVu1_Call` | [lido] `gs_core.c`, `gs_vu1.c`. VIF, GIF, estado GS e VU são máquinas sequenciais, com estado que atravessa blocos |
| Gravação, HLE e GS | `hle_program*` lê a VU, também chama GIF; `GsGpu_DrawVu0/4/6` resolve estado/textura, uniformes, vértices e índices. `Gs_Gif` interpreta PACKED/REGLIST/IMAGE, registra primitivas CPU ou envia imagem para a VRAM GS | [lido] VU0 neste ponto é a família de lutadores gerada pela VU1, não prova de um interpretador separado de VU0 consumindo os ms do desenho |
| Gravação, envio/hash | `transfer_data` escreve a VRAM de 4 MiB, inclusive paletas e formatos parciais; invalida gerações. `Gs_PageHash` já memoriza por geração. `texture_get` usa hashes e cache pela origem, decodifica e cria via XG apenas em falta | [lido] `gs_core.c`/`gs_draw.c`. Envios do jogo à VRAM GS são CPU, diferentes de envio de comandos D3D à GPU. Nos alvos há zero textura nova, não zero transferência |
| Entrega G | `GsVu1_FrameEnd`, `GsGpu_FrameEnd`, `desenho_entrega`: espera retrato anterior, alterna ponteiros de listas, copia alvos/metadados, `entrega` faz `vu_plano`, recursos/profundidades, versões e memória; publica retrato | [lido] `gs_draw.c`, `gs_d3d.cpp`. Listas alternadas e VU1 direto em três jogos já existem. `__sync` publica escritas combinadas; liberações seguem a fila por quadro e a cova |
| Desenho, hardware 3 | `desenho_thread`, único dono D3D, consome retrato; `frame_end`: estatísticas, `alfa_alto` periódico, escolha dos buffers, `ed_plano`, comandos em ordem: DATE/nativas, residência/Resolve/Z, `prepara`, uniformes, índices/UP e FBA | [lido] `gs_d3d.cpp`. A execução da GPU já se sobrepõe a esse envio. `usa_z`, `date_area`, `ed_plano` e `vu_plano` consultam a lista do quadro, obstáculo real ao envio precoce |
| Fim do desenho P | Descarrega alvos e profundidade, FXAA, cópia para tela, callback de fim da GPU, `Present`, limpeza/cova; a thread depois libera o retrato | [lido] A marca GPU é posta antes de `Present`; seu carimbo indica quando os comandos anteriores terminaram. `Present` retornar não demonstra imagem pronta |
| GPU U e tela | GPU termina comandos, swap entra na callback de vblank; o prazo é o segundo vblank após a entrada da troca anterior | [medido/lido] `picos_*`, `tela_*`, `vblank_cb`. Em 6959 → 6960 não houve repetição: o prazo tem associação íntegra; a ressalva do Blast 2 da seção 27 não se aplica |

**[Lido]** As esperas existentes são parte do contrato: jogo pela gravação em `sceGsSyncPath`, reset/empréstimo
de buffers, leitura de disco e guarda de estado; gravação sem trabalho até o jogo entregar; gravação pelo retrato
anterior e por memória liberada pelo desenho; rodízio VU1 pela callback da GPU; desenho pelo retrato e APIs D3D;
GPU pelas dependências de alvos/comandos; tela pelo vblank. Não retirar nenhuma como se fosse trabalho redundante.
O áudio ocupa 4/5, o diário a 1; pares SMT são 0/1, 2/3 e 4/5. A thread 1 disputa o núcleo da gravação,
e 4/5 não são trabalhadores disponíveis sem medir áudio junto. O teste antigo na 1 piorou contra a 3 (seção do degrau 2).

**[Medido, base de 28/10]** A é a imagem retida; B é o próximo quadro, cujos tempos aparecem.
Tempos em ms, carimbos relativos a J=0; arredondamento de décimos impede somar trabalho como relógio exato.

| Par A → B | J | G | P | U | Prazo | Gravação ocupada / frame_end |
|---|---:|---:|---:|---:|---:|---|
| 6929 → 6930, base1/2/3 | 0 | 14,6 | 21,3 / 21,3 / 21,3 | 35,6 / 35,6 / 35,7 | 32,9 | 14,7 / 6,7 |
| 6959 → 6960, base1 | 0 | 20,1 | 25,9 | 42,6 | 32,8 | 20,1 / 5,8 |
| 6959 → 6960, base2 | 0 | 20,1 | 25,9 | 42,7 | 32,8 | 20,2 / 5,8 |
| 6959 → 6960, base3 | 0 | 20,1 | 26,0 | 42,7 | 32,8 | 20,2 / 5,9 |

```text
Tempo relativo a J do quadro 6960, base3 [medido nos carimbos arredondados]
0                    20,1       26,0      32,8                  42,7 ms
J----------------------G----------P---------prazo------------------U
gravação 6960           frame_end   cauda da GPU até a marca
jogo 6961 pode avançar  GPU já executa durante frame_end
```

**[Medido]** O período do jogo é 33,4 ms, não 33,4 ms de lógica somáveis à linha acima. Lógica seguinte,
gravação e desenho de quadros diferentes podem se sobrepor; no mesmo quadro o desenho espera G integralmente,
e só a partir dos comandos a GPU executa. Não há medida do instante do primeiro comando útil na base.
As esperas por disco, XG, descarte, memória/desenho e cova são zero nos pares de picos de 27/10 dos alvos;
isso não mede bloqueios internos do D3D nem permite atribuir zeros individuais a cada vizinho sem linha de picos.

### Onde estão os 10,9 ms e o que ainda não tem nome

**[Medido]** A conta de prazo é `J-U`: 42,7 ms de cadeia para 32,8 disponíveis, atraso de 9,9.
**[Estimado, requisito de margem]** É preciso terminar até J+31,8: recuperar 10,9 ms.
Isso é um déficit do caminho crítico, não uma parcela escondida denominada `outros`, nem GPU total acima de 33 ms.
O ganho demonstrado de constantes 2/fecho 1 é 0,3 a 0,4 ms, deixando 10,5 de meta sobre ambos.

**[Medido, 27/10, gravação instrumentada do 6960]** O total exclusivo 21,741600 ms se reparte assim:

| Grupo | ms | Interpretação [lido] |
|---|---:|---|
| DMA / VIF / GIF / primitivas CPU | 0,240 / 1,978 / 2,072 / 2,371 | soma 6,660, sem seus filhos; inclui parser, estado, vértices CPU e `draw_state` |
| VU1 restante / constantes / fecho | 4,517 / 1,259 / 1,338 | soma 7,114; não somar o tempo inclusivo da família VU1 novamente |
| Unpack | 1,824 | memória da VU e conversões, após o caminho que já evita a cópia intermediária |
| Envio / hash | 3,496 / 2,237 | soma 5,733; 245 envios, 2.290 KB e 293 recálculos nos picos. As 404 chamadas de hash no rateio incluem acertos |
| Entrega | 0,410 | gancho, metadados e eventuais esperas, sem duplicar o tempo de outra thread |

O total tem custo de relógio por chamada; não é uma decomposição numérica dos 20,2 ms sem marcas de 28/10.
Eliminar ficticiamente todo envio/hash instrumentado não alcança 10,9; comparar origem/tamanho sem comprovar
bytes não autoriza pular escrita. Formatos 16/24 bits e paleta parcial podem preservar partes da palavra anterior.
**[Não medido]** No console não há rateio do envio por formato, acerto/invalidação, escrita realmente igual,
nem duração da parte da VU1 que seria paralelizável após resolver texturas. O novo orçamento não lhes credita ganho.

**[Medido, CPU nos perfis GPU]** Em 28/10, `outros` no 6930 pesa 5,380 a 5,384 ms; VU0 no 6960,
4,167 a 4,238. Em 27/10 eram 5,416 e 3,859. São intervalos entre trocas de classe, incluindo preparo do desenho
seguinte e possíveis bloqueios. A GPU VU0 fina pesa apenas 0,829 a 0,830 ms; não subtrair esses números para
anunciar uma espera medida. **[Lido/hipótese]** `alfa_alto` percorre índices e vértices para estatística quando
`sConta.quadros == 30`, antes de `tempo_abre`. É um candidato específico para `outros` do 6930, compatível com a
periodicidade, ainda sem medida própria. Não explica por si os 10,9 do 6960. Na VU0, `prepara`, estado/sampler,
uniformes, índices e APIs precisam ser separados. O perfil implementado abaixo resolve essa atribuição na próxima rodada.

**[Medido, GPU com desconto calibrado de 28/10]** Total do 6960 22,807 a 22,821 ms; comum 12,300 a 12,312,
com sem textura 5,757 a 5,762 e alvos 3,443 a 3,457. As duas cadeias de brilho comuns somam ~2,35 e leem versões
diferentes. Isso localiza trabalho, não ganho: o desconto das marcas IDLE não desfaz a drenagem ou a fase.
A cauda sem marcas de 16,7 a 16,8 ms já exclui a parte da GPU sobreposta ao desenho; não somar 22,8 à cauda.

### Caminhos de arquitetura e suas provas

Todos os ganhos abaixo são **[estimado]**, relativos ao pior alvo e sem crédito de ganho novo até medir U
sem marcas. Uma faixa inclui zero e pode piorar sob disputa; tetos idealizados não são previsões.
Para todos valem as provas comuns da subseção seguinte, incluindo entrada, fase, determinismo e memória.

| Caminho | Mudança concreta | Orçamento no 6960 e evidência distinta | Risco, prova específica e resolução maior |
|---|---|---|---|
| A. Prefixos do mesmo quadro chegam ao desenho antes de G | Gravação publica apenas desenhos fechados, estado/recursos imutáveis e contagens por lote; desenho único emite comandos em ordem enquanto a gravação continua. Um único Present final | É o único caminho que pode esconder uma parte grande dos ~20,2 ms à GPU. Meta de 6 a 12 ms na cadeia ao esconder cerca de 30% a 60% da gravação; faixa ainda possível 0 a 12, dependente do primeiro prefixo, pausas e custo da fila; ganho medido 0. Modelo ideal abaixo, não promessa de 12 | Alto: consultas ao futuro (`ed_plano`, `usa_z`, `date_area`), `cleared`, reutilização de vaga e filas. Provar versão de cada alvo e leitor, ordem, cercas e imagens de todos intermediários. Ajuda a resolução maior ao começar GPU antes; não reduz os pixels e pode perder residência/Resolve, a medir |
| B. Trabalhadores só na expansão de lotes VU | O leitor sequencial resolve GS/VRAM/textura e faz retrato da memória VU lida; trabalhador preenche faixa exclusiva, junção estável na ordem original. Dispositivo permanece em um dono | Todo VU restante/constantes/fecho instrumentado soma 7,114. Dividir isso perfeitamente em dois pouparia no máximo ~3,56, antes de snapshots/junção/SMT; faixa de investigação 0 a 3 ms na cadeia. Dividir os 20,2 inteiros em dois idealmente pouparia só 10,1, ainda insuficiente | Alto: ponteiros para VU reescrita, floats, contadores/uniões e cache global `draw_state_vu`; não executar esses mutadores nos trabalhadores. Copiar só entradas lidas, buffers limitados e retorno serial em falta. Provar atrasos 1/5/20 ms e junção normal idêntica. Ajuda CPU nas duas resoluções, não barateia GPU; não ocupar 4/5 sem prova do áudio |
| C. Eliminar repetição comprovada | Mesma arquitetura; deduplicar apenas envios de bytes idênticos com geração e regiões preservadas; calcular estatísticas durante uma passagem já necessária; reaproveitar estado com chave completa | Hipótese 0 a 2 ms em envio/hash e 0 a 1 no desenho do 6960; não somar a 0,4 já medido. Teto fictício envio/hash 5,733 não é ganho. `alfa_alto` pode ajudar especialmente 6930, sem valor atribuído ao 6960 | Médio: alias de origem, mudança in-place, wrap VRAM, PSM, CLUT e escritas parciais. Oráculo executa caminho completo e compara VRAM/gerações/saída, mutantes em cada campo. Preservar estatísticas quando deslocadas. Ajuda CPU e banda, em ambas as escalas |
| D. Recursos/estado da troca preparados cedo | Pré-alocar capacidades comprovadas e cabeçalhos fora do caminho crítico; manter caches de estado válidos por versão. Só antecipar dados independentes do controle atual | Ganho atribuído 0 ms: nos alvos há zero novas texturas/XG. Shaders são binários `fxc`, criados no `d3d_init`, não compilados na troca; `pipe_get` só guarda descrição. Não propor um cache de compilação que já não existe no quadro | Médio: mais memória, pressão e expulsão antecipada; a primeira utilização não pode usar conteúdo antigo. Provar teto/minimo livre e cargas frias. Pode ajudar recursos maiores, pode atrapalhar RAM; não aquecer por hardcode de 6960 |
| E. Mudar dependência GPU/CPU no fim | Retirar do início do envio somente trabalho sem efeito nos comandos; encurtar invalidações comprovadas. Antecipar comandos, preservando callback de fim após o último consumidor e Present após a imagem completa | Hipótese 0 a 2 ms na cadeia, só após localizar `outros`/VU0. Present em si é ~0,12 ms nos diagnósticos: mudar apenas sua chamada não tira a cauda de ~16,8 | Alto se publicar callback/rodízio antes do último leitor, perdendo vértices ou liberando textura viva. Provar última utilização e monotonia de sequência; capturas/intermediários e atraso de entrada. Ajuda começar GPU cedo; não aumenta vazão de pixels |
| F. Especialização exata de shaders e residência | Manter ordem/versões e especializar predicados completos; alterar plano de alvos só com prova de região lida/escrita, halos e formatos | Faixa antiga 0 a 3 ms para shader, 0 a 0,5 para redundância, sem ganho demonstrado e com sobreposição. Comum 12,3 não é removível; apagar brilho/FXAA/imagem não é candidato | Alto: precisão, mistura, DATE/FBA, Z, amostragem e bordas; oráculo shader e todas as escalas/intermediários. Maior potencial para resolução maior, mas não sustenta sozinho os 10,9 |

**[Estimado, modelo para decidir A cedo]** Com os números atuais como referência de escala, uma GPU de
22,8 ms precisaria começar trabalho útil até J+9,0 para acabar em J+31,8, se executasse sem lacunas.
Não é reconstrução do início real, pois usa perfil calibrado de outra abertura. No limite irreal de sobreposição
perfeita, `max(20,2; 5,9; 22,8) = 22,8` permitiria folga; isso só demonstra espaço aritmético para estudar A.

O teste válido usa prefixos: para cada lote i, medir `r_i` (pronto com recursos), custo CPU de submissão `c_i`
e custo GPU `g_i`; simular `C_i=max(r_i,C_anterior)+c_i`, `U_i=max(C_i,U_anterior)+g_i`.
Somar `g_i` calibrados exige ressalvar a instrumentação e repetir sem marcas; dependências/callbacks do fim
entram como lotes. A contribuição de C/E/F só vale uma vez e no cronograma combinado, nunca somando tetos.
Se o primeiro prefixo útil só ficar pronto depois de J+9,0, o modelo de 22,8 sem cortes GPU já não fecha;
se fica cedo mas a GPU espera prefixos depois, o início precoce também não basta.

### Projeto recomendado em degraus e ponto de parada

**[Lido/proposto]** Nenhuma chave de otimização abaixo foi implementada nesta primeira parte.
Cada uma nasce ausente/desligada; sem igualdade fica fora do produto.

1. **Diagnóstico**, `BT3_CADEIA_CPU` implementado abaixo, e controles sem marcas. Abrir alfa, VU0, envio/hash.
   Se `outros` for estatística, protótipo `BT3_ALFA_FORA=1` mantém o mesmo resultado da estatística numa passagem
   já necessária ou depois dos comandos, sem segurar o quadro seguinte. Conferir log além da imagem. Limite:
   uma rodada de localização e até dois dias de investigação; não gastar uma semana tentando explicar médias.
2. **Ensaio de dependências**, futuro `BT3_GS_PREFIXOS=1`, primeiro no falso: um produtor e um consumidor,
   fila limitada, descritores com quadro/índice final e faixas de dados imutáveis, snapshots de recursos e versão.
   Sem worker nem D3D concorrente. Publicar só após fechar desenho/uniforme e todas as escritas referidas;
   nunca publicar ponteiro para `vu.mem` ou `GsDraw` que o lote seguinte ainda pode juntar/modificar.
   Manter `gGsFrameJogo`, espera dos REF/CALL e o contador de entrada intactos.
3. **Envio progressivo D3D**, futuro `BT3_GS_LOTES=1`: trocar a barreira do retrato completo por lotes do mesmo
   quadro, mantendo uma thread dona e um Present. Usar plano conservador de dependências por lote para
   substituir consultas ao futuro, inclusive profundidade/DATE; plano de residência pode conservar o caminho
   integral para casos ainda não suportados. No estouro, bloquear produtor/consumidor sem perder quadro,
   sem descartar e sem publicar parcialmente um desenho. O bloqueio não pode criar fila de outro quadro.
   Renderização final, FXAA e tela só após todos os lotes. Callback `vu_cb`, cova e fila de profundidade só
   depois do último uso real, nunca ao fim da primeira fatia.
4. **Cortes C/E demonstráveis**, futuras `BT3_ENVIO_IGUAL=1` e `BT3_ESTADO_LOTES=1`, isoladamente e em combinação
   com A; retirar marcas e medir GPU até o mesmo prazo. Não mudar campos do GS, arredondamento ou ordem para
   ganhar paralelismo. B só começa se uma fração independente substancial da VU for medida e o ganho combinado
   ainda puder fechar, com medição da disputa SMT e do som. F segue como apoio, com orçamento próprio de pixels.

Fila proposta: capacidade fixa por janela de lotes, definida após censo de máximos, sem duplicar a VRAM GS ou
as listas completas. Um lote grande usa o caminho integral, sem recusa nova; pedir mais capacidade exige medir
mínimo livre, recursos vivos/pendentes e pressão de descarte, inclusive duas vistas. Os 3 a 8 MB por quadro do
estudo antigo eram estimativas de snapshots, não memória adicional já autorizada. Na resolução maior a lista
geométrica não necessariamente cresce, mas a alocação de alvos e GPU aumenta: publicar as duas contas separadas.

**Porta de decisão antes do D3D:** a prova de dependências deve registrar liberação dos prefixos com endereço,
tamanho, última escrita e consumidor, validar que nenhum plano consulta sufixo desconhecido e estimar o cronograma.
Se o ganho líquido combinado sustentado não chegar a 10,9 ms (ou 10,5 sobre ambos), ou qualquer dependência
obrigar esperar G integralmente para quase todo trabalho pesado, parar A antes da integração cara.
No primeiro protótipo de console, se dois ajustes de lote não adiantarem U de forma repetível para folga >=1 ms
nos dois alvos, recalcular o teto com as medidas; sem candidato independente para o restante, parar.
Proposta de limite: uma rodada diagnóstica, dois dias do ensaio e dois da primeira integração; se travar por
mais do dobro desses limites, registrar estado/hipóteses e parar conforme AGENTS. Não transformar esses limites
em promessa de prazo, nem declarar inviabilidade universal de reprojeto por falha de uma partição.

Ao parar, devolver ao Lucas os atrasos por quadro, folgas, longas/4+, latência, limites de memória e ganho
máximo restante, para os termos da exceção E. A decisão 29 não autoriza ativá-la automaticamente; mantêm-se
zero e a F7 pendentes. Os logs curtos com zero de quatro não provam as três janelas nem a tela dividida.

**Prova obrigatória para cada degrau de execução futuro (seção 28):** x86gv e PPCg sem mapa, Duelo/Treino,
sem threads/com gravação/com desenho, chaves 0/1 e combinações, atrasos 1/5/20 ms, listas normais e hashes
iguais, `divergencias.py --normal`, zero `race:`, uso após soltar e descartes novos; mutantes que quebrem cada
regra de ordem, versão, barreira e liberação. Comparar objetos planos com `objcopy --strip-debug`/`cmp`,
publicar exceção somente do carimbo de teste e SHA1 da `.text`; `REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo
f5_treino`, `SEMMAPA=1 IMG=1 roteiros.sh duelo`, `save_mc.sh` com estados e saves não vazios/iguais.
No console: mesmos estados e referências por versão, capturas de intermediários e final byte a byte iguais
contra a base na mesma arquitetura, ambas as escalas, Guide/saída/carga e pressão, áudio sem falha; não usar
apenas tolerância de comparação console/x86 para aceitar imagem alterada. Entrada: mesmo vblank de leitura,
mesmo botão/estado/hash e primeiro quadro que reage, mesmas associações de J/G/P/U/swap, sem vblank extra,
sem fila nova. Medir controles alternados, depois três janelas longas fria/aquecidas e sessão livre com cargas.
As três janelas continuam com zero longas, nenhuma repetição escondida. Testar cedo a carga de duas vistas;
nenhum número desta tarefa certifica F7 nem 720p.

### Próxima rodada preparada e o diagnóstico implementado

**[Lido/testado nativamente]** `port/x360/src/gs/cadeia_cpu.h`, incluído somente pelo backend 360,
implementa `BT3_CADEIA_CPU=A,B` (inclusivo), `=1` para 6960, ausente/0 desligado. Não insere comando,
cerca ou callback, não toca DPC, GS, entrada ou listas. Mede com `QueryPerformanceCounter` e a frequência
já inicializada, sem calibração dependente de `BT3_TEMPOS`. Ausente/0 não lê relógio nos escopos.
Só a thread dona do dispositivo escreve o diagnóstico; aproximadamente 1,3 KB de contadores fixos.

Escopos restauram o pai inclusive em retornos antecipados. Cruzam família (quadro/comum/VU0/VU4/VU6/nativa)
com etapa: alfa, buffers, plano, EDRAM, preparo, pipe, sampler, estado, textura, uniforme, índices, UP,
Present, limpeza, FXAA, tela e resto. Filhos são descontados do pai. As APIs de estado do `prepara`,
`aplica_pipe`/`aplica_sampler`, uniformes VU, índices/UP e Present têm marcas próprias; outras APIs ficam na
etapa envolvente. `GetLevelDesc` fica em preparo e `aplica_blend` em pipe, não são tempos isolados dessas APIs.
EDRAM do laço externo pertence ao grupo quadro; EDRAM chamada dentro da nativa herda nativa.
A amostra cobre o trecho inicial até a limpeza/memória, não os relatórios finais e a liberação do retrato.
`cadeia: cpu quadro Q total`, parcelas por grupo/etapa e `soma` conservam a amostra; captura a invalida.
O diário sai após fechar a amostra. Marca CPU inclui espera interna do driver/preempção: não é CPU pura nem
medidor de espera isolada. Marcas frequentes também perturbam a fase; nenhum diagnóstico aprova apresentação.

`cadeia_cpu.py LOG --quadros 6960` exige todas as parcelas presentes conservarem total/soma, quadros exatos,
nenhuma duplicata/classe desconhecida/indisponível e fim real do Duelo. Não inventa rodapé do backend falso.

**[Preparado, não executado no console]** `reprojeto_console.py SAIDA --rodada rp32a2810` gera 34 ambientes,
`ordem.txt`, `ambiente.txt`, `boot.txt` e `f5_duelo.txt`. Arquivos desta entrega em
`~/f5-reprojeto/console/`; saves `sv_rp32a2810_NOME`, distintos e não encontrados nos logs históricos lidos.
Não houve acesso ao console: a revisão deve confirmar que cada pasta ainda não existe antes de abrir o jogo.
Se existir qualquer uma, gerar rodada/diretório inteiramente novos; nunca reaproveitar save ou completar por cima.

| Ordem | Nomes | Chaves de diagnóstico e finalidade |
|---|---|---|
| 1 a 6 | base1/ambos1, base2/ambos2, base3/ambos3 | Sem marcas finas; constantes/fecho 0/0 ou 2/1, medir variação e a tela |
| 7 a 10 | cpu6930_1/2, cpu6960_1/2 | CPU nova só Q,Q, constantes/fecho 0/0, demais marcas 0; localizar alfa/VU0 primeiro |
| 11 a 18 | cpu6929_1/2, cpu6931_1/2, cpu6959_1/2, cpu6961_1/2 | Mesma CPU, um vizinho por abertura; comparar dependências sem supor mesma fase da x86 |
| 19 a 22 | grav6930_b/a, grav6960_b/a | Só `BT3_CORTES=Q,Q`, base/ambos; envio/hash/VIF/GIF exclusivos e efeito de constantes/fecho |
| 23 a 34 | qQ_R, Q=6929,6930,6931,6959,6960,6961; R=1/2 | Complemento gf29b: só `BT3_COMUM_GPU=Q,Q`, CPU e cortes 0, constantes/fecho 0/0 |

São grupos independentes: após 1 a 10, analisar a CPU antes de gastar as demais aberturas; o complemento GPU
fecha os vizinhos ausentes da seção 31, não condiciona localizar alfa. Manter som, hardware 3, 16:9, 1,33,
FXAA, origem, VU1 direto, envio rápido, tempos 0 e picos/tela como nos controles antigos. Nenhum ambiente novo
altera fila ou intervalo de vblank. A rodada não contém cortes novos ligados; ambos é a combinação anterior.

Na revisão Linux: compilar o commit final desta preparação com XDK, conferir avisos/stubs, `.dat`/`.swap`,
carimbo real e padrões; só depois preparar/enviar o lote da mesma build. Comparar base antes/depois do diagnóstico
com ele desligado. Para cada abertura guardar ambiente, carimbo, SHA256 original e log até o primeiro fim;
conferir 684/2.334 passos, resultado 0x2/0x1, vida 0/27160, zero erro/vigia/descartes, associações completas e
nenhum recurso recusado. Aplicar `passos.py LOG --console --janelas 2 --minimo 600 --carimbo CARIMBO_REAL`.
Se as duas longas persistirem, saída 1 é a reprovação correta do zero, não erro do extrator.
Publicar todos os tempos J/G/P/U, mínimo/p1/p10/mediana de folga e longas, distinguindo controles e marcas.
O envio por formato/igualdade e os carimbos de prefixos exigem novos ganchos na gravação, com oráculos;
ficam explicitamente para a próxima tarefa com emulador antes de um corte de envio ou do degrau 2 acima.

### Conferência desta primeira parte

**[Medido]** `teste_cadeia_cpu.py --saida ~/f5-reprojeto/provas --base bbc07118`: C++03 nativo com relógio e
APIs simulados, `-Wall -Wextra -Werror`; aninhamento inclusive de etapa repetida, herança de família, retorno
antecipado, faixa/atalho 1, chave ausente/0/inválida, efeitos das chamadas e captura inválida. Cinco mutantes
(soma, família, restauração, faixa e relógio desligado) abortam; a fonte sem marcas reproduz exatamente a base,
e uma chamada adulterada é recusada. Leitor recusa captura e seis cópias (fim, total, parcela, quadro, classe,
soma repetida), cada uma com saída 1. Gerador recusa diretório existente e identificador inválido.

| Rodada final sem emulador | Resultado |
|---|---|
| Diagnóstico, leitor e ambientes | tudo certo no teste nativo, sabotagens acusadas; 34 saves distintos |
| Reextração 28/10, dez logs + quinze negativos | estrutura 0 nas dez, negativos 1; seis controles continuam com as mesmas duas longas |
| Reextração 27/10, cinco logs + nove negativos | extração 0, negativos 1; 126 somas exclusivas e seis perfis GPU conservados |
| Aceite atual, quinze logs do console recortados, com seus carimbos reais | saída 1 nos quinze; zero continua reprovado |
| Testes nativos existentes `teste_cortes.py`, `teste_gpu_comum.py`, `teste_tela.sh` | tudo certo, incluindo os mutantes previstos nas duas primeiras ferramentas |
| Fontes fora de `port/x360/` | nenhuma alteração; chamadas D3D originais recompostas exatamente contra `bbc07118` |
| `.text` x86 preexistente, extraída novamente | `43444ea4d03a6d448aa1ae74a631e35b356a7caa` |

A `.text` é de `~/f5-gpu-comum/base/port/build/x86/bt3_x86`, carimbo `4c85c039 diff:da39a3ee5e`,
extraída para `~/f5-reprojeto/x86-preexistente.text`. Não é build nova. Não houve emulador, execução do jogo,
build 360, FTP ou alteração das tarefas concorrentes. As entradas da build plana não incluem os dois fontes D3D
alterados; não se atribui a esta entrega comparação nova de objetos compilados. A matriz x86/PPC, roteiros e save
da seção 28 fica especificada acima para a tarefa de implementação autorizada a usar emulador; não foi executada
nem certificada aqui. Igualdade de comandos por leitura/teste não substitui execução no XDK e capturas.
Ainda não estão provados o diagnóstico no Xenon, a hipótese de alfa, os prefixos, nenhum ganho novo, imagem/entrada
sob reprojeto, zero nas três janelas longas, sessão livre, duas vistas ou resolução maior.
