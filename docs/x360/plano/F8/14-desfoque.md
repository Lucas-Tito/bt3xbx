# F8: diagnóstico da imagem macia no console

Branch `blur-analise`, base 1.0.2 `9c69c4ba`. Referência de PC: `troca/ref/upstream-main`, `48d66c1f`.
Sem console e sem XDK no WSL. Estado atual na seção de fase 2 abaixo: o Lucas identificou
o desfoque da distância em sessão livre; a medição por capturas equivalentes ainda está pendente.

## Rodada preparada

`python3 port/x360/tools/blur_console.py` gera dezesseis ambientes e copia o roteiro do Duelo para `~/blur-console`.
O pedido detalhado está em `~/blur-pedido-console.md`. As sete chaves novas, todas com padrão zero, são
`BT3_SEM_DISTANCIA`, `BT3_SEM_PANORAMICO`, `BT3_SEM_RADIAL`, `BT3_SEM_BRUMA`, `BT3_SEM_NEVOA`, `BT3_SEM_BRILHO` e `BT3_SEM_CLIMA`.
Elas cortam, respectivamente, `StgFog_Draw`, `StgPanBlur_Draw`, `StgBlur_DrawAll`, `StgHaze_Draw`,
`StgDepthTint_Draw`, `GfxPost_DrawGlow` e `EftWeatherPtcl_Draw` (só o desenho). O panorâmico conserva a atualização das vistas.
A névoa colorida é separada do byte intermediário produzido por `GfxDepthFog_Draw`, necessário a outros efeitos.
O brilho engloba a cadeia do céu e dos objetos; `BT3_FX_OFF=8` continua disponível como corte da soma final.

`BT3_SHOT_FINAL=1` acrescenta aos mesmos quadros escolhidos por `BT3_SHOT_LISTA` a captura `filtro` depois do FXAA
e a `tela` de 1280x720 depois da cópia. `BT3_COPIA_PONTO=1` troca só essa cópia para o ponto. Sem as chaves, não
há captura adicional nem mudança de filtro. `BT3_AFIADO=3` permite experimentar o filtro afiado da cópia, que já
existia; o padrão atual é 1, somente a arte 2D. `BT3_MSAA=0`, `BT3_FXAA=0` e `BT3_ESCALA=1` com
`BT3_ALVO_AREA=640x480` cobrem os outros cortes, sem mudar o tamanho de trabalho por acidente.

Os padrões lidos na 1.0.2 são escala **1,4**, área **640x480**, MSAA **2**, FXAA **0**, afiado **1** e brilho **60**.
Não se deve tomar a introdução histórica dos documentos da F8 como configuração atual.

## Estado das provas

O núcleo dos cortes passou pelas 128 combinações de chaves em C89 estrito. Uma cópia com a seleção do bit presa
no primeiro efeito foi reprovada pelo mesmo verificador. Os cinco fontes C do console foram preparados pelo
`c89.py` sem falhas. Isso ainda não prova compilação XDK, shaders D3D ou imagem no console.

A variante de medição `BT3_TESTE_DESFOQUE=1 headless.py --alvo x86|ppc` usa pastas `x86b`/`ppcb`.
Só nas cópias geradas abre os sete blocos marcados de diagnóstico, sem definir `PORT_X360` nos headers ou nos
outros caminhos. A build plana conserva o original no `#else` dos fontes do jogo. A variante de diagnóstico
não serve de nova referência da `.text` plana.

O primeiro envio disponibilizou os controles do console; as seções seguintes registram a análise e a rodada local armada.

## O PC atual, comparado com a nossa base

Leitura dos fontes de `48d66c1f`, de 03/11/2026, sem merge. A função `draw_state` está em `gs_draw.c`;
`gs_gpu.c` é o executor SDL GPU, que escolhe o shader pelo pipeline. Há também o executor GL.
Os trechos antigos do README que dizem que brilho, panorâmico e bruma são descartados foram superados pelos
parágrafos seguintes e pelos fontes. O que vale hoje:

| Item | PC atual | 1.0.2 do 360 |
|---|---|---|
| Cópias entre alvos | sprites/faixas desenhados, CLAMP manual pela área do buffer, ponto ou bilinear conforme TEX1 | mesma lógica herdada |
| Casos recusados | ler o próprio alvo, vistas incompatíveis de 16 bits, acesso cru a canais da VRAM; usa nativas para profundidade/ids | mesma estratégia, implementada no D3D |
| Meio pixel dos sprites de alvo | subtrai `0,5 * delta_uv / delta_xy` do uv, conserva a cobertura | já temos, inclusive o cuidado de não deslocar a geometria |
| `fx_off`/`BT3_FX_OFF` | máscara de 8 bits: 1 contorno, 2 transparência, 4 névoa colorida, 8 brilho, 16 distância, 32 panorâmico, 64 clima, 128 metade das partículas | máscara de 5 bits; 32, 64 e 128 são perdidos pelo `&31` |
| Panorâmico | bit 32 corta na origem, atualiza as vistas mesmo desligado, opção “Movement blur” | não havia corte independente; chave nova nesta investigação |
| Brilho | intensidade do céu `glare` separada de `glow` dos objetos, ambas 60 por padrão | uma intensidade só, 60 |
| Alvos de trabalho do brilho | páginas 0x150, 0x170, 0x1F6, 0x1F8 na resolução 1 só durante a cadeia, marcadores 0x20/0x21 | escala uniforme 1,4 em todos os alvos |
| Amostra na escala alta | `GS_BLOCK`/`gswb.frag`, bit 25 do pipeline; a média dos blocos de texels antes do bilinear em coordenadas do GS | bilinear comum do hardware, entre texels da resolução interna |
| Distância | a cópia reduzida tem os mesmos marcadores; média dos blocos na redução impede folhas virarem quadrados ao esticar | nenhuma dessas duas correções posteriores |
| “Sharp stage blur” | `sharp_blur`/`BT3_SHARP_BLUR`, padrão 1: fora das cadeias marcadas conserva o detalhe da resolução interna; 0 reproduz amostragem mais macia do PS2 | não há opção equivalente |
| Clima | cortar ou reduzir as partículas, preservando movimento | corte novo `BT3_SEM_CLIMA`, sem redução pela metade |

