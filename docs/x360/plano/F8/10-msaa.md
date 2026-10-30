# F8: o MSAA 2x da EDRAM, o custo e o protótipo (30/10/2026)

[Índice do plano](../../plano.md) · [pendências da F8](pendencias.md) · [F5/08-720p](../F5/08-720p.md) (seções 10 a 12)

Branch `claude/f8-msaa` (base `main` `cf6e8aa2`). O pedido: o serrilhado incomoda; hoje há o FXAA de console (ligado por
padrão, uns 0,9 ms) na escala 1,33, e o Lucas quer ver o MSAA 2x lado a lado com a escala 1,4 (preparada noutro branch,
`claude/f8-escala`, que este não toca). Em 23/10 a escolha foi FXAA e nada de MSAA ([08-720p, seção 12](../F5/08-720p.md));
este estudo dá o custo do MSAA para a escolha ser feita com números.

Convenção: **[medido]** (logs do console, a rodada `gc_base1` de 28/10 em `build/sessao_2810/coleta/`), **[lido]** (código,
documentação do XDK 2.0.21256 extraída do `xbox360sdk.chm`) e **[estimado]**.


**Decisão do Lucas (30/10/2026, de manhã):** o MSAA só vai ao console se a escala maior (1,4, `plano/F8/09-escala.md`) com o
FXAA ainda incomodar. **Depois da rodada de 30/10 (seção 8):** o MSAA "ajudou muito" no serrilhado que incomodava; ele quer
**só o MSAA, sem FXAA**, e **não quer ficar na 1,33**. A escala (1,4 ou 1,5) não foi fixada: o MSAA completo tem de servir às
duas. O modo de 60 quadros fica fora por enquanto. Tudo continua atrás de `BT3_MSAA=2`, desligado por padrão, até o console
aprovar. **O MSAA completo** (a volta exata, o plano das vagas, o contorno por amostra, a arte 2D sem MSAA e o alfa para
cobertura desligado) está na seção 9, com a rodada preparada em `build/sessao_msaa2/`.

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

## 7. A rodada de console do protótipo (rodada em 30/10: os números na seção 8)

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

## 8. A rodada de 30/10 e as decisões do Lucas [medido]

Logs em `build/sessao_escala/coleta/{a133,b14,c14s,d15s,bt14,e14,e14s}`, `build/sessao_msaa/coleta/{c_com_fxaa,msaa_b_sem_fxaa}`
e `build/sessao_msaaesc/coleta/{m14,m15}` da cópia principal (fora do git). O roteiro `f5_duelo` com a ISO versão 2, as
mesmas chaves do `ambiente_base1.txt` de 28/10; o `m14` e o `m15` com a build `a5f5bc51` (este branch: o protótipo, a 1,4 e a
`BT3_OTIM_SEMTEX`). Todas terminaram com `roteiro: resultado estado 4 vencedor 0x2 razao 0x1 vida 0 27160` e nenhum `ERRO`.

**A folga da GPU** (linha `picos: tela, janela`; mínimo / p1 / p10 / mediana, ms; e as imagens que ficaram 3 vblanks):

| Abertura | Escala | AA | SEMTEX | Pausa (janela 1) | Luta (janela 2) | 3 vblanks (1 / 2) |
|---|---|---|---|---|---|---|
| `a133` | 1,33 | FXAA | | 4,7 / 5,4 / 6,1 / 8,4 | -9,5 / 2,5 / 6,3 / 8,5 | 0 / 2 |
| `b14` | 1,4 | FXAA | | 2,2 / 3,1 / 3,7 / 6,3 | -0,8 / 2,8 / 4,4 / 7,6 | 0 / 2 |
| `c14s` | 1,4 | FXAA | sim | 5,8 / 6,6 / 7,3 / 9,9 | -8,0 / 3,2 / 7,8 / 10,2 | 0 / 2 |
| `d15s` | 1,5 | FXAA | sim | 2,5 / 3,5 / 4,3 / 7,1 | -0,6 / 2,7 / 4,9 / 7,8 | 0 / 1 |
| `c_com_fxaa` | 1,33 | MSAA + FXAA | | 3,4 / 4,2 / 5,0 / 7,5 | -10,5 / 1,9 / 5,2 / 7,6 | 0 / 3 |
| `msaa_b_sem_fxaa` | 1,33 | MSAA | | 4,1 / 5,0 / 5,7 / 8,3 | -9,7 / 2,6 / 6,0 / 8,5 | 0 / 2 |
| `m14` | 1,4 | MSAA | sim | 4,0 / 4,8 / 5,6 / 8,4 | -9,7 / 2,6 / 6,1 / 8,9 | 0 / 2 |
| `m15` | 1,5 | MSAA | sim | **0,6 / 1,5 / 2,5 / 5,5** | -1,8 / 1,6 / 3,4 / 7,3 | 0 / **4** |

