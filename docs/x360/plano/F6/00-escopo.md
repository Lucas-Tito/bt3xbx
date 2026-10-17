# F6: escopo, redes de cobertura e critério

[Índice do plano](../../plano.md) | anterior: [F5](../F5/00-escopo.md)

Estudo de 17/10/2026, sobre a base `4ffaaaa`. **Plano proposto, sem implementação ou execução nesta tarefa.** O WSL
estava ocupado com as conferências da F5, então o estudo só leu documentos e código e rodou uma leitura de
cabeçalhos do disco (poucos segundos). As etapas abaixo estão por fazer. As decisões numeradas da seção 5 são
perguntas para o Lucas, não decisões novas do projeto. Antes de implementar, registrar as respostas em `decisoes.md`
(a próxima é a 25) e fechar o roteiro e os limites de aceite.

## 1. Escopo e ponto de partida

A entrega é o jogo com som no console, em tudo o que o PS2 toca: música, efeitos dos bancos, vozes (luta, menus,
guia, locutor) e os dois filmes. Quer dizer:

- **Música e fluxos de voz** (ADX da CRI): seis jogadores (`ADX_CH_BGM`, `MAP`, `SE0`, `SE1`, `VOICE0`, `VOICE1`,
  `include/sys/adx.h`), com volume em 0,1 dB, pan, pausa, laço e o estado que o jogo consulta (`ADXT_GetStat`).
- **Efeitos dos bancos** (ADPCM do chip de som do PS2): 361 bancos, o sampler de 48 vozes de `port/src/gs/snd_se.c`
  no lugar do `SOUNDS.IRX`.
- **Filmes**: só dois, a abertura (`ZS3USOP.PSS`, 2.810 imagens) e o final (`ZS3USED.PSS`, 4.200), cada um com o
  ADX próprio. Os "filmes" que a documentação dos menus cita (`Flash_Create`, `MFLASH`) são as animações Flash dos
  menus, já da F4; não entram aqui.
- **As duas dependências que a F5 deixou** (`plano/F4/07.md`, `plano/F4/10.md`): a boca de quem fala e a tela de
  resultado da História. A seção 1.1 mostra, pela leitura do código, que as duas têm uma causa comum e barata.

Fora do escopo proposto: som de `songs/` e `mods/` (o 360 recusa `mods/`, decisão 24, resposta 3), voz alternativa
da partida em rede (`Port_NetVoiceAlt`, sem rede no 360), o painel de volume do PC (`gPortMusicPercent`,
`gPortSePercent`; o 360 usa só o volume do save do jogo), saída 5.1 (estéreo e mono, como o jogo), 16:9 e
remapeamento (F8), conteúdo completo (F7). O que o jogo não toca no PS2 (efeitos de ambiente que o PC acrescentou,
se houver) não se acrescenta.

### 1.1 Achado da leitura: o 360 hoje não tem estado de som, e o jogo espera por ele

O jogo não pergunta só "o som tocou?". Ele **espera o estado do player** (`ADXT_GetStat`: 0 parado, 3 tocando, 5
fim do fluxo), e isso afeta a lógica e as imagens mesmo sem ouvir nada:

- `LipSync_Update` (`src/battle/view_b_c.c:303`, a espera em :313) só começa a contar as chaves da boca quando
  `Voice_GetStat(0) == 3`; sem isso `FlashAnim_Talk` nunca abre a boca de quem fala (Videl no menu, Dende nas
  Opções).
- `src/menu/` tem 76 ocorrências de `Voice_GetStat(0)` ou `StreamSe_GetStat(0)` (contagem do `rg`), a maioria esperando
  `== 5` ou animando a boca enquanto `!= 5` (`MVOICE_IDLE`, `UB_VOICE_IDLE`,
  `SOLO_VOICE_IDLE`, `G_WAIT` da História em `menu_c_b.c:52`, os `DcList`, o Torneio em `menu_j_b.c`, o resultado
  das simulações em `menu_r_c.c`...), em geral com "confirmar" como saída alternativa. Também
  `BtlScriptCmd_*` e `Voice_IsStopped` em `src/battle/btl_script*.c`, `btl_char_get.c:409` e `Adx_StopAll`
  (`src/sys/adx.c`, laço até os seis estarem em 0).
- `ADXT_GetStat` (`snd_adx.c`) devolve o estado contado em vblanks (`gPortAdxView`, `plat_sndstate.c`) só quando
  `by_ticks()` é verdadeiro, e `by_ticks()` lê `BT3_SYNCTEST`/`BT3_SOUND_TICKS` do ambiente. **No 360 `getenv`
  devolve sempre NULL** (`port/x360/src/ambiente.c`), então `by_ticks()` é falso; sem dispositivo, `start()` volta
  antes de criar o fluxo e o player fica em 0 para sempre: nunca 3 e nunca 5.
- O WSL e a PPC dos roteiros rodam com `BT3_SOUND_TICKS=1` (`port/x360/tools/menus.sh`, `save_mc.sh` e os outros), e
  por isso, pelo que a leitura indica, as referências das imagens têm a boca em movimento e as telas que esperam
  a voz avançam. O console não tinha nada disso.

Leitura mais forte do que a hipótese do `F4/10.md`, achado 3 ("a fala do vencedor em tempo real"): no console o
player nunca chega a 5, então a tela que espera o fim da fala não avança sozinha, e a boca fica fora de fase pelo
mesmo motivo. **É hipótese de leitura, não medida**: a etapa 1 a prova ou derruba no console com um log do
estado de cada player na tela que parou. Se confirmada, a correção é de uma linha em `PORT_X360` (contar o estado
em vblanks no 360), a mesma regra que a PPC e a x86 já seguem nos roteiros, e **pode ser feita antes do resto da
F6**, até dentro da F5 (etapa 9, boca documentada), sem tocar em XAudio2.

### 1.2 O que já existe e o que não

