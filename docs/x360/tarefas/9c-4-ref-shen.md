# Tarefa 9c-4: Referência de personagens e Shenlong

[Índice do plano](../../plano.md) | etapa: [09c.md](../plano/F4/09c.md)

Branch: `codex/9c-ref` (a partir de `codex/9c-torneio`, a tarefa 9c-3 ainda sem revisão, com a `troca/main` por cima).
Relatório: `~/entrega-9c-ref.md`. Orçamento: 4 a 6 h.

## O que fazer

As últimas chamadas de `Sprite_Unpack`, pela mesma linha de produção da 9c-1 a 9c-3 (leia as notas delas em
`docs/x360/plano/F4/09c.md` antes de começar):

| Função | Arquivo |
|---|---|
| `CharRef_Init` (2 chamadas) | `src/menu/menu_h_b.c` |
| `CharRef_UpdateImage` | `src/menu/menu_h_b.c` |
| `Shen_Init` | `src/sys/late_a.c` |
| `ShenSave_Init` | `src/sys/late_a_c.c` |

O inventário em `docs/x360/formatos/menus-modos.md` propõe `menu_charref` (463 s1, N = 28, com tabelas s32 de 161
entradas), os retratos como `menu_texfile` e `menu_shenlong` (472 s2, modos 70 e 71). Dois pontos já sabidos:

1. **O leitor da seção 2 do 472 é assembly** (`Shen_BuildList`): o layout no inventário é hipótese. Prove o layout
   pelo assembly (em `asm/`) e pelos dados antes de escrever o conversor, e registre a prova na especificação.
2. **O roteiro `shenlong.txt` não chega ao `ShenSave_Init`**: o save do Shenlong só vem depois de um desejo concedido,
   e com tudo desbloqueado o jogo recusa os desejos (os prêmios já são do jogador). Para alcançá-lo, um passo de teste
   que deixe pelo menos um desejo livre antes da tela, com as mesmas proteções do `desbloqueia` e do `funde discos`
   (só nas builds de teste, a `.text` só muda por código de teste). Se isso não sair de forma simples e provada em
   1,5 h, feche a Referência e o `Shen_Init` e registre o `ShenSave_Init` como sem roteiro, com o motivo.

## Critério

- Especificação dos formatos novos em `docs/x360/formatos/` (um arquivo novo, como `menus-dc.md` da 9c-2).
- Conversores e ganchos: o `unpacks.py` sem nenhuma pendente (é a última tarefa da etapa 9); cada conversor visto
  falhando sobre uma cópia estragada.
- `conv_tool --disco` sem erro e a 100% em todas as faixas de menu.
- Os roteiros na PPC sem mapa iguais à x86 em rastro, leituras, memória por tela (60, 70 e, se alcançada, 71), save e
  imagens dos menus: `REF=1 SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh referencia` e o mesmo com `shenlong`.
  O modelo 3D da Referência (ChrView) e a cena do dragão podem ficar fora das imagens se dependerem da luta (F5):
  registre quais.
- Controle de regressão: `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh duelo`, o mesmo com `torneio`, e
  `port/x360/tools/save_mc.sh`, "tudo certo". A rodada com todos os roteiros fica para o fim da F4.
- A `.text` da x86 (a referência que a nota da 9c-3 deixou; antes dela, `ecfc82aabe1b6f56c3b3775c34530af596449dca`):
  só muda por código de teste, provado pelos objetos.
- Ao fim, um parágrafo "Tarefa 9c-4" em `docs/x360/plano/F4/09c.md` com a tabela da rodada e as perguntas.

## Dicas das tarefas anteriores

- Telas com o trabalho em variável local: a tabela `OWNER_NA_PILHA` do `roteiros.py` (9c-2), uma linha por tela, com
  prova.
- Funções de double da libm chamadas pelo código do jogo (o `pow` da 9c-2) quebram as builds sem janela.
- Espere os emuladores com o processo em segundo plano e consultas espaçadas.

## Fora desta tarefa

O Disc Fusion no 360 (resposta 1 da 9b), que outro agente faz em paralelo, fora do WSL.
