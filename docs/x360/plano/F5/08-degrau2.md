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

## Etapa 3: a divisão portável, provada no WSL

Feita em 20/10/2026 no branch `f5-degrau2`, só no lado portável (`gs_core.c`, `gs_draw.c`, `gs_draw.h`, `gs_internal.h`) e no
falso, tudo dentro de `#ifdef BT3_GS_ESPERAS` (o `gs_draw.h` passou a definir o símbolo com a mesma condição, para o falso, que
não inclui o `gs_internal.h`, ver o mesmo `GsBackend`). Commits: `fa7883f` (o falso liga o `cleared`), `48075f6` (o retrato, a
terceira thread e a fila), `582bbe7`, `56ca15b` e `e92cdb2` (correções: o `gGsVu1Ms` só existe no 360; os acentos das mensagens
por escape octal para o `c89.py`; um aviso C4101 no 360). O `gs_d3d.cpp` não mudou: no 360 a chave é recusada.

**O que mudou.**

1. **O falso liga o `cleared` como o `gs_gpu.c`** (risco 2): no fim do quadro, todo alvo com desenho de primitivas (native 0,
   a VU1 inclusive) ou com imagem enviada (native 100 e 101); as nativas 1 a 5 não. Linha de base nova da `x86gv` sem thread:
   `f5_treino` 7.386 quadros, hash do jogo **`f76f0f6849b64629`** (era `70b6b1d1bb3f1d85`); `f5_duelo` 8.380 quadros,
   **`08a98b6d05b8775a`** (era `29caae14a161459a`). O hash mudou: o caminho do alvo como textura passou a existir no WSL.
2. **O retrato e a terceira thread.** `BT3_GS_DESENHO=1`, só com `BT3_GS_THREAD=1` (sem ela, uma linha no log e nada muda),
   decidido no fim do `GsGpu_Init`. O `frameEnd` do backend lê o quadro por `gsQuadro` (`const GsQuadro *`, `gs_draw.h`): sem a
   divisão ele aponta para as listas vivas com as contagens de agora (`vivo` 1, `dividido` 0) e o backend zera os contadores
   como antes; com ela, para o retrato. Na entrega (`desenho_entrega`, no lugar do `frameEnd`, na gravação): espera o desenho
   largar o retrato anterior, copia a parte usada de `gsDraws`, `gsVerts`, `gsVuVerts`, `gsVuIdx` e `gsVuUni` para uma área de
   **10.304 KB** (8.192 desenhos, 32 Ki vértices de CPU, 128 Ki da VU, 256 Ki índices, 4.096 constantes, o dobro da seção 4.2
   de `08-threads.md`), copia a tabela de alvos, o `gGsFrame`, `gGs.frame[0..1]`, o `GS_FBP_QUADRO`, `gsAnchor`, `gsTexCount`,
   `gsSkipped`, `gsNative` e o `gGsVu1Ms` (zerado ali), liga o `cleared` da gravação, zera os contadores e acorda a thread de
   desenho. Um quadro que não cabe vai sem cópia: o retrato aponta para as listas vivas e a gravação espera o desenho acabar
   (em série). O `sPipes` não se copia: o desenho só usa índices abaixo da contagem da entrega, e a entrega é a barreira.
   `BT3_INTERP` recusa a divisão (risco 10). O `Port_GsParar` para a thread de desenho depois da de gravação
   (`gGsDesenhoParar`).
3. **O `cleared` dividido** (riscos 1, 3 e 7). A gravação liga o seu na entrega, a partir da lista: 1 para todo alvo com
   desenho não nativo, num vetor `limpa[]` que o backend pode ajustar na nova entrada `GsBackend.entrega` (chamada na gravação,
   com o retrato pronto). O falso acrescenta os alvos com imagem enviada (a regra do `gs_gpu.c`); o `gs_d3d.cpp` não os liga
   (conta `semFb`), e é por esse gancho que a etapa 4 trata o caso do `sVuIni ~0` (d3d 2932): a decisão do `vu_copia` sobre o
   que cabe no par de buffers só depende da lista e da capacidade fixa (`sVuCapV`, `sVuCapI`), então a entrega pode fazer a mesma
   conta e desligar a marca do alvo cujos desenhos são todos da VU que não cabem (ou levar o próprio `vu_copia` para a entrega,
   com buffers do XG, a economia anotada acima). Na etapa 3 o 360 não divide, e o caso não existe no falso. O do desenho fica na
   cópia da tabela que vai no retrato (`GsQuadro.targets`): o desenho nunca escreve na tabela viva, e não deve chamar o
   `GsDraw_TargetGet` (escreve `last`; o `gs_d3d.cpp` 2966 e 3055 terão de buscar o `fbp` em `q->targets` na etapa 4). O
   cache de texturas segue só da gravação.
