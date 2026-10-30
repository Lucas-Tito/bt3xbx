# F8: o MSAA 2x da EDRAM, o custo e o protótipo (30/10/2026)

[Índice do plano](../../plano.md) · [pendências da F8](pendencias.md) · [F5/08-720p](../F5/08-720p.md) (seções 10 a 12)

Branch `claude/f8-msaa` (base `main` `cf6e8aa2`). O pedido: o serrilhado incomoda; hoje há o FXAA de console (ligado por
padrão, uns 0,9 ms) na escala 1,33, e o Lucas quer ver o MSAA 2x lado a lado com a escala 1,4 (preparada noutro branch,
`claude/f8-escala`, que este não toca). Em 23/10 a escolha foi FXAA e nada de MSAA ([08-720p, seção 12](../F5/08-720p.md));
este estudo dá o custo do MSAA para a escolha ser feita com números.

Convenção: **[medido]** (logs do console, a rodada `gc_base1` de 28/10 em `build/sessao_2810/coleta/`), **[lido]** (código,
documentação do XDK 2.0.21256 extraída do `xbox360sdk.chm`) e **[estimado]**.

## 1. Resumo

- **A EDRAM com MSAA 2x:** um tile tem 80 x 8 pixels com 2 amostras (80 x 16 com 1) [lido, `GPU_EDRAM_TILE`]: cada superfície
  dobra. Na 1,33 (alvo de 864 x 608) a cor e a profundidade do alvo principal passam de 418 + 418 para **836 + 836 = 1.672 de
  2.048 tiles**; na 1,4 (896 x 672, seção 2) de 504 + 504 para **1.008 + 1.008 = 2.016**. Cabe sem tiling predicado nas duas,
  mas o que sobra para os outros alvos residentes cai de 794 para 376 tiles (1,33) e de 536 para 32 (1,4).
- **O que deixa de caber vira despejo e volta, não tiling:** na 1,33 o alvo 4 do brilho perde a vaga pequena e as vagas A e B
  passam a dividir os tiles da cor de 2 amostras; uns 2 despejos e 2 voltas do alvo principal a mais por quadro. Na 1,4 só o
  alvo 6 cabe como pequeno: o 3 e o 4 revezam a B e despejam o alvo principal a cada entrada (seção 3).
- **O que quebra:** a média das amostras estraga os ids que o jogo guarda no alfa (contorno, tabelas, DATE). O protótipo lê o
  fragmento 0 na cópia dos ids e no DATE; o que ainda pode sair errado (ids médios depois de uma volta) é contado no log.
- **O que o MSAA suaviza no BT3:** as bordas de triângulo do estágio, das sombras e as internas dos lutadores. **O contorno de
  desenho animado não é geometria** (é uma passada de tela por pixel sobre a cópia dos ids, `native 1`): a linha preta em
  volta dos lutadores continua serrilhada com o MSAA sozinho. Também não suaviza bordas por teste de alfa, texturas nem o HUD.
- **Custo estimado na luta, 1,33:** de 0,8 a 2,2 ms de GPU a mais que hoje sem FXAA, ou seja, de -0,1 a +1,3 ms contra o FXAA
  de hoje; com o FXAA junto, de 1,7 a 3,1 ms a mais. **Na 1,4 com MSAA:** de 2,5 a 4,5 ms acima da própria 1,4. A rodada do
  console (seção 7) mede.
- **Um MSAA 2x completo** (contorno por amostra, ids exatos depois da volta, alfa para cobertura, 2D sem MSAA, plano das vagas):
  **5 a 7 dias** depois do protótipo, com o risco maior em o ganho visível ficar pequeno por causa do contorno.
- **O protótipo:** `BT3_MSAA=2`, desligado por padrão. Só o alvo que o quadro apresenta desenha com 2 amostras; compilado no XDK,
  sem a chave só o `gs_d3d.obj` muda (seção 6).

## 2. A conta da EDRAM [lido; os tamanhos do jogo, medido]

Tiles de 5.120 bytes, 2.048 na EDRAM. Com 1 amostra um tile cobre 80 x 16 pixels; com 2 amostras, 80 x 8; com 4, 40 x 8
(`GPU_EDRAM_TILE_*`, documentação do XDK). Cor A8R8G8B8 e profundidade D24S8, 32 bits por amostra.

