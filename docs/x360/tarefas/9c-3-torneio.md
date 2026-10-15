# Tarefa 9c-3: Torneio

[Índice do plano](../../plano.md) | etapa: [09c.md](../plano/F4/09c.md)

Branch: `codex/9c-torneio` (a partir de `troca/main`). Relatório: `~/entrega-9c-torneio.md`. Orçamento: 4 a 5 h.

## O que fazer

As chamadas de `Sprite_Unpack` do World Tournament, pela mesma linha de produção da 9c-1 e da 9c-2 (leia as notas
das duas em `docs/x360/plano/F4/09c.md` antes de começar: elas economizam rodadas):

| Função | Arquivo |
|---|---|
| `EntrySel_Init` (2 chamadas) | `src/menu/menu_i_d.c` |
| `EntrySel_UpdateImage` | `src/menu/menu_i_d.c` |
| `TourMenu_Init` (2 chamadas) | `src/menu/menu_j_b.c` |
| `TourBg_Init` | `src/menu/menu_k_d.c` |
| `Bracket_Load` | `src/menu/menu_k_f.c` |
| `Bracket_LoadImages` (2 chamadas) | `src/menu/menu_k_f.c` |
| `Bracket_UpdateImages` (2 chamadas) | `src/menu/menu_k_f.c` |

`python3 port/x360/tools/unpacks.py` lista as pendentes. O inventário em `docs/x360/formatos/menus-modos.md` já
propõe os formatos (`menu_tourmenu`, 450 s1; `menu_entrysel`, 450 s2; `menu_tourbg`, 968..972; `menu_bracket`,
454..458; os convites 963..967 e os retratos, como `menu_texfile`). Regra 5 daquele arquivo: um conversor por pacote
cobre todas as variantes, não só a que a tela mostra (os cinco torneios no EntrySel e no Bracket, as seções 16 e 17 do
TourMenu sem convite). O `conv_tool --disco` prova isso.

## Critério

- Especificação dos formatos novos em `docs/x360/formatos/` (um arquivo novo, como `menus-dc.md` da 9c-2).
- Conversores e ganchos: o `unpacks.py` sem nenhuma pendente desta tabela; cada conversor visto falhando sobre uma
  cópia estragada.
- `conv_tool --disco` sem erro e a 100% nas faixas novas.
- O roteiro `port/x360/roteiros/torneio.txt` (já vai do TourMenu à primeira luta; ajuste-o para alcançar as 11
  chamadas) na PPC sem mapa igual à x86 em rastro, leituras, memória por tela (33, 34 e 35), save e imagens dos menus:
  `REF=1 SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh torneio`. A luta continua fora das imagens até a F5. Se alguma
  chamada só acontece depois de uma luta (a chave atualizada), use o comando que termina a luta, como os roteiros da
  9b; se mesmo assim não der, registre qual chamada ficou sem roteiro e por quê.
- Controle de regressão: `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh duelo` e `port/x360/tools/save_mc.sh`, "tudo
  certo". A rodada com todos os roteiros fica para o fim da F4.
- A `.text` da x86 (referência atual `ecfc82aabe1b6f56c3b3775c34530af596449dca`, desde a 9c-2): só muda por código de
  teste, provado pelos objetos.
- Ao fim, um parágrafo "Tarefa 9c-3" em `docs/x360/plano/F4/09c.md` com a tabela da rodada e as perguntas.

## Dicas das tarefas anteriores

- Telas que guardam o trabalho como variável local gravam ponteiros para a pilha nos donos dos filmes: a tabela
  `OWNER_NA_PILHA` do `roteiros.py` (9c-2) aceita isso só por tela, com os tamanhos exatos. Uma tela nova entra lá com
  prova, nunca com a regra afrouxada.
- `pow` e outras funções de double da libm chamadas pelo código do jogo quebram as builds sem janela (9c-2): se a
  build cair por SIGFPE ou der números errados, procure isso primeiro.
- Espere os emuladores com o processo em segundo plano e consultas espaçadas; não fique preso em laços curtos.

## Fora desta tarefa

O Disc Fusion no 360 (resposta 1 da 9b), a Referência e o Shenlong.
