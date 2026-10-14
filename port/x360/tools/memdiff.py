#!/usr/bin/env python3
"""F1: compara a memória do jogo de duas builds de ordem de bytes diferente num mesmo quadro.

    memdiff.py X86.dump PPC.dump PROG_X86 PROG_PPC [--max N] [--faixa INI-FIM] [--estrito] [--resumo]
               [--marcas REGIAO=ARQ.marcas]... [--marcas-tam ARQ.marcas]... [--marcas-vivas DIR]

Os .dump são os de port/src/gs/state.c (BT3_HASH=<arq> BT3_HASH_AT=<quadro> -> <arq>.dump): heap,
scratchpad e as variáveis globais do jogo (<programa>.mem, escrito por tools/mem.py).
  - heap: pareado pelos blocos do alocador do jogo (HeapBlock, magic 'TBHS', include/sys/heap.h), na ordem em
    que aparecem; bloco de tamanho diferente nos dois lados é listado (struct com layout diferente) e
    comparado só até o menor;
  - scratchpad: pelo endereço (é fixo);
  - globais: cada programa as põe num lugar; pareadas pelo nome, com o tamanho até o símbolo seguinte (as
    tabelas definidas em assembly não têm tamanho no nm -S).

Cada palavra de 4 bytes alinhada cai numa classe:
  igual            os 4 bytes invertidos (u32/float/ponteiro do jogo), ou iguais e palíndromos (0, 0xFFFFFFFF)
  igual-sem-inverter  iguais byte a byte sem ser palíndromo: certo para bytes/strings, ERRADO para u32
  ponteiro-port    endereço da imagem (a partir da base de ligação, 0x20000000) ou do heap do C, nos dois lados
  2xu16            cada metade invertida: certo se forem dois u16, ERRADO se for um u32
  u64              o par de palavras trocado (sem zero; o par pelo endereço alinhado a 8): certo se for
                   u64/double, ERRADO se forem dois u32
  u64-com-zero     idem, mas uma das palavras é 0: também pode ser coincidência
  float-proximo    floats do mesmo sinal e expoente a até 64 unidades da última casa: arredondamento (a libm do host
                   difere entre as builds, F2), não ordem de bytes
  misto            dois bytes iguais e um u16 invertido na mesma palavra (u8+u8+u16, u16+u8+u8, u8+u16+u8): certo
                   para uma struct de larguras misturadas gravada pelo código; um u32 do disco sem conversão ou
                   convertido inteiro não dá essa forma (sai igual-sem-inverter ou igual)
  diferente        o resto
Sem largura gravada não dá para separar as classes ambíguas de erro; com --estrito elas são listadas.

--marcas heap#33=DIR/pzs3us1_00449.bin@0x40#bpe.marcas (repetível): o bloco guarda esse arquivo (ou conteúdo
descomprimido) a partir do endereço de dados do bloco (HeapBlock.data); a marcação vem de conv_tool --marcas (a
largura de cada número do formato). Nesse trecho a comparação é campo a campo: número de largura w com os w bytes
invertidos e byte igual contam como "conforme"; o resto é "fora-do-formato" (listado), e isso separa o que a
classificação por palavra não separa (bytes e u16 na mesma palavra, u16 desalinhado do Flash). Um campo que o jogo
grava depois da carga (relocação) continua conforme se gravou um número nativo da mesma largura.

--marcas-tam DIR/pzs3us1_00449.bin@0xAB5C0#bpe.marcas (repetível): o mesmo, com o bloco achado pelo tamanho pedido
(HeapBlock.dataSize, +0x18, igual ao tamanho da marcação, ou até 0x800 a mais, o arredondamento da leitura de
arquivo), em vez do índice: o índice de um bloco muda de um quadro e de um roteiro para outro. Tem de haver um só
bloco em uso com esse tamanho; senão o memdiff para com erro (saída 2).

--marcas-vivas DIR (F4, etapa 5b): cada DIR/*.marcas (conv_tool --marcas sobre todos os arquivos e conteúdos que a
rodada leu) nos blocos em uso que o guardam no começo: os que pedem pelo menos o tamanho dele e têm o conteúdo
(DIR/*.orig, o conteúdo antes da conversão, que o conv_tool grava ao lado): pelo menos metade dos trechos de 64 bytes
não nulos iguais aos da x86 (o jogo grava pouco no que carrega: a relocação). Sem o .orig, o tamanho pedido como no
--marcas-tam. Sem bloco é arquivo que não
está vivo nesse quadro (ou que mora num buffer de tamanho fixo), e só sai a contagem; o mesmo arquivo em dois blocos
(um rosto carregado duas vezes) confere os dois. Um bloco que dois arquivos reclamam, ou um arquivo sem .orig com mais
de um bloco do tamanho, não se decide: uma linha "marcas ambíguas" para cada caso e a saída 2 no fim.

--so-em-uso (F4, etapa 5b): os blocos livres do heap e, nos blocos em uso, o enchimento do alocador (entre o cabeçalho
de 0x20 bytes e os dados alinhados, e do fim do tamanho pedido ao rodapé) e o HeapBlock.unk1C (que src/sys/heap.c
nunca grava) ficam fora da comparação por palavra: é resto de uso anterior que o jogo não recebe; uma linha "livres:
..." diz quanto ficou de fora. Um jogo que lesse memória não escrita se separaria da x86 no rastro ou na luta, que se
comparam à parte.

Para cada região com marcação, uma linha "marcas REGIAO ARQ: conforme N, fora-do-formato M" (também com --resumo).
"""
import bisect, os, struct, subprocess, sys
from collections import Counter

