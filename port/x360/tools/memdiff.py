#!/usr/bin/env python3
"""F1: compara a memória do jogo de duas builds de ordem de bytes diferente num mesmo quadro.

    memdiff.py X86.dump PPC.dump PROG_X86 PROG_PPC [--max N] [--faixa INI-FIM] [--estrito] [--resumo]
               [--marcas REGIAO=ARQ.marcas]... [--marcas-tam ARQ.marcas]... [--marcas-vivas DIR] [--tipos BASE]
               [--donos BASE]

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
de um bloco do tamanho, não se decide: uma linha "marcas ambíguas" para cada caso e a saída 2 no fim. A menos que os
arquivos que reclamam o bloco tenham o mesmo conteúdo (.orig) e as mesmas marcas (F4, etapa 9a: o diálogo do save da
História é o da tela do cartão): então as marcas são as mesmas, e uma linha "marcas iguais: heap#N = a, b" diz quais.

Cauda de buffer reaproveitado (F7, piloto da transformação): num bloco com o arquivo achado pelo --marcas-vivas que
seja um dos três buffers fixos dos recursos do lutador (BUFFERS_FIXOS: modelo, anim0 e anim1, src/battle/btl_obj.c,
zerados no BtlObj_Init e recarregados no mesmo lugar pela troca de forma, BtlRes_SetFileId), o que fica depois do
arquivo é o resto do arquivo anterior maior. Esse resto se confere pelas marcas do arquivo anterior, achado pelo
conteúdo da x86 só nesse trecho (metade dos trechos de 64 bytes não nulos), em camadas se houver mais de um; o que
sobra depois da maior camada tem de ser igual byte a byte nas duas. Uma linha "marcas heap#N cauda ARQ [ini, fim): ..."
por camada e uma "marcas heap#N resto [ini, fim): ...", com a mesma forma "conforme N, fora-do-formato M". Dois
arquivos que conferem no mesmo trecho e dizem coisas diferentes ali: "marcas ambíguas" e a saída 2. Sem arquivo
anterior que confira, nada muda (o resto segue pela classe por palavra).

--so-em-uso (F4, etapa 5b): os blocos livres do heap e, nos blocos em uso, o enchimento do alocador (entre o cabeçalho
de 0x20 bytes e os dados alinhados, e do fim do tamanho pedido ao rodapé) e o HeapBlock.unk1C (que src/sys/heap.c
nunca grava) ficam fora da comparação por palavra: é resto de uso anterior que o jogo não recebe; uma linha "livres:
..." diz quanto ficou de fora. Um jogo que lesse memória não escrita se separaria da x86 no rastro ou na luta, que se
comparam à parte.

--tipos BASE (F4, revisão da etapa 5): as globais com tipo no DWARF (campos.py --globais PROG_X86 BASE) se comparam
campo a campo pelo tipo (pelo_tipo): o que a classe por palavra não decide (bytes e u16 na mesma palavra, um u64, um
float que só arredondou) sai "conforme" ou "fora-do-formato", e cada trecho fora do formato é listado (só bytes
contíguos). O vetor de bytes e o campo unk*/pad* da decomp são "opaco": vale qualquer leitura de uma largura só, menos
"diferente"; o que o port não usa é "morto-no-port", fora da comparação. O espaço entre o fim do tipo e o símbolo
seguinte fica na classe por palavra.

--donos BASE (idem): os blocos do heap em uso cuja chamada pediu sizeof(T) (campos.py --donos PROG VBLANK --tipos BASE,
no mesmo vblank do dump) se comparam campo a campo por T, repetido pelo tamanho pedido; a linha da região diz o tipo e
quem alocou.

Para cada região com marcação, uma linha "marcas REGIAO ARQ: conforme N, fora-do-formato M" (também com --resumo).
"""
import bisect, os, re, struct, subprocess, sys
from collections import Counter

BASE = 0x20000000  # -Ttext-segment de headless.py
MAGIC = 0x53484254
# (F7) os buffers fixos dos recursos do lutador (include/battle/btl_obj.h: BTL_RES_MODEL_SIZE, BTL_RES_ANIM0_SIZE,
# BTL_RES_ANIM1_SIZE), pelo tamanho pedido: a cauda de um arquivo anterior maior se confere pelas marcas dele
BUFFERS_FIXOS = {0xCE000: "modelo", 0x160800: "anim0", 0xCE800: "anim1"}
# as classes que são certas para uma largura e erradas para outra (ou arredondamento): fora de um bloco com marcas,
# o roteiros.py memoria exige que cada uma seja explicada (CLASSES_CONHECIDAS)
SUSPEITAS = {"igual-sem-inverter", "2xu16", "u64", "u64-com-zero", "misto", "float-proximo"}


