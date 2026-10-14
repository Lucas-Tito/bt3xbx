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
    roteiros.py vblanks_mem TELAS          as telas de menu do essencial que o rastro visita (TELAS_MEM), uma linha
                                            "TELA VBLANK" com o último vblank da primeira visita: o roteiros.sh compara
                                            a memória das duas builds também ali, com o pacote e as imagens da tela
                                            vivos (no fim da luta já foram liberados)
    roteiros.py memoria MEMDIFF [--tela N]  com --tela, também exige vivos e conformes os conteúdos da tela
                                            (TELAS_MEM): "sem bloco" não serve; com --tela CONTEÚDO (de um passo
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
    "menu_a_d.c:76": "episodio", "menu_a_d.c:126": "460 s2", "menu_a_d.c:128": "saga",
    "menu_b_c.c:79": "460 s2", "menu_b_c.c:107": "saga", "menu_b_d.c:147": "460 s1",
    "menu_c_c.c:180": "460 s3", "menu_c_c.c:182": "saga", "menu_c_c.c:191": "fundo_resultado",
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
    "menu_l_b.c:83": "retrato", "menu_l_b.c:148": "451 s4", "menu_l_b.c:254": "retrato",
    "menu_m_b.c:184": "retrato", "menu_m_b.c:266": "451 s4", "menu_m_b.c:421": "retrato",
    "menu_n_b.c:98": "ub_cursos", "menu_n_c.c:140": "ub_ranking", "menu_n_c.c:714": "retrato",
    "menu_n_c.c:715": "retrato", "menu_n_d.c:53": "451 s3", "menu_o_b.c:63": "451 s6", "menu_o_d.c:146": "451 s2",
    "menu_p_b.c:67": "451 s3", "menu_p_c.c:70": "451 s1",
    "menu_q_b.c:899": "sim_dia", "menu_q_b.c:1970": "retrato", "menu_q_b.c:1971": "retrato",
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
    # achados na correção da revisão da etapa 4 (o "confere" antigo pulava o que não é de menu): ids 0x1FF, 0x231 e
    # 0x2C7, entre os de cenário e os de menu, lidos na luta do primeiro episódio; quem os lê e o formato, a levantar
    "pzs3us1/00510.bin": "História, luta do primeiro episódio (id 0x1FF)",
    "pzs3us1/00560.bin": "História, luta do primeiro episódio (id 0x231)",
    "pzs3us1/00710.bin": "História, luta do primeiro episódio (id 0x2C7)",
}

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


def conv(caminho):
    """{nome: "sem conversor" | "convertido" | "ERRO: mensagem"} das linhas do conv_tool. Tudo que não é "convertido"
    nem "sem conversor" é erro (conv_tool.c, um(): "NOME: mensagem; cobertura ...", "não abre", "BPE inválido"), e a
    linha "  contra o mapa: ..." que segue a de um arquivo o torna erro com posições de outra largura ou não trocadas."""
    out = {}
    ultimo = None
    for l in pathlib.Path(caminho).read_text(errors="replace").splitlines():
        m = re.match(r"  contra o mapa: (\d+) posições; (\d+) iguais, (\d+) com outra largura, (\d+) não trocadas", l)
        if m:
            if ultimo is None:
                out["?"] = f"ERRO: linha \"contra o mapa\" sem arquivo antes: {l.strip()}"
            elif (int(m.group(3)) or int(m.group(4))) and not out[ultimo].startswith("ERRO"):
                out[ultimo] = f"ERRO: {l.strip()}"
            continue
        if l.startswith(" ") or not l.strip():
            continue  # (o detalhe das posições divergentes, "    0x...: rastreio leu ...", já contado na de cima)
        m = re.match(r"(\S+): (.*)$", l)
        if not m:
            out[l] = f"ERRO: linha que o roteiros.py não entende: {l}"
            ultimo = None
            continue
        ultimo, r = m.group(1), m.group(2)
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