- **A `BT3_OTIM_SEMTEX` ganha uns 2,6 ms na mediana da luta da 1,4** (`c14s` contra `b14`) e não muda a imagem: as capturas da
  `e14` e da `e14s` (1,4 sem e com a chave, `BT3_SHOT=600`) têm os 15 FNV comuns idênticos.
- **O MSAA só, no lugar do FXAA, custa quase o mesmo que o FXAA na 1,33** (`msaa_b_sem_fxaa` contra `a133`: a mediana igual, o
  p10 0,3 ms abaixo); com o FXAA junto, mais 0,9 ms (`c_com_fxaa`).
- **Na 1,4 com a SEMTEX, o MSAA só (`m14`) fica 1,3 ms abaixo do FXAA (`c14s`) na mediana da luta** e ainda acima da `a133`
  (o padrão de hoje): cabe.
- **Na 1,5 com a SEMTEX, o MSAA só (`m15`) aperta**: a pausa fica com o mínimo em 0,6 ms e o p1 em 1,5, e a luta perdeu o prazo
  em 4 imagens (a `d15s`, com FXAA, em 1). É o caso que o plano das vagas (seção 9) tem de aliviar.

**Os contadores do protótipo na luta** (a linha `MSAA 2x, por quadro`, a janela de 2.400 quadros, por quadro):

| | 1,33 (`msaa_b_sem_fxaa`) | 1,4 (`m14`) e 1,5 (`m15`) |
|---|---:|---:|
| Entradas no alvo com MSAA | 5,68 | 5,68 |
| Delas com a volta | 3,43 | 5,68 |
| Despejos de A ou B ao entrar nele / dele ao entrar em A ou B | 4,41 / 2,43 | 6,66 / 4,68 |
| DATE pelo fragmento 0, todos depois de uma volta | 10,74 | 10,74 |
| DATE pela textura (com a média) | 0,53 | 0,53 |
| Cópias dos ids pelo fragmento 0, depois de uma volta | 0,98 | 0,98 |

Na 1,4 e na 1,5 o plano do último quadro é `A alvo 5 (12 entradas); M alvo 2 (7 entradas, 2 amostras); B alvo 3 (8
entradas); B alvo 4 (5 entradas); pequena alvo 6 96x96 (12 tiles, 10 entradas)` e a trilha da pausa, `0M+z ^0m 3B+ ^3l 4B+
^4l 5A+ ^5l 6p+ ... 5A 4B 3B+ ^3l ^5m 0M+ ^0l 3B+ ^3l 0M+Z ^0m ...`: **toda entrada do 3 e do 4 na B despeja o alvo com MSAA,
e cada volta a ele é uma volta inteira**, como a seção 2 previa. **A primeira entrada de cada quadro já é uma volta** (`0M+z`:
o alvo traz o conteúdo do quadro em que foi mostrado pela última vez), então "depois de uma volta" vale para todo DATE e toda
cópia dos ids do quadro: o alfa que eles leem pelo fragmento 0 é o médio nos pixels que a volta trouxe e que nada redesenhou
desde então.