O bloco de amostragem do PC é limitado aos passes necessários: a soma de volta ao quadro lê uma textura já
suave sem dezesseis leituras por pixel. O shader usa quatro leituras bilineares por bloco nas escalas maiores,
com as coordenadas de cada vizinho presas aos centros extremos do CLAMP. Na escala 2 uma leitura no centro do
bloco 2x2 já dá sua média. A opção dos alvos nativos evita gastar esse caminho no miolo da cadeia.
Não se deve portar isso simplesmente redimensionando todas as páginas durante o quadro inteiro: as mesmas
páginas também guardam sombras, água, lente, calor e outros efeitos em outros momentos.

As teclas **F4 brilho e F5 distância** descritas no README são o histórico da interface antiga, com os bits 8 e
16. No fonte atual não há esses atalhos de efeito em `gs_input.c`; a janela de configurações abre com F1 e
apresenta os controles em `ui.cpp:effects_tab`. F4 é usado por padrão pela acessibilidade para ler o medidor do
segundo jogador. A configuração “Movement blur” continua sendo precisamente o bit 32, não FXAA.

Correção de colisão das tabelas de brilho: o upstream também conserva o conteúdo e confere os 256 elementos,
com os bits altos no hash. **Essa já temos** em `clut_texture` e `sClutPx`; não é uma lacuna nova do 360.
O mesmo vale para o meio pixel, CLAMP e o reconhecimento das cópias entre buffers.

As correções posteriores de brilho tendem a **borrar mais corretamente o halo**, evitando a duplicata nítida
fora do personagem. Sua ausência não prova que a tela inteira esteja macia. Não apliquei mudança de imagem.

## Caminho do 360, passe por passe

Tudo a seguir é **lido no código**, exceto os resultados numéricos identificados adiante. A presença do passe
numa captura específica precisa ser confirmada no rastro, no dump ou pela diferença do corte.
No GS a imagem visível é 512x448; o framebuffer pode ter largura 640. O armazenamento padrão do 360 é
896x672 para a área lógica 640x480. O recorte visível corresponde a 716,8x627,2 texels, depois ampliado a 1280x720.
A 4/3 usa 864x608 para 648x456; a parte mostrada corresponde a 682,67x597,33.

| Candidato e gatilho | Entrada e saída lógicas | Força e amostra | Divergência que precisa ser separada |
|---|---|---|---|
| Distância, `StgFog_Draw` | quadro 512x448 para página 0x150 de 256x224, depois de volta ao quadro | tabela de 256 alfas por profundidade, `curve[i]*128*scale`; ALPHA final 0x54, mistura pelo alfa de destino; retorno com uv +8/16 de texel | só na feature 3 do estágio e sem a flag 0x400000000000000; o byte de profundidade reconstruído escolhe a força; a cópia na escala 1,4 perde menos detalhe que a do PS2, falta a correção recente do PC |
| Panorâmico, `StgPanBlur_Draw` | quadro para 256x256, depois 128x128, duas camadas deslocadas, volta à vista | `strength` presa em 0..1, aumenta 0,1 ou diminui 0,15 conforme deslocamento; `side` presa em -1..1, alfas `max(101*strength - i*32,0)`; TEX1 bilinear | feature 0xD; vistas não atualizam na pausa, força anterior permanece; profundidade mascara o cenário; escala dos alvos e retorno podem alterar o rastro, não há leitura temporal automática de quadros passados |
| Radial, `StgBlur_Draw` | quadro para 256x224, alterna 0x150/0x16C, anéis de volta ao quadro | padrão 3 passes, escala 2, deslocamentos 2; só desenha com alfa de algum anel diferente de zero; cresce 1,2,4... por passe, uv dos vértices +0,5 | técnicas/eventos alteram cores e centro; não fica visível por padrão; sprites intermediários recebem correção do meio pixel, anéis Gouraud são triângulos e não recebem automaticamente essa regra dos sprites |
| Brilho, `GfxPost_DrawGlow` | quadro pelo alfa para 256x256, 128x128, 64x64; alterna 0x1F6/0x1F8; amplia a 128, 256 e quadro | chamadas usuais com 10 passes; TEX1=0x60; a ida lê 1..65 e a volta 0..64, de propósito; soma final ALPHA 0x68, RGB vezes 60% | céu depende de `gStgGlare.enabled` e do nível animado por parâmetros; objetos usam sua máscara; na 1,4 o filtro lê vizinhos internos, não blocos de texels do GS; não trocar uv pelo centro sem medir o halo |
| Distorção de calor/bruma, `StgHaze_Draw` | quadro e malha para uma cópia de altura 224, depois composição da malha | feature 9; padrão malha 20x50, amplitude 0x80 em unidades de 1/16, atualização por quadro, seguimento 0,1, cor 0x80808080; CLUT atenua conforme altura da câmera | deformação local, não um desfoque uniforme; passa por triângulos/faixas com uv e interpolação próprios; o corte mantém `UpdateClut` e `Step`, inclusive o consumo de aleatórios |
| Névoa colorida, `StgDepthTint_Draw` | byte de profundidade pela tabela do estágio sobre o quadro, native 3 | escolhe o meio acima/abaixo da água; só com `tint->enabled`; cor e alfa da CLUT, mistura normal 0x44 ou aditiva 0x48 | não amostra vizinhos da cor, reduz contraste sem borrar detalhe geometricamente; a quantização errada da profundidade pode aplicá-la perto demais |
| Poeira/bruma do clima, `EftWeatherPtcl_Draw` | sprites grandes de textura sobre a cena, sem cadeia de redução do framebuffer | `gEftWeather.enabled`, número, tamanho e cores dos parâmetros; oculta se estágio não desenha; movimento cessa na pausa | cobertura semitransparente pode dar sensação de imagem lavada; é diferente de StgHaze e do filtro por distância; o corte mantém movimento e sorteios |
| FXAA | alvo resolvido para textura própria, mesmo tamanho interno, antes da cópia para tela | filtro de contraste e vizinhos no quadro inteiro, sem máscara da interface; padrão desligado | se ligado por ambiente, pode suavizar a HUD; só comparar `gpu` não o enxerga; a b10 é o controle positivo |
| MSAA | só o alvo apresentado com 2 amostras; alvos temporários com 1; Resolve entrega a média da cor | padrão 2x, contorno por amostra ligado; ids/DATE leem fragmentos separados; volta exata desligada, A2C desligado, 2D sem MSAA desligado | média esperada nas bordas; desalinhamento, volta pela média e bordas fracionárias da HUD podem estender cobertura; interiores não deveriam receber um filtro espacial geral |
| Escala e cópia | 512x448 lógicos na escala interna para 1280x720 | bilinear comum por padrão na cópia; `HALFPIXELOFFSET=TRUE` põe centros em 0,5 como GL/Vulkan; afiado bit 1 só na arte 2D | o afiado da arte não protege da mistura posterior na cópia; bit 2 já existia mas foi recusado na rodada antiga; o corte no ponto e o bit 2 são só controles, não correções aprovadas |
| Saída física | backbuffer 1280x720 para modo de vídeo efetivo, HDMI e TV | fora dos shaders do jogo | escalonamento, overscan e processamento da TV não aparecem em nenhuma PPM interna; precisam da saída HDMI comparada com `tela` |