BASE = 0x20000000  # -Ttext-segment de headless.py
MAGIC = 0x53484254


def float_proximo(va, vb):
    """os dois u32 (nativos de cada build) são floats normais do mesmo sinal e expoente a até 64 unidades da última
    casa um do outro: arredondamento de float (a libm do host, F2), nunca troca de bytes, que daria outro número"""
    ea, eb = (va >> 23) & 0xFF, (vb >> 23) & 0xFF
    return va >> 31 == vb >> 31 and ea == eb and 0 < ea < 0xFF and va != vb and abs(va - vb) <= 64


def ler(caminho):
    d = open(caminho, "rb").read()
    out, o = [], 0
    while o < len(d):
        e = d.index(b"\n", o)
        nome, addr, size = d[o:e].split()
        addr, size = int(addr, 16), int(size, 16)
        out.append((nome.decode(), addr, d[e + 1:e + 1 + size]))
        o = e + 1 + size
    return out


def nm(prog, ppc):
    return subprocess.run(["powerpc-linux-gnu-nm" if ppc else "nm", "-n", "-S", prog], capture_output=True,
                          text=True).stdout.splitlines()


def faixa_do_programa(linhas):
    """Da base de ligação ao maior símbolo, mais 128 MB de heap do C."""
    ad = [int(q[0], 16) for q in (l.split() for l in linhas) if len(q) >= 3 and q[-2] in "tTdDbBrR"]
    ad = [a for a in ad if a >= BASE]
    return min(ad), max(ad) + 0x08000000


def dados(linhas):
    """{nome: (endereço, tamanho)} das variáveis, tamanho até o símbolo seguinte (ou o do nm -S, se menor)."""
    tab = []
    for l in linhas:
        q = l.split()
        if len(q) == 4 and q[2] in "bBdDrR":
            tab.append((int(q[0], 16), int(q[1], 16), q[3]))
        elif len(q) == 3 and q[1] in "bBdDrR":
            tab.append((int(q[0], 16), None, q[2]))
    out = {}
    for i, (a, n, nome) in enumerate(tab):
        viz = tab[i + 1][0] - a if i + 1 < len(tab) else 0
        t = min(n, viz) if n is not None and viz else (n if n is not None else viz)
        if a >= BASE and t:
            out[nome] = (a, t)
    return out