4. **A fila de liberação por quadro** (risco 4). Todo `texDestroy` do `gs_draw.c` (o `MAX_TEX`, o `clut_texture`, o
   `GsDraw_X360Abre`) e a vaga de alvo reaproveitada (`GsDraw_TargetGet`, no lugar do `targetDrop`) passam por `solta_pede`:
   sem a divisão, direto ao backend como antes; com ela, `GsBackend.soltaPede` na gravação (o falso mistura no hash o mesmo que o
   `texDestroy` e o `targetDrop` misturavam, no mesmo ponto) e um item `{tex, alvo, quadro}` numa lista que viaja com o retrato
   do quadro em que foi pedido; o desenho chama `GsBackend.solta` de cada item depois de desenhar esse quadro. O falso envenena a
   textura solta (um mapa de bits fixo, sem `realloc`) e conta todo desenho que a use: a linha `gpu_falso: N desenhos com
   textura já solta`, que o `divergencias.py` passou a exigir 0. Para a vaga de alvo a etapa 4 terá de, no `soltaPede`, tirar as
   texturas da vaga (o `targetEnsure` seguinte cria as novas) e soltá-las no `solta`.
5. **O falso como desenho:** lê tudo pelo `gsQuadro`, faz as seções depois da `S_TEX` num hash próprio, e recebe na entrega o
   que a gravação segue mudando (a `S_TEX` fechada no mesmo ponto, `sTexNovas`, `sPipeCount`, `gGsBytes` e o hash normal dos
   desenhos, que lê o `sConteudo`), o risco 11.
6. **Chaves só de teste**, nas builds `--gpu` (o `headless.py` passou a compilar o `gs_draw.c` com `-DBT3_GPU_FALSO`):
   `BT3_GS_DESENHO_ATRASO_US` (o desenho dorme antes de desenhar) e `BT3_GS_TEX_MAX=N` (o cache "cheio" com N texturas, para a
   varredura soltar texturas do quadro anterior: sem ela nenhum roteiro solta textura que o desenho ainda use, e a fila não se
   prova; no `f5_treino` sem ela não há nenhum pedido de soltar).
7. **O 360:** `BT3_GS_DESENHO=1` é recusado com a linha `gs: BT3_GS_DESENHO=1 recusado: o desenho numa terceira thread no 360 é
   a etapa 4 do degrau 2; segue o degrau 1` no `bt3xbx.log`. Sem a chave o caminho é o da etapa 2 (o `backend_frame_end` só
   preenche o `GsQuadro` vivo antes do `frameEnd`, que o `gs_d3d.cpp` não lê).

**As provas na x86 de teste (`x86gv`, WSL).** Base: a mesma build sem thread. Rodadas com `BT3_GS_RACE=1`; t1 é o degrau 1
(`BT3_GS_THREAD=1` sem o desenho), d0, d5 e d20 a divisão com a gravação atrasada em 0, 5 e 20 ms, r5 e r20 a divisão com o
desenho atrasado em 5 e 20 ms; tr20 é a divisão com `BT3_GS_TEX_MAX=256` e o desenho atrasado 20 ms, contra a base com o mesmo
`BT3_GS_TEX_MAX`. Commit `582bbe7` (tr20 do `f5_duelo` refeito no `e92cdb2`).

| Roteiro | Casos | Quadros diferentes | `race:` | Texturas usadas depois de soltas | Hash do jogo |
|---|---|---:|---:|---:|---|
| `f5_treino` (7.386) | t1, d0, d5, d20, r5, r20 | 0 em todos | 0 | 0 | `f76f0f6849b64629` em todos |
| `f5_duelo` (8.380) | t1, d0, d5, d20, r5, r20 | 0 em todos | 0 | 0 | `08a98b6d05b8775a` em todos |
| `f5_treino` | tr20 (3.694 pedidos de soltar) | 0 | 0 | 0 | `75d7f76b58fea039`, o da base com o mesmo `BT3_GS_TEX_MAX` |
| `f5_duelo` | tr20 (4.906 pedidos de soltar) | 0 | 0 | 0 | `ee6b3bc673e9f8aa`, idem |

`divergencias.py` saiu com 0 em todos (os dois contadores de quadros iguais no fim). Nenhum quadro deixou de caber no retrato.
O maior quadro: `f5_treino` 2.472 desenhos, 9.312 vértices de CPU, 48.481 da VU, 117.621 índices, 2.235 constantes; `f5_duelo`
2.473, 13.125, 53.618, 128.907 e 2.253 (desenhos e constantes acima dos 2.454 e 2.228 da etapa 6, todos com folha de mais de
três vezes). A entrega inteira (cópia, tabela, `cleared`, zerar) levou em média 0,11 a 0,18 ms por quadro na x86 do WSL; no 360
o estimado segue 1,5 a 2 ms (`08-threads.md`, seção 4.2), a medir na etapa 4. A `.text` da x86 plana continua
`cc46fa0752f092fca305bc8bf2e2e54390763af0`.