O panorâmico e o brilho escrevem TEX1=0x60 explicitamente; distância, radial e distorção de calor
herdam o mesmo 0x60 de `Gfx_PutDefaultEnv`, portanto pedem bilinear na redução, nas cópias
e na composição. As tabelas nativas e os ids usam ponto. Os sprites do clima usam o filtro da textura
que `EftGfx_DrawSprite` configura; não amostram o framebuffer. O backend `prepara` usa o sampler
correspondente e os shaders prendem os uv ao `rect` de CLAMP. A viewport faz a escala das posições, e `c6`
normaliza o uv pela área do alvo; o `gsScale` que grava a lista do 360 permanece 1. A correção de meio pixel
é em unidades do GS, antes dessa escala. Na 1,4 meio texel do GS vale 0,7 texel físico, portanto não é a mesma
operação que pôr os alvos da cadeia na escala 1.

Nenhuma leitura comum de alvo vem diretamente da EDRAM. `ed_leitura`/`sai_da_edram` resolvem a fonte suja numa
textura tiled, com retângulos alinhados a 8; o desenho seguinte amostra essa textura. Resolve sem MSAA copia a
cor, não faz bilinear. Com MSAA a média mistura as amostras **do mesmo pixel**, não uma janela arbitrária de
pixels vizinhos. Os temporários na escala 1,4 têm só uma amostra. O restauro de profundidade preserva os bits
pelo caminho de cor do autoteste, e a tabela reconstrói `z >= 65448 ? 0 : clamp(255-floor(z/256),0,255)`;
a tabela usa ponto nos centros. A equivalência visual dessa quantização à memória do PS2 continua exigindo
referência apropriada, não se deduz só de a fórmula ser igual à do PC.

A arte da seleção é outro caso: o fundo 1024x512 é reduzido para 512x448, já documentado na F4. A referência
por software lê no ponto e perde linhas; diferença de nitidez desse fundo sozinha não prova defeito D3D.
A UI e o HUD precisam de recortes próprios, separados do fundo e do cenário distante.

## Medidas locais e coleta pendente

`brilho_modelo.py`, executado nesta tarefa, repetiu seis impulsos, três posições em cada eixo. O bilinear
do GS tem centro +40,5 pixels em x e +35 em y contra o rasterizador de ponto; larguras 13,8 contra 4,6 em x
e 12,2 contra 4,0 em y. Não são capturas do PC ou do console: são as consequências das coordenadas do jogo
no modelo já existente, úteis para rejeitar a referência errada. A soma em y também muda, 15,8 contra 14.

`teste_escala.c`: 86 casos, zero falhas. `teste_afiado.c`: 163.200 casos, zero falhas. A área correta é indispensável
ao corte da escala: usar só `BT3_ESCALA` pode provocar a queda para 1024x1024 descrita na etapa 09.
A base x86 da 1.0.2 foi recompilada no próprio caminho, com os quatro fontes originais restaurados temporariamente
e repostos ao fim: 398 objetos, ligação com 104 stubs, `.text` SHA1 `94fe2737c0b9f6663ca659685be77a7cd1d36659`.
A comparação dos objetos finais, retirando depuração com `objcopy --strip-debug` e comparando byte a byte,
fica na rodada armada; somente o objeto de teste do carimbo pode diferir.

`blur_rodada.py`, sob `~/vez.sh`, compila x86, x86r e PPC do commit limpo e roda a conferência oficial
`SEMMAPA=1 IMG=1 roteiros.sh f5_duelo`. Depois compila as variantes de diagnóstico x86b/PPCb, coleta seis
momentos ancorados no roteiro em oito configurações e exige igualdade x86/PPC por `roteiros.py compara_imagens`.
O verificador é repetido contra uma cópia com um pixel estragado. Nenhum `PPC=1` é usado.

Também foi compilada uma cópia descartável do PC atual em `port/build/blur/pc-atual`, com 104 stubs e ligação
sem erro. A SDL local é 3.2.24: faltam `SDL_LoadPNG` e `SDL_LoadPNG_IO`, usados por dois fontes da interface nova.
Somente nessa cópia de diagnóstico, as cargas de PNG da janela/mods devolvem NULL; a imagem dos alvos do jogo
é capturada antes de qualquer arte lateral da janela. Não é uma build de distribuição nem prova da interface.
`blur_pc.py` abre os mesmos sete cortes nessa cópia, conserva a simulação de clima e calor, reproduz o desbloqueio
no vblank 2834 que a gravação de controle não registra e encerra a coleta após vblank 7000.
O serviço usa a entrada gravada pela x86, com cartão vazio, e captura os números do GS lidos das seis imagens
nas escalas 1 e 2 pelo backend GL real, via SDL offscreen. A disponibilidade desse vídeo e o alinhamento entre
versões ainda não foram provados: o serviço conserva a primeira falha para diagnóstico, sem multiplicar tentativas.
Mesmo se os números coincidirem, é obrigatório verificar cenário, pose, HUD e tempo antes de comparar pixels.