HEAP_FAIXA = [0, 0]  # [início, fim) do heap do jogo no dump (main): ponteiro ali nunca é "float-proximo"


def float_proximo(va, vb):
    """os dois u32 (nativos de cada build) são floats normais do mesmo sinal e expoente a até 64 unidades da última
    casa um do outro: arredondamento de float (a libm do host, F2), nunca troca de bytes, que daria outro número. Um
    valor dentro da faixa do heap do jogo não entra: um ponteiro deslocado de poucos bytes (struct de tamanho errado)
    também tem o mesmo expoente (revisão da etapa 5); o roteiros.py só aceita a classe nas regiões listadas"""
    if HEAP_FAIXA[0] <= va < HEAP_FAIXA[1] or HEAP_FAIXA[0] <= vb < HEAP_FAIXA[1]:
        return False
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


def pelo_formato(ax, dx, dp, marcas, de=0):
    """Compara [data + de, data + len(marcas)) do bloco pela marcação do conversor. Devolve (início, fim, classes,
    trechos fora do formato) com início e fim relativos ao bloco (o início é o dos dados, sem o de)."""
    ini = struct.unpack_from("<I", dx, 0x14)[0] - ax  # HeapBlock.data
    fim = min(ini + len(marcas), len(dx), len(dp))
    classes, trechos, k = Counter(), [], ini + de
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


def ler_tipos(base):
    """{nome: marcas} das globais (campos.py --globais: BASE.tsv e BASE.bin)"""
    b = open(base + ".bin", "rb").read()
    out = {}
    for l in open(base + ".tsv"):
        nome, _, tam, pos = l.split()
        out[nome] = b[int(pos, 16):int(pos, 16) + int(tam, 16)]
    return out


def ler_donos(base):
    """{endereço dos dados: (tipo, marcas, quem alocou)} dos blocos do heap (campos.py --donos --tipos)"""
    b = open(base + ".bin", "rb").read()
    out = {}
    for l in open(base + ".tsv"):
        q = l.rstrip("\n").split("\t")
        out[int(q[0], 16)] = (q[1], b[int(q[3], 16):int(q[3], 16) + int(q[2], 16)], q[4] if len(q) > 4 else "?")
    return out


