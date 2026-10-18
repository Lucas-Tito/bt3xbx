#!/usr/bin/env python3
"""F5, etapa 8: correlaciona os picos de tempo de quadro do console com as linhas que o diário gravou antes deles.

Lê um game:\\bt3xbx.log (as linhas "gs_d3d: quadro N: ... quadro inteiro X ms (pior Y)") e, para cada trecho de 60
quadros, imprime a média, o pior quadro, a média sem o pior quadro e quantas linhas o diário gravou no relatório
anterior. O relatório do trecho N sai depois de o quadro N ser medido, então o custo da gravação dele cai no quadro
N+1, que é do trecho seguinte (gs_d3d.cpp, frame_end: `sUltimoFim` é lido antes do `log_quadros`).

No fim diz a média, a média sem o pior quadro de cada trecho, a fração do tempo nos piores quadros e, nos trechos
estáveis (só o relatório engasga), o custo por linha gravada. Os trechos com captura (BT3_SHOT) ficam de fora. Só texto:
nenhum byte do jogo.

    port/x360/tools/picos_log.py LOG [--ate-luta]   (--ate-luta: só o que vem antes da linha "luta começou")
"""
import re
import sys

RE = re.compile(r"gs_d3d: quadro (\d+): (\d+) quadros sem captura: quadro inteiro ([\d.]+) ms \(pior ([\d.]+)\).*?"
                r"; (\d+) acima de 16,7 e (\d+) acima de 33,4 ms; (\d+) capturas")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 1:
        sys.exit(__doc__)
    linhas = open(args[0], encoding="utf-8", errors="replace").read().split("\n")
    fim = len(linhas)
    if "--ate-luta" in sys.argv:
        for i, l in enumerate(linhas):
            if "luta começou" in l:
                fim = i
                break
    rel = [(i, RE.search(l)) for i, l in enumerate(linhas[:fim])]
    rel = [(i, m) for i, m in rel if m]
    if len(rel) < 3:
        sys.exit("poucos relatórios de quadro no log")
    print("quadro  quadros  média   pior  sem o pior  acima de 33,4  linhas do relatório anterior")
    pontos = []
    tot = exc = nq = 0.0
    for k, (i, m) in enumerate(rel):
        fr, q, a33, cap = int(m[1]), int(m[2]), int(m[6]), int(m[7])
        med, pior = float(m[3]), float(m[4])
        # o relatório anterior ocupa as linhas do "quadro inteiro" dele até o "quadro inteiro" deste
        n_ant = (i - rel[k - 1][0]) if k > 0 else 0
        sem = (med * q - pior) / (q - 1) if q > 1 else 0.0
        print("%6d  %7d  %5.2f  %6.2f  %10.2f  %13d  %d" % (fr, q, med, pior, sem, a33, n_ant))
        if k > 0 and cap == 0 and q == 60:
            pontos.append((n_ant, pior, sem))
            tot += med * q
            exc += pior - sem
            nq += q
    # os trechos estáveis: sem nenhum outro engasgo (carga, troca de tela), só o relatório: a média sem o pior fica no
    # tempo de um quadro (16,7 ms) e há um quadro acima de 33,4 ms. Neles o pior quadro é a gravação do relatório anterior.
    est = [(l, p, m) for (l, p, m) in pontos if m <= 17.0]
    print("\ntrechos de 60 quadros (sem captura): %d; média %.2f ms, média sem o pior de cada trecho %.2f ms, %.1f%% do "
          "tempo nos piores quadros" % (len(pontos), tot / nq, (tot - exc) / (nq - len(pontos)), 100.0 * exc / tot))
    if est:
        custo = [(p - 16.7) / l for (l, p, m) in est]
        print("trechos estáveis (média sem o pior até 17 ms): %d; pior quadro de %.0f a %.0f ms (média %.0f) com %s linhas "
              "no relatório anterior: %.1f a %.1f ms por linha (média %.1f)"
              % (len(est), min(p for (l, p, m) in est), max(p for (l, p, m) in est), sum(p for (l, p, m) in est) / len(est),
                 "/".join(sorted(set(str(l) for (l, p, m) in est))), min(custo), max(custo), sum(custo) / len(custo)))


if __name__ == "__main__":
    main()
