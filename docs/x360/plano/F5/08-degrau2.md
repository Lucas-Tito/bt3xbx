# F5, etapa 8, degrau 2: o inventário e o desenho da divisão

[Índice do plano](../../plano.md) | estudo: [08-threads.md](08-threads.md) | tarefa: [f5-degrau2.md](../../tarefas/f5-degrau2.md)

Etapa 1 de 5 do degrau 2 (20/10/2026): o inventário do que a gravação e o desenho dividem, feito só lendo o código
(commit `15bc7a8`), com os achados que mudam o plano conferidos no fonte. Abreviações: `d3d` é
`port/x360/src/gs/gs_d3d.cpp`, `draw` é `port/src/gs/gs_draw.c`, `core` é `port/src/gs/gs_core.c`, `falso` é
`port/x360/headless/gpu_falso.c`.

O degrau 1 pôs a gravação inteira numa thread; nela rodam andar a cadeia, gravar a lista e o `frameEnd` (o desenho no D3D).
O degrau 2 tira o `frameEnd` dessa thread: a gravação monta o quadro N+1 enquanto uma terceira thread desenha o N, a partir de
um **retrato** do quadro feito na entrega.

## As etapas do degrau 2

1. **Inventário e desenho** (este arquivo).
2. **Texturas, alvos e profundidades pelo XG**, ainda com o arranjo do degrau 1, e o `gGsMainFbp` capturado na entrega. Rodada
   curta no console: a luta igual, sem textura errada nem branca.
3. **A divisão portável**: o retrato, a terceira pthread, o falso ligando o `cleared` como o `gs_gpu.c` (nova linha de base),
   as provas no WSL e no Deck.
4. **A thread de desenho no 360**: dono do dispositivo, `Present`, Guide, saída e queda; rodada do critério num serviço;
   rodada no console com três aberturas.
5. **Fechamento**: nota, guia, merge.

## 1. O que o desenho lê da gravação (entra no retrato)

| Dado | Quem escreve | Onde o desenho lê | No retrato |
|---|---|---|---|
| `gsDraws`/`gsDrawCount` | gravação (draw 601, 628, 1174) | d3d 1127-1133, 1959-2006, 2302, 2539, 2872-2952; falso 159, 179, 223 | parte usada |
| `gsVerts`/`gsVertCount` | gravação | d3d 2499 (`DrawPrimitiveUP` lê da memória da CPU), 2556, 2019; falso 181-186 | parte usada |
| `gsVuVerts`, `gsVuIdx`, `gsVuUni` e contagens | gravação | d3d 1942, 1990-2001, 2546-2551; falso 197-201 | parte usada |
| `gsTargets[]`/`gsTargetCount` | gravação (draw 524, 547-552, 641-643, 657, 670, 698, 979-980) **e o desenho** (d3d 850, 2942) | `cleared` em d3d 1229, 1254, 2407, 2425, 2463, 2814, 2881, 2977, 2984, 3056; o laço em 842, 1134, 2967; falso 167-174 | a tabela (576 B); o `cleared` se divide (risco 1) |
| `gGsMainFbp` | **thread do jogo** (gs_sony.c:73), antes da espera do core 1522 | d3d 2966 | o valor, capturado na entrega (risco 5) |
| `gGsFrame` | gravação (core 1085) | d3d 679, 756, 1135, 1143, 1808, 2415, 2434, 2845, 2973-2992; falso 158 | o número do quadro |
| `gGs.frame[0..1]` | gravação | d3d 1774-1775 (captura) | 16 B |
| `gsSkipped`, `gsNative`, `gsAnchor`, `gsTexCount`, `gGsVu1Ms`, `gGsVu1Lotes`, `gGsVu1Desconhecido`, `gGsVu1InstrDesc` | gravação, VU1 | contas e logs do d3d | os valores, zerados na entrega |
| `sPipes`/`sPipeCount` | `pipe_get` (d3d 887-907), na gravação | d3d 1130, 1914, 2310, 2423, 2481-2508 | sem cópia: só cresce; barreira na entrega e o desenho usa índices abaixo da contagem do retrato |
| falso: `sHash` (S_TEX), `sTexNovas`, `sTexBytes`, `gGsBytes`, `sConteudo`, `sPipeKey`/`sPipeCount` | gravação (falso 80, 104-141) | falso 157, 175-177, 209-227 | fechar a S_TEX e mandar o hash e as contagens; `sConteudo` sofre `realloc` (80): resolver na entrega |