def pelo_tipo(dx, dp, m, fx, fp, n, base=0):
    """Compara [0, n) de uma global pelos códigos do campos.py --globais (o tipo do DWARF): inteiro de largura w com
    os bytes invertidos, byte igual, float invertido, ponteiro invertido ou ponteiro do port nos dois lados (a imagem
    e o heap do C ficam em outro lugar em cada build), enchimento igual. O resto é "fora-do-formato" (com
    "float-proximo" contado à parte quando um float só difere no arredondamento). Devolve (classes, trechos)."""
    classes, trechos, k = Counter(), [], 0
    fim = min(n, len(m))
    while k < fim:
        c = m[k]
        if c == 0xFF:  # tabela do executável que o port não usa (campos.py --globais): fora da comparação
            classes["morto-no-port"] += 1
            k += 1
            continue
        if c == 0xFE or (c == 0 and k % 4 == 0 and k + 4 <= fim and not any(m[k:k + 4])):
            # campo opaco (unk*, pad*, vetor de bytes) ou enchimento do tipo: ninguém tipou o conteúdo; por palavra,
            # aceito qualquer leitura de uma largura (u32 invertido, bytes iguais, dois u16, u8+u8+u16, o u64 com o par
            # pelo endereço, o ponteiro do port), contado à parte ("opaco"); só "diferente" é fora do formato
            w = 4 if k % 4 == 0 and k + 4 <= fim and all(x in (0, 0xFE) for x in m[k:k + 4]) else 1
            if w == 1 and k % 2 == 0 and k + 2 <= fim and all(x in (0, 0xFE) for x in m[k:k + 2]):
                # (um campo opaco que começa no meio de uma palavra, como o s16 unk29E[3] do SScore,
                # include/menu/menu_s.h, que outra visão do mesmo bloco lê como o PClock.ms: por meia palavra, a leitura
                # de uma largura só é o u16 invertido ou os dois bytes iguais; byte a byte, um u16 certo saía fora do
                # formato. Etapa 9b, a memória do SurvResult)
                w = 2
            a, b = dx[k:k + w], dp[k:k + w]
            ok = a == b or a == b[::-1]
            if not ok and w == 4:
                va, vb = struct.unpack("<I", a)[0], struct.unpack(">I", b)[0]
                par = ((base + k) ^ 4) - base
                ok = (fx[0] <= va < fx[1] and fp[0] <= vb < fp[1]) or (a[:2] == b[1::-1] and a[2:] == b[:1:-1]) or \
                    (a[:2] == b[:2] and a[2:] == b[:1:-1]) or (a[:2] == b[1::-1] and a[2:] == b[2:]) or \
                    (a[0] == b[0] and a[3] == b[3] and a[1:3] == b[2:0:-1]) or \
                    (0 <= par and par + 4 <= len(dp) and a == dp[par:par + 4][::-1])
            classes["opaco" if ok else "fora-do-formato"] += 1
            if not ok:
                if trechos and k == trechos[-1][1]:
                    trechos[-1][1] = k + w
                else:
                    trechos.append([k, k + w])
            k += w
            continue
        w = c if c in (2, 4, 8) else (c & 0xF if c in (0x44, 0x48, 0x54, 0x84) else 1)
        if k + w > fim:
            w = fim - k
        a, b = dx[k:k + w], dp[k:k + w]
        prox = False
        if c in (2, 4, 8, 0x44, 0x48, 0x54):
            ok = a == b[::-1]
            if not ok and c == 8 and w == 8:
                # um u64 de pacote da GPU fica como dois u32, a baixa primeiro (decisão 19, PS2_PKT64): o tipo do C não
                # separa o pacote de um u64 de verdade
                ok = a[:4] == b[3::-1] and a[4:] == b[:3:-1]
            if not ok and c in (0x44,0x54) and w == 4:
                prox = float_proximo(struct.unpack("<I", a)[0], struct.unpack(">I", b)[0])
        elif c == 0x84 and w == 4:
            va, vb = struct.unpack("<I", a)[0], struct.unpack(">I", b)[0]
            ok = (a == b[::-1] or (fx[0] <= va < fx[1] and fp[0] <= vb < fp[1]) or
                  PONTEIROS_PILHA.get(base + k) == (va, vb))
        else:  # byte, enchimento, continuação solta
            ok = a == b
        if c == 0x54 and prox:
            classes["libm-host"] += 1
            ok = True
        classes["conforme" if ok else "float-proximo" if prox else "fora-do-formato"] += 1
        if not ok:
            # (só junta o que é contíguo: o roteiros.py confere cada trecho contra os campos explicados)
            if trechos and k == trechos[-1][1]:
                trechos[-1][1] = k + w
            else:
                trechos.append([k, k + w])
        k += w
    return classes, [tuple(t) for t in trechos]


# Sobreposições de símbolos da bss: gCharRefState tem só 16 bytes no esqueleto, mas CharRef_Init grava nove
# handlers. Os cinco seguintes coincidem com globais de outros modos. Só os nove endereços EXATOS das funções,
# em ambas as builds e nas posições esperadas, autorizam a visão de ponteiros. menus-ref.md, seção 6.
REF_HANDLERS = ["CharRef_StateIntro", "CharRef_StateList", "CharRef_StateDetails", "CharRef_StateProfile",
                "CharRef_StateVoice", "CharRef_StateComment", "CharRef_StateCostume", "CharRef_StateView",
                "CharRef_StateViewWait"]
REF_ALIASES = ["gUbMenuPlate3Open", "gSimTrainOutcome0", "gSimTrainOutcome1", "gSimTrainOutcome2", "gSimCardShown"]


