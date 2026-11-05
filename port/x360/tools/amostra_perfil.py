#!/usr/bin/env python3
"""O resultado do amostrador da thread do jogo (port/x360/src/f2/amostra360.cpp, BT3_AMOSTRA): lê um despejo do
bt3xbx.log, confere que o .map é da mesma build e soma as amostras por função.

    amostra_perfil.py resolver LOG MAPA [--despejo N] [--topo 40] [--json SAIDA]
        O relatório: por classe de quadro (normal, lento, longo), o tempo próprio (o Iar), o inclusivo (a função em
        qualquer ponto da pilha) e os pares folha/chamador; e o que mais cresce do quadro normal para o lento, em ms por
        quadro. Recusa (sai com 1) se as âncoras ou o carimbo do ligador do despejo não batem com o .map.
    amostra_perfil.py sintetico MAPA PASTA
        Um roteiro para o teste no host (teste_amostra/teste.c hist) com contagens conhecidas, e o esperado.json.
    amostra_perfil.py conferir OBTIDO.json ESPERADO.json
        Sai com 1 se alguma soma por função difere.

O teste completo, com as sabotagens que têm de falhar: port/x360/tools/amostra_confere.sh.
"""
import argparse
import bisect
import json
import pathlib
import re
import sys

CLASSES = ("normal", "lento", "longo")
ESPERA = "(sem Iar: numa espera do núcleo)"
NUCLEO = "(Iar no núcleo, fora da imagem)"
ANCORAS = ("__real_main", "Port_Sair", "Diario_Escrever", "Amostra_Comecar")


class Mapa:
    def __init__(self, caminho):
        self.caminho = caminho
        self.carimbo = None
        self.simbolos = {}
        ini = []
        partes = []
        for l in open(caminho, errors="replace"):
            m = re.match(r"\s*Timestamp is ([0-9a-fA-F]+)", l)
            if m:
                self.carimbo = int(m.group(1), 16)
                continue
            m = re.match(r"\s*0003:([0-9a-fA-F]{8}) ([0-9a-fA-F]{8})H\s+\S+\s+CODE", l)
            if m:
                partes.append((int(m.group(1), 16), int(m.group(2), 16)))
                continue
            m = re.match(r"\s*(0003):([0-9a-fA-F]{8})\s+(\S+)\s+([0-9a-fA-F]{8})( f)?\s+(\S+)", l)
            if m:
                end = int(m.group(4), 16)
                ini.append((end, m.group(3), m.group(6)))
                self.base3 = end - int(m.group(2), 16)
                self.simbolos.setdefault(m.group(3), end)
        if not ini or not partes:
            raise SystemExit(f"amostra_perfil: {caminho} não parece um .map do link do 360")
        ini.sort()
        self.inicios = [a for a, _, _ in ini]
        self.nomes = [(n, o) for _, n, o in ini]
        self.texto = (self.base3, self.base3 + max(o + t for o, t in partes))

    def funcao(self, e):
        if e == 0:
            return ESPERA
        if e == 1:
            return NUCLEO
        if not (self.texto[0] <= e < self.texto[1]):
            return f"(fora do código: {e:08X})"
        i = bisect.bisect_right(self.inicios, e) - 1
        if i < 0:
            return f"(antes da primeira função: {e:08X})"
        n, o = self.nomes[i]
        return f"{n} [{o}]"


