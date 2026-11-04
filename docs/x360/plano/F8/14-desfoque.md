# F8: diagnóstico da imagem macia no console

Branch `blur-analise`, base 1.0.2 `9c69c4ba`. Referência de PC: `troca/ref/upstream-main`, `48d66c1f`.
Sem console e sem XDK no WSL. A causa visual continua pendente de capturas equivalentes do console.

## Rodada preparada

`python3 port/x360/tools/blur_console.py` gera quinze ambientes e copia o roteiro do Duelo para `~/blur-console`.
O pedido detalhado está em `~/blur-pedido-console.md`. As seis chaves novas, todas com padrão zero, são
`BT3_SEM_DISTANCIA`, `BT3_SEM_PANORAMICO`, `BT3_SEM_RADIAL`, `BT3_SEM_BRUMA`, `BT3_SEM_NEVOA` e `BT3_SEM_BRILHO`.
Elas cortam, respectivamente, `StgFog_Draw`, `StgPanBlur_Draw`, `StgBlur_DrawAll`, `StgHaze_DrawAll`,
`StgDepthTint_Draw` e `GfxPost_DrawGlow`. O panorâmico conserva a atualização das vistas.
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

O núcleo dos cortes passou pelas 64 combinações de chaves em C89 estrito. Uma cópia com a seleção do bit presa
no primeiro efeito foi reprovada pelo mesmo verificador. Os quatro fontes C do console foram preparados pelo
`c89.py` sem falhas. Isso ainda não prova compilação XDK, shaders D3D ou imagem no console.

A variante de medição `BT3_TESTE_DESFOQUE=1 headless.py --alvo x86|ppc` usa pastas `x86b`/`ppcb`.
Só nas cópias geradas abre os seis blocos marcados de diagnóstico, sem definir `PORT_X360` nos headers ou nos
outros caminhos. A build plana conserva o original no `#else` dos fontes do jogo. A variante de diagnóstico
não serve de nova referência da `.text` plana.

A análise do upstream, a tabela de passes e a rodada local serão acrescentadas no próximo commit. O envio
inicial disponibiliza os controles para a compilação do console em paralelo com essa análise.