Os resultados e logs vão para `~/blur-analise/`; capturas e dados permanecem fora do git. Os quinze ambientes
iniciais ganharam `environment_b15_clima.txt`, totalizando **dezesseis**, e a bruma foi movida do corte do grupo
para o corte do desenho, para não mudar o gerador aleatório. O pedido atualizado fica em `~/blur-pedido-console.md`.

## Julgamento inicial e proposta conservadora (histórico)

**Estado antes do retorno do console:** não havia culpado comprovado nesta entrega parcial. A hipótese do FXAA sempre ligado é incompatível com o
padrão lido, mas ainda é preciso conferir o ambiente real do console. A hipótese de um efeito de luta explicar
letras macias no menu pede uma diferença que comece antes da cópia final; o código dos efeitos do estágio não
é executado pelos menus. A saída final e a TV continuam candidatas para maciez geral.

Quando chegarem capturas válidas, comparar base e repetição primeiro; depois cada corte no mesmo GS:

1. Diferença já no `gpu` restrita ao cenário ou ao halo localiza um efeito ou MSAA/escala. Confirmar que o passe
   estava ativo, e que a simulação não mudou, antes de atribuir causa.
2. `gpu` igual e `filtro` diferente só com FXAA ligado localizam esse filtro.
3. `gpu` e `filtro` iguais, mas `tela` mais definida com ponto/afiado, localizam a cópia. Proposta: avaliar com o
   Lucas o bit 2 ou outro filtro de ampliação, mantendo a mudança desligada até aprovação visual.
4. PPM `tela` definida e saída HDMI/TV macia localizam etapa posterior; não mexer no brilho para compensar.
5. Halo duplicado ou largura incorreta do brilho pedem portar os marcadores, a resolução temporária e a
   amostragem de blocos do upstream, cuidando de escala, DATE, residência e Resolve. Preparar variante separada
   para Lucas comparar, pois muda a imagem. Não subir a força nem trocar o meio pixel globalmente.
6. Distância aplicada perto demais pede conferir o byte/CLUT e o z de entrada por regiões. Corrigir só a
   transformação comprovadamente errada, sem desligar o efeito inteiro como solução.

O resultado da rodada local deve ser julgado antes de marcar o restante como concluído. Se o console ainda
não tiver resultado depois disso, o marcador solicitado é `test ! -d /home/codex/blur-resultado` em
`~/espera/blur-analise`. Até a rodada local terminar, o marcador acompanha somente o serviço desta tarefa.


## Retomada de 04/11: primeira rodada julgada e coleta corrigida

A pista do Lucas restringe primeiro a investigação aos elementos da arena, mantendo os lutadores nítidos.
A rodada do console deve começar pela base e repetição, distância, névoa, bruma/clima, MSAA e escala,
com recortes separados do chão próximo, cenário distante e lutadores. Os filtros gerais continuam cobertos
pelos controles, mas não há prova para promovê-los à explicação principal.

### Mipmaps, LOD e bilinear do cenário

No PS2 os estágios podem pedir mipmaps, mas **o backend atual do 360 não os implementa**.
`texture_get`, em `port/src/gs/gs_draw.c`, cria as texturas do jogo com `texCreate(..., 1)`;
`tex_create` do D3D ignora `levels`, `xg_cria_n` fixa um nível em `XGSetTextureHeader`, e
`tex_upload` recusa `level != 0`. `aplica_sampler` fixa `D3DSAMP_MIPFILTER=D3DTEXF_NONE`.
O sampler só recebe TEX1 bit 5 como escolha entre ponto e linear, igualmente para perto e longe;
os parâmetros K, L e MXL do GS não escolhem níveis nesse caminho. O 360 também não usa pacote de
substituição de texturas. No PC, níveis extras existem nos pacotes substitutos; as texturas originais
também são criadas com um nível no frontend.

Portanto `tex2Dgrad`, mesmo calculando derivadas em resolução 1,4, não pode selecionar um mipmap
mais borrado do cenário: não há nível alternativo disponível. Isso descarta **essa seleção de LOD
por código**, ainda sem prova visual do console. Não descarta o bilinear do nível base, a precisão
perspectiva ou as cópias do quadro. Não há motivo para inventar um viés K/L como correção.

A profundidade usada pela tabela merece uma separação: `z_sinc` resolve com
`D3DRESOLVE_DEPTHSTENCIL | sZFrag`, e `sZFrag` fica zero fora do autoteste. É o fragmento 0;
a tabela não faz média de profundidade das duas amostras. Isso pode divergir na cobertura de bordas,
mas o código não prova erro no interior inteiro de uma arena. Os pares base/distância, base/névoa e
base/MSAA 0 devem localizar a diferença. O teste MSAA 0 também muda as bordas de cor, então uma diferença
isolada de silhueta não basta para culpar o z. Conferir a CLUT e o byte de profundidade em regiões
próximas e distantes antes de propor mudança.

### Resultado do serviço anterior, commit 1f8b5266

| Conferência | Resultado |
|---|---|
| Builds x86, x86r e PPC limpas | concluídas; layout plano com 1965 structs, zero diferentes |
| Objetos x86 sem depuração | 326 iguais; só x360_passos.o diferente, o carimbo de teste |
| .text x86 | idêntica à base, SHA1 94fe2737c0b9f6663ca659685be77a7cd1d36659 |
| SEMMAPA=1 IMG=1 roteiros.sh f5_duelo | código 0, tudo certo; luta, memória, save e rastros aprovados |
| Imagens oficiais x86/PPC | 13 de 13 idênticas, 640x448 |
| Coleta com cortes independentes e PC | não iniciada: duplicação da conversão parou antes das capturas |
| Console | ~/blur-resultado ainda ausente na retomada |

O serviço terminou com código 1, mas **depois** da aprovação oficial. A coleta duplicava a conversão e
exigia retorno zero bruto do `conv_tool`. A rodada oficial havia aceitado o retorno 1 pelo julgamento
de `roteiros.py larguras`, com leitor e posição conferidos: `00005.bin` 0x7AC42/0x8CF02 em `Font_ReadChar`,
e `01432.bin` 0x18954 em `EftRays_SetKey`. A correção reaproveita `oficial/f5_duelo/dados` somente depois
de o critério inteiro terminar com zero. Nenhuma exceção nova foi acrescentada ao conversor.

