# F7, piloto: o banco de carga

[Escopo](00-escopo.md) | [Etapa 1](01.md) | [Pilotos](02-pilotos.md) | [Etapa 4](04.md) | [Índice](../../plano.md)

Legenda como no [escopo](00-escopo.md): **[medido]**, **[lido]**, **[estimado]**.

[Medido] **Estado em 29/10/2026, 17h50:** o banco existe e roda na x86, na x86r e na PPC (ligado e montado), sem
mudar o jogo; os 698 modelos e os 35 estágios do manifesto passam pela carga do jogo na x86 em 3,6 s; o
verificador foi visto falhando de propósito na comparação (retrato fabricado) e na rede de larguras (x86r). A PPC
no qemu ficou na fila de `~/wsl.lock` atrás da prova do alfa e da dos lotes: a rodada oficial do piloto, com os três
estragos na PPC, está **armada como serviço** e o resultado entra na seção 4 (como ler: seção 8). Branch
`claude/f7-banco-carga`, sobre a main `e005ac94`.

## 1. A ideia

[Lido] A etapa 4 ([04](04.md)) testaria o conteúdo rodando o jogo inteiro pelos menus, caso a caso, na x86 e na
PPC: 256 a 316 h de máquina (seção 2 de lá), quase tudo PPC no qemu, e por isso desarmada pela decisão 35. O risco
que ela quer pegar é um arquivo do jogo lido errado no big-endian. A queda do Final Flash no Broly
([queda-relocate](queda-relocate.md), branch `teste/queda-console`) mostrou o caminho curto: a prova foi chamar na
PPC as próprias rotinas de carga (`EftBurst_RelocateModel`, `Res_RelocateOffsets`) sobre o arquivo cru e o
convertido. O banco generaliza isso: chama só as rotinas de carga do jogo sobre os arquivos de um personagem ou de
um estágio, na x86 com os originais e na PPC com os convertidos pelo `conv_tool` (o mesmo conversor da ISO
convertida), e compara o que sai. Sem menu, sem luta, sem desenho.

## 2. O que foi isolado

[Lido] Ler o arquivo do AFS e deixá-lo utilizável passa por estado global demais para isolar função a função (o
heap, o pool de objetos, os pools de partes e efeitos, a tabela de recursos, o trabalho da luta, as tabelas do
SLUS). Em vez de recompor esse estado, o banco **liga com todos os objetos da build sem janela** (`headless.py`,
x86 e PPC, os mesmos 398/414 objetos, com as mesmas opções de ligação, agora em `headless.flags_ligacao`) mais
`port/x360/headless/banco.c`, e entra por `--wrap=BtlGame_PreUpdate`: a luta de demonstração (`BT3_DEMO=1`) sobe
pelo caminho normal até a carga inicial terminar (os dois objetos prontos, nenhum job nem leitura pendente), e
nesse quadro o banco toma o laço e não devolve. A partir daí só roda código do jogo, na ordem do jogo:

| Caso | Caminho do jogo | Arquivos |
|---|---|---|
| `personagem ID TRAJE VARIANTE` | o da troca de personagem e da recarga (`BtlLoad_PollCharaRequest`, `BtlLoad_Reload`): `BtlScene_FreeChar`, `BattleSide_SetForm` e o job `BtlLoad_StepChara` até o fim. Dentro dele: `BtlRes_Reload` e `File_UpdateRequests` (o leitor de arquivos do port, `BT3_DATA`), `BtlRes_CommitReload2`, o banco de voz, `BtlObj_Rebind` (`BtlObjMdl_Init`: tabelas, `BindFiles` com as relocações e o rebase das texturas, partes, corpo, rosto, sombra, animação), `BtlObjAnim_PlayAuto`, `BtlScene_CreateChar` (a ligação do pacote de efeitos, `EftShot_CreateChar` e `EftChar_Create`) e o `Res_RelocateOffsets` do bloco do membro. Depois, **todas** as animações do arquivo de movimentos por `BtlObjAnim_Load` (na PPC, com o `Port_ConvertAnim`) | modelo `0x590 + id*10 + traje (+4)`, movimentos `+8`, efeitos `+9`, bloco do membro `8 + id*2` (lado 0), voz `0xBDA + id` |
| `estagio N` | o job `BtlLoad_StepStageReload` até o fim: `BtlLoad_BeginStageSwap`, a leitura, o banco de som, `BtlLoad_EndStageSwap` (`StgModel_InitStage`, `BtlChars_OnStageLoaded`, `StgNav_Rebind`, `EftStage_Recreate`, `StgFx_Reset`) | estágio `0x171 + N`, som `0x14E + N` |