def ler_despejos(log):
    """{n: despejo} com o cabeçalho, as contas e as tabelas de cada classe."""
    despejos = {}
    for l in open(log, errors="replace"):
        l = l.rstrip("\r\n")
        m = re.match(r"amostra: d(\d+) (.*)$", l)
        if not m:
            continue
        n, resto = int(m.group(1)), m.group(2)
        d = despejos.setdefault(n, {"n": n, "classes": {c: {"folha": {}, "par": {}, "pilha": {}} for c in CLASSES},
                                    "fim": False})
        m = re.match(r"começo \((.*?)\): (.*); limiar (\d+) ms, longo (\d+) ms$", resto)
        if m:
            d.update(motivo=m.group(1), cabecalho=m.group(2), limiar=int(m.group(3)), longo=int(m.group(4)))
            continue
        if resto == "fim":
            d["fim"] = True
            continue
        m = re.match(r"(\w+): (\d+) quadros, ([\d.]+) ms, (\d+) amostras \(armadilha (\d+), varredura (\d+), sem Iar (\d+), "
                     r"nada (\d+)\), (\d+) descartadas, (\d+) fora das tabelas$", resto)
        if m and m.group(1) in CLASSES:
            k = d["classes"][m.group(1)]
            k.update(quadros=int(m.group(2)), ms=float(m.group(3)), amostras=int(m.group(4)),
                     armadilha=int(m.group(5)), varredura=int(m.group(6)), sem_iar=int(m.group(7)), nada=int(m.group(8)),
                     descartadas=int(m.group(9)), fora=int(m.group(10)))
            continue
        m = re.match(r"(\w+) durações \(ms:quadros\)(.*)$", resto)
        if m and m.group(1) in CLASSES:
            d["classes"][m.group(1)]["duracoes"] = {int(a): int(b) for a, b in re.findall(r"(\d+):(\d+)", m.group(2))}
            continue
        m = re.match(r"(\w+) (folha|pilha|par)((?: \S+)+)$", resto)
        if m and m.group(1) in CLASSES:
            t = d["classes"][m.group(1)][m.group(2)]
            for item in m.group(3).split():
                chave, n_ = item.rsplit(":", 1)
                chave = tuple(int(x, 16) for x in chave.split("/")) if m.group(2) == "par" else int(chave, 16)
                t[chave] = t.get(chave, 0) + int(n_)
            continue
        m = re.match(r"(\w+) (folha|pilha|par): (\d+) entradas, (\d+) despejadas$", resto)
        if m and m.group(1) in CLASSES:
            d["classes"][m.group(1)].setdefault("entradas", {})[m.group(2)] = (int(m.group(3)), int(m.group(4)))
    return despejos


def conferir_build(d, mapa):
    """As âncoras do cabeçalho contra o .map, e o carimbo do ligador (o TimeDateStamp lido na memória). Lista de erros."""
    erros = []
    cab = d.get("cabecalho", "")
    ancoras = dict(re.findall(r"(\w+)=([0-9A-Fa-f]{8})", cab))
    for nome in ANCORAS:
        if nome not in ancoras:
            erros.append(f"o despejo não traz a âncora {nome}")
        elif mapa.simbolos.get(nome) != int(ancoras[nome], 16):
            no_mapa = mapa.simbolos.get(nome)
            erros.append(f"{nome}: {ancoras[nome]} no log, {no_mapa:08X} no .map" if no_mapa is not None
                         else f"{nome} não está no .map")
    m = re.search(r"ligador ([0-9A-Fa-f]{8})", cab)
    if m and int(m.group(1), 16) != 0 and int(m.group(1), 16) != mapa.carimbo:
        erros.append(f"o ligador do log é {m.group(1)}, o do .map {mapa.carimbo:08X}")
    return erros


def somar(d, mapa):
    """Por classe: as somas por função (próprio, inclusivo, pares) e as contas."""
    saida = {}
    for c in CLASSES:
        k = d["classes"][c]
        if not k.get("amostras"):
            continue
        proprio, inclusivo, pares = {}, {}, {}
        for e, n in k["folha"].items():
            f = mapa.funcao(e)
            proprio[f] = proprio.get(f, 0) + n
        for e, n in k["pilha"].items():
            f = mapa.funcao(e)
            inclusivo[f] = inclusivo.get(f, 0) + n
        for (a, b), n in k["par"].items():
            chave = mapa.funcao(a) + "  <-  " + (mapa.funcao(b) if b else "(sem chamador)")
            pares[chave] = pares.get(chave, 0) + n
        uteis = k["amostras"] - k["nada"]
        saida[c] = {"proprio": proprio, "inclusivo": inclusivo, "pares": pares, "uteis": uteis,
                    "quadros": k["quadros"], "ms": k["ms"], "amostras": k["amostras"], "nada": k["nada"],
                    "armadilha": k["armadilha"], "varredura": k["varredura"], "sem_iar": k["sem_iar"],
                    "descartadas": k["descartadas"], "fora": k["fora"], "entradas": k.get("entradas", {})}
    return saida


def tabela(titulo, contas, uteis, ms_quadro, topo):
    print(f"  {titulo}")
    print(f"    {'%':>6} {'ms/quadro':>9} {'amostras':>8}  função")
    for f, n in sorted(contas.items(), key=lambda x: (-x[1], x[0]))[:topo]:
        p = n / uteis if uteis else 0
        print(f"    {100 * p:6.1f} {p * ms_quadro:9.2f} {n:8d}  {f}")


