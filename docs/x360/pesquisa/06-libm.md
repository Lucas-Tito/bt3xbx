# A matemática do jogo nas três plataformas: a "libm do host" é a dobra de constantes

Estudo de 29/10/2026 (F7, a parte 2 da tarefa do verificador da transformação). [Lido] é informação tirada do
código, do executável ou da documentação; [Medido] saiu de uma execução ou comparação feita nesta data;
[Estimado] é conta sem medida. Nenhum emulador foi usado: compilações e duas execuções curtas da x86 nativa no WSL,
e leitura dos dumps preservados no Deck.

**Resultado: a diferença que as provas chamam de `libm-host` não vem da libm.** As builds sem janela (x86 e PPC) já
usam a libm do próprio PS2, a newlib 1.10.0 de float compilada com float em software sobre o modelo do PS2, igual nas
duas. O que diverge é a **dobra de contas de float na compilação**: o clang da x86 calcula, ao mais próximo, contas
que só ficam constantes depois de propagar variáveis ou desenrolar laços, e assim pula o `softfloat_ps2.c`; o GCC da
PPC dobra menos, e o executável do PS2 faz essas contas em tempo de execução. A chave nova `BT3_SEM_DOBRA=1`
(`headless.py`, `-frounding-math`) tira essa dobra: com ela a x86 calcula o perfil do brilho igual à PPC, palavra
por palavra. Ela fica desligada até a prova com emulador e a decisão do Lucas (seção 6).

## 1. De onde vêm as funções matemáticas

[Lido] No código do jogo (`src/`, `include/`): `sqrtf` e `atan2f` 129 chamadas cada, `sinf` 90, `cosf` 73, `powf` 33,
`floorf` 28, `asinf` 22, `atanf` 13, `acosf` 11, `tanf` 9, mais `fabsf`. O `port/include/port_libm.h` entra à força
em todo fonte de float em software e troca cada nome por `Port_<nome>`; a `newlib_shim.h` faz o mesmo nos fontes da
newlib (`port/third_party/newlib_libm`), então `Port_sinf` e as outras **são a newlib do PS2**, compilada pelo
`headless.py` (x86 e PPC) e pelo `c89.py` (360) a partir do mesmo fonte, sem mudança.

[Lido] Ficam no host só `ceilf`, `expf`, `logf` e `fmodf` (`port/src/plat_libm.c`), que o jogo não chama (nenhuma
chamada em `src/` nem `include/`), e o `pow` de double do `ReplayMenu_Draw` (`menu_za_d.c`, a ponte
`port/x360/headless/ponte_pow.c`), que só forma dígitos inteiros de 10^i. As funções de double de `port/src/gs` (o som
e a rede) são código do port em float de hardware, fora da simulação.

[Lido] O upstream já provou a newlib contra o jogo original no PCSX2 2.7.303 (`docs/port/README.md`, "How the cause
was found"): `libm_test.c` chama as dez funções originais do executável (`sinf`, `cosf`, `tanf`, `atanf`, `atan2f`,
`asinf`, `acosf`, `sqrtf`, `powf`, `floorf`) em 1.500 entradas; depois do `eeconst.py` (o ee-gcc 2.96 converte as
constantes decimais **por truncamento**), **0 diferenças nas dez**. O mesmo README registra a lacuna que este estudo
mede: "constant arithmetic that only appears after the compiler propagates variables is folded by the host with
rounding to nearest; not measured".

## 2. O que o PS2, a decomp e o PCSX2 dizem sobre o EE e a VU

[Lido] O jogo não usa tabela de hardware para seno, arco ou raiz:
- a newlib de float roda na FPU do EE (polinômios; a única tabela é a dos bits de 2/pi do `kf_rem_pio2.c`, uma
  constante do fonte);
- `Mathf_SinFast`/`Mathf_CosFast` (`src/sys/mathf.c`) são um polinômio ímpar de grau 9 na VU0 em modo macro, com os
  coeficientes `gMathfSinCoef` do executável; o port os refaz operação por operação em `port/src/mathf_pc.c` pelo
  modelo da VU (`RefVu0_*Bits`, `src/port/vu0_a.c`);
- a biblioteca vetorial usa `vsqrt` e `vdiv` da VU0 (`vu0_a.c`, `vu0_b.c`); nenhuma instrução da EFU da VU1 (`esin`,
  `eatan`, `eexp`, `ersqrt`) aparece no jogo nem nos microprogramas que o port interpreta.