O que o `frame_end` hoje zera ou devolve ao lado portável, e passa a ser feito pela gravação na entrega: os contadores das
listas (d3d 3035-3039), `gsTargets[i].draws` (3032-3034), `gsAnchor`, `gsSkipped`, `gsNative` (3040-3043), `gGsVu1Ms` (2860).
O que ele devolve e **não** pode continuar assim: o `cleared` dos alvos (2942, 850), o `last` dos alvos (pelo `GsDraw_TargetGet`
em 2966 e 3055) e o cache de texturas (pelo `GsDraw_X360Abre` em 806 e 1082).

## 2. O estado do backend: quem fica dono

| Estado | Dono no degrau 2 | Como o outro lado o vê |
|---|---|---|
| cache de texturas (`sTex`, `sTexBucket`, `sLast`, `sCluts`, `gsTexCount`, draw 100-105) | gravação | o desenho para de chamar o `GsDraw_X360Abre` (prof_garante e alvos_sem_textura passam à gravação) |
| `sMem.vivos` (mem_cria 245), `pressoes`, `descN`, `descBytes`, `emUso`, `branca[]`, `acimaTeto`, `sConta.semTextura`/`texNovas` | gravação | no retrato, para os logs |
| a cova (`sCova*`, d3d 173-176) e `sMem.pend`/`esperas` | desenho | a gravação manda pedidos de liberação no retrato, com o quadro do pedido |
| `sMem.livres`/`livresMin` (mem_mede 221) | cada lado com a sua medida | a da gravação decide o descarte |
| `sAlvos[i].tex`, `sAlvoSemTex`, `sProf[]` (criação) | gravação, pelo XG | |
| `sAlvos[i].conteudo`, `sResidente`, `sSujo`, `sZDono`, `sSujoZ`, `sDateFonte` | desenho | os resets do `target_ensure`/`target_drop` viram eventos no retrato, aplicados no começo do quadro certo |
| `gGsD3dEtapa` | duas variáveis, uma por thread | o vigia lê as duas |
| `sFbpMostrado`, `sVu*`, `sCur*`, `sIds*`, `sTempo`, `sTabTipo`, `sDescAntes`, `sPs2Total` | desenho | |
| `sFreq`, `sBranco`/`gsWhite`, `sMem.pisoMb`, `seguro` | só leitura depois da iniciação | a gravação espera o init |

**As entradas do `GsBackend`:** `texCreate` (XG: `XGSetTextureHeader` LIN, `XPhysicalAlloc`, `XGOffsetResourceAddress`),
`texUpload` (`memcpy` linha a linha, sem `LockRect`), `texSlotsLeft`, `pipeGet` (não toca o dispositivo), `targetEnsure`,
`targetDrop` e `targetColor` (alvo tiled pelo XG, zerado por `memset`; o d3d 1092 já monta um cabeçalho XG) ficam na gravação.
`texDestroy`: a gravação pede, o desenho solta. `GsD3d_Falta`, `TexBytes`, `Descartou`, `Branca`: gravação; a espera pela GPU
do `Descartou` (d3d 664-667) vira espera por um sinal do desenho, com limite.

## 3. Chamadas ao dispositivo que hoje rodam na gravação

| Linha (d3d) | Chamada | Via | No degrau 2 |
|---|---|---|---|
| 722 | `CreateTexture` LIN | tex_create | XG |
| 737, 763 | `LockRect`/`UnlockRect` | tex_upload | `memcpy` no endereço físico |
| 786, 790-792 | `CreateTexture` do alvo, `LockRect` + `memset` | target_ensure | XG |
| 551, 561-564, 597 | `Release`, `SetTexture(NULL)`, `BlockUntilNotBusy`, `IsBusy` | a cova | desenho |
| 1106-1107 | `IsBusy`/`BlockUntilNotBusy` da profundidade | target_drop | desenho, pela fila |
| 643, 774 | `GetLevelDesc` | TexBytes, tex_destroy | lê o cabeçalho; serve com cabeçalho XG |
| 3060-3061 | `tela_copia` + `Present` | GsD3d_Repete | desenho |