def contexto_charref(x, p, lx, lp, pares, donos):
    """Visões provadas pelos valores nativos, símbolos, estado e dono; nenhuma faixa genérica de ponteiros."""
    def simbolos(linhas):
        return {q[-1]: int(q[0], 16) for l in linhas if len(q := l.split()) >= 3}
    sx, sp = simbolos(lx), simbolos(lp)
    def palavra(regioes, addr, ordem):
        b = ler_endereco(regioes, addr, 4)
        return struct.unpack(ordem + "I", b)[0] if b is not None and len(b) == 4 else None
    aliases, vazios = set(), {}
    lados = ((x, sx, "<"), (p, sp, ">"))
    if all(all(f in s for f in REF_HANDLERS + REF_ALIASES + ["gCharRefState"]) and
               all(palavra(r, s["gCharRefState"] + i * 4, o) == s[f] for i, f in enumerate(REF_HANDLERS)) and
               all(s[f] == s["gCharRefState"] + (i + 4) * 4 for i, f in enumerate(REF_ALIASES))
           for r, s, o in lados):
        aliases.update(REF_ALIASES)
    # Dois buffers do ChrView alocados, ainda sem arquivo: só a primeira palavra foi escrita. O restante nunca
    # foi inicializado. Exige tela 60 nas duas, o trabalho vivo e tipado com o dono exato, e os campos dos slots.
    # (F5, etapa 9) Com o modelo aberto (ChrView_StepLoad terminou: loading 0, stageOn e visible 1, cur o slot que
    # recebeu o arquivo), o mesmo vale só para o OUTRO slot, que nunca recebeu arquivo; o do modelo (tamanho e id de
    # um modelo, 0x590 + chara * 10 + traje) é conteúdo vivo e fica com as marcas.
    trabalhos, em_uso = [], []
    for r, s, o in lados:
        if "gChrView" not in s or "gProgress" not in s:
            return aliases, vazios
        prog = palavra(r, s["gProgress"], o)
        work = palavra(r, s["gChrView"], o)
        if prog is None or work is None or palavra(r, prog + 0x18, o) != 60:
            return aliases, vazios
        loading, stage, visivel, cur = (palavra(r, work + d, o) for d in (0x24, 0x30, 0x34, 0x3c))
        if (loading, stage, visivel, cur) == (0, 0, 0, 0):
            em_uso.append(None)
        elif (loading, stage, visivel) == (0, 1, 1) and cur is not None and cur != 0:
            k, resto = divmod(cur - work - 0x40, 0x38)
            ficha = (palavra(r, cur + 0xc, o), palavra(r, cur + 0x10, o)) if resto == 0 and k in (0, 1) else None
            if ficha is None or ficha[0] in (None, 0) or ficha[1] is None or \
                    not 0x590 <= ficha[1] < 0x590 + 0xa1 * 10 + 10:
                return aliases, vazios
            em_uso.append((k, ficha))
        else:
            return aliases, vazios
        trabalhos.append(work)
    if em_uso[0] != em_uso[1]:
        return aliases, vazios
    em_uso = em_uso[0]
    dono = donos.get(trabalhos[0])
    if not dono or dono[0] != "ChrView" or len(dono[1]) != 0xbc or \
            not dono[2].startswith("ChrView_Init@view_a_d.c:") or " < CharRef_Init@menu_h_b.c:" not in dono[2]:
        return aliases, vazios
    works = [nome for (nome, a, b), (_, c, d) in pares
             if nome.startswith("heap#") and min(len(b), len(d)) >= 0x20
             and struct.unpack_from("<I", b, 0)[0] == MAGIC and struct.unpack_from(">I", d, 0)[0] == MAGIC
             and struct.unpack_from("<I", b, 0x14)[0] == trabalhos[0]
             and struct.unpack_from(">I", d, 0x14)[0] == trabalhos[1]
             and struct.unpack_from("<I", b, 0x18)[0] == 0xbc and struct.unpack_from(">I", d, 0x18)[0] == 0xbc
             and struct.unpack_from("<i", b, 4)[0] != 0 and struct.unpack_from(">i", d, 4)[0] != 0]
    if len(works) != 1:
        return aliases, vazios
    for i in range(2):
        if em_uso is not None and em_uso[0] == i:
            continue  # (o slot do modelo aberto: conteúdo vivo, das marcas)
        buffers = []
        for (r, s, o), work in zip(lados, trabalhos):
            ofs = work + 0x40 + i * 0x38 + 8
            if palavra(r, ofs + 4, o) != 0 or palavra(r, ofs + 8, o) != 0xffffffff:
                break
            buf = palavra(r, ofs, o)
            if buf is None or palavra(r, buf, o) != 0:
                break
            buffers.append(buf)
        if len(buffers) != 2:
            continue
        for (nome, ax, dx), (_, ap, dp) in pares:
            if not nome.startswith("heap#") or len(dx) < 0x20 or len(dp) < 0x20:
                continue
            if (struct.unpack_from("<I", dx, 0x14)[0], struct.unpack_from(">I", dp, 0x14)[0]) != tuple(buffers):
                continue
            if struct.unpack_from("<i", dx, 4)[0] == 0 or struct.unpack_from(">i", dp, 4)[0] == 0 or \
                    struct.unpack_from("<I", dx, 0x18)[0] != 0xce000 or struct.unpack_from(">I", dp, 0x18)[0] != 0xce000:
                continue
            d0 = buffers[0] - ax
            if buffers[1] - ap != d0 or d0 + 0xce000 > min(len(dx), len(dp)):
                continue
            vazios[nome] = (d0 + 4, d0 + 0xce000, i)
    return aliases, vazios