# A memória das duas builds num vblank (memdiff.py --so-em-uso --marcas-vivas, gravado pelo roteiros.sh): o critério
# da etapa 5.
#   - o memdiff terminou (a linha "memdiff saiu com 0" que o roteiros.sh acrescenta; 2 é marca ambígua ou bloco que
#     falta);
#   - o heap no lugar: os mesmos blocos, de mesmo tamanho, nas duas builds;
#   - todo bloco vivo que guarda um arquivo ou um conteúdo convertido (achado pelo conteúdo) conforme número a número
#     pela largura que o conversor deu ("marcas ...: conforme N, fora-do-formato 0"), e pelo menos um;
#   - cada trecho de palavras "diferentes" fora desses blocos explicado em DIFERENTES_CONHECIDAS (o que o port ou o
#     ambiente deixam diferente sem ser ordem de bytes), e nenhum trecho sem listar (o memdiff lista até --max por
#     região: a contagem "N trechos a olhar" tem de bater com os listados).
# As classes "misto" (u8, u8 e u16 na mesma palavra) e "float-proximo" (arredondamento) não são diferença de ordem de
# bytes (memdiff.py) e contam à parte.
DIFERENTES_CONHECIDAS = [
    # (região, deslocamento ou None para a região inteira, por quê); formatos/menus-telas.md 8.4
    ("heap#0", 0x290, "SndMgr.iopBuf (Snd_Init): ponteiro do host (sceSifAllocIopHeap, calloc em 32 bits)"),
    ("heap#21", 0x34, "SaveData.unk0 e unk4 (Save_Init): a soma McCard_Checksum sobre os bytes de números nativos, "
                      "outra em cada build e conferida em cada uma"),
    ("gColWalkBox", 0, "ponteiro para um local da pilha do host (ColMesh_WalkBox; gravado e nunca lido)"),
    ("gColWalkCtx", 0, "idem"),
    ("gPortDataStripped", 0, "as tabelas do executável: a PPC liga o esqueleto e copia do SLUS (decisão 13)"),
    ("gSndRpcBuf", 0, "união s32/s16: a metade alta do count u32 da fila de som (RPC 0xD) sob o h[0] = 0 do RPC 0xF, "
                      "resto que ninguém lê"),
    ("gPortMoviePath", None, "caminho do host (BT3_DATA diferente nas duas rodadas)"),
    ("gPortAdxView", None, "caminhos do host (BT3_DATA diferente nas duas rodadas)"),
    ("sMc", None, "caminhos do host do cartão de memória (BT3_SAVES diferente nas duas rodadas)"),
    # os dois buffers de pacotes da GPU (Dma_InitBuffers): o pacote de sprite pronto (GfxPostSpritePkt, src/sys/
    # gfxm_a.c) é um local copiado inteiro para o pacote (StgPanBlur_DrawView), com uv0.pad, uv1.pad e nop nunca
    # iniciados: lixo da pilha, outro em cada build, em bits que o GS ignora (a metade alta de UV, o dado do NOP). Só
    # vale sem nenhuma palavra das classes de troca de ordem de um pacote (decisão 19: u64, u64-com-zero, 2xu16,
    # misto) na região
    ("heap#24", None, "pacotes da GPU: campos não iniciados do pacote de sprite copiados da pilha (o GS os ignora)"),
    ("heap#25", None, "idem, o outro buffer"),
]
# classes que, numa região aceita por inteiro na lista acima, denunciariam erro de ordem de bytes e não lixo
PROIBIDAS_EM_PACOTE = ("u64", "u64-com-zero", "2xu16", "misto", "float-proximo")


# As telas de menu do caminho essencial em que a memória se compara no último vblank (roteiros.sh, vblanks_mem), com o
# conteúdo convertido em tempo de jogo que tem de estar vivo e conforme ali (o nome do conteúdo pela classe(): o pacote e
# a seção, ou a faixa). Sem isto, a memória no fim da luta não via nenhuma conversão da etapa 5b (revisão da etapa 5).
TELAS_MEM = {
    38: ["448 s1"],                                 # DuelMenu (menu_duelo)
    39: ["448 s2", "retrato", "estagio"],           # CharSel (menu_selecao, menu_texfile, menu_fundo)
    40: ["448 s2", "retrato", "estagio"],           # TeamSel
    44: ["461 s1", "pagina_treino"],                # Train (menu_treino, menu_texfile)
    45: ["461 s2", "retrato", "estagio"],           # CharSel do treino
    62: ["453 s1"],                                 # Option (menu_opcoes)
}