**As provas negativas** (commit `e92cdb2`; cada sabotagem por `python` com `assert` de que casou, desfeita por `git checkout`
com a árvore limpa; o git do WSL terminou com 0 alterados):

| Sabotagem | Resultado |
|---|---|
| A entrega sem a cópia (o retrato aponta para as listas vivas e a gravação não espera) | com o desenho atrasado 20 ms: 7.382 de 7.386 quadros diferentes no `f5_treino` e 8.374 de 8.380 no `f5_duelo`; **sem atraso (d0), 0 nos dois**: o desenho do falso é rápido demais para a corrida aparecer, a chave de atraso é o que a mostra |
| A liberação sem a fila (o `solta` chamado já no pedido, na gravação) | com `BT3_GS_TEX_MAX=256` e o desenho atrasado 20 ms: 5.019 desenhos com textura já solta no `f5_treino` e 12.738 no `f5_duelo` (as listas iguais; o `divergencias.py` sai com 1 pela contagem); **sem o `BT3_GS_TEX_MAX`, 0**: nenhum roteiro solta textura que o desenho ainda use |
| O `cleared` ligado pelo desenho (a gravação não liga o seu; o falso liga o da tabela viva) | com o desenho atrasado 5 ou 20 ms: 1 quadro diferente em cada roteiro (o 4.071 do `f5_treino`, o 4.137 do `f5_duelo`; seções 2 e 3, desenhos e vértices de CPU); sem atraso, 0 |

**A build do 360** (`build_f2.sh`, commit `e92cdb2`): 317 de 317 fontes sem erro, avisos por fonte iguais aos da main (só os dois
C4090 de sempre no `gs_draw.c`), os mesmos 103 stubs, `default.dat` `6d30baaf87b1177fb2359a855f8befc0` e `default.swap`
`a7cf723fbfaea860fcc3707ad288e755`, iguais; `default.xex` `2fc2041c945da4a1e4344c5e9e49801a`. Não houve rodada no console nesta
etapa: sem a chave, o 360 roda o caminho da etapa 2.

**A PPC (serviço `claude-d2e3` no WSL, disparado sem esperar).** O script `~/provas-degrau2-e3.sh` (usuário `codex`; fica só
no WSL) faz, em série: a build `ppcg` e a x86 plana; a pasta de dados de cada roteiro só pelo passo do
`roteiros.sh` que a gera (os arquivos que a rodada lê, pelo `dprintf` `LEU` da x86 sob o gdb, convertidos pelo `conv_tool
--saida`; sem o rastreio e sem o mapa, que só conferem); e, nos dois roteiros, `vu1_perfil.sh lista ppc` da base sem thread e da
divisão (`BT3_GS_THREAD=1 BT3_GS_DESENHO=1 BT3_GS_RACE=1`) com a gravação atrasada em 0 e 20 ms e com o desenho atrasado em
20 ms, e o `divergencias.py` de cada par. Saída em `~/provas-degrau2-e3.log`, logs em `~/provas-degrau2-e3/`.

**Como ler o resumo** (`~/provas-degrau2-e3.resumo`): a linha `início` (commit `e92cdb2` e 0 alterados); `build ppcg`; `dados
ROTEIRO` (arquivos lidos e convertidos; um `AVISO` se a x86 sob o gdb não chegou ao fim); `base ROTEIRO` (quadros, hash do jogo
da PPC e `0 desenhos com textura já solta`); uma linha **`OK`** ou **`FALHA`** por par (seis no total), com os quadros comuns e
os diferentes, as texturas usadas depois de soltas e os dois hashes; os tempos de cada rodada; e `FIM`. A etapa vale na PPC com
seis `OK`. O hash da base da PPC não precisa ser o da x86 (no degrau 1 o do `f5_duelo` já era outro, `b65001fe18962dda`): o
critério é o par. Se a PPC parar na guarda por um arquivo que o `LEU` não listou, base e divisão param juntas e a linha diz
`SEM a linha final`: aí a pasta precisa do passo inteiro do `roteiros.sh` (o rastreio).

### A falha da PPC (20/10/2026)

**O sintoma.** A primeira rodada do serviço (commit `e92cdb2`) deu quatro `FALHA` e dois `OK`: com a gravação atrasada 0 ou 20 ms,
10 a 14 quadros por roteiro faltando ou "com a lista diferente", entre o 3.978 e o 4.102 (no meio, não na parada: o contador de
quadros do jogo e o do desenho chegaram ao mesmo número); com o desenho atrasado 20 ms, tudo igual. O hash do jogo igual em todas.

