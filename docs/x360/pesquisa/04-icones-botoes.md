# 04: ícones dos botões do PS2 e a troca pelos do 360

Investigação (só leitura) pedida em `docs/x360/tarefas/icones-botoes.md`. Fontes: `src/battle/col_c_b.c`,
`src/battle/hud_e_b.c`, `src/menu`, `port/x360/src/f2/pad360.c` e os dados da ISO USA (SLUS-21678) em
`gamedata/` (arquivos `pzs3us0/1/2`, SLUS e DBZP). **Nenhum byte do jogo está neste documento**: só arquivo,
seção, deslocamento, tamanho, formato e contagens. As imagens extraídas para olhar ficaram fora do git.

> **Resumo.** Os botões não são caracteres reservados nem imagens espalhadas: são **28 texturas de uma única
> folha** (`pzs3us1/00004.bin`, entrada 8), chamadas por **marcas de texto `<PAD=nome>`** e pelo prompt do HUD.
> Trocar essa folha troca todos os ícones do jogo de uma vez, sem mudar código nem posições, desde que a
> geometria de cada célula se mantenha. A troca em tempo de carga no 360 (opção b) custa uns 1,5 a 2 dias e não
> mexe na ISO convertida; na ISO (opção a) custa mais (conversor, marca e critério da `iso_convertida`).
> **Achado que contradiz o pedido:** nos menus deste jogo o X do PS2 confirma, mas quem **volta é o triângulo**
> (45 usos no código dos menus), não o círculo (1 uso). Com o mapeamento físico já feito em `pad360.c`, "voltar"
> é o botão **Y** do 360, não o B.

---

## 1. Onde os ícones estão (com a prova)

### 1.1 A folha de ícones (a única fonte de imagem)

| Item | Valor |
|---|---|
| Arquivo | `pzs3us1/00004.bin`, o arquivo de boot (`COMMON_FILE_BOOT`, id 5), 140.416 bytes, 11 entradas, sem BPE |
| Entrada | 8: deslocamento `0x19A40`, fim `0x22080`, tamanho `0x8640` (34.368 bytes) |
| Formato | `TexFile` com 28 `TexEntry` (a tabela de entradas em `+0x20`, 0x40 bytes cada), o mesmo contêiner das texturas de menu |
| Pixel | PSMT4 (psm `0x14`) com CLUT de 16 cores em CT32; pacote de pixel de 640 bytes (32x32) ou 2.176 bytes (64x64); pacote de CLUT de 192 bytes |
| Envio | `pixBlt` 0 (DBW tratado como 1), `tbw` 0, `TRXREG` 16x8 palavras (32x32) ou 32x16 (64x64) |
| Alfa da CLUT | escala do PS2 (`0x80` é opaco) |
| Tamanhos | texturas 0 a 19, 24 e 25: 32x32; 20 a 23, 26 e 27: 64x64 (quatro células de 32, duas imagens: solto e apertado) |

Como o código chega lá: `FontIcon_Init` (`col_c_b.c`, chamado de `main.c`) aponta `gFontIconRes` para
`boot + boot[8]/4` e relocaliza as posições com `Res_RelocateOffsets`. `FontIcon_GetRes()` devolve esse ponteiro
para o HUD, então **texto e HUD leem o mesmo buffer**.

Prova de que é a única cópia: a busca exata dos 28 fluxos de pixel distintos em todos os arquivos, descendo em
pacotes e BPE, deu 0 acertos fora da folha. Prova de que o formato está entendido: decodifiquei as 28 texturas
com as tabelas do GS copiadas de `port/src/gs/gs_core.c` (o T4 com a inversão `x^4` nas colunas ímpares) e a
folha de contato mostra, certos: `+ / ( )`, quatro setas amarelas, as formas de SELECT (retângulo arredondado) e
START (retângulo com ponta), ○ vermelho, □ rosa, △ verde, × violeta, L1 L2 L3 R1 R2 R3, os pares "apertado" e
as variantes ♂ (com ▼ amarelo). A volta (recodificar a imagem de índices) reproduz **o fluxo original byte a
byte nas 28 texturas**, e a mesma volta **falha** quando um pixel é mudado (o verificador visto falhando). Isso
prova o caminho de escrita da troca.

