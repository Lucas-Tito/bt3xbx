#!/usr/bin/env python3
"""F5, lotes com envio: as 14 comparações entre arquiteturas da conferência, com sabotagens.

    lotes_envio_arquiteturas.py ENTRADA SAIDA

A build normal já difere entre x86gv e ppcg no f5_duelo (11 quadros, de 4918 a 4937, desde a etapa das threads,
plano/F5/08-threads.md). Cada variante (normal, ctl, lot, alfa, atraso1/5/20) não precisa de zero diferenças entre
arquiteturas, mas da mesma diferença da normal: `divergencias.py --normal --como` com os dois logs normais do roteiro
como referência, ou seja, os mesmos quadros diferentes e, em cada um, os mesmos hashes dos dois lados. As outras
exigências do `divergencias.py` (quadros dos dois lados, fim, race, contador, texturas soltas, linhas misturadas)
continuam. Três cópias do ppcg do controle no f5_duelo, com um quadro diferente a mais, um hash trocado e um quadro
diferente a menos, têm de ser recusadas cada uma pelo seu motivo, e o original aceito. As cópias ficam em SAIDA e são
apagadas depois de comparadas. Sai com 0 só se tudo isso vale.
"""
import re
import subprocess
import sys
from pathlib import Path

CASOS = ("normal", "ctl", "lot", "alfa", "atraso1", "atraso5", "atraso20")
ROTEIROS = ("f5_duelo", "f5_treino")
DIV = Path(__file__).resolve().with_name("divergencias.py")
Q = re.compile(rb"^gpu_falso: normal quadro (\d+) (\w+)$", re.M)
REF = re.compile(r"^referência: \d+ quadros diferentes; a mais: (\d+).*; a menos: (\d+).*; hash trocado: (\d+)", re.M)


def compara(ent, x, p, roteiro):
    return subprocess.run([sys.executable, str(DIV), str(x), str(p), "--normal", "--como",
                           str(ent / f"normal-x86gv-{roteiro}.log"), str(ent / f"normal-ppcg-{roteiro}.log")],
                          capture_output=True, text=True)


def contagens(saida):
    m = REF.search(saida)
    return tuple(int(v) for v in m.groups()) if m else None


def hashes(f):
    return {int(m.group(1)): m.group(2) for m in Q.finditer(f.read_bytes())}


def outro(h, *evitar):
    """O mesmo hash com o último dígito trocado, diferente de todos os evitados."""
    for c in b"0123456789abcdef":
        novo = h[:-1] + bytes([c])
        if novo != h and novo not in evitar:
            return novo
    raise AssertionError(h)


def sabotagens(ent, sai):
    x, p = ent / "ctl-x86gv-f5_duelo.log", ent / "ctl-ppcg-f5_duelo.log"
    hx, hp = hashes(x), hashes(p)
    comuns = sorted(set(hx) & set(hp))
    dif = [n for n in comuns if hx[n] != hp[n]]
    iguais = [n for n in comuns if hx[n] == hp[n]]
    assert iguais, "nenhum quadro igual entre arquiteturas no controle do duelo"
    original = p.read_bytes()
    r = compara(ent, x, p, "f5_duelo")
    if r.returncode != 0 or contagens(r.stdout) != (0, 0, 0):
        return ["o original do controle no duelo não foi aceito:\n" + r.stdout + r.stderr]
    # (nome, quadro, hash novo no ppcg, contagens esperadas: a mais, a menos, hash trocado)
    casos = [("quadro-a-mais", iguais[len(iguais) // 2], lambda n: outro(hp[n], hx[n]), (1, 0, 0))]
    if dif:
        casos += [("hash-trocado", dif[0], lambda n: outro(hp[n], hx[n]), (0, 0, 1)),
                  ("quadro-a-menos", dif[-1], lambda n: hx[n], (0, 1, 0))]
    else:
        print("aviso: o duelo não tem diferença entre arquiteturas no controle; "
              "sabotagens de hash trocado e quadro a menos não se aplicam")
    falhas = []
    for nome, n, novo, esperado in casos:
        velha = b"\ngpu_falso: normal quadro %d %s\n" % (n, hp[n])
        if original.count(velha) != 1:
            falhas.append(f"sabotagem {nome}: linha do quadro {n} não é única")
            continue
        f = sai / f"sabotagem-arq-{nome}.log"
        f.write_bytes(original.replace(velha, b"\ngpu_falso: normal quadro %d %s\n" % (n, novo(n))))
        r = compara(ent, x, f, "f5_duelo")
        f.unlink()
        (sai / f"sabotagem-arq-{nome}.resultado").write_text(r.stdout + r.stderr)
        obtido = contagens(r.stdout)
        if r.returncode != 1 or obtido != esperado or "Traceback" in r.stderr:
            falhas.append(f"sabotagem {nome} (quadro {n}) não recusada pelo motivo certo: rc {r.returncode}, "
                          f"contagens {obtido}, esperado {esperado}")
        else:
            print(f"sabotagem {nome} (quadro {n}) recusada: a mais/a menos/hash trocado = {obtido}")
    return falhas


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    ent, sai = Path(sys.argv[1]), Path(sys.argv[2])
    sai.mkdir(parents=True, exist_ok=True)
    falhas = []
    for roteiro in ROTEIROS:
        for caso in CASOS:
            r = compara(ent, ent / f"{caso}-x86gv-{roteiro}.log", ent / f"{caso}-ppcg-{roteiro}.log", roteiro)
            (sai / f"arq-{caso}-{roteiro}.log").write_text(r.stdout + r.stderr)
            linha = next((l for l in r.stdout.splitlines() if l.startswith("referência:")), "sem linha de referência")
            print(f"{caso} {roteiro}: {'aceita' if r.returncode == 0 else 'RECUSADA'}; {linha}")
            if r.returncode != 0:
                falhas.append(f"{caso}-{roteiro}")
    falhas += sabotagens(ent, sai)
    if falhas:
        print("FALHA: " + "; ".join(falhas))
        sys.exit(1)
    print("14 comparações entre arquiteturas iguais às da normal (quadros e hashes); sabotagens recusadas")


main()