`Release`, `IsBusy`, `BlockUntilNotBusy` e `LockRect` são métodos do recurso, mas mexem na cerca e na memória do dispositivo:
ficam no desenho.

## 4. O que muda de dono

| Peça | Degrau 1 | Degrau 2 |
|---|---|---|
| `d3d_init`, `AcquireThreadOwnership` (d3d 1642) | na gravação (core 1215) | no desenho; as alocações do `GsGpu_Init` (draw 1364-1368) ficam na gravação, que espera o init |
| `GsD3d_Repete` (d3d 3050) | gravação ociosa 50 ms | desenho sem retrato há 50 ms; usa a cópia própria de `gsTargets`, nunca o `GsDraw_TargetGet` (escreve `last`) |
| `GsD3d_Soltar` | trabalho 3 da gravação, `Port_Sair`, `AoMorrer` | a gravação entrega o último retrato, o desenho o desenha, solta e sai; o `AoMorrer` ganha o ramo "caiu o desenho" |
| Capturas `BT3_SHOT` (d3d 1764, 2970-2980) | gravação | desenho, com `gGsFrame` e `gGs.frame` do retrato |
| Vigia (main360 57-85) | um estado e uma etapa | três estados e duas etapas; um `Port_GsDesenhoPartida` |

## 5. Riscos

| # | Risco | Gravidade | Tratamento |
|---|---|---|---|
| 1 | **O `cleared` volta do desenho para a gravação.** A gravação usa um alvo como textura se `gsTargets[src].cleared` (draw 675); quem o liga é o `frame_end` (d3d 2942) e quem o desliga é o `alvos_sem_textura` (850). Com o desenho de N em paralelo à gravação de N+1, a lista de N+1 depende do tempo | alta | o `cleared` da gravação ligado na entrega para todo alvo com desenho não nativo (o que o degrau 1 faz, salvo o alvo cujos desenhos são todos da VU1 que não couberam no par, o `sVuIni ~0` de d3d 2932); o do desenho só do lado dele |
| 2 | **O falso nunca liga o `cleared`** (só o lê, falso 170): no WSL o caminho do alvo como textura nunca aparece e a prova não vê o risco 1 | alta para a prova | o falso passa a ligá-lo como o `gs_gpu.c` (856, 915, 921), com linha de base nova, antes das provas |
| 3 | O desenho mexe no cache de texturas (`GsDraw_X360Abre` por prof_cria e alvos_sem_textura) enquanto a gravação o lê | alta (queda) | alvos e profundidades criados na gravação |
| 4 | **A regra dos dois quadros não vale em todo lugar:** `clut_texture` descarta uma paleta que não é do quadro atual da gravação (draw 456), e a varredura do `MAX_TEX` também (draw 303), ou seja, coisas do quadro que o desenho ainda está desenhando | alta (uso depois de liberar) | toda liberação pela fila, com o quadro do pedido, solta depois de o desenho passar desse quadro e da cerca |
| 5 | `gGsMainFbp` é escrito pela thread do jogo antes da espera: o desenho leria o do quadro seguinte. **Já é uma janela no degrau 1** | alta, barata | capturar na entrega (etapa 2) |
| 6 | Eventos de alvo fora de ordem | média | aplicados no começo do quadro certo |
| 7 | `GsDraw_TargetGet` no desenho escreve `last` | baixa | o desenho usa a cópia própria |
| 8 | Liberações 1 a 2 quadros mais tarde: mais pressão de memória (mínimo visto 31 MB, piso 32); a espera da gravação por memória pode depender de um retrato não entregue | média | espera com limite |
| 9 | A ordem dentro do quadro (DATE, ids, névoa) | nenhuma | toda do desenho, sem leitura de volta para a VRAM do GS |
| 10 | `BT3_INTERP` chama o `frameEnd` várias vezes trocando listas | baixa | desliga o degrau 2 |
| 11 | No falso, `sPipeCount` e `sConteudo` mudam durante a gravação de N+1 | média (só provas) | resolvidos na entrega |