| Base | O que se sabe | Consequência para a F6 |
|---|---|---|
| `snd_adx.c`, `snd_se.c` (PC) | Decodificam tudo por software (ADX de 4 bits em quadros de 18 bytes; ADPCM do chip em blocos de 16) e entregam PCM de 16 bits a fluxos da SDL3 por um retorno de chamada. Nenhum dos dois usa a plataforma além de 14 funções da SDL | O 360 só precisa de uma camada que ofereça essas 14 funções sobre o XAudio2. O decodificador e o mixer vêm prontos e já compilam no 360 (`port/x360/tools/c89.py`, `GS_SEM_JANELA`) contra o `headless/SDL3/SDL.h`, que nunca abre dispositivo |
| F1, `som-e-sprites.md` seção 1, guia seção 6 | Bancos convertidos pela saída A (7 números por banco); `bank32` e `queue32`/`queue16` em ordem nativa sob `PORT_BIG_ENDIAN` | A ordem de bytes do som está resolvida e provada na PPC pela fila de comandos (33 filas e 34 comandos iguais). Não refazer |
| `iso360.c`, decisão 18 | Fluxos de som e filmes passam direto da ISO, lidos a cada `fread`, com um handle e uma trava para todas as threads | Cada início de música ou voz custa leituras reais na ISO. A leitura precisa sair da thread do jogo |
| Disco, medido nesta tarefa (só cabeçalhos) | 65.201 ADX na partição 2, 1,12 GB: 65.093 mono e 108 estéreo, 24 kHz (um de 16 kHz). Mediana 6.088 bytes, p90 47.362, p99 93.352. 20 arquivos de 3,3 a 4,6 MB (índices 64974 a 64993, até 168,6 s), todos estéreo | Quase tudo é voz curta (uma leitura pequena e aleatória); a música é rara e grande. As duas realidades pedem tratamentos diferentes (decisão 4) |
| `snd_adx.c`, `start()` | Lê o **arquivo inteiro** com `fopen`/`fread` na thread do jogo; `view_start` abre de novo para ler 0x40 bytes (a duração em vblanks); `Port_FilePath` abre e fecha uma vez para ver se existe | Três acessos à ISO por início, dois deles na thread do jogo. Uma música de 4,6 MB pode custar vários quadros; uma voz num golpe custa uma busca aleatória |
| `snd_se.c` | `sSpu[SPU_TOTAL]` com 10 MiB estáticos: 2 MiB do chip de som e 8 x 1 MiB (`OWN_SLOT`) só para o conjunto de vozes alternativo da rede | O 360 não tem rede: 8 MiB a menos sob `PORT_X360`, com o original no `#else` |
| `plat_mpeg2.c`, `plat_movie.c` | Decodificador próprio de MPEG-2 só intra, só inteiros, validado contra o ffmpeg pelo autor do upstream. Os dois filmes: 7.010 imagens, 512 x 448, 301 MB (cerca de 43 KB e 10,3 Mbit/s por imagem), todas tipo I | O formato é simples e já compila no 360. O custo de CPU no Xenon é **desconhecido** e é o maior risco dos filmes |
| `src/sys/movie.c`, `plat_file.c` | O jogo lê o `.PSS` por `ADXF_ReadNw` (4 buffers de 0x80000, 4,4 MiB do heap do jogo no total), e o port **ignora esses bytes** e lê o arquivo outra vez pelo decodificador | O filme é lido duas vezes da ISO, sem necessidade. Sob `PORT_X360`, a leitura do jogo pode virar no-op |
| `gs_core.c` (`transfer_data`), `gs_draw.c` (`GsGpu_FbUpload`) | A imagem vai ao quadro pela cadeia do PS2: decodificador, blocos de 16 x 16, 896 transferências para a memória de vídeo do GS (embaralhada por pixel), e no fim do quadro o backend relê a memória de vídeo por pixel e sobe uma textura. No 360 isso é **contado e não desenhado** (decisão 20, `gs_d3d.cpp`) | Quatro cópias por pixel antes de aparecer, e hoje nenhuma aparece. A F6 liga o desenho (decisão 6) |
| Boot e `FirstRun_Main` | O filme de abertura toca **em todo boot** depois dos logos (`FirstRun_Main`, `menu_i_c.c:189`) e também pelo modo 2 do despachante (`menu_a_b.c:109`, de cuja condição de entrada o estudo não leu); o final, depois da História (`menu_b_c.c:932`). START pula | O primeiro que o jogador vê com som é o filme. Nenhum documento registra se ele chegou a rodar no console nas sessões da F3 e da F4 |
| F0 a F5 | Nenhuma linha de XAudio2 foi escrita (a sonda da F0 não tocou tom); o XDK tem `xaudio2.lib` (exige `xmcore.lib`), e o `build.sh` monta `lib/xbox` inteiro | A camada do console é código novo, e a primeira linha precisa de um tom de teste |

### 1.3 Dependências herdadas da F5

- **Piso de memória**: o orçamento da F5 (32 MiB livres propostos, etapa 1) passa a valer **com o áudio ligado**. A
  F6 usa o que a F5 medir; sem essa medida, o teto do áudio da seção 3 é só proposta.
- **Margem de tempo**: a F5 reserva cerca de 3,37 ms entre o p99 de 30 ms e o período de 33,37 ms "para o áudio
  futuro", sem provar que cabe. A F6 mede o custo real do som na thread do jogo e re-roda os critérios de fps da F5
  com o som ligado.
- **Roteiros**: `f5_duelo` e `f5_treino` (etapa 3 da F5) são a luta de prova também do som: Goku (Early) contra
  Kuririn, com as vozes, os efeitos e o locutor que ela alcançar. A F6 usa o rastro deles, sem refazer a luta.
- **Boca e resultado da História**: a seção 1.1.

## 2. Redes de cobertura

Reaproveitar as redes da F5 e acrescentar as que o som pede. **O som não se compara por imagem**: o que se prova
é (a) o estado que o jogo vê, (b) as amostras que saem dos decodificadores, (c) o que o console faz com elas.
Cada resultado diz commit, ambiente, roteiro, arquivos lidos e limites; uma rede não substitui a outra.

1. **Código e contratos (WSL).** Inventário de toda chamada do jogo à CRI e ao `SOUNDS.IRX` (as `ADXT_*`, `ADXF_*`, as
   `sceMpeg*` e os nove comandos de `sceSifCallRpc` que `snd_se.c` atende), do que cada uma devolve ao jogo e de quem
   espera o retorno. Verificador que lista toda chamada sem implementação no 360 e todo `getenv` do som que o 360
   perde (a lição de 1.1: **um `getenv` que vira NULL muda o comportamento em silêncio**). Visto falhando com uma
   chamada retirada de uma cópia.
