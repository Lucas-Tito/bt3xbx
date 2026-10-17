#!/usr/bin/env python3
"""Triagem de capturas do console (gpu_NNNNN.ppm) contra a referência do rasterizador de software, sem PIL nem numpy.

O WSL do agente só tem a biblioteca padrão do Python. Subcomandos (PPM P6 de 640 x 448, o formato do `gs_core.c`):

  png   ENTRADA.ppm SAÍDA.png [x0 y0 x1 y1 [zoom]]   converte (e recorta e amplia, sem filtro) para ver a imagem
  patch ENTRADA.ppm x0 y0 x1 y1                      média, desvio, mínimo e máximo da luminância de um retângulo
  stats ENTRADA.ppm [x0 y0 x1 y1]                    a área 3D (linhas 75 a 447, colunas 0 a 511), sem o retângulo
                                                     dado (a caixa da pausa, por exemplo): luminância média, % de
                                                     pixels saturados (R, G e B >= 240), % de escuros (todos <= 40)
                                                     e o gradiente horizontal médio (nitidez: imagem borrada dá menos)

Os números só comparam imagens parecidas (a mesma luta, o mesmo estágio): a referência é de outro quadro, e a
textura do rasterizador de software é lida no ponto, o que infla o gradiente dela. Serve para ordem de grandeza.
"""
import math
import struct
import sys
import zlib


def ler(caminho):
    d = open(caminho, "rb").read()
    partes = d.split(None, 4)
    if partes[0] != b"P6":
        raise SystemExit("%s: não é P6" % caminho)
    w, h = int(partes[1]), int(partes[2])
    if len(d) < w * h * 3 + 11:
        raise SystemExit("%s: arquivo truncado (%d bytes para %dx%d)" % (caminho, len(d), w, h))
    return w, h, d[len(d) - w * h * 3:]


def png(caminho, w, h, px):
    cru = b"".join(b"\x00" + px[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def bloco(tipo, dados):
        corpo = tipo + dados
        return struct.pack(">I", len(dados)) + corpo + struct.pack(">I", zlib.crc32(corpo) & 0xFFFFFFFF)

    with open(caminho, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + bloco(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                bloco(b"IDAT", zlib.compress(cru)) + bloco(b"IEND", b""))


def lum(px, o):
    return (px[o] * 299 + px[o + 1] * 587 + px[o + 2] * 114) // 1000


def cmd_png(a):
    w, h, px = ler(a[0])
    dado = list(map(int, a[2:7]))
    x0, y0, x1, y1, z = dado + [0, 0, w, h, 1][len(dado):]
    linhas = []
    for y in range(y0, y1):
        r = bytearray()
        for x in range(x0, x1):
            o = (y * w + x) * 3
            r += px[o:o + 3] * z
        linhas += [bytes(r)] * z
    png(a[1], (x1 - x0) * z, (y1 - y0) * z, b"".join(linhas))


def cmd_patch(a):
    w, h, px = ler(a[0])
    x0, y0, x1, y1 = map(int, a[1:5])
    v = [lum(px, (y * w + x) * 3) for y in range(y0, y1) for x in range(x0, x1)]
    m = sum(v) / len(v)
    print("média %.1f desvio %.2f mín %d máx %d" % (m, math.sqrt(sum((e - m) ** 2 for e in v) / len(v)), min(v), max(v)))


def cmd_stats(a):
    w, h, px = ler(a[0])
    ex = tuple(map(int, a[1:5])) if len(a) >= 5 else None
    n = sat = esc = soma = 0
    cols = min(w, 512)
    linhas = []
    for y in range(75, h):
        l = []
        for x in range(cols):
            if ex and ex[0] <= x < ex[2] and ex[1] <= y < ex[3]:
                l.append(None)
                continue
            o = (y * w + x) * 3
            r, g, b = px[o], px[o + 1], px[o + 2]
            e = lum(px, o)
            l.append(e)
            n += 1
            soma += e
            sat += min(r, g, b) >= 240
            esc += max(r, g, b) <= 40
        linhas.append(l)
    grad = gn = 0
    for l in linhas:
        for x in range(1, cols):
            if l[x - 1] is not None and l[x] is not None:
                grad += abs(l[x] - l[x - 1])
                gn += 1
    print("luminância %.1f, saturados %.1f%%, escuros %.1f%%, gradiente horizontal %.2f" %
          (soma / n, 100.0 * sat / n, 100.0 * esc / n, grad / gn))


if __name__ == "__main__":
    c = {"png": cmd_png, "patch": cmd_patch, "stats": cmd_stats}
    if len(sys.argv) < 3 or sys.argv[1] not in c:
        raise SystemExit(__doc__)
    c[sys.argv[1]](sys.argv[2:])
