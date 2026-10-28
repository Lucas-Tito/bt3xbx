# Port para Xbox 360

O port usa a ISO USA SLUS-21678 do próprio usuário, convertida no PC. Nenhum dado
do jogo acompanha o repositório. Contrato e comandos em
[ISO convertida](../../docs/x360/formatos/iso-convertida.md).

A ferramenta é `tools/convert_iso.py`. Copie a saída para a raiz da pasta do jogo
com o nome `bt3-x360.iso`, ao lado de `default.xex`, `default.dat` e `default.swap`.
A versão 2 da conversão inclui os ícones dos controles do 360. Reconverta a
original se a ISO foi feita com a versão 1.

## Créditos dos ícones

Ícones de Kenney, **www.kenney.nl**, pacote Input Prompts 1.5A, domínio público
CC0. Os doze PNG utilizados, a licença original e a origem estão em
[arte/kenney/CREDITOS.md](arte/kenney/CREDITOS.md). O script
`tools/icones360.py` reduz essa arte para as paletas e os tamanhos do jogo.
O marcador amarelo e o estado apertado são acrescentados pelo script.