[Medido] Depois de cada caso o banco grava um retrato no formato dos dumps do `state.c` (`nome endereço tamanho` e
os bytes): os arquivos no buffer do jogo depois da carga, no tamanho exato do arquivo (o resto do buffer fixo é do
arquivo anterior), cada animação descomprimida com o nome de conteúdo que o `conv_tool` entende
(`pzs3us1/01441.bin@0x700#bpe`) e o objeto do lutador (`BtlObj`, 0x1670 bytes). Nada do jogo muda: o banco só
acrescenta arquivos de teste, e a `.text` da x86 saiu `844d20c11329fff807d49472ff89bc794f249413` antes e depois
(`e005ac94` e `b210336b`, 104 stubs).

## 3. A comparação

[Lido] `banco.py compara` lê os dois retratos de um caso. As regiões têm de ser as mesmas, na mesma ordem e do
mesmo tamanho, e as do heap no mesmo endereço (o heap do jogo fica no mesmo lugar nas duas builds; só o buffer das
animações, do próprio banco, muda).

- **Arquivos e animações:** pela marcação do conversor (`conv_tool --marcas`, a largura de cada número). Número de
  largura w com os w bytes invertidos e byte igual são conformes. O que a carga grava (ponteiros relocados, rebase
  das texturas, ligação dos efeitos) continua conforme se é número nativo da mesma largura: ponteiro para o heap é
  igual nas duas builds, ponteiro para o programa é conforme se cai no mesmo símbolo e deslocamento (`nm`). Uma
  palavra sem marca com os quatro bytes invertidos conta como "número gravado fora das marcas".
- **Objeto:** pela folha do DWARF de cada byte, juntando as três visões da decomp (`BobjObj`, `ObjMdlObj`,
  `BtlObj`, 1.007 folhas, 3.772 bytes tipados); o que nenhuma tipa (vetores `unk`) vale como byte igual ou palavra
  invertida, contado à parte.
- **Larguras (RASTREIO=1):** a comparação acima usa as marcas do próprio conversor, então um conversor que desse a
  largura errada a um campo concordaria consigo nas duas pontas. Por isso o banco roda também na x86r: o
  `rastreio.c` anota cada leitura de número das rotinas de carga, o `rastreio.py mapa` faz o mapa e o
  `conv_tool --mapa` confere que cada posição lida como número tem no conversor a mesma largura (a rede 3 do
  `roteiros.sh`, aqui só sobre a carga).

[Lido] `banco.sh LISTA SAIDA` faz a rodada inteira: x86 (com `BT3_LEU=1`, a lista do que a PPC vai pedir),
`conv_tool --saida` desses arquivos, `conv_tool --marcas` das regiões, a PPC no qemu sob `flock ~/wsl.lock` só
durante ela, e a comparação caso a caso; sai 0 só com tudo certo.

## 4. A prova: o verificador visto falhando

[Medido] Os estragos que o pedido exigia e o que cada um mostra:

| Estrago | Onde | Esperado | Visto |
|---|---|---|---|
| Arquivo sem conversão (o caso da transição do Final Flash): o modelo original no lugar do convertido e ainda na lista `BT3_CONVERTIDOS` | PPC, `banco.sh NEGATIVOS=1` | a carga lê os deslocamentos crus: queda ("banco: FALHA ... sinal") ou números fora na comparação | **pendente** (rodada armada, seção 8) |
| Um byte trocado no modelo convertido (`0x5A` em +0x2000) | PPC | aquele byte fora | **pendente** (rodada armada, seção 8) |
| Uma rotina de carga que pula um campo: `BT3_BANCO_MUTANTE=pula`, o `Res_RelocateOffsets` deixa a paleta da última textura de cada `TexFile` sem relocar | PPC | os ponteiros `clut` fora | **pendente** (rodada armada, seção 8) |
| Íntegro | PPC | os 12 casos do piloto conformes | **pendente** (rodada armada, seção 8) |
| Uma largura trocada no mapa da x86r (a primeira posição de 4 bytes vira 2) | x86r, `RASTREIO=1 NEGATIVOS=1` | `conv_tool --mapa` reprova | **reprovado** (duas rodadas, `~/banco/r2` e a oficial) |

[Medido] Antes da PPC, o comparador foi provado contra um retrato "PPC" fabricado da x86 pelas próprias marcas
(script de laboratório, fora do git): íntegro, 363 regiões conformes (3.999.420 bytes de arquivo e animação,
1.007 folhas do objeto e 493 palavras sem tipo); o mesmo retrato sem nenhuma inversão (o arquivo cru) deu 493.859
números fora e saída 1; um byte trocado no modelo deu exatamente 1 fora (`modelo +0x1234`) e saída 1.

## 5. Números medidos

[Medido] No WSL (12 threads, 8 GB), relógio de parede, com os arquivos no cache:

| Medida | Casos | Tempo | Observação |
|---|---|---:|---|
| x86, sem retratos, o manifesto inteiro | 698 modelos + 35 estágios | 3,6 s (processo inteiro, com a subida da luta) | 0 falhas, 25 MB de memória |
| x86, o piloto (`banco_piloto.txt`) | 9 personagens + 3 estágios | 0,04 a 0,12 s | por personagem: carga 1 a 2 ms, 357 animações descomprimidas em 5 ms, retrato 1,4 ms |
| x86 com retratos, amostra (um modelo a cada 35 e um estágio a cada 7) | 20 + 5 | 0,20 s | 100 MB de retratos: 4 MB por personagem, 6,7 MB por estágio |
| `conv_tool --saida` da amostra | 130 arquivos (93 MB) | 0,43 s | |
| `conv_tool --marcas` da amostra | 7.190 regiões (arquivos e animações) | 1,6 s | 226 MB de marcas e `.orig` |
| `banco.py compara` da amostra | 25 casos | 12 s | 0,5 s por caso |
| x86r, o piloto | 12 casos | 15 s, 1,3 GB | 8 milhões de posições anotadas |
| `rastreio.py mapa` + `conv_tool --mapa`, o piloto | 74 arquivos e conteúdos, 40.826 trechos | 77 s (3,4 GB) + 0,8 s | |
| PPC (qemu), o piloto | 12 casos | **pendente** (rodada armada, seção 8) | [estimado] uns 16 vezes a x86, a razão PPC sem mapa contra uma x86 nos pilotos da F7 (396 s contra uns 25 s na Missão 0, [02-pilotos](02-pilotos.md)) |

[Lido] Uma rodada inteira de `banco.sh` sobre o piloto, sem a espera pela trava, é dominada pelo mapa da x86r
(cerca de 2 min); a PPC [estimado] deve ficar em segundos e a comparação dos 12 casos em uns 6 s.

## 6. Estimativa da etapa 4 com o banco

[Estimado] Pelas medidas acima, o elenco inteiro (698 modelos, os 35 estágios, os efeitos e as animações dentro
de cada personagem) pelo banco, em lotes de uma dezena de casos por processo (memória da x86r e disco dos
retratos, que se apagam depois de comparados):