**A 1,5 ocupa a mesma EDRAM que a 1,4** [medido no boot da `m15`]: alvo de 960 x 672, `a cor de 2 amostras na base 0 com 1008
tiles e a profundidade de 2 amostras na base 1008 com 1008`, `as pequenas de 2016 a 2048 (32 tiles)`, como a `m14` com 896 x
672 (12 colunas de tiles de 80 pixels nos dois casos; 84 linhas de 8). O autoteste da profundidade de 2 amostras escolheu o
modo 1 (como cor) pelos dois fragmentos nas duas; a cor de 2 amostras: Resolve 0,179 ms e volta 0,177 ms na 1,4, 0,191 e
0,190 na 1,5 (o alvo inteiro). O mesmo plano de vagas serve às duas escalas.

**O que o Lucas viu:** o MSAA "ajudou muito" no serrilhado que o incomodava. Decisões: só o MSAA, sem FXAA; nada de ficar na
1,33; a escala entre 1,4 e 1,5 fica para depois do MSAA completo (seção 9); o modo de 60 quadros fora por enquanto.

## 9. O MSAA completo (branch `claude/f8-msaa-escala`, 30/10)

Pedido: transformar o protótipo num MSAA 2x completo para a 1,4 e a 1,5, sem FXAA, com a lista da seção 6 ("O que falta").
Tudo continua atrás de `BT3_MSAA=2`, desligado por padrão; cada parte tem uma subchave (lida só com o MSAA ligado), para a
rodada separar o custo de cada uma. Todas as subchaves em 0 dão o protótipo de `a5f5bc51`.

| Commit | Subchave (padrão com o MSAA) | O quê |
|---|---|---|
| `52884f50` | | a rodada de 30/10 (seção 8) |
| `299a79b8` | `BT3_MSAA_VOLTA=1` | a volta exata pelas amostras cruas, o DATE adiado, o autoteste das amostras e a sabotagem dele |
| `2d6d85e8` | `BT3_MSAA_VAGAS=1` | as pequenas sobre a profundidade de 2 amostras; o alvo com MSAA fica na EDRAM o quadro todo |
| `9219245c` | `BT3_MSAA_CONTORNO=1` | o contorno por amostra (a cópia dos ids do fragmento 1 e duas passadas com a máscara) |
| `f1cac186` | `BT3_MSAA_2D=1` | a arte 2D sem MSAA (`D3DRS_MULTISAMPLEANTIALIAS` falso em volta de cada desenho) |
| `d2e7e50a` | `BT3_MSAA_A2C=0` | o alfa para cobertura nos desenhos com teste de alfa, desligado |
| `8c466c42` | | as correções da revisão: a volta em tiles inteiros, a média que passa pelo parque contada, a escolha do parque |

### 9.1 A volta exata (`BT3_MSAA_VOLTA`) [lido; a exatidão, o autoteste do boot]

**O problema** (seção 8): o protótipo trazia o alvo com MSAA de volta pela média das amostras; o DATE e a cópia dos ids, que
leem o fragmento 0, liam o alfa médio nos pixels trazidos pela volta. A primeira entrada de cada quadro já é uma volta.

**A saída escolhida, e por que ela:** a documentação do `D3DRS_MULTISAMPLEMASK` diz que, no MSAA 2x, cada pixel do buffer é
um bloco vertical de 1 x 2 no buffer de amostras, e o `XGSurfaceSize` dá os mesmos tiles para a cor de 2 amostras de `W x H` e
para a de 1 amostra de `W x 2H`. Uma vista de 1 amostra com o dobro das linhas na base 0 (`sMsCru`) lê e escreve as amostras
cruas. Quando o alvo com MSAA perde a EDRAM (despejado no quadro, ou no fim do quadro, `ED_FIM`), o conteúdo dele vai por um
Resolve dessa vista para um parque (`ms_parque`, uma textura `W x 2H`); a volta (`ms_volta`) desenha o parque na mesma vista.
As duas amostras voltam bit a bit, o alfa do fragmento 0 também, e a média do fim continua a mesma (as amostras são as de
antes). O estudo sugeria guardar só o alfa do fragmento 0 e restaurá-lo no alfa; isso exigiria um Resolve do fragmento 0 e
duas passadas na volta (a do fragmento 0 só no alfa e a outra com `2 x média - a0`, para a média do fim não mudar): o mesmo
tráfego da vista crua (o Resolve mede pelos bytes escritos: 0,179 ms para os 2,4 MB da média da 1,4), menos exato (a cor
de cada amostra voltaria média) e com mais estados. A vista crua é exata em tudo.

