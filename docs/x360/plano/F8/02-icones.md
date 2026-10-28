# F8: ícones do Kenney na ISO e caixas para o usuário

[Decisão 30](../../decisoes.md) | [Pendências 1, 3 e 9](pendencias.md)

A instalação passa a usar `convert_iso.py`, saída pública `bt3-x360.iso`, e marca
versão 2. `Conv_Icones360`, em C89 comum, recebe um pacote de arte e troca os pixels
e as paletas de 10 a 27, depois da conversão numérica do boot. Mantém o tamanho
da entrada 8, seus deslocamentos e cabeçalhos, os ícones 0 a 9 e todo o texto.
SELECT não muda. A versão 1 é recusada em caixa própria, pedindo reconversão.

Os dez PNG sem alteração, a licença original e os créditos de Kenney, www.kenney.nl,
estão em `port/x360/arte/kenney/`. Há crédito também no README do port.
`icones360.py --arte DIR` reduz o conteúdo visível mantendo a proporção, quantiza
para 16 cores por textura, converte o alfa para 0 a 128 e produz PSMT4. Desenha o ▼
amarelo e escurece o segundo quadro. A metade inferior das texturas 64x64 fica vazia.
O cabeçalho C é reproduzível e não contém nenhum byte extraído do jogo.

## Medidas da folha USA

Coordenadas exclusivas no fim, por célula 32x32, medidas pelo alfa original:

| Texturas | Caixa original solta | Quadro apertado |
|---|---|---|
| 10 a 13, 20 a 23 | `(0,4)-(20,29)` | 20 a 23: `(0,9)-(20,29)` |
| 14, 15, 17, 18 | `(0,6)-(29,27)` | sem par |
| 16, 19 | `(0,3)-(31,31)` | sem par |
| 24, 25 | `(0,3)-(20,29)` | sem par, ▼ incluído |
| 26, 27 | `(0,0)-(31,31)` | `(0,3)-(31,31)` |

A arte nova cabe nessas caixas. O mapeamento continua físico: círculo B, quadrado X,
triângulo Y, xis A, L1/L2/L3 LB/LT/clique esquerdo e R1/R2/R3 RB/RT/clique direito.
Voltar continua Y e remapeamentos da luta mantêm a limitação já existente no PS2.
A folha completa fica **fora do git**, em `~/icones/contato_kenney.png`.

## Caixas e busca

Textos ingleses estáticos em `iso_avisos.h`, usados por `MostrarRecusa`, sem caminho
variável, `BT3_ISO` ou `ambiente.txt`. Dizem `convert_iso.py`, raiz do jogo e
`bt3-x360.iso`. A quinta caixa separa conversão antiga de região/revisão incompatível.
A caixa de imagem ilegível orienta nova cópia ou nova conversão da original.

Sem configuração, prefere o nome público, depois percorre outras `.iso` como antes.
Sem imagem, agora mostra a caixa de ausente. `BT3_ISO` explícito permanece definitivo.
Os nomes antigos registrados nas provas e nos ambientes de outras fases são nomes
históricos ou caminhos explícitos de desenvolvimento; precisam de conteúdo versão 2
para esta build. Não foram renomeadas as ISOs nem alteradas as pastas dessas rodadas.

## Provas locais e critério da entrega

`teste_icones360.py` verifica as 28 voltas PSMT4, as 18 trocas, as caixas, alfa, dois
quadros, marcador e igualdade de todos os outros bytes contra o `--saida` sem arte.
Prova a recusa sem escrita parcial de índice, tamanho, alfa e deslocamento ruins.
O verificador rejeita pixel e arte adulterados. `icones360.py --conferir` também deve
rejeitar um PNG com CRC estragado e um cabeçalho alterado, em cópias.

`teste_iso_convertida.py` inclui essas provas, reabertura integral, arte adulterada
na própria ISO restaurada depois, marca versão 1 e os estragos anteriores. Mantém
somente uma convertida desta tarefa. A original é aberta apenas para leitura.
`teste_iso_avisos.py` testa versão, nome preferido, ausência, identificação, textos
públicos e seus controles negativos, em 64 e 32 bits. `teste_som360.sh` é a regressão
do leitor compartilhado. `teste_iso.sh` usa a mesma API de teste atual e a referência
com ícones para conferir todas as entradas pela carga real do console no WSL.

A tabela efetiva, hashes e commit ficam em `~/entrega-icones-iso.md`, com logs em
`~/icones/final/`. A rodada final recompila as ferramentas do commit entregue.
A `.text` x86 deve manter SHA1 `43444ea4d03a6d448aa1ae74a631e35b356a7caa` e os
objetos, retirando depuração, devem ser iguais à base, exceto o carimbo de teste.

Os roteiros não mudam: além do diff sem alterações em roteiros/headless/upstream,
o teste usa o `--saida` **sem** a opção nova e prova os pixels PS2 preservados.
A PPC continua recebendo esse mesmo caminho em `roteiros.sh`; a troca só entra
quando o instalador pede `--icones360`. Não se rodou emulador, conforme a tarefa.
Não se rodou `save_mc.sh`, que também usa qemu. Não há XDK nem sessão de console.

## Revisão no console

Seis ambientes e instruções estão em
[icones-iso/LEIA-ME.md](../../../../port/x360/ambientes/icones-iso/LEIA-ME.md), com
saves vazios e destinos de captura novos e separados em `~/icones/console/`.
O Lucas compila e sobe a build e a ISO nova. A antiga permanece só para a recusa.
Ficam pendentes a aparência das cinco caixas, os quatro usos visuais e a inspeção
da folha pelo Lucas. Não há prova de ausência de outros ícones em CT32/CT16 ou
Flash, nem mudança de fonte, texto, entrada ou remapeamento fora deste escopo.