[Lido] O modelo do PCSX2 (`port/src/softfloat_ps2.c`, conferido pelo upstream em 1.500 entradas por classe de operação
no PCSX2): soma e subtração da FPU truncadas, com o operando menor perdendo primeiro os bits baixos (diferença de
expoente d de 1 a 24 apaga d - 1 bits; 25 ou mais deixa só o sinal); multiplicação truncada; divisão da FPU **ao mais
próximo** e da VU truncada; raiz da VU truncada e da FPU ao mais próximo; sem denormais nem infinito; estouro e divisão
por zero dão `±0x7F7FFFFF`. A soma do PS2 não é a truncada do IEEE: é o "bit de guarda" que a decisão 5 mediu no 360.

[Lido] O compilador do PS2 dobra contas **truncando** (`1.0f / 30.0f = 0x3D088888`), e o `eeconst.py` reproduz isso
nas contas que a gramática mostra (literais). Onde o ee-gcc dobrou uma conta só visível depois de propagar, o cálculo
em tempo de execução pelo modelo do PS2 pode diferir da dobra dele na divisão (a FPU arredonda ao mais próximo);
não medido, e o ee-gcc 2.96 em `-O2` não desenrola laços.

## 3. O que as provas aceitam hoje e de onde vem

[Lido] Única concessão `libm-host` na memória da luta: os y/z das 20 direções do perfil do brilho
(`EftGlow_BuildTables`, `gEftGlow->dir`, código 0x54 em `memoria_luta.py`, até 64 ULP), e, desde a transformação da
F7, as `dir`/`worldDir` das partes do brilho que copiam esse perfil (`plano/F7/02-pilotos.md`). A classe genérica
`float-proximo` do `memdiff.py` não tem nenhuma região aceita em `CLASSES_CONHECIDAS`. Fora da memória, o mixer de
som (F6) aceita 1 LSB por usar `pow`, `cos` e `sin` de double do host, em float de hardware no código do port.

[Medido] O laço do perfil:

```c
for (i = 6; i < 20; i++) tbl[i] = (f32)(i - 6) * 0.5f / 14.0f + 0.5f;
...
GLOWDIR[i].y = -cosf((yMin + tbl[i] * yRange) * 3.14159265f / 180.0f);
```

| Onde | Contas de float da função |
|---|---|
| executável do PS2 (`0x16DE30`, `llvm-objdump` no WSL, só contagens) | 3 `cvt.s.w`, 4 `div.s`, 7 `mul.s`, 3 `add.s`, 1 `neg.s`: o `tbl` calculado em execução |
| PPC (GCC 13.3, `-msoft-float`) | 2 `__floatsisf`, 3 `__divsf3`, 4 `__mulsf3`, 2 `__addsf3`: em execução |
| x86 (clang 22.1.8, `-msoft-float`) | 0 `__floatsisf`, 1 `__divsf3`, 2 `__mulsf3`, 1 `__addsf3`: o `tbl` dobrado |
| x86 com `BT3_SEM_DOBRA=1` | 22 `__floatsisf`, 22 `__divsf3`, 26 `__mulsf3`, 16 `__addsf3` (laços desenrolados, em execução) |

O `+ 0.5f` truncado do PS2 e o arredondado do clang dão `tbl` diferentes de 1 ULP em 8, 9, 11, 12, 18 e 19; depois de
`cosf`/`sinf` e da normalização, saem diferentes os y/z das amostras 8, 11, 12, 18 e 19, de 1 a 64 ULP (a 12 está no
limite exato da concessão).

[Medido] O perfil inteiro (80 palavras), lido dos dumps no vblank 5260 da transformação:

| Build | Igual à PPC preservada? |
|---|---|
| x86 preservada (Deck, `84cd35b4`) | não: 8 palavras (as aceitas como `libm-host`) |
| x86 do WSL, base (`427efedd`), execução nativa | igual à x86 preservada, as mesmas 8 diferentes da PPC |
| x86 do WSL com `BT3_SEM_DOBRA=1`, execução nativa | **igual à PPC nas 80 palavras** |

## 4. Quanto a dobra mexe nas builds

[Medido] Builds do WSL no commit `427efedd` com a chave nova, cada objeto sem a depuração (`objcopy --strip-debug`,
`cmp`) e, nos diferentes, as funções com código diferente e as chamadas de float em software de cada lado:

| Build | Objetos diferentes | Funções com código diferente | Com outras chamadas de float |
|---|---:|---:|---:|
| x86 (clang) | 165 de 327 | 1.019 | 600 |
| PPC (GCC) | 31 de 343 | 42 | 36 |

