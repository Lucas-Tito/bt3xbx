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
