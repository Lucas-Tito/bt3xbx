# Transições de destruição de cenário

Formato dos arquivos `pzs3us1/00446.bin` e `00447.bin`, ids `0x1BF` e `0x1C0`.
Leitor: `BtlLoad_StepStageChange` (`battle_load.c`), ramo de destino 3 para o primeiro,
outros destinos para o segundo. Não são contêineres BPE e não passam por `Sprite_Unpack`.
Por isso o inventário de `unpacks.py` não detecta sua ausência da tabela de formatos.

## Pacote externo

Tabela `Conv_Tabela`, exatamente três entradas, deslocamentos em bytes a partir do arquivo.
Ambos têm 273.728 bytes. Cabeçalho alinhado até 0x40, câmera em 0x40..0x280, modelo em
0x280..0x2FC00 e `TexFile` em 0x2FC00..0x42D40. A tabela e os limites são validados pelo
conversor, sem depender desses deslocamentos fixos. Enchimento não utilizado deve ser zero.

| Entrada | Leitor | Formato e conversão |
| --- | --- | --- |
| 1 | `EftBurst_Init`, `EftCam_Start` | CMAn, animação de câmera já descrita em `personagem.md` 5.12; `Conv_Camera` |
| 2 | `EftBurst_RelocateModel`, `EftBurst_DrawModel` | modelo cmdl abaixo |
| 3 | `EftBurst_Start`, inicializadores das partículas | `TexFile` normal, 7 texturas, `Conv_TexFile`; deslocamentos em palavras |

## Modelo cmdl

Cabeçalho com assinatura de quatro bytes, contagem de grupos u32 em +4, deslocamento de grupos em +8,
contagem de texturas u32 em +12, deslocamento de texturas em +16 e sinal de relocação u32 em +20.
Esse sinal deve ser zero no disco. `EftBurst_RelocateModel` o põe em 1, soma a base aos ponteiros de grupos,
texturas, vértices e triângulos e grava os ponteiros de imagem e paleta. Uma segunda chamada não repete essa soma.

Os dois modelos têm 2 grupos e 2 texturas. A tabela de texturas está em +0x40, grupos em +0x22340.
Cada grupo ocupa 0x18: flags u32, índice de textura s32, deslocamento de vértices u32,
deslocamento de triângulos u32, escala f32, quantidade de triângulos s16, quantidade de vértices u16.
O último campo é chamado `unk16` no header do leitor; nos dois arquivos corresponde exatamente ao tamanho
do vetor de vértices até o início dos triângulos. O conversor também exige índices dentro desse vetor.
O primeiro grupo tem 821 vértices e 1560 triângulos; o segundo, 56 e 90.

Vértice: três s16. Triângulo: três s16 de índice, seis s16 de normal e seis u16 de cor, 0x1E bytes.
As cores são RG e BA numéricos: `DrawModel` separa os componentes com máscaras e deslocamentos.
Converter cada u16 conserva esses valores; tratá-las como RGBA de bytes daria a ordem errada.

A textura do modelo ocupa 0x40 e tem os campos de `TexEntry`, mas os dois deslocamentos iniciais são em
**bytes**, somados diretamente à base do modelo. Os tamanhos em +8/+12 limitam os pacotes.
Os números até +0x24 são u32, +0x28..+0x2F são bytes, +0x30 é TEX0 u64,
+0x38/+0x3C são ponteiros u32 gravados na relocação. Os pacotes usam `Conv_PacoteImagem`.
Não confundir com a entrada 3, que usa os deslocamentos em palavras de `TexFile`.

## Conferência

`teste_conv_transicao.py CONV_TOOL DADOS` converte os dois arquivos e confere valores nativos e cobertura.
Em cópias fora do repositório, corrompe 12 campos por arquivo e exige erro: tabela, fim, assinatura,
contagem de grupos, deslocamento de grupos, sinal de relocação, textura do grupo, vértices,
índice de triângulo, imagem do modelo, contagem e tabela do `TexFile`. A origem é conferida intacta.
`teste_transicao.py` compila o caso dos leitores reais para x86/PPC. Executá-lo sob a trava do emulador.
O caso não comprova o combate, a GPU nem a volta para o dashboard.

Para a rodada anterior à correção, guardar o conversor antigo e os dados crus. O `--disco` só percorre os
formatos cadastrados: passar com todos os cadastrados convertidos não significa cobrir todos os leitores.

O censo dos 68.601 arquivos das três partições, exigindo tabela externa válida de três entradas,
primeira assinatura CMAn e segunda cmdl, encontrou exatamente esses dois pacotes.
O teste integral da ISO versão 3 passou: 3.327 arquivos e 712 conteúdos expandidos sem erro,
4.039 conversões com cobertura integral. A ISO original permaneceu intacta.