2. **Rastro de som (WSL, exato).** Um registro, por vblank, de cada evento que o jogo manda ao som: início de fluxo
   (id, volume, pan), parada, pausa, volume, pan, e cada comando da fila de efeitos (banco, id, volume, pan, pitch,
   handle), mais o estado devolvido a cada `ADXT_GetStat`. Feito por `--wrap` em `port/x360/headless/` (código de
   teste: a `.text` x86 do upstream não muda), no padrão do rastro de chamadas já existente. Critério: x86 e PPC
   (`SEMMAPA=1`) com o **rastro igual byte a byte** em cada roteiro, e o rastro igual entre duas x86. É a prova de
   que a luta toca as mesmas coisas, nos mesmos vblanks, que no PC.
3. **Amostras (WSL, exato contra referência independente).**
   - Um decodificador de ADX e um de ADPCM do chip **escritos de novo, em Python, a partir do formato** (por quem
     não escreveu o C), sem reaproveitar o código do port. Critério: PCM idêntico byte a byte ao do C do port, em
     todos os 20 arquivos grandes, em 300 sorteados com semente fixa (os 108 estéreo e as vozes mais longas
     incluídos) e em **todas as amostras dos 361 bancos**; e x86 igual a PPC nesses mesmos arquivos.
   - O arredondamento dos coeficientes do ADX é a dúvida registrada em `snd_adx.c` (`adx_coefs`: truncado, como o
     vgmstream; o ffmpeg arredonda, e a diferença chegou a 91 de 32.768). A referência independente confere a
     **fórmula do formato**, não prova que o decodificador da CRI arredonda assim; o limite fica escrito, e a
     escuta com o Lucas (rede 5) e a comparação com o PCSX2 (decisão 9) são o que fecha a dúvida.
   - Laço: o PCM em torno do ponto de laço (uma janela de 4 quadros de cada lado) igual ao da referência, nas 20
     músicas. O `snd_adx.c` arredonda o começo do laço para baixo ao quadro de 32 amostras; conferir que as
     músicas do disco têm o laço alinhado (a observação do arquivo diz que sim, o censo não foi feito).
4. **Mixer e fila (WSL).** O mesmo mixer compilado numa build de teste com um **dispositivo gravador** no lugar da
   SDL: um relógio de teste puxa o retorno de chamada em blocos fixos por vblank e grava o PCM de cada fluxo e a
   mistura final. Critério: x86 e PPC iguais nos fluxos ADX (aritmética inteira) e a mistura dos efeitos igual
   dentro de 1 LSB (o mixer usa `pow`, `cos` e `sin` da libm, que a PPC implementa por conta própria); mesma
   contagem de vozes ativas por bloco. Estragar de propósito (pitch trocado, pan invertido, banco com um byte
   trocado) e ver a rede acusar.
5. **Filmes (WSL).** O decodificador sobre os dois `.PSS` (lidos da ISO só leitura, sem copiar para o repositório):
   um hash por imagem do RGBA, **x86 igual a PPC nas 7.010**; e o `mpeg2test` do upstream como referência da x86
   (o ffmpeg do upstream não existe no WSL; se o Lucas o tiver no Linux, uma comparação nas 7.010 ao fim). Em
   qemu uma passada inteira leva minutos, não horas: pode rodar uma vez na rodada final, com amostragem nas outras.
6. **Console (rodada própria).** Contadores, não imagens: faltas de dado do XAudio2 (`BuffersQueued` zero com o
   fluxo ativo), latência do evento ao primeiro buffer entregue, tempo do pior bloco mixado, bytes e tempo de cada
   leitura por thread, tempo de som por passo na thread do jogo, mínimo de `dwAvailPhys`, imagens do filme
   apresentadas contra as esperadas e a deriva de áudio e vídeo. Mais **uma sessão de escuta com o Lucas** com uma
   lista fechada (seção 3). O qemu prova ordem de bytes e estado, não latência, falta de dado nem o som que sai.

No WSL, um ciclo de emulador por vez; conferir `pgrep -af "qemu|roteiros"` antes. `SEMMAPA=1`, nunca `PPC=1`. As
build, os objetos e a `.text` seguem as regras da F5 (seção 2): mudanças no upstream só em `#ifdef PORT_X360` ou
`PORT_BIG_ENDIAN` com o original no `#else`, objetos conferidos por `objcopy --strip-debug` e `cmp`, e a referência
da `.text` x86 a confirmar com a que a F5 entregar. Logs longos com `tee` em `~/`. Dados, ISOs, trechos de áudio
e capturas do usuário ficam fora do git.

## 3. Critério de fim proposto

Os números abaixo são uma proposta verificável para aprovação do Lucas (decisões 8 e 9). Nenhuma medição nova deste
estudo sustenta que já sejam atingíveis.

### Som

- **Cobertura.** Zero "sound file not found" e zero "not an ADX file" nas rodadas. Todo início de fluxo e todo
  comando de efeito dos roteiros (`f5_duelo`, `f5_treino`, `duelo`, `historia`, o roteiro das Opções) tem arquivo,
  banco e amostra existentes. Cada família tocada ao menos uma vez no console: música de título, de menu, de luta e
  de resultado; efeitos dos menus (`Snd_PlaySe`) e da luta (golpes, ki, projétil); a voz de um golpe; o locutor
  (`StreamSe`); a fala do guia com a boca; as duas séries de vozes (`SAVE_FLAG_ALT_VOICE`); mono e estéreo das Opções.
- **Estado.** Rastro de som igual na x86, na PPC e entre duas x86, em todos os roteiros. No console, a boca da
  Videl e da Dende em fase com as imagens de referência da F4 (etapa 7), e a tela de resultado da História chegando
  à tela 9 sem apertar nada.
- **Amostras.** As redes 3 e 4 da seção 2, sem exceção, com os controles negativos vistos falhando.
- **Música.** Laço sem pausa nem estalo contado (o PCM em torno do laço igual ao da referência); fade e volume
  seguindo `Adx_CalcOutVol` (a tabela de 0,1 dB conferida contra o ganho linear aplicado, a conta feita em teste);
  pan seguindo `Adx_ConvPan`.