| Parte | Escala | Horas |
|---|---|---:|
| x86 com retratos | 733 casos, uns 3 GB de retratos que vão e voltam em lotes | menos de 0,1 |
| `conv_tool --saida` e `--marcas` | uns 1.300 arquivos e 60 mil animações | menos de 0,1 |
| comparação | 733 casos a 0,5 s | 0,1 |
| x86r e mapa | 733 casos a uns 8 s | 1,5 a 2 |
| PPC no qemu | 733 casos: 3,6 s da x86 vezes uns 16, mais a escrita dos retratos e uma subida da luta por lote | 0,1 a 0,5 |
| **Total** | | **2 a 3** |

[Estimado] Contra as **256 a 316 h** da etapa 4 de hoje ([04](04.md), seção 2): o banco substitui as famílias
de censo, que só pedem carga: **E1** (35 a 58 h; os 349 pares, as 161 animações e os 33 estágios já no piloto; o
rosto do lado 1 e a outra voz por extensão direta), **E2** forçada (40 a 45 h; a variante 1 dos 349 pares já
carrega no banco, ficam os dois casos reais do mecanismo) e **S3** (35 h; os estágios divididos por extensão
direta). São **110 a 138 h** que passam para umas duas horas. Ficam como luta, porque são comportamento e não
carga: **T** (45 a 60 h), **F** (15 h), **S1** (3 h), **S2** (23 a 30 h), **E3** (60 a 70 h) e os dois casos
reais de E2, **146 a 178 h** no ritmo de hoje, das quais a maior parte é PPC, memória e imagens; com a decisão 35
(a x86 roda tudo e a PPC uma amostra por família) e o banco garantindo a carga de todos os ids na PPC, a PPC
dessas famílias pode ficar em um ou dois casos por família.

## 7. O que o banco não cobre

[Lido] O banco prova a carga. Fica fora, e pede luta (na x86 por inteiro e na PPC por amostra, decisão 35) ou uma
extensão do banco:

| Fora do banco | Por quê | Caminho |
|---|---|---|
| Rosto e bloco do membro do lado 1 (`8 + id*2 + 1`), a outra voz (`0xC7B + id`) | o piloto carrega só no lado 0, com a voz do cartão | extensão direta do banco: o mesmo job no lado 1 e a bandeira `SAVE_FLAG_VOICE` trocada |
| Estágios divididos (`0x198 + N`), transições `0x1BF`/`0x1C0` | o piloto usa a recarga de tela cheia | extensão direta: `Battle_IsSplitScreen` e o job `BtlLoad_StepStageChange` (o caso da queda) |
| Números lidos só durante o combate: parâmetros (`BtlParam`, golpes, rajadas, técnicas, IA), chaves das animações ao tocar, câmeras (`DemoCam_FixupAnim` na primeira vez), lábio | a carga só guarda o ponteiro; a conversão desses campos é conferida pelas marcas (consistência) e a largura só pelo rastreio dos roteiros já rodados | estender o banco para tocar cada animação (`BtlObjAnim_SamplePose`) e cada câmera, com a x86r; o resto em luta |
| Modelos de efeito criados no uso (`EftObj_Create` dos pacotes de técnica e do personagem) | só existem quando a técnica dispara | extensão: chamar a criação de cada efeito do pacote; senão luta (E3) |
| Variante 1 pelo mecanismo do jogo, transformação e fusão por R3, troca de cenário no meio da luta | é lógica de luta, não de carga (o banco já carrega os modelos e estágios de destino) | luta: famílias T, F, S2 e os dois casos reais de E2 |
| GPU, som tocado, imagem | o banco não desenha nem toca | as redes de imagem e som dos roteiros, na amostra |

## 8. Recomendação e como retomar

[Estimado] **Recomendação:** adotar o banco como a rede de conteúdo da PPC para a etapa 4. Ordem:

1. Ler a rodada armada (abaixo). Com o íntegro conforme e os três estragos reprovados na PPC, o banco vale.
2. Estender o banco nas extensões diretas da seção 7 (lado 1, a outra voz, estágios divididos, as duas transições),
   cada uma com o seu estrago, e rodar o elenco inteiro em lotes com `RASTREIO=1`.