**A 1,4:** a área tem de dar inteiro na escala e múltiplo de 32 na saída (`escala.h`, `esc_monta`): com 7/5, a largura e a
altura do GS múltiplas de 160. A menor área que cobre o quadro do jogo (640 x 448) é **640 x 480, alvo de 896 x 672**. Se o
branch da escala escolher outra área, a conta abaixo muda na mesma proporção.

| | 1,33 (área 648 x 456) | 1,4 (área 640 x 480) |
|---|---:|---:|
| Alvo em pixels de saída | 864 x 608 | 896 x 672 |
| Cor ou profundidade, 1 amostra | 11 x 38 = **418** | 12 x 42 = **504** |
| Cor ou profundidade, 2 amostras | 11 x 76 = **836** | 12 x 84 = **1.008** |
| Par principal com 2 amostras | **1.672** | **2.016** |

**Hoje (alvos residentes, `BT3_EDRAM_RESIDENTE=1`, padrão desde 23/10):** A (o alvo que mais entra) na base 0, a profundidade
em T, B (a vaga de troca) em 2T, as pequenas de 3T em diante. Na luta [medido, `gc_base1`, plano do último quadro]: A é o
alvo 5 (64 x 64 do brilho, 12 entradas), B o alvo do quadro com z (0 ou 2, 5 a 7 entradas), pequenas o 3 (a cópia de 512 x
448 que alimenta o brilho, 688 x 600, 342 tiles), o 4 (128 x 128, 33) e o 6 (64 x 64, 12).

| Plano | 1,33 | 1,4 |
|---|---|---|
| Hoje: A + Z + B | 1.254, sobram 794; as pequenas da luta usam 387 | 1.512, sobram 536; as pequenas da luta (360 + 36 + 12) usam 408 |
| MSAA no principal: M + Zm (2 amostras) | 1.672, sobram **376** | 2.016, sobram **32** |
| Quem cabe como pequena (o que mais entra primeiro) | 6 (12) e 3 (342) = 354; **o 4 (33) não** | **só o 6** (12) |
| Quem vai para A e B | A: 5; B: 4 | A: 5; B: 3 e 4, revezando |
| Tudo residente ao mesmo tempo (M + Zm + A + B + pequenas) | 1.672 + 836 + 387 = 2.895: não cabe | 2.016 + 1.008 + 408: não cabe |

**A e B dentro de M:** com o MSAA, A (base 0) e B (base T) moram nos tiles da cor de 2 amostras (que vai de 0 a 2T), e a
profundidade de 1 amostra passa para a base da de 2 amostras (2T): só uma delas vale por vez, a posse já é uma só (`sZDono`).
Entrar em M despeja A e B; entrar em A ou B despeja M. As pequenas ficam depois da profundidade de 2 amostras e não se tocam.

**O que isso custa, em vez de tiling:** o tiling predicado reenviaria a geometria de cada visita ao alvo principal uma vez
por faixa e não conversa com as ~38 trocas de alvo e as passadas nativas do quadro; não vale. O que não cabe custa despejos
(Resolves) e voltas (o restauro da cor):

- **1,33:** na sequência da luta [medido, a trilha da `gc_base1`: `0B+z 3p+ 4p+ 5A+ 6p+ 5A 6p ... 5A 4p 3p 0B 3p 0BZ 3p 4p 5A
  ... 0B 3p 0BZ 0BZ`], entre as visitas ao alvo do quadro só a cadeia do brilho (4 e 5) usa A e B, duas vezes por quadro: **2
  despejos de M e 2 voltas de M a mais por quadro**, e os despejos pequenos de 4 e 5 ao voltar para M. As visitas separadas
  só pela pequena 3 continuam sem volta.
- **1,4:** o 3 entra entre as visitas ao alvo do quadro e não cabe como pequeno: **cada entrada do 3 despeja M e cada volta a M
  é uma volta inteira** (umas 5 por quadro), e o 3 e o 4 trocam a B entre si (cada troca, um Resolve e uma volta do 3, 720 x
  632).

## 3. A estimativa de GPU [estimado a partir do medido]

