# Tarefa F5-etapa 5: a memória no meio da luta

[Índice do plano](../plano.md) | fase: [F5/00-escopo.md](../plano/F5/00-escopo.md)

Branch: `codex/f5-memoria` (a partir de `troca/main`). Relatório: `~/entrega-f5-memoria.md`. Orçamento: 4 a 6 h.

## O que fazer

A etapa 5 da tabela da seção 4 de `docs/x360/plano/F5/00-escopo.md`: a memória no meio da luta, comparada entre a
x86 e a PPC sem mapa campo a campo, com dono, tipo e vida útil. Leia antes:

- a seção 2 do escopo (rede 3, "Execução e memória no WSL") e a etapa 5 da tabela;
- `docs/x360/plano/F5/03.md` (a etapa 3: os roteiros `f5_duelo` e `f5_treino` e os pontos de memória e de captura que
  ela definiu) e `docs/x360/plano/F5/04.md` (a etapa 4: as cores dos efeitos, já sem nenhuma tolerância de cor);
- `docs/x360/plano/F4/05.md` (a arena do `BtlPool`, de 0x1A0800 bytes, aceita hoje por teto de classe) e a resposta 4
  da 9b em `docs/x360/plano/F4/09b.md` (os cinco trechos de `gBtlScript` sem prova no meio da luta);
- a seção do guia sobre a comparação de memória (`roteiros.py memoria`, `memdiff.py`, `campos.py`).

O trabalho: tipar as subalocações vivas do `BtlPool` (slot, geração e dono), interpretar a união de
`BtlScriptRequest` pelo `kind` ativo, e cobrir os componentes de câmera e as tarefas GSC vivas, nos pontos da etapa 3.
Onde houver tipo, o teto genérico da classe sai. Nenhuma exceção nova para encobrir diferença: uma diferença real é
erro de port a corrigir (no upstream só dentro de `#ifdef PORT_BIG_ENDIAN`, com o original no `#else`), e um campo sem
conteúdo vivo só se exclui com o contexto provado.

Também desta tarefa (resposta 2 do Lucas na etapa 3): as três posições que o `conv_tool --mapa` acusa nos roteiros
`f5_*` (`docs/x360/plano/F5/03.md`): duas no `00005.bin` (0x7AC42 e 0x8CF02, o `Font_ReadChar` lendo 2 bytes no meio
de um u32 que o conversor troca inteiro) e uma no `01432.bin` (0x18954, sem leitor anotado). Ache a largura certa de
cada uma pelo leitor e corrija o conversor (ou prove que a leitura é de largura conhecida, com o leitor), até os
roteiros da F5 darem "tudo certo"; cada mudança de conversor vista falhando e o `conv_tool --disco` a 100%.

A memória nos pontos da luta está atrás do `MEMLUTA=1` (resposta 4 da etapa 3): quando ela passar, ela vira o padrão
dos roteiros `f5_*` e a opção sai.

## Critério

- Os pontos de memória da etapa 3 no meio da luta comparados campo a campo, sem teto genérico na arena onde há tipo.
- Cada regra nova vista falhando: um número vivo estragado e o contexto (dono, `kind`, slot) estragado à parte, em
  cada variante e sub-bloco vivo.
- `REF=1 SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh f5_duelo f5_treino` (os pontos de memória da etapa 3 incluídos)
  tudo certo, e, como controle, `SEMMAPA=1 IMG=1 port/x360/tools/roteiros.sh duelo missao`. Um emulador por vez.
- A `.text` da x86 (a referência da nota da etapa 3) só muda por código de teste, provado pelos objetos.
- Ao fim, `docs/x360/plano/F5/05.md` com a tabela da rodada e as perguntas, e a linha da etapa 5 do escopo apontando
  para ele.

## Fora desta tarefa

As etapas 1, 6, 7 e 8 (dependem do console) e o lado do 360 da etapa 2.