## 6. O retrato

Por quadro, pelos máximos medidos: `Vtx` 28 B, `GsDraw` 128 B, vértice da VU 48 B, índice 4 B, `Vu0Uniform` 304 B.

| | Máximo | Bytes | Com o dobro |
|---|---:|---:|---:|
| Vértices de CPU | 13.182 | 369.096 | 738.192 |
| Vértices da VU | 53.618 | 2.573.664 | 5.147.328 |
| Índices | 128.772 | 515.088 | 1.030.176 |
| Constantes | 2.228 | 677.312 | 1.354.624 |
| Desenhos | 2.454 | 314.112 | 628.224 |
| **Total** | | **4,24 MiB** | **8,49 MiB** |

Mais uns 1 KB fixo e as filas. Com as capacidades arredondadas da seção 4.2 de `08-threads.md`, a área dá 10,06 MiB. Uma área
só basta: o desenho (uns 12,5 ms) larga o retrato antes da entrega seguinte (uns 38 ms).

## O que muda na tarefa

1. **Pipelines não viram pedido:** o `pipeGet` não chama o dispositivo.
2. **O plano de reserva da tarefa não cabe no 360:** um quadro que não coube no retrato não pode ser desenhado na gravação, que
   deixa de ser a dona do dispositivo. Em vez disso, a gravação espera o desenho acabar e o desenho lê as listas vivas, sem
   cópia (o quadro sai em série, como antes do degrau 1).
3. **Alvos e profundidades também pelo XG, na gravação:** só as liberações e os eventos de alvo atravessam, sem vagas e sem
   levar o índice do alvo fonte no `GsDraw`.
4. **O `cleared` se divide, e o falso o liga antes das provas** (riscos 1 e 2).
5. **A fila de liberação por quadro é a regra geral**, não o contrato dos dois quadros (risco 4).
6. **O `gGsMainFbp` se captura na entrega** já na etapa 2, porque a janela existe no degrau 1.

Fora do escopo, anotado: no 360 o `gGsFbUploads` nunca é zerado (só o `gs_gpu.c` e o `gs_gl.c` zeram); depois do primeiro envio
direto para a tela, o marcador de filme nunca mais se grava. Uma economia possível, para depois: 3,1 dos 4,4 MB do retrato são
vértices e índices da VU, que o `vu_copia` (d3d 1954) recopia para a GPU; fazer essa cópia na gravação, em buffers do XG,
evitaria copiar duas vezes.

## Etapa 2: texturas, alvos e profundidades pelo XG; o `gGsMainFbp` da entrega

Feita em 20/10/2026, ainda com o arranjo do degrau 1 (gravação e desenho na mesma thread). Dois commits no branch
`f5-degrau2`.

**O que mudou no `gs_d3d.cpp`.** As texturas do jogo (`tex_create`), as dos alvos (`textura_alvo`) e as de profundidade
(`prof_cria`) não passam mais pelo dispositivo: `xg_cria` monta o cabeçalho com `XGSetTextureHeader` num bloco de memória
comum (`TexXg`: o cabeçalho primeiro, o endereço dos dados, os bytes e o pitch), reserva os dados com `XPhysicalAlloc`
(página de 4 KB, que é o alinhamento que o XDK pede para textura; `PAGE_READWRITE | PAGE_WRITECOMBINE`, a memória que o XDK
exige para recurso feito à mão e a mesma que o D3D usa sem `D3DUSAGE_CPU_CACHED_MEMORY`) e liga os dois com
`XGOffsetResourceAddress`. O envio vira `memcpy` linha a linha no endereço com o pitch do XG; alvo e profundidade são
zerados por `memset` (a textura é nova, a GPU nunca a viu). O cabeçalho que lê a profundidade como cor (`z->cor`) sai do
novo cabeçalho como antes (`XGGetTextureLayout`).