Medido no boot da 1,33 com 1 amostra (`gc_base1`): o Resolve de um alvo inteiro 0,156 ms, a volta 0,155 ms; a profundidade,
Resolve 0,157 ms, restauro como cor 0,185 ms, pelo oDepth 0,412 ms. O tráfego da EDRAM por quadro na luta, antes dos alvos
residentes: entradas 1,54 a 1,67 ms, voltas 1,78 a 2,22, Resolves de cor 1,22 a 1,54 ([08-720p, seção 11](../F5/08-720p.md));
os residentes economizaram 3,35 ms. O protótipo mede no boot os mesmos números com 2 amostras (seção 7).

O Xenos faz o teste de profundidade e a mistura de 2 amostras na EDRAM sem perder taxa de preenchimento, e o pixel shader roda
uma vez por pixel: o desenho em si custa quase o mesmo. O custo está no tráfego:

| Parcela, por quadro de luta | 1,33 | 1,4 (acima da 1,4 sem MSAA) |
|---|---|---|
| Resolves do alvo principal com 2 amostras (uns 6: leituras pelo brilho, DATE, fonte do DATE, fim) | +0,3 a 0,8 | +0,4 a 0,9 |
| Voltas a mais do principal (despejado por A ou B) | +0,3 a 0,6 (2) | +1,0 a 1,7 (umas 5) |
| B revezando 3 e 4 | 0 | +1,0 a 1,5 |
| Profundidade de 2 amostras (um restauro por quadro, Resolve pelo fragmento 0) | +0,0 a 0,2 | +0,0 a 0,2 |
| Desenho, limpeza e mistura com 2 amostras | +0,2 a 0,6 | +0,1 a 0,2 |
| **Total** | **+0,8 a 2,2** | **+2,5 a 4,5** |

Contra hoje (1,33 com FXAA de 0,9 ms): **MSAA sem FXAA, de -0,1 a +1,3 ms; MSAA com FXAA, de +0,8 a +2,2 a mais que hoje.** A
folga da GPU na luta da 1,33 [medido, `gc_base1`, janela 2]: mediana 8,4 ms, p10 6,3, p1 2,3, mínimo -9,8 (as duas imagens
longas conhecidas). O MSAA na 1,33 cabe na mediana e aperta o p1; a 1,4 sozinha já custa uns 2 a 3 ms (14,6% mais pixels que a
1,33) e com o MSAA passaria da folga no trecho pesado.

## 4. O que o jogo faz com os alvos que complica o MSAA [lido]

O `gs_draw.c` desenha cada efeito como sprites comuns de alvo para alvo (cabeçalho de `GsDraw_TargetGet`: o brilho reduzido
pelo alfa até 64 x 64 e borrado entre dois alvos, o desfoque pela profundidade, o desfoque panorâmico, a névoa) e recusa ler
o próprio alvo. No 360 nenhum alvo é lido da EDRAM: toda leitura é a textura dele depois de um Resolve.

| Uso | Onde | O que o MSAA faz | O protótipo |
|---|---|---|---|
| Alvo lido como textura no mesmo quadro (a cópia 0 para 3, a cadeia 4, 5 e 6, a sombra, os desfoques) | `ed_leitura`, `sai_da_edram` | o Resolve com média entrega a imagem já suavizada: certo, e o brilho sai de bordas suaves | média (`D3DRESOLVE_ALLFRAGMENTS`, o padrão); os alvos de trabalho ficam com 1 amostra (são borrados de qualquer jeito) |
| A volta (o restauro da cor ao reentrar) | `entra_res`, `entra_na_edram` | escreve a média nas 2 amostras: a borda suavizada continua suave, o que se desenha depois volta a ter 2 amostras | igual; contada (`com a volta`) |
| Ids no alfa: cópia dos ids (`native 4`), contorno (`native 1`), tabela pelos ids (`native 3`) | `nat_ids`, `nat_contorno`, `nat_tabela` | a média de dois ids vizinhos é um terceiro id: contorno falso e cor de tabela errada na borda | **a cópia dos ids pelo fragmento 0** (exata); depois de uma volta no mesmo quadro, o alfa da EDRAM já é a média: contado (`depois de uma volta`) |
| DATE (`native 5`, o bit 7 do alfa) | `frame_end`, o marcador | a média de 0xFF com 0 dá um alfa no limite: o teste muda na borda | **o Resolve para o `sDate` pelo fragmento 0**; quando o DATE lê a textura do alvo (fora da EDRAM), lê a média: contado (`pela textura`) |
| Mistura pelo alfa do destino (o desfoque do que está longe) | `ps_gs`, a mistura | por amostra, certo | nada |
| Profundidade reaproveitada (o dono fica na EDRAM entre visitas) e lida (a tabela da névoa) | `prof_entrega`, `prof_poe`, `prof_sincroniza` | o Resolve de profundidade de uma superfície multiamostra lê um fragmento só (o padrão é o 0) [lido]; o restauro tem de escrever as 2 amostras | o Resolve pelo fragmento 0; o restauro pelo modo que o autoteste de 2 amostras escolher, provado nos fragmentos 0 e 1; sem modo exato, sem MSAA |
| Desenho que lê o próprio alvo | `gs_draw.c` | não existe: o `gs_draw.c` recusa | nada |
| Arte 2D e HUD | `ps_gs_af`, os sprites | quads alinhados: as amostras iguais, salvo nas bordas fracionárias da 1,33 (meia cobertura, a borda da peça um pouco mais suave); risco de franja por amostrar fora da peça (sem centroide) | nada; o completo desligaria o MSAA nos desenhos 2D (`D3DRS_MULTISAMPLEANTIALIAS` falso: as amostras iguais pelo centro do pixel) |
| O filme, as passadas de tela | `filme_desenha`, `passada` | por pixel, as duas amostras iguais | nada |
| O FXAA e a cópia para a tela | `fxaa_passa`, `tela_copia` | leem a textura (a média); o FXAA continua valendo | nada |
| As capturas (`BT3_SHOT`) | `captura` | a textura com a média: diferem da base, de propósito | nada |

