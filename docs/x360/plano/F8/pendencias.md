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

## Fechamento de gaps, antes do release (27/10/2026)

Defeitos conhecidos que o Lucas mandou deixar para o fim do plano, para não travar as fases:

4. **O preto intermitente de Namek na animação do Shenlong** (vinha da F5, etapa 9): no começo da animação, com as
   esferas subindo, um pedaço do céu e da água fica preto e volta. O pulso do brilho do Shenlong, que era outro
   defeito, foi corrigido (o cache das tabelas, `f5-clut`). Já testado no console sem resolver: o cache da GPU
   (`BT3_INVALIDA`), os alvos limpos, o alfa (`BT3_ALFA255`, `BT3_ALFA_INT`), o alvo sobreposto (`BT3_PULA_FBP`), o
   brilho em 100% (`BT3_GLOW`) e todos os efeitos de tela fora. O céu são 32 triângulos da CPU com Gouraud e CLUT
   estável; a água, grupos da VU1 com DATE e FBA. Estado, hipóteses e a rodada de 4 aberturas preparada:
   [F5/09](../F5/09.md) e os branches `f5-shenlong` e `codex/f5-namek`.
5. **O estágio do deserto à noite escuro demais** (visto pelo Lucas jogando, 23/10): talvez o mesmo brilho somado
   mais fraco no 360 (a H9 de [F5/09](../F5/09.md)); sem captura ainda.
6. **Arrumação para a finalização** (27/10/2026): o 360 achar a ISO convertida quando há a original e a convertida na
   mesma pasta (hoje a busca pega a primeira `.iso`); a numeração repetida das seções das notas da F5; as pastas
   temporárias do WSL; propor ao upstream as correções do cache que valem também para o PC (a colisão das tabelas do
   `f5-clut` e as 16 cores do `codex/f5-clut16`).

## A abertura do repositório, a última parte do plano (27/10/2026)

7. **Antes de abrir o repositório** (pedido do Lucas, só anotado, sem plano ainda): os documentos em inglês, com o
   guia de instalação e o de contribuição; e o GitHub configurado para receber as contribuições.