def relatorio(d, somas, topo):
    print(f"despejo d{d['n']} ({d.get('motivo', '?')}); limiar {d.get('limiar')} ms, longo {d.get('longo')} ms")
    print(f"  {d.get('cabecalho', '')}")
    if "autoteste passou" not in d.get("cabecalho", ""):
        print("  ATENÇÃO: o autoteste do amostrador não passou: o Iar e a pilha podem estar errados")
    if not d.get("fim"):
        print("  ATENÇÃO: o despejo está incompleto (sem a linha de fim)")
    for c, s in somas.items():
        msq = s["ms"] / s["quadros"] if s["quadros"] else 0
        print(f"\n== {c}: {s['quadros']} quadros, {msq:.1f} ms por quadro; {s['amostras']} amostras ({s['armadilha']} pelo "
              f"quadro de armadilha, {s['varredura']} pela varredura, {s['sem_iar']} sem Iar, {s['nada']} sem nada), "
              f"{s['descartadas']} descartadas, {s['fora']} fora das tabelas")
        for t, (n, m) in sorted(s["entradas"].items()):
            if n > m:
                print(f"  (tabela {t}: só as {m} maiores de {n} entradas vieram no despejo)")
        tabela("tempo próprio (o Iar):", s["proprio"], s["uteis"], msq, topo)
        tabela("inclusivo (a função em qualquer ponto da pilha; recursão conta mais de uma vez):", s["inclusivo"],
               s["uteis"], msq, topo)
        tabela("folha  <-  chamador:", s["pares"], s["uteis"], msq, topo // 2)
    if "normal" in somas and "lento" in somas:
        n, l = somas["normal"], somas["lento"]
        mn = n["ms"] / n["quadros"] if n["quadros"] else 0
        ml = l["ms"] / l["quadros"] if l["quadros"] else 0
        for titulo, chave in (("tempo próprio", "proprio"), ("inclusivo", "inclusivo")):
            dif = {}
            for f in set(n[chave]) | set(l[chave]):
                a = n[chave].get(f, 0) / n["uteis"] * mn if n["uteis"] else 0
                b = l[chave].get(f, 0) / l["uteis"] * ml if l["uteis"] else 0
                dif[f] = (b - a, a, b)
            print(f"\n== o que cresce do quadro normal ({mn:.1f} ms) para o lento ({ml:.1f} ms), {titulo}, em ms por quadro")
            print(f"    {'mais':>7} {'normal':>7} {'lento':>7}  função")
            for f, (x, a, b) in sorted(dif.items(), key=lambda x: (-x[1][0], x[0]))[:topo]:
                print(f"    {x:7.2f} {a:7.2f} {b:7.2f}  {f}")


def resolver(a):
    mapa = Mapa(a.mapa)
    despejos = ler_despejos(a.log)
    if not despejos:
        print(f"amostra_perfil: nenhum despejo do amostrador em {a.log}")
        return 1
    n = a.despejo if a.despejo is not None else max(despejos)
    if n not in despejos:
        print(f"amostra_perfil: não há o despejo d{n} (há {sorted(despejos)})")
        return 1
    d = despejos[n]
    erros = conferir_build(d, mapa)
    if erros:
        print(f"amostra_perfil: o .map {a.mapa} não é da build que gerou o log; recusado:")
        for e in erros:
            print(f"  {e}")
        return 1
    somas = somar(d, mapa)
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps(
            {c: {k: s[k] for k in ("proprio", "inclusivo", "pares", "quadros", "amostras", "nada")}
             for c, s in somas.items()}, ensure_ascii=False, indent=1, sort_keys=True))
    relatorio(d, somas, a.topo)
    return 0