## 5. O que o MSAA suaviza no BT3 [lido]

- **Suaviza:** as bordas de triângulo: o estágio (`vu4`: prédios, pedras, a linha do horizonte contra o céu), a sombra no chão
  (`vu6`), os lutadores (`vu0`) onde não há troca de id (dobras, braços na frente do corpo, a borda contra uma parte do mesmo
  material), golpes e efeitos feitos de malha.
- **Não suaviza:**
  - **o contorno:** o `ObjOutline_Draw` emite o marcador 1, e o 360 desenha uma passada de tela (`ps_contorno`) que compara o
    id do pixel com os vizinhos na cópia dos ids e escurece o pixel inteiro ([gs-nativos, 3.1](../../formatos/gs-nativos.md)).
    A linha preta dos lutadores é decidida por pixel, com o id do fragmento 0: continua em degraus. A silhueta embaixo dela
    fica suave, mas a linha cobre justamente o anel de fora do lutador;
  - bordas por teste de alfa (o `clip` do `ps_gs`, por pixel): cabelos, folhagem e recortes em textura;
  - texturas e o brilho delas (o MSAA não amostra textura mais vezes);
  - o HUD e o texto (já alinhados ao pixel).
- **O FXAA**, por ser na imagem pronta, pega o contorno, as bordas de alfa e o HUD (por isso amacia o texto). Com os dois
  ligados (a abertura c), o MSAA cuida das bordas de geometria e o FXAA do contorno.

## 6. O protótipo (`BT3_MSAA=2`)

Commit `b798b956`, `port/x360/src/gs/gs_d3d.cpp`, só no backend do 360 (nada portável muda; o `gs_draw.c` é o mesmo).

- **No boot** (`msaa_base`, `msaa_init`): a conta da EDRAM pelo `XGSurfaceSize` com `D3DMULTISAMPLE_2_SAMPLES`; sem caber, a
  chave se recusa com o motivo. Cabendo, a cor de 2 amostras na base 0 (`sMsCor`), a profundidade de 2 amostras logo depois
  (`sMsProf`, e a de 1 amostra na mesma base) e a vista dela como cor (`sMsProfCor`). O autoteste da profundidade
  (`prof_autoteste`) roda de novo nas superfícies de 2 amostras, duas vezes: com os Resolves da profundidade pelo fragmento 0 e
  pelo 1 (as duas amostras têm de voltar certas). O restauro fica como cor se for exato nas duas, pelo oDepth se ele passar nas
  duas, e sem modo exato a chave se recusa. As linhas dele dão os tempos de Resolve e de volta com 2 amostras.