| Medida | Aceite proposto | Como medir |
|---|---|---|
| Falta de dado do XAudio2 | Zero, em cada janela de 90 s da luta, na abertura, no final e na sessão de 30 min | Contador do retorno de chamada e do `BuffersQueued`; registrar o primeiro caso com a thread e o tempo |
| Latência dos efeitos | p99 até 50 ms (três vblanks) do comando na fila ao primeiro buffer entregue | Relógio na chamada de `Snd_RpcFlush` e no retorno do mixer que consome o comando |
| Início de voz e música | p99 até 50 ms do `ADXT_Start*` ao primeiro buffer, **sem** contar a leitura da ISO, e a leitura medida à parte, por thread | Registro por início, com bytes, tempo de leitura e thread |
| Custo do som na thread do jogo | p99 até 1,5 ms por passo da luta, máximo até 3 ms, **somando** as chamadas de som e qualquer espera de E/S de áudio; os limites de fps da F5 (p99 de 30 ms, nenhum passo acima de 33,37 ms) valem de novo **com o som ligado** | O vigia da F5 com a soma separada do trabalho do passo; três janelas de pelo menos 2.700 passos |
| Mixer | O pior bloco de 10 ms mixado em até 5 ms, nas threads 4 e 5, sem E/S nem espera por trava da thread do jogo acima de 1 ms | Tempo por bloco e o maior tempo esperando a trava do fluxo |
| Memória | Áudio em regime até 8 MiB (hoje 10 MiB só do `sSpu`, 8 deles a retirar), filme até 12 MiB durante o filme (4,4 MiB do heap do jogo mais os buffers do port); dentro do piso de 32 MiB livres da F5 | Bytes reais, mínimo de `dwAvailPhys` com a música, a voz e o filme ativos |
| Estabilidade | Sessão de 30 min, com ao menos dez ciclos de luta e menus, a Guide aberta três vezes na luta e uma na carga, dez saídas ao dashboard, sem queda, sem espera sem limite e sem som preso depois de parar | Sessão com o Lucas; log do diário; `StopEngine` na queda |
| Escuta | Lista fechada (abaixo) sem "ruim": cada item aprovado ou anotado | Sessão com o Lucas, ao final |

Lista de escuta: música do título e dos menus, o clique e o cancelar dos menus, a contagem da luta e o locutor, a
música da luta, um golpe com voz, um ki, a pausa (a música segue, a voz e os efeitos param), a fala do guia, a tela
de resultado, a abertura e o final com som, e o volume das Opções em dois extremos. Os níveis relativos (música,
efeitos, voz) ainda estão **de ouvido** (`SE_GAIN_DEFAULT` 0,25 foi escolhido "por ouvido" no PC); a escuta do
Lucas é a primeira comparação com o PS2 (decisão 9).

### Filmes

- **Os dois de ponta a ponta no console**, com som: a abertura no boot (sem pular) e o final depois da História; START
  pula com o fade de 0,5 s do jogo; no fim ou na saída o jogo volta ao fluxo e o som para (`Adx_Stop`); nada ao
  fundo continua rodando.
- **Imagens do decodificador**: x86 igual a PPC nas 7.010 (rede 5).
- **Ritmo**: 29,97 imagens por segundo (um passo a cada dois vblanks, como o jogo conta): pelo menos 99% dos
  intervalos entre imagens novas dentro de um período mais 1 ms, nenhum acima de três vblanks (50,05 ms), e o tempo
  de parede de cada filme dentro de 1% de 2.810 / 29,97 = 93,8 s e 4.200 / 29,97 = 140,1 s.
- **Sincronismo**: o jogo **não sincroniza** imagem e som (`src/sys/movie.c`: o som sai quando o primeiro quadro
  começa e os dois correm livres; só pausa o som quando o disco atrasa mais de quatro campos). Aceite proposto:
  diferença entre a posição do áudio tocado e a imagem mostrada até 100 ms em todo o filme, com a deriva ao
  fim registrada (decisão 7).
- **Memória**: até 12 MiB durante o filme, medida; sem leitura dupla da ISO.
- **Captura**: imagens 0, 1.000, 2.000 e a última do filme, no console, comparadas à imagem do decodificador da x86
  reduzida pelo mesmo filtro: pelo menos 98% dos pixels até 8 e PSNR de 30 dB (os limiares da F5, seção 3), mais
  aprovação visual do Lucas. Os limiares fecham antes da rodada.

### Fim da F6

Tudo acima, na build do commit final, mais: os critérios de fps e de memória da F5 repetidos **com som e filmes
ligados**; as duas dependências da seção 1.1 fechadas; e o guia atualizado com o que custou rodada.

## 4. Etapas e dependências

Estimativas de trabalho de uma pessoa familiarizada com o port, incluindo implementação, conferência e correções
comuns, sem a espera pela disponibilidade do console. Um dia útil significa 6 h de trabalho efetivo. São faixas de
planejamento pela leitura do código, não medições. Nenhuma execução foi feita no estudo. Build XDK e deploy são no
Linux; no WSL só código e testes locais.

