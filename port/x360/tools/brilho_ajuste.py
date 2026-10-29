#!/usr/bin/env python3
"""O brilho somado (glow) de duas capturas contra o de outras duas: forma, posição e força (F8, pendência 5).

    brilho_ajuste.py A_COM A_SEM B_COM B_SEM [--fator F] [--desloca DX,DY] [--borrao N] [--passo P]

A = A_COM - A_SEM e B = B_COM - B_SEM, na luminância inteira (299 R + 587 G + 114 B) / 1000, cada um o que o brilho
soma à imagem (por exemplo o console com BT3_GLOW=100 menos o mesmo quadro com 60, e o rasterizador depois menos antes
do desenho da soma). As duas diferenças são reduzidas a blocos de P x P pixels (padrão 4) e o B é deslocado e borrado
(N passadas de [1 2 1] / 4 nos blocos, de 0 a --borrao) até casar com o A: "A em (x, y) mostra B em (x - dx, y - dy)",
com o ganho de mínimos quadrados. Diz o encaixe sem mexer, o melhor só com ganho, e o melhor com deslocamento e
borrão, com o erro (média do quadrado nos blocos) e a correlação. --fator divide o A (0,4 para 100 menos 60: o que a
soma inteira daria). --desloca move o A de propósito (o teste do próprio script: o deslocamento achado tem de mudar
pelo mesmo tanto). Só números; nada de imagem sai daqui.
"""
import argparse
import sys


