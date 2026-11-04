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

## B feito: a tabela por vértice no EftBurst_DrawModel (30/10/2026, ramo f8-explosao)

Aprovado sozinho (sem A: a biblioteca da VU0 fica como está). Commits `3f48d42f` (o código), `8c0e7678` e `07503778`
(o teste); build do 360 `8c0e7678`: `default.xex` be7c6b7b9f13a0cf78639740f7a05c93, `.dat` 4fe9d0495c144043fdccda41281655fa
e `.swap` 2e97a3e533accf0ae852e4d25df5283a (os dois iguais aos da referência f5-alfa de `build/sessao_padrao3110/`).

**O que mudou.** Em `src/battle/eft_d_b.c`, dentro da tentativa `#if 0` do `EftBurst_DrawModel` (que só o port compila),
um ramo `#ifdef PORT`; o `#else` é a tentativa original, letra por letra (o teste confere contra o arquivo da f5-alfa).
O que um canto calcula do vértice sozinho vai para `EftBurst_PortVertex` e para uma tabela estática de 2.048 vértices
(`sEftBurstVtx`, uns 120 KB), zerada por grupo com um carimbo (`sEftBurstStamp`): `IVec4_Set` e `IVec4_ToFloat12` da
posição, `Vec4_Scale` pela escala do grupo, `Vec4_Sub` da origem, `Mtx_ProjectPointStq` (a posição de tela e o 1.0 * Q)
e, com 0x800 a partir do quadro 11, `sqrtf(Vec3_Dot)`, o `lit`, a cor da luz (`Vec4_Scale`, `Vec4_Add`, `Vec4_Clamp`,
`Vec4_ToInt`), o `len < dist` e o `lightDir * lit`. Por canto ficam a normal (`IVec4_ToFloat12`), o st (`Vec4_Scale` da
normal pelo Q), a cor do canto (`IVec4_ToFloat`) e, perto do centro, a soma com a luz. Índice negativo ou acima de 2.047:
o canto é calculado sem a tabela, `gEftBurstVtxFora` conta e o primeiro desenho que tiver um escreve no diário
"eft_d_b: explosão: N cantos fora da tabela de vértices".

**Por que os bits são os mesmos** [verificado no código e no teste]:
- dentro do laço de triângulos de um grupo nada escreve nos vértices, na matriz da vista, no quadro, em `fade` ou em
  `dist`: só a lista de desenho é escrita, e as chamadas são da biblioteca vetorial e o `sqrtf`;
- das rotinas puladas, só a `IVec4_ToFloat12` mexe no estado da VU0 emulada (vf4 e vf5, sem flags, ACC nem Q); as
  outras são de `vu0_b.c`, que lê só as constantes vf0 a vf3. Todo canto continua terminando com a mesma
  `IVec4_ToFloat12` (a normal) ou `IVec4_ToFloat` (a cor), que reescrevem vf4 e vf5 inteiros: o estado depois de cada
  canto é o do original. O teste compara o `gRefVu0` inteiro no fim;
- o st do original é `(uv.x * Q, uv.y * Q, 1.0 * Q, uv.w)` dentro do `Mtx_ProjectPointStq`. A tabela guarda o `1.0 * Q`
  (projeção com uv zero) e o canto faz `Vec4_Scale(normal, Q)`: a mesma `f_mul` com os mesmos operandos na mesma ordem,
  desde que `1.0 * Q == Q`. No PC o Q vem da divisão do modelo do PS2 (±0, normal ou ±máximo); no 360, da divisão da FPU
  (`float360.c`), que pode dar infinito, denormal ou NaN silencioso, e a multiplicação IEEE por 1,0 devolve o operando.
  Conferido em todos os padrões: 4.261.412.866 no modelo do PC e 4.286.578.690 no do 360 (todos menos NaN sinalizador),
  zero diferentes;
- as contas de float do jogo que foram para `EftBurst_PortVertex` são as mesmas expressões; o `cl` não funde nenhuma
  multiplicação-soma no `battle_eft_d_b.obj`, nem no velho nem no novo (`dumpbin /disasm`: zero `fmadds`/`fnmsubs`;
  48 `fmuls`, 12 `fdivs` e 11 `fsubs` nos dois).