- **Por quadro:** o alvo com MSAA é o que o quadro apresenta (o do `fbp`); sem `fbp`, ou sem entrada nele, o quadro todo fica
  sem MSAA (contado). Com os alvos residentes ele vai para a vaga M (o plano mostra `M alvo N`, a trilha `0M+z`); sem eles,
  o caminho de um par só troca a superfície na entrada dele. O Resolve de saída faz a média; a cópia dos ids e o DATE leem o
  fragmento 0; a profundidade sai pelo fragmento 0 e, quando o dono é de outro tipo que a superfície posta, o Resolve põe a do
  dono antes (contado).
- **O log**, por trecho de 60 quadros (`gs_d3d:   MSAA 2x, por quadro: ...`): as entradas no alvo com MSAA e quantas com volta,
  os despejos de A ou B por ele e dele por A ou B, o DATE e a cópia dos ids pelo fragmento 0 (e quantos depois de uma volta), o
  DATE pela textura, os restauros da profundidade dele e o modo, os Resolves com a superfície trocada, os quadros sem o alvo
  com MSAA e o alvo do último quadro.
- **Sem a chave nada muda:** nenhum objeto criado, `sMsAlvo` fica -1 e todo teste novo cai no caminho de antes; a profundidade
  e a vista dela como cor postas (`sProfPosta`, `sProfCorPosta`) são as de sempre, o fragmento da profundidade é 0, os laços
  pelas vagas visitam as mesmas vagas e as contas do log dão os mesmos números.

### As provas [medido]

- **Build do 360** (`build_f2.sh` no `b798b956`, carimbo `b798b956 diff:da39a3ee5e`): 317 de 317 fontes sem erro, nenhum aviso
  no `gs_d3d.cpp` (igual à main), os mesmos 103 stubs, `default.dat` `4fe9d0495c144043fdccda41281655fa` e `default.swap`
  `2e97a3e533accf0ae852e4d25df5283a` iguais aos da main; **`default.xex` `4e90f1289016afc343746a57fcfa0a31`**.
- **Os objetos contra a main `cf6e8aa2`** (`xdk_objetos.py` do `claude/f5-lotes`, `--so` com os 348 `obj/*.obj` e `x360/*.obj`,
  `--espera-diferente x360/gs_d3d.obj`): saída 0, **347 objetos com a mesma desmontagem e só o `gs_d3d.obj` diferente**, como
  esperado (a prova de que o verificador enxerga uma diferença real); no `.map`, só as quatro funções novas (`msaa_base`,
  `msaa_init`, `ms_autoteste`, `ms_log`). Dentro do `gs_d3d.obj`, com registradores e deslocamentos normalizados, 229 de 261
  funções iguais; as outras são as que ganharam código atrás da chave (a profundidade posta, os laços pelas vagas, as entradas,
  o plano, o DATE, a cópia dos ids, o init e o log) e as que só mudaram de deslocamento pelas variáveis novas.
- **Não rodado no console.** A conta da EDRAM, o autoteste de 2 amostras e a imagem são da rodada da seção 7.

### O que falta para um MSAA 2x completo [estimado]

| Item | Por quê | Dias |
|---|---|---:|
| O contorno por amostra | a linha preta é o serrilhado mais visível dos lutadores: duas cópias dos ids (fragmentos 0 e 1) e a passada do contorno duas vezes com `D3DRS_MULTISAMPLEMASK` em cada amostra; uns 0,3 ms | 1 |
| Os ids exatos depois da volta | se o log mostrar cópias dos ids ou DATE depois de uma volta e a imagem acusar: guardar o alfa do fragmento 0 numa cópia e restaurá-lo só no alfa | 0,5 a 1 |
| O plano das vagas com M | na 1,33, dar ao 4 uma vaga pequena (a 3 do tamanho exato) e não despejar M na cadeia do brilho; na 1,4 não há lugar: aceitar ou ficar na 1,33 | 0,5 a 1 |
| Alfa para cobertura nos desenhos com teste de alfa | `D3DRS_ALPHATOMASKENABLE` com os desvios fixos (`D3DALPHATOMASK_SOLID`) só nos desenhos com ATST; risco de pontilhado | 0,5 a 1 |
| O 2D sem MSAA e o centroide | `D3DRS_MULTISAMPLEANTIALIAS` falso nos desenhos 2D (o afiado continua igual) e `centroid` no uv se aparecer franja | 0,5 |
| As provas | as capturas na escala contra a base, a prova da chave desligada, os verificadores vistos falhando | 1 |
| A rodada de console e o ajuste | duas rodadas | 1 |
| **Total** | | **5 a 7** |