- **Dois parques** (`MS_PARQUES`): o jogo alterna dois alvos de quadro (0 e 2), e a volta do começo de um quadro traz o alvo
  mostrado dois quadros antes. Um parque vale para o alvo e o retângulo que guardou, com a mesma textura do alvo
  (`sAlvoD[].ver`); um Resolve de cor para a textura desse alvo num quadro em que ele não é o com MSAA tira a validade
  (`ms_resolveu`). Sem parque que valha, a volta é pela média, como antes, e conta como tal.
- **Em tiles inteiros:** o parque e a volta são em retângulos com x em múltiplos de 80 e y em múltiplos de 8 (`alinha80`),
  o caso que o autoteste prova: com o alvo inteiro, a ida e a volta pela mesma vista acertam com qualquer arranjo das
  amostras dentro do tile, mas um retângulo que corta um tile dependeria dele. A volta devolve o retângulo inteiro do
  parque; fora do conteúdo ele tem o que a limpeza da entrada dava. (A revisão por outro agente achou o furo na versão
  com x em múltiplos de 8, `8c466c42`.)
- **A média que passa pelo parque:** uma volta pela média (sem parque, com a textura que recebeu a média de um Resolve do
  alvo com MSAA, `sMsTexMedia`) põe a média nas duas amostras, e o parque guardado depois disso a carrega; o parque leva a
  marca (`media`) e a volta por ele conta como média. Uma volta por uma textura que só recebeu Resolves de 1 amostra é
  exata (as duas amostras com o valor de 1 amostra).
- **Sem Resolve à toa:** o parque só é refeito se o alvo mudou na EDRAM desde o último (`sMsMudou`: algum Resolve de sujo
  dele, ou a volta pela média).
- **O DATE adiado:** o marcador de DATE do alvo com MSAA quando ele não é o posto lia a textura (a média). Com a subchave, a
  cópia espera a entrada nele (`ms_date_adiado`, no fim do `entra_res` e do `entra_na_edram`) e sai do fragmento 0 da EDRAM,
  com as mesmas contas do caminho do alvo posto (`date_area`). Nada desenha nele entre o marcador e a entrada, então é o
  mesmo conteúdo.
- **O autoteste das amostras** (`ms_amostras_autoteste`, no `msaa_init`): dois padrões pseudoaleatórios desenhados com 1
  amostra são a referência; na cor de 2 amostras, cada padrão vai pela máscara de um bit (`0x1111`, `0x2222`: a máscara
  tem 4 bits por pixel da quadra de 2 x 2) e os Resolves pelo fragmento 0 e pelo 1 dizem qual máscara escreve qual
  fragmento (`sMsMasc`). Depois a volta crua: parque, a cor limpa (o controle: o fragmento 0 tem de sair diferente), a volta
  e os dois fragmentos de novo, que têm de ser os de antes em todos os pixels. Sem a volta exata, o MSAA se desliga com
  `ERRO` (o alfa depois de uma volta seria o médio). Mede também a GPU do parque e da volta crua de um alvo inteiro.
- **A sabotagem:** `BT3_MSAA_SABOTA=1` lê a volta uma linha da vista crua abaixo (a amostra de baixo no lugar da de cima):
  o autoteste tem de dizer `ERRO: não exata` e o MSAA se desligar; `BT3_MSAA_SABOTA=2` desenha os dois padrões com a mesma
  máscara: `ERRO: nenhuma máscara escreve um fragmento só`, e o contorno fica por pixel.
