#!/usr/bin/env python3
"""F4, etapa 4: o que os roteiros de controle (port/x360/roteiros, BT3_ROTEIRO) alcançam, comparado com o inventário
das chamadas de Sprite_Unpack (docs/x360/formatos/menus-modos.md).

    roteiros.py telas SAIDA                 o rastro de uma rodada: as linhas "tela", "mcflow" e "roteiro:" da saída
    roteiros.py compara A B                 dois rastros (arquivos de "telas"): iguais, ou a primeira diferença
    roteiros.py chamadas LOG --prog P       as chamadas de Sprite_Unpack que a rodada fez, pelo log do gdb (linhas
                                            "UNPACK vblank modo retorno"), cada uma casada com a lista do unpacks.py
    roteiros.py confere LOG --prog P --conv CONV --dados D
                                            cruza as chamadas da rodada com o "sem conversor" do conv_tool --mapa
                                            (CONV) sobre o rastreio da mesma rodada: o conteúdo de cada chamada
                                            pendente alcançada tem de sair "sem conversor", e só ele
    roteiros.py soma LOG... --prog P        as chamadas de várias rodadas juntas contra o inventário: as pendentes do
                                            caminho essencial (16) e as dos outros modos
    roteiros.py semmapa PPC X86 GDB [--hash HX HP] [--mem MEMDIFF]
                                            a PPC sem mapa (só os conversores, como o 360) contra a x86 do mesmo
                                            roteiro (F4, etapa 5): ou ela chega ao "fim" com o rastro, a sequência de
                                            arquivos lidos (linhas LEU), a luta (BT3_HASH) e a memória no último vblank
                                            antes do fim (MEMDIFF, a saída do memdiff.py que o roteiros.sh grava; o
                                            critério em memoria()) iguais aos da x86, ou para na guarda do
                                            Sprite_Unpack, que nomeia o próximo bloqueio, com o rastro e os arquivos
                                            lidos até ali iguais ao começo dos da x86. Qualquer outro fim (queda, tempo
                                            esgotado, erro de conversor, falha do roteiro) é falha
    roteiros.py memoria MEMDIFF             só o critério da memória (memoria()) sobre a saída de um memdiff.py
    roteiros.py vblank_menu TELAS          o último vblank da primeira visita ao menu principal (tela 4) num rastro: o
                                            quadro em que o roteiros.sh compara a memória das duas builds com MEM=menu
                                            (o pacote dele ainda vivo, antes da tela seguinte; o critério da etapa 5a)
    roteiros.py vblank_fim TELAS           o último vblank antes do passo "fim" num rastro: o quadro em que o
                                            roteiros.sh compara a memória das duas builds (o critério da etapa 5)
    roteiros.py vblanks_mem TELAS          as telas de menu do essencial e da História que o rastro visita (TELAS_MEM), uma linha
                                            "TELA VBLANK" com o último vblank da primeira visita: o roteiros.sh compara
                                            a memória das duas builds também ali, com o pacote e as imagens da tela
                                            vivos (no fim da luta já foram liberados)
    roteiros.py memoria MEMDIFF [--tela N]  com --tela, também exige vivos e conformes os conteúdos da tela
                                            (TELAS_MEM): "sem bloco" não serve; com --tela CONTEÚDO (de um passo
                                            "marca memoria CONTEÚDO" do roteiro, '/' trocada por '_'), aquele
    roteiros.py save X86 PPC TELAS          (F4, etapa 9a) o save que a rodada deixou no cartão (BT3_SAVES) da x86 e
                                            o da PPC sem mapa, campo a campo (save_entre.py campos: os mesmos valores,
                                            cada um na ordem da sua build); e cada saga que o roteiro conferiu na
                                            memória ("ok saga N" no rastro TELAS, o passo "confere saga N") com o fim
                                            visto (MSLOT_OUTRO_SEEN) também no arquivo das duas: o save da História
                                            gravado, não só em memória
    roteiros.py larguras CONV RASTREIO --prog X86R
                                            (F4, etapa 9a) a saída do conv_tool --mapa que saiu com 1 só pelas posições
                                            de LARGURA_CONHECIDA: 0 se nenhum arquivo deu erro, todo "com outra
                                            largura" está na lista e cada uma foi lida no rastreio da x86r (RASTREIO,
                                            pelo programa X86R) só pelas funções da entrada (o roteiros.sh aceita então
                                            a saída 1 do conv_tool)
    roteiros.py partes RASTREIO PASTA [N]   o rastreio em partes de até N posições (4 milhões), cada arquivo inteiro numa
                                            parte só: o rastreio.py decide cada posição só com as do mesmo arquivo, então
                                            os mapas das partes, juntos e ordenados, são o mapa do rastreio inteiro, com
                                            a memória de uma parte (a Ultimate Battle lê 13 milhões de posições, e o
                                            rastreio.py não cabia nos 8 GB do WSL)

O conteúdo de cada chamada (arquivo e seção, ou a faixa de arquivos inteiros) é o da tabela CONTEUDO abaixo, transcrita
do inventário (menus-modos.md 1 e 3). O "confere" lê a tabela de seções de cada pacote de menu em DADOS para dizer a
seção de um deslocamento: roda no WSL, onde estão os dados, e só imprime números.
"""
import argparse, collections, pathlib, re, struct, subprocess, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "port/x360/tools"))
import unpacks  # noqa: E402

# O conteúdo que cada chamada descomprime (menus-modos.md): "NNN sK" é a seção K do pacote de menu de índice NNN; um
# nome é uma faixa de arquivos inteiros (FAIXAS).
CONTEUDO = {
    "menu_a.c:55": "449 s2", "menu_a_c.c:56": "449 s1",
    "menu_a_d.c:76": "episodio", "menu_a_d.c:132": "460 s2", "menu_a_d.c:140": "saga",
    "menu_b_c.c:79": "460 s2", "menu_b_c.c:113": "saga", "menu_b_d.c:147": "460 s1",
    "menu_c_c.c:180": "460 s3", "menu_c_c.c:188": "saga", "menu_c_c.c:203": "fundo_resultado",
    "menu_c_d.c:25": "460 s4",
    "menu_c_e.c:226": "retrato", "menu_c_e.c:300": "estagio", "menu_c_e.c:400": "461 s2", "menu_c_e.c:409": "448 s2",
    "menu_c_e.c:746": "retrato", "menu_c_e.c:759": "estagio",
    "menu_e_b.c:290": "retrato", "menu_e_b.c:367": "estagio", "menu_e_b.c:469": "448 s2",
    "menu_e_b.c:771": "retrato", "menu_e_b.c:784": "estagio",
    "menu_g_c.c:195": "448 s1",
    "menu_h_b.c:49": "463 s1", "menu_h_b.c:119": "retrato", "menu_h_b.c:849": "retrato",
    "menu_h_d.c:650": "461 s1", "menu_h_d.c:1221": "pagina_treino",
    "menu_i_b.c:24": "473 s2", "menu_i_c.c:173": "473 s3",
    "menu_i_d.c:98": "retrato", "menu_i_d.c:166": "450 s2", "menu_i_d.c:334": "retrato",
    "menu_j_b.c:59": "450 s1", "menu_j_b.c:117": "convite", "menu_k_d.c:24": "fundo_torneio",
    "menu_k_f.c:31": "chave", "menu_k_f.c:805": "retrato", "menu_k_f.c:806": "retrato",
    "menu_k_f.c:830": "retrato", "menu_k_f.c:831": "retrato",
    "menu_l_b.c:83": "retrato", "menu_l_b.c:154": "451 s4", "menu_l_b.c:266": "retrato",
    "menu_m_b.c:184": "retrato", "menu_m_b.c:272": "451 s4", "menu_m_b.c:433": "retrato",
    "menu_n_b.c:98": "ub_cursos", "menu_n_c.c:140": "ub_ranking", "menu_n_c.c:720": "retrato",
    "menu_n_c.c:727": "retrato", "menu_n_d.c:53": "451 s3", "menu_o_b.c:63": "451 s6", "menu_o_d.c:146": "451 s2",
    "menu_p_b.c:67": "451 s3", "menu_p_c.c:70": "451 s1",
    "menu_q_b.c:899": "sim_dia", "menu_q_b.c:1976": "retrato", "menu_q_b.c:1983": "retrato",
    "menu_r_c.c:38": "451 s3", "menu_r_d.c:53": "451 s5", "menu_s_b.c:93": "451 s2", "menu_s_c.c:30": "451 s3",
    "menu_u_g.c:60": "retrato", "menu_u_h.c:542": "452 s3", "menu_u_h.c:544": "estagio", "menu_v_b.c:759": "retrato",
    "menu_v_d.c:146": "452 s2", "menu_w_c.c:133": "452 s1", "menu_x_c.c:39": "453 s1",
    "menu_y_b.c:957": "retrato", "menu_y_b.c:1002": "459 s2", "menu_y_b.c:1008": "retrato",
    "menu_z_c.c:211": "459 s1", "menu_z_d.c:535": "retrato", "menu_z_d.c:952": "459 s3",
    "menu_za_d.c:254": "459 s4", "late_a.c:555": "472 s2", "late_a_c.c:52": "472 s2",
    "loading.c:166": "folha", "mcflow_a.c:2636": "mcflow",
}

# arquivos inteiros (um contêiner BPE no deslocamento 0), por índice na partição 1 (menus-modos.md 1)
FAIXAS = [("chave", 454, 458), ("saga", 464, 471), ("sim_dia", 474, 474), ("ub_ranking", 475, 475),
          ("ub_cursos", 476, 476), ("mcflow", 477, 477), ("retrato", 760, 923), ("estagio", 924, 959),
          ("folha", 960, 962), ("convite", 963, 967), ("fundo_torneio", 968, 972), ("episodio", 973, 1020),
          ("fundo_resultado", 1021, 1024), ("pagina_treino", 1025, 1422)]
PACOTES = {448, 449, 450, 451, 452, 453, 459, 460, 461, 462, 463, 472, 473}

# Arquivos fora das faixas de menu que o rastreio vê ler como número e que ainda não têm conversor, já conhecidos: o
# "confere" acusa todo outro "sem conversor" fora de disc/ (um caso novo não pode sumir por não ser de menu). Cada um
# é pendência da etapa 9 (plano.md, F4).
CONHECIDOS = {
    # atores da cena do Shenlong (ShenScene_*, src/battle/view_b_d.c:366, ids 0xD3D e 0xD46): roteiro shenlong
    "pzs3us1/03388.bin": "Shenlong, ator da cena (id 0xD3D)",
    "pzs3us1/03397.bin": "Shenlong, ator da cena (id 0xD46)",
    # (os três da luta do primeiro episódio da História, 00510, 00560 e 00710, saíram na etapa 9a: o roteiro de evento,
    # os textos e as bocas da luta, formatos/historia-luta.md)
}