### 1.2 As marcas de texto `<PAD=...>`

Não há código de caractere reservado para botão. O parser de texto (`Font_SetTagCallback` com `FontTag_Handle`,
`FontTag_Pad`, `FontTag_PadSequence`, `FontIcon_Find`, `FontIcon_PutSprite`) trata, dentro do texto em UTF-16:

- `<PAD=nome>`: um ícone, pelo nome de 1 a 4 caracteres largos (ganha o casamento mais longo);
- `<PADS=a,b,c,d>`: uma sequência (o código existe, **nenhum dado a usa**);
- `<COL=RRGGBBAA>`, `<COL=DEF>` e `<UB0>` (desenha o caractere `0xFF10`), que não são ícone.

As tabelas ficam no `.data` do SLUS, que o esqueleto do repositório não traz (`port/data/cod_1C3400.data.s` é
removido): os valores vêm de `gamedata/disc/SLUS_216.78` (deslocamento no arquivo = endereço - `0x2C3400` +
`0x1C4400`):

- `gFontIconTables` @`0x2C6B18`: 4 ponteiros, um por tipo de controle. O tipo 0 (@`0x2C6988`) tem 28 entradas de 12
  bytes (nome, definição, escala 1,0). **Os tipos 1 a 3 (@`0x2C6AE8/AF8/B08`) estão vazios.**
- Nomes @`0x2C6490`: 78 de 12 bytes (4 caracteres u16 e o comprimento); os de 29 a 77 não são referenciados
  (nomes do tipo Wii como `†本`, `†Ａ`, `†Ｂ`, `振縦`).
- Definições @`0x2C6838`: 28 de 12 bytes: textura, tamanho, avanço, quadros, tempo de cada quadro.

Tabela do tipo 0 (índice = textura = definição; "avanço" é a largura que o texto reserva):

| Índice | Nome na marca | Imagem | Avanço |
|---|---|---|---|
| 0 a 3 | `＋` `／` `（` `）` | sinais de texto | |
| 4 a 7 | `↑` `↓` `→` `←` | setas amarelas | 16 |
| 8 | nome de 2 caracteres largos (SELECT) | retângulo arredondado | 24 |
| 9 | nome de 2 caracteres largos (START) | retângulo com ponta | 24 |
| 10 a 13 | `○` `□` `△` `×` | botões de ação | 20 |
| 14, 15, 17, 18 | `Ｌ１` `Ｌ２` `Ｒ１` `Ｒ２` | gatilhos e ombros | 29 |
| 16, 19 | `Ｌ３` `Ｒ３` | clique do analógico | 31 |
| 20 a 23 | `○*` `□*` `△*` `×*` | par solto e apertado (64x64, 2 quadros, tempo 3 e 3) | |
| 24, 25 | `□♂` `△♂` | 32x32 com ▼ amarelo (marca de segurar/carregar) | |
| 26, 27 | `Ｌ３♂` `Ｒ３♂` | 64x64, 2 quadros, tempo 15 e 60 | |

(O texto exato dos nomes em largura cheia está na tabela do SLUS, não é copiado aqui.)

### 1.3 O prompt do HUD

`HudPrompt` (`src/battle/hud_e_b.c`, `include/battle/hud_e.h`, chamado de `hud_a.c`: `HudPrompt_SetButton`,
`SetCommand`) lê a mesma folha por `iconRes = FontIcon_GetRes()` e `iconDefs = FontIcon_GetDefs()`. Tabelas no
SLUS: `gHudPromptIconDefs` @`0x2C6268` (16 definições), `gHudPromptIconDefs16` @`0x2C6328` (17) e
`gHudPromptCueDefs` @`0x2C63F8` (4). **Nesta folha americana só valem as texturas 20 a 23** (os pares
apertados): as outras definições apontam para texturas acima de 27 (54, 55, 60 a 63, 69, 70...) e parecem vir de
uma folha maior de outra versão; ficam mortas. Como as definições estão no SLUS, trocar a imagem no lugar vale
para o HUD sem mudar código.