def ler(caminho):
    with open(caminho, "rb") as f:
        dados = f.read()
    campos, pos = [], 0
    while len(campos) < 4:
        while dados[pos:pos + 1].isspace():
            pos += 1
        if dados[pos:pos + 1] == b"#":
            while dados[pos:pos + 1] not in (b"\n", b""):
                pos += 1
            continue
        ini = pos
        while not dados[pos:pos + 1].isspace():
            pos += 1
        campos.append(dados[ini:pos])
    if campos[0] != b"P6" or int(campos[3]) != 255:
        sys.exit("%s: só P6 de 8 bits" % caminho)
    w, h = int(campos[1]), int(campos[2])
    px = dados[pos + 1:pos + 1 + w * h * 3]
    lum = [(299 * px[i] + 587 * px[i + 1] + 114 * px[i + 2]) // 1000 for i in range(0, w * h * 3, 3)]
    return w, h, lum


def diferenca(com, sem):
    wa, ha, a = ler(com)
    wb, hb, b = ler(sem)
    if (wa, ha) != (wb, hb):
        sys.exit("tamanhos diferentes: %s %dx%d, %s %dx%d" % (com, wa, ha, sem, wb, hb))
    return wa, ha, [x - y for x, y in zip(a, b)]


def blocos(w, h, d, p):
    gw, gh = w // p, h // p
    g = [0.0] * (gw * gh)
    for y in range(gh * p):
        linha = y * w
        base = (y // p) * gw
        for x in range(gw * p):
            g[base + x // p] += d[linha + x]
    return gw, gh, [v / (p * p) for v in g]


def borra(gw, gh, g):
    """uma passada de [1 2 1] / 4 nas duas direções, a borda repetida"""
    t = [0.0] * len(g)
    for y in range(gh):
        for x in range(gw):
            e, d = max(x - 1, 0), min(x + 1, gw - 1)
            t[y * gw + x] = (g[y * gw + e] + 2 * g[y * gw + x] + g[y * gw + d]) / 4
    s = [0.0] * len(g)
    for y in range(gh):
        c, b = max(y - 1, 0), min(y + 1, gh - 1)
        for x in range(gw):
            s[y * gw + x] = (t[c * gw + x] + 2 * t[y * gw + x] + t[b * gw + x]) / 4
    return s


def encaixe(gw, gh, a, b, dx, dy):
    """A em (x, y) contra B em (x - dx, y - dy), em blocos; só onde os dois existem"""
    saa = sbb = sab = sa = sb = 0.0
    n = 0
    for y in range(gh):
        yb = y - dy
        if yb < 0 or yb >= gh:
            continue
        for x in range(gw):
            xb = x - dx
            if xb < 0 or xb >= gw:
                continue
            va, vb = a[y * gw + x], b[yb * gw + xb]
            saa += va * va
            sbb += vb * vb
            sab += va * vb
            sa += va
            sb += vb
            n += 1
    if n < gw * gh // 2 or sbb == 0:
        return None
    g = sab / sbb
    erro = (saa - 2 * g * sab + g * g * sbb) / n
    va = saa / n - (sa / n) ** 2
    vb = sbb / n - (sb / n) ** 2
    cor = (sab / n - sa * sb / n / n) / (va * vb) ** 0.5 if va > 0 and vb > 0 else 0.0
    return erro, g, cor, n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a_com")
    ap.add_argument("a_sem")
    ap.add_argument("b_com")
    ap.add_argument("b_sem")
    ap.add_argument("--fator", type=float, default=1.0)
    ap.add_argument("--desloca", default="0,0")
    ap.add_argument("--borrao", type=int, default=4)
    ap.add_argument("--passo", type=int, default=4)
    ap.add_argument("--alcance", type=int, default=64, help="o maior deslocamento procurado, em pixels")
    ap.add_argument("--testa", action="append", default=[], help="DX,DY: o encaixe nesse deslocamento (com o melhor borrão)")
    o = ap.parse_args()
    w, h, da = diferenca(o.a_com, o.a_sem)
    wb, hb, db = diferenca(o.b_com, o.b_sem)
    if (w, h) != (wb, hb):
        sys.exit("A %dx%d e B %dx%d" % (w, h, wb, hb))
    sx, sy = (int(v) for v in o.desloca.split(","))
    if sx or sy:  # o A deslocado de propósito: A'(x, y) = A(x - sx, y - sy), zero fora
        da = [da[(y - sy) * w + (x - sx)] if 0 <= x - sx < w and 0 <= y - sy < h else 0
              for y in range(h) for x in range(w)]
    da = [v / o.fator for v in da]
    p = o.passo
    gw, gh, a = blocos(w, h, da, p)
    _, _, b = blocos(w, h, db, p)
    print("imagens %dx%d, blocos de %d (%dx%d); A dividido por %.2f%s" %
          (w, h, p, gw, gh, o.fator, ", A deslocado de propósito em (%d, %d)" % (sx, sy) if sx or sy else ""))
    print("soma média por pixel: A %.2f, B %.2f, A / B %.3f" %
          (sum(a) / len(a), sum(b) / len(b), (sum(a) / sum(b)) if sum(b) else 0.0))
    r = encaixe(gw, gh, a, b, 0, 0)
    print("sem mexer: erro %.2f ganho %.3f correlação %.3f" % (r[0], r[1], r[2]))
    alc = o.alcance // p
    melhor = None
    bb = b
    for n in range(o.borrao + 1):
        if n:
            bb = borra(gw, gh, bb)
        if n == 0 or n == o.borrao:
            r0 = encaixe(gw, gh, a, bb, 0, 0)
            print("borrão %d, sem deslocar: erro %.2f ganho %.3f correlação %.3f" % (n, r0[0], r0[1], r0[2]))
        for dy in range(-alc, alc + 1):
            for dx in range(-alc, alc + 1):
                r = encaixe(gw, gh, a, bb, dx, dy)
                if r is not None and (melhor is None or r[0] < melhor[0][0]):
                    melhor = (r, n, dx * p, dy * p)
    (e, g, c, nb), n, dx, dy = melhor
    for t in o.testa:
        tx, ty = (int(v) for v in t.split(","))
        bb = b
        for k in range(n):
            bb = borra(gw, gh, bb)
        r = encaixe(gw, gh, a, bb, tx // p, ty // p)
        print("em (%d, %d), borrão %d: erro %.2f ganho %.3f correlação %.3f" % (tx, ty, n, r[0], r[1], r[2]))
    print("melhor: deslocamento (%d, %d) pixels, borrão %d, erro %.2f ganho %.3f correlação %.3f (%d blocos)" %
          (dx, dy, n, e, g, c, nb))


if __name__ == "__main__":
    main()