def vivos_conformes(linhas, dados):
    """{classe: [nomes]} dos conteúdos descomprimidos (#bpe) com bloco vivo e conforme nas linhas "marcas heap#N
    NOME.marcas: conforme N, fora-do-formato 0" do memdiff"""
    out = collections.defaultdict(list)
    for l in linhas:
        m = re.match(r"marcas heap#\d+ (\S+)\.marcas: conforme (\d+), fora-do-formato 0$", l)
        if not m or not m.group(1).endswith("#bpe") or int(m.group(2)) == 0:
            continue
        rel = m.group(1).replace("_", "/", 1)
        c = classe(rel, dados)
        if c is not None:
            out[c].append(rel)
    return out


def conhecida(regiao, desl):
    for r, d, porque in DIFERENTES_CONHECIDAS:
        if r == regiao and (d is None or d == desl):
            return porque
    return None


def memoria(caminho, pre="", tela=None, dados="gamedata"):
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
        if l.startswith(("marcas ambíguas", "--marcas-vivas", "--marcas-tam", "livres:")):
            print(f"{pre}  {l}")
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
    # os trechos "diferentes" de cada região, um a um contra a lista
    regiao, esperados, vistos, explicados = None, 0, 0, collections.Counter()

    def fecha():
        nonlocal ok
        if regiao is not None and vistos != esperados:
            print(f"{pre}DIFERENÇA: memória: {regiao} com {esperados} trechos e {vistos} listados (aumente o --max)")
            ok = False
    for l in linhas:
        m = re.match(r"região (\S+?)(?: \(.*?\))? x86 .*; (\d+) trechos a olhar", l)
        if m:
            fecha()
            regiao, esperados, vistos = m.group(1), int(m.group(2)), 0
            if regiao in ("heap#24", "heap#25"):
                ruins = [c for c in PROIBIDAS_EM_PACOTE if re.search(r"\b" + re.escape(c) + r" \d", l)]
                if ruins:
                    print(f"{pre}DIFERENÇA: memória: {regiao} (pacotes da GPU) com palavras {', '.join(ruins)}: "
                          "troca de ordem num pacote, não lixo")
                    ok = False
            continue
        m = re.match(r"  [0-9a-f]{8} \+([0-9a-f]+)\s+\((\S+?)\+0x([0-9a-f]+)\)", l)
        if m and regiao is not None:
            vistos += 1
            r, d = m.group(2), int(m.group(3), 16)
            porque = conhecida(r, d)
            if porque is None:
                print(f"{pre}DIFERENÇA: memória: {r}+0x{d:x} (+0x{m.group(1)} bytes) diferente e não explicado")
                ok = False
            else:
                explicados[r] += 1
            continue
        if m is None and re.match(r"  [0-9a-f]{8} \+", l) and regiao is not None:
            vistos += 1
            print(f"{pre}DIFERENÇA: memória: trecho sem nome em {regiao}: {l.strip()}")
            ok = False
    fecha()
    tot = next((l for l in linhas if l.startswith("total: ")), "")
    print(f"{pre}  palavras: {tot[7:]}")
    if explicados:
        print(f"{pre}  trechos diferentes explicados (DIFERENTES_CONHECIDAS): " +
              ", ".join(f"{r} {n}" for r, n in sorted(explicados.items())))
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
            ok &= memoria(a.mem, "  ")
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("acao", choices=["telas", "compara", "chamadas", "confere", "soma", "partes", "semmapa",
                                     "vblank_menu", "vblank_fim", "vblanks_mem", "memoria"])
    ap.add_argument("arqs", nargs="*")
    ap.add_argument("--prog")
    ap.add_argument("--conv")
    ap.add_argument("--dados", default="gamedata")
    ap.add_argument("--hash", nargs=2)
    ap.add_argument("--mem")
    ap.add_argument("--tela")
    a = ap.parse_args()

    if a.acao == "semmapa":
        return semmapa(a)
    if a.acao == "memoria":
        ok = memoria(a.arqs[0], tela=a.tela, dados=a.dados)
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
