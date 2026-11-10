# Auditoria dos padrões do produto (F8, item "Padrões do produto" de 1.1)

Critério (`00-escopo.md`, seção 1.1): sem `environment.txt`, o jogo roda só com os padrões de `Ambiente_Carregar`
(`posix360.c:72`) e dos consumidores; nenhuma chave de teste ou de diagnóstico ligada; log, autotestes, vigia e todo
diagnóstico desligados na versão pública.

## Método

- Inventário regenerado por `port/x360/tools/audita_logs360.py --atualizar`: **189 chaves** lidas pelos fontes que entram na
  build do 360 (eram 173 em `teste/queda-console`; as 16 novas vieram do MSAA, dos lotes, do relógio da gravação, do
  SEMTEX, da receita e do roteiro). O `04-chaves.tsv` voltou a bater com a árvore. Classes: 54 de diagnóstico (o
  `posix360.c:127` devolve NULL para elas sem `BT3_LOG=1`, lista em `diagnostico360.h`), 1 (`BT3_LOG`, só vale com o
  valor `1`, `posix360.c:61`) e 134 de configuração ou teste explícito.
- Para cada uma das 134 mais as 54, li o consumidor: o padrão sem `environment.txt` é "ausente" (NULL) ou o valor local.
  Uma varredura à parte nas 271 chaves de todo o `port/` (inclui rede, GL e setup do PC, que não entram no 360) não achou
  nada além do que está abaixo.
- Só 14 chaves têm padrão na tabela de `Ambiente_Carregar` (`posix360.c:79-99`), todas de produto: `BT3_GS=gpu`, `BT3_VU1=1`,
  `BT3_VU_VERTS`, `BT3_MEM_PISO_MB`, `BT3_MEM_TEX_MB`, `BT3_GS_THREAD=1`, `BT3_GS_DESENHO=1`, `BT3_SOM=1`, `BT3_WIDE=1`,
  `BT3_SOUND_TICKS=1`, `BT3_SETTINGS=""`, `BT3_DATA`, `BT3_SAVES`, `BT3_MENU_ITEM4=0` (Dragon Net Battle escondido).

## O que o padrão liga de teste ou diagnóstico

Nenhuma **chave** tem padrão que ligue teste ou diagnóstico. O que roda por padrão e é de teste são quatro autotestes de boot
do `gs_d3d.cpp`, que não são chaves (não há chave que os desligue) e que decidem modos que o desenho usa:

| O que liga | arquivo:linha (definição; chamada) | O que decide |
|---|---|---|
| Autotestes da profundidade, 3 rodadas: 1 amostra, e as de 2 amostras pelo fragmento 0 e pelo 1 | `gs_d3d.cpp:4167` e `4629` (`prof_autoteste`, `ms_autoteste`); `5079` e `4876-4879` | `sModoZ` (restauro da profundidade: 0 zerada, 1 como cor, 2 oDepth) e `sMsModoZ`; sem modo exato nos dois fragmentos, sem MSAA. Mede também a GPU do resolve e do restauro (`prof_cronometra`, 8 repetições cada), só para o log |
| Autoteste das amostras do MSAA | `gs_d3d.cpp:4693`; `4918` | `sMsMasc[2]` e `sMsMascOk`: qual máscara (0x1111 ou 0x2222) escreve cada fragmento, que o contorno do MSAA usa. A prova da volta crua só roda com `BT3_MSAA_VOLTA=1` (padrão 0); sem ela o autoteste só acha as máscaras |
| Autoteste das nativas | `gs_d3d.cpp:6673`; `6861` | tira de `sNativos` a passada que não bater com a CPU (contorno, tabelas pelos ids, tabelas pela profundidade) |
| Sabotagens de prova (`BT3_MSAA_SABOTA`, `BT3_EDRAM_SABOTA`) | `gs_d3d.cpp:4698`, `3928` | padrão 0, desligadas (certo); só existem para o autoteste ser visto falhando |

`crash.txt` (`queda360.h:59`) é sempre gravado: é produto (a caixa de erro promete o arquivo), não diagnóstico.

## O que está certo

- Log, `output.txt`, vigia (`main360.cpp:444`, `if (gX360Log)`), tempos, picos, estado, medidas de filme e som, captura
  (`BT3_SHOT*`), hash, roteiro, desbloqueio, queda de teste, receita, rastreio e relógio da gravação: ausentes ou 0 sem
  `BT3_LOG=1`; as 54 chaves de diagnóstico nem chegam ao consumidor sem ele.
- Gravações auxiliares fora de saves e configurações são recusadas sem log (`escrita_permitida`, `posix360.c:197`).
- `BT3_MSAA_VOLTA=0`, `BT3_MSAA_2D=0`, `BT3_MSAA_A2C=0`, `BT3_FXAA=0`, `BT3_PS_TRIVIAL=0`, `BT3_TEX_TILED=0`,
  `BT3_ALFA_CONFERE=0`, `BT3_COR_FUNDO` (fundo preto): padrões de produto.
