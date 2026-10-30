# F8: a VU0 emulada é o maior custo de CPU da thread do jogo (estudo, 30/10/2026)

Análise sem console dos dados do amostrador de 29/10 (`build/sessao_amostra/`, fora do git; builds `0757ec25` e `6b09afe1`
do branch `claude/f8-amostra`, lidos com o `port/x360/tools/amostra_perfil.py` de lá).

## O que as amostras pegaram [medido]

A explosão do planeta **não** está nas coletas: a assinatura dela na sessão `qd_planeta` (169 quadros de 74 a 92 ms, "VU1 0
lotes", uns 3.200 desenhos e 11 a 12 mil vértices de CPU) não aparece em nenhum quadro. O que há é luta comum, na
diferença dos despejos d6 a d10 da `coleta2`:

| Origem, % das amostras úteis | lento (83 quadros, 53,4 ms) | normal (3.520 quadros, 29,3 ms) |
|---|---|---|
| esperas (gravação, vblank) | 51,6 | 42,4 |
| **VU0 emulada do port** (`pc_vu0_a`, `pc_vu0_b`, float360) | **33,8 (18,1 ms por quadro)** | **39,8 (11,7 ms por quadro)** |
| jogo traduzido (`src/`) | 6,3 | 4,3 |
| biblioteca | 3,6 | 3,7 |
| GS/VU1 do port | 0,8 | 1,0 |

As mais quentes: `mtx_apply` (`src/port/vu0_b.c`, matriz por vetor; 84% pelo `Mtx_MulVec4` do culling do cenário),
`op_vmaddabc` e `op_vmaddbc` (`src/port/vu0_a.c`, instruções da VU0 interpretadas; o `Mtx_Mul` dos ossos), `Vec4_Add`,
`Vec4_Scale`, `Mtx_Mul`, `Vu0_SinCos`.

## Por quê [medido na desmontagem, custo inferido]

Os floats andam como padrões de bits `uint32_t` (`f_bits`, `Sf_AddBits` com `memcpy`, `port/x360/src/hw/softfloat_ps2_inl.h`).
O Xenon não move dado entre registrador inteiro e de float sem passar pela memória: `mtx_apply` tem 189 instruções para 28
de float e uns 26 pares `stw`/`lfs` no mesmo endereço da pilha (load-hit-store, dezenas de ciclos cada); `op_vmaddabc`, 100
para 8; `Vec4_Add`, 61 para 4.

## O que fazer, se o Lucas aprovar [estimado]

- **A. A biblioteca da VU0 em float nativo no 360**, na mesma ordem das contas (o `Sf_AddBits` do 360 já soma em float
  nativo; o custo é só a ida e volta pela memória). `Port_FastMtxApply`, que no 360 sempre devolve 0, passa a fazer a conta.
  Estimado de 3 a 5 vezes mais rápido nessas funções: uns 8 a 9 ms por quadro na luta normal, 12 a 14 nos de 53 ms. Riscos:
  o `cl` fundir em `fmadds` (conferir na desmontagem que não sai nenhum) e a igualdade com a x86 e a PPC, que os roteiros provam.
- **B. Na explosão** (`EftBurst_DrawModel`, `src/battle/eft_d_b.c`): a projeção é feita por canto de triângulo (1.650 x 3 =
  4.950 por quadro) para 877 vértices distintos; com uma tabela por vértice, os mesmos bits com 82% menos projeções. Com A,
  a explosão cairia de uns 83 para 30 a 40 ms (inferido).

## A coleta que falta

Mesma build `0757ec25` com `BT3_LOG=1`, `BT3_PICOS=40`, `BT3_AMOSTRA=1000` e **`BT3_AMOSTRA_LIMIAR=65`** (só a explosão
cai em "lento"), o Final Flash que destrói o planeta de fato e 5 s de luta depois para sair o despejo; conferir pelas linhas
`picos:` com "VU1 0 lotes, desenhos ~3.2xx".