**A causa: o log, não a divisão.** O hash da lista (o campo do começo da linha) era igual ao da base em **todos** os quadros das
quatro rodadas, inclusive nos "faltando". A linha `gpu_falso: quadro N lista ... secoes ...` saía em nove `fprintf` no `stderr` sem
buffer (`gpu_falso.c` 312 a 317, no `e92cdb2`); com a divisão quem a escreve é a thread de desenho, e na PPC a thread do jogo escreve
ao mesmo tempo as linhas `LEU arquivo` (`BT3_LEU=1`, `inverter.c` 146, só na PPC, para o `roteiros.py` comparar os arquivos lidos)
no mesmo arquivo. Uma `LEU` no meio da linha a cortava: logo depois de `secoes`, a regex do `divergencias.py` não casava e o quadro
"faltava"; depois de algumas seções, sobravam seções a menos e o quadro dava "diferente" nas seções que faltavam. As linhas cortadas
(`grep -c '.LEU '`) eram 11, 14, 12 e 14 nas quatro rodadas, exatamente os quadros faltando mais os diferentes de cada uma; 0 na base
e nas de desenho atrasado (o desenho, dormindo, escreve fora das leituras de arquivo). Na x86 não há `LEU` no log (o `dprintf` do gdb
só roda no passo que gera a pasta de dados), por isso ela passou. O risco 11 e o resto do retrato não tiveram parte.

**A correção** (`f1d25b3`, só no falso e no verificador): a linha montada num buffer e escrita com um `fputs`; o `divergencias.py`
com a regex das seções estrita (só grupos de 8 dígitos hexa até o fim) e uma linha nova, `linhas misturadas (outra escrita no meio da
linha do quadro)`, que conta a linha do quadro cortada e texto de outra linha no meio, e faz sair com 1. Visto falhando: o novo
`divergencias.py` sobre os logs do `e92cdb2` (guardados em `~/provas-degrau2-e3-e92cdb2` no WSL) dá 11, 12, 14 e 14 linhas
misturadas, nenhum quadro com a lista diferente, e sai com 1; base contra base e as de desenho atrasado seguem "iguais".

**As provas** (`f1d25b3`). Na `x86gv`, a matriz curta (t1, d0, d20, r20 nos dois roteiros) com o `divergencias.py` saindo com 0 nas
oito, 0 `race:`, 0 texturas usadas depois de soltas, os hashes `f76f0f6849b64629` e `08a98b6d05b8775a` (`~/provas-d2e3-x86/resumo`).
Na PPC, o mesmo serviço (`claude-d2e3`; a build `ppcg` pede a x86 plana do mesmo commit antes, e o script a fazia depois: a primeira tentativa parou na build, e o script passou a fazer a x86 primeiro), **seis `OK`**:

| Roteiro | Caso | Quadros comuns | Diferentes | Hash do jogo (base e divisão) | Tempo |
|---|---|---:|---:|---|---:|
| `f5_treino` | gravação atrasada 0 | 7.386 de 7.386 | 0 | `f76f0f6849b64629` | 145 s |
| `f5_treino` | gravação atrasada 20 ms | 7.386 | 0 | idem | 265 s |
| `f5_treino` | desenho atrasado 20 ms | 7.386 | 0 | idem | 211 s |
| `f5_duelo` | gravação atrasada 0 | 8.380 de 8.380 | 0 | `994627a2aecd429a` | 170 s |
| `f5_duelo` | gravação atrasada 20 ms | 8.380 | 0 | idem | 319 s |
| `f5_duelo` | desenho atrasado 20 ms | 8.380 | 0 | idem | 245 s |

Em todas, 0 texturas usadas depois de soltas, 0 `race:`, 0 linhas misturadas, 0 retratos sem cópia; as 42 linhas `LEU` de cada
rodada seguem no log, todas inteiras. A etapa 3 vale na PPC.

**O que fica para a etapa 4:** o `gs_d3d.cpp` lendo pelo `gsQuadro` (as leituras de `gsDraws`, `gsVerts`, `gsVu*`, `gsTargets`,
`gGsFrame`, `GS_FBP_QUADRO` e `gGsVu1Ms` do inventário viram `q->`), sem zerar contadores com `dividido`; o `entrega` (o
`sVuIni ~0`, os pedidos de alvo), o `soltaPede` e o `solta` (a cova); o `cleared` e o conteúdo dos alvos persistentes do lado do
desenho; a thread dona do dispositivo, o `Present`, a Guide, a saída e a queda; e tirar a recusa do `divisao_decide`.

## Etapa 4: a thread de desenho no 360

Feita em 20/10/2026 no branch `f5-degrau2` (commit `ea1a01e`). Com `BT3_GS_THREAD=1` e `BT3_GS_DESENHO=1` no `ambiente.txt`, o
360 passa a ter três threads no GS: o jogo (hardware 2), a gravação (hardware 0) e o desenho (hardware 3, ou 1 com
`BT3_GS_DESENHO_HW=1`). Sem a chave, o caminho é o da etapa 2 (a gravação grava e desenha).

