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