**O teste** (`python3 port/x360/tools/teste_explosao.py [--mutantes] [--identidade] [--tempo]`): liga o arquivo do jogo
duas vezes (velho e novo) com a VU0 do port, em 480 cenas (oito modelos: aleatório, vértices repetidos entre grupos que
dividem o vetor com escalas diferentes, o tamanho do planeta, acima do limite com índices negativos, valores extremos,
w minúsculo, w enorme, matriz aleatória; 0x800 em grupos alternados, em nenhum e em todos; quadros 5, 10, 11, 37 e 90;
quatro distâncias da luz, uma zero), e compara a lista de desenho inteira, as texturas pedidas, o estado da VU0 e o modelo.
Variantes: `x86` (gcc -m32 com as opções de `undefined.py`, float por software) e `x86-360` (float de hardware em
truncamento, a soma e a multiplicação da VU0 pela FPU e a divisão do `float360.c`); `ppc` só onde houver o gcc cruzado
e o qemu (o WSL): não rodada. Resultado: 480 cenas iguais nas duas; os seis mutantes (tabela sem zerar por grupo, Q
errado, cor guardada por vértice, normal convertida depois da cor, limite ignorado, luz sem o `len < dist`) recusados.

**Ganho** [medido no host, estimado no console]. Chamadas do jogo à biblioteca vetorial num desenho do tamanho do planeta
(dois grupos de 1.650 triângulos e 877 vértices, 0x800, quadro 37): `Mtx_ProjectPointStq` 9.900 → 1.748, `Vec4_Sub`,
`Vec3_Dot` e `sqrtf` igual, `IVec4_ToFloat12` e `IVec4_Set` 19.800 → 11.648, `Vec4_Scale` 27.445 → 14.744,
`Vec4_Add`, `Vec4_Clamp` e `Vec4_ToInt` 17.545 → 9.393. No quadro real (4.950 cantos, 877 vértices) são 4.950 → ~877
projeções. Tempo do desenho no host (`--tempo`, 200 desenhos): o novo leva 30% do velho no modelo do PC e 53% no do 360
(o float de hardware da x86, sem as idas e voltas pela memória do Xenon: no console a proporção deve ficar entre as
duas). Quanto isso tira dos 83 ms da explosão depende da parte do `EftBurst_DrawModel` no quadro, que as amostras de
29/10 não pegaram: a sessão `build/sessao_explosao/` (fora do git) mede pelas linhas `picos:` da janela da explosão,
referência contra nova, com o `explosao_picos.py` de lá.

## A coleta que falta

Mesma build `0757ec25` com `BT3_LOG=1`, `BT3_PICOS=40`, `BT3_AMOSTRA=1000` e **`BT3_AMOSTRA_LIMIAR=65`** (só a explosão
cai em "lento"), o Final Flash que destrói o planeta de fato e 5 s de luta depois para sair o despejo; conferir pelas linhas
`picos:` com "VU1 0 lotes, desenhos ~3.2xx".


## A implementado no ramo vu0-nativo: aceite sem console concluído (04/11/2026)

Base `9c69c4ba`, a 1.0.2. A pendência continua aberta para o aceite no XDK e no console.
Não há medida nova de tempo no Xenon nesta rodada.

### Implementação e domínio exato

`port/x360/src/f2/vu0_nativo.c` recebe os operandos por ponteiros e inteiros. Aceita somente ±0 ou
`2^-30 <= |x| <= 2^30`, classificados pelos bits antes de qualquer conta. Denormal, expoente 255,
normal muito pequeno ou muito grande fazem desistir sem publicar a saída. O chamador então executa
o caminho anterior inteiro, inclusive os limites e a aritmética próprios dele. O modelo de hardware
atual do 360 já difere do modelo inteiro nos extremos (decisão 5); esta mudança não redefine esse modelo.

Na faixa aceita, os produtos ficam entre `2^-60` e `2^60`. Depois do truncamento a float, o menor
passo possível é `2^-83`: nem os produtos, nem cancelamentos e as três somas seguintes chegam a denormal
ou estouro. A thread do 360 já usa `_RC_CHOP`. Não se muda o arredondamento dela.