**A posse do dispositivo.** No 360 a divisão se decide antes do init do backend (`GsGpu_Init`, `divisao_decide`): com ela, o
`d3d_init` (e o `AcquireThreadOwnership`) roda na thread de desenho, num trabalho próprio (`desenho_init`), e a gravação
espera; as alocações das listas do `GsGpu_Init` ficam na gravação. A thread de desenho é a única que chama o dispositivo: o
`frame_end`, o `Present`, a cova (`IsBusy`, `BlockUntilNotBusy`, `xg_libera`), a repetição do último quadro para a Guide e a
soltura na saída. Ociosa, ela acorda a cada 50 ms (`Port_X360CondEsperaMs`) e: atende o pedido de memória da gravação
(`GsD3d_Ocioso`) e, se passou de 50 ms sem `Present`, repete o último quadro (`GsD3d_Repete`, com a textura que o próprio
desenho guardou, `sMostradoTex`; a cova a esquece quando ela é solta). Enquanto repete, a entrega espera (`sDesTem` 2);
enquanto a gravação monta o retrato (`sEntregando`), o desenho não repete. O `GsD3d_Repete` da thread de gravação (degrau 1)
segue no código e não faz nada com a divisão (só a dona do dispositivo, `sDono`, repete).

**O `gs_d3d.cpp` lendo o retrato.** Todo o `frame_end` lê `gsQuadro` (`sQ` nas funções que ele chama): listas, contagens, a
tabela de alvos (o `cleared` do desenho é o da cópia), `quadro`, `frame[2]`, `fbp`, `texCount`, `skipped` e `vu1Ms`. Sem a
divisão ele zera os contadores da gravação como antes; com ela, não (a entrega zerou). O alvo mostrado se busca em
`q->targets` (`alvo_do_fbp`), sem o `GsDraw_TargetGet` (que escreve o `last`): com a divisão, a entrega o chama no lugar do
`frame_end`, no mesmo ponto da gravação (o mesmo `gGsFrame`), então o `last` e a escolha da vaga a reaproveitar não mudam.

**A entrega** (`entrega`, o gancho do `GsBackend`, na gravação com o desenho parado): é o ponto em que um lado passa ao outro o
que mudou, sem trava, porque o outro está parado (a barreira é o mutex da entrega).

1. O `last` do alvo mostrado (acima).
2. A conta da VU1 (`vu_plano`): o que cabe no par de buffers só depende da lista e da capacidade fixa (`sVuCapV`, `sVuCapI`),
   então a entrega a faz e guarda, por desenho, o índice no par (`sVuIni`), o primeiro vértice e quantos (`sVuLo`, `sVuNv`,
   512 KB a mais); o desenho só copia (`vu_copia`), sem varrer os índices de novo. Com ela, **a regra do `cleared` da gravação**
   (o caso do `sVuIni ~0`): `limpa[t]` só para o alvo com algum desenho não nativo que o desenho vai fazer; o da VU1 que não
   coube não conta. É a regra que o `frame_end` aplicava ao entrar na EDRAM.
3. As profundidades novas (`prof_garante`) e as vagas sem textura (`alvos_sem_textura`, que desliga também o `limpa` da vaga
   que ganhou textura): as duas abrem espaço no cache pelo `GsDraw_X360Abre`, agora só na gravação (riscos 3 e 7). Diferença do
   degrau 1: a nova tentativa da vaga sem textura acontece no fim da gravação do quadro, não no fim do desenho, então o
   `cleared` zerado dela vale um quadro depois (caso raro: só com a memória esgotada).
4. A medida da memória do quadro (`mem_mede`, a base da estimativa da gravação; o desenho mede por `livres_agora`, sem
   escrever na medida da gravação).
5. As texturas das vagas para o lado do desenho (`sincroniza_alvos`, risco 6): `sAlvos`/`sProf` são da gravação (criação pelo
   XG, `target_color`, a fila), `sAlvoD`/`sProfD` do desenho (a textura e o `conteudo`). Cada troca na gravação (`target_ensure`,
   a vaga solta, a vaga que ganhou textura, a profundidade criada ou solta) sobe a versão (`ver`) da vaga; na entrega, a vaga cuja
   cor mudou esquece o conteúdo dela e o da profundidade, a cuja profundidade mudou, o da profundidade. Como o próximo quadro que
   o desenho desenha é o entregue, isto é "no começo do quadro certo". O `sResidente`, o `sZDono` e o `sDateFonte` não precisam
   de evento: entre dois quadros eles são sempre -1 e NULL (o `frame_end` acaba com `sai_da_edram` e `prof_entrega` e começa
   zerando o `sDateFonte`). Sem a divisão, a mesma sincronização roda no começo do `frame_end`.
6. A fila das profundidades soltas troca de lado (`sProfFila`, abaixo) e os contadores do trecho que a gravação escreve passam
   para os do log (`grav_junta`: pressões, descartes, brancas, texturas novas e recusadas, e os lotes e instruções da VU1 do
   `gs_vu1.c`, que o log zerava de outra thread).

