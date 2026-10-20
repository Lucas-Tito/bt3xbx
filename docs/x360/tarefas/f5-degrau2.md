# Tarefa: F5, etapa 8, o degrau 2 da divisão em threads (o desenho numa terceira thread)

[Índice do plano](../plano.md) | estudo: [F5/08-threads.md](../plano/F5/08-threads.md)

Branch: `codex/f5-degrau2` (a partir de `troca/main`, com o degrau 1: `5a7d98c` ou depois). Relatório: `~/entrega-f5-degrau2.md`.
Orçamento: 5 a 8 h de agente, mais a rodada final.

Leia antes, inteiro, `docs/x360/plano/F5/08-threads.md`: as seções 4.2, 4.3 e 5 (o corte C2, a opção (c + a)) e a seção
"Degrau 1", com a rodada no console de 20/10. O degrau 1 pôs a gravação inteira numa thread (`BT3_GS_THREAD=1`) e a luta caiu
de 72,3 para 48,3 ms. Nessa thread, por quadro de luta: andar a lista e gravar, uns 38 ms; o fim do quadro (`frame_end`, o
desenho no D3D), uns 12,5 ms. O degrau 2 tira o fim do quadro dessa thread: a gravação do quadro N+1 corre enquanto uma terceira
thread desenha o N. Estimativa: de 48 para uns 38 a 42 ms. Não fecha os 30 fps sozinho (a gravação, uns 38 ms, vira o período).

## O que o XDK diz (conferido em 20/10, `xbox360sdk.chm` do XDK 21256)