### 1.4 Glifos nas fontes (a prova de que não é por aqui)

As entradas 1 e 2 do `00004.bin` são as fontes 0 (208 glifos) e 1 (425 glifos, 512x256 PSMT4). A fonte 1 tem as
formas geométricas simples (○ □ △ × ↑ ↓ ← → + / ( )) como **caracteres comuns**, e a 0 só tem `×` (`00D7`). Fora
das marcas, esses símbolos aparecem em 8 folhas de texto de BOM, todas nomes de golpes ("疾風×字拳" em
`02113/02114/02117/02118` seção 44 e um ○ em `02743/02744/02747/02748` seção 46): são texto do golpe, não botão.
Logo a marca é praticamente o único caminho.

### 1.5 Filmes Flash e telas desenhadas inteiras

Não achei ícone de botão nelas, mas **a prova é parcial**. A busca por assinatura de cor (as cores exatas de
vermelho, rosa, verde e violeta dos ícones nas paletas) em 17.177 `TexFile` e 43.703 texturas achou só a folha e
um falso positivo (`00505.bin.bpe#3`, textura 37, uma cena de dragão T8 128x128). Não cobre texturas CT32 e CT16,
nem as cores de formas Flash, nem telas inteiras desenhadas com outra paleta. Fica como **não provado**; o
título "PRESS START" é provavelmente imagem (não localizada), e como o 360 também diz START não precisa de troca.

## 2. Quantos lugares e quantas imagens

- **Imagens distintas: 28**, num só arquivo, sem repetição em outro pacote. Dessas, 22 aparecem em texto (as
  que não aparecem: SELECT, START, `○*`, `△*`, `×*` e `Ｌ３♂`; `○*`, `△*` e `×*` o HUD usa).
- **Lugares que chamam** (a busca cobriu todo o `gamedata`, recursiva em pacotes e BPE, deduplicada por sha1, em
  UTF-16 e também em 8 bits, que deu 0 acerto):

| Lugar | Arquivo | Folhas com marca | Marcas | Observação |
|---|---|---|---|---|
| Textos de HUD e pausa (inglês) | `pzs3us1/00005.bin` (id 6) | 33 | 68 | 4 conjuntos de 40 textos, um por tipo de controle (bases `0x26`, `0x4E`, `0x76`, `0x9E`, mais a configuração) |
| Lições do Treino | `pzs3us1/00461.bin`, seção 1 (BPE), seção 17 | 126 | 130 | o texto da lição |
| Lista de golpes | 698 arquivos de lutador em `pzs3us1` (1423 a 3032) | cerca de 984 marcas por arquivo, seções 44 a 52 | | linhas `!<máscara>#<PAD=...>`; a máscara (bits 1, 2, 4, 8) escolhe o tipo de controle 0 a 3 |
| HUD (prompt) | `src/battle/hud_e_b.c` pelas definições do SLUS | | | usa só 20 a 23 |

- **Total:** 254 marcas distintas (129 resolvem na tabela do tipo 0; 125 não resolvem, são os nomes do tipo Wii),
  625.822 ocorrências em todas as cópias e 144.000 em conteúdos únicos (os arquivos de lutador se repetem muito
  entre trajes). Só `<PAD=`; nenhum `<PADS=`. O `00006.bin` (idioma 1) não tem marca.
- Uso por índice nos conteúdos únicos (só as 129 marcas que resolvem no tipo 0, 80.014 das 144.000 ocorrências; o
  resto são marcas do tipo Wii): **mais usados** `□` 112.797, `＋` 61.817, `△` 47.609, `↑` 31.822, `／`
  29.053, `△♂` 19.215, `↓` 19.291, `○` 18.342, `×` 16.206, `（` e `）` 10.422 cada, `→` 9.502, `←` 9.415, `Ｌ２`
  6.626, `Ｒ１` 6.489, `Ｌ１` 6.487, `Ｒ３` 825, `□*` 911, `□♂` 3.895, `Ｒ３♂` 480; **uso quase nulo** `Ｌ３` (1) e `Ｒ２`
  (2); **nunca no texto:** 8, 9, 20, 22, 23, 26 (20 a 23 são do prompt do HUD).

