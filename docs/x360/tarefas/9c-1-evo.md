# Tarefa 9c-1: Evolution Z e loja

[Índice do plano](../../plano.md) | etapa: [09c.md](../plano/F4/09c.md)

Branch: `codex/9c-evo` (a partir de `origin/main`). Relatório: `~/entrega-9c-evo.md`. Orçamento: 3 a 4 h.

## O que fazer

As chamadas de `Sprite_Unpack` do Evolution Z e da loja, pela mesma linha de produção da 9a e da 9b:

| Função | Arquivo |
|---|---|
| `EvoTop_Init` | `src/menu/menu_w_c.c` |
| `EvoZ_Init` | `src/menu/menu_u_g.c` |
| `EvoZ_Load` (2 chamadas) | `src/menu/menu_u_h.c` |
| `EvoZ_UpdateFace` | `src/menu/menu_v_b.c` |
| `Shop_Init` | `src/menu/menu_v_d.c` |

`python3 port/x360/tools/unpacks.py` lista as pendentes. O inventário em `docs/x360/formatos/menus-modos.md` já
propõe os formatos (`menu_evo_topo`, 452 s1; `menu_loja`, 452 s2; `menu_evoz`, 452 s3; o retrato e o fundo 959) e
diz quais seções não têm leitor achado. A seção 11 do 452 s1 é o diálogo do DcSave (veja o item 2 de "Perguntas" em
`menus-modos.md`): o `menu_mcflow` já tem gancho, confira se o DcSave passa por ele.

Também desta tarefa (resposta 5 do Lucas na 9b, em `docs/x360/plano/F4/09b.md`): o `conv_tool` passa a não contar como
divergente a leitura de 4 bytes num deslocamento ímpar sobre um byte (impossível no PS2, artefato do rastreio), e as
entradas desse tipo saem da `LARGURA_CONHECIDA`. Veja o verificador falhar de propósito.

## Critério

- Especificação dos formatos novos em `docs/x360/formatos/` (um arquivo novo, como `menus-ub.md` da 9b).
- Conversores e ganchos: o `unpacks.py` sem nenhuma pendente desta tabela; cada conversor visto falhando sobre uma
  cópia estragada.
- `conv_tool --disco` sem erro e a 100% nas faixas novas.
- O roteiro `port/x360/roteiros/evolution.txt` (da etapa 4; ajuste-o para alcançar as 6 chamadas, e a loja) na PPC
  sem mapa igual à x86 em rastro, leituras, memória por tela, save e imagens dos menus:
  `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh evolution`.
- Controle de regressão: `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh duelo` e `port/x360/tools/save_mc.sh`, "tudo
  certo". A rodada com todos os roteiros fica para o fim da F4.
- A `.text` da x86 (referência atual `3b51cce642529085aeaba71fde7b3dbdcbf76e72`): só muda por código de teste, provado pelos objetos.
- Ao fim, um parágrafo "Tarefa 9c-1" em `docs/x360/plano/F4/09c.md` com a tabela da rodada e as perguntas.

## Fora desta tarefa

O Disc Fusion no 360 (resposta 1 da 9b), o Data Center, o Replay, o Torneio, a Referência e o Shenlong.