A lista da coleta também repetia `roteiro: arquivo:linha`, embora `Imagens_Evento` retire esse prefixo
antes da comparação. Foi corrigida para as mesmas seis âncoras já medidas pela rodada oficial:

| Momento | Âncora e espera | GS medido |
|---|---|---|
| Menu | tela 4 +100 | 2969 |
| Seleção | tela 39 +100 | 3362 |
| Apresentação | marca ponto carga +0 | 4590 |
| Combate | marca ponto combate +0 | 4977 |
| Técnica/câmera | marca ponto tecnica +0 | 5357 |
| Pausa/HUD | marca ponto pausa +0 | 5418 |

Os ambientes do console agora usam os **treze GS medidos**, incluindo os seis acima, em lugar dos doze
números estimados. O cenário, pose, HUD e tempo ainda precisam coincidir na GPU e no PC atual; números
iguais não são, sozinhos, prova de alinhamento. Uma chamada de corte alcançada tampouco prova força
não zero naquele quadro.

O preflight novo foi visto recusando a âncora com prefixo numa cópia da lista, antes de aceitar as seis
âncoras válidas no rastro. A conferência dos PPM do PC foi vista recusando conjunto vazio, PPM truncado,
dimensões trocadas, escala errada e quadro extra, antes de ser usada na coleta real. Ela só prova presença,
formato e resolução; a equivalência visual fica para o julgamento seguinte. O comparador existente
aceitou as 13 imagens originais e recusou uma cópia com um pixel alterado em (639,447), desvio 255.
O verificador de larguras recusou a troca intencional de 0x7AC42 para 0x7AC44 numa cópia do log.

A nova rodada recompila e executa o critério com o commit limpo que contém estas correções, depois coleta
os oito pares x86/PPC e os dezesseis casos do PC. Usa uma vaga de `~/vez.sh` durante todo o ciclo;
a guarda global de qemu foi retirada conforme a autorização de três vagas da tarefa. Os resultados da
primeira rodada são preservados separados. A coleta corrigida ainda exige julgamento; nenhuma causa
visual foi declarada e nenhuma correção de imagem foi ativada por padrão.


## Fase 2: retorno do console e tratamentos do PC (04/11, segunda retomada)

O `LEIA-ME.md` recebido em `~/blur-resultado/` registra o teste do Lucas com `4310e558`:
sem `StgFog_Draw`, o incômodo desapareceu. Ele quer conservar algum desfoque e comparar o tratamento do PC.
O log confirma o corte alcançado. Foram inspecionados os quatorze quadros `gpu` em contato visual:
o treino é em uma arena gramada, diferente da cidade do roteiro local. Não existe captura da base
nessa sessão, porque faltou `BT3_LOG=1`. Portanto a identificação vem da observação do Lucas;
não existe ainda uma diferença quantitativa base/corte no mesmo estágio, pose, câmera e tempo.
Não atribuir os interiores do cenário ao fragmento de profundidade do MSAA sem essa prova.

### Rodada encerrada de 4310e558

| Conferência | Resultado |
|---|---|
| Builds x86/x86r/PPC | concluídas; PPC com 1965 structs, zero diferentes |
| Objetos x86 sem depuração | 326 iguais; só x360_passos.o difere pelo carimbo |
| .text x86 | idêntica à base: SHA1 94fe2737c0b9f6663ca659685be77a7cd1d36659 |
| SEMMAPA=1 IMG=1 roteiros.sh f5_duelo | código 0, tudo certo; 13/13 capturas idênticas |
| Cortes independentes | 8 casos, 6 momentos cada: 48/48 pares x86/PPC idênticos |
| Pixel adulterado em cópia | recusado, (639,447), desvio 255 |
| PC GL atual | iniciou e terminou com 0, mas nenhum PPM; fopen falhou sem pasta shots |

Pixels alterados pelo corte contra a base no rasterizador de ponto, em imagens 640x448:

| Corte | Menu | Seleção | Apresentação | Combate | Técnica | Pausa |
|---|---:|---:|---:|---:|---:|---:|
| Distância | 0 | 0 | 4897 | 21340 | 7361 | 5917 |
| Panorâmico | 0 | 0 | 0 | 97690 | 0 | 0 |
| Radial | 0 | 0 | 0 | 0 | 0 | 0 |
| Bruma/calor | 0 | 0 | 0 | 0 | 0 | 0 |
| Névoa | 0 | 0 | 84467 | 116732 | 80106 | 72950 |
| Brilho | 0 | 0 | 198655 | 194904 | 173896 | 159790 |
| Clima | 0 | 0 | 142299 | 196747 | 0 | 0 |

Ausência de diferença no radial ou calor não os descarta em outros quadros. A comparação de clima
mantém a movimentação e os aleatórios; o rasterizador não reproduz o bilinear dos efeitos da GPU.
Os números medem influência nesse roteiro, não a nitidez da imagem do console.

### Implementação opcional, sem trocar o padrão

| Chave, padrão 0 | O que faz |
|---|---|
| BT3_SHARP_BLUR | Com blocos ligados, mantém amostragem na resolução interna fora das cadeias marcadas; dentro, conserva o tratamento da cadeia. |
| BT3_DISTANCIA_NATIVA | Delimita StgFog com marcadores 0x22/0x23; a página reduzida 0x150 recebe um texel físico por pixel do GS só durante a cadeia. |
| BT3_DISTANCIA_MEDIA | Na redução da imagem para a página nativa, interpola quatro médias de blocos do GS; depende de DISTANCIA_NATIVA. |
| BT3_GLOW_BLOCK | Traduz o GS_BLOCK/gswb.frag: só fontes de trabalho bilineares entre alvos de trabalho, mais a redução para alvos nativos; nunca a composição final inteira. |
| BT3_GLOW_NATIVE | Delimita GfxPost_DrawGlow com 0x20/0x21; páginas 0x150/0x170/0x1F6/0x1F8 em resolução 1 só dentro da cadeia. |