**A liberação** (risco 4, a fila da etapa 3 no 360): `soltaPede` (na gravação) passa os bytes de vivos a pendentes já
(`cova_conta`; os pendentes mudam dos dois lados, então são atômicos, `pend_soma`) e, na vaga reaproveitada, tira as texturas
da vaga (o `targetEnsure` seguinte cria as novas); `solta` (no desenho, depois de desenhar o quadro do pedido) põe o recurso na
cova (`cova_poe`), que o solta quando a GPU o largar. A cor da vaga viaja na fila do `gs_draw.c` (o `tex` que ele guarda é o
`target_color` da vaga); a profundidade, que não cabe no `GsSolta`, numa fila do backend com dois lados (`sProfFila`), posta na
cova no fim do `frame_end` do quadro em que foi pedida. O cabeçalho que lê a profundidade como cor passou a morar com ela
(`ProfXg`), e a cova espera os dois cabeçalhos (`xg_ocupado`, `xg_espera`): o `prof_solta` não bloqueia mais.

**O descarte por pressão** (risco 8): com a divisão, a cova é do desenho, então o `GsD3d_Falta` não a varre e o
`GsD3d_Descartou` não espera a GPU: pede ao desenho (`espera_desenho`: `sPedeLivre` e acorda o ocioso; o que está desenhando
atende no fim do quadro) e espera um evento, com limite de 100 ms. Não espera quando tudo o que está pendente foi pedido desde a
última entrega (`sPendGrav`: viaja com o quadro em gravação, e só sai depois de uma entrega que esta espera atrasaria), nem
depois de atendido com a cova vazia. As esperas da gravação pelo desenho saem no log à parte das esperas pela GPU.

**As threads no `main360.cpp`.** `Port_GsDesenhoPartida` (o primeiro que a thread de desenho faz): arredondamento por
truncamento como a gravação, a thread de hardware pelo `BT3_GS_DESENHO_HW` (1 ou 3; outro valor vira 3, com uma linha no log) e
a identidade. A saída em ordem: o `Port_Sair` chama o `Port_GsParar`, que espera a gravação, depois para o desenho (ele desenha o
último retrato, solta o dispositivo e sai) e por fim a gravação; a main toma o dispositivo como antes (até 2 s, depois à força).
O `AoMorrer` ganhou o ramo "caiu o desenho" (solta o dispositivo, `GsDraw_DesenhoCaiu` para a gravação não esperar por ele, e os
quadros seguintes se perdem); a queda da gravação pede a parada do desenho sem esperar (`GsDraw_DesenhoPararJa`: ele sai na
volta seguinte, sem desenhar o retrato pendente, e solta o dispositivo); a queda do jogo pede as duas. O vigia diz as três
threads e as duas etapas: `gGsD3dEtapa` (o desenho) e `gGsD3dEtapaGrav` (a parte do backend que roda na gravação: criando
alvos e profundidades, esperando o desenho soltar memória, a entrega). O `gs_core.c` só mudou no trabalho de parar da gravação:
com a parada pedida sem esperar (`Port_GsPararJa`), ela para o desenho antes de sair.

**O log novo, com a divisão:** `gs_d3d: o dispositivo é da thread N (a de desenho, BT3_GS_DESENHO=1)` no init, `thread de
desenho do GS (BT3_GS_DESENHO) na thread de hardware 3` (ou 1), `gs: BT3_GS_DESENHO=1: o desenho numa terceira thread, retrato de
10304 KB`, e a cada 60 quadros, depois da memória, `gs: divisão: N retratos, N sem cópia (não couberam), N pedidos de soltar;
maior quadro ...; entrega média X ms, pior Y ms` (o pior é do trecho). A linha `gs_d3d:   descarte:` ganhou "N da gravação pelo
desenho". Na linha de 60 quadros, "jogo e gravação" vira, com a divisão, o tempo em que o desenho ficou esperando retrato.

**A build do 360** (`build_f2.sh` no worktree `bt3xbx-d2`, o código do `ea1a01e`): 317 de 317 fontes sem erro e de primeira, os
avisos por fonte iguais aos da main (0 fontes com avisos diferentes; `gs_d3d.cpp` e `main360.cpp` sem nenhum), os mesmos 103
stubs (`diff` com o `stubs.txt` da main vazio), `default.dat` `6d30baaf87b1177fb2359a855f8befc0` e `default.swap`
`a7cf723fbfaea860fcc3707ad288e755`, iguais. O `default.xex` da rodada no console é o da build refeita no commit da nota (o md5
está no relatório da etapa; muda a cada build, pela data embutida). Nas mensagens novas do `gs_draw.c` nenhuma string tem acento
(o `c89.py`, guia, seção da etapa 3); o `.c` preparado em `build/x360/f2/c89` foi conferido no `desenho_thread`.

**As provas rápidas na x86 de teste** (`x86gv`, WSL, commit `ea1a01e`; script `~/provas-d2e4-x86.sh`, resumo em
`~/provas-d2e4-x86/resumo`): só o lado portável, que mudou no `gs_draw.c` e no `gs_core.c` dentro de `#ifdef PORT_X360` (no PC
e no falso a divisão segue igual). Base: a mesma build sem thread; t1 o degrau 1, d0 e d20 a divisão com a gravação atrasada em
0 e 20 ms, r20 a divisão com o desenho atrasado em 20 ms, todas com `BT3_GS_RACE=1`.

