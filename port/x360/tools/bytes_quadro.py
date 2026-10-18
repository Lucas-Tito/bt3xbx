#!/usr/bin/env python3
"""F5, etapa 8, degrau 1 (no WSL): o resumo dos bytes que a cadeia de cada quadro leva (BT3_GPU_BYTES=1 numa build --gpu,
vu1_perfil.sh lista). Lê as linhas `gpu_falso: bytes quadro N ...` (gs_core.c, bytes_walk) e as `gpu_falso: quadro N lista ...`
do mesmo log, confere que há uma de cada por quadro (e na mesma ordem) e diz, para os quadros de luta (mais de 20 mil
vértices de VU1), para os de menu (nenhum) e para todos: média, mediana e máximo de cada coluna, em KiB.

    bytes_quadro.py LOG [LOG...]

Sai com 1 se o log não tiver as linhas, se faltar uma por quadro, se a ordem não bater com a das listas ou se uma coluna
tiver valor impossível (a lista tem de caber nos dois buffers de 1 MiB; refunico não passa de ref).
"""
import re
import statistics
import sys

COLS = ["tags", "lista", "externa", "ref", "refunico", "imagem"]
B = re.compile(r"^gpu_falso: bytes quadro (\d+) tags (\d+) lista (\d+) externa (\d+) ref (\d+) refunico (\d+) imagem (\d+) vu (\d+)/(\d+)/(\d+)$")
Q = re.compile(r"^gpu_falso: quadro (\d+) lista ")


def le(caminho):
    bytes_q, quadros = [], []
    with open(caminho, errors="replace") as f:
        for linha in f:
            linha = linha.rstrip("\n")
            m = B.match(linha)
            if m:
                bytes_q.append(tuple(int(x) for x in m.groups()))
                continue
            m = Q.match(linha)
            if m:
                quadros.append(int(m.group(1)))
    return bytes_q, quadros


def confere(caminho, bytes_q, quadros):
    erros = []
    if not bytes_q:
        erros.append("nenhuma linha 'gpu_falso: bytes quadro' (faltou BT3_GPU_BYTES=1, ou a build não é --gpu)")
        return erros
    if len(bytes_q) != len(quadros):
        erros.append(f"{len(bytes_q)} linhas de bytes e {len(quadros)} de lista")
    elif [b[0] for b in bytes_q] != quadros:
        erros.append("os números de quadro das linhas de bytes não são os das listas")
    for b in bytes_q:
        n, tags, lista, externa, ref, refunico, imagem = b[:7]
        if lista > 2 * 1048576 or refunico > ref or tags == 0:
            erros.append(f"quadro {n}: valor impossível {b[1:7]}")
            break
    return erros


def resumo(nome, linhas):
    if not linhas:
        print(f"{nome}: nenhum quadro")
        return
    print(f"{nome}: {len(linhas)} quadros")
    for i, c in enumerate(COLS):
        v = [l[1 + i] for l in linhas]
        d = 1 if c == "tags" else 1024.0
        print(f"  {c:9s} média {statistics.mean(v) / d:10.1f} mediana {statistics.median(v) / d:10.1f} máximo {max(v) / d:10.1f}"
              + ("" if c == "tags" else " KiB"))
    fecho = [l[2] + l[3] + l[5] for l in linhas]  # lista + externa + refunico: o que uma cópia da cadeia na entrega leva
    print(f"  fecho (lista + externa + refunico): média {statistics.mean(fecho) / 1048576:.2f} MiB, "
          f"máximo {max(fecho) / 1048576:.2f} MiB")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    ruim = 0
    for caminho in sys.argv[1:]:
        bytes_q, quadros = le(caminho)
        print(f"== {caminho}")
        erros = confere(caminho, bytes_q, quadros)
        for e in erros:
            print("ERRO:", e)
        if erros:
            ruim = 1
            continue
        resumo("todos", bytes_q)
        resumo("luta (VU1 com mais de 20 mil vértices)", [b for b in bytes_q if b[7] > 20000])
        resumo("menus (sem VU1)", [b for b in bytes_q if b[7] == 0])
    sys.exit(ruim)


main()