A 1.0.2 já amostra fora das cadeias na resolução interna. Por isso `SHARP_BLUR=1` sozinho não
pode criar detalhe novo: deve ser comparado com `GLOW_BLOCK=1, SHARP_BLUR=0`, como no pedido.
As correções de distância do PC podem **aumentar** sua suavização: corrigem a perda de força com
a escala, não reduzem a intensidade da CLUT. Nenhuma foi apresentada como solução visual aprovada.

O frontend guarda a seleção em `GsDraw.orig` (x: blocos; y: fonte nativa; w: destino nativo),
sem mudar o layout de 128 bytes. Os bits viajam com o retrato já usado pela thread de desenho.
O backend aplica a escala física na reprodução, porque `gsScale` do 360 é sempre 1:

- `vs_gs_baixo` leva só as posições dos destinos marcados para o canto nativo; a tesoura também fica em pixels nativos.
- Fontes nativas usam uv multiplicado por GS_W/sAlvoW e GS_H/sAlvoH. Fontes comuns mantêm GS_W/sAreaW e GS_H/sAreaH.
  O meio pixel dos sprites e o CLAMP continuam os mesmos do frontend; nenhuma troca global de centro de pixel.
- `ps_gs_bloco` compartilha o corpo do shader normal, inclusive DATE, teste de alfa e FBA. Só a leitura muda:
  bilinear manual entre quatro texels do GS, cada centro preso ao CLAMP, e a média interna do bloco do PC.
  O shader comum recebe o parâmetro constante falso, sem as leituras extras. Os shaders novos entram no build_f2.sh.
- EDRAM, alocação, residência e Resolve mantêm seus objetos e fluxo. O retângulo sujo usa a tesoura física real;
  nenhuma textura é lida diretamente na EDRAM. O quadro com MSAA e sua profundidade permanecem na escala normal.
  A página temporária continua com uma amostra. Não há mudança no cálculo de profundidade ou na CLUT da distância.

Custo que dá para delimitar sem console: nos blocos, quatro leituras bilineares por fragmento nas
escalas até 2, contra uma no caminho comum; acima de 2,5, são dezesseis como no PC. Na escala 2,
uma leitura no centro é a média exata do bloco 2x2. Nas escalas fracionárias 4/3 e 1,4, é a
adaptação do filtro do PC ao bloco fracionário, ainda sem prova D3D. Alvos nativos reduzem o número
de fragmentos do miolo por aproximadamente 1,78 e 1,96, respectivamente; não reduzem a memória
alocada, pois usam o canto da textura existente. A composição final continua uma leitura por pixel.
O GL disponível é llvmpipe, em CPU: seus tempos não medem a GPU Xenos. Custo de GPU real fica pendente.

`BT3_FX_OFF` agora preserva os oito bits tanto na leitura do ambiente quanto em SetSettings, dentro
da guarda big-endian e com o original no else. Os bits 32 e 64 entram nos cortes do panorâmico e clima;
128 desenha metade das partículas, arredondando para cima, depois do movimento, como o PC.
Os sete cortes independentes continuam disponíveis. Todos os padrões e FX_OFF=0 ficam como antes.

### Conferência e próxima espera

Os testes C89 da política passaram, incluindo cadeia encerrada, reuso da página, seleção
de fonte nativa, separação de brilho/distância e composição final fora do caminho caro. O teste foi
visto recusando uma cópia que aplicava Sharp também dentro da cadeia. Os 256 valores da máscara
passaram no núcleo dos cortes, e as 128 combinações dos cortes independentes continuaram passando.
O preparador C89 produziu os quatro fontes alterados sem falhas; isso não equivale à compilação XDK.
A build x86bgv do frontend real também ligou com 401 objetos e 104 stubs.

O novo comparador de metadados recusa flag inválida, máscara truncada e lista vazia, em cópias
adulteradas. Ele só prova seleção de passes, não prova os shaders, DATE ou Resolve do D3D real.
A cópia do PC foi recompilada e ligada com 104 stubs. A coleta passa a criar shots antes de executar;
a verificação de presença/tamanho continua obrigatória e já foi vista falhando na retomada anterior.

`blur_fase2.py`, sob uma vaga de `~/vez.sh`, começa pela coleta base do PC para não repetir horas
antes de detectar a mesma falha; depois recompila o commit final, prova objetos/.text, repete o
critério oficial e coleta as seis variantes na x86/PPC, em software e na gravação GPU. Também
exercita FX_OFF=224 no frontend. A coleta PC usa escalas 1 e 2, as oito variantes livres e os sete
cortes. A instrumentação só da cópia descartável permite separar os marcadores e a média na redução;
não se alterou `troca/ref/upstream-main`. O recurso PNG indisponível na SDL local continua explicitamente
fora dessa cópia de diagnóstico, inclusive substituições de arte, sem impedir captura dos alvos do jogo.

O pedido curto está em `~/blur-pedido-console-2.md`; os oito ambientes livres em `~/blur-console-2/`.
Todos têm `BT3_LOG=1`, `BT3_SHOT=300`, sem roteiro. O gerador da fase 1 também ganhou LOG=1.
Resultados locais em `~/blur-analise/fase2/`. Ainda faltam a rodada oficial do novo commit,
o julgamento do alinhamento/capturas por regiões, a compilação XDK e a escolha visual no console.
Não marcar a tarefa concluída por a coleta acabar. O relatório registra o serviço e o marcador.

## Retomada de 05/11: critério aprovado e duas falhas da coleta

A rodada de `f2a1a7c5` terminou com código 1, depois da aprovação oficial.
Os resultados foram preservados em `~/blur-analise/fase2-f2a1a7c5/` antes da nova execução.

