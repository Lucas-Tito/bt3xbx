# F8: itens anotados para o acabamento

[Índice do plano](../../plano.md)

Itens que o Lucas pediu para o acabamento ao longo das outras fases. Cada um vira etapa quando a F8 for escopada.

## A ISO (22/10/2026)

Hoje (decisão 23, `port/x360/src/iso360.c`): o `iso_convertida.py` converte no PC e confere o tamanho, o MD5 e o SHA256
da SLUS-21678; no console, uma ISO sem a marca da convertida é recusada com uma caixa do sistema, e o jogo não começa.
A mensagem é a mesma para qualquer ISO sem a marca.

1. **Mensagens separadas por caso** (pedido): a ISO USA original sem converter ("converta no PC com o script"), uma
   versão não suportada (outra região, outra revisão: "só a USA SLUS-21678") e a imagem ilegível ou truncada. O console
   distingue pela identificação do disco (o `SYSTEM.CNF` e o volume) antes de olhar a marca.
2. **Conversão no próprio console** (pergunta para quando a F8 for escopada; seria rever a decisão 23): o código de
   conversão já rodou no console antes da decisão 23; o custo é ler e gravar 3 GB no FATX, com tela de progresso, e os
   conteúdos comprimidos (BPE) continuariam convertidos na carga.

## Os textos para o usuário (22/10/2026)

3. **Todo aviso que aparece para o usuário em inglês** (pedido): as caixas do sistema (a ISO recusada, a falta de
   memória, a queda), as telas de falha e qualquer texto do port na tela. O que vai só para o log (`bt3xbx.log`,
   `saida.txt`) pode seguir em português. Fazer o levantamento dos textos visíveis (as chamadas de `XShowMessageBoxUI`,
   as telas de cor por fase e os textos desenhados pelo port) antes de traduzir.