# Posições que o --mapa dá "com outra largura" e que estão certas, uma a uma (arquivo, deslocamento, largura lida pelo
# rastreio, largura trocada pelo conversor): valores s32 que o jogo só lê pela metade baixa, para os quais o conv_tool
# não tem classe. O valor é (porquê, leitores): a posição só está certa se TODO leitor estreito dela no big-endian
# lê a metade certa (PS2_LOW ou uma struct permutada), e isso depende de quem lê, não da posição (o GSDT de um roteiro
# é um pool de constantes deduplicadas: a mesma posição pode ser operando de vários comandos). Por isso a entrada
# guarda as funções que a leram no rastreio da x86r (as do mapa.tsv.conflitos e as do "rastreio.py resumo"), e o
# "larguras" falha se o rastreio da rodada mostrar outra, ou se a lista de leitores do rastreio estiver cheia (4 pcs:
# pode haver um leitor que ele não anotou). O "confere" aceita a linha "contra o mapa" de um arquivo só se cada
# posição com outra largura dele estiver aqui (o conv_tool lista as 10 primeiras: --detalhe); qualquer outra é erro.
LARGURA_CONHECIDA = {
    # o roteiro de evento da luta do primeiro episódio da História (formatos/historia-luta.md 2.6): no GSDT, 301 e 125
    # são só operandos do comando 12 (SetMember, BtlScriptCmd_SetMember), os itens id[1] e id[2] de um membro (os
    # operandos 8 e 9), lidos pela metade baixa pelo PS2_LOW do BattleSetup_SetMemberByItemIds (battle_load.c:1579, a
    # cópia items) e, pela cópia em work->items, pelo BtlFacadeItem do BtlFacade_SetMemberItems (btl_facade.c:218,
    # permutada em PORT_BIG_ENDIAN); trocados como u32, os dois leem a metade certa no big-endian
    ("pzs3us1/00510.bin", 0x2368, 2, 4): (
        "id de item s32 do GSDT (comando 12, item 1) lido pela metade baixa",
        {"BattleSetup_SetMemberByItemIds", "BtlFacade_SetMemberItems"}),
    ("pzs3us1/00510.bin", 0x236C, 2, 4): (
        "id de item s32 do GSDT (comando 12, item 2) lido pela metade baixa",
        {"BattleSetup_SetMemberByItemIds", "BtlFacade_SetMemberItems"}),
    # (etapa 9b) bytes dos arquivos de lutador (o file[0], personagem.md) que o rastreio dá como lidos com 4 bytes: todos
    # num deslocamento ímpar (1 mod 4), e uma leitura de 4 bytes num endereço ímpar não existe no PS2 (lw e lwc1 dariam
    # exceção, o lq ignora os bits baixos) com o arquivo carregado alinhado; o endereço lido não é o do arquivo, é uma
    # cópia para onde o rastreio levou a origem do byte (os leitores são os de vetor da luta, nos locais da pilha e nas
    # tabelas que o EftGfx_LightClutSpecular monta; a tabela de normais dele foi procurada nos cinco arquivos e não está
    # lá). O conversor os trata como bytes, certo. Os lutadores da missão 0 da Missão 100 (roteiro missao)
    ("pzs3us1/01424.bin", 0x760CD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular', 'Port_FastMtxApply', 'Vec3_Dot'}),
    ("pzs3us1/01673.bin", 0x645CD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/01673.bin", 0x648CD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'op_lqc2'}),
    ("pzs3us1/01683.bin", 0x5D28D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/01683.bin", 0x5D2CD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular'}),
    ("pzs3us1/01693.bin", 0x6750D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular'}),
    ("pzs3us1/01693.bin", 0x6758D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/01703.bin", 0x5BA4D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular'}),
    ("pzs3us1/01703.bin", 0x5BBCD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/01703.bin", 0x5BCCD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'op_lqc2'}),
    # os lutadores do Disc Fusion (roteiro discfusion: o do jogador, o do ranking e os cinco do curso 0), o mesmo caso
    ("pzs3us1/01423.bin", 0x760CD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular', 'Port_FastMtxApply', 'Vec3_Dot'}),
    ("pzs3us1/02283.bin", 0x6440D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular'}),
    ("pzs3us1/02283.bin", 0x6448D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/02293.bin", 0x74F0D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/02293.bin", 0x750CD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'op_lqc2'}),
    ("pzs3us1/02293.bin", 0x7514D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular'}),
    ("pzs3us1/02303.bin", 0x6338D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular'}),
    ("pzs3us1/02303.bin", 0x6348D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/02303.bin", 0x6354D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'op_lqc2'}),
    ("pzs3us1/02313.bin", 0x6928D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
    ("pzs3us1/02323.bin", 0x661CD, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'EftGfx_LightClutSpecular'}),
    ("pzs3us1/02323.bin", 0x6624D, 4, 1): (
        "byte do lutador com a origem levada pelo rastreio a uma leitura de 4 bytes (deslocamento ímpar)",
        {'Vec3_Dot'}),
}
NOTAS_CONV = []  # (conv(): as posições de LARGURA_CONHECIDA achadas, para o confere dizer)

# O caminho essencial da decisão 22 (menus-modos.md 4: 16 pendentes, 5 com gancho)
ESSENCIAL = {"menu_a.c:55", "menu_c_e.c:226", "menu_c_e.c:300", "menu_c_e.c:400", "menu_c_e.c:409", "menu_c_e.c:746",
             "menu_c_e.c:759", "menu_e_b.c:290", "menu_e_b.c:367", "menu_e_b.c:469", "menu_e_b.c:771",
             "menu_e_b.c:784", "menu_g_c.c:195", "menu_h_d.c:650", "menu_h_d.c:1221", "menu_x_c.c:39",
             "menu_a_c.c:56", "menu_i_b.c:24", "menu_i_c.c:173", "loading.c:166", "mcflow_a.c:2636"}


def chave(onde):
    """"src/menu/menu_a.c:55 MainMenu_Init" -> "menu_a.c:55" """
    arq, resto = onde.split(":", 1)
    return f"{pathlib.Path(arq).name}:{resto.split()[0]}"


def lista_unpacks():
    """{chave: (situação, função)} das 82 chamadas (unpacks.py)"""
    res, _, _ = unpacks.levantar()
    return {chave(onde): (s, onde.split()[1]) for s, onde, _ in res if s != "bpe-direto"}


def linhas_de_chamada():
    """{(arquivo, função): [linhas das chamadas]}, para casar o addr2line com a chamada mais próxima"""
    out = collections.defaultdict(list)
    for k, (s, f) in lista_unpacks().items():
        arq, n = k.split(":")
        out[(arq, f)].append(int(n))
    return out


def telas(caminho):
    out = []
    for l in pathlib.Path(caminho).read_text(errors="replace").splitlines():
        if l.startswith(("tela ", "mcflow ", "roteiro: ")) and not re.match(r"roteiro: \S+, \d+ passos$", l):
            out.append(l)
    return out


def chamadas(log, prog):
    """[(vblank, modo, chave)] das linhas "UNPACK vblank modo 0xRET" do log do gdb"""
    ev = []
    for l in pathlib.Path(log).read_text(errors="replace").splitlines():
        m = re.match(r"UNPACK (\d+) (-?\d+) (0x[0-9a-fA-F]+)$", l.strip())
        if m:
            ev.append((int(m.group(1)), int(m.group(2)), int(m.group(3), 16)))
    if not ev:
        return []
    enderecos = sorted({r for _, _, r in ev})
    a2l = subprocess.run(["addr2line", "-f", "-e", prog] + [hex(r - 1) for r in enderecos], capture_output=True,
                         text=True).stdout.split("\n")
    onde = {}
    linhas = linhas_de_chamada()
    for i, r in enumerate(enderecos):
        func, pos = a2l[2 * i], a2l[2 * i + 1]
        arq, n = pos.split(" ")[0].rsplit(":", 1)
        arq = pathlib.Path(arq).name
        cand = linhas.get((arq, func), [])
        n = int(n) if n.isdigit() else 0
        # a linha da chamada no fonte: a mais próxima que não passa da que o addr2line deu (o comando pode ter várias
        # linhas; e há chamadas em linhas seguidas, como as de Bracket_UpdateImages, :805 e :806)
        antes = [c for c in cand if c <= n]
        onde[r] = (arq, func, max(antes) if antes else (min(cand) if cand else n))
    out = []
    for vb, modo, r in ev:
        arq, func, n = onde[r]
        k = f"{arq}:{n}"
        # CharSel_Init: o compilador juntou as duas chamadas do pacote (:400 no treino, modo 45; :409 no duelo) numa
        # só (menu_c_e.c, CharSel_Init): o modo diz qual
        if k in ("menu_c_e.c:400", "menu_c_e.c:409"):
            k = "menu_c_e.c:400" if modo == 45 else "menu_c_e.c:409"
        out.append((vb, modo, k))
    return out


def classe(nome, dados):
    """o conteúdo de um nome do rastreio ("pzs3us1/00449.bin@0xAB5C0#bpe" ou "pzs3us1/00449.bin") pela tabela"""
    m = re.match(r"pzs3us1/(\d{5})\.bin(?:@0x([0-9A-Fa-f]+)#bpe)?$", nome)
    if not m:
        return None
    i, ofs = int(m.group(1)), m.group(2)
    for c, a, b in FAIXAS:
        if a <= i <= b:
            return c if ofs is None or int(ofs, 16) == 0 else f"{c}@0x{ofs}"
    if i in PACOTES:
        if ofs is None:
            return f"{i}"
        dados_arq = (pathlib.Path(dados) / f"pzs3us1/{i:05d}.bin").read_bytes()
        n = struct.unpack_from("<I", dados_arq, 0)[0]
        for s in range(1, n + 1):
            if struct.unpack_from("<I", dados_arq, 4 * s)[0] == int(ofs, 16):
                return f"{i} s{s}"
        return f"{i}@0x{ofs}"
    return None


def leitores(rastreio, prog, chaves):
    """{(arquivo, deslocamento): (funções que leram, cheia)} das posições dadas, pelos pcs que o rastreio da x86r gravou
    (port/x360/headless/rastreio.c: até 4 pcs diferentes por posição, a coluna 6; com os 4 ocupados pode haver outro
    leitor que ele não anotou). Posição que o rastreio não tem fica de fora."""
    padroes = "".join(f"{a}\t{o}\t\n" for a, o in chaves)
    linhas = subprocess.run(["grep", "-aF", "-f", "-", rastreio], input=padroes, capture_output=True,
                            text=True).stdout.splitlines()
    pcs_de = {}
    for l in linhas:
        c = l.split("\t")
        k = (c[0], int(c[1])) if len(c) > 5 and c[1].isdigit() else None
        if k in chaves:
            pcs_de[k] = pcs_de.get(k, []) + [[int(p, 16) for p in c[5].split(",") if int(p, 16)]]
    todos = sorted({pc for ls in pcs_de.values() for pcs in ls for pc in pcs})
    # (o nome pelo addr2line, como em chamadas(); o rastreio.py não se importa: ele roda o main() ao carregar)
    a2l = subprocess.run(["addr2line", "-f", "-e", prog] + [hex(pc) for pc in todos], capture_output=True, text=True,
                         stdin=subprocess.DEVNULL).stdout.split("\n") if todos else []
    nome = {pc: a2l[2 * i] for i, pc in enumerate(todos)}
    return {k: ({nome[pc] for pcs in ls for pc in pcs}, any(len(pcs) >= 4 for pcs in ls)) for k, ls in pcs_de.items()}


def confere_leitores(rastreio, prog):
    """As posições de LARGURA_CONHECIDA achadas (NOTAS_CONV) contra os leitores que o rastreio da rodada viu: True se
    cada uma foi lida só pelas funções da entrada, e a lista do rastreio não encheu."""
    chaves = {(k[0], k[1]) for k in NOTAS_CONV}
    vistos = leitores(rastreio, prog, chaves)
    ok = True
    for k in NOTAS_CONV:
        _, previstos = LARGURA_CONHECIDA[k]
        fs, cheia = vistos.get((k[0], k[1]), (set(), False))
        erros = []
        if not fs:
            erros.append("o rastreio não tem a posição (leitores não conferidos)")
        if fs - previstos:
            erros.append(f"lida também por {', '.join(sorted(fs - previstos))} (a entrada de LARGURA_CONHECIDA só "
                         f"aceita {', '.join(sorted(previstos))}): confira se o novo leitor lê a metade certa no "
                         "big-endian")
        if cheia:
            erros.append("a lista de leitores do rastreio encheu (4 pcs): pode haver um leitor não anotado")
        for e in erros:
            print(f"  DIFERENÇA: {k[0]} 0x{k[1]:X}: {e}")
        if not erros:
            print(f"  leitores de {k[0]} 0x{k[1]:X}: {', '.join(sorted(fs))}")
        ok = ok and not erros
    return ok


def conv(caminho):
    """{nome: "sem conversor" | "convertido" | "ERRO: mensagem"} das linhas do conv_tool. Tudo que não é "convertido"
    nem "sem conversor" é erro (conv_tool.c, um(): "NOME: mensagem; cobertura ...", "não abre", "BPE inválido"), e a
    linha "  contra o mapa: ..." que segue a de um arquivo o torna erro com posições de outra largura ou não trocadas."""
    out = {}
    ultimo = None
    largura = []  # as posições "com outra largura" do arquivo atual (as linhas de detalhe antes do "contra o mapa")
    for l in pathlib.Path(caminho).read_text(errors="replace").splitlines():
        m = re.match(r"    0x([0-9A-Fa-f]+): rastreio leu (\d+), conversor marca (\d+) \((trocou com outra largura|tratou "
                     r"como byte)\)$", l)
        if m:
            largura.append((int(m.group(1), 16), int(m.group(2)), int(m.group(3))))
            continue
        m = re.match(r"  contra o mapa: (\d+) posições; (\d+) iguais, (\d+) com outra largura, (\d+) não trocadas", l)
        if m:
            outra, nao = int(m.group(3)), int(m.group(4))
            conhecidas = [(ultimo, *x) for x in largura if (ultimo, *x) in LARGURA_CONHECIDA]
            if ultimo is None:
                out["?"] = f"ERRO: linha \"contra o mapa\" sem arquivo antes: {l.strip()}"
            elif (outra or nao) and len(largura) == outra + nao and len(conhecidas) == outra + nao:
                NOTAS_CONV.extend(conhecidas)  # (todas as divergentes estão em LARGURA_CONHECIDA: certo)
            elif (outra or nao) and not out[ultimo].startswith("ERRO"):
                out[ultimo] = f"ERRO: {l.strip()}"
            largura = []
            continue
        if l.startswith(" ") or not l.strip():
            continue  # (o detalhe das posições divergentes, "    0x...: rastreio leu ...", já contado na de cima)
        m = re.match(r"(\S+): (.*)$", l)
        if not m:
            out[l] = f"ERRO: linha que o roteiros.py não entende: {l}"
            ultimo = None
            continue
        ultimo, r = m.group(1), m.group(2)
        largura = []
        if out.get(ultimo, "").startswith("ERRO"):
            continue  # (um erro do mesmo nome numa linha anterior fica: o conv_tool pode dizer mais de uma coisa dele)
        if r == "sem conversor":
            out[ultimo] = r
        elif r.startswith("convertido; cobertura "):
            out[ultimo] = "convertido"
        else:
            out[ultimo] = f"ERRO: {r}"
    return out


def partes(entrada, pasta, limite):
    cont = collections.Counter()
    with open(entrada, encoding="utf-8", errors="replace") as f:
        for l in f:
            cont[l.split("\t", 1)[0]] += 1
    grupo, atual, n = {}, 0, 0
    for arq in sorted(cont):
        if n and n + cont[arq] > limite:
            atual, n = atual + 1, 0
        grupo[arq] = atual
        n += cont[arq]
    pasta = pathlib.Path(pasta)
    pasta.mkdir(parents=True, exist_ok=True)
    saidas = [open(pasta / f"parte_{i:02d}.tsv", "w") for i in range(atual + 1)]
    with open(entrada, encoding="utf-8", errors="replace") as f:
        for l in f:
            saidas[grupo[l.split("\t", 1)[0]]].write(l)
    for g in saidas:
        g.close()
    print(f"partes: {atual + 1} de até {limite} posições ({sum(cont.values())} no total, {len(cont)} arquivos)")


def leu(caminho):
    """a sequência de arquivos lidos: as linhas "LEU arquivo" (o dprintf do gdb na x86, BT3_LEU=1 na PPC)"""
    return [l.split(" ", 1)[1] for l in pathlib.Path(caminho).read_text(errors="replace").splitlines()
            if l.startswith("LEU ") and " " in l]


def comeco(a, b, oque):
    """a (a rodada que parou) é o começo de b (a da x86)? Imprime e devolve True/False"""
    for i, (l1, l2) in enumerate(zip(a, b)):
        if l1 != l2:
            print(f"  DIFERENÇA: {oque}, linha {i + 1}:\n    ppc {l1}\n    x86 {l2}")
            return False
    if len(a) > len(b):
        print(f"  DIFERENÇA: {oque}: a PPC tem {len(a)} linhas, a x86 {len(b)}")
        return False
    return True


# como a PPC sem mapa pode parar: a guarda do Sprite_Unpack (port/x360/conv/conv_jogo.c) nomeia o bloqueio; o resto
# é falha (sem nome não há o que fazer a seguir)
BLOQUEIO = [re.compile(r"Sprite_Unpack sem conversor em (\w+) \(retorno [^)]*\): o conteúdo ficou cru até o (.*)$"),
            re.compile(r"Sprite_Unpack em (\w+) \(retorno [^)]*\) sobre um contêiner BPE cru")]


# A memória das duas builds num vblank (memdiff.py --so-em-uso --marcas-vivas --tipos, gravado pelo roteiros.sh): o
# critério da etapa 5.
#   - o memdiff terminou (a linha "memdiff saiu com 0" que o roteiros.sh acrescenta; 2 é marca ambígua ou bloco que
#     falta);
#   - o heap no lugar: os mesmos blocos, de mesmo tamanho, nas duas builds;
#   - todo bloco vivo que guarda um arquivo ou um conteúdo convertido (achado pelo conteúdo) conforme número a número
#     pela largura que o conversor deu ("marcas ...: conforme N, fora-do-formato 0"), e pelo menos um; numa tela de
#     menu (--tela), os conteúdos da tela (TELAS_MEM) entre eles;
#   - as globais com tipo no DWARF campo a campo (--tipos): o que sai do formato vira trecho, como "diferente";
#   - cada trecho fora do formato ou "diferente" fora desses blocos explicado em DIFERENTES_CONHECIDAS (o que o port
#     ou o ambiente deixam diferente sem ser ordem de bytes), e nenhum trecho sem listar (o memdiff lista até --max
#     por região: a contagem "N trechos a olhar" tem de bater com os listados);
#   - as classes que são certas para uma largura e erradas para outra (memdiff.py SUSPEITAS: igual-sem-inverter,
#     2xu16, u64, u64-com-zero, misto, float-proximo), fora dos blocos com marcas e das globais com tipo, só nas regiões
#     de CLASSES_CONHECIDAS, cada uma com o porquê e um teto; e a soma delas nas regiões listadas igual à do total (uma
#     palavra dessas classes fora de uma linha de região não passa calada);
#   - os dois buffers de pacotes da GPU (pedido 0x100000) só com "diferente" nos campos não iniciados do pacote de
#     sprite (PACOTE_GPU) e com teto.
# Uma região do heap se nomeia pelo tamanho pedido ("heap:0x4000"), não pelo índice, que muda de um quadro e de um
# roteiro para outro (o dono de cada uma, conferido com campos.py --donos, vai no comentário); uma global, pelo nome.
SUSPEITAS = ("igual-sem-inverter", "2xu16", "u64", "u64-com-zero", "misto", "float-proximo")


def campo(passo, ini, tam):
    """onde de DIFERENTES_CONHECIDAS: o trecho cai inteiro em [ini, ini + tam) de um elemento de `passo` bytes"""
    return ("campo", passo, ini, tam)


DIFERENTES_CONHECIDAS = [
    # (região, onde, por quê). Onde: o deslocamento exato do começo do trecho, None para a região inteira, ou campo();
    # num bloco do heap, contado do começo dos dados (HeapBlock.data). Cada um conferido no código e, os do port, no gdb
    # (formatos/menus-telas.md 8.4; revisão da etapa 5)
    ("heap:0x2a8", 0x260, "SndMgr.iopBuf (Snd_Init, include/sys/snd.h): ponteiro do host (sceSifAllocIopHeap, calloc "
                          "em 32 bits)"),
    ("heap:0x4000", 0x0, "SaveData.unk0 e unk4 (Save_Init): a soma McCard_Checksum sobre os bytes de números nativos, "
                         "outra em cada build e conferida em cada uma"),
    ("heap:0x39f90", 0x38, "McFlow.saveBuf (McFlow_Init, include/sys/mcflow_a.h): a cópia do SaveData lida do cartão, "
                           "com a mesma soma unk0/unk4"),
    ("heap:0x39f90", 0x4038, "McFlow.repSave.hdr.sum e hdrSum (McFlowHeader): as somas do arquivo de replay, pela mesma "
                             "McCard_Checksum"),
    # os pools do motor de scripts (Gsc_Init, src/sys/gsc.c:55-70): Heap_Alloc sem memset, e cada elemento só é posto na
    # lista livre (o link). O resto de um elemento que nunca foi usado guarda o que ocupava a memória antes (bytes
    # iguais nas duas builds); a tarefa ou o arquivo usados gravam esses campos na ordem do host
    ("heap:0xfa0", campo(0x50, 0x08, 0x28), "GscTask livre do pool: unk8..abort nunca gravados desde a alocação"),
    ("heap:0x168", campo(0x24, 0x08, 0x1c), "GscFile livre do pool: unk8..data nunca gravados desde a alocação"),
    # (etapa 9b) o UbResult (include/menu/menu_o.h) tipa o +0x2A0 do placar como "s32 unk2A0", mas ali moram dois s16,
    # o PClock.timeLeft e o unkE da cópia do relógio da luta (score->clock = result->clock, UbScore_Fill,
    # src/menu/menu_p_d.c:83; PClock em include/menu/menu_p.h), trocados certo como u16; nenhum código lê o unk2A0
    # (grep em src/ e include/). O s32 escalar não é opaco no campos.py (só os vetores unk*)
    ("heap:0x784", 0x770, "UbResult.score.unk2A0: os dois s16 do PClock (timeLeft, unkE) que a visão de menu_o.h "
                          "tipa como um s32 que ninguém lê"),
    ("gColWalkBox", 0, "ponteiro para um local da pilha do host (ColMesh_WalkBox; gravado e nunca lido)"),
    ("gColWalkCtx", 0, "idem"),
    ("gPortDataStripped", 0, "as tabelas do executável: a PPC liga o esqueleto e copia do SLUS (decisão 13)"),
    ("gSndRpcBuf", None, "o buffer de envio dos RPC de som (SndRpcBuf, include/sys/snd.h): união de s32, s16 e da fila "
                         "SndQueue (u8 e u16), com o resto do último RPC de cada largura; o snd_se.c lê cada RPC pela "
                         "forma dele (a fila, na ordem nativa em PORT_BIG_ENDIAN)"),
    ("gPortMoviePath", None, "caminho do host (BT3_DATA diferente nas duas rodadas): a variável é só o caminho"),
    # PortAdxView (port/src/plat_sndstate.c) = {stat, paused, left, vol, pan[2], path[256]}, 0x118 bytes, 16 jogadores:
    # só o path, nunca os números (revisão da etapa 5)
    ("gPortAdxView", campo(0x118, 0x18, 0x100), "PortAdxView.path: caminho do host (BT3_DATA diferente nas duas rodadas)"),
    # sMc (port/src/plat_mc.c) = {open, mode, pos, path[600]}, 0x264 bytes por arquivo: só o path
    ("sMc", campo(0x264, 0xC, 600), "o path de cada arquivo do cartão: caminho do host (BT3_SAVES diferente nas duas "
                                    "rodadas)"),
]

# As regiões em que as classes SUSPEITAS são esperadas, fora dos blocos com marcas e dos tipados (globais pelo DWARF
# ou pelas declarações, blocos do heap pelo tipo que a alocação pede): (região, {classe: teto}, por quê). O teto é o
# maior número visto nos quatro roteiros do essencial (no fim e em cada tela de menu) com folga, ou o bloco inteiro
# num buffer de bytes; passar dele é diferença. O dono de cada uma pelo campos.py --donos.
CLASSES_CONHECIDAS = [
    ("heap:0x3000", {"igual-sem-inverter": 0x3000 // 4},
     "gProgress->loadPack (Progress_Init, src/battle/view_a_e.c:48): o arquivo da folha da tela de carregamento, um "
     "contêiner BPE (fluxo de bytes); as marcas conferem o arquivo lido na rodada, o resto do buffer são bytes"),
    ("heap:0x6800", {"igual-sem-inverter": 0x6800 // 4},
     "gProgress->loadRes (view_a_e.c:49): o conteúdo descomprimido da folha (pixels, bytes) e o resto do buffer; os "
     "números dele as marcas conferem"),
    ("heap:0x16800", {"igual-sem-inverter": 0x16800 // 4, "2xu16": 64, "u64": 64},
     "faceFile[0] e [1] do CharSel e do TeamSel (menu_c_e.c:700, menu_e_b.c:699): o arquivo do retrato como veio do "
     "disco, um contêiner BPE (fluxo de bytes); o conteúdo convertido mora no faceRes, que as marcas conferem. Etapa "
     "9b: os do tabuleiro do Sim Dragon (SimDay) guardam o arquivo do retrato (as marcas conferem) num buffer maior que "
     "ele (os arquivos de retrato têm de 10.304 a 90.304 bytes; o buffer, 0x16800), e o resto são bytes de antes "
     "(Heap_Alloc não limpa): 10 palavras 2xu16 na tela 22 e 16 u64 na 29, números trocados de um conteúdo que morava "
     "ali antes; o Bpe_Decode lê só o contêiner. Teto 64 de cada"),
    ("heap:0x1c000", {"igual-sem-inverter": (0x1c000 - 69760) // 4},
     "gModeMenu->imageFile (ModeMenu_Init, src/menu/menu_a_d.c:256): o arquivo da imagem do episódio como veio do disco, "
     "um contêiner BPE (fluxo de bytes), lido num buffer fixo maior que ele; as marcas conferem o arquivo, o resto do "
     "buffer são bytes de antes (Heap_Alloc não limpa). Etapa 9a. O teto é o resto depois do menor arquivo de episódio "
     "(00973..01020: 69.760 a 113.872 bytes), e só na tela 7 (SO_NAS_TELAS; revisão da 9a, apertado na 9b)"),
    ("heap:0x20800", {"igual-sem-inverter": (0x20800 - 0x20560) // 4},
     "gModeMenu->imageRes (menu_a_d.c:257): o episódio descomprimido (0x20560 bytes nos 48, conferido pelas marcas) num "
     "buffer de 0x20800; o resto, 0x2A0 bytes depois do conteúdo, são bytes de antes (Heap_Alloc não limpa e o "
     "Bpe_Decode só grava o conteúdo). O teto é esse resto. Etapa 9a. A classe se nomeia pelo tamanho pedido, e 0x20800 "
     "é também o faceRes dos retratos (CharSel menu_c_e.c:702, TeamSel menu_e_b.c:701, as seleções, o tabuleiro e o "
     "ranking do Ultimate Battle), e os 164 retratos descomprimem aos mesmos 0x20560 bytes: o mesmo resto, o mesmo "
     "teto. Só nas telas desses buffers (SO_NAS_TELAS), nunca no fim; os outros donos de 0x20800 (menu_y_b.c, "
     "menu_z_d.c, menu_h_b.c..., da 9c) ficam de fora (revisão da 9a, apertado e estendido na 9b)"),
    ("heap:0x68", {"igual-sem-inverter": 8},
     "gBtlChars->unk8 e unkC (BtlChar_AllocAll, btl_char_mgr.c:1287): BtlFlagSoundSet, cujo BtlFlagSound.kind é u8 "
     "(btl_char_flag.h:48; o resto da palavra é enchimento), quatro por conjunto, dois conjuntos"),
    ("heap:0x100000", {"igual-sem-inverter": 2048},
     "os buffers de pacotes da GPU (Dma_InitBuffers): as cores dos efeitos da luta gravadas byte a byte (os u8 "
     "rgba[4] dos vértices, o 3c3c3cNN; formatos/gpu.md seção 6). ERRO DE CONVERSÃO CONHECIDO, da F5: a etapa 6 viu "
     "na imagem da luta 26 primitivas com os quatro bytes da cor invertidos na PPC. Só depois de uma luta "
     "(SO_NA_LUTA; visto: 108 no máximo no essencial; 1.061 depois da luta da Missão 100, quatro adversários e 1.566 "
     "quadros, etapa 9b: o teto foi a 2.048)"),
    ("heap:0x83040", {"igual-sem-inverter": 160},
     "a tabela de ordenação (Ot_Init, gfx_ot.c:62: o pacote e as entradas num bloco só): as mesmas cores dos efeitos "
     "da luta byte a byte nos primitivos, o mesmo erro da F5 (visto: 78, no fim da luta; SO_NA_LUTA)"),
    ("heap:0x1a0800", {"igual-sem-inverter": 320, "2xu16": 660, "misto": 80, "u64": 930, "float-proximo": 16},
     "a arena do BtlPool (BtlPool_Init, btl_pool.c:29): as estruturas da cena da luta num alocador próprio, sem tipo "
     "por estrutura (u8, u16 e u64 de campo); float-proximo: as tabelas do EftGlow_BuildTables (cosf e sinf da libm "
     "do host). Só teto (visto: 244, 522, 59, 742 e 8): tipar a arena por sub-bloco fica pendente (plano, etapa 5)"),
]

# As regiões de CLASSES_CONHECIDAS que só valem depois de uma luta, nunca numa tela de menu antes dela (memoria --tela
# sem --apos-luta): as cores dos efeitos da luta byte a byte (F5). Nos menus a etapa 6 comparou as imagens (idênticas)
# e a memória das telas não tem nenhuma dessas palavras: uma que aparecesse seria cor de menu na ordem errada. Depois de
# uma luta elas ficam nos buffers de pacotes como resto (etapa 9b: a Missão 100 deixou 1.050 e 1.061, as mesmas no
# resultado e na seleção depois dele, que não as regravam), e as telas de menu dali em diante as aceitam com o teto
# (o roteiros.sh passa --apos-luta à memória de uma tela visitada depois da primeira luta do rastro).
SO_NA_LUTA = {"heap:0x100000", "heap:0x83040"}

# As regiões de CLASSES_CONHECIDAS que só valem na memória de certas telas (memoria --tela N), nunca no fim nem noutra
# tela: a classe se nomeia pelo tamanho pedido ao alocador, e outro dono do mesmo tamanho noutra tela (o faceRes dos
# retratos das seleções também pede 0x20800) ganharia a tolerância sem ter o porquê (revisão da 9a, item m6)
SO_NAS_TELAS = {"heap:0x1c000": {7},
                # (o 0x20800 também onde ele guarda um retrato: os 164 retratos descomprimem a 0x20560 bytes, como os
                # episódios, e o resto do buffer é o mesmo; visto na tela 29 depois de uma luta, 157 palavras. Etapa 9b)
                "heap:0x20800": {7, 15, 18, 21, 22, 25, 26, 29, 39, 40, 45}}

# Os dois buffers de pacotes da GPU (Dma_InitBuffers, 0x100000 bytes cada): o pacote de sprite pronto
# (GfxPostSpritePkt, src/sys/gfxm_a.c, 0x60 bytes, alinhado a 16) é um local copiado inteiro para o pacote
# (StgPanBlur_DrawView), com uv0.pad (+0x3C), uv1.pad (+0x4C) e nop (+0x58, 8 bytes) nunca iniciados: lixo da pilha,
# outro em cada build, em bits que o GS ignora. Só valem trechos "diferentes" com a forma desses campos (o memdiff
# junta palavras a até 16 bytes): começando em 12 mod 16 (uv0.pad, uv1.pad, a metade alta do nop) com 4, 0x14 ou 0x24
# bytes (até o fim do uv1.pad ou do nop), ou em 8 mod 16 (o nop) com 4 ou 8; e nenhuma palavra das classes de troca de
# ordem de um pacote (decisão 19).
PACOTE_GPU = "heap:0x100000"
FORMAS_GPU = {12: (4, 0x14, 0x24), 8: (4, 8)}
PROIBIDAS_EM_PACOTE = ("u64", "u64-com-zero", "2xu16", "misto", "float-proximo")
TETO_TRECHOS_GPU = 64   # trechos por buffer (o maior visto: 22)
TETO_DIFERENTES_GPU = 160  # palavras "diferente" por buffer (o maior visto: 88)


# As telas de menu do caminho essencial e da História (etapa 9a) em que a memória se compara no último vblank
# (roteiros.sh, vblanks_mem), com o conteúdo convertido em tempo de jogo que tem de estar vivo e conforme ali (o nome do
# conteúdo pela classe(): o pacote e a seção, ou a faixa). Sem isto, a memória no fim da luta não via nenhuma conversão
# da etapa 5b (revisão da etapa 5).
TELAS_MEM = {
    6: ["460 s1"],                                  # HistSel (menu_hist_sagas)
    7: ["460 s2", "saga", "episodio"],              # ModeMenu (menu_hist_lista, menu_saga, menu_texfile)
    8: ["460 s3", "saga", "fundo_resultado"],       # HistResult (menu_hist_resultado, menu_saga, menu_hist_fundo)
    9: ["460 s2", "saga"],                          # HistOutro (menu_hist_lista, menu_saga)
    10: ["460 s4"],                                 # HistSave (menu_hist_save)
    38: ["448 s1"],                                 # DuelMenu (menu_duelo)
    39: ["448 s2", "retrato", "estagio"],           # CharSel (menu_selecao, menu_texfile, menu_fundo)
    40: ["448 s2", "retrato", "estagio"],           # TeamSel
    44: ["461 s1", "pagina_treino"],                # Train (menu_treino, menu_texfile)
    45: ["461 s2", "retrato", "estagio"],           # CharSel do treino
    62: ["453 s1"],                                 # Option (menu_opcoes)
    # o Ultimate Battle (etapa 9b, menus-ub.md): o pacote 00451 por seção, o tabuleiro, o ranking e os cursos inteiros;
    # nas seleções, o retrato do cursor (os retratos do tabuleiro e do ranking só existem no anúncio da luta)
    13: ["451 s1"],                                 # UbMenu (menu_ub_menu)
    14: ["451 s2"],                                 # MisSel (menu_ub_selecao)
    15: ["451 s4", "retrato"],                      # SoloSel ou UbTeamSel da Missão (menu_ubsel, menu_texfile)
    16: ["451 s3"],                                 # MisResult (menu_ub_resultado)
    17: ["451 s2"],                                 # SurvSel
    18: ["451 s4", "retrato"],                      # SoloSel da Sobrevivência
    19: ["451 s3"],                                 # SurvResult
    20: ["451 s5"],                                 # SimTop (menu_sim_inicio)
    21: ["451 s4", "retrato"],                      # SoloSel do Sim Dragon
    22: ["sim_dia"],                                # SimDay (menu_sim_dia)
    23: ["451 s3"],                                 # SimResult
    24: ["451 s6"],                                 # DiscFusion (menu_discfusion)
    25: ["451 s4", "retrato"],                      # SoloSel do ranking
    26: ["ub_ranking"],                             # UbRank (menu_ub_ranking)
    27: ["451 s3"],                                 # UbResult do ranking
    28: ["ub_cursos"],                              # UbzSel (menu_ub_cursos)
    29: ["451 s4", "retrato"],                      # SoloSel dos cursos
    30: ["451 s3"],                                 # UbResult dos cursos
}


def vivos_conformes(linhas, dados):
    """{classe: [nomes]} dos conteúdos descomprimidos (#bpe) com bloco vivo e conforme nas linhas "marcas heap#N
    NOME.marcas: conforme N, fora-do-formato 0" do memdiff"""
    out = collections.defaultdict(list)
    # (um bloco com o mesmo conteúdo e as mesmas marcas de mais de um arquivo, memdiff.py "marcas iguais": vale por todos)
    iguais = {}
    for l in linhas:
        m = re.match(r"marcas iguais: (heap#\d+) = (.*) \(o mesmo conteúdo e as mesmas marcas\)$", l)
        if m:
            iguais[m.group(1)] = [n[:-len(".marcas")] for n in m.group(2).split(", ")]
    for l in linhas:
        m = re.match(r"marcas (heap#\d+) (\S+)\.marcas: conforme (\d+), fora-do-formato 0$", l)
        if not m or int(m.group(3)) == 0:
            continue
        for nome in iguais.get(m.group(1), [m.group(2)]):
            if not nome.endswith("#bpe"):
                continue
            rel = nome.replace("_", "/", 1)
            c = classe(rel, dados)
            if c is not None:
                out[c].append(rel)
    return out


def chave_regiao(nome, estado):
    """a chave de uma região do memdiff: "heap:0xPEDIDO" para um bloco do heap em uso, o nome para o resto"""
    m = re.search(r"pedido (0x[0-9a-f]+)", estado or "")
    return f"heap:{m.group(1)}" if nome.startswith("heap#") and m else nome


def conhecida(regiao, desl, tam):
    for r, d, porque in DIFERENTES_CONHECIDAS:
        if r != regiao:
            continue
        if d is None or d == desl:
            return porque
        if isinstance(d, tuple) and d[0] == "campo":
            _, passo, ini, n = d
            if ini <= desl % passo and desl % passo + tam <= ini + n:
                return porque
    return None


def classes_da_linha(texto):
    return {k: int(v) for k, v in re.findall(r"([a-z0-9-]+) (\d+)", texto)}


def memoria(caminho, pre="", tela=None, dados="gamedata", apos_luta=False, tela_fim=None):
    p = pathlib.Path(caminho)
    if not p.exists():
        print(f"{pre}DIFERENÇA: memória: sem {caminho} (a PPC não chegou ao vblank do dump?)")
        return False
    linhas = p.read_text(errors="replace").splitlines()
    ok = True
    saida = next((l for l in linhas if l.startswith("memdiff saiu com ")), None)
    if saida != "memdiff saiu com 0":
        print(f"{pre}DIFERENÇA: memória: {saida or 'o memdiff não terminou'}")
        ok = False
    heap = next((l for l in linhas if l.startswith("heap: ") and " blocos na x86" in l), None)
    if heap is None or not heap.endswith(", 0 em lugar ou tamanho diferente"):
        print(f"{pre}DIFERENÇA: memória: {heap or 'sem a linha do heap'}")
        ok = False
    else:
        print(f"{pre}memória: {heap[6:]}")
    for l in linhas:
        if l.startswith(("marcas ambíguas", "marcas iguais", "--marcas-vivas", "--marcas-tam", "livres:", "--tipos:",
                         "--donos:")):
            print(f"{pre}  {l}")
    if not any(l.startswith("--tipos: ") for l in linhas):
        print(f"{pre}DIFERENÇA: memória: o memdiff sem --tipos (as globais sem o tipo)")
        ok = False
    if not any(l.startswith("--donos: ") for l in linhas):
        print(f"{pre}DIFERENÇA: memória: o memdiff sem --donos (os blocos do heap sem o tipo pedido)")
        ok = False
    marcas = [l for l in linhas if l.startswith("marcas heap")]
    fora = [l for l in marcas if not l.endswith("fora-do-formato 0")]
    if not marcas:
        print(f"{pre}DIFERENÇA: memória: nenhum bloco vivo com arquivo ou conteúdo convertido achado")
        ok = False
    conf = sum(int(re.search(r"conforme (\d+)", l).group(1)) for l in marcas)
    print(f"{pre}  {len(marcas)} blocos com arquivo ou conteúdo convertido, {conf} números e bytes conformes")
    for l in fora:
        print(f"{pre}DIFERENÇA: memória: {l}")
        ok = False
    if tela is not None and not str(tela).isdigit():
        # um passo "marca memoria CONTEÚDO" (vblanks_mem): aquele conteúdo vivo e conforme
        rel = str(tela).replace("_", "/", 1)
        if any(re.match(r"marcas heap#\d+ " + re.escape(str(tela)) + r"\.marcas: conforme [1-9]\d*, fora-do-formato 0$", l)
               for l in linhas):
            print(f"{pre}  {rel} vivo e conforme")
        else:
            print(f"{pre}DIFERENÇA: memória: {rel} sem bloco vivo e conforme no vblank da marca")
            ok = False
    elif tela is not None:
        tela = int(tela)
        if tela not in TELAS_MEM:
            print(f"{pre}DIFERENÇA: memória: tela {tela} fora de TELAS_MEM")
            return False
        vivos = vivos_conformes(linhas, dados)
        for c in TELAS_MEM[tela]:
            if c in vivos:
                print(f"{pre}  tela {tela}: {c} vivo e conforme ({', '.join(sorted(vivos[c]))})")
            else:
                print(f"{pre}DIFERENÇA: memória: tela {tela} sem o conteúdo {c} vivo e conforme (sem bloco não prova "
                      "a conversão)")
                ok = False
    # região a região: as classes suspeitas contra CLASSES_CONHECIDAS, e os trechos um a um contra a lista
    regiao, chave_r, esperados, vistos = None, None, 0, 0
    explicados, suspeitas, somadas = collections.Counter(), collections.Counter(), collections.Counter()

    def fecha():
        nonlocal ok
        if regiao is not None and vistos != esperados:
            print(f"{pre}DIFERENÇA: memória: {regiao} com {esperados} trechos e {vistos} listados (aumente o --max)")
            ok = False
    for l in linhas:
        m = re.match(r"região (\S+?)(?: \((.*?)\))? x86 \S+ ppc \S+ \(\+[0-9a-f]+\): (.*); (\d+) trechos a olhar$", l)
        if m:
            fecha()
            regiao, esperados, vistos = m.group(1), int(m.group(4)), 0
            chave_r = chave_regiao(regiao, m.group(2))
            # (num bloco do heap, o deslocamento dos trechos conta do começo dos dados, HeapBlock.data: o enchimento de
            # alinhamento entre o cabeçalho e os dados muda de um bloco para outro)
            md0 = re.search(r"dados \+(0x[0-9a-f]+)", m.group(2) or "")
            dados0 = int(md0.group(1), 16) if md0 else 0
            cl = classes_da_linha(m.group(3))
            for c in SUSPEITAS:
                somadas[c] += cl.get(c, 0)
            if chave_r == PACOTE_GPU:
                ruins = [c for c in PROIBIDAS_EM_PACOTE if cl.get(c)]
                if ruins:
                    print(f"{pre}DIFERENÇA: memória: {regiao} (pacotes da GPU) com palavras {', '.join(ruins)}: "
                          "troca de ordem num pacote, não lixo")
                    ok = False
                if esperados > TETO_TRECHOS_GPU or cl.get("diferente", 0) > TETO_DIFERENTES_GPU:
                    print(f"{pre}DIFERENÇA: memória: {regiao} (pacotes da GPU) com {esperados} trechos e "
                          f"{cl.get('diferente', 0)} palavras diferentes (tetos {TETO_TRECHOS_GPU} e "
                          f"{TETO_DIFERENTES_GPU})")
                    ok = False
            for c in SUSPEITAS:
                n = cl.get(c, 0)
                if not n or (chave_r == PACOTE_GPU and c in PROIBIDAS_EM_PACOTE):
                    continue  # (nos pacotes da GPU as proibidas já são diferença, acima)
                teto = next((t[c] for r, t, _ in CLASSES_CONHECIDAS if r == chave_r and c in t), None)
                if tela is not None and chave_r in SO_NA_LUTA and not apos_luta:
                    teto = None  # (numa tela de menu antes de qualquer luta, nenhuma: SO_NA_LUTA)
                # (no fim, a tela em que o roteiro termina: o historia_novo acaba na lista de episódios, tela 7)
                t_cl = tela if tela is not None else tela_fim
                fora_da_tela = chave_r in SO_NAS_TELAS and not (t_cl is not None and str(t_cl).isdigit() and
                                                                  int(t_cl) in SO_NAS_TELAS[chave_r])
                if fora_da_tela:
                    teto = None  # (só nas telas de SO_NAS_TELAS)
                if teto is None or n > teto:
                    print(f"{pre}DIFERENÇA: memória: {regiao} ({chave_r}) com {n} palavras {c}" +
                          (f", acima do teto {teto}" if teto is not None else
                           ", numa tela de menu antes da luta (só depois dela: SO_NA_LUTA)" if tela is not None and
                           chave_r in SO_NA_LUTA and not apos_luta else
                           f", fora das telas {sorted(SO_NAS_TELAS[chave_r])} (SO_NAS_TELAS)" if fora_da_tela else
                           ", classe não explicada ali (CLASSES_CONHECIDAS)"))
                    ok = False
                else:
                    suspeitas[c] += n
            continue
        m = re.match(r"  ([0-9a-f]{8}) \+([0-9a-f]+)\s+\((\S+?)\+0x([0-9a-f]+)\)", l)
        if m and regiao is not None:
            vistos += 1
            ender, tam, d = int(m.group(1), 16), int(m.group(2), 16), int(m.group(4), 16)
            if chave_r == PACOTE_GPU:
                if tam in FORMAS_GPU.get(ender % 16, ()):
                    explicados["pacotes da GPU"] += 1
                else:
                    print(f"{pre}DIFERENÇA: memória: {regiao}+0x{d:x} (+0x{tam:x} bytes, endereço {ender % 16} mod 16) "
                          "nos pacotes da GPU fora da forma de uv0.pad, uv1.pad e nop")
                    ok = False
                continue
            porque = conhecida(chave_r, d - dados0, tam)
            if porque is None:
                print(f"{pre}DIFERENÇA: memória: {regiao}+0x{d:x} ({chave_r}" +
                      (f", dados+0x{d - dados0:x}" if dados0 else "") + f", +0x{tam:x} bytes) diferente e não explicado")
                ok = False
            else:
                explicados[chave_r] += 1
            continue
        if m is None and re.match(r"  [0-9a-f]{8} \+", l) and regiao is not None:
            vistos += 1
            print(f"{pre}DIFERENÇA: memória: trecho sem nome em {regiao}: {l.strip()}")
            ok = False
    fecha()
    tot = next((l for l in linhas if l.startswith("total: ")), "")
    print(f"{pre}  palavras: {tot[7:]}")
    total = classes_da_linha(tot[7:])
    for c in SUSPEITAS:
        if total.get(c, 0) != somadas[c]:
            print(f"{pre}DIFERENÇA: memória: {total.get(c, 0)} palavras {c} no total e {somadas[c]} nas regiões "
                  "listadas (palavra suspeita fora de uma linha de região)")
            ok = False
    if explicados:
        print(f"{pre}  trechos diferentes explicados (DIFERENTES_CONHECIDAS e pacotes da GPU): " +
              ", ".join(f"{r} {n}" for r, n in sorted(explicados.items())))
    if suspeitas:
        print(f"{pre}  classes suspeitas explicadas (CLASSES_CONHECIDAS): " +
              ", ".join(f"{c} {n}" for c, n in sorted(suspeitas.items())))
    return ok


def semmapa(a):
    ppc, x86, gdb = a.arqs[:3]
    saida = pathlib.Path(ppc).read_text(errors="replace").splitlines()
    tp, tx = telas(ppc), telas(x86)
    lp, lx = leu(ppc), leu(gdb)
    fim = any(re.match(r"roteiro: \S+ fim vblank", l) for l in tp)
    bloqueio = None
    for l in saida:
        for r in BLOQUEIO:
            m = r.search(l)
            if m and bloqueio is None:
                bloqueio = (m.group(1), m.group(2) if r.groups > 1 else "Bpe_Decode (arquivo sem conversor)")
    ok = True
    if not lx:
        print("  DIFERENÇA: a x86 sob o gdb não tem linhas LEU (o unpack.gdb mudou?)")
        ok = False
    if not lp:
        print("  DIFERENÇA: a PPC sem linhas LEU (sem BT3_LEU=1, ou a build sem o inverter.c)")
        ok = False
    ultima = next((l for l in reversed(tp) if l.startswith("tela ")), "nenhuma tela")
    if fim and bloqueio is None:
        print(f"ppc sem mapa: chegou ao fim do roteiro ({ultima})")
        if tp != tx:
            if comeco(tp, tx, "rastro (telas, mcflow, esperas)"):
                print(f"  DIFERENÇA: rastro com {len(tp)} linhas, a x86 com {len(tx)}")
            ok = False
        else:
            print(f"  rastro igual ao da x86 ({len(tp)} linhas)")
        if lp != lx:
            if comeco(lp, lx, "arquivos lidos (LEU)"):
                print(f"  DIFERENÇA: {len(lp)} leituras, a x86 {len(lx)}")
            ok = False
        else:
            print(f"  arquivos lidos iguais aos da x86 ({len(lp)} leituras)")
        if a.hash:
            luta_x = any(" hp " in l for l in pathlib.Path(a.hash[0]).read_text(errors="replace").splitlines())
            if luta_x:
                r = subprocess.run([sys.executable, str(ROOT / "port/x360/tools/compara.py"), a.hash[0], a.hash[1],
                                    "--big"], capture_output=True, text=True)
                print("  luta (BT3_HASH): " + r.stdout.strip().replace("\n", "\n  "))
                ok &= r.returncode == 0
            else:
                print("  luta (BT3_HASH): o roteiro não chega à luta")
        # o quarto item do critério (plano, F4 etapa 5): a memória no último vblank antes do fim, campo a campo pelas
        # marcas de cada arquivo e conteúdo vivo
        if not a.mem:
            print("  FALTA: a memória no fim (--mem, a saída do memdiff.py): sem ela a rodada inteira não está provada")
            ok = False
        else:
            fim_t = [l.split()[1] for l in tx if l.startswith("tela ")]
            ok &= memoria(a.mem, "  ", tela_fim=fim_t[-1] if fim_t and fim_t[-1].isdigit() else None)
    elif bloqueio is not None:
        print(f"ppc sem mapa: próximo bloqueio {bloqueio[0]} (o conteúdo cru até o {bloqueio[1]}); "
              f"última {ultima}")
        if comeco(tp, tx, "rastro (telas, mcflow, esperas)"):
            print(f"  rastro igual ao começo do da x86 ({len(tp)} de {len(tx)} linhas)")
        else:
            ok = False
        if comeco(lp, lx, "arquivos lidos (LEU)"):
            print(f"  arquivos lidos iguais ao começo dos da x86 ({len(lp)} de {len(lx)} leituras)")
        else:
            ok = False
        if bloqueio[0] not in {f for _, f in lista_unpacks().values()}:
            print(f"  DIFERENÇA: {bloqueio[0]} não é uma função que chama Sprite_Unpack (unpacks.py)")
            ok = False
        print("  luta (BT3_HASH) e memória no fim: não alcançadas (bloqueio)")
    else:
        print(f"ppc sem mapa: parou sem chegar ao fim nem a um bloqueio nomeado; última {ultima}")
        for l in saida[-200:]:
            if l.startswith(("conv:", "inverter:", "roteiro: FALHA", "gancho", "qemu:")) or "Sprite_Unpack" in l:
                print(f"  {l[:300]}")
        ok = False
    print("igual à x86" if ok else "DIFERENTE da x86")
    return 0 if ok else 1


# As imagens das telas (F4, etapa 6; port/x360/headless/imagens.c, roteiros.sh com IMG=1): o rasterizador de software
# da x86 e o da PPC sem mapa no mesmo momento de cada tela, ancorado numa linha do rastro (não num vblank), e as
# imagens comparadas byte a byte. As telas de menu do essencial e da História (o menu principal e as de TELAS_MEM): logo depois de
# entrar (ENTRADA quadros depois da troca, com o fade de entrada acabado) e o último quadro antes da troca seguinte (a
# tela no estado final: personagens e estágio escolhidos; SAIDA quadros antes, fora do fade de saída); a carga
# antes da luta; a luta (LUTA quadros depois de começar, só com IMG=luta); e cada "marca" do
# roteiro (os retratos 923 e 921 vivos, as páginas do Treino, salvo e carregado), cada "ok som" (a opção mudada) e
# cada pergunta do cartão ("ok mcflow 12" e "ok mcflow 2", PERGUNTA quadros depois, com o diálogo aberto).
TELAS_IMG = {4, 38, 39, 40, 44, 45, 62, 6, 7, 8, 9, 10} | set(range(13, 31))  # (o essencial, a História da 9a e o
# Ultimate Battle da 9b)
ENTRADA, SAIDA, LUTA, PERGUNTA, CARGA = 100, 100, 50, 40, 60


def eventos_rastro(caminho):
    """[(quadro, texto)] das linhas do rastro com vblank e quadro: o texto é o da linha antes do " vblank ", sem o
    "roteiro: arquivo:linha " (o mesmo que o imagens.c compara com a âncora)"""
    out = []
    for l in pathlib.Path(caminho).read_text().splitlines():
        m = re.match(r"(?:roteiro: \S+ )?(.*?) vblank \d+ quadro (\d+)$", l)
        if m:
            out.append((int(m.group(2)), m.group(1)))
    return out


def quadro_esperado(eventos, n, anc):
    """o quadro do roteiro da imagem ancorada em anc ("TEXTO" ou "TEXTO#K") n quadros depois, pelo rastro"""
    texto, _, k = anc.partition("#")
    vistas = [q for q, t in eventos if t == texto]
    k = int(k) if k else 1
    return vistas[k - 1] + n if len(vistas) >= k else None


def lista_imagens(caminho, luta=False):
    """[(nome, n, âncora)] do rastro de referência (um arquivo de "telas"): a âncora é o texto da linha antes do
    " vblank " (com "#K" a K-ésima vez), n os quadros do roteiro depois dela. A luta só com luta=True (IMG=luta): os
    efeitos dela gravam a cor dos vértices byte a byte (u8 rgba[4], formatos/gpu.md seção 6), a corrigir na F5, e a
    imagem sai diferente até lá (plano, etapa 6)"""
    out, vezes = [], collections.Counter()
    eventos = eventos_rastro(caminho)  # (quadro, texto da âncora sem o #K)
    telas_vistas = [(q, t) for q, t in eventos if t.startswith("tela ")]
    for i, (q, t) in enumerate(eventos):
        vezes[t] += 1
        anc = t if vezes[t] == 1 else f"{t}#{vezes[t]}"
        rotulo = re.sub(r"[^0-9A-Za-z]+", "_", anc).strip("_")
        if t.startswith("tela "):
            x = t.split()[1]
            if x == "luta":
                if luta:
                    out.append(("luta", LUTA, anc))
                continue
            if x == "carga":
                # (a tela de carregamento antes da luta, 2D: a imagem dela, CARGA quadros depois de entrar)
                out.append((f"{rotulo}_entrada", CARGA, anc))
                continue
            if not (x.isdigit() and int(x) in TELAS_IMG):
                continue
            seguinte = next((qq for qq, tt in telas_vistas if qq > q), None)
            if seguinte is None:
                # (a última tela do roteiro, até o fim: o menu principal das Opções)
                fim = next((qq for qq, tt in eventos if tt == "fim" and qq > q), None)
                if fim is not None:
                    out.append((f"{rotulo}_entrada", min(ENTRADA, fim - q - 1), anc))
                continue
            if seguinte - q > ENTRADA:
                out.append((f"{rotulo}_entrada", ENTRADA, anc))
            # (SAIDA quadros antes da troca seguinte: as últimas dezenas de quadros são o fade de saída, até o preto;
            # numa visita curta, como a primeira do menu principal, a entrada já é a tela inteira)
            if seguinte - q - SAIDA > ENTRADA + 30:
                out.append((f"{rotulo}_saida", seguinte - q - SAIDA, anc))
        elif t.startswith("marca ") or t.startswith("ok som "):
            out.append((rotulo, 0, anc))
        elif t in ("ok mcflow 12", "ok mcflow 2"):
            out.append((rotulo, PERGUNTA, anc))
    return out


def ler_ppm(caminho):
    dados = pathlib.Path(caminho).read_bytes()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+255\s", dados)
    if not m:
        raise ValueError("não é P6")
    w, h = int(m.group(1)), int(m.group(2))
    px = dados[m.end():]
    if len(px) != w * h * 3:
        raise ValueError(f"{len(px)} bytes de pixels, esperados {w * h * 3}")
    return w, h, px


def compara_imagens(lista, dx, dp, sx=None, sp=None, telas=None):
    """as imagens da LISTA (lista_imagens) nas pastas da x86 (dx) e da PPC (dp): cada uma presente nas duas, P6 de
    640 x 448, e idêntica byte a byte. Uma diferença diz a tela (a âncora), o quadro e o vblank de cada build (das
    linhas "imagem:" das saídas sx e sp), quantos pixels diferem, o retângulo deles e o maior desvio por canal. Com o
    rastro de referência (telas), a imagem da x86 tem de ser do quadro que a lista pede (a âncora certa)."""
    def quadros(saida):
        out = {}
        if saida and pathlib.Path(saida).exists():
            for l in pathlib.Path(saida).read_text(errors="replace").splitlines():
                m = re.match(r"imagem: (\S+) quadro (\d+) vblank (\d+)", l)
                if m:
                    out[m.group(1)] = f"quadro {m.group(2)} vblank {m.group(3)}"
        return out
    qx, qp = quadros(sx), quadros(sp)
    eventos = eventos_rastro(telas) if telas else None
    erros = iguais = 0
    for nome, n, anc in lista:
        onde = f"{nome} ({anc} +{n}; x86 {qx.get(nome, 'sem linha imagem:')}, ppc {qp.get(nome, 'sem linha imagem:')})"
        a, b = pathlib.Path(dx) / f"{nome}.ppm", pathlib.Path(dp) / f"{nome}.ppm"
        if not a.exists() or not b.exists():
            print(f"  DIFERENÇA: {onde}: imagem faltando ({'x86' if not a.exists() else ''}"
                  f"{' ' if not a.exists() and not b.exists() else ''}{'ppc' if not b.exists() else ''})")
            erros += 1
            continue
        if qx.get(nome) != qp.get(nome):
            print(f"  DIFERENÇA: {onde}: capturada em outro momento")
            erros += 1
        if eventos is not None:
            esp = quadro_esperado(eventos, n, anc)
            got = re.match(r"quadro (\d+)", qx.get(nome, ""))
            if esp is None or got is None or int(got.group(1)) != esp:
                print(f"  DIFERENÇA: {onde}: a lista pede o quadro {esp} (a âncora no rastro de referência)")
                erros += 1
        try:
            wa, ha, pa = ler_ppm(a)
            wb, hb, pb = ler_ppm(b)
        except ValueError as e:
            print(f"  DIFERENÇA: {onde}: imagem ruim ({e})")
            erros += 1
            continue
        if (wa, ha) != (640, 448) or (wb, hb) != (640, 448):
            print(f"  DIFERENÇA: {onde}: tamanho {wa}x{ha} e {wb}x{hb}, esperado 640x448")
            erros += 1
            continue
        if a.read_bytes() == b.read_bytes():
            # (uma imagem de uma cor só, toda preta num fade, não prova nada: a lista tem de pegar a tela mostrada)
            if len(set(pa[i:i + 3] for i in range(0, len(pa), 3))) == 1:
                print(f"  DIFERENÇA: {onde}: imagem de uma cor só nas duas builds (o momento não mostra a tela)")
                erros += 1
                continue
            iguais += 1
            continue
        dif, pior, x0, y0, x1, y1 = 0, 0, 640, 448, -1, -1
        for y in range(448):
            la, lb = pa[y * 1920:(y + 1) * 1920], pb[y * 1920:(y + 1) * 1920]
            if la == lb:
                continue
            for x in range(640):
                d = max(abs(la[x * 3 + c] - lb[x * 3 + c]) for c in range(3))
                if d:
                    dif += 1
                    pior = max(pior, d)
                    x0, y0, x1, y1 = min(x0, x), min(y0, y), max(x1, x), max(y1, y)
        print(f"  DIFERENÇA: {onde}: {dif} de {640 * 448} pixels diferentes, em ({x0},{y0})-({x1},{y1}), "
              f"maior desvio {pior}" + ("" if dif else " (o cabeçalho)"))
        erros += 1
    print(f"{iguais} de {len(lista)} imagens idênticas" + ("" if not erros else f"; {erros} diferenças"))
    return 1 if erros or not lista else 0


def confere_saves(x86, ppc, telas_ref):
    """o save que cada build deixou no cartão: os mesmos valores (save_entre.campos) e, em cada um, as sagas que o
    roteiro conferiu em memória com o fim visto (SaveSlot.flags bit 2, MSLOT_OUTRO_SEEN; slot N em 0x10 + N * 0x10)"""
    import save_entre  # noqa: E402 (port/x360/tools)
    ok = True
    arqs = []
    for rot, d, ordem in (("x86", x86, "<"), ("ppc", ppc, ">")):
        achados = sorted(pathlib.Path(d).rglob(f"card1/{save_entre.DADOS}/{save_entre.DADOS}"))
        if len(achados) != 1:
            print(f"  DIFERENÇA: save da {rot}: {len(achados)} arquivos de dados em {d} (esperado 1)")
            ok = False
            continue
        arqs.append((rot, achados[0], ordem))
    if not ok:
        return 1
    r = save_entre.campos(arqs[0][1].parent, arqs[1][1].parent, False)
    ok &= r == 0
    sagas = sorted({int(m.group(1)) for l in pathlib.Path(telas_ref).read_text().splitlines()
                    for m in [re.match(r"roteiro: \S+ ok saga (\d) vblank ", l)] if m})
    for n in sagas:
        for rot, arq, ordem in arqs:
            b = arq.read_bytes()
            flags = struct.unpack_from(ordem + "i", b, 0x10 + n * 0x10)[0]
            if flags & 4:
                print(f"  saga {n}: o fim visto gravado no save da {rot} (flags 0x{flags:x})")
            else:
                print(f"  DIFERENÇA: saga {n}: o save da {rot} sem o fim visto (flags 0x{flags:x}), e a memória o "
                      "tinha (\"ok saga\")")
                ok = False
    print("saves iguais" if ok else "saves DIFERENTES")
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("acao", choices=["telas", "compara", "chamadas", "confere", "soma", "partes", "semmapa",
                                     "vblank_menu", "vblank_fim", "vblanks_mem", "memoria", "imagens", "compara_imagens",
                                     "save", "larguras"])
    ap.add_argument("arqs", nargs="*")
    ap.add_argument("--prog")
    ap.add_argument("--conv")
    ap.add_argument("--dados", default="gamedata")
    ap.add_argument("--hash", nargs=2)
    ap.add_argument("--mem")
    ap.add_argument("--tela")
    ap.add_argument("--saidas", nargs=2)
    ap.add_argument("--luta", action="store_true")
    ap.add_argument("--telas")
    ap.add_argument("--apos-luta", action="store_true")
    a = ap.parse_args()

    if a.acao == "imagens":
        for nome, n, anc in lista_imagens(a.arqs[0], luta=a.luta):
            print(nome, n, anc)
        return 0
    if a.acao == "compara_imagens":
        lista = [tuple(l.split(" ", 2)) for l in pathlib.Path(a.arqs[0]).read_text().splitlines() if l.strip()]
        lista = [(nome, int(n), anc) for nome, n, anc in lista]
        return compara_imagens(lista, a.arqs[1], a.arqs[2], *(a.saidas or (None, None)), telas=a.telas)

    if a.acao == "semmapa":
        return semmapa(a)
    if a.acao == "save":
        return confere_saves(*a.arqs[:3])
    if a.acao == "larguras":
        if len(a.arqs) < 2 or not a.prog:
            print("larguras CONV RASTREIO --prog X86R: sem o rastreio e o programa os leitores não se conferem")
            return 1
        erros = {n: r for n, r in conv(a.arqs[0]).items() if r.startswith("ERRO")}
        for n, r in sorted(erros.items()):
            print(f"  {n}: {r}")
        for k in NOTAS_CONV:
            print(f"  conhecida: {k[0]} 0x{k[1]:X} ({k[2]} lidos, {k[3]} trocados): {LARGURA_CONHECIDA[k][0]}")
        ok = not erros and NOTAS_CONV and confere_leitores(a.arqs[1], a.prog)
        print("conv_tool: só larguras conhecidas" if ok else "conv_tool: erro fora de LARGURA_CONHECIDA")
        return 0 if ok else 1
    if a.acao == "memoria":
        ok = memoria(a.arqs[0], tela=a.tela, dados=a.dados, apos_luta=a.apos_luta)
        print("memória igual" if ok else "memória DIFERENTE")
        return 0 if ok else 1
    if a.acao == "vblank_fim":
        for l in pathlib.Path(a.arqs[0]).read_text().splitlines():
            m = re.match(r"roteiro: \S+ fim vblank (\d+) ", l)
            if m:
                print(int(m.group(1)) - 1)
                return 0
        return 1
    if a.acao == "vblanks_mem":
        linhas = [l for l in pathlib.Path(a.arqs[0]).read_text().splitlines() if l.startswith("tela ")]
        vistas = set()
        for i, l in enumerate(linhas[:-1]):
            t = l.split()[1]
            if t.isdigit() and int(t) in TELAS_MEM and int(t) not in vistas:
                vistas.add(int(t))
                print(t, int(linhas[i + 1].split()[3]) - 1)
        # e os passos "marca memoria NNNNN" do roteiro: a memória naquele vblank, com o conteúdo do arquivo NNNNN da
        # partição 1 (um contêiner BPE inteiro: retrato, estágio, página) vivo e conforme
        for l in pathlib.Path(a.arqs[0]).read_text().splitlines():
            m = re.match(r"roteiro: \S+ marca memoria (\d{5}) vblank (\d+) ", l)
            if m:
                print(f"pzs3us1_{m.group(1)}.bin@0x0#bpe", m.group(2))
        return 0
    if a.acao == "vblank_menu":
        linhas = [l for l in pathlib.Path(a.arqs[0]).read_text().splitlines() if l.startswith("tela ")]
        for i, l in enumerate(linhas[:-1]):
            if l.split()[1] == "4":
                print(int(linhas[i + 1].split()[3]) - 1)
                return 0
        return 1

    if a.acao == "partes":
        partes(a.arqs[0], a.arqs[1], int(a.arqs[2]) if len(a.arqs) > 2 else 4000000)
        return 0
    if a.acao == "telas":
        print("\n".join(telas(a.arqs[0])))
        return 0
    if a.acao == "compara":
        x, y = (pathlib.Path(p).read_text().splitlines() for p in a.arqs[:2])
        for i, (l1, l2) in enumerate(zip(x, y)):
            if l1 != l2:
                print(f"DIFERENTES na linha {i + 1}:\n  {l1}\n  {l2}")
                return 1
        if len(x) != len(y):
            curto = x if len(x) < len(y) else y
            print(f"DIFERENTES: {len(x)} e {len(y)} linhas; as {len(curto)} do mais curto são o começo do outro, até "
                  f"\"{curto[-1] if curto else ''}\"")
            return 1
        print(f"iguais ({len(x)} linhas)")
        return 0

    todas = lista_unpacks()
    if a.acao == "chamadas":
        ev = chamadas(a.arqs[0], a.prog)
        cont = collections.Counter(k for _, _, k in ev)
        for k in sorted(cont, key=lambda k: min(vb for vb, _, kk in ev if kk == k)):
            s, f = todas.get(k, ("FORA DA LISTA", "?"))
            print(f"  {s:10s} {k:18s} {f:24s} {cont[k]:3d}x  {CONTEUDO.get(k, '?')}")
        return 0

    if a.acao == "soma":
        vistas = set()
        for log in a.arqs:
            vistas |= {k for _, _, k in chamadas(log, a.prog)}
        pend = {k for k, (s, _) in todas.items() if s == "pendente"}
        ess = pend & ESSENCIAL
        print(f"pendentes do essencial alcançadas: {len(ess & vistas)} de {len(ess)}")
        for k in sorted(ess - vistas):
            print(f"  FALTA (essencial): {k} {todas[k][1]}")
        print(f"pendentes dos outros modos alcançadas: {len((pend - ESSENCIAL) & vistas)} de {len(pend - ESSENCIAL)}")
        for k in sorted((pend - ESSENCIAL) - vistas):
            print(f"  não alcançada: {k} {todas[k][1]} ({CONTEUDO.get(k, '?')})")
        gancho = {k for k, (s, _) in todas.items() if s != "pendente"}
        print(f"com gancho alcançadas: {len(gancho & vistas)} de {len(gancho)}")
        return 1 if ess - vistas else 0

    # confere: as chamadas da rodada contra o "sem conversor" do --mapa da mesma rodada
    ev = chamadas(a.arqs[0], a.prog)
    vistas = {k for _, _, k in ev}
    resultado = conv(a.conv)
    erros = 0
    for nome, r in sorted(resultado.items()):
        if r.startswith("ERRO"):
            print(f"  DIFERENÇA: conv_tool em {nome}: {r[6:]}")
            erros += 1
    for k in NOTAS_CONV:
        print(f"  NOTA: {k[0]} 0x{k[1]:X} contra o mapa com outra largura ({k[2]} lidos, {k[3]} trocados), conhecida: "
              f"{LARGURA_CONHECIDA[k][0]}")
    # o que não é de menu e saiu sem conversor (fora do disc/, que o jogo lê por outro caminho): só as exceções
    # conhecidas, uma a uma
    for nome, r in sorted(resultado.items()):
        if r == "sem conversor" and not nome.startswith("disc/") and classe(nome, a.dados) is None:
            base = nome.split("@")[0]
            if base in CONHECIDOS:
                print(f"  NOTA: {nome} sem conversor, exceção conhecida (etapa 9): {CONHECIDOS[base]}")
            else:
                print(f"  DIFERENÇA: {nome} lido como número, fora das faixas de menu e sem conversor")
                erros += 1
    sem = collections.Counter()
    com = collections.Counter()
    for nome, r in resultado.items():
        if not nome.endswith("#bpe") or r.startswith("ERRO"):
            continue
        c = classe(nome, a.dados)
        if c is None:
            continue  # (as animações do lutador, Bpe_Decode direto, convertidas; o sem conversor já foi acima)
        (sem if r == "sem conversor" else com)[c] += 1
    esperado_sem = {CONTEUDO[k] for k in vistas if todas.get(k, ("",))[0] == "pendente"}
    esperado_com = {CONTEUDO[k] for k in vistas if todas.get(k, ("",))[0] != "pendente"}
    print("conteúdo sem conversor no --mapa: " + ", ".join(f"{c} ({n})" for c, n in sorted(sem.items())))
    print("conteúdo convertido no --mapa:    " + ", ".join(f"{c} ({n})" for c, n in sorted(com.items())))
    for c in sorted(esperado_sem - set(sem)):
        if c not in com:
            # o rastreio só vê o conteúdo lido como número: um descomprimido que ninguém lê não aparece (o DcList_Init
            # descomprime um retrato que o DcList_UpdateFace descomprime de novo antes de ler, menus-modos.md 3.2)
            print(f"  NOTA: chamada pendente alcançada e o conteúdo dela ({c}) nunca lido como número: nada a converter")
            continue
        print(f"  DIFERENÇA: chamada pendente alcançada e o conteúdo dela ({c}) não saiu sem conversor")
        erros += 1
    for c in sorted(set(sem) - esperado_sem):
        print(f"  DIFERENÇA: {c} saiu sem conversor e nenhuma chamada pendente alcançada o descomprime")
        erros += 1
    for c in sorted(esperado_com - set(com)):
        print(f"  DIFERENÇA: chamada com gancho alcançada e o conteúdo dela ({c}) não saiu convertido")
        erros += 1
    for c in sorted(set(com) - esperado_com):
        print(f"  DIFERENÇA: {c} saiu convertido e nenhuma chamada com gancho alcançada o descomprime")
        erros += 1
    # os arquivos de menu inteiros (antes da descompressão): os que o essencial pede no formato_de (menus-modos.md 4)
    arqs = collections.Counter()
    for nome, r in resultado.items():
        if not nome.endswith("#bpe") and r == "sem conversor":
            c = classe(nome, a.dados)
            if c is not None:
                arqs[c] += 1
    print("arquivos de menu sem conversor:    " + ", ".join(f"{c} ({n})" for c, n in sorted(arqs.items())))
    print("igual ao inventário" if erros == 0 else f"{erros} diferenças")
    return 1 if erros else 0


if __name__ == "__main__":
    sys.exit(main())