**A liberação.** O D3D não conta esses recursos, então nenhum `Release` neles: a cova guarda só `TexXg` e `xg_libera`
(`XPhysicalFree` mais o cabeçalho) roda depois do `IsBusy` falso (`cova_varre`) ou do `BlockUntilNotBusy`
(`cova_espera_primeira`), que o XDK documenta para "manually allocated resource". O autoteste da profundidade espera as
duas texturas (`BlockUntilNotBusy`, o que o `Release` final fazia por dentro) antes de soltar. A captura `BT3_SHOT` e a
comparação do autoteste leem direto no endereço (a GPU já está parada nas duas), sem `LockRect`. Seguem com `Release` os
recursos do dispositivo: `sBranco`, os buffers da VU, a tela, o padrão do autoteste e as texturas dos autotestes.

**A conta da memória.** Os bytes de cada recurso são os do `tam_real` (o retorno do `XGSetTextureHeader` arredondado à
página), que é exatamente o que o `XPhysicalAlloc` reserva, e o `dwAvailPhys` cai com o `XPhysicalAlloc` como caía com o
`CreateTexture` (que reserva pelo mesmo caminho). O cabeçalho (menos de 100 bytes no heap) fica fora da conta, como o do D3D
ficava. A calibração do `mem_init` passou a criar as texturas de teste pelo `xg_cria`: as duas linhas `gs_d3d:
calibração:` do log comparam a conta com o `dwAvailPhys` no caminho novo.

**O `gGsMainFbp` (risco 5).** O `render_post` do `gs_core.c` guarda o `gGsMainFbp` na entrega de cada trabalho (a lista e o
canal da GIF), e a thread o põe em `gGsFbpEntregue` ao pegar o trabalho; sem a thread, `gGsFbpEntregue` recebe o
`gGsMainFbp` na própria thread do jogo logo antes de a lista andar. Os leitores passam a usar `GS_FBP_QUADRO`, que é o
`gGsFbpEntregue` nas builds com `BT3_GS_ESPERAS` e o próprio `gGsMainFbp` nas outras. O `BT3_GS_ESPERAS` foi do
`gs_core.c` para o `gs_internal.h`, com a mesma condição, para os outros arquivos o verem. **O inventário tinha listado só
parte dos leitores:** além de draw 633, 656, 691, 697, core 1049 e d3d 2966, leem o valor durante a gravação draw 924 (o
brilho), 933 (o esticamento da tela larga) e 2756 (`interp_prims`), e no fim do quadro os backends do PC (`gs_gl.c` 947,
`gs_gpu.c` 912); todos passaram para o `GS_FBP_QUADRO`. O `imagens.c` do headless continua com o `gGsMainFbp`: roda na
thread do jogo.

**O que foi provado.**

| Prova | Resultado |
|---|---|
| Build do 360 (`build_f2.sh`, commit `2d4a52d`) | 317 de 317 fontes sem erro; avisos por fonte iguais aos da main (só os dois C4090 de sempre no `gs_draw.c`); os mesmos 103 stubs; `default.dat` `6d30baaf87b1177fb2359a855f8befc0` e `default.swap` `a7cf723fbfaea860fcc3707ad288e755`, iguais; `default.xex` `8c29afa22ddb53c644606657ba902331` |
| `.text` da x86 plana (WSL) | `cc46fa0752f092fca305bc8bf2e2e54390763af0`, a mesma |
| Lista por quadro, `f5_treino`, x86gv, sem thread contra `BT3_GS_THREAD=1 BT3_GS_RACE=1 BT3_GS_ATRASO_US=5000` | 7.386 quadros nos dois, 0 com a lista diferente, 0 linhas race, hash do jogo `70b6b1d1bb3f1d85` nos dois (o de referência), `divergencias.py` sai com 0 |
| O mesmo, `f5_duelo` | 8.380 quadros nos dois, 0 diferentes, 0 race, hash `29caae14a161459a` nos dois (o de referência), saída 0 |