3. Estender a carga espalhada que dá para chamar sem luta: tocar cada animação (`BtlObjAnim_SamplePose` em alguns
   quadros) e cada câmera, e criar cada efeito dos pacotes (`EftObj_Create`), também com a x86r para as larguras.
4. Na luta, só as famílias de comportamento (T, F, S1, S2, E3 e os dois casos reais de E2), a x86 inteira e a PPC
   por amostra, como na decisão 35.

[Medido] **A rodada armada:** unidade `systemd --user` `bt3-banco-piloto-0eef00b5.service`, script
`~/banco-rodada.sh 0eef00b5`, worktree fixo `~/bt3xbx-banco-rodada` no commit `0eef00b5`. Ela refaz as builds sem
janela (x86, PPC e x86r) e o banco, confere a `.text` (saiu `844d20c1...`), roda
`RASTREIO=1 NEGATIVOS=1 banco.sh banco_piloto.txt` e só pega `~/wsl.lock` em volta de cada `qemu-ppc`. Fim: a
última linha de `~/banco/rodada-0eef00b5/servico.log` é `FIM RC=N`; o relatório em
`~/banco/rodada-0eef00b5/piloto.log` (uma linha por estrago, "tudo certo" no fim), os tempos em
`piloto/tempos.tsv` (`ppc` e `neg_*` medidos dentro da trava, `espera+*` com a espera) e os tempos por caso nas
linhas `banco: caso` de `piloto/ppc.txt`. Os retratos (uns 100 MB) ficam em `piloto/x86`, `piloto/ppc` e
`piloto/neg_*`: apagar depois de ler.

```sh
port/x360/tools/headless.py --alvo x86 && port/x360/tools/headless.py --alvo ppc
port/x360/tools/headless.py --alvo x86 --rastreio && port/x360/tools/banco.py construir --rastreio
RASTREIO=1 NEGATIVOS=1 port/x360/tools/banco.sh port/x360/tools/banco_piloto.txt ~/banco/NOVA
```

## 9. Extensão de 29/10 à noite: objetos extras, lado 1, outra voz, estágios divididos, transições (parcial)

[Medido] **A queda do Dragon Fist (Goku SSJ3, chara 6) e a trava do Father-Son Kamehameha (Gohan SSJ2, chara 16)
são objetos extras sem conversor.** O `BtlLoad_StepObject` pede `0xC1C + id` para id >= 0x100 (`BTL_FILE_OBJECT`),
e o conversor não tem faixa para os ids 0xD1C..0xD3B nem 0xD3E..0xD44 (índices 3355..3386 e 3389..3395): o 360 lê
esses arquivos crus (o `iso360.c` só registra no diário a leitura de arquivo com conversor, por isso o log da queda
não mostra a leitura). Dragon Fist: golpe 0x2BC do chara 6, objeto 256, **`pzs3us1/03355.bin`** (id 0xD1C), cru,
`MdlTex_RebaseChain` segue um deslocamento little-endian e lê `BFDC9D5A`. Father-Son: golpe 0x2B0 do chara 16,
objeto 267, **`pzs3us1/03366.bin`** (id 0xD27), cru (a trava é laço sobre a cadeia lixo; não provado na PPC ainda).

[Medido] A lista estática, tirada das tabelas do próprio jogo na x86 (`BT3_BANCO_PEDIDOS=1` sobre os 698 modelos do
manifesto, `banco.py objetos`, 70 casos em `~/banco/obj/objetos.txt`): os golpes pedem os objetos 256, 257, 258,
261, 262, 267, 274..279, 281..284, 286, 287 (arquivos 03355, 03356, 03357, 03360, 03361, 03366, 03373..03378,
03380..03383, 03385, 03386, **todos sem conversor**) e modelos de personagem (ids < 0x100: 4, 11, 18, 20, 23, 47, 57,
88, 89, 96, 102, 112; com conversor). Transformação de sequência 3 pede 101 e 102 (traje 2); fusões pedem 3, 10, 34,
38, 44..47 (com conversor). Sem pedido achado no código (nenhum outro chamador de `BtlChange_RequestObject`): 259,
260, 263..266, 268..273, 280, 285 e 0x122..0x128 (03389..03395, sete arquivos iguais de 45.696 bytes). Os 44 arquivos
0xD1C..0xD47 carregam como modelo na x86 pelo caminho do `StepObject` e o `conv_tool --formato personagem` cobre
100% de cada um (03355: 526.086 bytes em números trocados).