- **Os contadores** (`MSAA 2x, por quadro`): `N com a volta, que traz a saída anterior: E exatas, pelo parque, e as outras
  pela média`; `DATE nele N pelo fragmento 0 (V depois de uma volta, M depois de uma pela média; A esperaram a entrada nele)
  e T pela textura`; `cópias dos ids N pelo fragmento 0 (V depois de uma volta, M depois de uma pela média)`. "Pela média"
  conta o quadro em que o conteúdo do alvo com MSAA veio de uma textura com a média (direto ou por um parque marcado). Exato
  é `M` zero e `T` zero. A linha `MSAA 2x, o completo, por quadro` dá os parques e as amostras guardadas; a trilha marca o parque
  (`^0k`) e a volta exata (`0M+x`).

**O custo [estimado]:** o parque escreve o dobro da média (uns 0,36 ms por alvo inteiro na 1,4) e a volta crua lê o dobro
(uns 0,35 contra 0,18). Com as vagas da seção 9.2, sobra uma volta por quadro (a do começo) e um parque (o do fim): uns +0,5
ms por quadro contra a volta pela média. O autoteste mede os dois tempos no boot.

### 9.2 O plano das vagas (`BT3_MSAA_VAGAS`) [lido; os tamanhos, do log da `m14` e da `m15`]

A 1,5 (área 640 x 448, alvo de 960 x 672) ocupa os mesmos 1.008 + 1.008 tiles que a 1,4 (640 x 480, 896 x 672): 12 colunas de
tiles de 80 pixels e 84 linhas de 8 nas duas (seção 8). O plano é o mesmo para as duas.

**A ideia:** a cadeia do brilho (3, 4, 5 e 6) não usa profundidade. Nos quadros com o alvo com MSAA, as pequenas moram sobre a
profundidade de 2 amostras (de 1.008 ao fim da EDRAM, 1.040 tiles) e A fica só para quem precisa da vaga grande (z, nativas,
o filme). O alvo com MSAA não sai mais da EDRAM pela cadeia do brilho; quem sai é a profundidade:

- a pequena que entra com uma profundidade na EDRAM a entrega à textura do dono (`ms_z_sai`, `prof_entrega` com as
  superfícies do dono postas) e a posse acaba;
- a profundidade que volta numa visita com z (limpa ou restaurada) despeja antes as pequenas que moram em cima dela
  (`ms_pequenas_sai`: o sujo delas vai para as texturas; a próxima entrada de cada uma é com a volta).

As duas regras mantêm o invariante: nunca há profundidade e pequena com conteúdo nos mesmos tiles. A e B continuam dentro da
cor de 2 amostras (quem precisa delas despeja o alvo com MSAA, pelo parque).

| Pequenas da luta (tesoura do GS) | 1,4 (tiles) | 1,5 (tiles) |
|---|---:|---:|
| 3 (512 x 448) | 720 x 632: 360 | 768 x 672: 420 |
| 4 (128 x 128) | 184 x 184: 36 | 192 x 192: 36 |
| 5 e 6 (64 x 64) | 96 x 96: 12 cada | 96 x 96: 12 cada |
| Soma, de 1.008 em diante | 420, até 1.428 | 480, até 1.488 |

**A trilha esperada na pausa** (a de 30/10 com o plano novo): `0M+xz ^0z 3p+ ^3l 4p+ ^4l 5p+ ^5l 6p+ ... 3p ^3l 0M ^0l 3p ^3l
0Mz ...`: uma volta do alvo com MSAA por quadro, nenhum `^0m`, a profundidade saindo na primeira pequena depois de cada visita
com z e voltando (`z`) na visita com z seguinte. **O custo [estimado, 1,4, luta]:** saem as 5,7 voltas inteiras do alvo
com MSAA (uns 1,0 ms), os 6 despejos por ela (uns 0,3 a 0,5 ms) e o revezamento do 3 e do 4 na B (uns 0,3 ms); entram uns
2 restauros da profundidade de 2 amostras a mais (0,22 ms cada) e as voltas do 3 depois deles (uns 0,1 ms cada).

