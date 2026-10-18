# Tarefa: estudo da divisão em threads da gravação e do desenho (F5, etapa 8)

[Índice do plano](../plano.md) | fase: [F5/00-escopo.md](../plano/F5/00-escopo.md), etapa 8

Branch: `cursor/f5-threads-estudo` (a partir de `troca/main`). Relatório: `~/entrega-f5-threads-estudo.md`.
Orçamento: 1,5 a 2,5 h.

## Limite

Só leitura e escrita de documento, mais scripts de leitura de poucos segundos. Nada de build, emulador nem mudança de
código: o entregável é o estudo, para o Lucas decidir a arquitetura.

## O ponto de partida (medido no console)

A seção "Rodada no console (18/10/2026)" de `docs/x360/plano/F5/08.md`: na luta, o quadro leva 75,7 ms; a thread do
jogo gasta 61,2 (a lógica, uns 15 ms na F2, decisão 17; a gravação da VU1, 26,3; o resto da gravação do GS, uns 20) e
o fim do quadro na CPU mais 14,5 (a cópia da VU1, 6,5; a vu0, 6,3); a GPU usa uns 17 ms. A GPU não é o gargalo: a CPU
em série numa thread é. O Xenon tem 3 núcleos e 6 threads de hardware; hoje o jogo usa praticamente uma (a 2), o
áudio da F6 vai para a 4 e a 5 (`CLAUDE.md`, lições do 360), o diário escreve na 1.

## O que responder

1. **Onde dá para cortar a série:** o que a thread do jogo faz em cada quadro, na ordem (a lógica, a gravação do GS no
   `port/src/gs/gs_core.c` e `gs_draw.c`, a VU1 no `gs_vu1.c`, o fim do quadro no `port/x360/src/gs/gs_d3d.cpp`), e
   quais dados cada parte lê e escreve. Que ponto de corte deixa a lógica do quadro N+1 rodar enquanto o quadro N é
   gravado ou desenhado em outra thread, e o que precisa ser copiado ou travado para isso (a VRAM do GS, as listas de
   desenho, os pacotes da VU1, as texturas decodificadas, o estado do GS).
2. **O que já existe e por que falhou:** o caminho experimental `BT3_GS_THREAD` do port de PC (o plano da F5 registra
   que o jogo alterava dados ainda referidos pela lista e a câmera saltava), a decisão 24 (resposta 5: a thread do jogo
   dona do D3D) e o vigia da `main360.cpp`. O que exatamente o jogo altera depois de entregar e como evitar.
3. **As opções**, cada uma com o ganho estimado pelos números acima, o custo em dias de agente, a memória a mais, os
   riscos e como provar que nada mudou na imagem e na lógica (o WSL tem a build `--gpu` com o backend falso da etapa 8,
   `port/x360/headless/gpu_falso.c`, que compara a lista de desenhos quadro a quadro): por exemplo (a) só o fim do
   quadro numa thread de desenho; (b) a gravação da VU1 e o fim do quadro numa thread, a lógica e o resto na do jogo;
   (c) a gravação inteira do GS numa thread, com a VRAM em dois buffers; (d) dividir a VU1 entre threads por lote.
   Diga a sua recomendação e por quê, sem decidir pelo Lucas.
4. **O que fica no código do 360** (precisa do compilador do Xbox, outra máquina) e o que é portável e se prova no WSL.

## Entregável

`docs/x360/plano/F5/08-threads.md`, em português, sem travessão, ligado de uma linha no fim do `08.md`. Ao final:
relatório em `~/entrega-f5-threads-estudo.md`, `git push troca cursor/f5-threads-estudo`, git limpo, nenhum processo seu.
