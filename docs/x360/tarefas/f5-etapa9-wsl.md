# Tarefa F5-etapa 9, a parte do WSL: o visualizador 3D da Referência e a cena do Shenlong

[Índice do plano](../plano.md) | fase: [F5/00-escopo.md](../plano/F5/00-escopo.md) | decisão 24, resposta 9

Branch: `cursor/f5-etapa9` (a partir de `troca/main`). Relatório: `~/entrega-f5-etapa9.md`. Orçamento: 3 a 5 h.

## O que fazer

A etapa 9 da F5 fecha a dívida 3D dos menus deixada pela F4 (`docs/x360/plano/F5/00-escopo.md`, etapa 9; a decisão
24, resposta 9: o mínimo, uma abertura real do `ChrView` e o dragão 1 com a lista e a recompensa). Esta tarefa é só
a parte do WSL; a do console vem depois da etapa 7.

1. O roteiro `port/x360/roteiros/referencia.txt` hoje percorre a lista, o retrato e os detalhes, mas não abre o
   visualizador 3D (`ChrView`; `docs/x360/plano/F4/09c.md`, nota da 9c-4). Estenda-o para abrir de verdade o modelo
   de um personagem (o Goku (Early) de preferência), girá-lo um pouco e voltar, com a tela do `ChrView` na memória
   por tela e nas imagens (`roteiros.py`, `TELAS_MEM` e `TELAS_IMG`), e o modelo real carregado.
2. A cena do Shenlong (`shenlong.txt`) já compara a lista e a recompensa sobre a cena (F5, etapa 4, `04.md`):
   confira que segue assim e que a memória da tela 70 está tipada sem exceção genérica.
3. A memória viva dessas telas comparada por tipo e dono, como a etapa 5 fez na luta (`docs/x360/plano/F5/05.md`),
   sem afrouxar a comparação: diferença real é erro de port a corrigir (no upstream só dentro de
   `#ifdef PORT_BIG_ENDIAN`, com o original no `#else`).

## Critério

- `REF=1 SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh referencia shenlong` tudo certo, com a tela do `ChrView` na
  memória e nas imagens idênticas entre a x86 e a PPC sem mapa; controle `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh
  duelo` tudo certo. As três builds refeitas no commit final, um emulador por vez.
- Cada verificador novo visto falhando de propósito numa cópia.
- A `.text` da x86 (referência `b7855304d8d39bb3e28e88ad6dc77476787f4943`) só muda por código de teste, provado pelos
  objetos.
- Ao fim, `docs/x360/plano/F5/09.md` com a tabela da rodada e as perguntas, e a linha da etapa 9 do `00-escopo.md`
  apontando para ele.

## Limites

O WSL tem uns 7 GB de memória: scripts processam arquivo por arquivo. Nada do 360 (`port/x360/src`), que fica para a
parte do console. Ao final: relatório em `~/entrega-f5-etapa9.md`, `git push troca cursor/f5-etapa9`, git limpo,
nenhum processo seu.
