#!/usr/bin/env python3
"""F5, etapa 8, gravação (docs/x360/plano/F5/08-gravacao.md, seção 18): o que o transfer_data envia, por formato.

Lê o arquivo do BT3_ENVIO_CONTA (uma linha por chamada do transfer_data, gs_core.c: quadro, psm, dbp, dbw, tx, ty, tw, th,
a posição no começo, os bytes, ns, os pixels que foram pelo envio por colunas) e, se vier, o log da rodada (as linhas
"gpu_falso: quadro N" e "roteiro: seq 3" / "roteiro: seq 4", que dão os quadros do combate). Diz, no roteiro inteiro e no
combate: as chamadas, os envios (as chamadas que começam um retângulo), os bytes, o tempo e os ns por byte de cada formato;
quanto dos bytes de 8 e de 4 bits cabe no envio por colunas (o retângulo alinhado às colunas: tx e tw múltiplos de 16 nos
8 bits, de 32 nos 4 bits; e as faixas de 4 linhas inteiras dentro de um pedaço); e a média por quadro do combate e nos
quadros mais pesados dele.

  envio_conta.py CONTA [LOG] [--pesados 10]
"""
import argparse
import collections
import re

NOMES = {0x00: "PSMCT32", 0x01: "PSMCT24", 0x02: "PSMCT16", 0x0A: "PSMCT16S", 0x13: "PSMT8", 0x14: "PSMT4",
         0x1B: "PSMT8H", 0x24: "PSMT4HL", 0x2C: "PSMT4HH"}
BITS = {0x00: 32, 0x01: 24, 0x02: 16, 0x0A: 16, 0x13: 8, 0x14: 4, 0x1B: 8, 0x24: 4, 0x2C: 4}


def combate(log):
    """(primeiro, último) quadro do GS do combate: de 'roteiro: seq 3' até antes de 'roteiro: seq 4'"""
    q, ini, fim = 0, None, None
    rq = re.compile(r"^gpu_falso: quadro (\d+) ")
    with open(log, encoding="utf-8", errors="replace") as f:
        for linha in f:
            m = rq.match(linha)
            if m:
                q = int(m.group(1))
            elif linha.startswith("roteiro: seq 3") and ini is None:
                ini = q
            elif linha.startswith("roteiro: seq 4") and ini is not None and fim is None:
                fim = q - 1
    return ini, fim


def faixas_possiveis(psm, tx, ty, tw, th, tpos, n):
    """os pixels deste pedaço que caem em faixas de 4 linhas inteiras do envio por colunas (a mesma regra do transfer_data)"""
    larg = 16 if psm == 0x13 else 32
    if psm not in (0x13, 0x14) or tx % larg or tw % larg:
        return 0
    total, fim, k, i = tw * th, min(tpos + n, tw * th), 0, tpos
    while i < fim:
        col, y = i % tw, ty + i // tw
        if col == 0 and y % 4 == 0 and fim - i >= 4 * tw and total - i >= 4 * tw and (psm == 0x13 or (i - tpos) % 2 == 0):
            k += 4 * tw
            i += 4 * tw
        else:
            i += tw - col
    return k


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("conta")
    ap.add_argument("log", nargs="?")
    ap.add_argument("--pesados", type=float, default=10.0, help="os quadros do combate entre os N%% de mais bytes")
    a = ap.parse_args()
    ini, fim = combate(a.log) if a.log else (None, None)

    def novo():
        return collections.defaultdict(lambda: collections.Counter())

    tudo, luta = novo(), novo()
    quadro = collections.defaultdict(lambda: collections.Counter())
    with open(a.conta) as f:
        for linha in f:
            c = linha.split()
            if len(c) < 12:
                continue
            q, psm = int(c[0]), int(c[1], 16)
            tx, ty, tw, th, tpos, by, ns, fx = (int(v) for v in c[4:12])
            px = by * 8 // BITS.get(psm, 32)
            pode = faixas_possiveis(psm, tx, ty, tw, th, tpos, px)
            alvos = [tudo] + ([luta] if ini is not None and ini <= q <= fim else [])
            for t in alvos:
                e = t[psm]
                e["chamadas"] += 1
                e["envios"] += tpos == 0
                e["bytes"] += by
                e["ns"] += ns
                e["faixas_px"] += fx
                e["pode_px"] += pode
                e["px"] += px
            if len(alvos) == 2:
                quadro[q]["bytes"] += by
                quadro[q]["chamadas"] += 1
                quadro[q][psm] += by
                quadro[q]["ns"] += ns
                quadro[q]["ns%x" % psm] += ns

    def tabela(nome, t, nq=None):
        tb = sum(e["bytes"] for e in t.values()) or 1
        tn = sum(e["ns"] for e in t.values()) or 1
        print(f"== {nome}" + (f" ({nq} quadros)" if nq else ""))
        print("   formato    chamadas  envios      bytes  %bytes  ns/byte  %tempo  cabe nas colunas  foi pelas colunas")
        for psm in sorted(t, key=lambda p: -t[p]["bytes"]):
            e = t[psm]
            pxb = max(e["px"], 1)
            print(f"   {NOMES.get(psm, hex(psm)):9s} {e['chamadas']:9d} {e['envios']:7d} {e['bytes']:10d} {100 * e['bytes'] / tb:6.1f}%"
                  f" {e['ns'] / max(e['bytes'], 1):8.3f} {100 * e['ns'] / tn:6.1f}%  {100 * e['pode_px'] / pxb:15.1f}%  {100 * e['faixas_px'] / pxb:16.1f}%")
        print(f"   total     {sum(e['chamadas'] for e in t.values()):9d} {sum(e['envios'] for e in t.values()):7d} {tb:10d}"
              f"          {tn / tb:8.3f}")
        if nq:
            print(f"   por quadro: {sum(e['chamadas'] for e in t.values()) / nq:.1f} chamadas, {tb / nq / 1024:.1f} KB, "
                  f"{tn / nq / 1e6:.3f} ms no host")

    tabela("o roteiro inteiro", tudo)
    if ini is not None:
        nq = fim - ini + 1
        tabela(f"o combate, quadros {ini} a {fim}", luta, nq)
        qs = sorted(range(ini, fim + 1), key=lambda q: -quadro[q]["bytes"])
        k = max(1, int(len(qs) * a.pesados / 100))
        pes = qs[:k]
        tot = collections.Counter()
        for q in pes:
            tot.update(quadro[q])
        print(f"== os {k} quadros do combate com mais bytes (os {a.pesados:g}%: de {quadro[pes[-1]]['bytes'] / 1024:.0f} a "
              f"{quadro[pes[0]]['bytes'] / 1024:.0f} KB)")
        print(f"   por quadro: {tot['chamadas'] / k:.1f} chamadas, {tot['bytes'] / k / 1024:.1f} KB, {tot['ns'] / k / 1e6:.3f} ms no host; "
              "por formato: " + ", ".join(f"{NOMES.get(p, hex(p))} {tot[p] / k / 1024:.1f} KB ({tot['ns%x' % p] / k / 1e6:.3f} ms)"
                                          for p in sorted(NOMES, key=lambda p: -tot[p]) if tot[p]))


if __name__ == "__main__":
    main()
