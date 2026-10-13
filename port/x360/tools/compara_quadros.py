#!/usr/bin/env python3
"""F3, etapa 7: compara as capturas do console (BT3_SHOT do backend D3D9, gpu_NNNNN.ppm) com as imagens do
rasterizador de software da etapa 4 (frame_NNNNN.ppm, mesmo número de quadro do GS). Roda no WSL, onde as imagens
estão; daqui só saem números.

    compara_quadros.py DIR_CONSOLE DIR_REFERENCIA [--lista] [--limiar N]

As duas são 640 x 448 (a largura do quadro do GS, o resto em preto). O GPU e o rasterizador não arredondam igual
(filtro, mistura, meio pixel), então o resultado é uma medida, não "igual / diferente":
  - pixels iguais, pixels com diferença de até 2 e até N (padrão 8) no pior canal, e o pior canal;
  - erro médio por canal e PSNR;
  - por fase (cartão, logos, título, carga da demo: os quadros do GS de formatos/gpu.md seção 5) e os piores quadros.
Uma imagem do console toda preta onde a referência não é conta à parte (o alvo mostrado errado ou nada desenhado).
"""
import math, os, re, sys

FASES = [("cartão", 0, 233), ("logos", 234, 1747), ("título", 1748, 4243), ("carga da demo", 4244, 10 ** 9)]


def ler_ppm(caminho):
    with open(caminho, "rb") as f:
        dados = f.read()
    m = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", dados)
    if not m:
        raise ValueError(f"{caminho}: não é P6")
    w, h = int(m.group(1)), int(m.group(2))
    px = dados[m.end():m.end() + w * h * 3]
    if len(px) != w * h * 3:
        raise ValueError(f"{caminho}: curto")
    return w, h, px


def medir(a, b, limiar):
    """(pixels, iguais, até 2, até limiar, pior canal, soma das diferenças por canal, soma dos quadrados, a preta)"""
    try:
        import numpy as np
        x = np.frombuffer(a, np.uint8).reshape(-1, 3).astype(np.int16)
        y = np.frombuffer(b, np.uint8).reshape(-1, 3).astype(np.int16)
        d = np.abs(x - y)
        m = d.max(axis=1)
        return (len(m), int((m == 0).sum()), int((m <= 2).sum()), int((m <= limiar).sum()), int(m.max()),
                int(d.sum()), int((d.astype(np.int64) ** 2).sum()), not x.any() and y.any())
    except ImportError:  # (o WSL não tem numpy: linha igual conta inteira, sem passar pixel a pixel)
        n = len(a) // 3
        iguais = ate2 = ateN = pior = soma = quad = 0
        passo = 640 * 3
        for o in range(0, len(a), passo):
            la, lb = a[o:o + passo], b[o:o + passo]
            if la == lb:
                k = len(la) // 3
                iguais += k
                ate2 += k
                ateN += k
                continue
            for i in range(0, len(la), 3):
                m = 0
                for c in range(3):
                    d = abs(la[i + c] - lb[i + c])
                    soma += d
                    quad += d * d
                    m = max(m, d)
                iguais += m == 0
                ate2 += m <= 2
                ateN += m <= limiar
                pior = max(pior, m)
        return n, iguais, ate2, ateN, pior, soma, quad, not any(a) and any(b)


def main():
    a = sys.argv[1:]
    if len(a) < 2:
        sys.exit(__doc__)
    console, ref = a[0], a[1]
    lista = "--lista" in a
    limiar = int(a[a.index("--limiar") + 1]) if "--limiar" in a else 8
    quadros = sorted(int(m.group(1)) for m in (re.match(r"gpu_(\d+)\.ppm$", n) for n in os.listdir(console)) if m)
    if not quadros:
        sys.exit(f"nenhum gpu_NNNNN.ppm em {console}")
    por_fase = {f[0]: [0, 0, 0, 0, 0, 0, 0, 0, 0] for f in FASES}  # imagens, pixels, iguais, até 2, até N, soma, quad, pior, pretas
    piores, sem_ref = [], 0
    for q in quadros:
        r = os.path.join(ref, f"frame_{q:05d}.ppm")
        if not os.path.exists(r):
            sem_ref += 1
            continue
        w1, h1, p1 = ler_ppm(os.path.join(console, f"gpu_{q:05d}.ppm"))
        w2, h2, p2 = ler_ppm(r)
        if (w1, h1) != (w2, h2):
            print(f"quadro {q}: tamanhos {w1}x{h1} e {w2}x{h2}")
            continue
        n, ig, a2, aN, pior, soma, quad, preta = medir(p1, p2, limiar)
        fase = next(f[0] for f in FASES if f[1] <= q <= f[2])
        t = por_fase[fase]
        for i, v in enumerate((1, n, ig, a2, aN, soma, quad)):
            t[i] += v
        t[7] = max(t[7], pior)
        t[8] += preta
        mse = quad / (3 * n)
        psnr = 99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse)
        piores.append((aN / n, q, fase, pior, soma / (3 * n), psnr, preta))
        if lista:
            print(f"quadro {q:5d} {fase:14s} iguais {100 * ig / n:6.2f}%  até 2 {100 * a2 / n:6.2f}%  até {limiar} "
                  f"{100 * aN / n:6.2f}%  pior {pior:3d}  médio {soma / (3 * n):6.3f}  PSNR {psnr:5.1f} dB"
                  + ("  CONSOLE PRETO" if preta else ""))
    print(f"{len(quadros)} capturas do console, {len(quadros) - sem_ref} com referência, {sem_ref} sem")
    for nome, _, _ in FASES:
        im, n, ig, a2, aN, soma, quad, pior, pretas = por_fase[nome]
        if im == 0:
            continue
        mse = quad / (3 * n)
        psnr = 99.0 if mse == 0 else 10 * math.log10(255 * 255 / mse)
        print(f"{nome:14s} {im:4d} imagens: iguais {100 * ig / n:6.2f}%  até 2 {100 * a2 / n:6.2f}%  até {limiar} "
              f"{100 * aN / n:6.2f}%  pior canal {pior:3d}  erro médio {soma / (3 * n):6.3f}  PSNR {psnr:5.1f} dB"
              f"  pretas no console {pretas}")
    print("piores quadros (fração de pixels até o limiar):")
    for frac, q, fase, pior, medio, psnr, preta in sorted(piores)[:8]:
        print(f"  quadro {q:5d} ({fase}): {100 * frac:6.2f}% até {limiar}, pior {pior}, médio {medio:.3f}, PSNR {psnr:.1f} dB"
              + ("  CONSOLE PRETO" if preta else ""))


if __name__ == "__main__":
    main()
