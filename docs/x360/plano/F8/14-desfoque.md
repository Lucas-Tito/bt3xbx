# F8: diagnóstico da imagem macia no console

Branch `blur-analise`, base 1.0.2 `9c69c4ba`. Referência de PC: `troca/ref/upstream-main`, `48d66c1f`.
Sem console e sem XDK no WSL. A causa visual continua pendente de capturas equivalentes do console.

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

`blur_rodada.py`, sob `flock ~/wsl.lock`, compila x86, x86r e PPC do commit limpo e roda a conferência oficial
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

## Julgamento e proposta conservadora

**Não há culpado comprovado nesta entrega parcial.** A hipótese do FXAA sempre ligado é incompatível com o
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
