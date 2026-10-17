# Tarefa: investigar os ícones dos botões (PS2 para 360)

[Índice do plano](../plano.md)

Branch: `cursor/icones-botoes` (a partir de `troca/main`). Relatório: `~/entrega-icones-botoes.md`. Orçamento: 1 a 2 h.

## Pedido do Lucas

O jogo mostra os botões do PS2 (triângulo, círculo, xis, quadrado, L1/R1/L2/R2, START, SELECT, os direcionais e os
analógicos) nos menus, nas telas de ajuda, na lista de golpes, nos tutoriais e no HUD. O Lucas quer saber se dá para
trocar pelos do 360 (A, B, X, Y, LB/RB, LT/RT, Start, Back) e quanto custa. **É só investigação**: nada de implementar.

## Limite

Só leitura e escrita de documento, mais scripts de leitura de poucos segundos. Nada de `headless.py`, `roteiros.sh`,
`qemu-ppc`, `gdb` nem build. Nenhum byte do jogo no repositório: o documento descreve onde as imagens estão (arquivo,
seção, deslocamento, tamanho, formato), sem copiar pixels; imagens extraídas para olhar ficam fora do git (por
exemplo em `~/icones/`).

## O que responder

1. **Onde os ícones estão**, cada fonte com a prova: texturas nos pacotes de menu (`docs/x360/formatos/menus*.md`
   descreve os pacotes), glifos especiais das fontes (o `Font_ReadChar` e as fontes de `00005.bin`; códigos de
   caractere reservados para botões dentro dos textos), sprites dos filmes Flash do HUD e dos menus, texturas da luta
   (o HUD, a lista de golpes) e o que mais aparecer. Use os dados em `/srv/bt3/dados/gamedata` e o código em `src/`.
2. **Quantos lugares** e quantas imagens distintas, por tipo, e se o mesmo ícone se repete em vários pacotes.
3. **O mapeamento** PS2 para 360 que o jogo implica: lembre que no PS2 americano o xis confirma e o círculo volta (o
   A do 360 confirma e o B volta); os gatilhos L2/R2 viram LT/RT; SELECT vira Back. Liste os casos ambíguos (por
   exemplo, ícones de analógico pressionado, L3/R3).
4. **As opções de troca**, com o custo de cada: (a) a ferramenta do PC que gera a ISO convertida
   (`port/x360/tools/iso_convertida.py`, decisão 23) aplica a troca, com ícones desenhados pelo projeto (nunca a arte
   da Microsoft copiada), (b) troca em tempo de carga no 360, (c) outra que você achar. Diga também o que muda nos
   conversores e na marca da ISO, e o que isso faz com os roteiros de teste (as imagens de referência mudam).
5. **Os riscos**: textos que citam os botões por nome em vez de ícone, ícones dentro de imagens maiores (telas de
   tutorial desenhadas inteiras), tamanhos e paletas diferentes entre pacotes.

## Entregável

`docs/x360/pesquisa/04-icones-botoes.md`, em português, sem travessão. Ao final: relatório em
`~/entrega-icones-botoes.md`, `git push troca cursor/icones-botoes`, git limpo, nenhum processo seu.