def ler_endereco(regioes, addr, n):
    for _, a, d in regioes:
        if a <= addr and addr + n <= a + len(d):
            return d[addr - a:addr - a + n]
    return None


def globais(x, p, lx, lp):
    """Uma "região" por variável presente nos dois programas (tamanho: o menor dos dois)."""
    sx, sp = dados(lx), dados(lp)
    gx = [r for r in x if r[0].startswith("global")]
    gp = [r for r in p if r[0].startswith("global")]
    out, fora, diferentes = [], 0, 0
    for nome, (ax, n) in sorted(sx.items(), key=lambda kv: kv[1][0]):
        if nome not in sp:
            continue
        diferentes += sp[nome][1] != n
        n = min(n, sp[nome][1])
        dx, dp = ler_endereco(gx, ax, n), ler_endereco(gp, sp[nome][0], n)
        if dx is None or dp is None:
            fora += 1
            continue
        out.append(((nome, ax, dx), (nome, sp[nome][0], dp)))
    print(f"globais: {len(out)} pareadas pelo nome ({diferentes} com espaço diferente até o vizinho; "
          f"{fora} fora das regiões do .mem)")
    return out


def blocos(d, be):
    """[(deslocamento, tamanho)] dos blocos do alocador do jogo no heap, em ordem de endereço."""
    fmt = ">" if be else "<"
    out, o = [], 0
    while o + 0x20 <= len(d):
        m, = struct.unpack_from(fmt + "I", d, o)
        if m == MAGIC:
            tam, = struct.unpack_from(fmt + "i", d, o + 0x10)
            if 0x20 <= tam and o + tam <= len(d) and struct.unpack_from(fmt + "i", d, o + tam - 4)[0] == tam:
                out.append((o, tam))
                o += tam
                continue
        o += 4
    return out


def pares_heap(nome, ax, dx, ap, dp):
    bx, bp = blocos(dx, False), blocos(dp, True)
    out, ruins = [], 0
    for i, ((ox, tx), (op, tp)) in enumerate(zip(bx, bp)):
        if tx != tp or ox != op:
            ruins += 1
            if ruins <= 10:
                print(f"heap: bloco {i} x86 {ax + ox:08x} +{tx:x}  ppc {ap + op:08x} +{tp:x}")
        n = min(tx, tp)
        out.append(((f"{nome}#{i}", ax + ox, dx[ox:ox + n]), (f"{nome}#{i}", ap + op, dp[op:op + n])))
    print(f"heap: {len(bx)} blocos na x86, {len(bp)} na ppc, {ruins} em lugar ou tamanho diferente")
    return out


def pelo_formato(ax, dx, dp, marcas):
    """Compara [data, data + len(marcas)) do bloco pela marcação do conversor. Devolve (início, fim, classes,
    trechos fora do formato) com início e fim relativos ao bloco."""
    ini = struct.unpack_from("<I", dx, 0x14)[0] - ax  # HeapBlock.data
    fim = min(ini + len(marcas), len(dx), len(dp))
    classes, trechos, k = Counter(), [], ini
    while k < fim:
        m = marcas[k - ini]
        w = m if m in (2, 4, 8) else 1
        if k + w > fim:
            w = fim - k
        a, b = dx[k:k + w], dp[k:k + w]
        if m in (2, 4, 8):
            ok = a == b[::-1]
        elif m == 1:
            ok = a == b
        else:  # livre ou continuação solta: nenhum conversor disse o que é
            ok = a == b and a == bytes(w)
        classes["conforme" if ok else "fora-do-formato"] += 1
        if not ok:
            if trechos and k - trechos[-1][1] <= 16:
                trechos[-1][1] = k + w
            else:
                trechos.append([k, k + w])
        k += w
    return ini, fim, classes, trechos