| Conferência do commit f2a1a7c5 | Resultado |
|---|---|
| Builds x86/x86r/PPC | 398/399/422 objetos, zero falhas; 1965 structs PPC, zero diferentes |
| Objetos x86 sem depuração | 326 iguais; somente x360_passos.o diferente pelo carimbo |
| .text x86 e base | idênticas, SHA1 94fe2737c0b9f6663ca659685be77a7cd1d36659 |
| SEMMAPA=1 IMG=1 roteiros.sh f5_duelo | código 0, tudo certo; memória, luta e saves aprovados |
| Imagens oficiais | 13/13 pares idênticos |
| PC GL base, escala 1 | seis PPM íntegros, mas cenas incompatíveis; recusados para comparação visual |
| Variantes locais da fase 2 | não iniciadas; build PPC GPU recebeu --vu1 indevidamente |

O coletor agora usa `--gpu --vu1` na x86 e apenas `--gpu` na PPC, com os caminhos
`x86bgv/bt3_x86bgv` e `ppcbg/bt3_ppcbg`, respectivamente. As quatro builds de diagnóstico
foram movidas para antes do roteiro oficial. A chamada PPC corrigida compilou 425 objetos e
ligou sem erro; nessa árvore ainda suja, parou depois pela guarda de origem da referência x86.
A rodada limpa continua obrigada a conferir tabelas/layout antes de executar esse binário.

O contato `fase2-f2a1a7c5/alinhamento-base.png` revelou que o PC estava na História,
enquanto a referência x86 mostrava Duelo na cidade. O rastro adicional em
`alinhamento/pc-1-diagnostico.log` confirmou menu no vblank 2210 e História no 2299,
contra menu no 2873 e Duelo no 3000 da x86. O PC atual acelera o cartão por padrão:
`McFlow_Start` consulta `save_quick`, cujo padrão é 1. A coleta agora escreve
`save_quick=0` em um arquivo de ajustes exclusivo por caso.

O ensaio real com esse ajuste terminou com código 0, seis PPM íntegros e os seis
vblanks da referência: 2974, 3367, 5142, 5916, 6676 e 6798. Menu e seleção são modos
4 e 39; as quatro imagens da luta têm sequência 2, 3, 3 e 3. A inspeção de
`alinhamento/save-lento-contato.png` recuperou o Duelo no cenário esperado. O contato
usa imagens inteiras de 640x448 da x86 e 512x448 do GL, portanto não serve para
subtrair pixels sem antes recortar a área visível e conferir câmera/pose.

A instrumentação só da cópia descartável registra GS, vblank, modo de menu e sequência
de luta no instante de cada captura. O novo verificador exige menu principal, seleção do
Duelo, apresentação e combate nas seis posições, além dos vblanks medidos. Foi visto
recusando cópias com História no lugar do menu, apresentação trocada por combate,
vblank deslocado em um quadro, capturas duplicadas e lista vazia.
Isso impede aceitar as seis imagens erradas da rodada anterior, mas ainda não prova
mesma pose/câmera nem a nitidez das regiões. A nova rodada fica registrada no relatório
de entrega, com seus resultados e limites. O prazo do PC passa a crescer com a área
da escala: 20 minutos em 1 e 80 em 2; llvmpipe é CPU, sem medida transferível ao Xenos.

Nenhum código do produto mudou nesta retomada. Os tratamentos opcionais continuam
desligados por padrão, e o pedido livre para o console continua em `~/blur-pedido-console-2.md`.

## Julgamento de 05/11: coleta extensa concluída, referência Sharp corrigida

O serviço `bt3-blur-analise-fase2-r1.service` encerrou com `Result=success` e código 0.
Resultados de `fcf9986a` em `~/blur-analise/fase2/`, sem `FALHA` e com `COLETA_CONCLUIDA`.
O término foi julgado pelas provas abaixo; não aprova o D3D do 360.

| Conferência | Resultado |
|---|---|
| Builds x86/x86r/PPC | 398/399/422 objetos, zero falhas; PPC: 1965 structs, zero diferentes |
| Objetos x86 sem depuração | 326 iguais; somente x360_passos.o diferente pelo carimbo |
| `.text` x86 contra 1.0.2 | `cmp` idêntico; SHA1 94fe2737c0b9f6663ca659685be77a7cd1d36659 |
| `SEMMAPA=1 IMG=1 roteiros.sh f5_duelo` | código 0, `tudo certo`; memória, luta, saves e rastros aprovados |
| Capturas oficiais | 13/13 pares x86/PPC idênticos |
| Seis variantes em software | 36/36 pares x86/PPC idênticos; todos também iguais à própria base |
| Listas de desenho | sete casos incluindo FX_OFF=224, 8380 quadros cada; x86/PPC iguais, zero flags inválidas |
| PC GL | 180 PPM íntegros: quinze casos, seis momentos, escalas 1 e 2; cenas/vblanks conferidos |
| Repetição PC | 6/6 idênticas à base em cada escala, byte a byte |

A igualdade em software é esperada: o rasterizador de referência não executa os shaders novos.
As listas do backend falso provam a seleção e o transporte do estado. A base não armou nenhuma
flag; c03 marcou 4093 fontes e 4094 destinos nativos; c04 acrescentou 4094 desenhos com blocos;
c05 marcou 12108 blocos, 124275 fontes e 132318 destinos nativos. Isso não é prova de pixels D3D.

### Alinhamento visual e limites

Foi inspecionado `fase2/alinhamento-final.png`: colunas x86 recortada em x=0..511,
PC escala 1 e PC escala 2 reduzida só para contato; linhas menu, seleção, apresentação,
combate, técnica e pausa. Os PPM originais permanecem intactos.

Menu e seleção correspondem às mesmas telas, mas o upstream atual reposiciona elementos
em 16:9. No combate, a cidade, os dois lutadores na mesma pose de impacto, a câmera, o
cronômetro 230 e o dano 590 são compatíveis. Técnica e pausa mostram Kamehameha no tempo
218, mesma pose/câmera e mesma janela de pausa; há diferenças de composição e de renderização.
Não se exige igualdade de pixels entre versões nem se usa essa diferença como nitidez.

**A apresentação no GS 4590 foi recusada para comparar versões:** a x86 já mostra a câmera
atrás de Goku com a imagem escurecida; o PC ainda mostra o close de Kuririn com transição.
Ambos registram sequência 2 e vblank 5142. Portanto, mesmo estado, GS e vblank não bastam
para certificar câmera/pose. As comparações de variantes do PC são contra a própria base
na mesma escala e versão; a apresentação não é usada para validar imagem equivalente à x86.