## 3. O mapeamento PS2 para 360

Controle físico, já feito em `port/x360/src/f2/pad360.c` (não muda): A é o xis, B o círculo, X o quadrado, Y o
triângulo, LB/RB são L1/R1, LT/RT são L2/R2 (limiar 93), clique dos analógicos é L3/R3, START é START e BACK é
SELECT. O ícone novo deve mostrar o botão do 360 que **fisicamente** produz aquele bit do PS2.

| Ícone do PS2 | Ícone do 360 | Nota |
|---|---|---|
| ✕ (`×`) | A | confirma |
| ○ | B | |
| □ | X | |
| △ | Y | **é o "voltar" dos menus** |
| L1, R1 | LB, RB | |
| L2, R2 | LT, RT | gatilho analógico lido com limiar |
| L3, R3 | LS, RS (clique) | |
| START | START | rótulo igual; só a forma muda se quiser |
| SELECT | BACK | rótulo muda |
| `↑ ↓ → ←` | iguais | ver caso ambíguo 3 |
| `＋ ／ （ ）` | iguais | sinais de texto |
| pares apertados, ♂ | mesmo botão com o estado ou a marca | o ▼ amarelo permanece |

**Casos ambíguos e o que fiz:**

1. **"Voltar" é o triângulo, não o círculo.** `src/menu` usa o X (`CROSS`) 66 vezes para confirmar e o triângulo 45
   vezes para cancelar; o círculo só uma vez (`menu_z_d.c:1237`, teclado do DcPass); quadrado 3, START 4, SELECT 0,
   L1 e R1 2 cada, L2 e R2 1 cada. Então nos menus o 360 volta com **Y**. Opção conservadora: manter o físico e
   documentar. Se o Lucas quiser B voltando, é mudança de entrada (um alias de B para cancelar nos menus), fora
   deste estudo. Os textos com ícone são da luta, do Treino e da lista de golpes.
2. **Os ícones do texto assumem o controle padrão da luta.** As 8 ações passam por `gSaveData->keyConfig` (padrão
   `{2,1,0,3,4,5,6,7}` sobre `{CIRCLE,CROSS,SQUARE,TRIANGLE,L1,L2,R1,R2}`, `btl_input.c:492`). O texto traz o
   ícone do padrão; se o usuário remapear, o ícone mente, **no PS2 também**. Não é regressão da troca.
3. **Setas:** no texto "direcionais" são amarelas e neutras; o 360 tem direcional digital e analógico, e o jogo
   não diz qual. Manter neutras.
4. **L3/R3:** cliques de analógico, quase sem uso (L3 1, R3 825 vezes). No 360 o clique existe, mas é pior de
   alcançar; só trocar a arte.
5. **Gatilhos:** L2/R2 do PS2 são digitais no jogo; no 360 são analógicos com limiar. O ícone LT/RT é o certo.
6. **Pares apertados (20 a 23):** o quadro "apertado" precisa existir para A, B, X, Y.
7. **O tipo de controle 1 (nomes tipo Wii) e os tipos 2 e 3** nunca aparecem: `gFontIconTables[1..3]` estão
   vazios e o `padType` do console é sempre 0 (`Pad_GetLastStatus`), então só o tipo 0 é mostrado.

## 4. As opções de troca e o custo

Todas trocam o **mesmo conjunto**: 19 texturas (10 a 13 botões, 14 a 19 ombros, gatilhos e cliques, 20 a 23
pares, 24 a 27 variantes ♂ e cliques ♂, e SELECT, 8, que vira BACK); mantêm 0 a 7 (sinais e setas) e START (9),
9 que ficam, 28 no total. Cada textura é 4 bits com CLUT própria de 16 cores, então a arte
pode ter cores diferentes em cada ícone. **Arte feita pelo projeto** (círculos coloridos com A verde, B vermelho,
X azul e Y amarelo desenhados por um gerador no repositório; nunca a arte da Microsoft). A geometria precisa
caber na célula de 32 (ou 2x2 de 32) e no avanço da definição (20, 29, 31, 24), porque as definições ficam no SLUS
do usuário e não se mudam sem tocar nos dados.

