#!/usr/bin/env python3
"""F2: resume a saída de cl_jogo.sh: quantos fontes passaram e os erros mais comuns.

    cl_resumo.py SAIDA [--arquivos]
"""
import collections, pathlib, re, sys

saida = pathlib.Path(sys.argv[1])
erros, primeiros, ok, falhos = collections.Counter(), collections.Counter(), 0, []
for t in sorted(saida.glob("*.txt")):
    linhas = [l for l in t.read_text(errors="replace").splitlines() if re.search(r"\berror [CL]\d+", l)]
    if not linhas:
        ok += 1
        continue
    falhos.append((t.stem, len(linhas), linhas[0]))
    for l in linhas:
        erros[re.search(r"error ([CL]\d+): ?(.{0,70})", l).group(1, 2)] += 1
    m = re.search(r"error ([CL]\d+): ?(.{0,70})", linhas[0])
    primeiros[m.group(1, 2)] += 1
print(f"{ok} de {ok + len(falhos)} fontes sem erro")
print("primeiro erro de cada fonte:")
for (c, msg), n in primeiros.most_common(25):
    print(f"  {n:4d}  {c} {msg}")
print("todos os erros:")
for (c, msg), n in erros.most_common(15):
    print(f"  {n:5d}  {c} {msg}")
if "--arquivos" in sys.argv:
    for nome, n, l in falhos:
        print(f"{nome}: {n} erros; {l.strip()[:200]}")