| Função | Caminho novo |
|---|---|
| `mtx_apply` | `Port_FastMtxApply` chama `Vn_MtxApply`: 16 produtos, depois as três somas por componente, na ordem original. |
| `op_vmaddabc`, `op_vmaddbc` | `Vn_Bc4` multiplica quatro componentes pelo escalar e soma ACC, conservando `setFlags`, máscara e publicação originais. |
| `Mtx_Mul` | Conserva a sequência completa do interpretador, inclusive ACC, flags e registradores finais; as instruções acima usam os núcleos novos. |
| `Vec4_Add`, `Vec4_Scale` | `Vn_Op4`/`Vn_Bc4`, com destino temporário; recuo isolado em função sem inlining com o corpo anterior. |
| `Vu0_SinCos` | Conserva o polinômio, os resíduos e a raiz anterior; acelera as instruções `vadd`, `vmul`, `vaddbc`, `vsubbc`, `vmulbc` e `vmulabc` que compartilha. |

Tudo nos fontes upstream está em `PORT_X360`, com o original no `#else`. A build normal x86/PPC continua
como antes. Os produtos ficam em duas funções `noinline` (`Vn_Produtos` e `Vn_ProdutosMatriz`); o chamador
só soma depois da chamada. Essa fronteira impede a fusão. Manter `/fp:precise`, sem `/GL`/LTCG; nos testes,
`-ffp-contract=off -frounding-math -fno-strict-aliasing`. Ainda há temporários float em memória entre as
etapas de produto e soma: não se afirma que sumiram todos os custos de dependência da memória.

### Prova sem console

`python3 port/x360/tools/teste_vu0_nativo.py --casos 2000000 --mutantes`, sob a vaga de emulador autorizada:
compila os arquivos completos da base e os atuais, expõe só as instruções estáticas para o teste e compara
bits de saída e todo o `gRefVu0`. São 2 milhões de casos por função e 16 milhões de instruções por variante,
com todas as máscaras, vf0, ACC e resíduos aleatórios, destinos sobrepostos, bits arbitrários, ±0,
denormais, expoente máximo, estouro e vizinhanças de 1,0 e dos limites da guarda. A matriz aceitou 1 milhão
e recusou 1 milhão em cada variante x86/PPC, modelos inteiro e 360. `Mtx_Mul` e `Vu0_SinCos` são testados como funções completas,
sem reimplementar a sequência no oráculo.

O mesmo comando roda o `port/tools/fastvec_check.c` existente com o núcleo novo, 2 milhões de matrizes por
variante. Nas quatro variantes x86/PPC, modelos inteiro e 360: zero diferenças; desistência 100% em bits arbitrários, 0% perto
de 1,0, 97,8% com extremos misturados e 0% nos casos de poucos bits. Os três mutantes foram vistos falhando
nas quatro variantes: produto ao mais próximo, soma da matriz em outra ordem e denormal aceito pela guarda.
Também foram recusadas as cópias com um bit estragado na saída de cada função/instrução e do fastvec.
A PPC big-endian passou os mesmos verificadores, incluindo estado completo, alias e todos os controles negativos.

Para a prova do jogo, `BT3_TESTE_VU0_NATIVO=1 python3 port/x360/tools/headless.py --alvo ppc` liga a mesma
implementação, conservando a ABI soft-float do jogo: só o objeto dos núcleos usa hardware. A ponte de teste
restaura o modo da FPU após cada núcleo, para não mudar o resto da build. A rodada oficial de
`SEMMAPA=1 roteiros.sh f5_duelo` terminou com “tudo certo”: duas repetições x86, referência, x86r
e PPC com o caminho novo deram o mesmo rastro de 69 linhas; `BT3_HASH` e os sete pontos de memória
foram aprovados, incluindo carga, combate, técnica, pausa e retorno. As telas aqui são transições
e estados; imagens PPM não foram comparadas (`IMG` desligado). Não há roteiro
da explosão do planeta nesta árvore (`port/x360/roteiros`); a reprodução dela fica para o console.

Base x86 reconstruída com o `headless.py` e os dois fontes VU0 de `9c69c4ba`: 398 objetos iguais (326 de código e 72 de dados) após
`objcopy --strip-debug` e `cmp`; só `x360_passos.o`, código de teste com carimbo, difere. SHA1 da `.text`
das duas: `94fe2737c0b9f6663ca659685be77a7cd1d36659`. Uma cópia de objeto com um bit da `.text`
alterado foi recusada por `cmp` (saída 1). A preparação C89 dos dois fontes terminou com zero falhas.

