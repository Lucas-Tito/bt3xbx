# Tarefa F5-etapa 6, triagem: os defeitos do cenário no console

[Índice do plano](../plano.md) | etapa: [F5/06.md](../plano/F5/06.md)

Branch: `cursor/f5-vu1-triagem` (a partir de `troca/f5-vu1`). Relatório: `~/entrega-f5-vu1-triagem.md`.
Orçamento: 1 a 2 h.

## O que aconteceu

A segunda parte da etapa 6 ligou no 360 as famílias vu4 (estágio) e vu6 (sombra no chão) da VU1, em HLSL traduzido
do port de PC (leia o `docs/x360/plano/F5/06.md` inteiro). Na rodada no console (Duelo, Goku (Early) contra Kuririn,
estágio 5, o mirante do Kami), o Lucas viu:

1. o cenário brilhante demais, lavado;
2. o cenário em baixa resolução e borrado em alguns lugares;
3. texturas pretas em alguns lugares do cenário;
4. os lutadores sempre por cima do cenário, mesmo com coisa na frente; dá para ver dentro do chão do mirante;
5. as sombras por cima do personagem e pretas demais;
6. a UI das vidas esquisita (já estava assim antes da vu4/vu6).

Os itens 4 e 5 (e talvez o 6) são, em princípio, da etapa 7 da F5 (profundidade persistente e o byte de alfa:
`docs/x360/plano/F5/00-escopo.md`, etapa 7 e "Profundidade e alfa são uma entrega própria"). Os itens 1 a 3 parecem
defeitos novos da tradução dos shaders ou do caminho das texturas.

## Dados (só leitura)

- As capturas do console, em `/srv/bt3/dados/capturas/`: `shots_etapa6b/` (esta rodada, com o cenário; uma captura a
  cada 600 quadros do GS, PPM) e `shots_etapa6/` (a rodada anterior, só os lutadores), e os logs `etapa6_r2.log` e
  `etapa6_r1.log`.
- A referência: `/srv/bt3/dados/capturas/referencia_f5_duelo/`, as imagens do rasterizador de software da x86 do
  roteiro `f5_duelo` (a mesma luta, o mesmo estágio), que sai idêntica à PPC: é como a luta deve ficar.

## O que fazer

1. **Triagem com prova:** compare as capturas do console com a referência (os quadros não são os mesmos, mas a luta,
   o estágio e a câmera são parecidos: compare regiões do cenário, brilho médio e histograma, nitidez, áreas pretas,
   com scripts de poucos segundos). Para cada item de 1 a 6, diga a causa provável e se é da etapa 6 (tradução dos
   shaders, constantes, texturas) ou da etapa 7 (profundidade e alfa), com a prova.
2. **Leia lado a lado** o shader do PC (`vu4.vert`, `vu6.vert`, os fragment shaders que o PC usa com eles, e como o
   backend do PC preenche as constantes e as texturas desses desenhos) e o HLSL do 360 (`port/x360/src/gs/gs_d3d.hlsl`:
   `vs_vu4`, `vs_vu6`, o `ps_gs`) e o caminho no `port/x360/src/gs/gs_d3d.cpp` (o `prepara`, as texturas e os
   samplers desses desenhos). Suspeitas a conferir, não a assumir: a escala da cor (no PS2, 0x80 vale 1,0; um fator
   dobrado lava a imagem), a névoa, o nível de detalhe e o filtro das texturas (as versões menores usadas de perto),
   paletas e formatos das texturas que saem pretas.
3. **Corrija o que for da etapa 6** no HLSL e no `gs_d3d.cpp`, com comentário curto do porquê. Você NÃO consegue
   compilar o shader do 360 (o compilador do Xbox fica em outra máquina): escreva com cuidado, no mesmo estilo do
   `vs_vu0` que já funciona, e diga no relatório o que não pôde ser conferido. Não mexa no que for da etapa 7.
4. Registre a triagem no `docs/x360/plano/F5/06.md`, numa seção "Triagem da segunda rodada", com a tabela dos itens,
   a causa, a etapa e o que foi corrigido.

## Limites

Sem `headless.py`, `roteiros.sh`, `qemu-ppc`, `gdb` nem build: só leitura, scripts curtos de análise de imagem e as
edições. Nada de bytes do jogo no repositório (as capturas e a referência ficam em `/srv/bt3/dados`). Ao final:
relatório em `~/entrega-f5-vu1-triagem.md`, `git push troca cursor/f5-vu1-triagem`, git limpo, nenhum processo seu.