O WSL só prova o lado portável (o `GS_FBP_QUADRO` e a captura não mudam a lista, com e sem thread, e a x86 plana não
muda). O XG não roda lá: o falso não tem dispositivo. As provas também não mostram a janela do risco 5 acontecendo: os
hashes da base já batiam antes da mudança, ou seja, nos dois roteiros o jogo não chegou a escrever o `gGsMainFbp` do quadro
seguinte enquanto a gravação ainda o lia. A captura fecha a janela; que ela existia é leitura do código.

**O que só o console prova** (a rodada é do Lucas): no `bt3xbx.log`,

1. as duas linhas `gs_d3d: calibração:` com "contadas" igual (ou muito perto) de "medidas": 64 de 8x8, 256 KB; 16 de
   256x256, 4096 KB;
2. a linha `gs_d3d: profundidade: autoteste ...` com os mesmos números da rodada do degrau 1 (o modo escolhido e os
   "iguais" do modo 1 e 2): ela lê as texturas do XG direto no endereço;
3. nenhuma linha `gs_d3d: BRANCA`, `ERRO: o alvo ... ficou sem textura` ou `ficou sem textura de profundidade` que a rodada
   do degrau 1 não tinha, e as pressões de memória (`gs_d3d: pressão`) e os livres mínimos na mesma faixa (mínimo visto
   31 MB com piso 32);
4. o resumo de 60 quadros com a luta em 48 ms como no degrau 1 (o `memcpy` no lugar do `LockRect` não deve custar nada);
5. nas capturas `BT3_SHOT=600` (`game:\shots`), a luta e os menus sem textura errada, preta ou branca, e o fundo dos alvos
   preto (o `memset`);
6. nenhuma queda: o `SetUnhandledExceptionFilter` diria `Iar` e `Lr`; um erro no `xg_libera` (liberar o que a GPU ainda lê)
   apareceria como textura corrompida ou queda depois de uma pressão de memória.

**Roteiro da rodada.** Copiar `build/x360/f2/default.xex`, `default.dat`, `default.swap` e `ambiente.txt` do worktree
`bt3xbx-d2` para a pasta do jogo (o `ambiente.txt` já traz `-BT3_DEMO`, o `BT3_GS_THREAD=1` e o `BT3_SHOT=600`); abrir pela
FreeStyle; menu principal, modo duelo, uma luta de uns dois minutos com troca de personagem e golpes especiais, um menu 3D
(o `ChrView`), sair para o dashboard; trazer o `bt3xbx.log` e a pasta `shots`.

### A rodada no console (20/10/2026) [medido]

O `.xex` `8c29afa22ddb53c644606657ba902331` com `BT3_GS_THREAD=1`, Duelo, Goku contra Kuririn, estágio 5, saída pela pausa e
pela Guide. A primeira abertura pareceu travar numa tela preta depois das logos: era o filme de abertura, que no 360 é contado e
não desenhado (cabeçalho do `gs_d3d.cpp`, fica para a F6) e dura uns dois minutos; START o pula. O log era idêntico ao do degrau 1
até ali.

| | Degrau 1 (20/10, abertura 1) | Etapa 2 (XG) |
|---|---:|---:|
| Calibração da memória (8x8 e 256x256) | 256 KB e 4096 KB | **256 KB e 4096 KB**, contadas igual às medidas |
| Luta, passos (mediana, p95, p99) | 48,33, 72,87, 97,92 ms | 48,91, 71,64, 85,36 ms |
| Texturas pretas (RGB zero, do próprio jogo) | 32, nos mesmos grupos | 32, nos mesmos grupos |
| Texturas brancas | 0 | 0 |
| Descarte | 4 pressões, 649 texturas (66 MB), 0 esperas | 3 pressões, 409 texturas (49 MB), 0 esperas |

Os recursos do XG funcionam na GPU (texturas, alvos e profundidades), a cerca deles vale (o descarte soltou 409 texturas sem
espera e sem queda), a conta da memória bate e a velocidade é a do degrau 1. A etapa 3 pode partir daqui.