**Riscos:** o ganho visível pequeno nos lutadores enquanto o contorno for por pixel; a 1,4 sem lugar na EDRAM para os residentes;
o restauro da profundidade de 2 amostras como cor (o autoteste decide; pelo oDepth ele é inexato no byte baixo e custa o dobro);
o alfa médio nas bordas; o trecho pesado da pausa, onde a folga é de uns 2 ms; e o modelo de amostras do Xenos na arte
2D (meia cobertura nas bordas da 1,33).

## 7. A rodada de console (preparada, não rodada)

Em `build/sessao_msaa/` da cópia principal (fora do git): `build/` com o `default.xex` do `b798b956` e o `.dat` e o `.swap`
(iguais aos da main), os roteiros `f5_duelo.txt` e `boot.txt` (iguais aos do repositório) e os ambientes, com o
`LEIA-ME.txt`. Cada ambiente parte de `build/sessao_2810/gpu/ambiente_base1.txt` (roteiro `f5_duelo`, `BT3_PICOS=36`,
`BT3_PICOS_TELA=1`, `BT3_TEMPOS=0`; todas as chaves conferidas na main), com save próprio e `BT3_ISO=game:\bt3-x360.iso.v2`
(a main aceita a conversão versão 2; a `bt3-x360.iso` do console é a versão 3):

| Abertura | Ambiente | O que muda |
|---|---|---|
| a | `ambiente_msaa_a_base.txt` | nada: 1,33 com FXAA (a referência) |
| b | `ambiente_msaa_b_sem_fxaa.txt` | `BT3_MSAA=2`, `BT3_FXAA=0` |
| c | `ambiente_msaa_c_com_fxaa.txt` | `BT3_MSAA=2`, `BT3_FXAA=1` |
| d, e (opcionais) | `ambiente_msaa_d_base_tempos.txt`, `ambiente_msaa_e_tempos.txt` | a e b com `BT3_TEMPOS=1`: a GPU por classe (com o custo das marcas) |

**No boot da b e da c**, o log tem de ter: `BT3_MSAA=2: o MSAA 2x no alvo que o quadro apresenta ... a cor de 2 amostras na base
0 com 836 tiles e a profundidade de 2 amostras na base 836 com 836`, as duas rodadas do autoteste de 2 amostras (o controle
perto de 0, o modo escolhido em cada fragmento, os tempos de Resolve e de volta com 2 amostras) e `EDRAM residente com
BT3_MSAA=2: M ... as pequenas de 1672 a 2048 (376 tiles)`. Um `ERRO` de `BT3_MSAA` ali quer dizer que o protótipo se desligou.

**A GPU no log:** a linha `picos: tela, janela: ... folga até o prazo ..., da GPU acabando o quadro: mínimo, p1, p10, mediana`
de cada janela (a 1 é a pausa, a 2 o combate). O custo do MSAA é quanto a folga cai da a para a b e para a c (a mediana e o p10
dizem o custo típico; o p1 e o mínimo, o trecho pesado). Junto: `imagens de fato por vblanks na tela ... 3 ...` (as imagens que
perderam o prazo; na a, 2 no combate) e as linhas `picos: quadro N` acima de 36 ms. Nas opcionais d e e, a linha `tempos, GPU
por classe` separa `resolves de cor`, `volta da cor`, `entradas na EDRAM`, `profundidade`, `comum` e os `vu*`. Por trecho, a
linha `MSAA 2x, por quadro` diz quantas entradas, voltas e despejos o alvo com MSAA teve e se o DATE ou a cópia dos ids
aconteceram depois de uma volta; o plano e a trilha da EDRAM residente mostram a vaga M.

**Na tela** (o Lucas, de perto, na luta do roteiro e na pausa): as bordas do estágio contra o céu e entre prédios e pedras, a
sombra no chão e as bordas internas dos lutadores (braços, pernas) devem sair lisas na b; **o contorno preto dos lutadores
continua serrilhado na b** (seção 5) e fica liso só na c; a c deve ser a mais lisa. Procurar defeitos: franjas ou pontos de
outra cor nas bordas dos lutadores (os ids médios, seção 4), um halo diferente no brilho, a barra do HUD cortada diferente (o
DATE), o texto e os ícones (devem ficar iguais à a sem o amaciamento do FXAA na b).
