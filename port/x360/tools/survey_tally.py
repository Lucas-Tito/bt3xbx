#!/usr/bin/env python3
"""Soma a saída do survey.sh: quantos arquivos passam e quais erros dominam."""
import collections, pathlib, re, sys

saida = pathlib.Path(sys.argv[1])
kinds, per, limpos = collections.Counter(), [], 0
for f in sorted(saida.glob("*.txt")):
    erros = [l for l in f.read_text(errors="replace").splitlines() if re.search(r"\berror C\d+", l)]
    limpos += not erros
    per.append((len(erros), f.stem))
    for e in erros:
        m = re.search(r"error (C\d+): (.*)", e)
        kinds[(m.group(1), re.sub(r"'[^']*'", "'X'", m.group(2))[:80])] += 1
print(f"{limpos} de {len(per)} arquivos sem erro; {sum(n for n, _ in per)} erros")
for (c, m), n in kinds.most_common(30):
    print(f"{n:6d}  {c} {m}")
print("piores:", ", ".join(f"{s}({n})" for n, s in sorted(per, reverse=True)[:10] if n))