**O que fica aproximado:** a profundidade de 2 amostras sai pelo fragmento 0 (o único Resolve de profundidade do Xenos) e
volta com ele nas duas amostras. Antes isso só acontecia na volta do começo do quadro; agora também nas visitas com z depois
da cadeia do brilho. Um desenho com teste de z depois do brilho, num pixel de borda de silhueta, vê na amostra 1 a
profundidade da amostra 0. Se a imagem acusar, o mesmo parque cru serve à profundidade (a profundidade de 2 amostras vista
como cor de 1 amostra com o dobro das linhas), com o dobro do tráfego.

### 9.3 O contorno por amostra (`BT3_MSAA_CONTORNO`)

Na cópia dos ids do alvo com MSAA (`native 4`) sai também a do fragmento 1 (`sIds1`, do tamanho da `sIds`, contada com os
alvos). O contorno (`native 1`) nesse alvo roda duas vezes, cada uma com a máscara de uma amostra (`sMsMasc`, a que o
autoteste mediu) e os ids dela; a máscara volta a `0xFFFFFFFF` logo depois. A linha preta passa a seguir as amostras: na
silhueta, o pixel de borda tem a linha numa amostra e o fundo na outra, e a média do fim a suaviza. Sem a máscara achada,
ou sem a textura, `ERRO` e o contorno por pixel do protótipo. Custo [estimado]: um Resolve dos ids (uns 0,17 ms na 1,4) e
uma passada do contorno a mais (uns 0,1 a 0,2 ms). As passadas de tabela pelos ids (`native 3`, a chave e o brilho de
objeto) continuam por pixel: o mesmo esquema serviria a elas, se a rodada mostrar degrau nelas.

### 9.4 O alfa para cobertura (`BT3_MSAA_A2C`, desligado)

O Xenos soma ao alfa um desvio por pixel da quadra antes de contar as amostras (`alfa + desvio / 8` no 2x; `SOLID` é +1/4,
`DITHERED` pontilha). Com os quatro desvios em 3 (0xFF, +3/8, sem pontilhado): abaixo de 1/8 nenhuma amostra, de 1/8 a 5/8
uma, de 5/8 em diante as duas. Só nos desenhos 3D no alvo com MSAA com teste de alfa GEQUAL de AREF 17 ou mais (ou GREATER de
16), sem mistura e sem FBA: todo pixel que passa no teste cobre ao menos uma amostra. **Desligado porque o risco é alto:** o
alfa gravado no BT3 é dado (os ids do contorno, o bit 7 do DATE) e o opaco do PS2 é 0x80; um texel de alfa 0x40 (meio) num
desenho com teste de alfa e sem mistura, que hoje sai opaco, sai com meia cobertura na peça inteira, não só na borda. Para o
console: uma abertura com a chave, para o Lucas olhar cabelos e recortes, e o contador `desenhos com o alfa para cobertura`.

### 9.5 A arte 2D sem MSAA (`BT3_MSAA_2D`) e o centroide

Em volta de cada desenho de arte 2D (`misc.w`) no alvo com MSAA, `D3DRS_MULTISAMPLEANTIALIAS` falso: as amostras pelo
centro do pixel, a cobertura e a leitura como sem MSAA. Na 1,4 e na 1,5 as bordas das peças caem no meio do pixel; o afiado e
o texto ficam como sem o MSAA. **O centroide no uv não foi feito:** pede uma variante dos shaders do 3D (mudaria os headers
da build sem a chave, ou mais variantes) e só serve se houver franja nas bordas 3D, que a rodada mostra.

### 9.6 O custo do completo [estimado, 1,4 e 1,5, luta, contra o protótipo]