| Roteiro | Casos | Quadros diferentes | `race:` | Texturas usadas depois de soltas | Hash do jogo |
|---|---|---:|---:|---:|---|
| `f5_treino` (7.386) | t1, d0, d20, r20 | 0 em todos | 0 | 0 | `f76f0f6849b64629` em todos (o da base) |
| `f5_duelo` (8.380) | t1, d0, d20, r20 | 0 em todos | 0 | 0 | `08a98b6d05b8775a` em todos (o da base) |

`divergencias.py` saiu com 0 nas oito; a build `x86gv` com os 104 stubs de sempre. Na `d20` do `f5_duelo`: 8.380 retratos, 0 sem
cópia, entrega média 0,219 ms, pior 3,011 ms. O WSL não prova nada do `gs_d3d.cpp` nem do `main360.cpp` (o falso não tem
dispositivo): a posse, o `Present`, a Guide, a cova no desenho, a espera por memória e as quedas só o console mostra.

**A rodada do critério (serviço `claude-d2e4` no WSL, disparado sem esperar).** O script `~/rodada-d2e4.sh` (usuário `codex`)
refaz as três builds do HEAD do `f5-degrau2` (`headless.py --alvo x86`, `--alvo x86 --rastreio`, `--alvo ppc`, nessa ordem),
confere a `.text` da x86 plana contra `cc46fa0752f092fca305bc8bf2e2e54390763af0` e roda, em série, `REF=1 SEMMAPA=1 IMG=1
roteiros.sh f5_duelo f5_treino` e `SEMMAPA=1 IMG=1 roteiros.sh duelo`. Saída do serviço em `~/rodada-d2e4.log`, logs em
`~/rodada-d2e4-build-*.log`, `~/rodada-d2e4-f5.log` e `~/rodada-d2e4-duelo.log`; o resumo em `~/rodada-d2e4.resumo`, uma linha
por passo com `RC=` e a linha da `.text` ("a mesma" ou "DIFERENTE"), terminando com `FIM`. Vale com os cinco `RC=0`, a `.text` a
mesma e o git sem alterados no fim. Leva umas 2 h 40.

**O que só o console prova** (o `bt3xbx.log` de cada abertura; a 3 é a referência da mesma sessão):

1. Nas aberturas 1 e 2: `thread de desenho do GS (BT3_GS_DESENHO) na thread de hardware 3` (na 2, `1`), `gs: BT3_GS_DESENHO=1: o
   desenho numa terceira thread, retrato de 10304 KB` e `gs_d3d: o dispositivo é da thread N (a de desenho, BT3_GS_DESENHO=1)`;
   na 3, `(a de gravação ou a do jogo)` e nenhuma das outras duas.
2. Os autotestes do init (calibração da memória, profundidade, tempos, nativas) com os números da etapa 2: agora rodam na
   thread de desenho.
3. O resumo de 60 quadros na luta: o quadro inteiro (o período) contra os 48 ms do degrau 1 (a estimativa é de 38 a 42 ms: o
   período vira o da gravação); o "fim do quadro" (o desenho, uns 12,5 ms) e, com a divisão, "jogo e gravação" é o tempo em
   que o desenho esperou retrato. Os passos da luta (mediana, p95, p99) como na tabela da etapa 2, e a comparação entre a
   thread de hardware 3 e a 1 (o estudo pede as duas).
4. A linha `gs: divisão:` a cada 60 quadros: quantos não couberam (esperado 0), a entrega média e a pior no 360 (estimada em 1,5
   a 2 ms; na x86 do WSL, 0,2 ms).
5. A memória: as linhas `gs_d3d: pressão` e `gs_d3d:   descarte:` (as esperas da gravação pelo desenho, novas; nenhum `NÃO
   CABE`), o mínimo livre visto contra o piso (31 MB com piso 32 no degrau 1: as liberações agora chegam um quadro e meio mais
   tarde, e o retrato ocupa 10 MB), nenhuma linha `BRANCA`, `ficou sem textura` ou `a fila das profundidades soltas encheu`.
6. As capturas `BT3_SHOT=600` (`game:\shots`): a luta e os menus sem textura errada, preta ou branca; o alvo de um quadro com
   o conteúdo dele (o risco 6: uma vaga reaproveitada mostraria o conteúdo antigo ou um retângulo preto).
7. A Guide: abre no menu, na carga e na pausa, nas três aberturas; nenhuma linha `vigia:` além das de carga que o degrau 1 já
   tinha (se houver, ela diz as três threads e as duas etapas).
