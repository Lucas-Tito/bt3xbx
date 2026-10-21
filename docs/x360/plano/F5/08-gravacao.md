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
- **A VU1 dentro da janela** saiu da proporção dos trechos vizinhos (42 a 49%), não de medida dentro da luta.
- **A PPC não foi usada.** A x86 bastou para ordenar, e a dúvida que sobra é de memória e de execução em ordem, que a contagem
  de instruções do `qemu-ppc` também não vê. A medida certa é a do console.
- **A memória do item 1** depende da decisão de capacidade das listas vivas.