[Medido] Na x86 a soma das diferenças: `__mulsf3` +782, `__subsf3` +597, `__floatsisf` +407, `__ltsf2` +334 e
`__gtsf2` -303 (comparação invertida, o mesmo resultado), `__addsf3` -250 (parte é `a + (-b)` virando `a - b`, o mesmo
resultado no modelo), `__divsf3` +155, `Port_sinf`/`Port_cosf` +4 cada (laços desenrolados). Nem toda mudança muda o
resultado: inverter comparação, trocar soma de negativo por subtração e converter inteiro pequeno são exatos. As que
mudam são as dobras inexatas, como a do perfil.

[Medido] Três funções conferidas contra o executável do PS2 (contagens estáticas de instruções de float):

| Função | PS2 | Base | Com a chave |
|---|---|---|---|
| `Quat_Slerp` | 10 add, 20 mul, 1 sub | x86 7/17/4, PPC 7/16/4 | x86 10/20/1, PPC 10/20/1 |
| `EftStorm_InitRain` | 3 add, 3 cvt.s.w | PPC 0 add | PPC 3 add |
| `EftGlow_BuildTables` | acima | x86 dobra | x86 em execução |

As duas builds base dobram contas que o PS2 faz em execução, a x86 muito mais; com a chave as duas ficam com as
contas do executável original nesses casos.

[Medido] `.text` (sha1): x86 base `755ae3036c2e99099b38da8932c8b8d68d50fa63`, x86 com a chave
`62a23f86688706d370d1c7d12cec30c23314bf09`; PPC base `9b563058c7a123dfbb6bba5349a18ec98e12be07`, PPC com a chave
`165c67943780dc5a79b5259f2a4ac62fecbd5092`. Sem a chave os comandos de compilação são os mesmos de antes.

## 5. O 360

[Lido] O 360 compila a mesma newlib com o `cl` do XDK (`c89.py`), `-fp:precise`, `#pragma fp_contract(off)`, e roda o
jogo com a FPU em truncamento (`_controlfp(_RC_CHOP)`, `main360.cpp`, `posix360.c`). Do modelo do PS2 ele difere em
toda conta, não só na libm (decisão 5): a soma sem o bit de guarda (30% das somas a 1 ULP), a divisão truncada contra
a do PS2 ao mais próximo, denormais e estouro. E o `cl` também dobra constantes ao mais próximo (decisão 4). Por isso a
luta do 360 se separa da x86 no primeiro quadro de luta (decisão 17).

[Estimado] Uma libm exata só no 360 (a newlib pelo `softfloat_ps2.c` inteiro, como na x86) não deixa a matemática
igual: o código do jogo em volta continua com a aritmética do Xenon. Custo: a operação em software custa 78 a 143 ns
contra 2,3 ns (decisão 5), e um `sinf` da newlib tem umas 20 a 30 operações, então 2 a 4 µs por chamada; as chamadas
por quadro não foram medidas. Com a F5 a 0,4 ms da meta de 33,37 ms, não cabe agora. **A chave não muda o 360 nem as
imagens dele.**

## 6. O caminho proposto, em etapas

1. **Feito nesta entrega:** a chave `BT3_SEM_DOBRA=1` no `headless.py` (desligada), as medidas acima, e as regras do
   verificador para os dumps atuais (a do brilho continua necessária com a chave desligada).
2. **Armado, espera o emulador** (serviço no WSL, `entrega-f7-verificador-libm.md`): x86, x86r e PPC com a chave,
   referências numa cópia (`REF=1`), `SEMMAPA=1 IMG=1 roteiros.sh f7_transformacao f5_duelo f5_treino duelo` e o
   `save_mc.sh`. Aceite: x86 igual à PPC em tudo, `libm-host` 0 em toda memória, imagens iguais; e a lista das telas
   da x86 com a chave contra as referências atuais, que diz se o comportamento da x86 mudou.
3. **Decisão do Lucas:** ligar a chave por padrão. Custo: a `.text` da x86 e da PPC muda (a referência da `.text` no
   índice do plano também), as referências dos roteiros se regravam se a x86 mudar, e saem as duas concessões da libm
   do `memoria_luta.py` (o perfil e as partes do brilho): a partir daí, uma diferença ali é recusada como qualquer
   outra. Risco: a x86 deixa de ser a referência histórica; ganho: x86 e PPC iguais entre si e às contas do executável
   original em mais lugares, sem tolerância de ULP na memória.
4. **360, opcional e depois da F5:** medir onde o `cl` dobra contas inexatas, compilando com `#pragma fenv_access(on)`
   no prelúdio do `c89.py` (ou `-fp:strict` no `build_f2.sh`) e comparando os objetos com o `xdk_objetos.py --so` do
   branch `codex/f5-lotes-envio`; medir o custo no console. A libm exata no 360 só faz sentido se o 360 um dia adotar a
   aritmética exata do PS2, o que a decisão 5 descartou pelo custo.