8. A saída: `saída do jogo com código 0` e `voltando ao dashboard`, sem `o dispositivo ainda era da thread N: tomado` (a de
   desenho soltou) e sem `CRASH`. Uma queda diz a thread (`a queda foi na thread de desenho` ou `de gravação`) e a etapa.

**Roteiro da rodada no console (três aberturas).** Copiar do worktree `bt3xbx-d2`, de `build/x360/f2/`, o `default.xex`, o
`default.dat` e o `default.swap` para a pasta do jogo. Para cada abertura, copiar o `ambiente_abN.txt` como `ambiente.txt` (todos
começam com `-BT3_DEMO` e trazem o `BT3_GS_THREAD=1` e o `BT3_SHOT=600`): 1, `BT3_GS_DESENHO=1` e `BT3_GS_DESENHO_HW=3`; 2,
`BT3_GS_DESENHO=1` e `BT3_GS_DESENHO_HW=1`; 3, sem o `BT3_GS_DESENHO` (o degrau 1). Abrir pela FreeStyle; pular o filme de
abertura com START (a tela preta depois das logos é o filme, contado e não desenhado); abrir a Guide no menu principal e fechar;
modo Duelo, Goku contra Kuririn, estágio 5; abrir a Guide durante a carga e fechar; uns 60 s de luta (golpes especiais, troca
de personagem se houver); pausa, abrir a Guide e fechar; sair pela pausa e voltar ao dashboard. Trazer o `bt3xbx.log` de cada
abertura (renomeado `bt3xbx_abN.log` antes da seguinte) e a pasta `shots` (as capturas levam o número do quadro do GS; separar
por abertura).

**O que fica em aberto.** Tudo do lado do dispositivo só o console confere: a posse na thread de desenho, o `Present`, a
repetição para a Guide, a cova e o `xg_libera` no desenho, a espera da gravação por memória (o limite de 100 ms e o pedido ao
desenho ocioso nunca rodaram) e os ramos de queda. Diferenças de comportamento que se aceitaram: a nova tentativa da vaga sem
textura vale um quadro depois (só com a memória esgotada); as liberações chegam à cova um quadro e meio mais tarde que no
degrau 1; um `target_color` de vaga reaproveitada sem textura conta uma branca a mais no log (a fila do `gs_draw.c` pede a cor da
vaga); 512 KB a mais de memória fixa para a conta da VU1 (`sVuLo`, `sVuNv`).

### A rodada do critério (20/10/2026) [medido]

No WSL, serviço `claude-d2e4`, HEAD `c79548b`, de 20:54 a 23:51: as três builds do HEAD (x86, x86 com rastreio, PPC), a `.text`
da x86 `cc46fa0752f092fca305bc8bf2e2e54390763af0` (a mesma), `REF=1 SEMMAPA=1 IMG=1 roteiros.sh f5_duelo f5_treino` com RC 0 e
"tudo certo" (11 de 11 imagens idênticas no treino), `SEMMAPA=1 IMG=1 roteiros.sh duelo` com RC 0 e "tudo certo" (9 de 9), o git
sem alterados no fim. Saídas de `port/build/roteiros` apagadas depois de conferidas. Falta a rodada no console (as três aberturas).

### A rodada no console (21/10/2026) [medido]

O `.xex` `a9dfad59276f216acfbd0b1be733c4c3` (`c79548b`), três aberturas, Duelo, Goku contra Kuririn, estágio 5, START no filme,
pausa e Guide. A medida é a mediana da janela da luta (`passos: janela`, fechada pela pausa), não a média dos trechos de 60 quadros
do `gs_d3d`, que mistura a pausa e as cenas paradas.

| Abertura | Luta, mediana | p95 | p99 |
|---|---:|---:|---:|
| 3. Degrau 1 (`BT3_GS_THREAD=1`) | 47,4 ms | 73,0 | 104,5 |
| 2. Degrau 2, desenho na thread de hardware 1 | 45,6 ms | 66,7 | 87,7 |
| **1. Degrau 2, desenho na thread de hardware 3** | **38,4 ms** | **59,5** | **78,6** |

- **O desenho na 3 tira 19% da luta** (de uns 21 para uns 26 quadros por segundo), dentro da estimativa (38 a 42 ms). Na 1, irmã da
  gravação, o ganho quase some: o desenho divide o núcleo com a thread mais lenta. Fica a 3 como padrão.
- Sem queda, sem linha do vigia; a Guide abriu no menu e na carga com a thread de desenho dona do dispositivo; 7.500 retratos sem
  nenhum fora da área; nenhuma textura branca vista na tela (o log conta 4 num trecho de pressão de memória na abertura 1).
- O descarte trabalha mais que no degrau 1 (43 e 19 pressões contra 5): as liberações chegam um quadro e meio depois.
- Falta para os 30 fps: uns 5 ms. Com o desenho fora, o período é o da gravação (andar a lista e a VU1): é a outra frente do estudo. O inventário dela (onde vai o tempo e o que cortar, em ordem) está em [`08-gravacao.md`](08-gravacao.md).