- **Só a thread dona do dispositivo chama o dispositivo** (`AcquireThreadOwnership`: "Only the thread that owns the Direct3D
  device can make function calls using that device object"), e o 360 **não aceita** `D3DCREATE_MULTITHREADED`. Então toda
  chamada a `sDev->...` (desenho, alvos, `CreateTexture`, shaders, `Present`) fica na thread de desenho.
- **Uma textura se cria sem o dispositivo:** `XGSetTextureHeader` (cabeçalho em memória comum) mais os dados em `XPhysicalAlloc`
  e `XGOffsetResourceAddress` (ou `XGOffsetBaseTextureAddress`). O `gs_d3d.cpp` já usa esse caminho (linhas perto de 208 e de
  1092). Com formato `D3DFMT_LIN_*`, o envio é um `memcpy` linha a linha, sem `LockRect`. Liberar (`XPhysicalFree`) só depois
  de a GPU ter acabado de ler: pela regra dos dois quadros do descarte, ou por cerca (`InsertFence`/`BlockOnFence`), na thread
  de desenho.

## Faça, nesta ordem, cada parte com commit próprio

1. **A entrega da gravação para o desenho, portável e provada no WSL.** O `frameEnd` do backend passa a consumir uma lista
   entregue, não a que a gravação está montando. Escolha conservadora (o Lucas não encolhe as capacidades agora; decisão 24,
   resposta 2): as listas de gravação ficam como estão (66,4 MiB) e **a entrega copia só a parte usada** para uma área do
   desenho com o dobro dos máximos medidos; um quadro que não couber é desenhado na própria thread de gravação, como no
   degrau 1 (nunca perder um quadro, nunca truncar). Máximos medidos: nos roteiros da etapa 6, 13.182 vértices de CPU, 53.618
   de VU, 128.772 índices, 2.224 constantes, 2.454 desenhos; no console em 20/10 (as duas aberturas), 6.834, 53.618, 128.772,
   2.228 e 2.378. Meça também nos `f5_duelo` e `f5_treino` e use o maior. Conte no relatório o tempo da cópia (estimado em
   1,5 a 2 ms) e quantos quadros não couberam.
2. **O que a gravação e o desenho dividem, cada um com dono** (seção 4.3): a tabela `gsTargets` (separar "tem imagem para a
   gravação" de "tem imagem na textura do alvo"), o cache de texturas e o descarte (com o `gGsFrame` e o contrato dos dois
   quadros), a `cova`, os contadores `sMem` e o `sTexBucket`. As entradas do `GsBackend` que a gravação chama
   (`texCreate`, `texUpload`, `texDestroy`, `texSlotsLeft`, `targetEnsure`, `targetDrop`, `targetColor/Aux/Depth`, `pipeGet`):
   para cada uma, diga de que lado roda e por quê. Texturas pelo caminho do XG na gravação; alvos e pipelines viram pedidos
   resolvidos na thread de desenho (o número que a gravação recebe é uma vaga, preenchida antes de o quadro ser desenhado).
3. **A thread de desenho no 360** (`port/x360/src`): dona do dispositivo (o `d3d_init`, o `Present`, o `GsD3d_Repete` da
   Guide, o `GsD3d_Soltar` na saída e na queda passam para ela); a de gravação sem o dispositivo. Chave nova no `ambiente.txt`,
   `BT3_GS_DESENHO=1`, que só vale com `BT3_GS_THREAD=1`; sem ela, tudo como no degrau 1. Thread de hardware do desenho por
   `BT3_GS_DESENHO_HW` (padrão 3, a irmã do jogo; a 1, irmã da gravação, para comparar: o estudo manda medir as duas; nunca a 4
   nem a 5, do áudio). O vigia diz o estado das três threads. Você NÃO consegue compilar o 360: C89 do `cl.exe` e o C++ do
   backend com cuidado, como no degrau 1, e diga no relatório o que não pôde ser conferido.

## Como provar no WSL

O backend falso (`gpu_falso.c`) ganha o mesmo arranjo: o `frameEnd` numa terceira pthread, consumindo a lista entregue. A prova é
a do degrau 1: `divergencias.py` contra a build sem as threads, nos `f5_duelo` e `f5_treino`, em `x86gv` e `ppcg`, com zero
quadros diferentes, zero linhas `race:`, o fim do roteiro e os contadores iguais; com a gravação atrasada (`BT3_GS_ATRASO_US`)
**e** com o desenho atrasado (uma chave de teste nova, por exemplo `BT3_GS_DESENHO_ATRASO_US`), cada uma a 0, 5 e 20 ms. Provas
negativas, cada uma vista falhando: a entrega sem a cópia (o desenho lendo a lista que a gravação reescreve), uma textura
liberada sem esperar os dois quadros, e o pedido de alvo resolvido depois do desenho.

## Critério

- No commit final: as três builds refeitas, `REF=1 SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh f5_duelo f5_treino` e
  `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh duelo` tudo certo, um emulador por vez; a `.text` da x86
  (`cc46fa0752f092fca305bc8bf2e2e54390763af0`) só muda por código de teste, provado pelos objetos.
- Cada verificador novo visto falhando de propósito (commit antes de estragar; confira com `assert` que a sabotagem casou).
- Ao fim, a seção "Degrau 2" no `08-threads.md`: o que foi feito, as provas, a estimativa no 360, o que falta compilar e conferir
  no console e o roteiro da rodada. O `ambiente.txt` do roteiro começa com `-BT3_DEMO` (sem ela o jogo abre na demo) e tem as três
  aberturas: `BT3_GS_THREAD=1` com `BT3_GS_DESENHO=1` e `BT3_GS_DESENHO_HW=3`, o mesmo com `BT3_GS_DESENHO_HW=1`, e só
  `BT3_GS_THREAD=1` (o degrau 1, para comparar).

## Rodada final e limites

- **A rodada final dos roteiros (2 a 3 h) roda num serviço e você para depois de dispará-la**: diga no relatório o nome do serviço,
  os logs e o que conferir. A análise do resultado é feita de fora.
- O WSL tem uns 7 GB de memória e o disco do Windows é limitado: processe arquivo por arquivo e apague as saídas grandes de
  `port/build/roteiros` depois de cada rodada conferida. Ao final: relatório em `~/entrega-f5-degrau2.md`,
  `git push troca codex/f5-degrau2`, git limpo, nenhum processo seu além do serviço da rodada final.