# Os dois extremos da lista do Shenlong apontam para o nó cabeça, que mora no trabalho local de Shen_Run.
# O endereço da cabeça é FlashTl.owner - offsetof(ShenWork, flash) + offsetof(ShenWork, list), isto é, owner+0x370.
PONTEIROS_PILHA = {}


def contexto_shen(pares, donos):
    raizes, clips, nos = [], [], []
    for (nome, ax, dx), (_, ap, dp) in pares:
        if not nome.startswith("heap#") or min(len(dx), len(dp)) < 0x20:
            continue
        d0 = struct.unpack_from("<I", dx, 0x14)[0] - ax
        p0 = struct.unpack_from(">I", dp, 0x14)[0] - ap
        pedido = struct.unpack_from("<I", dx, 0x18)[0]
        if struct.unpack_from("<i", dx, 4)[0] == 0 or struct.unpack_from(">i", dp, 4)[0] == 0 or \
                struct.unpack_from(">I", dp, 0x18)[0] != pedido or d0 != p0 or d0 + pedido > min(len(dx), len(dp)):
            continue
        dono = donos.get(ax + d0)
        if not dono:
            continue
        tipo, marcas, quem = dono
        if tipo == "FlashTl" and pedido == 0x18 and len(marcas) == 0x18 and \
                re.match(r"Flash_Create@gfxm_c.c:\d+ < Shen_Init@late_a.c:\d+ < Shen_Run@", quem):
            raizes.append((ax + d0, struct.unpack_from("<I", dx, d0)[0], struct.unpack_from(">I", dp, p0)[0]))
        if tipo == "FlashClip" and pedido == 0x10ec and len(marcas) == 0xe4 and \
                re.match(r"FlashClipList_Init@gfxm_c.c:\d+ < Flash_Create@gfxm_c.c:\d+ < Shen_Init@", quem):
            clips.append((ax + d0, dx[d0:d0 + pedido], dp[p0:p0 + pedido]))
        if tipo == "ShenNode" and pedido == 0x14 and len(marcas) == 0x14 and \
                re.match(r"ShenList_Build@late_a.c:\d+ < Shen_BuildList@late_a.c:\d+ < Shen_Init@", quem):
            no = struct.unpack_from("<I", dx, d0)[0]
            if no == struct.unpack_from(">I", dp, p0)[0]:
                nos.append((no, ax + d0, dx[d0:d0 + pedido], dp[p0:p0 + pedido]))
    if len(raizes) != 1 or len(clips) != 1 or sorted(n[0] for n in nos) != list(range(1, 7)):
        return {}
    raiz, ox, op = raizes[0]
    ca, cx, cp = clips[0]
    owners = (ox, op)
    if any((struct.unpack_from("<I", cx, i * 0xe4 + 0xcc)[0],
            struct.unpack_from(">I", cp, i * 0xe4 + 0xcc)[0]) != owners for i in range(19)):
        return {}
    cabecas = (ox + 0x370, op + 0x370)
    certos = {raiz: owners}
    caixas = 0
    for i in range(19):
        certos[ca + i * 0xe4 + 0xcc] = owners
        over = (struct.unpack_from("<I", cx, i * 0xe4 + 0xc8)[0], struct.unpack_from(">I", cp, i * 0xe4 + 0xc8)[0])
        if over in [(ox + 0xe4 + k * 0x8c, op + 0xe4 + k * 0x8c) for k in range(5)]:
            certos[ca + i * 0xe4 + 0xc8] = over
            caixas += 1
    print(f"ponteiros Shen: raiz e 19 owners iguais ao filme local, {caixas} overArg nas caixas provadas")
    for no, addr, dx, dp in nos:
        campo = 0xc if no == 1 else 0x10 if no == 6 else None
        if campo is not None and (struct.unpack_from("<I", dx, campo)[0], struct.unpack_from(">I", dp, campo)[0]) == cabecas:
            certos[addr + campo] = cabecas
            print(f"ponteiros Shen: nó {no} +{campo:#x}, cabeça do trabalho local conferida nas duas builds")
    return certos