Trabalho comum às opções (a) e (b): o gerador de arte (Python, sem dependências, já que Pillow e numpy não estão
no WSL) e a codificação PSMT4 com as tabelas do `gs_core.c`, cuja volta fiel já está provada (seção 1.1).
Estimativa: 0,5 a 1 dia para o desenho e o gerador.

### (a) `iso_convertida.py` aplica a troca

Como o `00004.bin` não é BPE, a entrada 8 pode ser trocada no lugar, com o mesmo tamanho e os mesmos
deslocamentos (a ISO convertida nunca recomprime BPE, decisão 23). O que muda:

- **Conversor/ferramenta:** a troca tem de entrar no `conv_tool --saida` (um passo para a entrada de boot) ou num
  passo novo do `iso_convertida.py`. Hoje o critério de reabertura exige que cada entrada convertida seja igual
  ao `--saida` e todo o resto igual à original: uma troca por fora desse caminho quebra o critério.
- **Marca:** a identidade dos fontes (SHA256 de `port/x360/conv/*.c|h` e `iso_convertida.py`) muda sozinha.
  Convém subir a versão do contrato (hoje 1) ou usar bytes reservados da marca para um campo "pacote de ícones"
  (hash do conjunto de arte), para que uma ISO convertida sem a troca seja recusada.
- **Testes:** o `teste_iso_convertida.py` precisa de novos casos (a entrada trocada, a arte estragada numa cópia,
  a marca da versão antiga) e de novos hashes esperados. Não afeta os roteiros (seção 6).
- **Textos:** o único texto que cita o botão por nome ("SELECT button", `00461.bin` seção 1, BPE, seção 17,
  texto 370) está dentro de BPE; para trocar na ISO seria preciso um compressor BPE que ainda não existe.
- **Custo:** mais 1 a 2 dias sobre o trabalho comum. Desvantagem: a escolha fica gravada na ISO, e o usuário que
  quiser os ícones do PS2 precisa reconverter.

### (b) Troca em tempo de carga no 360 (recomendada)

Depois de `Res_RelocateOffsets` em `FontIcon_Init`, copia-se por cima do buffer da folha, para cada textura, o
fluxo de pixel (512 ou 2.048 bytes) e a CLUT (64 bytes) da arte do projeto, compilada no `.xex`. É a mesma
posição, o mesmo tamanho e os bytes intactos (a ISO convertida deixa imagem e paleta como bytes), e por haver um
só buffer vale para texto, HUD e lista de golpes.

- **Código:** uma chamada `Port_IconesTrocar(gFontIconRes)` em `FontIcon_Init` dentro de `#ifdef PORT_X360` (só
  acrescenta, sem `#else`), mais uma função e uma tabela gerada em `port/x360/` (~28 KB de dados, arte do
  projeto, nenhum byte do jogo). Mantém o PS2 no x86 e no PPC sem janela.
- **Texto "SELECT button":** a lição está descomprimida na memória depois do carregamento; troca-se no lugar a
  palavra e fecha-se o vão (as cadeias terminam em zero), com um gancho na lição do Treino (uma hora).
- **Marca e ISO:** nada muda. A mesma ISO convertida serve; o usuário pode ganhar uma opção (ícones do PS2 ou do
  360) sem reconverter.
- **Custo:** o comum mais 0,5 a 1 dia (função, gerador de tabela, gancho do texto, conferência no console por
  captura em HUD, pausa, lição e lista de golpes). Total de 1,5 a 2 dias.

### (c) Outras

- **Usar a máquina de `padType`:** preencher `gFontIconTables[2]` com nomes novos que apontam para texturas
  acrescentadas (as variantes 2 e 3 do `00005.bin` já usam os mesmos nomes do PS2) e deixar o 360 devolver
  `padType` 2. Permite os dois conjuntos juntos, mas exige alocar uma folha maior, reconstruir a tabela do SLUS na
  memória e mudar o `status` do pad (que vale 0 em vários testes `status == 0` pelos menus). **Não recomendo.**
