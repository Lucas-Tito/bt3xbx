# Decisões do port para Xbox 360

Numeradas e com o porquê; o que foi recusado também fica. Hipótese que se provou errada não se
apaga: ganha nota dizendo o que se viu.

## 1. Fork do Tenkaichi3Decomp, não submódulo (09/10/2026)

O `bt3xbx` nasce do histórico do [Tenkaichi3Decomp](https://github.com/z3xox/Tenkaichi3Decomp)
(remote `upstream`). O port de PC já roda o jogo inteiro e tem o teste de replay contra o PCSX2;
recomeçar da decomp pura jogaria isso fora.

Recusado: submódulo com patches. O código do jogo vai precisar de `#ifdef` para o 360
(endianness), e patch em cima de submódulo é o tipo de coisa esquisita que o projeto quer evitar.
Novidades do upstream entram por `git merge upstream/main`, feito de propósito.

Tudo nosso fica em `port/x360/` e `docs/x360/`, para o merge do upstream não conflitar.

## 2. Title ID 0xFFFF4254, nome e ícone num SPA gerado por script (09/10/2026)

O prefixo `0xFFFF` é o dos títulos de desenvolvimento, longe dos IDs de jogos de verdade (o port de
Virtua Fighter usa `0xFFFF5361`). `4254` é "BT" em ASCII.

O XDK monta o SPA (o arquivo XDBF com nome e ícone que o Aurora e a FreeStyle leem) com o
`spac.exe`, mas ele é .NET e não roda no wine da imagem. `port/x360/tools/make_spa.py` grava o
mínimo (XTHD, XSTC, nome em inglês e ícone 64x64) no layout conferido com o SPA do port de Virtua
Fighter, e o `imagexex -section:FFFF4254=spa.bin` o embute. O ícone é arte própria
(`make_icon.py`), sem nada do jogo.

## 3. O jogo compila em C (`-TC`), não em C++ (09/10/2026)

Levantamento com `port/x360/tools/survey.sh` (só sintaxe, os 276 fontes de `src/`):

| Modo | Sem ajuda | Com `msvc_compat.h` |
|---|---|---|
| `-TC` (C89 do `cl.exe` 16.00) | 89 de 276 | **256 de 276** |
| `-TP` (C++) | | 93 de 276 (`void *` implícito em todo lugar) |
| `-TC` com o `cl.exe` 16.10 do TechPreview | | 256 de 276 (igual: também é C89) |

O `msvc_compat.h` apaga `__attribute__` e `__asm__`, troca `inline` por `__inline` e declara o
`getenv` que falta na CRT do Xbox. Os 20 arquivos que sobram, classificados pelo **primeiro** erro de
cada um (um arquivo pode ter mais de uma extensão), usam extensões do GCC: intervalo em
`case a ... b` (3), literal composto `(T){...}` (7), declaração depois de comando (4),
expressão-comando `({ ... })` (2), `typeof` (1) e assembly do VU0 (3, que o port de PC já troca por
C: `port/tools/portsrc.py`).

Pendências que este levantamento não cobre e que entram na F1:
- **Ponto flutuante.** O port de PC compila o jogo com `-msoft-float` e faz cada operação em
  software, igual ao PS2 (`port/src/softfloat_ps2.c`). O MSVC não tem essa opção; a sonda da F0
  mede quanto custa e quanto a FPU do Xenon difere.
- **Constantes de float.** O `port/tools/eeconst.py` reescreve constantes como o compilador do PS2
  (truncando) e as grava em hexadecimal (`0x1.99999ap-4f`), que o MSVC 2010 não aceita. Dá para
  gravar em decimal com dígitos suficientes.
- **Alinhamento.** 120 `__attribute__((aligned(16)))` somem **em silêncio** com o `msvc_compat.h`
  de levantamento. É perigoso: no 360 o `lvx`/`lvx128` ignora os 4 bits baixos do endereço, então
  uma struct desalinhada lida pelo VMX dá dado errado sem crash. Na F1 o `msvc_compat.h` tem de
  transformar `aligned` em `__declspec(align(16))` na posição certa ou falhar o build, nunca apagar.
- **Nomes de assembly** (`extern ... __asm__("Outro")`, 9 casos): viram `/alternatename` do linker.
- **`s128`/`u128`** (`mode(TI)`): o MSVC não tem inteiro de 128 bits.

Recusado: C++. Converter `void *` implícito em ~1.100 lugares mexe no código do jogo inteiro.

## 4. Ponto flutuante do 360: sem `-fp:fast`, e o compilador não pode dobrar contas (09/10/2026)

`-fp:fast` deixa o compilador fundir multiplicação e soma em `fmadds`, que arredonda uma vez só e
muda o resultado em relação ao PS2. O port usa `-fp:precise`. Isso não basta:

- A doc do XDK diz que `fp_contract` vem **ligado** por padrão. Num teste o `-fp:precise` não fundiu
  `a*b+c`, mas nada garante para todo padrão: o código do jogo leva `#pragma fp_contract(off)`, e o
  build deve procurar `fmadds`/`fmsubs`/`fnmadds`/`fnmsubs` na desmontagem e falhar se achar.
- O compilador **dobra contas de constantes arredondando ao mais próximo**, mesmo com
  `-fp:precise`: `1.0f/30.0f` vira `3D088889` no binário, inclusive depois de inline. Com a FPU em
  truncamento isso diverge do PS2 (que dá `3D088888`). Saídas: `#pragma fenv_access(on)` nos fontes
  do jogo (impede a dobra; custo a medir) ou levar a dobra para o `eeconst.py`, que já reproduz a do
  compilador do PS2.
- O modelo do PS2 sob o PCSX2 (`softfloat_ps2.c`) é soma e multiplicação **truncadas**, divisão da
  FPU **ao mais próximo**. A FPU do 360 tem um modo só por vez: ou se troca o modo a cada divisão,
  ou a divisão sai de outro jeito. A sonda da F0 mede as duas coisas.

Revisão de 09/10/2026: a primeira versão da sonda comparava tudo em truncamento (a divisão ficaria
"errada" em metade dos casos por erro do teste) e dobrava `1.0f/3.0f` na compilação, de modo que o
teste do modo nunca passaria pela FPU. Corrigido antes de rodar no console.

## 5. Resultados da sonda da F0 no console (09/10/2026)

Log completo em `medicoes/f0-sonda-1.log` (Xbox 360 E, kernel 17150, FreeStyle 3.0.780).

**Memória em endereço fixo: funciona.** As regiões do PS2 abaixo de `0x40000000` reservam e
confirmam no endereço exato, antes do D3D: `0x00100000..0x02000000` (dados + heap), a pilha em
`0x02000000`, `0x10000000`, `0x12000000` e 192 MB só reservados em `0x13000000`. Livre: 478 MB no
início, 430 MB depois de reservar, 424 MB com o D3D.

**`0x70000000` (scratchpad) não dá:** é ali que moram as pilhas das threads (a da main estava em
`0x7004FC90`, a de medição em `0x700DFEE0`). O scratchpad do port vai para outro endereço, e o código
do jogo que usa `0x70000000` como constante precisa ser traduzido.

**Páginas de 4 KB custam caro:** leitura aleatória em 32 MB levou 345 ns com páginas de 4 KB contra
120 ns com 64 KB (2,9x, falta de TLB). A RAM do PS2 em identidade fica na faixa de 4 KB. Antes de
fixar o mapeamento é preciso ver quanto o jogo depende do endereço exato (tabelas do ELF com
ponteiros absolutos, comparação com o PCSX2) e se a heap pode ir para `0x40000000+` com páginas de
64 KB.

**Leitura invertida é barata:** `lwbrx` 1,38 ns contra `lwz` 0,63 ns por palavra na cache. Trocar
bytes na leitura não é o problema. **int -> float é caro:** 12 ns (load-hit-store).

**Ponto flutuante: hardware, não software.**

| Operação | Hardware (truncado) | Software do PS2 |
|---|---|---|
| soma | 2,4 ns | 143 ns |
| multiplicação | 2,25 ns | 78 ns |
| divisão | 26,6 ns | 77 ns |

O software é 30 a 60 vezes mais lento: 1 milhão de somas por quadro já passaria de 140 ms. Fora de
questão no Xenon.

Exatidão do hardware contra o PS2 (sob o PCSX2), 1 milhão de pares por caso:
- **Multiplicação truncada: igual em 100%** na faixa comum e perto do estouro.
- **Divisão ao mais próximo: igual em 100%** na faixa comum. Perto do estouro dá infinito onde o PS2
  dá `7F7FFFFF`.
- **Soma truncada: difere em 30%** (sempre 1 ulp), por causa do bit de guarda do somador do PS2. Com
  a máscara inteira antes da soma fica igual em 100%, mas custa 44 ns (passa pelos bits:
  load-hit-store).
- **Denormais:** o `_controlfp(_DN_*)` não tem efeito nenhum no 360 (os quatro modos dão o mesmo
  resultado). Resultados perto do zero saem denormais onde o PS2 dá zero. Falta testar o bit NI do
  FPSCR direto.
- **Trocar o modo de arredondamento por divisão: 2.426 ns.** Inviável. A divisão ao mais próximo
  com a FPU em truncamento tem de sair de outro jeito (ex.: dividir em double e arredondar).

Conclusão para a F1: FPU de hardware em truncamento. Falta decidir quanto da exatidão vale o custo:
a soma com máscara (44 ns contra 2,4 ns), a divisão ao mais próximo, os denormais e o estouro. Sem
eles o jogo roda, mas o replay de validação deixa de bater bit a bit com o PCSX2.

## 6. A F1 roda no Windows do Lucas, pelo WSL, comandada daqui por SSH (10/10/2026)

Este computador não pode receber o ISO nem rodar os testes com os dados do jogo. O Windows dele
(`192.168.0.5`) tem o OpenSSH Server (porta 22 liberada só para `192.168.0.0/24`) e o Ubuntu 24.04
no WSL. Daqui: `ssh bt3win "wsl -d Ubuntu-24.04 -u lucas -- bash -lc '...'"` (alias em
`~/.ssh/config`, chave só para isso). O ISO, os dados extraídos e os builds ficam lá; para cá só
voltam texto e logs.

- Preparo versionado: `port/x360/tools/preparar-wsl.sh` (roda como root, idempotente).
- O repositório lá é clonado com uma deploy key **só de leitura**; o código se edita aqui e se
  puxa lá.
- Sem `gcc-multilib`: conflita com o `gcc-powerpc-linux-gnu` no Ubuntu. A referência x86 é a build
  de 64 bits do upstream, que exige **clang/LLVM 22** (o do container de release dele); com o
  clang 18 do Ubuntu, 3 arquivos falham no `ptr32.py`.
- Resultado em 10/10/2026: o ISO confere com os hashes do lançamento americano, 68.611 arquivos
  extraídos, `port/build/bt3_64` compilado e a luta de demonstração rodou até o fim.

## 7. F1: build PowerPC sem janela + rastreio, e as três classes de problema de ordem de bytes (10/10/2026)

Ferramentas (todas em `port/x360/`, rodando no WSL, decisão 6):
- `tools/headless.py --alvo x86|ppc [--rastreio] [--chamadas]`: o jogo sem janela em x86 32 bits
  (referência) e em PowerPC 32 bits big-endian no qemu (o 360 em ordem de bytes). As duas com a
  aritmética do PS2 em software: diferença entre elas só pode vir da ordem dos bytes.
- `headless/rastreio.c` + `tools/rastreio.py`: na x86 instrumentada (ASan em modo chamada), cada byte
  vindo do disco carrega a origem (arquivo, deslocamento); o mapa diz, por arquivo, que posições o jogo
  lê como número de 2, 4 ou 8 bytes. `headless/inverter.c` aplica o mapa na PowerPC.
- `headless/chamadas.c` + `tools/chamadas.py`: sequência de funções do jogo (e do `rand`) nas duas
  builds; a primeira diferença é onde se separam. `BT3_VIGIAR=<endereço>` acha quem escreve num lugar.
- `tools/memdiff.py`: memória do jogo das duas builds num quadro, ignorando a ordem dos bytes.

Resultado: a PowerPC carrega tudo, entra na luta de demonstração e roda 2.940 quadros; vida, posições,
relógio e gerador batem com a x86 por ~350 quadros de luta.

As três classes de problema:
1. **Dados do disco lidos como estrutura.** Resolvido por inversão na carga. Mas o mapa por posição só
   cobre o que a luta rastreada usou (ex.: só as faixas de lábio das falas ditas; das outras, nada).
   Não generaliza: o produto precisa de conversão por FORMATO (decisão 8).
2. **Campos de bits.** O código conta com o primeiro campo no bit 0 (`task->bits = ~3`). No 360:
   `#pragma bitfield_order(lsb_to_msb)` do compilador do XDK (conferido na documentação). Na F1:
   `tools/bitfields.py` reescreve o C pré-processado para o GCC dar o mesmo layout.
3. **Union e pacotes montados como imagem de memória do PS2.** O jogo grava a etiqueta DMA como
   palavra e relê a metade `qwc` como u16 (`GfxPostSpritePkt`, `OtPrim`): corrigidos com
   `#ifdef PORT_BIG_ENDIAN` trocando a ordem das metades. Os pacotes da GPU têm mais disso (u64 gravado
   como dois u32, campos de 8/16 bits dentro de registradores de 64): é assunto do renderizador (F3).

## 8. A conversão dos dados do jogo será por formato, não por posição (10/10/2026)

O rastreio provou o método e achou os formatos que a luta usa (com as funções que os leem), mas a
cobertura por posição é parcial por natureza. O produto precisa de conversores por formato (modelo,
textura, animação comprimida, colisão do cenário, banco de som, fonte, faixa de lábio...), escritos a
partir das structs da decomp, aplicados na carga do arquivo (ou no instalador). O rastreio continua
servindo de catálogo (que arquivo é lido por que função, com que largura) e de teste: mapa e conversor
têm de concordar em cada posição que o rastreio viu.

## 9. Revisão da F1 e o que mudou na base de comparação (10/10/2026)

Uma revisão independente apontou que as ferramentas aceitavam como "igual" justamente os erros que a
conversão vai produzir. Corrigido antes de seguir:
- PowerPC com `-fsigned-char`; `.mem` gerado (globais no dump); `memdiff` com ponteiro do port pela faixa
  real de cada programa, larguras ambíguas contadas à parte, globais pareadas por símbolo.
- Rastreio sem junção de leituras de bytes (`-aggressive-instcombine-max-scan-instrs=0`; em `-O0` o jogo
  diverge por comportamento indefinido), tamanhos exatos, alinhamento pelo endereço, até 4 leitores por
  posição, conflitos em arquivo à parte.
- A referência x86 de 32 bits bate com a build oficial de 64 bits do upstream nos 4.242 quadros.
- `wsl.sh enviar` põe o clone no mesmo commit; cada build carimba a origem.

Achados novos:
- **Tabelas de dados do executável** (decisão 7 não listava): o `gen_data.py` repassa os tipos do splat
  (`.long` para tudo). O rastreio agora marca esses bytes (`ELF:<objeto>`) e `elfdados.py` reescreve cada
  diretiva com a largura lida. No produto elas chegam little-endian do SLUS do usuário e são mais um
  formato da conversão.
- **Ordem de avaliação de argumentos**: seis funções do golpe de ki liam `chr`/`kind` na mesma chamada que
  os preenche. Clang e GCC escolhem ordens diferentes; corrigido com `#ifdef PORT` (vale para o PC também).
- **Detector de visões** (`BT3_VISOES`, `visoes.py`): grava a largura de cada escrita na memória e acusa
  leitura com outra largura ou posição. Acha a classe 3 sistematicamente (231 combinações na luta de
  demonstração), em vez de um crash por vez.

Estado: a PowerPC roda a luta de demonstração inteira; vida, posições e relógio iguais nos 4.242 quadros;
o gerador aleatório diverge a partir do quadro 2.751 (efeitos visuais) e há um crash no quadro ~3.979
depois das últimas correções. A lista do detector é o que falta resolver da classe 3.

## 10. F1 concluída na luta de demonstração (10/10/2026)

A build PowerPC big-endian (o 360 em ordem de bytes) reproduz a luta de demonstração **idêntica à x86 em
todos os 4.242 quadros**: vida, posições, relógio e gerador aleatório. O que foi preciso, além das
ferramentas das decisões 7 e 9:
- mapa de inversão dos dados do disco (rastreio) e tabelas do executável retipadas (`elfdados.py`);
- campos de bits na ordem do PS2 (`bitfields.py`; no 360, `#pragma bitfield_order`);
- sete correções no código do jogo com `#ifdef PORT_BIG_ENDIAN` (etiquetas DMA lidas por metades, `objId`
  do emissor de quads, bit do chão, `u64` sobre flags de 32 bits, cores do desfoque) e uma com `#ifdef PORT`
  (ordem de avaliação de argumentos no golpe de ki, que vale para qualquer host).

O que a F1 NÃO prova: outras lutas, personagens, cenários e os menus (o mapa por posição só cobre o que
esta luta usou: decisão 8); o caminho do som e o da GPU (a build roda sem janela e sem áudio); e a FPU do
360 (as duas builds usam a aritmética do PS2 em software). Ainda falta: o detector de visões tem resíduos
na pilha a entender, e os pacotes da GPU ficam para a F3.

**Nota (revisão nº 3, decisão 11):** a afirmação valia para as 11 palavras comparadas, não para a memória.
A PPC tinha 10 structs com layout diferente da x86 (`bitfields.py` corrompia structs aninhadas) e o heap
deslocado a partir da HUD; as ferramentas que deviam mostrar isso aceitavam a diferença. Refeita na
decisão 11.

## 11. Revisão nº 3: ferramentas rigorosas e a F1 refeita (10/10/2026)

A terceira revisão independente mostrou que a decisão 10 era prematura (nota nela). Corrigido:

- **Campos de bits:** `bitfields.py` reescreve cada corpo de struct uma vez só (o de dentro vira marcador
  opaco para o de fora) e mantém as linhas do arquivo. `layout.py` compara pelo DWARF o layout de toda
  struct do jogo entre x86 e PPC (offset, tamanho, posição do campo de bits no valor da unidade), e a build
  PPC falha se divergir. O clang só gravava as structs usadas por variáveis: `-fno-eliminate-unused-debug-types`.
  As três structs permutadas de propósito por `#ifdef PORT_BIG_ENDIAN` estão em `layout.PERMUTADAS`, com o
  motivo, e delas se exige permutação (mesmo tamanho, mesmos campos).
- **`memdiff.py`:** heap pareado pelos blocos do alocador do jogo (`HeapBlock`, 'TBHS'), não pelo endereço;
  globais pelo nome com tamanho até o símbolo vizinho (as tabelas em assembly entravam sem tamanho);
  ponteiro do port a partir da base de ligação (os símbolos TLS valem 0); classes "igual sem inverter" e
  "u64 com zero" à parte.
- **Detector de visões:** sombra do valor gravado no lugar da "pilha neutra" (que dependia do endereço da
  pilha e se desligava em ~1 de cada 10 execuções por ASLR); simetria pelo trecho certo; quem gravou vai junto
  nas cópias; leitura mais estreita que a gravação e depois copiada é o erro (`objId`); cópia mais larga só
  leva a forma; sem vetorização na build instrumentada; pares tratados em `visoes_resolvidas.tsv`.
- **Mapa:** conflito nunca inverte, e leitura que começa dentro de um número é conflito. Um conflito de byte se
  resolve quando todas as leituras como byte são campos de structs já permutadas: o rastreio guarda quem leu
  como byte e `tipos.py` diz o campo. Assim se resolveram as 246 posições de flags de polígono do cenário
  (s32 no disco, lidas como byte numa cópia dentro de `BtlCapiBPose`).
- **Código do jogo:** varredura estática de casts achou mais 7 visões (u64 sobre flags e botões, u16 de um
  u32, u8 de um u16). Correção com as macros `PS2_LOW(T, x)` e `PS2_LD64(x)` (`include/types.h`; na build
  de matching expandem para a expressão original, não compilado aqui porque o WSL não tem a toolchain de
  matching; o port as redefine em `port_compat.h`).
- **Ciclo versionado:** `f1.sh` e `compara.py`; carimbo com os arquivos novos e o hash do rastreio usado;
  rastreio com `setarch -R`.
- **Erro no caminho:** casar cópia por pedaço de vetor só pelo valor espalhou origens erradas (zeros
  coincidem): conflitos de 266 para 3.585, e a PPC caiu. Levando só a forma do detector, os falsos foram de
  18 para 326 combinações. Removido.

Estado com tudo no mesmo carimbo: luta PPC idêntica à x86 nos 4.242 quadros; 1.964 structs com o mesmo
layout; os 180 blocos do heap no mesmo lugar e tamanho; mapa com 20 conflitos, nenhum invertido.

Memória no último quadro (cerca de 89 mil palavras "diferentes", todas em blocos do heap):
- pacotes da GPU (blocos de 1 MB em dobro, `gOt`, um de 256 KB com etiquetas GIF em u64 gravado por
  metades): F3;
- dados de modelo e textura que a luta não leu, com u16 sem inverter: é o que a conversão por formato
  cobre;
- um bloco (o 91, 0x12054 bytes) com `ffffffff` na x86 onde a PPC tem um ponteiro: a investigar.

Detector: 18 combinações, todas do mesmo caso real. As operações de vetor do port (`Vec4_Add` em 16 bytes)
gravam a componente w como float sobre campos u16 que vêm depois de um Vec3 no registro dos golpes de ki, e
o jogo os relê como u16. No PS2 o float desnormal vira zero, e se é desnormal depende de qual metade fica em
cima: depende da ordem dos bytes. A luta não esbarra nisso; fica como risco conhecido.

Aberto:
- `tipos.py` (decisão 8) liga 99% das leituras de um modelo de personagem a struct e campo; é a base dos
  conversores por formato.

## 12. Conversores por formato em C, no instalador e na ferramenta de PC (10/10/2026)

- **Onde:** `port/x360/conv/`, em C89 (o mesmo código compila no `cl -TC` do 360 e no PC). O instalador do
  `.xex` (pesquisa 02, seção 8.5) converte os arquivos do usuário uma vez; a ferramenta de PC `conv_tool` faz o
  mesmo e é o teste: converte, mede a cobertura e confere contra o mapa do rastreio.
- **Como:** troca in-place (nada muda de tamanho nem de lugar), todo acesso com limite conferido, cada byte
  marcado (trocar duas vezes é erro), enchimento tem de ser zero (exceto trechos que o jogo comprovadamente
  não lê, nomeados no código). Formato que não bate é erro com mensagem, não chute.
- **Especificação antes do código:** cada formato ganha um documento em `docs/x360/formatos/`, escrito do
  código da decomp e conferido contra o rastreio (`personagem.md`: file[0], file[1] e file[2] do slot do
  lutador).
- **Animações comprimidas (BPE):** os números só existem depois de descomprimir, então a conversão é em
  tempo de jogo, no caminho das animações (`BtlObjAnim_Load` chama `Port_ConvertAnim` logo depois de
  `Bpe_Decode`, em `#ifdef PORT_BIG_ENDIAN`). Recusados: recomprimir no instalador (muda tamanhos e a tabela)
  e guardar descomprimido (muda tamanhos e o caminho de carga). Cada descompressão é um buffer novo: não há
  conversão dupla. O gancho não é em `Bpe_Decode` em geral porque o mesmo decodificador serve a outros
  formatos (`Sprite_Unpack`).
- **Pacotes da GPU (DMA/VIF/GIF):** toda palavra como u32; um valor de 64 bits fica como duas palavras, a
  baixa primeiro. É como o código C lê o fluxo VIF do modelo (`MdlTex_RebaseChain`), e vale também para o
  cabeçalho e o fim dos pacotes de textura, que nenhum código C lê: uma convenção só para a F3 decodificar.
  Campo de struct declarado u64 (TEX0 de `TexEntry`, da costura) se converte como u64.
- **`flagsTop`:** o byte alto das flags do cabeçalho do modelo, lido como `char` por uma struct de visão.
  `BObjModelBytes` ganhou a ordem big-endian em `#ifdef PORT_BIG_ENDIAN` e entrou em `layout.PERMUTADAS`.

Resultado nos quatro arquivos de personagem da luta de demonstração (01473, 01481, 01793, 01801): cobertura
de 100% dos bytes, e as 223.200 posições que o rastreio viu batem com a largura do conversor.

## 13. Tabelas do executável: tipagem estática gerada no build, aplicada na carga (10/10/2026)

As tabelas de dados do executável chegam little-endian do SLUS do usuário (`Port_LoadGameData` as copia para o
esqueleto `port/data` na partida). O tipo de cada posição sai das declarações C da decomp (AST do clang de todos os
`.i`, layout pelo DWARF de uma sonda compilada com as flags da build): `port/x360/tools/elftipos.py`,
projeto e resultados em `docs/x360/formatos/elf.md`.

- **Gerado no build, sem bytes do jogo:** o mapa sai idêntico rodado sobre o esqueleto versionado do upstream e
  sobre as tabelas do usuário (49.629 posições): depende só de rótulos, ponteiros e código.
- **Aplicado na carga:** em `Port_LoadGameData`, depois da cópia e da conferência do hash (que segue sobre os bytes
  originais), cada posição troca de ordem pela largura. Recusado: converter no instalador (seria um segundo formato
  em disco e o hash teria de mudar).
- **Só o vivo:** rótulos que o C do port não alcança (tabelas da Sony, do CRI e da libc usadas só pelo assembly não
  compilado) ficam como vieram; o build deve falhar se um rótulo vivo ficar sem tipo.
- **Contra o rastreio:** 1.490 diretivas concordam, nenhuma discorda, e 494 diretivas vivas que a luta não leu
  sairiam erradas no 360 sem isso.

Validado na F1 (elf.md 6.8): a PPC segue esse caminho (esqueleto, cópia do SLUS, troca pelo mapa estático) e a luta
fica idêntica; nenhum byte copiado fora do mapa difere da x86. `elfdados.py` (retipagem pelo rastreio) saiu da PPC.

Em aberto: o esqueleto no `cl`/`link` do XDK (F2), o microcódigo da VU1 como u32 e o u64 de `TexBltPacket` (F3), o
layout contra o compilador do 360, e declarar no C os poucos rótulos que só o splat tipou.

## 14. A luta de demonstração em big-endian sem o mapa do rastreio (10/10/2026)

A build PowerPC da F1 roda a luta de demonstração **sem nenhuma informação tirada do rastreio**: os 20 arquivos do
disco que ela lê vêm dos conversores por formato (personagem, comuns, cenário, som, tela de carregamento, rostos;
decisão 12), as animações e a folha da tela de carregamento se convertem depois da descompressão, as tabelas do
executável vêm do esqueleto com o mapa estático aplicado na carga (decisão 13), e as visões de outra largura estão
corrigidas no código. Resultado: idêntica à x86 nos 4.242 quadros, 1.964 structs com o mesmo layout, os 180 blocos
do heap no lugar. `conv_tool --disco`: 2.380 arquivos do disco convertidos sem erro, com 100% de cobertura.

O rastreio e o mapa por posição ficam como ferramenta de teste (`F1_COM_MAPA=1` aplica o mapa aos arquivos sem
conversor) e de catálogo para os formatos que faltam.

Falta para o produto: os formatos que a demo não usa (menus, cenas, modos de jogo), os pacotes da GPU (F3), o
áudio (F6), e levar tudo isso ao 360 (F2: o mesmo jogo sem janela no console, com o float do 360).

## 15. Alvo de vídeo: 720p a 30 fps (12/10/2026)

O alvo é **1280x720 a 30 quadros por segundo** nas lutas, o ritmo do jogo original (a lógica anda em passo fixo de
30 por segundo; os menus seguem a 60, como no PS2). 60 fps fica fora por enquanto, nos dois caminhos possíveis:

- **Interpolação** (o renderizador desenha um quadro intermediário, como o protótipo `BT3_INTERP` do port de PC):
  não muda a luta, mas custa um quadro de atraso e o dobro de desenho, e o protótipo só suaviza o que passa pelos
  programas de vértice.
- **Lógica a 60 de verdade:** exigiria reescalar tudo o que o jogo conta em passos (velocidades, timers, animação,
  janelas de golpe, IA); mexe no jogo inteiro e muda a sensação da luta.

Consequência para a F3: o renderizador desenha só a área visível do GS na resolução de saída (cabe na EDRAM de
10 MB), não a memória inteira do GS como o port de PC. Reabrir esta decisão só depois de medir a folga de CPU (F2)
e de GPU (F3).

## 16. Mod Tag Team: depois do jogo base, só a tela dividida de 4 (12/10/2026)

O [Tag Team Mod](https://github.com/tehmufti/Budokai-Tenkaichi-3-Tag-Team-Mod-PCSX2-) (GPL-3.0) põe vários
lutadores por lado ao mesmo tempo, com tela dividida para até 4 jogadores e um online experimental. Ele não roda no
port como está: é um programa em Python ao lado do PCSX2 que lê e escreve a RAM do PS2 pela PINE e engancha o código
MIPS original, que o port não tem.

Decisão do Lucas: portar depois que o BT3 estiver rodando bem no Xbox, reimplementando em C dentro do port, e só a
parte da tela dividida para 4 jogadores locais. O online fica de fora.

Para quando chegar a hora: usar o mod como referência de comportamento e escrever a partir da decomp (traduzir o
código dele faria do port obra derivada sob GPL-3.0); medir o custo de desenhar a cena até 4 vezes contra a meta de
720p a 30 (decisão 15). Até lá, não fechar portas: a camada de entrada do 360 aceita 4 controles.

## 17. F2: a luta de demonstração roda no 360 em 45% do tempo real (12/10/2026)

O jogo sem janela (`port/x360/tools/build_f2.sh`: o código do jogo rebaixado para C89 pelo `c89.py`, compilado pelo
`cl` do XDK, tabelas do executável pelo `coff.py` e pelo `dat360.py`) roda a luta de demonstração inteira no console
e termina bem. Os dados vão do WSL direto ao console: SLUS, DBZP.BIN e os 20 arquivos da luta já convertidos.

Medido no console, sem desenho nem som, com o jogo sozinho numa thread de hardware:

- a luta inteira gasta **0,45 do tempo real**: cerca de 15 ms dos 33,3 ms de um quadro a 30 fps (decisão 15);
- **nenhum quadro da luta** passa de 33,3 ms (pior par de vblanks: cerca de 20 ms). Os 3 que passam são da
  carga, antes da luta (238 ms);
- 103 funções ainda em assembly viraram stubs, e nenhuma foi chamada.

Comparada com a x86, a luta se separa no primeiro quadro de luta, por 1 bit numa posição. É a diferença de float
que a decisão 5 já previa: soma e multiplicação em truncamento na FPU contra o float por software do PS2, e a
divisão. Daí em diante a luta segue outro caminho e termina em 3.182 quadros de luta, contra 4.242, sem valor
absurdo.

Consequências:

- **Desempenho:** o risco 1 do plano (CPU do Xenon) cai muito. Sobra mais da metade do quadro para o desenho e o
  áudio, antes de qualquer otimização (o código ainda é `-Ox` genérico, sem VMX).
- **Comparação exata:** o `hash.txt` do console não serve para pegar erro de ordem de bytes, que já está coberto
  pela F1. Para conferir o 360 quadro a quadro seria preciso um gêmeo no qemu com o mesmo float. Fica para quando
  houver suspeita.
- **Próximo passo:** F3 (instalador e boot até a tela de título).

## 18. Os dados vêm direto da ISO, convertidos na carga, sem instalador (12/10/2026)

O usuário copia a própria ISO (SLUS-21678, 3,02 GB, abaixo do limite de 4 GB do FATX) para a pasta do jogo, e o
jogo lê tudo de dentro dela: o SLUS e o DBZP.BIN para as tabelas do executável (decisão 13), os arquivos soltos de
`DATA/` e as entradas dos três AFS. Cada entrada que tem conversor (`Conv_Arquivo`, decisão 12) é lida inteira e
convertida na hora em que o jogo a abre; as outras (fluxos de som e filmes) passam direto da ISO.

- **Por quê:** os conversores trocam no lugar, sem mudar tamanho nem posição, então converter na carga custa uma
  passada sobre os bytes de cada arquivo aberto. Sem instalador não há etapa de minutos no primeiro uso, nem 3 GB a
  mais no HD, nem as pastas de milhares de entradas que o FATX não aguenta. Decisão do Lucas, entre as duas opções.
- **Onde:** `port/x360/src/iso360.c` (ISO 9660 e AFS) atrás do `fopen` do shim: o código do jogo e do port não
  muda. Arquivo em `mods/` continua valendo, como no PC, no formato do PS2: o que tem conversor se converte ao
  abrir, como a entrada da ISO (e também os dados soltos, sem ISO).
- **Versão:** o hash das tabelas copiadas do SLUS e do DBZP.BIN (`Port_LoadGameData`) já recusa outra versão.
- **Substitui** o instalador do plano (seção 3, passo 2) e a conversão "no instalador" da decisão 12. A
  `conv_tool` de PC fica como teste dos conversores.

Medido no console (12/10/2026): a luta lida da ISO sai idêntica à da F2 quadro a quadro. A carga dos 20 arquivos
gasta cerca de 210 ms lendo da ISO e 500 ms convertendo; só o cenário (`pzs3us1/00369`, 6,7 MB) leva 70 ms de
leitura e 264 ms de conversão. Se a pausa da carga incomodar com imagem, o primeiro passo é um caminho rápido do
conversor na carga (a conferência byte a byte fica na `conv_tool`), antes de pensar num cache em disco.

## 19. Valores de 64 bits para a GPU: a imagem do PS2 em palavras de 32 bits (12/10/2026)

Os pacotes da GPU (etiquetas GIF, registradores do GS em A+D) têm campos de 64 bits e vêm de dois lugares: montados
pelo código do jogo, que grava `u64` nativo (no 360 a metade alta fica primeiro), e prontos nos arquivos do disco,
convertidos palavra a palavra pela decisão 12 (a metade baixa primeiro, como no PS2). O leitor de pacotes
(`port/src/gs/gs_core.c`, `gs_vu1.c`) não sabe de onde cada um veio.

Decisão do Lucas, entre as duas saídas: **tudo o que vai para a GPU fica como a imagem do PS2 em palavras de 32 bits
nativas, a metade baixa primeiro**, na memória principal e na da VU.

- **No código do jogo:** toda gravação de 64 bits num pacote passa por uma macro que, em `PORT_BIG_ENDIAN`, troca as
  metades (a maior parte vem de `GIF_TAG_EX` e das macros de registrador; o resto, por auditoria).
- **No leitor:** um valor de 64 bits se lê como duas palavras, a baixa primeiro (uma regra só, igual para a memória
  principal e a da VU).
- **No disco:** nada muda; a decisão 12 fica como está.
- **Conferência:** na comparação de memória x86 contra PPC (`memdiff.py`), os buffers de pacotes têm de ficar sem
  nenhuma palavra da classe `u64`; cada uma que sobrar é uma gravação esquecida. Depois, as imagens do rasterizador de
  software das duas builds têm de sair idênticas (etapa 4 da F3).
- **Recusado:** trocar os campos da GPU como `u64` nos conversores do disco. Reabriria conversores já validados,
  exigiria que cada um separasse campo de 64 bits de pixel de textura, e deixaria o leitor com duas regras.

**Implementação e conferência (12/10/2026, etapa 4 da F3).** A convenção está em `formatos/gpu.md`.

- **A troca fica na gravação.** A auditoria pelo AST (`port/x360/tools/pacotes64.py`) achou cerca de 2.000 gravações
  de 64 bits em pacotes, a maioria com literal ou número de registrador gravado direto, sem passar por `GIF_TAG_EX`
  nem pelas `GS_SET_*`. Por isso uma macro só, `PS2_PKT64(x)` (`include/types.h`, a do port em
  `port/include/port_compat.h`), no lado direito de cada gravação (1.998 sítios em 50 arquivos, quase todos por
  reescrita automática conferida linha a linha) e das cópias de número para pacote; as macros de valor seguem dando o
  número. `PS2_PKT64_FIELDS` declara os campos de bits das 26 structs XYZF2 com a metade alta primeiro no host
  big-endian. `GfxPostSpritePkt`, `EftSprPkt`, `EftTriVtx` e `EftNVtx` tiveram os bytes e metades de cor e UV
  permutados; o NLOOP do `Font_CloseSpriteTag` virou `PS2_LOW(u16, *(u32 *)tag)`; o `gTexBltPacket` das tabelas do
  executável se troca como dois `u32` (`elftipos.PACOTES_GPU`).
- **No leitor:** `PS2_PKT64` também em `port/src/gs/gs_internal.h` (`Gs_Gif`, `kick` e as versões em shader da VU1,
  `gs_sony.c`), o UNPACK de 8 e 16 bits por palavra, e a memória do GS como os bytes do PS2 (`GS_LE32` em `vram_rw`;
  os uploads já copiavam bytes).
- **Conferência no WSL:** no quadro 3000 dos menus, os buffers de pacotes (heap#24 e #25) e o `gTexBltPacket` sem
  nenhuma palavra `u64`, `u64-com-zero`, `2xu16`, `igual-sem-inverter` ou `diferente` (antes: 8.682 `u64` e 5.584
  `u64-com-zero` em cada buffer); no `gGfx` sobram as quatro palavras de `disp[0..1].dispfb`, número que a biblioteca
  da Sony grava no registrador privilegiado do GS, não pacote. As lutas das demos seguem iguais (19.802 quadros, até
  o vblank 47.604) e a x86 não mudou (mesma seção `.text`, luta da demo igual nos 4.242 quadros).
- **Imagens do rasterizador de software,** do boot até a demo, um quadro a cada 20 (217): cartão, logos, título e
  tela de carregamento da demo idênticos byte a byte entre x86 e PPC. A primeira rodada teve 1 a 9 pixels diferentes
  em 4 quadros do título: era o `fmadd` do GCC na PPC (a x86 não tem FMA), não ordem de bytes; o GS da PPC passou a
  compilar com `-ffp-contract=off`, como o resto do port.
- **Fica para a F5:** os vetores `u8 rgba[4]` dos vértices dos efeitos da luta e os pacotes da VU1 das tabelas do
  executável (`vu1_micro.data`), tipados como `u8[]` (`gpu.md` seção 6).

## 20. O GsBackend D3D9 do 360: um alvo na EDRAM, os outros em texturas (13/10/2026)

Etapa 7 da F3. A gravação do quadro é a do port de PC (`port/src/gs/gs_draw.c`, compilada no 360 sem mudança além da
escolha do backend em `#ifdef PORT_X360`); o backend novo é `port/x360/src/gs/gs_d3d.cpp`, com os shaders em
`gs_d3d.hlsl` (tradução de `gs.vert` e `gs.frag`) compilados pelo `fxc` do XDK no `build_f2.sh`. A mesma build roda
sem janela (padrão) ou desenhando (`BT3_GS=gpu` no `ambiente.txt`).

Padrões que o Lucas deixou e que podem ser revertidos:

- **SCALE = 1.** Um alvo do GS de 1024 x 1024 com profundidade ocupa 832 + 832 dos 2.048 tiles da EDRAM. Escala maior
  exige tiling predicado.
- **Saída 1280 x 720**, a imagem 4:3 (512 x 448 do GS) em 960 x 720 no meio da tela, filtro linear.
- **O desenho na thread do jogo,** com present a cada quadro do GS. A main cria o dispositivo e o solta
  (`ReleaseThreadOwnership`); o backend o pega no init; `GsD3d_Soltar` o devolve no fim do jogo ou numa queda, e a main
  volta a mostrar a cor do fim. Numa carga longa ninguém apresenta (a Guide demora a abrir nesse intervalo).

Escolhas de implementação, dentro desses padrões:

- **Um par de superfícies na EDRAM para todos os alvos.** Cor A8R8G8B8 na base 0, profundidade D24S8 logo depois;
  cada alvo do GS tem uma textura tiled com o conteúdo dele. Trocar de alvo resolve o anterior (só o retângulo sujo,
  múltiplo de 8) e põe o novo de volta com um retângulo desenhado sobre a EDRAM limpa. A profundidade de um alvo que
  sai da EDRAM se perde: basta para o 2D; a luta (F5) vai precisar resolvê-la também, ou medir se as trocas de alvo
  acontecem com profundidade viva.
- **Sem o segundo alvo com o byte de alfa** (`outAux` do `gs.frag`): não cabe junto na EDRAM a 1024 x 1024. O teste
  de alfa do destino (DATE) lê o alfa da própria cor, que guarda `min(A / 0x80, 1)` e chega a 1 exatamente com o bit 7
  ligado; o FBA grava alfa 1 (o GS liga o bit 7). Os efeitos nativos que precisam do byte (contorno, tabela de
  profundidade, cópia dos ids) ficam para a F5, os programas de vértice (VU1) também, e as imagens copiadas direto para
  o quadro (filmes) para a F6: são contados no log e não desenhados.
- **Vértices por `DrawPrimitiveUP`**, sem buffer próprio: o 2D dos menus tem poucos.
- **Texturas decodificadas** em `D3DFMT_LIN_A8B8G8R8` (o u32 do `gs_draw.c` sem troca, etapa 6), soltas só quando a
  GPU terminou com elas (`IsBusy` depois do `InsertFence`). Sem pacote de texturas no 360.
- **Ordem de bytes das texturas:** o `gs_draw.c` decodifica pela `Gs_VramRead`, que já lê a `sVram` (os bytes do PS2)
  como little-endian em qualquer host (`GS_LE32`, decisão 19). O backend de GPU do PC não lê a `sVram` direto; só o
  leitor de pacote de texturas usa `Gs_BlockPtr`, e para um hash de bytes.

- **Vértices por `DrawPrimitiveUP`** em blocos de até 16.380 (448 KB), um quarto do ring buffer secundário. A
  primeira versão usava até 65.532 e travou o console no começo da luta (primeira rodada, 13/10).

Conferência: o backend grava as imagens mostradas (`BT3_SHOT`, `game:\shots\gpu_NNNNN.ppm`, no formato e na numeração
do rasterizador de software) e `port/x360/tools/compara_quadros.py` as mede contra as da etapa 4 no WSL. O teste no
console está pendente (o console estava desligado quando o código ficou pronto).

**Primeira rodada no console (13/10/2026).** Logos, cartão e título na tela; comparados com o rasterizador da etapa 4
(`compara_quadros.py`, pixels com diferença de até 8): cartão 96,7% (PSNR 22), logos 97,5%, título 83,1% (PSNR 25),
carga da demo 98,9%. Três problemas, corrigidos na segunda versão:

- **O texto do cartão não aparecia:** os glifos vêm com z `0x0FFFFFF0` num buffer Z24, z / zmax perto de 16, e o D3D9
  do 360 corta a primitiva (o PC liga `GL_DEPTH_CLAMP`). O shader prende z em 0..1.
- **O título saía macio:** a arte 2D era lida com o filtro linear no canto do pixel do GS, a borda do texel. Desenhada
  texel por pixel, ela agora é lida no ponto, como o rasterizador. Ampliada ou reduzida, segue o filtro do jogo (o GS
  de verdade filtraria; o rasterizador não).
- **O console congelou no começo da luta** (vblank 4433), sem erro e sem a Guide. Causa provável, ainda não
  confirmada no console: os blocos do `DrawPrimitiveUP` passavam do limite do ring buffer (acima). Para o caso de outro travamento, a main vigia a thread do jogo e registra
  onde ela está parada (a etapa do fim do quadro, o quadro do GS, o vblank, o progresso da VU1), e os primeiros quadros
  da luta saem passo a passo no log.
- **Os programas da VU1 ficam desligados no 360** (`BT3_VU1=1` liga): o backend ainda não desenha o que eles produzem,
  e em big-endian o microcódigo das tabelas ainda vem na ordem errada (`formatos/gpu.md` seção 6). No qemu, com o
  rasterizador ligado a partir do quadro 4400, a PPC teve um programa que rodou 4 milhões de instruções sem terminar
  logo no começo da luta (a x86, nenhum): no console isso parece o jogo congelado. Segundo suspeito do travamento.
- **1,8 vez o tempo real nos menus:** as capturas (`BT3_SHOT=20`) custavam cerca de 260 ms cada, três a cada 60
  quadros; descontadas pela conta, o quadro inteiro fica perto dos 16,7 ms (a confirmar na segunda rodada). A captura passou a sair do tiling de uma vez e a ser
  gravada num `WriteFile` só, e o log mede os quadros com captura à parte.

**Segunda rodada no console (13/10/2026): a etapa 7 fecha.** Comparação com o rasterizador (até 8): cartão 99,0%
(PSNR 34,6), logos 97,5%, título 95,9% (PSNR 28,1), carga 99,9% (PSNR 46,2). O texto do cartão aparece. O pior quadro
do título (2200, arte ampliada) é filtrado no console e pixelado na referência: a diferença é da referência, que nunca
filtra. Os menus ficaram em 16,2 ms por quadro sem as capturas (a lentidão era delas). A luta não trava mais logo no
começo, mas esgota a memória: 128 MB de texturas, 24 alvos e 3 MB livres; alvos e texturas recusados, a thread do
jogo parada de 8 a 80 s e, sem memória livre, a Guide sem abrir até o console travar. Escolha do Lucas: fechar a
etapa aqui, parar os testes antes da luta (`BT3_ATE=4400`) e começar a F5 por um teto de memória no backend
(plano, seção 5).