def main():
    args, opc, it = [], {}, iter(sys.argv[1:])
    marcas, por_tam, nome_marcas, vivas, tipos, donos = {}, [], {}, [], {}, {}
    for a in it:
        if a == "--tipos":
            tipos = ler_tipos(next(it))
            opc[a] = True
        elif a == "--donos":
            opc["donos_base"] = next(it)
            donos = ler_donos(opc["donos_base"])
            opc[a] = True
        elif a == "--marcas":
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
        if n.startswith("heap"):
            HEAP_FAIXA[:] = [a, a + len(d)]
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
    def iguais_em_tudo(arqs):
        """os arquivos têm o mesmo conteúdo (.orig) e as mesmas marcas: o bloco guarda qualquer um deles, e as marcas
        dizem o mesmo (o diálogo do save da História, 460 s4, é o da tela do cartão, 473 s2; F4, etapa 9a)"""
        conteudos = set()
        for a in arqs:
            orig = a[:-len(".marcas")] + ".orig"
            if not os.path.exists(orig):
                return False
            conteudos.add((open(a, "rb").read(), open(orig, "rb").read()))
        return len(conteudos) == 1
    for b, arqs in sorted(reclamados.items()):
        if len(arqs) > 1 and iguais_em_tudo(arqs):
            print(f"marcas iguais: {b} = " + ", ".join(a.rsplit('/', 1)[-1] for a in arqs) +
                  " (o mesmo conteúdo e as mesmas marcas)")
            reclamados[b] = arqs = arqs[:1]
        if len(arqs) > 1:
            print(f"marcas ambíguas: {b} com o tamanho de {len(arqs)} arquivos: " +
                  ", ".join(a.rsplit('/', 1)[-1] for a in arqs))
            ambiguas += 1
            continue
        print(f"marcas pelo tamanho: {b} = {arqs[0].rsplit('/', 1)[-1]}")
        marcas[b] = open(arqs[0], "rb").read()
        nome_marcas[b] = arqs[0]
    # (F7, piloto da transformação) A cauda dos buffers fixos dos recursos do lutador (BUFFERS_FIXOS): um arquivo menor
    # carregado no mesmo buffer deixa depois dele o resto do arquivo anterior. As camadas se acham como o --marcas-vivas
    # acha o arquivo vivo, pelo conteúdo da x86 (o .orig, metade dos trechos de 64 bytes não nulos), mas só no trecho
    # depois do fim da camada de cima; o maior arquivo que confere ali vira a camada seguinte, e assim por diante.
    caudas, conteudos = {}, {}

    def orig_de(arq):
        o = arq[:-len(".marcas")] + ".orig"
        if o not in conteudos:
            conteudos[o] = open(o, "rb").read() if os.path.exists(o) else None
        return conteudos[o]

    def resto_de(d, orig, de):
        """o trecho [de, len(orig)) da x86 (d, a partir dos dados do bloco) guarda o fim de orig?"""
        zero = bytes(64)
        trechos = [k for k in range(de, len(orig), 64) if orig[k:k + 64] != zero[:len(orig[k:k + 64])]]
        if len(trechos) < 4:
            return False
        return sum(d[k:k + 64] == orig[k:k + 64] for k in trechos) * 2 >= len(trechos)

    for b in sorted(marcas):
        if nome_marcas.get(b) not in vivas or orig_de(nome_marcas[b]) is None:
            continue  # (só um bloco achado pelo conteúdo: o arquivo de cima é o que o --marcas-vivas provou vivo)
        ax, dx = dados_x86[b]
        pedido = struct.unpack_from("<i", dx, 0x18)[0]
        if pedido not in BUFFERS_FIXOS:
            continue
        d = dx[struct.unpack_from("<I", dx, 0x14)[0] - ax:]
        pos, camadas = len(marcas[b]), []
        while pos < pedido:
            achados = [a for a in vivas if orig_de(a) is not None and pos < len(orig_de(a)) <= pedido and
                       resto_de(d, orig_de(a), pos)]
            if not achados:
                break
            maior = max(achados, key=lambda a: len(orig_de(a)))
            mm = open(maior, "rb").read()
            # (dois arquivos que conferem no mesmo trecho têm de dizer o mesmo nele: conteúdo e marcas)
            outros = [a for a in achados if orig_de(a)[pos:] != orig_de(maior)[pos:len(orig_de(a))] or
                      open(a, "rb").read()[pos:] != mm[pos:len(orig_de(a))]]
            if outros or len(mm) != len(orig_de(maior)):
                print(f"marcas ambíguas: {b} cauda [{pos:#x}, ...) com " +
                      ", ".join(a.rsplit('/', 1)[-1] for a in [maior] + outros))
                ambiguas += 1
                camadas = []
                break
            camadas.append((pos, len(mm), maior, mm))
            pos = len(mm)
        if camadas:
            caudas[b] = (camadas, pos, pedido)
    if vivas:
        vivos = {a for b, arqs in reclamados.items() if len(arqs) == 1 for a in arqs}
        print(f"--marcas-vivas: {len(vivas)} arquivos e conteúdos, {len(vivos)} vivos em "
              f"{sum(len(a) == 1 for a in reclamados.values())} blocos, {sem_bloco} sem bloco, {ambiguas} ambíguos")
    listar = {"diferente"} | ({"2xu16", "u64", "u64-com-zero", "igual-sem-inverter"} if estrito else set())
    PONTEIROS_PILHA.clear()
    PONTEIROS_PILHA.update(contexto_shen(pares, donos))
    ref_aliases, ref_vazios = contexto_charref(x, p, lx, lp, pares, donos)
    for nome in sorted(ref_aliases):
        print(f"aliases CharRef: {nome}, handler exato conferido nas duas builds")
    import memoria_luta
    contexto = None
    if donos and "--tipos" in opc:
        try:
            contexto = memoria_luta.Contexto(x, p, lx, lp, args[0], args[1], opc["donos_base"], (args[2], args[3]))
        except (ValueError, OSError, KeyError) as erro:
            print(f"contexto vivo inválido: {erro}")
            sys.exit(2)
        for rotulo in contexto.rotulos:
            print(rotulo)
        print("contexto vivo: ok")
    livres = Counter()
    tipadas, tipados_heap = [0], [0]
    for (nx, ax, dx), (np_, ap, dp) in pares:
        n = min(len(dx), len(dp)) // 4 * 4
        estado = ""
        if nx.startswith("heap") and "#" in nx and len(dx) >= 0x20:
            magic, usado = struct.unpack_from("<Ii", dx, 0)
            pedido, = struct.unpack_from("<i", dx, 0x18)
            estado = (f" (em uso, pedido {pedido:#x}, dados +{struct.unpack_from('<I', dx, 0x14)[0] - ax:#x})" if usado
                      else " (livre)")
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
        if nx in ref_vazios:
            a_, b_, slot = ref_vazios[nx]
            # Não cobre conteúdo identificado pelas marcas; tampouco a primeira palavra inicializada ou o cabeçalho.
            if nx not in marcas:
                pula.append((a_, b_))
                print(f"não inicializado: {nx}, ChrView.slot[{slot}].file[0], {b_ - a_} bytes após a primeira palavra")
        trechos, ini, fim = [], None, None
        fmt_ini = fmt_fim = 0
        dono = None
        if estado.startswith(" (em uso") and nx not in marcas and donos:
            dono = donos.get(struct.unpack_from("<I", dx, 0x14)[0])
        dinamico = None
        if contexto:
            dinamico = contexto.mapas.get(ax)
            if estado.startswith(" (em uso"):
                d0 = struct.unpack_from("<I", dx, 0x14)[0] - ax
                dinamico = contexto.mapas.get(ax + d0)
            else:
                d0 = 0
        if dinamico is not None:
            ct, tt = pelo_tipo(dx[d0:], dp[d0:], dinamico, fx, fp, len(dinamico), ax + d0)
            classes.update(ct)
            trechos += [(a_ + d0, b_ + d0) for a_, b_ in tt]
            fmt_ini, fmt_fim = d0, d0 + len(dinamico)
            print(f"contexto {nx}: conforme {ct['conforme']}, fora-do-formato {ct['fora-do-formato']}, "
                  f"opaco {ct['opaco']}, inativo {ct['morto-no-port']}")
        elif nx in ref_aliases and n == 4:
            classes["conforme"] += 1
            fmt_ini, fmt_fim = 0, 4
        elif nx in tipos and not nx.startswith(("heap", "scratch")):
            # uma global com o tipo do DWARF: campo a campo, e fora disso (o espaço até o símbolo seguinte) por palavra
            ct, tt = pelo_tipo(dx, dp, tipos[nx], fx, fp, n, ax)
            classes.update(ct)
            trechos += tt
            fmt_ini, fmt_fim = 0, min(n, len(tipos[nx]))
            tipadas[0] += 1
        elif dono is not None:
            # um bloco do heap com o tipo que a chamada pediu (Heap_Alloc(sizeof(T) ...), campos.py --donos --tipos):
            # campo a campo, T repetido pelo tamanho pedido quando ele é múltiplo de sizeof(T)
            d0 = struct.unpack_from("<I", dx, 0x14)[0] - ax
            m = dono[1] * (pedido // len(dono[1]) if pedido % len(dono[1]) == 0 else 1)
            ct, tt = pelo_tipo(dx[d0:], dp[d0:], m, fx, fp, min(n - d0, len(m)), ax + d0)
            classes.update(ct)
            trechos += [(a_ + d0, b_ + d0) for a_, b_ in tt]
            fmt_ini, fmt_fim = d0, d0 + min(n - d0, len(m))
            estado = estado[:-1] + f", {dono[0]} de {dono[2]})"
            tipados_heap[0] += 1
        elif nx in marcas:
            fmt_ini, fmt_fim, cf, tf = pelo_formato(ax, dx, dp, marcas[nx])
            classes.update(cf)
            trechos += [tuple(t) for t in tf]
            print(f"marcas {nx} {nome_marcas[nx].rsplit('/', 1)[-1]}: conforme {cf['conforme']}, "
                  f"fora-do-formato {cf['fora-do-formato']}")
            if nx in caudas:
                # cada camada pelas marcas do arquivo dela, e o que sobra depois da maior igual byte a byte nas duas
                # (o BtlObj_Init zera o buffer; a leitura só grava o arquivo)
                camadas, ultimo, pedido_c = caudas[nx]
                for de, ate, arq, mm in camadas:
                    _, _, cc, tc = pelo_formato(ax, dx, dp, mm, de)
                    classes.update(cc)
                    trechos += [tuple(t) for t in tc]
                    print(f"marcas {nx} cauda {arq.rsplit('/', 1)[-1]} [{de:#x}, {ate:#x}): conforme {cc['conforme']}, "
                          f"fora-do-formato {cc['fora-do-formato']}")
                cr = Counter()
                for o in range(fmt_ini + ultimo, fmt_ini + pedido_c, 4):
                    w = min(4, fmt_ini + pedido_c - o)
                    ok = dx[o:o + w] == dp[o:o + w]
                    cr["conforme" if ok else "fora-do-formato"] += 1
                    if not ok:
                        if trechos and o - trechos[-1][1] <= 16:
                            trechos[-1] = (trechos[-1][0], o + w)
                        else:
                            trechos.append((o, o + w))
                classes.update(cr)
                print(f"marcas {nx} resto [{ultimo:#x}, {pedido_c:#x}): conforme {cr['conforme']}, "
                      f"fora-do-formato {cr['fora-do-formato']}")
                fmt_fim = fmt_ini + pedido_c
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
        # (a linha da região sai também sem trecho a listar quando a região tem palavra de uma classe que pode ser erro
        # de ordem de bytes: o roteiros.py memoria julga as classes por região, e a soma delas tem de bater com o total)
        if (not trechos and not set(classes) & SUSPEITAS) or "--resumo" in opc:
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
    if "--tipos" in opc:
        print(f"--tipos: {tipadas[0]} globais comparadas pelo tipo do DWARF, de {len(tipos)} com tipo")
    if "--donos" in opc:
        print(f"--donos: {tipados_heap[0]} blocos do heap comparados pelo tipo pedido, de {len(donos)} com tipo")
    print("total: " + ", ".join(f"{k} {v}" for k, v in tot.most_common()))
    if vivas and ambiguas:
        sys.exit(2)


if __name__ == "__main__":
    main()
