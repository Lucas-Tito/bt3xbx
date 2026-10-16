# Tarefa F5-inventário: as cores dos efeitos e os ids da luta de teste

[Índice do plano](../plano.md) | fase: [F5/00-escopo.md](../plano/F5/00-escopo.md) | decisão 24

Branch: `codex/f5-inventario` (a partir de `troca/main`). Relatório: `~/entrega-f5-inventario.md`. Orçamento: 1 h.

## Limite desta tarefa (importante)

**Só leitura e escrita de documento**, como no estudo da F5: o WSL roda a rodada completa da F4 no emulador. Nada de
`headless.py`, `roteiros.sh`, `save_mc.sh`, `save_entre.sh`, `qemu-ppc`, `gdb`, `make` nem build ou execução do jogo.
Pode ler código, documentos e dados, e rodar scripts de leitura de poucos segundos. Nenhuma mudança de código.

## O que fazer

1. **Inventário das cores dos efeitos (preparo da etapa 4 da F5).** Todo lugar do código da luta e do desenho que
   grava cores `u8 rgba[4]` (ou equivalentes) em pacotes que vão para a GPU, partindo de `docs/x360/formatos/gpu.md`
   seção 6 e das 26 primitivas conhecidas (`docs/x360/plano/F4/06.md`). Para cada gravação: arquivo e linha, a
   struct, se é cor da CPU, cor de pacote ou cópia de pacote (a distinção do plano da F5, seção 2, rede 1), e a
   correção proposta no padrão do projeto (`#ifdef PORT_BIG_ENDIAN` com o original no `#else`). Diga também como
   o inventário foi buscado, para que um verificador possa repetir a busca e recusar uma gravação esquecida.
2. **Os ids da luta de teste (preparo da etapa 3).** Goku (Early) contra Raditz (decisão 24). Ache nos dados e no
   código os ids de personagem e os trajes de cada um, e prove que nenhum dos dois tem transformação nem fusão
   disponível na luta (`src/battle/btl_act_j.c` e as tabelas que ele lê). Se o Raditz tiver, faça o mesmo com o
   Nappa. Não deduza pelos nomes nem pela posição no menu.

## Entregável

`docs/x360/plano/F5/inventario.md` com as duas partes, e uma linha apontando para ele no fim de
`docs/x360/plano/F5/00-escopo.md`. Português, sem travessão. Ao final: relatório em `~/entrega-f5-inventario.md`,
`git push troca codex/f5-inventario`, git limpo, nenhum processo seu.