- **Trocar só em tempo de desenho (sobrepor sprite no `FontIcon_PutSprite`):** mais código e o mesmo resultado
  visual que (b); não vale.
- **Desenhar pelas fontes:** inviável, os glifos da fonte são geométricos simples e sem cor.

**Recomendação:** (b) com uma opção de menu para voltar aos ícones do PS2. A (a) só vale se o Lucas quiser o
resultado fixado na ISO.

## 5. Riscos

1. **Texto com nome do botão:** só "SELECT button" (lição do Treino) e as menções genéricas a "analog stick" (6
   ocorrências em `00461.bin`; `00005.bin` diz "left analog stick", "L Stick" e "Control Stick" nas variantes de
   tipo 2 e 3 que não aparecem). Procurei em inglês; **só inglês existe** nos textos de lições (a busca por
   `palanca`, `botón`, `Taste`, `bouton`, `manette`, `tasto` e `Knopf` em `00461` e `00453` deu 0).
   As folhas com palavras como R1 ou L2 em `00348`, `00370`, `00415`, `00448`, `00468` não têm BOM: são binário.
2. **Ícones dentro de imagens inteiras:** não achei (seção 1.5), mas a prova não cobre CT32/CT16 nem formas Flash.
3. **Tamanhos e paletas:** as 28 texturas têm CLUT própria, então não há conflito de paleta. O avanço das
   definições está no SLUS do usuário: a arte precisa caber no avanço antigo (20, 29, 31, 24).
4. **Tabela do SLUS ausente do repositório:** o `.data` com as tabelas vem do SLUS do usuário em tempo de
   execução; não há problema para a troca de imagem, só para mudar nomes ou larguras.
5. **A premissa do "círculo volta"** não vale (seção 3, caso 1): o ícone do "voltar" mostrado em dicas é △ e não ○.
6. **Remapeamento de botões na luta** (caso 2): o ícone pode divergir do botão real, como no PS2.
7. **Texturas de pares apertados com tempo próprio:** as definições mandam 2 quadros; a arte deve trazer os dois.
8. **Cache de texturas do GS:** o texto das lições e o HUD sobem as mesmas texturas; como a troca é feita antes
   do primeiro uso, não há textura velha em cache. Trocar em tempo de execução (opção do menu) exige invalidar o
   cache por endereço.

## 6. O que isso faz com os roteiros de teste

As referências dos roteiros (`~/referencias/*.telas`) são **rastros de troca de tela**, não imagens: não mudam. As
imagens (`IMG=1`) comparam a x86 com a PPC sem mapa, **as duas com os mesmos dados do `gamedata`** e sem
`PORT_X360`; como nem (a) (só mexe na ISO convertida) nem (b) (só em `PORT_X360`) alteram os dados e o código
dessas builds, a igualdade segue. O que muda são as **capturas do console** contra a x86 na F5 e na F6: as áreas
de HUD, pausa, lição e lista de golpes passam a diferir só nos ícones, o que precisa ser dito nas tolerâncias.
A `.text` da x86 não muda (o código novo é só `PORT_X360`).

## 7. O que não ficou provado

- Ícones em texturas CT32/CT16, em telas inteiras e em formas Flash (seção 1.5).
- Onde ficam o "PRESS START" do título e qualquer SELECT/START desenhado em imagem.
- Os nomes dos tipos 1 a 3, que nunca aparecem em tela (tabelas vazias).
- As definições do HUD para texturas acima de 27: não existem nesta folha; não vi a de onde vieram.
- A troca em si (nada foi implementado, como pedido).

## 8. Como foi feita a varredura

Um script Python de varredura chegou a 7,6 GB e foi morto: o decodificador de BPE, ao testar candidatos que não eram
BPE, crescia a pilha e a saída sem limite. A segunda versão: `ulimit -v 1500000`, arquivo por arquivo em fluxo,
decodificador de BPE que recusa `size <= 0`, exige a cauda depois do `packSize` de no máximo 128 bytes, limita a
pilha a 254 e a saída ao `size`, e filtros `in` de bytes antes de qualquer expressão regular (cada folha passava
por 20 expressões e a varredura demorava demais). Rodou em cerca de 5 minutos com menos de 600 MB.
