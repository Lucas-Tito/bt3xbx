#!/usr/bin/env python3
"""A cadeia do brilho (GfxPost_DrawGlow, src/sys/gfxm_a.c) em uma dimensão, lida no ponto (o rasterizador x86) e com o
bilinear do GS (o texel em u - 0,5; o que o caminho de GPU reproduz): centro, largura e soma da resposta a uma faixa de
16 pixels em três posições de cada eixo (F8, pendência 5, plano/F8/05-brilho.md). Sem dados do jogo.

Os passos, com as coordenadas do jogo (u do pixel x, em texels; entre parênteses a faixa do CLAMP):
  900 quadro 512 x 448 -> alvo 3, 256 x 256: u = 2x, v = 1,75y        901 3 -> 4, 128: u = 2x
  902 4 -> 5, 64: u = 2x + 1 (o segundo sprite substitui o primeiro)   903 a 911 nove passadas de 5 e 6: a ida u = x + 1, a
  volta u = x (a décima volta não é lida: o 913 lê o alvo 6)           913 6 -> 4: u = 0,25 + 0,5x
  914 4 -> 3: u = 0,5 + 0,5x                                            915 3 -> quadro: u = 0,5 + 0,5x, v = 0,5 + (256/448)y
"""
import math


def passo(src, n, f, lim, modo):
    out = []
    for x in range(n):
        u = f(x)
        if modo == "ponto":
            out.append(src[min(max(int(math.floor(u)), 0), lim)])
        else:
            t = u - 0.5
            i0 = math.floor(t)
            w = t - i0
            out.append((1 - w) * src[min(max(i0, 0), lim)] + w * src[min(max(i0 + 1, 0), lim)])
    return out


def cadeia(s, eixo, modo):
    a = passo(s, 256, (lambda x: 2 * x) if eixo == "x" else (lambda y: 1.75 * y), 511 if eixo == "x" else 447, modo)
    b = passo(a, 128, lambda x: 2 * x, 255, modo)
    c = passo(b, 64, lambda x: 2 * x + 1, 127, modo)
    for k in range(9):
        c = passo(c, 64, (lambda x: x + 1) if k % 2 == 0 else (lambda x: x), 63, modo)
    e = passo(c, 128, lambda x: 0.25 + 0.5 * x, 63, modo)
    f = passo(e, 256, lambda x: 0.5 + 0.5 * x, 127, modo)
    if eixo == "x":
        return passo(f, 512, lambda x: 0.5 + 0.5 * x, 255, modo)
    return passo(f, 448, lambda y: 0.5 + 256 / 448 * y, 255, modo)


def main():
    for eixo, n in (("x", 512), ("y", 448)):
        for pos in (n // 4, n // 2, 3 * n // 4):
            res = {}
            for modo in ("ponto", "bilinear"):
                s = [0.0] * n
                for k in range(pos - 8, pos + 8):
                    s[k] = 1.0
                o = cadeia(s, eixo, modo)
                tot = sum(o)
                cen = sum(i * v for i, v in enumerate(o)) / tot
                larg = math.sqrt(sum((i - cen) ** 2 * v for i, v in enumerate(o)) / tot)
                res[modo] = (tot, cen, larg)
            print("%s fonte em %d: ponto soma %.1f centro %.1f largura %.1f | bilinear soma %.1f centro %.1f largura %.1f"
                  " | do ponto para o bilinear %+.1f" % ((eixo, pos) + res["ponto"] + res["bilinear"] +
                                                         (res["bilinear"][1] - res["ponto"][1],)))


if __name__ == "__main__":
    main()
