# Conversão do save do BT3 entre PS2 e 360

Código: `port/x360/conv/save_conv.c/.h` (usa o contêiner `psu.c` e o mapa `save_larguras.c`). Ferramenta de PC para os
testes e roteiros do WSL: `port/x360/tools/save_conv_tool.c`, chamada por `save_conv.py`. Teste:
`port/x360/tools/teste_save_conv.py`.

## Fluxo

O save é a pasta `BASLUS-21678DBZT3` com `icon.sys` (964), `dbzsm.ico` (40.090, igual nas duas ordens) e
`BASLUS-21678DBZT3` (16.384, os dados). O PS2 grava os números em little-endian, o 360 em big-endian.

**PS2 para 360** (`SaveConv_Ps2Para360`): `Psu_Ler` e `Psu_ConferirBT3`; confere a soma dos dados na ordem do PS2
(erro `PS2 save is damaged (checksum)`); copia; `SaveLarg_Inverte` nos dados e no `icon.sys`; copia o `.ico`; recalcula a
soma e grava as duas palavras em big-endian.

**360 para PS2** (`SaveConv_360ParaPs2`): confere a soma na ordem do 360 (erro `save is damaged (checksum)`); inverte;
recalcula a soma em little-endian; preenche um `PsuSave` pronto para `Psu_Escrever`. O 360 não tem relógio confiável e o
save dele não guarda datas: a política de datas é de quem chama (a função usa as duas datas passadas em todas as entradas).

## A soma

`McCard_Checksum` (src/battle/stgm_a_b.c) soma os bytes depois dos 8 primeiros (de posição par, relativa a esse ponto, uma
vez; de posição ímpar, vezes 256) em 16 bits, e grava `buf[0] = soma >> 8` e `buf[1] = soma & 0xFF` como palavras u32 na
ordem do host. Como a soma é sobre os bytes, ela **muda** quando a ordem dos números muda: depois de inverter, é preciso
recalcular, não só trocar as palavras. A conferência daqui exige as DUAS palavras certas (o jogo aceita se uma só confere,
regra do PS2); um save gravado pelo jogo sempre tem as duas. A build big-endian do port recusa palavra acima de 0xFF (de
outra ordem), e o resultado sempre cumpre isso (`save_entre.py preve --build ppc`).

## Ferramenta e testes

`save_conv.py psu-para-360 ARQ.psu PASTA`, `save_conv.py 360-para-psu PASTA ARQ.psu [--data AAAA-MM-DDTHH:MM:SS]` e
`save_conv.py confere ARQ.psu|PASTA` (diz `ps2 ok`, `360 ok` ou `danificado`).

`teste_save_conv.py` usa saves sintéticos (bytes aleatórios, soma calculada em Python pela definição acima): ida e volta
byte a byte, soma válida e regra da build big-endian, cada número do mapa invertido, erros (soma ruim em cada lado, outra
pasta, arquivo faltando) e onze mutantes de `save_conv.c` que têm de sair com 1 (sem recalcular, palavras na ordem errada,
`icon.sys` sem trocar, sem conferir a entrada, entre outros).