- Ligados por padrão e de produto, com a decisão registrada: `BT3_MSAA=2` (decisão 37), `BT3_OTIM*`, `BT3_AFIADO=1`,
  `BT3_NATIVOS=7`, `BT3_FBA`, `BT3_CORTE`, `BT3_LOTES`, `BT3_LOTES_ENVIO`, `BT3_EDRAM_RESIDENTE`, `BT3_VBLANK_HW`,
  `BT3_VU1_DIRETO`, `BT3_ENVIO_RAPIDO`, `BT3_TEX_ORIGEM`, `BT3_TEX_RAPIDA`, `BT3_UNPACK_PRE`, `BT3_FILME`,
  `BT3_FILME_BLOCO_KB=256`, `BT3_GS_DESENHO_HW=3`.
- Chaves de teste só por consumidor ausente (NULL): `BT3_ROTEIRO`, `BT3_DESBLOQUEIO`, `BT3_QUEDA_TESTE`, `BT3_CRASH_TEST`,
  `BT3_SOM_TESTE`, `BT3_FLUXO_SABOTA`, `BT3_GPU_*`, `BT3_PERFIL*`, `BT3_RELOGIO_*`.
  Os números "padrão" que aparecem junto de chaves de medida (`BT3_MPEG_MEDE_N=600`, `BT3_SOM_TESTE_HZ=440`) só valem com a
  chave-mãe ligada.

## Corrigido nesta auditoria

- Os sete ambientes de brilho e do deserto (`05-ambientes/environment_*.txt`) e o `ambientes/f7-etapa3/comum.txt` usavam
  captura (`BT3_SHOT*`) e `BT3_SOM_DERIVA` sem `BT3_LOG=1`, o que as deixaria ignoradas pelo `posix360.c`. Ganharam
  `BT3_LOG=1`; o `audita_logs360.py` voltou a passar, com os mutantes acusados.

## Decisão do mantenedor (31/10): os autotestes de boot ficam atrás de `BT3_AUTOTESTES`

Implementada no `gs_d3d.cpp`: a chave `BT3_AUTOTESTES` (padrão 0, em `diagnostico360.h`, só vale com `BT3_LOG=1`) liga os três
autotestes (profundidade, amostras do MSAA, nativas). Sem ela valem os valores fixos da proposta abaixo (profundidade como
cor nos dois, máscaras 0x1111 e 0x2222, `sNativos` 7), sem criar as texturas e superfícies que só os testes usam, e o diário
traz uma linha "autotestes pulados" por teste. As sabotagens exigem `BT3_AUTOTESTES=1` (uma linha no diário avisa; sem ela são
ignoradas) e a volta exata do MSAA (`BT3_MSAA_VOLTA=1`) também (sem prova fica desligada, com uma linha). Dois carimbos
("carimbo do init", começo e fim de `d3d_init`) medem na próxima rodada de console quanto o boot ganhou. A tabela acima
descreve o que rodava por padrão antes disso.

## Proposta: os autotestes de boot sem religar o custo

**Custo medido** (logs de 30/10, `build/sessao_msaa2/coleta/*/bt3xbx.log`, oito rodadas, todas com `BT3_LOG=1`): a
thread do jogo fica parada de **3,85 a 4,95 s** até o backend ficar pronto (`voltou a andar depois de 3848 a 4949 ms`; as
duas rodadas com sabotagem, 4,6 s). O vigia acusa a parada aos 3 s sempre na fase "autoteste das amostras do MSAA", a
última antes do "pronto". O log não carimba o tempo de cada fase, então não separo quanto é autoteste e quanto é criação
das superfícies (a RAM e a EDRAM são criadas de qualquer jeito); o custo individual dos autotestes sai de uma rodada com
um carimbo por fase.

**Como ficaria sem eles** (valores fixos e uma chave para religar):

1. `sModoZ = 1` e `sMsModoZ = 1` (restauro da profundidade como cor) em `prof_autoteste` e `msaa_init`, sem os 3 autotestes
   da profundidade nem as medidas de GPU. Foi o modo escolhido em todas as rodadas de 30/10 (linhas "escolhido o modo 1").
2. `sMsMasc = {0x1111, 0x2222}` e `sMsMascOk = 1`, sem `ms_amostras_autoteste` (a prova da volta crua já está desligada).
   O log mostra "a máscara 0x1111 escreve o fragmento 0" no console de teste.
3. `sNativos` fixo em 7 (o autoteste das nativas passou 100% em todos os logs: ids 65536/65536, contorno 64516/64516).
4. Chave `BT3_AUTOTESTES=1` religa os quatro (e escolhe o modo pela medição); entra em `diagnostico360.h`, então só vale
   com `BT3_LOG=1`. Sem ela, o `Diario` registra uma linha "autotestes pulados, modo 1, máscaras 0x1111/0x2222".

**Risco a decidir:** os valores foram medidos num Xbox 360 E (Corona) com kernel 2.0.17150; o autotest é a rede para um
modelo ou GPU em que o restauro como cor ou a máscara do fragmento se comportem diferente (Xenon, Falcon, Jasper), que
ainda não foi testado. Opções: (a) fixar já e testar só nos modelos que o mantenedor tiver; (b) manter o autoteste da profundidade
(o que escolhe entre dois modos exatos) e fixar só as máscaras e as nativas; (c) manter tudo na primeira release e fixar
depois de testar em outro console. O ganho máximo é o tempo de boot acima; o risco é MSAA com imagem errada, sem log
para acusar, num modelo não testado.
