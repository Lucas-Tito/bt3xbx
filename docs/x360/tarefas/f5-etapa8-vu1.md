# Tarefa F5-etapa 8, primeira parte: o custo da VU1 na CPU, no código portável

[Índice do plano](../plano.md) | fase: [F5/00-escopo.md](../plano/F5/00-escopo.md), etapa 8

Branch: `cursor/f5-etapa8-vu1` (a partir de `troca/main`). Relatório: `~/entrega-f5-etapa8-vu1.md`. Orçamento: 3 a 5 h.

## O ponto de partida (medido no console)

Na luta Goku (Early) contra Kuririn, estágio 5 (`docs/x360/plano/F5/06.md`, as duas rodadas), o quadro levou de 48
a 63 ms só com os lutadores e de 76 a 80 ms com o cenário (o alvo é 33,37 ms, `docs/x360/plano/F5/00-escopo.md`,
seção 3). O maior item medido é a **gravação da VU1, de 24 a 31 ms por quadro na CPU**: o tempo dentro do
`GsVu1_Call` (`port/src/gs/gs_vu1.c`, o relógio sob `PORT_X360`), que lê os pacotes da VU1 e monta os vértices dos
programas reconhecidos (`hle_program*`) antes de a GPU desenhar. O log do console está em
`/srv/bt3/dados/capturas/etapa6_r2.log` (linhas `VU1, lotes` e `quadro inteiro`).

## O que fazer

1. **Perfil no WSL:** onde vai o tempo dentro do `GsVu1_Call` na luta dos roteiros `f5_duelo`/`f5_treino`, por
   programa e por trecho de código (na x86 nativa com `perf` ou contagem de instruções; na PPC, que é o que mais
   se parece com o 360 em ordem de bytes, com contagem de instruções e de acessos à memória). O tempo da PPC emulada
   não é o do Xenon: use contagens como aproximação e diga isso.
2. **Otimizar o maior custo** no código portável, sem mudar o resultado: menos cópias, menos troca de ordem de bytes
   por vértice, menos chamadas por vértice, laços mais simples. O código é do upstream: toda mudança dentro de
   `#ifdef PORT_BIG_ENDIAN` (que a PPC do WSL e o 360 usam), com o original no `#else` (a x86 de referência não muda).
3. **Provar que nada mudou:** `REF=1 SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh f5_duelo f5_treino` e
   `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh duelo` tudo certo no commit final (a luta, a memória e as imagens
   iguais entre a x86 e a PPC), as três builds refeitas, um emulador por vez; a `.text` da x86
   (`cc46fa07…`, a referência atual) igual, porque a mudança é só em `PORT_BIG_ENDIAN`.
4. **Estimar o ganho** no 360 e dizer o que medir no console (as linhas do log que já existem: `VU1, lotes ... tempo
   por quadro: gravação`).

## Limites

Não mexa em `port/x360/src` (o backend do 360, compilado em outra máquina e mexido por outra frente). O WSL tem uns
7 GB: nada de carregar dados inteiros na memória. Ao final: `docs/x360/plano/F5/08.md` com o perfil, o que mudou, a
prova e a estimativa, a linha da etapa 8 do `00-escopo.md` apontando para ele, relatório em
`~/entrega-f5-etapa8-vu1.md`, `git push troca cursor/f5-etapa8-vu1`, git limpo, nenhum processo seu.