| # | Onde | O que faz | Critério de saída | Tamanho estimado | Estado |
|---|---|---|---|---|---|
| 1 | WSL e console | Estado de som visto pelo jogo no 360: contar em vblanks sob `PORT_X360` (a regra da PPC e da x86 nos roteiros), sem XAudio2. Log do estado dos seis players na tela que parar. Provar ou derrubar a leitura da seção 1.1 | A tela de resultado da História chega à tela 9 sem apertar nada; a boca da Videl e da Dende como nas imagens de referência da F4; nenhuma tela do essencial trava esperando voz; o `getenv` do som inventariado (rede 1) | 1 a 2 dias | por fazer; pode entrar na etapa 9 da F5 |
| 2 | WSL | Rastro de som por `--wrap` em `headless/`; decodificadores de ADX e de ADPCM em Python escritos de novo; dispositivo gravador do mixer; verificadores com controle negativo | Rede 2, 3 e 4 verdes na x86 e na PPC (`SEMMAPA=1`) nos roteiros existentes; todos os controles vistos falhando; `.text` x86 intacta fora de `headless/` | 2 a 3 dias | por fazer |
| 3 | WSL e console | Camada de áudio do 360: as 14 funções da SDL sobre o XAudio2, um dispositivo de 48 kHz estéreo, `StopEngine` na queda e na saída; tom de teste primeiro. Núcleo portátil testado no WSL (fila do fluxo, ganho, conversão de taxa e de canais), pequeno trecho de XAudio2 testado só no console | Tom de 1 kHz limpo por 10 min com a thread do jogo sob carga de luta; zero falta de dado; Guide aberta e fechada sem estalo nem travar; saída ao dashboard sem pendurar o console (dez vezes); contadores no diário | 2 a 3 dias | por fazer; decisões 2 e 3 |
| 4 | WSL e console | Efeitos dos bancos no console: `snd_se.c` na camada, retirada dos 8 MiB de `OWN_SLOT` sob `PORT_X360`, medidas do mixer | O clique dos menus e os efeitos da luta ouvidos; pior bloco mixado e latência medidos; rede 4 igual na x86 e na PPC; memória do áudio dentro do teto | 1 a 2 dias | por fazer |
| 5 | WSL e console | Fluxos ADX: leitura em blocos fora da thread do jogo (decisão 4), duração sem leitura na thread do jogo, estado por vblank (etapa 1), laço, pausa, fade, pan; as seis vozes | Música dos menus e da luta com laço limpo; vozes de luta, locutor e fala do guia; as duas séries de vozes; leitura de ISO fora da thread do jogo (bloqueio de E/S de áudio dentro do aceite); rede 3 igual | 3 a 4 dias | por fazer; decisão 4 |
| 6 | Console, apoio no WSL | Filme, decodificador: **medir primeiro** o custo por imagem no Xenon (p50, p99, máximo, 600 imagens de cada filme lidas da ISO), depois escolher a thread (decisão 5); desenho da imagem (decisão 6); retirar a leitura dupla do `.PSS` | O custo medido e publicado antes de escolher; as 7.010 imagens iguais na x86 e na PPC; filme na tela no ritmo (seção 3) sem som | 2 a 4 dias | por fazer; decisões 5 e 6 |
| 7 | Console | Filme com som: ADX do filme pelos fluxos da etapa 5, sincronismo, START e fade, abertura no boot e pelo modo 2, final da História, memória do filme | Os dois filmes de ponta a ponta com som; deriva dentro de 100 ms; memória e ritmo no aceite; volta ao fluxo do jogo; captura de quatro imagens | 2 a 3 dias | por fazer; decisão 7 |
| 8 | Console, com Lucas | Som na luta e na vida do console: custo na thread do jogo, o vigia da F5 com a soma do som, Guide e pausa, perda e volta do controle, saída ao dashboard e queda, 30 min | Seção 3, todas as linhas da tabela do som; lista de escuta | 2 a 3 dias | por fazer; decisão 8 |
| 9 | WSL e console com Lucas | Rodada final do commit de entrega: redes completas, filmes, escuta, relatório; guia atualizado | Todos os critérios cumpridos com builds do commit final; tabela por roteiro e por rede; falhas provocadas; objetos e SHA1 x86 | 1 a 2 dias | por fazer |

Total indicativo: **16 a 26 dias úteis (96 a 156 h)**, sem filas do console. Maior incerteza: o custo do
decodificador no Xenon (etapa 6) e as leituras aleatórias da ISO durante a luta (etapa 5). Rever a previsão
depois das duas primeiras medições, em vez de prometer o ritmo dos filmes só pela simplicidade do formato.

Ordem: 1 e 2 não dependem de nenhum XAudio2 nem da GPU e podem começar já, no WSL, **em paralelo com a F5**. A 3 vem
antes de 4 e 5. A 6 (a medição do decodificador) não depende do som e pode adiantar-se também, mas escrever o
desenho do filme (decisão 6) conflita com o backend da F5 (etapas 6 e 7): fazer depois da integração da F5. A 8 e a
9 precisam da luta completa da F5 (etapas 8 a 10). A etapa 4 usa o orçamento de memória da etapa 1 da F5.

### Pontos de implementação que mudam a estimativa

- **Threads (guia, seção 6; `pesquisa/01`, 2.3).** O XAudio2 mora nas threads de hardware 4 e 5 e **nada de E/S roda
  lá**: o retorno de chamada do mixer só lê memória. A thread do jogo está na 2 (`main360.cpp`); o carregador de
  áudio vai para a 1 (par da main, parada), o decodificador do filme para a 3 (par do jogo, que durante o
  filme não tem lógica). Pôr E/S na 4 já causou queda e som estourado. A trava do fluxo (`SDL_LockAudioStream`) é
  tomada pela thread do jogo a cada comando e pelo mixer a cada bloco: uma seção crítica curta, medida.
- **O núcleo da SDL que falta são 14 chamadas** (`SDL_InitSubSystem`, `OpenAudioDevice`, `ResumeAudioDevice`,
  `CreateAudioStream`, `DestroyAudioStream`, `BindAudioStream`, `SetAudioStreamGetCallback`, `PutAudioStreamData`,
  `ClearAudioStream`, `LockAudioStream`, `UnlockAudioStream`, `SetAudioStreamGain`, `GetCurrentAudioDriver` e
  `GetError`), hoje todas stubs que falham (`SDL_InitSubSystem` devolve falso, e `device()` nunca abre nada). O que a SDL fazia de graça e o shim precisa fazer: **converter 24 kHz (ou 16 e 48) para 48 kHz** e
  mono para estéreo (a ADX usa `spec.freq = p->rate`), somar os fluxos, aplicar o ganho (`SetAudioStreamGain` em
  decibéis do jogo), e chamar o retorno com `additional` (quanto falta) no ritmo do dispositivo. `DestroyAudioStream`
  tem de esperar um retorno em andamento (o `stop()` do port conta com isso).
- **Ordem de bytes do PCM.** O código produz `int16_t` na ordem nativa do host, que no 360 é a que o XAudio2 espera
  (a conferir na documentação do XDK, que não está no WSL); nada a trocar, e o quanto antes um tom de teste na
  etapa 3 confirma. Os dados do disco já estão resolvidos (bancos pela saída A, ADX lido byte a byte).
- **Quando o dispositivo abre.** `device()` só tenta abrir com `GsGpu_Enabled()` (desenho ligado) ou `BT3_SOUND`, e
  `getenv` é NULL no 360: o som do console só nasce nas rodadas com desenho, e nas sem desenho (roteiros) o jogo
  continua sem dispositivo, como hoje. Isso é desejável, e é mais um motivo para o estado do som não depender do
  dispositivo (decisão 1).
- **O estado do som é do jogo, não do dispositivo** (seção 1.1 e decisão 1). Hoje, com dispositivo e sem `by_ticks`,
  `ADXT_GetStat` lê `p->stat`, que a thread do mixer escreve sem trava: uma corrida de dados e uma dependência do
  relógio do dispositivo. Contar em vblanks (`gPortAdxView`) tira os dois e é o que o WSL já prova; a cobrança é a
  deriva entre o fim contado e o fim ouvido, que o rastro do console registra.
