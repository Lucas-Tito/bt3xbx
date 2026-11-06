# Conferência da 1.1.0 no console

Propostas para a auditoria do Lucas. Esta tarefa não compila XDK nem acessa o console.
Copiar um modelo como `environment.txt` na pasta de `default.xex`, `default.dat` e
`default.swap`, todos da mesma build. A ISO convertida é `game:\bt3-x360.iso`, encontrada
sem chave BT3_ISO. Preservar saves antes dos roteiros; usar uma pasta de teste separada.

| Modelo | Conferência | Arquivos de roteiro na raiz da pasta do jogo |
|---|---|---|
| livre | Jogar, conferir distância, folhagem, sombras, som e Guide | nenhum |
| duelo | Luta, técnicas e retorno | f5_duelo.txt, boot.txt |
| dividido | Duas portas, técnica cheia e retorno às duas vistas | f7_dividido.txt, f7_dois_inicio.txt, boot.txt |
| historia | Menus de História | historia.txt, boot.txt |
| falha_video | Recusa deliberada do layout antes dos shaders; caixa e dashboard | nenhum |
| caixa_publica | Roteiro deliberadamente ausente; caixa com motivo e dashboard | não copiar release_ausente.txt |

Conferir os `inclui` dos arquivos antes de copiar. Os roteiros não medem a abertura,
por isso usam BT3_NOMOVIE=1. O livre mantém a abertura.

Build pública: `BT3_TESTE_FALHAS=0` ao compilar (padrão). Log, autotestes, vigia e
amostrador ficam desligados no uso normal. O modelo falha_video liga só o log para
habilitar BT3_TESTE_VIDEO, chave de diagnóstico ignorada sem ele. A cor continua preta
nessa build mesmo com log. Depois do OK deve abrir o dashboard e permitir reabrir o jogo.
Trocar o ambiente após o ensaio para não repetir a sabotagem.

Build de teste: compilar com `BT3_TESTE_FALHAS=1`. Nos dois ensaios de falha explícita,
exigir vermelho e saída por um novo botão. Não distribuir essa build. Exceções com
crash.txt gravado continuam usando a caixa Unexpected error já existente.

Se Direct3DCreate9 ou CreateDevice falhar, não existe dispositivo capaz de apresentar
uma caixa do sistema: o único recuo disponível é sair diretamente ao dashboard.
Se o sistema recusar a caixa por 10 s, a build pública também sai. Conferir esse limite
separadamente da recusa do renderer, que conserva o dispositivo da main.

Aprovar os textos de `docs/x360/release-1.1.0.md`, comparar as listras da sombra com o
PS2 e só então preparar o pacote real com as três saídas da mesma build pública e
`pacote.py --estrito`. Nenhuma publicação está autorizada nesta tarefa.
