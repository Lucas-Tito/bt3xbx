# Tarefa F5-etapa 7, especificação da segunda parte: as passadas nativas e o alfa

[Índice do plano](../plano.md) | etapa: [F5/07.md](../plano/F5/07.md)

Branch: `cursor/f5-etapa7-spec` (a partir de `troca/main`). Relatório: `~/entrega-f5-etapa7-spec.md`. Orçamento: 1 h.

## Limite

Só leitura e escrita de documento, mais scripts de leitura de poucos segundos. Nada de `headless.py`, `roteiros.sh`,
`qemu-ppc`, `gdb` nem build. Nenhuma mudança de código.

## O que fazer

A segunda parte da etapa 7 da F5 (veja `docs/x360/plano/F5/00-escopo.md`, etapa 7, "Profundidade e alfa são uma
entrega própria" e a pergunta 6; `docs/x360/plano/F5/06.md`, "Triagem da segunda rodada"; e
`docs/x360/plano/F5/07.md`, a primeira parte) vai fazer no 360 o que o backend do PC já faz e o 360 hoje pula:
os efeitos nativos 1 a 4 (contorno, chave, tabela de profundidade, cópia de ids: o log do console mostra
`nativos 60/0/360/60`), o teste de alfa do destino (DATE), o FBA, a névoa e o brilho, e o byte exato de alfa do alvo
(hoje saturado). Quem vai implementar é outro agente, no código do 360 (que você não compila); o seu trabalho é a
especificação que poupa a ele a leitura.

Para cada efeito: o que ele faz na imagem; quando o jogo o usa (quem emite, em que telas e com que frequência por
quadro, pelo log do console em `/srv/bt3/dados/capturas/etapa6_r2.log` e pelo código); como o backend do PC o
implementa (arquivos, funções, shaders, o que lê e escreve, que anexo do alvo usa); o que o backend do 360 tem hoje
(`port/x360/src/gs/gs_d3d.cpp`, `targetAux`, `targetDepth`, os nativos, `sai_da_edram`/`entra_na_edram`, e a
profundidade da primeira parte, que está no branch `f5-profundidade` do GitHub se não estiver na `troca/main`); e o
caminho proposto no 360 com o custo provável (EDRAM, RAM, passadas extras), no limite da decisão 24 (resposta 6:
persistir por alvo com passadas extras; reorganizar a EDRAM só se não couber). Liste também o que o byte exato de
alfa exige (ids acima de 0x80, a leitura de volta) e a ordem de implementação que você recomenda, do que mais muda a
imagem para o que menos muda.

## Entregável

`docs/x360/formatos/gs-nativos.md`, em português, sem travessão. Ao final: relatório em
`~/entrega-f5-etapa7-spec.md`, `git push troca cursor/f5-etapa7-spec`, git limpo, nenhum processo seu.