[Medido] O banco agora tem (branch `claude/f7-banco-carga`, x86 conferida): `objeto ID TRAJE VARIANTE` (o job
`BtlLoad_StepObject` no tipo 0, retrato `objeto=` e `obj=`, depois toca cada animação do modelo com
`BtlObjAnim_SamplePose` e solta com `BtlObj_Destroy`/`BtlRes_Free`), `personagem ID TRAJE VARIANTE LADO VOZ` (lado 1:
bloco `8 + id*2 + 1`; voz 1: `0xC7B + id`), `estagio_dividido N` (0x198 + N), `troca N` (o `BtlLoad_StepStageChange`
até o Fade, retrato `transicao=`, depois o fim do job), `BT3_BANCO_TETO` (SIGALRM: laço vira `banco: FALHA` com o id
do arquivo) e `BT3_BANCO_PEDIDOS`. O `banco.sh` ganhou `CONV=PASTA` (o conversor a provar), a rede "arquivo lido pela
carga sem conversor" (antes da PPC) e os estragos `objcru` e `transcru`. Na x86: os 70 objetos pedidos e os 44 de
0xD1C..0xD47 carregam (0,1 s), os 35 estágios divididos, `troca 3` (00446), `troca 5`/`troca 0` (00447) e os lados 1
com a outra voz passam. **A PPC não rodou** (a trava estava com o alfa e com os lotes desde a tarde).

**O que falta (para quem continuar):**

1. Correção do conversor, branch `claude/conv-objetos` (worktree WSL `~/bt3xbx-convobj`, criado de
   `troca/codex/conv-console` 6210953c, **sem commit ainda**; o `09a325dc` do conv-console está só no notebook):
   em `port/x360/conv/conv.c`, `sFaixas`, trocar as duas faixas do ShenScene por `{0xD1C - 1, 0xD47 - 1, CONV_ARQ,
   "personagem"}` com o comentário do `BTL_FILE_OBJECT` (os 0xD3E..0xD44 entram por serem do mesmo formato e da mesma
   faixa, todos 100%); documentar em `docs/x360/formatos/personagem.md` (os objetos são arquivo de modelo de slot);
   **subir a marca para versão 4**: `port/x360/src/iso360.h` (`ISO_MARCA_VERSAO`), `port/x360/tools/convert_iso.py`
   (`VERSAO`), `port/x360/conv/iso_conv.c` (o total `3327` vira **3366**, +39 arquivos: 32 de 0xD1C..0xD3B e 7 de
   0xD3E..0xD44 e já contados os 5 do ShenScene; conferir com `conv_tool --lista-conversores | wc -l`),
   `teste_iso_avisos.py` (recusar 3), `teste_iso_convertida.py` (versões antigas 1, 2, 3),
   `preparar_iso_console.py`/`build_f2.sh` (o texto do selo), `docs/x360/formatos/iso-convertida.md` (seção da
   versão 4). O Lucas terá de **reconverter a ISO USA original** (no console ou no PC) com o instalador novo.
2. Provar a correção no banco: `CONV=~/bt3xbx-convobj/port/x360/conv RASTREIO=1 NEGATIVOS=1 banco.sh LISTA SAIDA`
   com uma lista que comece por um personagem e tenha os objetos e as trocas (por exemplo `personagem 6 0 0`,
   `objeto 256 0 0`, `objeto 267 0 0`, `troca 3`, `troca 0`, o resto de `~/banco/obj/objetos.txt`). Esperado: rede sem
   conversor limpa, PPC conforme, `objcru` e `transcru` reprovados. E o contrário, ver falhando: o mesmo sem `CONV`
   (o conversor da main, sem objetos nem transições) tem de reprovar na rede e cair/reprovar na PPC no 256.