- **Leitura dos fluxos.** `start()` lê o arquivo inteiro: uma música de 4,6 MB e 156 s fica toda na RAM por player e
  trava a thread do jogo na leitura. Em luta, `StreamSe_Play` (`btl_char_flag_snd.c:247`), `Voice_PlayDefault`
  (`btl_script.c:461`) e o locutor (`hud_e.c`) abrem fluxos novos com o jogo andando, cada um com uma busca
  aleatória na ISO. A F5 deixa 3,37 ms de margem, menos que uma busca de disco rígido. Por isso a leitura sai da
  thread do jogo (decisão 4), e a duração (a conta em vblanks que `view_start` faz abrindo o arquivo de novo) precisa
  de outra fonte ou de um cabeçalho em cache.
- **Bancos e memória.** `sSpu` tem 10 MiB estáticos (2 + 8), e 8 deles só servem à rede. Retirar sob `PORT_X360`
  é a primeira economia do áudio, antes de qualquer outra medida.
- **Ponto flutuante do mixer.** O sampler usa `pow`, `cos` e `sin` por efeito iniciado e float na mistura. O modo de
  arredondamento da FPU do jogo (decisão 5, truncado) é por thread: o mixer, em thread própria, fica no modo normal. A
  conferência é medir que a thread não herda o modo truncado, e a comparação com a x86 admite 1 LSB.
- **Filme, quatro cópias por pixel.** Decodificador, `sFrame`, blocos do `sceMpegGetPicture`, memória de vídeo do GS
  (embaralhada por pixel em `transfer_data`) e a releitura por pixel de `GsGpu_FbUpload`: cerca de 230 mil pixels,
  quatro vezes, por imagem, além do decodificador. Antes de mexer, medir cada parte (etapa 6); o atalho da decisão
  6 só entra se a cadeia inteira passar do orçamento.
- **Filme sem desenho.** Fora de `GsGpu_Enabled()` ou de `BT3_PAD_PLAY`, o filme termina na hora
  (`plat_movie.c`, `movie_start`); os roteiros do WSL usam `BT3_NOMOVIE=1` por isso. Para provar o filme na x86 e na
  PPC é preciso o caminho de `BT3_PAD_PLAY`, em que o filme leva o mesmo número de quadros.
- **O filme usa o player 0 (música).** O `Movie_Open` inicia o ADX do filme em `ADX_CH_BGM` pausado e o retoma ao
  primeiro quadro; a música da abertura é do mesmo player que a do menu. O estado por vblanks vale aqui também, e
  `Movie_WaitRead` (pausa o som se o disco atrasa) usa `ADXT_GetStat` 3 e 4.

## 5. Decisões para o Lucas

Os custos abaixo são incrementos relativos à linha proposta, sujeitos às medições. As recomendações não foram
aplicadas ao jogo. Sem resposta, manter a questão aberta e não implementar a arquitetura dependente.

1. **Que estado de som o jogo vê?** (a) Contado em vblanks pela duração do arquivo (`gPortAdxView`), como a x86 e a PPC
   fazem nos roteiros e como a partida em rede já exige: o jogo não depende do dispositivo, o WSL prova o estado,
   some a corrida de dados; custo: o fim "contado" pode divergir do fim "ouvido" em milissegundos, e uma queda de
   quadros atrasa a conta junto com a imagem. (b) Lido do mixer real, como no PC com som: casa com o que se ouve, mas
   é não determinístico, tem a corrida de dados e o WSL não o prova. (c) Híbrido (contar, mas corrigir pelo fim
   ouvido): mais código, ganho incerto. **Recomendação: a**, com o rastro do console registrando a deriva entre o fim
   contado e o fim ouvido; só passar a (c) se a deriva passar de 100 ms.
2. **Formato do som no console.** (a) PCM de 16 bits decodificado por software (o que o port já faz) e o XAudio2 só
   toca PCM: sem conversão de dados, sem ferramenta nova, o ADX continua na ISO como está. (b) XMA, o formato nativo do
   console: exige um codificador (o `xma2encode` do XDK, que não se distribui) numa conversão da ISO no PC, com
   perda, e um decodificador de hardware com as regras de memória (`XPhysicalAlloc` com alinhamento 0). A
   menção a XMA no guia e na tabela da F6 vem do plano geral, não de uma medida. **Recomendação: a.** O ADX de 4 bits
   a 24 kHz mono custa pouco para decodificar; só voltar a (b) se o custo de CPU medido na etapa 8 sobrar de fato.
