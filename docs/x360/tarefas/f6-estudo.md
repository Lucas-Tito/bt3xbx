# Tarefa F6-estudo: o plano da F6 (áudio e filmes)

[Índice do plano](../plano.md)

Branch: `cursor/f6-estudo` (a partir de `troca/main`). Relatório: `~/entrega-f6-estudo.md`. Orçamento: 1,5 a 2,5 h.

## Limite desta tarefa (importante)

**Só leitura e escrita de documento.** O WSL está rodando conferências no emulador, e dois emuladores juntos travam a
máquina. Nada de `headless.py`, `roteiros.sh`, `save_mc.sh`, `save_entre.sh`, `qemu-ppc`, `gdb`, `make` nem build ou
execução do jogo. Pode ler código, documentos e dados e rodar scripts de leitura de poucos segundos. Nenhuma mudança
de código: o entregável é texto.

## O que fazer

A F6 é "áudio (ADX e efeitos) e filmes" no console (tabela de fases em `docs/x360/plano.md`). Hoje ela não tem plano.
Escreva-o no formato do plano da F5 (`docs/x360/plano/F5/00-escopo.md` como modelo: escopo e ponto de partida,
redes de cobertura, critério de fim, etapas com onde roda, o que faz, critério e tamanho, decisões para o Lucas,
riscos com a medição que os revela cedo, divisão entre agentes e o que pede o console).

Leia antes:

- o `CLAUDE.md` da raiz, "Lições do 360": o XAudio2 mora nas threads de hardware 4 e 5 (nada de I/O lá), os dados
  XMA em `XPhysicalAlloc` com alinhamento 0, `StopEngine` na queda;
- `docs/x360/decisoes.md` (procure áudio, som, ADX, XMA, filme, a decisão 18 sobre os fluxos que passam direto da
  ISO, e a 20 sobre os filmes contados e não desenhados) e `docs/x360/plano/base.md`;
- `docs/x360/formatos/som-e-sprites.md` e o que o port de PC já faz com o som e os filmes (procure em `port/src/` e
  no código do jogo em `src/` o caminho do ADX, dos efeitos, da voz e do player de filme, e no 360 o que já existe em
  `port/x360/src/`);
- as dependências que a F5 deixou para a F6: a boca de quem fala sai fora de fase sem a voz
  (`docs/x360/plano/F4/07.md`), e a hipótese de que a tela de resultado da História espera a fala do vencedor em
  tempo real (`docs/x360/plano/F4/10.md`, achado 3);
- `docs/x360/guia-de-port.md`: o método (roteiros, comparação x86 contra PPC, imagens, memória) que a F6 deve
  reaproveitar onde couber (o som não se compara por imagem: proponha como provar que o áudio e os filmes estão certos
  no WSL e no console).

## Entregável

`docs/x360/plano/F6/00-escopo.md`, com o escopo e o critério de fim verificáveis, as etapas na ordem com estimativa,
as **decisões para o Lucas** numeradas (cada uma com as opções, o custo de cada e a sua recomendação; ele decide a
arquitetura), os riscos e o que pode ser dividido entre agentes. Atualize a linha da F6 no índice
`docs/x360/plano.md` para apontar para o arquivo novo. Português, sem travessão.

Ao final: relatório em `~/entrega-f6-estudo.md`, `git push troca cursor/f6-estudo`, git limpo, nenhum processo seu.
