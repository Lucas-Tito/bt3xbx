# Tarefa F5-estudo: o plano da F5 (uma luta a 30 fps)

[Índice do plano](../plano.md)

Branch: `codex/f5-estudo` (a partir de `troca/main`). Relatório: `~/entrega-f5-estudo.md`. Orçamento: 1,5 a 2,5 h.

## Limite desta tarefa (importante)

**Só leitura e escrita de documento.** Enquanto você trabalha, o WSL roda a rodada completa da F4 no emulador, e dois
emuladores juntos travam a máquina. Então, nesta tarefa:

- nada de `headless.py`, `roteiros.sh`, `save_mc.sh`, `save_entre.sh`, `qemu-ppc`, `gdb` sobre o jogo, `make` ou
  qualquer build ou execução do jogo;
- pode ler o código (`src/`, `include/`, `port/`, `asm/`), os documentos e os dados em modo leitura, e rodar
  scripts pequenos de leitura (contar, procurar, medir tamanho de arquivo), sempre de poucos segundos;
- nenhuma mudança de código: o entregável é texto.

## O que fazer

A F5 é "uma luta a 30 fps" no console (tabela de fases em `docs/x360/plano.md`; decisão 15: 720p a 30). Hoje ela só
tem riscos anotados. Escreva o plano dela, no formato do da F4 (`docs/x360/plano/F4/00-escopo.md` como modelo: escopo,
redes de cobertura, critério e a tabela de etapas, cada uma com onde roda, o que faz, o critério e o estado).

Leia antes:

- `docs/x360/plano/F3.md`, "Riscos anotados para a F5": a luta esgota a memória com o D3D9 (texturas no teto de
  128 MB, alvos em 24, paradas de 8 a 80 s, a Guide sem abrir), a ISO convertida no PC (decisão 23) e o custo do
  `gs_draw.c`;
- as menções à F5 nas notas da F4 (`grep -n F5 docs/x360/plano/F4/*.md`): as cores `u8 rgba[4]` dos efeitos
  invertidas (`docs/x360/formatos/gpu.md` seção 6, uns 150 pontos), a memória no meio da luta (`gBtlScript`, a arena do
  `BtlPool`, resposta 4 da 9b), a cena 3D do Shenlong e o modelo do ChrView, e a boca de quem fala fora de quadro
  (07.md);
- `docs/x360/decisoes.md` e `docs/x360/plano/base.md`: arquitetura, as medições da F2 (tempo de quadro no Xenon) e a
  VU1;
- `docs/x360/guia-de-port.md`: o método (roteiros, comparação x86 contra PPC, memória por tela, imagens), que a F5 deve
  reaproveitar para a luta;
- o código da luta e do desenho (`src/battle/`, `port/src/gs/`, `port/x360/`) só onde precisar para estimar.

## Entregável

`docs/x360/plano/F5/00-escopo.md`, com:

1. **Escopo e critério de fim da F5**, verificáveis (o que "uma luta a 30 fps" quer dizer em números e em que roteiro
   ou teste no console).
2. **Etapas**, na ordem, cada uma com onde roda (WSL ou console), o que faz, o critério e uma estimativa de tamanho.
   Comece pelo teto de memória do backend, que a F3 já marcou como primeiro item.
3. **Decisões para o Lucas**, numeradas, cada uma com as opções, o custo de cada uma e a sua recomendação. Ele decide
   a arquitetura; não decida por ele.
4. **Riscos e incertezas**, com o que mediria cada um cedo.
5. **O que pode ser dividido entre agentes** (como a 9c foi dividida) e o que pede o console.

Atualize a linha da F5 no índice `docs/x360/plano.md` para apontar para o arquivo novo. Português, sem travessão.
Ao final: relatório em `~/entrega-f5-estudo.md`, `git push troca codex/f5-estudo`, git limpo, nenhum processo seu.