3. A rodada armada do piloto (`bt3-banco-piloto-0eef00b5`) segue na fila com o banco antigo; pode ser parada e
   trocada por uma rodada do item 2 (serviço `systemd-run --user`, qemu só sob `flock ~/wsl.lock`).
4. O elenco inteiro em lotes (`RETRATOS=0`, umas 50 casas por lote): 698 personagens, um caso `personagem ID 0 0 1 1`
   por id, 35 `estagio`, 35 `estagio_dividido`, `troca 3` e `troca 0`, os objetos 256..299 e os de
   `objetos.txt`. Falta escrever o laço de lotes (o `banco.sh` roda a lista inteira num processo).

## 10. ISO versão 4: correção dos objetos (29/10/2026)

[Lido] A base `09a325dc` já contém as transições `0x1BF..0x1C0`. A faixa
`0xD1C..0xD47` agora recebe `personagem`, incluindo os cinco modelos de ShenScene
anteriormente cadastrados. São 3.366 arquivos, +39.

[Medido] `conv_tool` reconheceu integralmente os 44 arquivos `03355..03398.bin`.
A marca passou a 4 em ambos os instaladores e no leitor; os testes passam a
recusar também a versão 3. A prova do banco na PPC será registrada após a fila
do WSL; cobertura estrutural não basta como prova da carga.

A rodada usa o banco externo de `07f5f5a7`, com a unidade de teste ligada aos
objetos finais desta worktree. A revisão anterior `0eef00b5` não contém os
casos extras. A lista tem 131 casos: os personagens 6 e 16, lado 1/voz 1,
o piloto, os 44 objetos 256..299, os 70 pedidos e as transições 3 e 0.
O controle com o conversor `09a325dc` deve reprovar na rede e na PPC;
com `CONV=` apontando para a versão 4, exige carga e rastreio conformes e os
seis estragos recusados. Resultado final e logs em `~/entrega-iso-v4.md`.

[Medido] A primeira rodada oficial terminou os 131 casos na PPC, mas seu
adaptador deixou de importar o módulo externo ao coletar as regiões. As
marcas ficaram vazias e a comparação reprovou. Após corrigir a preparação,
os mesmos 131 retratos ficaram conformes, com marcas completas, e o byte
alterado reprovou por diferença de conteúdo. Essa reanálise não substitui a
nova rodada oficial das builds finais. O adaptador agora interrompe antes
da PPC se faltar importação, região ou marca; ambos os primeiros controles
foram vistos falhando. O controle negativo não aceita erro de infraestrutura
como diferença de conteúdo. Evidências em `~/iso-v4/banco-rejulgado/` e
`~/iso-v4/sonda-adaptador/`.

[Medido] A nova rodada de `28882f91` passou os 131 casos na PPC e os seis
controles negativos (`largura`, `cru`, `byte`, `pula`, `objcru`, `transcru`).
O controle com o conversor `09a325dc` reprovou na rede de arquivos sem
conversor e na PPC, como esperado. Evidências em
`~/iso-v4/banco-final-28882f91.log` e `banco-sem-faixa-28882f91.log`.
A rodada parou depois no comparador de memória dos menus, conforme a nota
da F8, sem falha no banco. A correção do comparador não muda os conversores
nem o jogo; o banco será repetido com os carimbos finais.

[Medido] A repetição com `550d8532` passou novamente os 131 casos, o
rastreio e os seis estragos, com marcas completas. O controle sem a faixa
reprovou na rede e na PPC. Os sete primeiros roteiros de menu passaram;
a rodada parou no Ultimate por fragmentos de arquivos em blocos
reutilizados, descritos na nota da F8. A nova correção só muda ferramentas
de comparação e documentação; o banco será repetido com o carimbo final.
