#!/usr/bin/env bash
# F4, etapa 4 (no WSL, na raiz): os roteiros de controle (port/x360/roteiros, BT3_ROTEIRO) contra as referências e
# contra o inventário. Pede as builds x86 e x86r (headless.py --alvo x86 [--rastreio]); com PPC=1, também a ppc.
#   roteiros.sh [NOME...]     (sem nomes: os do caminho essencial: duelo duelo_time treino opcoes; "todos": também
#                             historia ultimate torneio evolution datacenter referencia shenlong, da etapa 9)
# Para cada roteiro:
#   1. a x86 duas vezes: o rastro (as trocas de tela e as esperas cumpridas, com o vblank) tem de sair igual nas duas
#      e igual à referência guardada em ~/referencias/NOME.telas (fora do repositório; a primeira rodada a grava, e
#      REF=1 a regrava);
#   2. a x86 sob o gdb: cada Sprite_Unpack (vblank, modo, endereço de retorno), casado com a lista do unpacks.py;
#   3. a x86r com o rastreio (BT3_RASTREIO_BITS=24), o mapa (rastreio.py) e o conv_tool --mapa sobre o que a rodada
#      leu: o "sem conversor" por conteúdo tem de ser o das chamadas pendentes alcançadas (roteiros.py confere);
#   4. com PPC=1: a PPC no qemu com os arquivos que têm conversor convertidos, o mapa no resto (BT3_MAPA, como o
#      F1_COM_MAPA do f1.sh) e a guarda só anotando (BT3_GUARDA=anota): o rastro tem de ser o começo do da x86, e a
#      guarda nomeia cada chamada sem conversor. O mapa por posição não passa de todos os menus (o Flash lê registros
#      com duas larguras, guia, seção 6): é a ferramenta para conferir a guarda, não o critério. Desde a etapa 5a ela
#      passa do menu principal (convertido), anota o DuelMenu_Init ou o Train_Init e trava no Flash da tela seguinte,
#      até o TP esgotar (uma hora por roteiro): use PPC=1 só quando precisar;
#   5. com SEMMAPA=1 (F4, etapa 5, o critério): a PPC sem mapa, só com os conversores, como o 360 (os arquivos pelo
#      conv_tool --saida, o conteúdo descomprimido pelos ganchos, sem BT3_MAPA, a guarda fatal), com as leituras
#      (BT3_LEU=1). roteiros.py semmapa: ou ela chega ao fim com o rastro, os arquivos lidos e a luta iguais aos da
#      x86, ou para na guarda, que nomeia o próximo bloqueio, com o rastro e os arquivos lidos até ali iguais ao
#      começo dos da x86; qualquer outro fim é falha. A memória das duas no último vblank antes do passo fim
#      (BT3_HASH_AT, uma rodada a mais da x86): o heap no lugar, cada bloco vivo com um arquivo ou conteúdo lido
#      conforme número a número pelas marcas do conversor (memdiff.py --marcas-vivas) e nenhuma palavra diferente
#      fora deles (roteiros.py memoria). MEM=menu compara no último vblank da primeira visita ao menu principal (o
#      critério da etapa 5a), e aí a memória se julga mesmo com a PPC parando depois. Sem MEM=menu, a memória também
#      no último vblank de cada tela de menu do essencial e da História visitada (38, 39, 40, 44, 45, 62; 6 a 10:
#      roteiros.py TELAS_MEM), com
#      os conteúdos convertidos da tela vivos e conformes (uma rodada a mais da PPC e da x86 por tela).
# Antes de tudo, as builds: x86, x86r e ppc do commit de agora, sem nada fora de commit (o CARIMBO de cada uma), e a
# árvore sem alteração; senão sai com 1. DESENVOLVIMENTO=1 roda assim mesmo, e o fim diz "NÃO VALE COMO CRITÉRIO".
# No fim, as chamadas de todos os roteiros juntos contra o inventário (roteiros.py soma).
# Sai com 1 se qualquer conferência falhar em qualquer roteiro (a x86 sem "fim" ou com FALHA, a repetição, a
# referência, a x86r, o rastreio.py, o conv_tool, o confere, a PPC, a soma), com a lista no fim; 0 só com tudo certo.
#   6. com IMG=1 (F4, etapa 6): as imagens das telas pelo rasterizador de software, na x86 e na PPC sem mapa, no mesmo
#      momento do roteiro (ancorado numa linha do rastro: roteiros.py imagens), idênticas byte a byte (roteiros.py
#      compara_imagens; a diferença diz a tela, o quadro e quantos pixels). As imagens ficam em $O/NOME/img (no WSL,
#      nunca no repositório). JANELA=8: quadros pintados antes de cada imagem (port/x360/headless/imagens.c).
#      A luta entra também, desde a F5, etapa 4 (idêntica desde a correção das cores dos efeitos, plano/F5/04.md;
#      decisão do Lucas, 16/10/2026); IMG=menus deixa a luta de fora, como até a F4 (IMG=luta continua aceito)
#   7. (F5, etapa 3) nos roteiros f5_*: roteiros.py f5 (o que a F5 exige: a luta montada, cada ação confirmada no
#      estado do jogo e na matriz, o resultado e a volta ao menu, a janela contínua, os pontos) e passos.py (a medida de
#      tempo por passo) no log da x86 e no da PPC sem mapa, os dois do commit de agora
#   Nos f5_*, a memória de cada "marca ponto" é obrigatória (F5, etapa 5).
#   8. (F6, etapa 2) com SOM=1: a rede 2, o rastro de som (BT3_SOM, port/x360/headless/som.c) das duas rodadas da x86
#      igual byte a byte e, com SEMMAPA=1, o da PPC sem mapa igual ao da x86 (som.py rastro; o começo dele se a PPC
#      parou na guarda); e a rede 4, o dispositivo gravador (as builds x86a e ppca, headless.py --gravador, com
#      BT3_SOUND=1): o rastro de telas e o de som da x86a iguais aos da x86 (abrir o dispositivo não muda o jogo) e, com
#      SEMMAPA=1, a gravação da ppca igual à da x86a (som.py mixer: o PCM do ADX exato, a mistura dos efeitos dentro de
#      1 LSB, as vozes ativas por bloco). As gravações são grandes: apagadas depois de comparadas, menos com GUARDA_SOM=1
#   TP=3600     segundos da PPC no qemu
#   MEM=menu    a memória no menu principal em vez do fim (SEMMAPA=1)
#   REUSA=1     não roda nada do jogo: refaz as conferências sobre as saídas já guardadas em $O/NOME (para testar os
#               verificadores; a x86 tem de ser a mesma build que gravou o gdb.txt, pelo addr2line)
#   REUSA=x86   reaproveita só as rodadas da x86 e da x86r (e o mapa); roda as da PPC (PPC=1, SEMMAPA=1) de novo: depois
#               de mudar só conversores, ganchos (em PORT_BIG_ENDIAN) ou o port/x360, com a .text da x86 conferida igual
#   O=pasta     onde ficam as saídas (port/build/roteiros); REFS=pasta, as referências (~/referencias)
# RAPIDO=0 conserva as rodadas separadas para a prova de equivalência.
# O padrão junta as memórias e as imagens na PPC normal e numa passagem adicional da x86.
set -uo pipefail
# Toda simbolização usa o DWARF da build local, sem consultar servidores de depuração do host.
export DEBUGINFOD_URLS=
B=port/build; O=${O:-$B/roteiros}; REFS=${REFS:-~/referencias}
FALHAS=()
falhou() { FALHAS+=("$n: $*"); echo "   >> FALHA: $*"; }
export BT3_GS=none BT3_SETTINGS= BT3_SOUND_TICKS=1 BT3_NOMOVIE=1 BT3_MENU_ITEM4=0 BT3_DESBLOQUEIO=1
NOMES=("$@"); [ ${#NOMES[@]} -gt 0 ] || NOMES=(duelo duelo_time treino opcoes)
[ "${NOMES[0]}" != todos ] || NOMES=(duelo duelo_time treino opcoes historia historia_novo historia_perde ultimate missao sobrevivencia simdragon discfusion torneio evolution datacenter \
    referencia shenlong)
mkdir -p $O $REFS
gcc -std=c89 -O2 -w -o $O/conv_tool $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1
cat > $O/unpack.gdb <<'G'
set pagination off
set confirm off
dprintf Port_FileReadAt,"LEU %s\n", rel
break *Sprite_Unpack
commands
silent
printf "UNPACK %u %d 0x%x\n", gPortVBlanks, *(int*)(*(int*)&gProgress + 0x18), *(int*)$esp
continue
end
run
G
echo "x86 $(cat $B/x86/CARIMBO) | x86r $(cat $B/x86r/CARIMBO)$([ -z "${PPC:-}${SEMMAPA:-}${IMG:-}" ] || echo " | ppc $(cat $B/ppc/CARIMBO)")"
# As builds comparadas têm de sair do mesmo commit, o de agora, sem nada fora de commit (o carimbo do headless.py:
# "COMMIT diff:HASH", e o HASH da diferença vazia é da39a3ee5e), e a árvore daqui sem alteração: a revisão da etapa 5
# achou a tabela final feita com a x86 de um estado e a PPC de outro que não estava em commit nenhum. DESENVOLVIMENTO=1
# deixa rodar assim, e o fim diz que a rodada não vale como critério.
CARIMBOS=(x86 x86r); [ -z "${PPC:-}${SEMMAPA:-}${IMG:-}" ] || CARIMBOS+=(ppc)
[ -z "${SOM:-}" ] || CARIMBOS+=(x86a); [ -z "${SOM:-}" ] || [ -z "${SEMMAPA:-}" ] || CARIMBOS+=(ppca)
AGORA=$(git rev-parse --short HEAD)
ORIGEM=()
for b in "${CARIMBOS[@]}"; do
    c=$(head -1 $B/$b/CARIMBO 2>/dev/null | cut -d' ' -f1,2)
    [ "$c" = "$AGORA diff:da39a3ee5e" ] || ORIGEM+=("$b é de \"${c:-sem carimbo}\", não do commit de agora ($AGORA) sem diferença")
done
[ -z "$(git status --porcelain --untracked-files=normal)" ] || ORIGEM+=("a árvore tem alterações fora de commit")
if [ ${#ORIGEM[@]} -gt 0 ]; then
    printf '   >> origem: %s\n' "${ORIGEM[@]}"
    if [ -z "${DESENVOLVIMENTO:-}" ]; then
        echo "== builds de origens diferentes ou fora de commit: refaça x86, x86r e ppc do HEAD (ou DESENVOLVIMENTO=1)"
        exit 1
    fi
fi
LOGS=()
# a largura de cada byte das globais da x86 pelo tipo do DWARF, para o memdiff comparar as globais campo a campo
# (memdiff.py --tipos; sem isso um u16 ou um byte numa palavra saíam como classe ambígua, sem julgamento)
if [ -n "${SEMMAPA:-}" ]; then
    python3 port/x360/tools/campos.py --globais $B/x86/bt3_x86 $O/globais > $O/globais.txt 2>&1
    grep -q '^GLOBAIS ' $O/globais.txt && [ -s $O/globais.tsv ] || { echo "campos.py --globais falhou:"; cat $O/globais.txt; exit 1; }
fi
# (o que roda de novo: com REUSA=1 nada; com REUSA=x86 só a PPC)
roda_x86() { [ -z "${REUSA:-}" ]; }
roda_ppc() { [ "${REUSA:-}" != 1 ]; }
# Medição de cada execução, inclusive gdb e donos. Não muda o ambiente do programa.
timeout() {
    /usr/bin/time -a -o "$D/tempos.tsv" -f "${BT3_SAVES:-sem-cartao}\t%e" timeout "$@"
}
for n in "${NOMES[@]}"; do
    JUNTA=0
    D=$O/$n; roda_x86 && { rm -rf $D; mkdir -p $D; }
    R=port/x360/roteiros/$n.txt
    echo "== $n"
    for v in a b; do
        if roda_x86; then
            BT3_SOM=${SOM:+$D/som_$v.txt} BT3_ROTEIRO=$R BT3_SAVES=$D/s$v BT3_HASH=$D/h$v.txt timeout 900 $B/x86/bt3_x86 > $D/x86_$v.txt 2>&1
            r=$?
        else
            r=$(grep -aq '^roteiro: .* fim ' $D/x86_$v.txt && echo 0 || echo "?")
        fi
        echo "   x86 ($v) saiu com $r: $(grep -a '^roteiro: .* fim \|FALHA' $D/x86_$v.txt | head -1)"
        { [ "$r" = 0 ] && grep -aq '^roteiro: .* fim ' $D/x86_$v.txt && ! grep -aq 'FALHA' $D/x86_$v.txt; } ||
            falhou "x86 ($v) sem chegar ao fim do roteiro"
        python3 port/x360/tools/roteiros.py telas $D/x86_$v.txt > $D/telas_$v.txt
    done
    echo "   repetição: $(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_b.txt | tr '\n' ' ')"
    python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_b.txt > /dev/null || falhou "repetição diferente"
    if [ -n "${SOM:-}" ]; then
        # (F6, etapa 2, rede 2) o rastro de som das duas rodadas da x86
        python3 port/x360/tools/som.py rastro $D/som_a.txt $D/som_b.txt > $D/som_xx.txt
        r=$?; sed 's/^/   /' $D/som_xx.txt
        [ $r = 0 ] || falhou "som: o rastro de som das duas rodadas da x86 diferente (rede 2)"
        # (rede 4) a x86 com o dispositivo gravador: o jogo igual (telas e rastro de som) e a gravação, para a ppca.
        # O cartão dela começa vazio, numa pasta só dela: o boot.txt faz o New Game, que só existe com o cartão vazio, e
        # a pasta sa já tem o save da rodada (a) (com ela a x86a carregava o save e ficava 4000 quadros sem o mcflow 10)
        roda_x86 && rm -rf $D/sxa
        roda_x86 && BT3_SOUND=1 BT3_GRAVADOR=$D/grav_x.bin BT3_SOM=$D/som_xa.txt BT3_ROTEIRO=$R BT3_SAVES=$D/sxa \
            BT3_HASH=$D/hxa.txt timeout 900 $B/x86a/bt3_x86a > $D/x86a.txt 2>&1
        python3 port/x360/tools/roteiros.py telas $D/x86a.txt > $D/telas_xa.txt
        python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_xa.txt > /dev/null ||
            falhou "som: a x86 com o gravador com outro rastro de telas (abrir o dispositivo mudou o jogo)"
        cmp -s $D/som_a.txt $D/som_xa.txt || falhou "som: a x86 com o gravador com outro rastro de som"
        echo "   x86a (gravador): $(grep -a '^roteiro: .* fim \|FALHA' $D/x86a.txt | head -1 | cut -c1-80); $(stat -c %s $D/grav_x.bin 2>/dev/null) bytes gravados"
        [ -n "${SEMMAPA:-}" ] || [ -n "${GUARDA_SOM:-}" ] || rm -f $D/grav_x.bin
    fi
    if [ ! -s $REFS/$n.telas ] || [ -n "${REF:-}" ]; then
        cp $D/telas_a.txt $REFS/$n.telas
        echo "   referência gravada: $REFS/$n.telas ($(grep -c '^tela ' $REFS/$n.telas) trocas de tela)"
    else
        echo "   referência: $(python3 port/x360/tools/roteiros.py compara $REFS/$n.telas $D/telas_a.txt | tr '\n' ' ')"
        python3 port/x360/tools/roteiros.py compara $REFS/$n.telas $D/telas_a.txt > /dev/null ||
            falhou "rastro diferente da referência $REFS/$n.telas"
    fi
    grep '^tela ' $D/telas_a.txt | cut -d' ' -f2,4 | tr '\n' ' ' | sed 's/^/   telas (vblank): /'; echo
    # (F5, etapa 3) os roteiros da F5: o que cada um tem de alcançar (roteiros.py f5: a luta montada, cada ação
    # confirmada no estado do jogo e na matriz, o resultado e a volta ao menu, a janela contínua, os pontos) e a medida
    # de tempo por passo (passos.py: cada janela com as três linhas), no log da x86 deste commit (a linha "passos:
    # origem" igual ao commit de agora sem diferença, com o sufixo do wsl.sh enviar só se for "(0 alterados, 0
    # apagados)": um log de outra build é recusado; passos.py mesma_origem)
    if [[ $n == f5_* ]]; then
        python3 port/x360/tools/roteiros.py f5 $n $D/x86_a.txt --carimbo "$AGORA diff:da39a3ee5e" > $D/f5.txt
        r=$?; sed 's/^/   /' $D/f5.txt
        [ $r = 0 ] || falhou "f5: o roteiro não alcançou o que a F5 exige (roteiros.py f5)"
        python3 port/x360/tools/passos.py $D/x86_a.txt --carimbo "$AGORA diff:da39a3ee5e" > $D/passos.txt
        r=$?; sed 's/^/   /' $D/passos.txt
        [ $r = 0 ] || falhou "passos: a medida de tempo por passo incompleta (passos.py)"
    fi
    roda_x86 && BT3_ROTEIRO=$R BT3_SAVES=$D/sg BT3_HASH=$D/hg.txt timeout 900 gdb -batch -nx -x $O/unpack.gdb \
        --args $B/x86/bt3_x86 > $D/gdb.txt 2>&1
    grep -aq '^roteiro: .* fim ' $D/gdb.txt || falhou "x86 sob o gdb sem chegar ao fim do roteiro"
    LOGS+=($D/gdb.txt)
    echo "   Sprite_Unpack alcançados:"; python3 port/x360/tools/roteiros.py chamadas $D/gdb.txt --prog $B/x86/bt3_x86
    roda_x86 && BT3_RASTREIO_BITS=24 BT3_ROTEIRO=$R BT3_SAVES=$D/sr BT3_HASH=$D/hr.txt \
        BT3_RASTREIO=$D/rastreio.tsv timeout 3600 setarch -R $B/x86r/bt3_x86r > $D/x86r.txt 2>&1
    echo "   x86r: $(grep -a '^rastreio:' $D/x86r.txt)"
    python3 port/x360/tools/roteiros.py telas $D/x86r.txt > $D/telas_r.txt
    echo "   x86r contra x86: $(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_r.txt | tr '\n' ' ')"
    python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_r.txt > /dev/null || falhou "x86r com outro rastro"
    # (o rastreio.py guarda tudo na memória, e os 13 milhões de posições da Ultimate Battle não cabem nos 8 GB do WSL:
    # o rastreio vai em partes, cada arquivo inteiro numa só, e os mapas das partes se juntam; e um roteiros.sh por vez)
    if roda_x86; then
        rm -rf $D/partes; python3 port/x360/tools/roteiros.py partes $D/rastreio.tsv $D/partes | sed 's/^/   /'
        ok=1; : > $D/mapa.txt
        for p in $D/partes/parte_*.tsv; do
            python3 port/x360/tools/rastreio.py mapa $p -o ${p%.tsv}.mapa --prog $B/x86r/bt3_x86r >> $D/mapa.txt 2>&1 || ok=
        done
        if [ -z "$ok" ]; then falhou "rastreio.py mapa saiu com erro (memória?): $(tail -1 $D/mapa.txt)"; continue; fi
        cat $D/partes/*.mapa | LC_ALL=C sort -t$'\t' -k1,1 -k2,2n > $D/mapa.tsv
        cat $D/partes/*.mapa.conflitos | LC_ALL=C sort -t$'\t' -k1,1 -k2,2n > $D/mapa.tsv.conflitos
    fi
    # o que o rastreio viu ler como número e todo arquivo aberto (gdb): a PPC do passo 4 lê todos, e um que tem
    # conversor e chegasse sem conversão pararia o inverter.c (00001 a 00005 só se leem como número na luta)
    roda_x86 && { cut -f1 $D/rastreio.tsv; grep -a '^LEU ' $D/gdb.txt | cut -d' ' -f2; } |
        grep -v -e '^ELF:' -e '^disc/' | sort -u > $D/lidos.txt
    # (com --saida: os arquivos que têm conversor, convertidos para a PPC do passo 4, como no f1.sh)
    # (os convertidos de agora: o conv_tool --saida acrescenta à lista, e um REUSA a duplicaria)
    rm -rf $D/dados/mods; mkdir -p $D/dados/mods; for d in gamedata/*; do [ -e $D/dados/$(basename $d) ] || ln -s "$(realpath "$d")" $D/dados/; done
    $O/conv_tool --mapa $D/mapa.tsv --saida $D/dados/mods gamedata $(cat $D/lidos.txt) > $D/conv.txt
    r=$?
    touch $D/dados/mods/convertidos.txt
    echo "   conv_tool --mapa saiu com $r ($(wc -l < $D/lidos.txt) arquivos e conteúdos lidos)"
    # (a saída 1 só por posições "com outra largura" que estão certas, uma a uma em LARGURA_CONHECIDA do roteiros.py:
    # um s32 que o jogo lê pela metade baixa, e só pelas funções da entrada no rastreio desta rodada; F4, etapa 9a)
    [ $r = 0 ] || { [ $r = 1 ] && python3 port/x360/tools/roteiros.py larguras $D/conv.txt $D/rastreio.tsv \
        --prog $B/x86r/bt3_x86r | sed 's/^/   /' &&
        [ "${PIPESTATUS[0]}" = 0 ]; } || falhou "conv_tool --mapa saiu com $r"
    # (o "sem conversor" de cada arquivo e conteúdo, o do que não é de menu inclusive, o confere julga logo abaixo)
    grep -v -e "sem conversor$" -e ': convertido; cobertura [0-9]* de [0-9]* bytes (100.0%)' \
        -e ' iguais, 0 com outra largura, 0 não trocadas$' $D/conv.txt | sed 's/^/   /' | head -10
    python3 port/x360/tools/roteiros.py confere $D/gdb.txt --prog $B/x86/bt3_x86 --conv $D/conv.txt > $D/confere.txt
    r=$?
    sed 's/^/   /' $D/confere.txt
    [ $r = 0 ] || falhou "confere: o sem conversor do --mapa diferente do inventário"
    if [ -n "${PPC:-}" ]; then
        # (o cartão de cada rodada começa vazio: com REUSA=x86 a pasta do roteiro não se apaga, e o save da rodada
        # anterior faria o boot carregá-lo, com outro rastro)
        roda_ppc && rm -rf $D/sp
        roda_ppc && BT3_MAPA=$D/mapa.tsv BT3_DATA=$D/dados BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt \
            BT3_GUARDA=anota BT3_ROTEIRO=$R BT3_SAVES=$D/sp BT3_HASH=$D/hp.txt timeout ${TP:-3600} qemu-ppc $B/ppc/bt3_ppc \
            > $D/ppc.txt 2>&1
        echo "   ppc com mapa: $(grep -a '^roteiro: .* fim \|FALHA\|abort' $D/ppc.txt | head -1)"
        python3 port/x360/tools/roteiros.py telas $D/ppc.txt > $D/telas_p.txt
        echo "   ppc contra x86: $(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_p.txt | tr '\n' ' ')"
        # hoje a PPC para no Flash do menu principal (acima): o rastro dela tem de ser o começo do da x86, ou ele inteiro
        # (sem pipe: com o pipefail, a saída 1 do compara, "DIFERENTES" num começo, contaria como falha)
        cmp_p=$(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $D/telas_p.txt)
        grep -q -e '^iguais' -e 'do mais curto são o começo do outro' <<< "$cmp_p" || falhou "PPC com outro rastro que a x86"
        [ "$(wc -l < $D/telas_p.txt)" -le "$(wc -l < $D/telas_a.txt)" ] || falhou "PPC com rastro mais longo que a x86"
        grep -a '^guarda: ' $D/ppc.txt | sed 's/ (retorno.*//' | sort | uniq -c | sed 's/^/   /'
    fi
    if [ -n "${SEMMAPA:-}" ]; then
        # (a memória das duas builds no último vblank antes do passo fim, o critério da etapa 5; com MEM=menu, no
        # último vblank da primeira visita ao menu principal, o da etapa 5a)
        if [ "${MEM:-fim}" = menu ]; then
            V=$(python3 port/x360/tools/roteiros.py vblank_menu $D/telas_a.txt) || V=-1
        else
            V=$(python3 port/x360/tools/roteiros.py vblank_fim $D/telas_a.txt) || V=-1
        fi
        JUNTA=0
        if [ "${RAPIDO:-1}" = 1 ] && [ "${MEM:-fim}" != menu ]; then
            JUNTA=1
            PONTOS=(); [[ $n != f5_* ]] || PONTOS=(--pontos)
            python3 port/x360/tools/roteiros.py vblanks_mem $D/telas_a.txt "${PONTOS[@]}" > $D/mem_telas.txt ||
                falhou "vblanks_mem sem as telas"
        fi
        COLETA=(); DESENHO=()
        if [ "$JUNTA" = 1 ] && roda_ppc; then
            : > $D/memorias_x86.txt; : > $D/memorias_ppc.txt
            while read -r t VT; do
                M=$D/mem_$t
                rm -rf $M; mkdir -p $M
                printf '%s %s/hm.txt\n' "$VT" "$M" >> $D/memorias_x86.txt
                printf '%s %s/hq.txt\n' "$VT" "$M" >> $D/memorias_ppc.txt
            done < $D/mem_telas.txt
            if [ -n "${IMG:-}" ]; then
                LUTAARG=(--luta); [ "$IMG" != menus ] || LUTAARG=()
                python3 port/x360/tools/roteiros.py imagens $D/telas_a.txt "${LUTAARG[@]}" > $D/imagens.txt
                DESENHO=(BT3_GS=1 BT3_GS_FROM=1000000000 BT3_IMAGENS_JANELA=${JANELA:-8})
                for b in x86 ppc; do
                    rm -rf $D/img/$b; mkdir -p $D/img/$b
                    cp $D/imagens.txt $D/img/$b/lista.txt
                done
            fi
        fi
        if roda_ppc; then
            rm -rf $D/hm.txt* $D/hq.txt* $D/sq $D/sm
            [ "$JUNTA" != 1 ] || [ ! -s $D/memorias_ppc.txt ] || COLETA=(BT3_MEMORIAS=$D/memorias_ppc.txt)
            [ "$JUNTA" != 1 ] || [ -z "${IMG:-}" ] || COLETA+=(BT3_IMAGENS=$D/img/ppc/lista.txt)
            env "${COLETA[@]}" "${DESENHO[@]}" BT3_SOM=${SOM:+$D/som_p.txt} BT3_DATA=$D/dados BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_LEU=1 \
                BT3_ROTEIRO=$R BT3_SAVES=$D/sq BT3_HASH=$D/hq.txt BT3_HASH_AT=$V \
                /usr/bin/time -a -o "$D/tempos.tsv" -f 'ppcsm\t%e' timeout ${TP:-3600} qemu-ppc $B/ppc/bt3_ppc \
                > $D/ppcsm.txt 2>&1
            COLETA=()
            [ "$JUNTA" != 1 ] || [ ! -s $D/memorias_x86.txt ] || COLETA=(BT3_MEMORIAS=$D/memorias_x86.txt)
            [ "$JUNTA" != 1 ] || [ -z "${IMG:-}" ] || COLETA+=(BT3_IMAGENS=$D/img/x86/lista.txt)
            env "${COLETA[@]}" "${DESENHO[@]}" BT3_ROTEIRO=$R BT3_SAVES=$D/sm BT3_HASH=$D/hm.txt BT3_HASH_AT=$V \
                /usr/bin/time -a -o "$D/tempos.tsv" -f 'x86mem\t%e' timeout 900 $B/x86/bt3_x86 > $D/x86mem.txt 2>&1
            if [ "$JUNTA" = 1 ]; then
                while read -r t VT; do cp $D/ppcsm.txt $D/mem_$t/ppc.txt; done < $D/mem_telas.txt
                if [ -n "${IMG:-}" ]; then
                    cp $D/ppcsm.txt $D/img/ppc/saida.txt
                    cp $D/x86mem.txt $D/img/x86/saida.txt
                fi
            fi
        fi
        # quem alocou cada bloco vivo nesse vblank e o tipo que pediu (Heap_Alloc(sizeof(T))): o memdiff compara esses
        # blocos campo a campo (--donos). A x86 sob o gdb até o vblank; com REUSA=1, só se ainda não houver
        if roda_ppc || [ ! -s $D/donos.tsv ]; then
            rm -rf $D/sd $D/donos.*
            BT3_ROTEIRO=$R BT3_SAVES=$D/sd BT3_HASH=$D/hd.txt timeout 1800 python3 port/x360/tools/campos.py --donos \
                $B/x86/bt3_x86 $V --tipos $D/donos > $D/donos.txt 2>&1 < /dev/null
        fi
        # memdiff no vblank V: o heap em uso pareado pelos blocos, e cada bloco vivo que guarda um arquivo ou um
        # conteúdo lido na rodada (lidos.txt), número a número pela largura que o conversor deu (--marcas-vivas, o
        # bloco achado pelo conteúdo, o .orig ao lado das marcas); sem os bytes: o sed corta o hexa das linhas de
        # trecho e deixa a região e o deslocamento. O semmapa julga (roteiros.py memoria)
        rm -rf $D/marcas $D/memdiff.txt; mkdir -p $D/marcas
        if [ -s $D/hq.txt.dump ] && [ -s $D/hm.txt.dump ]; then
            $O/conv_tool --marcas $D/marcas gamedata $(cat $D/lidos.txt) > /dev/null
            python3 port/x360/tools/memdiff.py $D/hm.txt.dump $D/hq.txt.dump $B/x86/bt3_x86 $B/ppc/bt3_ppc \
                --so-em-uso --marcas-vivas $D/marcas --tipos $O/globais --donos $D/donos --max 400 | sed 's/  x86 [0-9a-f]\{8\} .*ppc [0-9a-f ]*$//' \
                > $D/memdiff.txt
            echo "memdiff saiu com ${PIPESTATUS[0]}" >> $D/memdiff.txt
        fi
        # (com MEM=menu o semmapa não recebe a memória: uma PPC que chegasse ao fim não está provada nele)
        MD=(--mem $D/memdiff.txt); [ "${MEM:-fim}" != menu ] || MD=()
        python3 port/x360/tools/roteiros.py semmapa $D/ppcsm.txt $D/x86_a.txt $D/gdb.txt --hash $D/ha.txt $D/hq.txt \
            "${MD[@]}" > $D/semmapa.txt
        r=$?
        sed 's/^/   /' $D/semmapa.txt
        [ $r = 0 ] || falhou "PPC sem mapa diferente da x86 (ou parou sem nome)"
        if [ -n "${SOM:-}" ]; then
            # (F6, etapa 2, rede 2) o rastro de som da PPC sem mapa igual ao da x86 (o começo dele se ela parou na guarda)
            python3 port/x360/tools/som.py rastro $D/som_a.txt $D/som_p.txt --prefixo > $D/som_xp.txt
            r=$?; sed 's/^/   /' $D/som_xp.txt
            [ $r = 0 ] || falhou "som: o rastro de som da PPC sem mapa diferente do da x86 (rede 2)"
            # (rede 4) a ppca com o gravador, nos mesmos arquivos convertidos, contra a x86a
            if roda_ppc; then
                rm -rf $D/sqa
                BT3_SOUND=1 BT3_GRAVADOR=$D/grav_p.bin BT3_SOM=$D/som_pa.txt BT3_DATA=$D/dados \
                    BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_ROTEIRO=$R BT3_SAVES=$D/sqa BT3_HASH=$D/hqa.txt \
                    timeout ${TP:-3600} qemu-ppc $B/ppca/bt3_ppca > $D/ppca.txt 2>&1
            fi
            echo "   ppca (gravador): $(grep -a '^roteiro: .* fim \|FALHA\|guarda' $D/ppca.txt | head -1 | cut -c1-80)"
            cmp -s $D/som_p.txt $D/som_pa.txt || falhou "som: a PPC com o gravador com outro rastro de som"
            python3 port/x360/tools/som.py mixer $D/grav_x.bin $D/grav_p.bin > $D/som_mixer.txt
            r=$?; sed 's/^/   /' $D/som_mixer.txt
            [ $r = 0 ] || falhou "som: a gravação do mixer da PPC diferente da x86 (rede 4)"
            [ -n "${GUARDA_SOM:-}" ] || rm -f $D/grav_x.bin $D/grav_p.bin
        fi
        # (F5, etapa 3) o mesmo que a F5 exige, no log da PPC sem mapa (o rastro igual ao da x86 já está no semmapa;
        # aqui, a origem dela e a medida de tempo)
        if [[ $n == f5_* ]]; then
            python3 port/x360/tools/roteiros.py f5 $n $D/ppcsm.txt --carimbo "$AGORA diff:da39a3ee5e" > $D/f5_ppc.txt
            r=$?; echo "   PPC sem mapa: $(tail -1 $D/f5_ppc.txt)"
            [ $r = 0 ] || { sed 's/^/     /' $D/f5_ppc.txt; falhou "f5: a PPC sem mapa sem o que a F5 exige"; }
            python3 port/x360/tools/passos.py $D/ppcsm.txt --carimbo "$AGORA diff:da39a3ee5e" > $D/passos_ppc.txt
            r=$?; echo "   PPC sem mapa: $(tail -1 $D/passos_ppc.txt)"
            [ $r = 0 ] || { sed 's/^/     /' $D/passos_ppc.txt; falhou "passos: a medida de tempo da PPC incompleta"; }
        fi
        # (F4, etapa 9a) o save que a rodada deixou no cartão, a x86 contra a PPC sem mapa campo a campo, e as sagas
        # que o roteiro conferiu em memória ("confere saga N") também no arquivo: o save da História gravado
        if grep -aq '^roteiro: .* fim vblank' $D/ppcsm.txt; then
            python3 port/x360/tools/roteiros.py save $D/sa $D/sq $D/telas_a.txt > $D/save.txt
            r=$?
            echo "   save no cartão (x86 contra PPC sem mapa):"; sed 's/^/     /' $D/save.txt
            [ $r = 0 ] || falhou "save no cartão diferente entre a x86 e a PPC sem mapa"
        fi
        if [ "${MEM:-fim}" = menu ]; then
            # (MEM=menu: a PPC passa do menu principal e a memória dele se confere mesmo que ela pare depois)
            echo "   memória no vblank $V (menu principal):"
            python3 port/x360/tools/roteiros.py memoria $D/memdiff.txt | sed 's/^/     /'
            python3 port/x360/tools/roteiros.py memoria $D/memdiff.txt > /dev/null ||
                falhou "memória no menu principal diferente (memdiff)"
        else
            # a memória também no último vblank de cada tela de menu do essencial e da História que o roteiro visita (DuelMenu,
            # seleções, Treino, Opções): no fim da luta o pacote, os retratos, os estágios e as páginas já foram
            # liberados, e só ali as conversões da etapa 5b estão vivas. Cada tela exige os conteúdos dela vivos e
            # conformes (roteiros.py TELAS_MEM); uma rodada a mais da PPC e da x86 por tela
            PONTOS=(); [[ $n != f5_* ]] || PONTOS=(--pontos)
            python3 port/x360/tools/roteiros.py vblanks_mem $D/telas_a.txt "${PONTOS[@]}" > $D/mem_telas.txt ||
                falhou "vblanks_mem sem as telas"
            while read -r t VT; do
                M=$D/mem_$t
                if [ "$JUNTA" != 1 ] && roda_ppc; then
                    rm -rf $M; mkdir -p $M
                    BT3_DATA=$D/dados BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_ROTEIRO=$R BT3_SAVES=$M/sq \
                        BT3_HASH=$M/hq.txt BT3_HASH_AT=$VT BT3_MEMORIA_SAIR=1 timeout ${TP:-3600} qemu-ppc $B/ppc/bt3_ppc > $M/ppc.txt 2>&1 \
                        < /dev/null
                    BT3_ROTEIRO=$R BT3_SAVES=$M/sm BT3_HASH=$M/hm.txt BT3_HASH_AT=$VT BT3_MEMORIA_SAIR=1 timeout 900 $B/x86/bt3_x86 \
                        > /dev/null 2>&1 < /dev/null
                fi
                if roda_ppc || [ ! -s $M/donos.tsv ]; then
                    rm -rf $M/sd $M/donos.*
                    BT3_ROTEIRO=$R BT3_SAVES=$M/sd BT3_HASH=$M/hd.txt timeout 1800 python3 port/x360/tools/campos.py \
                        --donos $B/x86/bt3_x86 $VT --tipos $M/donos > $M/donos.txt 2>&1 < /dev/null
                fi
                rm -f $M/memdiff.txt
                if [ -s $M/hq.txt.dump ] && [ -s $M/hm.txt.dump ]; then
                    python3 port/x360/tools/memdiff.py $M/hm.txt.dump $M/hq.txt.dump $B/x86/bt3_x86 $B/ppc/bt3_ppc \
                        --so-em-uso --marcas-vivas $D/marcas --tipos $O/globais --donos $M/donos --max 400 | sed 's/  x86 [0-9a-f]\{8\} .*ppc [0-9a-f ]*$//' \
                        > $M/memdiff.txt
                    echo "memdiff saiu com ${PIPESTATUS[0]}" >> $M/memdiff.txt
                fi
                python3 port/x360/tools/roteiros.py memoria $M/memdiff.txt --tela $t --roteiro $n --lidos $D/lidos.txt \
                    > $M/memoria.txt
                r=$?
                echo "   memória na tela $t (vblank $VT):"; sed 's/^/     /' $M/memoria.txt
                [ $r = 0 ] || falhou "memória na tela $t diferente (memdiff)"
            done < $D/mem_telas.txt
        fi
    fi
    if [ -n "${IMG:-}" ]; then
        # as imagens das telas (F4, etapa 6): o rasterizador de software na x86 e na PPC sem mapa (os mesmos arquivos
        # convertidos do passo 5), no mesmo momento de cada tela, ancorado no rastro (roteiros.py imagens: a entrada e
        # a saída de cada tela de menu do essencial, a carga, cada marca, cada "ok som", cada pergunta do cartão; e
        # a luta, salvo com IMG=menus),
        # gravado em PPM por port/x360/headless/imagens.c. Só as janelas antes de cada imagem se pintam
        # (BT3_IMAGENS_JANELA, BT3_GS_FROM): pintando tudo, a PPC levaria umas três horas por roteiro. O rastro de cada
        # rodada tem de ser o da x86 (o mesmo momento), e as imagens idênticas byte a byte (roteiros.py
        # compara_imagens: a tela, o quadro e quantos pixels diferem)
        I=$D/img
        LUTAARG=(--luta); [ "$IMG" != menus ] || LUTAARG=()
        python3 port/x360/tools/roteiros.py imagens $D/telas_a.txt "${LUTAARG[@]}" > $D/imagens.txt
        [ -s $D/imagens.txt ] || falhou "imagens: lista vazia (o rastro da x86?)"
        IMGENV=(BT3_GS=1 BT3_GS_FROM=1000000000 BT3_IMAGENS_JANELA=${JANELA:-8})
        for b in x86 ppc; do
            if [ "${JUNTA:-0}" != 1 ] && { { [ $b = x86 ] && roda_x86; } || { [ $b = ppc ] && roda_ppc; }; }; then
                rm -rf $I/$b; mkdir -p $I/$b; cp $D/imagens.txt $I/$b/lista.txt
                if [ $b = x86 ]; then
                    env "${IMGENV[@]}" BT3_IMAGENS=$I/$b/lista.txt BT3_ROTEIRO=$R BT3_SAVES=$I/$b/s timeout 900 \
                        $B/x86/bt3_x86 > $I/$b/saida.txt 2>&1 < /dev/null
                else
                    env "${IMGENV[@]}" BT3_IMAGENS=$I/$b/lista.txt BT3_DATA=$D/dados \
                        BT3_CONVERTIDOS=$D/dados/mods/convertidos.txt BT3_ROTEIRO=$R BT3_SAVES=$I/$b/s \
                        timeout ${TP:-3600} qemu-ppc $B/ppc/bt3_ppc > $I/$b/saida.txt 2>&1 < /dev/null
                fi
            fi
            python3 port/x360/tools/roteiros.py telas $I/$b/saida.txt > $I/$b/telas.txt
            cmp_i=$(python3 port/x360/tools/roteiros.py compara $D/telas_a.txt $I/$b/telas.txt)
            echo "   imagens ($b): $(grep -c '^imagem: ' $I/$b/saida.txt) gravadas; rastro: $cmp_i"
            grep -q '^iguais' <<< "$cmp_i" || falhou "imagens: o rastro da $b com o rasterizador diferente do da x86"
        done
        python3 port/x360/tools/roteiros.py compara_imagens $D/imagens.txt $I/x86 $I/ppc \
            --saidas $I/x86/saida.txt $I/ppc/saida.txt --telas $D/telas_a.txt > $D/imagens_compara.txt
        r=$?
        sed 's/^/   /' $D/imagens_compara.txt
        [ $r = 0 ] || falhou "imagens das telas diferentes entre a x86 e a PPC"
    fi
    rm -f bt3_crash.txt
done
echo "== todos juntos"
n=soma
python3 port/x360/tools/roteiros.py soma "${LOGS[@]}" --prog $B/x86/bt3_x86
r=$?
# (as 16 pendentes do essencial só se alcançam com os 4 roteiros dele juntos: com menos, a soma só informa)
essencial=0; for e in duelo duelo_time treino opcoes; do [[ " ${NOMES[*]} " == *" $e "* ]] && essencial=$((essencial + 1)); done
[ $r = 0 ] || [ $essencial -lt 4 ] || falhou "pendente do essencial não alcançada"
if [ ${#FALHAS[@]} -gt 0 ]; then
    echo "== ${#FALHAS[@]} falha(s):"; printf '   %s\n' "${FALHAS[@]}"
    [ ${#ORIGEM[@]} -eq 0 ] || echo "== NÃO VALE COMO CRITÉRIO: $(printf '%s; ' "${ORIGEM[@]}")"
    exit 1
fi
if [ ${#ORIGEM[@]} -gt 0 ]; then
    echo "== tudo certo, mas NÃO VALE COMO CRITÉRIO (DESENVOLVIMENTO=1): $(printf '%s; ' "${ORIGEM[@]}")"
    exit 0
fi
echo "== tudo certo"