3. **Quem mistura?** (a) Cada fluxo é uma voz do XAudio2 e o próprio XAudio2 converte a taxa e soma: menos código,
   mas a mistura final não é provável no WSL e a conversão do XAudio2 não é a da SDL. (b) Uma voz só, de 48 kHz estéreo,
   e um mixer portátil do port soma os fluxos e converte a taxa (o padrão do MKart360 e de "um jogo que já mixa por
   software"): a mistura inteira é código que compila e roda no WSL, com o gravador de teste; custo: cerca de 1 dia a
   mais (o conversor de taxa e a soma, que a SDL fazia) e um pouco de CPU nas threads 4 e 5, que sobra.
   **Recomendação: b**, pela prova no WSL. A qualidade da conversão (interpolação linear de 24 para 48 kHz, o dobro
   exato) se escuta na sessão; se for ruim, um filtro melhor é trabalho local da camada.
4. **Como ler os fluxos da ISO.** (a) Como no PC: o arquivo inteiro na thread do jogo: simples e já funciona, mas uma
   música de 4,6 MB e uma busca aleatória por voz entram na luta e a margem é de 3,37 ms. (b) Uma thread de carga na
   thread de hardware 1, em blocos de 64 KiB num anel por player, com o início do arquivo (cabeçalho e o primeiro
   bloco) pedido na hora, e a duração em vblanks vinda do cabeçalho lido uma vez ou de um cache de cabeçalhos
   (nunca de uma leitura na thread do jogo no meio da luta). Memória cai de 4,6 MB por música para cerca de 128 KiB
   por player. (c) Além de (b), carregar as vozes dos dois lutadores na carga da luta (de 2 a 4 MB por lutador, pela
   mediana de 6 KB e as cem falas): elimina a busca durante o golpe, ao custo de RAM e tempo de carga. (d) Acrescentar
   uma tabela de durações no conversor da ISO: tira a leitura do cabeçalho, mas mexe no formato da decisão 23.
   **Recomendação: b; c só se o tempo de início de voz medido na luta passar de 50 ms; d não.** A medição de início
   de voz na luta entra na etapa 5 antes de decidir c.
5. **Onde decodificar os filmes.** (a) Na thread do jogo, como no PC: simples, custo desconhecido. (b) Numa thread
   própria (a 3) com uma fila de duas ou três imagens, enquanto a thread do jogo desenha: dobra a folga, custa
   a fila e a posse do dispositivo de desenho. (c) Reduzir o filme (meia taxa de imagens, ou resolução menor):
   muda a obra e o ritmo que o jogo conta; só com aprovação. (d) Reencodar o filme no PC: muda dados e viola o
   critério de dados originais da decisão 18. **Recomendação: medir a primeira e escolher por limiar: a se a
   imagem levar até 20 ms de CPU no p99 (limiar de proposta, que deixa cerca de 13 ms do período para as cópias, a
   textura e o desenho; revê-lo com a medida), b acima disso; c só com a imagem e a medida na mão.** Não abrir o filme na etapa 6 sem o número.
6. **Como a imagem do filme chega à tela.** (a) Manter a cadeia do jogo (a memória de vídeo do GS, sem mudar o
   `movie.c` nem o backend além de desenhar o `GsGpu_FbUpload`): nada do jogo muda; custa as quatro cópias por
   pixel. (b) Atalho sob `PORT_X360`: `plat_movie.c` entrega o RGBA direto ao backend (uma textura por imagem) e
   `MovieTag_Send` vira um corte: uma cópia por pixel e imagem idêntica; mexe na camada do port, não no jogo, e
   precisa que o backend aceite uma imagem sem passar pela memória do GS. **Recomendação: a, medida; b se a cadeia
   passar do orçamento da etapa 6.**
7. **Sincronismo de som e imagem do filme.** (a) Como o PS2: correm livres, com a pausa do som que o jogo já faz
   quando o disco atrasa (`Movie_WaitRead`); aceite de 100 ms de diferença. (b) Travar a imagem no relógio do
   áudio (largar ou repetir imagens): muda o laço do `Movie_Run` sob `PORT_X360`, que é código do jogo. (c) Travar o
   áudio na imagem: pausar o som quando a imagem atrasa, pelo mesmo mecanismo do jogo, estendendo seu limite de
   quatro campos. **Recomendação: a, com a deriva medida em cada filme; c se passar de 100 ms.** Sem (b): o jogo não
   faz isso, e o ritmo da abertura é o do PS2.
8. **Guide, pausa, controle e saída.** (a) A Guide pausa a luta (decisão 24, resposta 5) e **também** os efeitos e as vozes
   (`Adx_PauseSeVoice` e `Snd_SetPause(4, 1)` do banco comum da luta, o par que o jogo já usa na pausa dele, em
   `btl_seq.c:1335` e `hud_e_d.c:60`), com a música seguindo; ao
   fechar, volta de onde estava; o controle desconectado vale a mesma pausa. (b) Tudo mudo na Guide (inclusive a
   música): mais simples, mas corta a música, que o jogo deixa tocar na pausa dele. (c) Nada muda no áudio na Guide: o som continua com
   a luta parada. A saída ao dashboard desmonta threads, áudio e texturas antes do `XLaunchNewImage` e, na queda,
   o `StopEngine`, como as lições do console. **Recomendação: a.**
9. **Quão estrita é a prova do som e como se calibra o volume.** (a) Só de ouvido: barata, deixa passar erro de fase
   e de laço. (b) As redes da seção 2 (rastro, decodificadores independentes, gravador, contadores) mais a sessão de
   escuta com lista fechada: exige o trabalho da etapa 2; é o que o WSL pode provar. (c) Acrescentar a gravação
   de áudio do PCSX2 das mesmas cenas (título, uma luta, a abertura) e a comparação dos níveis (RMS por segundo) e
   do espectro com a do console: calibra `SE_GAIN` e os níveis relativos fora do ouvido; custa a gravação
   (precisa do PCSX2 e do disco numa máquina do Lucas, a conferir) e uma ferramenta de comparação. **Recomendação: b; c
   como ajuste fino dos níveis depois da etapa 8, se o Lucas quiser.** A igualdade de amostras com o PS2 não é um
   critério (o chip de som tem interpolação e envelope que o port não reproduz, `snd_se.c`).

## 6. Riscos e medições que os revelam cedo

| Risco ou incerteza | Medição antecipada | O que fazer com o resultado |
|---|---|---|
| O estado de som é a causa da boca e da tela de resultado (seção 1.1) e não um tempo real | Etapa 1: log do estado dos seis players e da função que espera, na tela que parou, no console, com o roteiro `historia` sem desenho | Se for outra causa, abrir o achado do `F4/10.md` de novo com o log na mão; não implementar o resto achando que fechou |
| O decodificador de MPEG-2 não cabe nos 33,37 ms do Xenon | Etapa 6, antes de tudo do filme: p50, p99 e máximo por imagem de 600 imagens de cada filme, lidas da ISO, sem desenho e depois com | Escolher a thread (decisão 5) pelo número; se nem a thread bastar, levar ao Lucas as opções c e d com a medida |
| As quatro cópias por pixel do filme passam do orçamento | Etapa 6: o tempo de `transfer_data`, de `GsGpu_FbUpload` e da subida da textura, cada um sozinho | Decisão 6b com o número; não otimizar às cegas |
| Leitura aleatória da ISO na luta passa da margem de 3,37 ms | Etapa 2 conta os inícios de fluxo por luta no WSL (rastro); etapa 5 mede o tempo de cada início no console, por thread, em disco rígido e em USB | Decisão 4c com a contagem; se o USB for lento, dizer ao Lucas |
| Falta de dado do XAudio2 por disputa com o jogo ou o sistema | Etapa 3: o contador de `BuffersQueued` com a thread do jogo sob carga e a Guide aberta; registrar a thread e o instante | Aumentar a fila (latência) até o aceite; nunca pôr E/S na 4 ou na 5 |
| A corrida de dados do `p->stat` e dos campos do `Player` (a thread do mixer escreve, a do jogo lê) | A decisão 1a a elimina; com 1b, trava ou variável atômica | Não aceitar "funcionou nos testes" como prova |
| Um `getenv` do som devolver NULL e mudar o comportamento em silêncio, como em 1.1 | Etapa 1, rede 1: lista de todo `getenv` e de todo ramo de `GsGpu_Enabled` em `snd_*.c` e `plat_*` que o 360 vê, com o valor no console | Cada um com decisão escrita (valor fixo sob `PORT_X360`, ou `#ifdef`), nunca herdado |
| O coeficiente do ADX arredondado diferente da CRI (`adx_coefs`) | Rede 3: a fórmula contra a referência independente; escuta do Lucas; a comparação com o PCSX2 se a decisão 9c valer | Se soar diferente, trocar o arredondamento é uma linha; o limite fica no relatório |
| A conversão de taxa do shim soar pior que a da SDL | Escuta com a música a 24 kHz (a maioria dos fluxos) e as vozes | Filtro melhor na camada; só aí |
| O laço da música tem estalo (`loopStart & ~31u`, o histórico do filtro não é zerado) | Rede 3: PCM em torno do laço contra a referência nas 20 músicas; o censo de laços alinhados | Corrigir o arredondamento do laço se a referência o pedir; documentar |
| Memória: áudio e filme somados passam do piso da F5 | Etapa 4 e etapa 6: bytes reais; mínimo de `dwAvailPhys` com música, voz e filme | Retirar os 8 MiB de `OWN_SLOT` antes de qualquer corte; reduzir anéis só depois |
| A Guide, o controle desconectado ou a saída ao dashboard prendem ou estouram o som | Etapa 8: três Guides na luta, uma na carga, dez saídas, a queda provocada | `StopEngine` e a ordem de desmontagem; o que fizer pendurar vira a primeira linha do diário |
| O filme de abertura rodar a cada boot e atrasar ou travar o início | Etapa 1: ler o diário de uma rodada de boot e ver se `Movie_PlayOpening` rodou; etapa 6 mede | Se rodar lento, é a primeira coisa que o Lucas vê: prioridade da etapa 6 |
| A leitura dupla do `.PSS` (jogo e decodificador) | Etapa 6: bytes lidos da ISO por thread durante o filme | Sob `PORT_X360`, no-op no `ADXF_ReadNw` do `.PSS`; conferir que o jogo não depende dos bytes |
| Roteiros sem desenho passarem pelo filme na hora (`movie_start`) e esconderem um erro do filme | Etapa 2: um roteiro que toca o filme com `BT3_PAD_PLAY` | Sem esse roteiro, a rede 5 não cobre o filme no jogo |
| O som de `songs/` ou `mods/` | Fora do escopo: o 360 recusa `mods/` (decisão 24, resposta 3) | Documentar como limite declarado, sem alegar cobertura |

## 7. Divisão futura e pontos que pedem o console

Este estudo foi feito por uma pessoa, sem agentes auxiliares. Para a implementação, o modelo da 9c: entregas pequenas
com contrato, critério e integração por responsável, em vez de vários ciclos simultâneos.

| Frente possível | Arquivos e contrato principal | Pode adiantar sem console? | Integração necessária |
|---|---|---|---|
| Estado de som e rastro | `snd_adx.c`/`plat_sndstate.c` (só `PORT_X360`), `--wrap` em `headless/` | Sim, a leitura, o rastro e a regra; a prova da causa pede o console (etapa 1) | Antes de tudo: a F5 (boca) e a F6 dependem dela |
| Referências e gravador | Decodificadores em Python, dispositivo gravador em `headless/`, verificadores com controle negativo | Sim, inteira | A rede 3 e a 4 precisam do mixer e do shim da etapa 3 |
| Camada de áudio do console | `port/x360/src/` (shim das 14 chamadas, mixer portátil, XAudio2) | O núcleo portátil, sim; o XAudio2 só no console | Responsável único até o tom de teste; depois 4 e 5 podem ser de outros autores |
| Efeitos e fluxos | `snd_se.c` e `snd_adx.c` sob `PORT_X360`, carregador de blocos | O carregador e os testes, sim; as medidas, não | Depende da camada e do orçamento de memória da F5 |
| Filmes | `plat_movie.c`, `plat_mpeg2.c`, `ADXF_ReadNw` do `.PSS`, o desenho no backend | O decodificador na PPC e o hash por imagem, sim; a medida de custo e o desenho, não | O desenho conflita com o backend da F5 (etapas 6 e 7); fazer depois da integração delas |
| Provas do console | Diário, contadores, lista de escuta | Preparar sim; rodar não | Sessão com o Lucas |

Se houver vários autores, usar branches separados. No WSL, um ciclo de emulador por vez, com `pgrep` antes. O estudo
independente antes das etapas 3 e 6 pode ser só leitura, como pede o guia.

Exigem o console: tudo do XAudio2 (falta de dado, latência, o som que sai, ordem de bytes do PCM), o custo real do
decodificador e da cadeia da imagem no Xenon, o tempo de leitura da ISO (disco rígido contra USB), a Guide e a saída,
o ritmo e a deriva dos filmes, e a escuta. O WSL não os aprova.

Na entrega de cada etapa, registrar o que passou e o que não foi exercitado. Na final, uma tabela por roteiro e
por rede: contagem de eventos, falta de dado, latências (p50, p95, p99, máximo), custo na thread do jogo, memória, ritmo
e deriva dos filmes, imagens comparadas, falhas provocadas. Se uma etapa exceder o dobro do tempo previsto, parar com
estado e hipóteses documentados e apenas o trabalho provado em commit, conforme o processo do projeto.

## Respostas do Lucas (17/10/2026, decisão 25)

1a, 2a, 3b, 4b (c só com a medida; d não), 5a (outra opção só se aparecer problema), 6a, 7a, 8a, 9b (sem o ajuste
fino contra o PCSX2). Texto em [decisoes.md](../../decisoes.md), decisão 25.

**Correção da seção 1.1:** a build do 360 (F2 em diante) não usa o `port/x360/src/ambiente.c`; o `getenv` é o de
`port/x360/src/f2/posix360.c`, que lê os padrões e o `game:\ambiente.txt`. A conclusão vale (o `BT3_SOUND_TICKS` não
está definido no console), mas a prova não pede build: basta `BT3_SOUND_TICKS=1` no `ambiente.txt`, testado na rodada
da VU1 da F5; confirmado, ele entra nos padrões do `posix360.c`.