| Parte | ms por quadro |
|---|---:|
| As vagas: sem as voltas e os despejos do alvo com MSAA e sem o revezamento na B | -1,6 a -1,8 |
| As vagas: a profundidade restaurada nas visitas com z e as voltas do 3 | +0,6 a +0,9 |
| A volta exata: o parque do fim e a volta crua do começo | +0,5 |
| O contorno por amostra | +0,3 a +0,4 |
| **Soma** | **-0,6 a +0,2** |

O completo deve custar perto do protótipo (que já ficou 1,3 ms abaixo do FXAA na 1,4 com a SEMTEX), com a volta exata e o
contorno suave pagos pelas vagas. Na 1,5 a pausa (mínimo 0,6 ms no protótipo) segue apertada: as aberturas da seção 9.8
dizem o ganho de cada parte. Se faltar, o próximo passo é a volta do começo do quadro: quando o primeiro desenho do alvo com
MSAA no quadro cobre o conteúdo inteiro sem ler o destino, a volta e o parque dele são trabalho morto.

### 9.7 As provas [medido no XDK; nada rodou no console]

- **Build do 360** (`build_f2.sh` em cada commit; o último, `8c466c42`, carimbo `8c466c42 diff:da39a3ee5e`): 317 de 317 fontes sem erro,
  nenhum aviso no `gs_d3d.cpp` (os avisos dos outros fontes do 360 são os 8 de antes), os mesmos 103 stubs, `default.dat`
  `4fe9d0495c144043fdccda41281655fa` e `default.swap` `2e97a3e533accf0ae852e4d25df5283a` iguais aos de `a5f5bc51`;
  **`default.xex` `42844cae9ec6347d4d2b036eb572ae2e (1.519.616 bytes, 4 KB a mais)`**.
- **Os shaders são os mesmos:** nenhum shader novo; os 27 headers do `fxc` saem byte a byte iguais aos de `a5f5bc51` (o mesmo
  caminho de saída). A volta crua usa o `ps_copia`, o contorno por amostra o `ps_contorno`; o resto é estado.
- **Os objetos contra `a5f5bc51`** (`xdk_objetos.py` do `claude/f5-lotes`, os 317 `obj/*.obj` e os 31 `x360/*.obj`,
  `--espera-diferente x360/gs_d3d.obj`): saída 0, **347 objetos com a mesma desmontagem e só o `gs_d3d.obj` diferente**; no
  `.map`, só as funções novas do `ms_*` e do autoteste. **O controle:** sem `--espera-diferente`, a mesma comparação sai com
  1 apontando o `gs_d3d.obj` e as funções novas do `.map`; o `--mutantes` recusa a instrução trocada, o registrador trocado,
  a instrução a menos, o rótulo falso e a função a mais no `.map`.
- **Dentro do `gs_d3d.obj`** (a desmontagem por blocos, cada rótulo do dump um bloco, com registradores, números e
  deslocamentos normalizados, cada bloco de `a5f5bc51` procurando um igual): 487 de 523 blocos têm um igual (o `gs_d3d.obj` novo tem 554). Os sem par estão nas funções que
  ganharam código atrás da chave (`entra_res`, `entra_na_edram`, `sai_da_edram`, `ed_descarrega`, `ed_plano`, `ed_fim`,
  `ed_init`, `ed_log`, `nat_ids`, `nat_contorno`, `frame_end`, `msaa_init`, `ms_log`) ou que as têm em linha (o `cl` põe as
  funções estáticas pequenas dentro de quem chama: o `nat_tabela` e o `nat_autoteste` levam o `entra_na_edram`); a
  atribuição de um bloco `$M` à função com nome que vem antes dele no dump é aproximada. O comparador (`funcoes_dis.py`, no
  scratchpad da sessão) com uma instrução trocada de propósito acusa o bloco.