def main():
    args, opc, it = [], {}, iter(sys.argv[1:])
    marcas, por_tam, nome_marcas, vivas = {}, [], {}, []
    for a in it:
        if a == "--marcas":
            r, arq = next(it).split("=", 1)
            marcas[r] = open(arq, "rb").read()
            nome_marcas[r] = arq
        elif a == "--marcas-tam":
            por_tam.append(next(it))
        elif a == "--marcas-vivas":
            d = next(it)
            vivas += sorted(os.path.join(d, f) for f in os.listdir(d) if f.endswith(".marcas"))
        elif a in ("--max", "--faixa"):
            opc[a] = next(it)
        elif a.startswith("--"):
            opc[a] = True
        else:
            args.append(a)
    mx = int(opc.get("--max", 40))
    faixa = [int(v, 16) for v in opc["--faixa"].split("-")] if "--faixa" in opc else None
    estrito = "--estrito" in opc
    so_em_uso = "--so-em-uso" in opc
    x, p = ler(args[0]), ler(args[1])
    lx, lp = nm(args[2], False), nm(args[3], True)
    fx, fp = faixa_do_programa(lx), faixa_do_programa(lp)
    syms = sorted((int(q[0], 16), q[-1]) for q in (l.split() for l in lx) if len(q) >= 3 and q[-2] in "bBdDCsSrR")
    sa = [s[0] for s in syms]
    tot = Counter()
    rp = {n: (a, d) for n, a, d in p if not n.startswith("global")}
    pares = []
    for n, a, d in x:
        if n.startswith("global") or n not in rp:
            continue
        if n.startswith("heap"):
            pares += pares_heap(n, a, d, *rp[n])
        else:
            pares.append(((n, a, d), (n, *rp[n])))
    pares += globais(x, p, lx, lp)
    def pelo_tamanho(m, folga=0x800):
        """os blocos em uso com len(m) a len(m) + folga bytes pedidos (folga None: qualquer tamanho que caiba)"""
        achados = []
        for (nx, ax, dx), _ in pares:
            if nx.startswith("heap") and "#" in nx and len(dx) >= 0x20:
                magic, usado = struct.unpack_from("<Ii", dx, 0)
                pedido, = struct.unpack_from("<i", dx, 0x18)
                if magic == MAGIC and usado and len(m) <= pedido and (folga is None or pedido < len(m) + folga):
                    achados.append(nx)
        return achados

    for arq in por_tam:
        m = open(arq, "rb").read()
        achados = pelo_tamanho(m)
        if len(achados) != 1:
            print(f"--marcas-tam {arq}: {len(achados)} blocos em uso com {len(m):#x} bytes pedidos (é preciso um só)")
            sys.exit(2)
        print(f"marcas pelo tamanho: {achados[0]} = {arq.rsplit('/', 1)[-1]}")
        marcas[achados[0]] = m
        nome_marcas[achados[0]] = arq
    dados_x86 = {nx: (ax, dx) for (nx, ax, dx), _ in pares}

    def mesmo_conteudo(b, orig):
        """o bloco b da x86 guarda orig? (metade ou mais dos trechos de 64 bytes iguais)"""
        ax, dx = dados_x86[b]
        ini = struct.unpack_from("<I", dx, 0x14)[0] - ax  # HeapBlock.data
        d = dx[ini:ini + len(orig)]
        if len(d) < len(orig):
            return False
        # só os trechos do arquivo que não são zero contam: um bloco zerado "teria" o conteúdo de um arquivo cheio de
        # enchimento (os retratos)
        zero = bytes(64)
        trechos = [k for k in range(0, len(orig), 64) if orig[k:k + 64] != zero[:len(orig[k:k + 64])]]
        if len(trechos) < 4:
            return False
        amostra = trechos[::max(1, len(trechos) // 64)]
        if sum(d[k:k + 64] == orig[k:k + 64] for k in amostra) * 4 < len(amostra):
            return False  # (amostra antes da conta inteira: quase todo bloco não é o arquivo)
        iguais = sum(d[k:k + 64] == orig[k:k + 64] for k in trechos)
        return iguais * 2 >= len(trechos)

    ambiguas, reclamados, sem_bloco = 0, {}, 0
    for arq in vivas:
        m = open(arq, "rb").read()
        orig = arq[:-len(".marcas")] + ".orig"
        if os.path.exists(orig):
            # pelo conteúdo, em todo bloco em uso em que cabe: os buffers de tamanho fixo (os recursos do lutador,
            # BtlRes_Init) guardam o arquivo no começo e pedem mais que ele
            o = open(orig, "rb").read()
            achados = [b for b in pelo_tamanho(m, None) if b not in marcas and mesmo_conteudo(b, o)]
        else:
            achados = [b for b in pelo_tamanho(m) if b not in marcas]  # (os do --marcas e do --marcas-tam ficam)
        if len(achados) > 1 and not os.path.exists(orig):
            print(f"marcas ambíguas: {arq.rsplit('/', 1)[-1]} em {len(achados)} blocos do mesmo tamanho (sem o .orig)")
            ambiguas += 1
            continue
        if not achados:
            sem_bloco += 1
            continue
        for b in achados:
            reclamados.setdefault(b, []).append(arq)
    for b, arqs in sorted(reclamados.items()):
        if len(arqs) > 1:
            print(f"marcas ambíguas: {b} com o tamanho de {len(arqs)} arquivos: " +
                  ", ".join(a.rsplit('/', 1)[-1] for a in arqs))
            ambiguas += 1
            continue
        print(f"marcas pelo tamanho: {b} = {arqs[0].rsplit('/', 1)[-1]}")
        marcas[b] = open(arqs[0], "rb").read()
        nome_marcas[b] = arqs[0]
    if vivas:
        vivos = {a for b, arqs in reclamados.items() if len(arqs) == 1 for a in arqs}
        print(f"--marcas-vivas: {len(vivas)} arquivos e conteúdos, {len(vivos)} vivos em "
              f"{sum(len(a) == 1 for a in reclamados.values())} blocos, {sem_bloco} sem bloco, {ambiguas} ambíguos")
    listar = {"diferente"} | ({"2xu16", "u64", "u64-com-zero", "igual-sem-inverter"} if estrito else set())
    livres = Counter()
    for (nx, ax, dx), (np_, ap, dp) in pares:
        n = min(len(dx), len(dp)) // 4 * 4
        estado = ""
        if nx.startswith("heap") and "#" in nx and len(dx) >= 0x20:
            magic, usado = struct.unpack_from("<Ii", dx, 0)
            pedido, = struct.unpack_from("<i", dx, 0x18)
            estado = f" (em uso, pedido {pedido:#x})" if usado else " (livre)"
            if not usado and so_em_uso:
                livres["blocos"] += 1
                livres["bytes"] += n
                continue
        classes = Counter()
        pula = []  # (com --so-em-uso) o enchimento do alocador num bloco em uso: do cabeçalho aos dados alinhados e
        #           do fim do pedido ao rodapé, que o jogo não recebe
        if estado.startswith(" (em uso") and so_em_uso:
            dados_ini = struct.unpack_from("<I", dx, 0x14)[0] - ax
            # (e o HeapBlock.unk1C, +0x1C do cabeçalho, que o alocador nunca grava: src/sys/heap.c)
            pula = [(0x1C, 0x20), (0x20, dados_ini), (dados_ini + pedido, n - 4)]
            livres["enchimento"] += sum(max(0, b_ - a_) for a_, b_ in pula)
        trechos, ini, fim = [], None, None
        fmt_ini = fmt_fim = 0
        if nx in marcas:
            fmt_ini, fmt_fim, cf, tf = pelo_formato(ax, dx, dp, marcas[nx])
            classes.update(cf)
            trechos += [tuple(t) for t in tf]
            print(f"marcas {nx} {nome_marcas[nx].rsplit('/', 1)[-1]}: conforme {cf['conforme']}, "
                  f"fora-do-formato {cf['fora-do-formato']}")
        for o in range(0, n, 4):
            if fmt_ini <= o < fmt_fim or fmt_ini < o + 4 <= fmt_fim:
                continue
            if pula and any(a_ <= o < b_ for a_, b_ in pula):
                continue
            a, b = dx[o:o + 4], dp[o:o + 4]
            va, vb = struct.unpack("<I", a)[0], struct.unpack(">I", b)[0]
            # a outra metade de um u64 pelo endereço (alinhado a 8), não pelo deslocamento na região: um bloco do
            # heap começa em qualquer múltiplo de 4, e o par pelo deslocamento juntava metades de dois u64
            par = ((ax + o) ^ 4) - ax
            if a == b[::-1]:
                c = "igual"
            elif a == b:
                c = "igual-sem-inverter"
            elif fx[0] <= va < fx[1] and fp[0] <= vb < fp[1]:
                c = "ponteiro-port"
            elif a[:2] == b[1::-1] and a[2:] == b[:1:-1]:
                c = "2xu16"
            elif 0 <= par < n and dx[o:o + 4] == dp[par:par + 4][::-1]:
                c = "u64-com-zero" if va == 0 or dx[par:par + 4] == bytes(4) else "u64"
            elif (a[:2] == b[:2] and a[2:] == b[:1:-1]) or (a[:2] == b[1::-1] and a[2:] == b[2:]) or \
                    (a[0] == b[0] and a[3] == b[3] and a[1:3] == b[2:0:-1]):
                c = "misto"  # u8, u8 e u16 (ou u16, u8, u8; ou u8, u16, u8) na mesma palavra, cada um na sua ordem
            elif float_proximo(va, vb):
                c = "float-proximo"
            else:
                c = "diferente"
            classes[c] += 1
            if c in listar:
                if ini is not None and o - fim <= 16:
                    fim = o + 4
                else:
                    if ini is not None:
                        trechos.append((ini, fim))
                    ini, fim = o, o + 4
        if ini is not None:
            trechos.append((ini, fim))
        trechos.sort()
        tot.update(classes)
        if not trechos or "--resumo" in opc:
            continue
        print(f"região {nx}{estado} x86 {ax:08x} ppc {ap:08x} (+{n:x}): " +
              ", ".join(f"{k} {v}" for k, v in classes.most_common()) + f"; {len(trechos)} trechos a olhar")
        if faixa:
            trechos = [t for t in trechos if faixa[0] <= ax + t[0] < faixa[1]]
        for ini, fim in trechos[:mx]:
            addr = ax + ini
            i = bisect.bisect_right(sa, addr) - 1
            onde = f" ({syms[i][1]}+0x{addr - syms[i][0]:x})" if i >= 0 and addr - syms[i][0] < 0x10000 else ""
            if nx.startswith("heap") and "#" in nx:
                onde = f" ({nx}+0x{ini:x})"
            print(f"  {addr:08x} +{fim - ini:<5x}{onde}  x86 {dx[ini:min(fim, ini + 16)].hex(' ', 4)}  ppc {dp[ini:min(fim, ini + 16)].hex(' ', 4)}")
    if so_em_uso:
        print(f"livres: {livres['blocos']} blocos livres do heap fora da comparação ({livres['bytes']:#x} bytes), e "
              f"{livres['enchimento']:#x} bytes de enchimento do alocador nos blocos em uso")
    print("total: " + ", ".join(f"{k} {v}" for k, v in tot.most_common()))
    if vivas and ambiguas:
        sys.exit(2)


main()