def sintetico(a):
    """Um roteiro com contagens conhecidas: endereços dentro de funções reais do .map (cru, como a folha sem .pdata),
    duas classes de quadro, as amostras sem Iar e no núcleo; o esperado somado por função."""
    mapa = Mapa(a.mapa)
    pasta = pathlib.Path(a.pasta)
    pasta.mkdir(parents=True, exist_ok=True)
    # funções do jogo com espaço para 4 endereços dentro
    cand = [i for i in range(len(mapa.inicios) - 1)
            if mapa.inicios[i + 1] - mapa.inicios[i] >= 0x40 and not mapa.nomes[i][0].startswith("_")]
    escolhidas = sorted({cand[(k * 97) % len(cand)] for k in range(12)})
    ends = [mapa.inicios[i] for i in escolhidas]
    linhas = [f"C {a.limiar} 400"]
    cab = ("carimbo sintético; ligador %08X; âncoras " % mapa.carimbo
           + " ".join(f"{n}={mapa.simbolos[n]:08X}" for n in ANCORAS)
           + "; código %08X-%08X; .pdata 0; intervalo 1000 us; autoteste passou (sintético)" % mapa.texto)
    linhas.append("H " + cab)
    esperado = {c: {"proprio": {}, "inclusivo": {}, "pares": {}, "quadros": 0, "amostras": 0, "nada": 0}
                for c in ("normal", "lento")}

    def soma(t, f, n=1):
        t[f] = t.get(f, 0) + n

    semente = 12345

    def aleatorio(m):
        nonlocal semente
        semente = (semente * 1103515245 + 12345) & 0x7FFFFFFF
        return semente % m

    for q in range(60):
        classe = "lento" if 20 <= q < 35 else "normal"
        ms = 80.0 + aleatorio(10) if classe == "lento" else 14.0 + aleatorio(3)
        e = esperado[classe]
        amostras = int(ms)
        for _ in range(amostras):
            r = aleatorio(100)
            # o lento concentra nas primeiras funções; o normal espalha
            if classe == "lento" and r < 60:
                i = aleatorio(3)
            else:
                i = aleatorio(len(ends))
            cham = ends[(i + 1) % len(ends)]
            pilha = [cham] + sorted({ends[(i + 2 + aleatorio(3)) % len(ends)], ends[-1]} - {ends[i], cham})
            if r % 17 == 0:
                tipo, folha, funcs = 3, 0, pilha
            elif r % 23 == 0:
                tipo, folha, funcs = 1, 1, pilha
            elif r % 29 == 0:
                tipo, folha, cham, funcs = 0, 0, 0, []
            else:
                folha = ends[i] + 4 * aleatorio(4)  # cru, dentro da função
                tipo, funcs = 1, [folha] + pilha
            linhas.append("S %u %08X %08X %s" % (tipo, folha, cham, " ".join("%08X" % f for f in funcs)))
            e["amostras"] += 1
            if tipo == 0:
                e["nada"] += 1
                continue
            soma(e["proprio"], mapa.funcao(folha))
            soma(e["pares"], mapa.funcao(folha) + "  <-  " + (mapa.funcao(cham) if cham else "(sem chamador)"))
            for f in funcs:
                soma(e["inclusivo"], mapa.funcao(f))
        linhas.append(f"Q 1 {ms:.1f}")
        e["quadros"] += 1
    linhas.append("D sintético")
    (pasta / "roteiro.txt").write_text("\n".join(linhas) + "\n")
    (pasta / "esperado.json").write_text(json.dumps(esperado, ensure_ascii=False, indent=1, sort_keys=True))
    print(f"sintético: {sum(e['amostras'] for e in esperado.values())} amostras em 60 quadros, {len(ends)} funções")
    return 0


def conferir(a):
    obtido = json.loads(pathlib.Path(a.obtido).read_text())
    esperado = json.loads(pathlib.Path(a.esperado).read_text())
    erros = []
    for c in sorted(set(obtido) | set(esperado)):
        o, e = obtido.get(c, {}), esperado.get(c, {})
        for k in ("proprio", "inclusivo", "pares", "quadros", "amostras", "nada"):
            if o.get(k) != e.get(k):
                if isinstance(e.get(k), dict) and isinstance(o.get(k), dict):
                    for f in sorted(set(o[k]) | set(e[k])):
                        if o[k].get(f) != e[k].get(f):
                            erros.append(f"{c} {k} {f}: obtido {o[k].get(f)}, esperado {e[k].get(f)}")
                else:
                    erros.append(f"{c} {k}: obtido {o.get(k)}, esperado {e.get(k)}")
    if erros:
        print(f"conferir: {len(erros)} diferenças")
        for e in erros[:20]:
            print(f"  {e}")
        return 1
    contas = ", ".join(f"{c}: {obtido[c]['amostras']} amostras" for c in sorted(obtido))
    print(f"conferir: igual ({contas})")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("resolver")
    r.add_argument("log")
    r.add_argument("mapa")
    r.add_argument("--despejo", type=int)
    r.add_argument("--topo", type=int, default=40)
    r.add_argument("--json")
    s = sub.add_parser("sintetico")
    s.add_argument("mapa")
    s.add_argument("pasta")
    s.add_argument("--limiar", type=int, default=40)
    c = sub.add_parser("conferir")
    c.add_argument("obtido")
    c.add_argument("esperado")
    a = ap.parse_args()
    return {"resolver": resolver, "sintetico": sintetico, "conferir": conferir}[a.cmd](a)


if __name__ == "__main__":
    sys.exit(main())