- **Sem a chave, o caminho é o mesmo** [lido, o diff de `a5f5bc51` a `8c466c42`]: todo código novo que roda sem `BT3_MSAA`
  é um teste que cai no caminho de antes. `sMsaa` é 0, `sMsAlvo` é -1, `sMsVoltaX`, `sMsVagas`, `sMsContorno`, `sMs2d` e
  `sMsA2c` são 0 (só o `msaa_init` com o MSAA aceito os lê). Um por um: o `ms_resolveu` no `sai_da_edram` e no
  `ed_descarrega` só com `sMsaa`; o `ms_parque` do `sai_da_edram` só com o residente igual a `sMsAlvo` e `sMsAlvo >= 0`, e o
  do `entra_res` e do `ed_fim` só na vaga M, que sem a chave nenhum alvo recebe (o `ed_prox` a pula com `sMsAlvo` -1); o
  `ms_volta` só na vaga M ou com o alvo igual a `sMsAlvo` (`exata` fica 0 e a volta é o mesmo `copia`); o `ms_date_adiado`
  só com `sMsDatePend >= 0`, que só o ramo `sMsVoltaX && d->target == sMsAlvo` do marcador põe (sem a chave o marcador só
  escreve -1 nele); o `ms_z_sai` e o `ms_pequenas_sai` só com `sMsVagasQ`, que o `ed_plano` põe do `msv`, 0 sem a chave
  (`sMsVagas` 0); no `ed_plano`, com `msv` 0 a escolha de A ganha um `(!msv || ...)` sempre verdadeiro e a base das pequenas
  e a conta do `tilesMax` ficam `sEdPool`; no `nat_ids`, o ramo do alvo com MSAA não é tomado e o outro ganha um
  `sMsIds1 = 0` que ninguém lê sem a chave; no `nat_contorno`, `d->target == sMsAlvo` é falso e a passada é a de antes; no
  `frame_end`, o `ms_desenho_poe` só no alvo com MSAA (`msEst` 0, nenhum estado posto); o `entra_res` ganha `exata` 0 e um
  `""` a mais no texto da trilha (a mesma sequência no log), e a legenda da trilha no `ed_log` ganha um `%s` vazio sem a
  chave. Os únicos efeitos sem a chave são esses testes e o `sMsDatePend = -1` de cada marcador.

### 9.8 A rodada de console (preparada, não rodada)

Em `build/sessao_msaa2/` da cópia principal (fora do git): o `default.xex`, o `.dat`, o `.swap` e o `.map` do `8c466c42`, o
`f5_duelo.txt` e o `boot.txt` (iguais aos do repositório), um `ambiente_NOME.txt` por abertura (o `ambiente_m14.txt` de 30/10
com as subchaves e `BT3_SAVES=game:\sv_ms2_NOME`), o `ordem.txt` e o `LEIA-ME.txt`, que diz que linha do log prova cada item.

| Abertura | Escala | Subchaves (VOLTA, VAGAS, CONTORNO, 2D, A2C) | Mede |
|---|---|---|---|
| `c14` | 1,4 | 0, 0, 0, 0, 0 | o controle: o protótipo, tem de bater com a `m14` |
| `t14` | 1,4 | 1, 1, 1, 1, 0 | o completo na 1,4 |
| `t15` | 1,5 | 1, 1, 1, 1, 0 | o completo na 1,5 (a pausa) |
| `v14` | 1,4 | 0, 1, 0, 0, 0 | o ganho do plano das vagas (contra a `c14`) |
| `v15` | 1,5 | 0, 1, 0, 0, 0 | o mesmo na 1,5 (contra a `m15`) |
| `x14` | 1,4 | 0, 1, 1, 1, 0 | o custo da volta exata (a `t14` contra esta) |
| `k14` | 1,4 | 1, 1, 0, 1, 0 | o custo do contorno por amostra (a `t14` contra esta) |
| `a14` | 1,4 | 1, 1, 1, 1, 1 | a imagem do alfa para cobertura |
| `s1` | 1,4 | tudo, `BT3_MSAA_SABOTA=1`, roteiro `boot.txt` | o autoteste acusa a volta crua estragada e o MSAA se desliga |
| `s2` | 1,4 | tudo, `BT3_MSAA_SABOTA=2`, roteiro `boot.txt` | o autoteste acusa as máscaras iguais e o contorno fica por pixel |