### Resultado oficial e origem das provas

O serviço `bt3-vu0-nativo-oficial` executou `~/vu0-nativo-oficial-2.sh` de 14:27:45 a 14:57:40
(04/11, UTC-3), na segunda vaga autorizada pelo Lucas. Resultado 0, commit de implementação
`2bcd1c05`, árvore limpa e carimbos iguais nas três builds. Logs e resultados fora do Git em
`~/vu0-nativo-final/`; log geral `~/vu0-nativo-oficial.log`.

| Conferência | Resultado |
|---|---|
| x86 inteiro, x86 360, PPC inteiro e PPC 360 | Cada uma: 2 milhões de casos por função, 16 milhões de instruções, estado e alias iguais |
| `fastvec_check`, quatro variantes | Cada uma: 2 milhões de matrizes, zero diferenças |
| Arredondamento, ordem e denormal | 12 execuções recusadas, saída 1 |
| Bit estragado por função/instrução e no fastvec | 52 comparações e quatro fastvec recusados, saída 1 |
| Objetos x86 e `.text` contra a base | 398 objetos iguais sem depuração; só o carimbo de teste difere; `.text` idêntica |
| C89 | Dois fontes preparados, zero falhas |
| `SEMMAPA=1 roteiros.sh f5_duelo` | Tudo certo; referência, repetição, rastro, hashes e sete pontos de memória |

O fechamento altera somente estes registros de documentação. As builds do commit de fechamento
são reconstruídas e os objetos comparados aos da rodada aprovada, exceto `x360_passos.o` (carimbo de
teste). O julgamento das mesmas saídas é repetido com `REUSA=1`, sem apresentar isso como nova execução
do jogo. O relatório `~/entrega-vu0-nativo.md` registra os commits e o resultado dessa conferência.

### Desmontagem GCC PPC antes e depois

GCC `-O2 -fno-stack-protector -ffp-contract=off`, modelo hardware do 360, big-endian. Contagens estáticas
por símbolo, sem expandir laços nem somar funções chamadas. Pares são ocorrências de `stw` e `lfs` no mesmo
deslocamento de r1, não uma análise temporal de dependências. Arquivos `.dis` e contagem saem do verificador.

| Símbolo | Instruções antes | Depois | Pares stw/lfs antes | Depois |
|---|---:|---:|---:|---:|
| `mtx_apply` (inclui recuo) | 85 | 85 | 0 | 0 |
| `op_vmaddabc` (inclui recuo) | 51 | 68 | 0 | 1 |
| `op_vmaddbc` (inclui recuo) | 58 | 72 | 0 | 1 |
| `Ref_Mtx_Mul` (chama instruções) | 204 | 204 | 0 | 0 |
| `Ref_Vec4_Add` | 17 | 42 | 0 | 0 |
| `Ref_Vec4_Scale` | 13 | 47 | 0 | 1 |
| `Ref_Vu0_SinCos` (chama instruções) | 232 | 191 | 5 | 3 |

Núcleos novos, também com zero pares stw/lfs: `Vn_MtxApply` 67 instruções e
`Vn_ProdutosMatriz` 16; `Vn_Op4` 126, `Vn_Bc4` 14 e `Vn_Produtos` 15. Recuos isolados
`vn_vec4_add_lento`/`vn_vec4_scale_lento`: 17/13. Nenhuma fusão nos símbolos contados.
O GCC já conserva muitos floats em registradores no código anterior, ao contrário do `cl` medido no estudo;
esta tabela não demonstra redução do total de instruções nem ganho de tempo. As chamadas e guardas têm custo.

### Aceite que ainda cabe ao Lucas

No XDK: compilar o objeto novo (o `build_f2.sh` já enumera `src/f2/*.c`), conferir `pc_vu0_a`, `pc_vu0_b`,
`float360` e `vu0_nativo` com os dois núcleos de produto fora de linha; zero `fmadds`, `fmadd`, `fmsubs`,
`fnmadds` ou equivalentes nas contas trocadas. Conferir o recuo e os temporários, incluindo pares `stfs/lfs`,
e comparar as contagens com a build anterior. No console: repetir o amostrador em luta comum, quadros lentos,
tela dividida e explosão de fato, e medir a thread do jogo. O ganho estimado do estudo continua sem confirmação.
