# Sonda curta da tela dividida

Preparada no WSL, sem XDK, envio ou execução no console. Nenhum desempenho aprovado.
O gerador exige pasta local nova e um identificador novo para a rodada:

```sh
python3 port/x360/tools/f7_console.py ~/f7e3-console-r001 r001
```

Na revisão Linux, compilar o commit entregue. Levar juntos o `.xex`, `.dat`, `.swap` e a pasta inteira
`port/x360/roteiros` para `game:\roteiros`. Usar a ISO convertida. Não reutilizar os identificadores de uma
rodada anterior: os quatro ambientes gerados apontam para saves distintos. Antes de cada abertura, conferir
que a pasta indicada por `BT3_SAVES` ainda não existe; gerar nova rodada se existir. Não apagar saves do usuário.
Anotar commit, hash dos três arquivos da build, ambiente, ISO e identificador. Copiar só o ambiente escolhido
como `game:\environment.txt`, reiniciar o título e recolher/renomear o log antes de outro caso.

1. `01_simples.txt`: Goku Early/Kuririn, mirante do Kami, 1P/COM, três janelas de 600 passos, separadas por pausa.
2. `02_dividido.txt`: mesma dupla/arena, 1P/2P pelo roteiro, três janelas de 600 passos. A primeira é fria e as
   demais aquecidas na mesma luta. As janelas começam em guarda; há som, sem cortes de escala/efeitos nem folga de vblank.
3. `03_fluxo.txt`: seleção independente, cancelamento do 2P, confirmação simultânea, movimento/carga, técnica de
   investida com vista cheia, retorno dividido, pausa por ambos, resultado dado pelo dano do jogo e volta ao menu.
   Inspecionar HUD e geometria por metade, ausência de mistura entre as tesouras e o menu cheio. Este caso usa
   `termina luta`: não conta como vitória jogando. Fotografias/capturas ficam fora do git. Para capturar, usar uma
   segunda execução com identificador novo e os números dos quadros do log, nunca misturar seu tempo com o da sonda.
4. `04_real.txt`: sem roteiro nem desbloqueio. Ligar primeiro um controle em quadrante diferente de 1; entrar em
   Duelo 1P/2P com dois controles reais. Cada um escolhe/cancela seu personagem e move seu lado. Pausar por ambos,
   desligar/religar cada controle, trocar quadrante com um terceiro se disponível e abrir/fechar Guide na luta.
   O controle reservado deve voltar ao dono, sem botão preso. Encerrar luta, resultado, sair ao menu, salvar nas
   Opções, sair e reabrir com o mesmo ambiente para verificar Continue. Essa reabertura é a única reutilização
   intencional de save, identificada no registro.

Extrair o perfil das linhas `passos:`, `gs:` e `memoria:` por janela: trabalho p50/p95/p99/máximo, tempo da
cadeia/gravação/GPU, imagens na tela de 1/2/3/4+ vblanks, trocas perdidas/puladas, descartes, piso de memória e som.
`passos.py LOG --carimbo 'COMMIT diff:da39a3ee5e'` confere a estrutura e publica as janelas. Conferir também o log
bruto de cada janela, sem somar a pausa ao combate. A comparação 1P/COM contra 1P/2P inclui a troca de COM por humano.

**600 passos são só uma sonda.** Mesmo com zero longas, não cumpre as três janelas de pelo menos 2.700 passos
fria/aquecidas das decisões 27/28 nem a sessão livre. `passos.py --console` conserva esse mínimo e deve recusar esta
sonda como aceite. Publicar toda imagem longa encontrada, as três janelas separadas e os dois lados da captura.
O WSL não mede Xenos/Xenon, apresentação, XAudio2 ou latência física. Se não couber no prazo, levar os números ao
o mantenedor sob as decisões 28/29, sem aprovar exceção ou alterar a imagem por conta própria.