### Regiões medidas no combate, GS 4977

Medição sem redimensionar, em RGB de 8 bits. Retângulos na coordenada lógica, com limite
superior exclusivo; na escala 2 multiplicam-se as quatro coordenadas por 2:

| Região | Retângulo x0,y0,x1,y1 | Pixels físicos na escala 2 |
|---|---|---:|
| Chão próximo | 16,330,108,430 | 36800 |
| Árvore distante | 275,135,309,201 | 8976 |
| Interior de Kuririn | 148,214,159,234 | 880 |
| Interior de Goku | 240,240,248,250 | 320 |
| Trecho da barra da HUD | 68,17,150,23 | 1968 |

As duas amostras internas não cobrem a silhueta inteira dos lutadores. O trecho da HUD
não permite afirmar que a interface inteira ficou igual. Contagem de pixels alterados
não é uma medida isolada de nitidez nem determina a preferência visual.

| PC escala 2, contra c00 | Chão próximo | Árvore distante | Kuririn | Goku | Barra HUD |
|---|---:|---:|---:|---:|---:|
| c01 blocos macios | 11692 | 8734 | 0 | 0 | 127 |
| c03 distância nativa | 0 | 2746 | 0 | 0 | 0 |
| c04 distância nativa e média | 0 | 2235 | 0 | 0 | 0 |
| c05 conjunto completo | 2335 | 8774 | 0 | 0 | 116 |
| c06 sem distância | 0 | 3986 | 0 | 0 | 0 |
| Sem panorâmico | 31682 | 6172 | 0 | 0 | 0 |
| Sem névoa | 0 | 8894 | 0 | 0 | 684 |
| Sem brilho | 36800 | 8976 | 0 | 0 | 653 |
| Sem clima | 36800 | 8976 | 0 | 0 | 702 |

Na árvore, erro absoluto médio RGB contra a base: c03 0,3020; c04 0,1860;
c06 0,5625. As correções de distância atuam na região distante, sem trocar o chão
próximo nesses recortes. Inspeção ampliada em `longe-base-nativa-media-corte.png`,
colunas base, nativa, média, corte. A diferença é pequena neste estágio, não prova
eliminação de quadrados em todo cenário nem melhora aprovada na arena do Lucas.
O conjunto completo também muda o brilho e não isola a distância.

Na escala 1, c01 a c05 são idênticas à base nos seis quadros, como esperado para
tratamentos da escala alta. Cortar a distância altera 22440 pixels no combate:
954 na árvore, zero nos outros quatro recortes; erro médio da árvore 0,5501.
O radial e o calor/bruma não alteram nenhum dos seis momentos nas duas escalas;
isso só demonstra ausência de contribuição nesses momentos, não descarte geral.
Menu e seleção não mudam com nenhum dos sete cortes no PC.

Dados completos em `medidas-regioes.json`, cálculo em `~/blur-analise/medir-final.py`,
log em `medidas-regioes.log`. O controle de repetição foi visto recusando uma cópia
com um pixel adulterado antes de aceitar as doze repetições reais.

### Falha descoberta no julgamento e repetição limitada

A referência c02 Sharp na escala 2 ficou idêntica à base nos seis quadros. Isso foi
**recusado como prova do tratamento**: o adaptador da cópia descartável condicionava
os próprios marcadores a `DISTANCIA_NATIVA`/`GLOW_NATIVE`. Com ambos em zero, Sharp
não enxergava nenhuma cadeia e cortava também os blocos de dentro dela. O frontend
do 360 conserva os marcadores, portanto as variantes não eram equivalentes.

`preparar_cadeias` agora conserva os quatro marcadores, distingue brilho (0x20/0x21)
de distância (0x22/0x23) e consulta a opção nativa somente em `lowres_now`.
A condição de Sharp continua usando a presença da cadeia. Só a cópia descartável
é instrumentada; nenhum código de produto, padrão ou referência upstream mudou.
A cópia real foi recompilada com 104 stubs e zero linhas de erro.

`teste_blur_pc.py` aplica a instrumentação aos fontes da referência congelada e
compila a política resultante em C89. Passaram 24 combinações de cadeia, opções
nativas e Sharp, incluindo fechamento e alvo inválido. O verificador recusou o
adaptador anterior e uma cópia que selecionava a resolução pela cadeia errada.
O controle da coleta também recusou a referência c02 anterior sem contribuição e
um controle com pixel adulterado, antes de aceitar controles válidos.

`blur_fase2_cadeias.py` repete somente quatro casos PC: base 2, Sharp 1 e 2 e conjunto
completo 2, mais builds, objetos/.text e critério oficial do novo commit. Exige base,
Sharp 1 e conjunto completo idênticos aos anteriores; Sharp 2 precisa exercer a luta
sem alterar menu/seleção. Os demais 156 PPM da coleta anterior ficam preservados;
c02 na escala 2 permanece fora das conclusões até julgar a repetição.
O serviço e o marcador estão no relatório; não repetir as doze horas inteiras sem necessidade.

### Decisão e próximo resultado

O culpado continua sendo a distância conforme a observação do Lucas, apoiada pelo
log do corte. Os recortes do PC mostram a atuação seletiva esperada, mas o console
anterior não tem base capturada e não permite atribuir um erro à profundidade ou ao MSAA.
As correções opcionais continuam propostas de comparação. Nenhuma delas foi promovida
a padrão, nem se alteraram CLUT, intensidade ou meio pixel global.

O pedido livre permanece em `~/blur-pedido-console-2.md`, com oito ambientes em
`~/blur-console-2/`, todos LOG=1 e SHOT=300. `~/blur-resultado-2/` ainda está ausente.
Depois da repetição local e de seu julgamento, se só faltar o console, substituir o
marcador pelo comando `test ! -d /home/codex/blur-resultado-2` e encerrar a sessão.
Compilação XDK, imagem D3D, DATE/EDRAM/Resolve reais, custo de GPU e escolha do Lucas
continuam fora das provas locais. Não se acessou console, XDK, outra worktree ou serviço alheio.
