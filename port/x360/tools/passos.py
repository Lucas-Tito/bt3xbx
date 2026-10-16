#!/usr/bin/env python3
"""F5, etapa 3: as janelas da medida de tempo por passo da luta (port/x360/headless/passos.c) num ou mais logs: a
saída da x86 ou da PPC sem janela, ou o bt3xbx.log do 360. O que ler no console está em docs/x360/plano/F5/03.md.

    passos.py LOG... [--carimbo C]                  a estrutura: cada log diz de que build saiu ("passos: origem C",
                                                    igual a C), e cada janela tem as três linhas (tempo, apresentação,
                                                    descartes) com números; os números sem janela são do emulador
    passos.py LOG... --console [--janelas N] [--minimo P] [--carimbo C]
                                                    o aceite proposto no escopo da F5 (seção 3, "Tempo, memória e uso
                                                    do console"), em cada janela de pelo menos P passos (2.700), e pelo
                                                    menos N delas (3: uma rodada fria e duas aquecidas):
                                                      - parede dentro de 1% do esperado (2 vblanks de 16,6833 ms por passo);
                                                      - trabalho (sem a espera de vblank) com p99 até 30 ms e nenhum
                                                        passo acima de um período (33,37 ms);
                                                      - pelo menos 99% dos intervalos entre imagens novas até um período
                                                        mais 1 ms, nenhum acima de três vblanks (50,05 ms);
                                                      - zero descartes de VU1, nativos, direto no quadro e texturas
                                                        recusadas (as primitivas só do PS2 se listam: o escopo não exige
                                                        zero nesse contador bruto)
Sai com 0 só com tudo certo; cada falha diz o log, a janela e o número.
"""
import argparse, pathlib, re, sys

PERIODO = 2 * 16.68335
TEMPO = re.compile(r"passos: janela (\d+) \(vblank (\d+) a (\d+), fim (\S+)\): (\d+) passos, parede ([\d.]+) ms, "
                   r"esperado ([\d.]+) ms \(razão ([\d.]+)\); trabalho p50 ([\d.]+) p95 ([\d.]+) p99 ([\d.]+) máximo "
                   r"([\d.]+) ms; acima de 30 ms (\d+), acima de [\d.]+ ms (\d+); espera de vblank ([\d.]+) ms; Present "
                   r"([\d.]+) ms, pior ([\d.]+) ms")
APRES = re.compile(r"passos: janela (\d+) apresentação: (\d+) intervalos entre imagens novas, (\d+) repetidas; p50 "
                   r"([\d.]+) p99 ([\d.]+) máximo ([\d.]+) ms; acima de [\d.]+ ms (\d+) \(([\d.]+)%\), acima de [\d.]+ ms "
                   r"(\d+)")
SEM_IMAGEM = re.compile(r"passos: janela (\d+) apresentação: nenhuma imagem")
DESC = re.compile(r"passos: janela (\d+) descartes: VU1 (\d+), nativos (\d+) (\d+) (\d+) (\d+) \(contorno, chave, "
                  r"tabela, ids\), direto no quadro (\d+), texturas recusadas (\d+), primitivas só do PS2 (\d+)")
SEM_DESC = re.compile(r"passos: janela (\d+) descartes: sem backend")


def ler(caminho):
    """(origem, {janela: dados}) de um log"""
    origem, jan = None, {}
    for l in pathlib.Path(caminho).read_text(errors="replace").splitlines():
        m = re.search(r"passos: origem (.*) \(tempo por passo", l)
        if m and origem is None:
            origem = m.group(1).strip()
        for rx, chave in ((TEMPO, "tempo"), (APRES, "apres"), (SEM_IMAGEM, "apres"), (DESC, "desc"), (SEM_DESC, "desc")):
            m = rx.search(l)
            if m:
                jan.setdefault(int(m.group(1)), {})[chave] = m.groups()[1:] if m.groups()[1:] else ()
    return origem, jan


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--carimbo")
    ap.add_argument("--console", action="store_true")
    ap.add_argument("--janelas", type=int, default=3)
    ap.add_argument("--minimo", type=int, default=2700)
    a = ap.parse_args()
    erros, boas = [], 0
    for log in a.logs:
        if not pathlib.Path(log).exists():
            erros.append(f"{log}: não existe")
            continue
        origem, jan = ler(log)
        if origem is None:
            erros.append(f"{log}: sem a linha \"passos: origem\" (a build não tem a medida, ou o log está cortado)")
        elif a.carimbo is not None and origem != a.carimbo.strip():
            erros.append(f"{log}: log de outra build (origem \"{origem}\", esperado \"{a.carimbo.strip()}\")")
        if not jan:
            erros.append(f"{log}: nenhuma janela medida")
        for k in sorted(jan):
            d = jan[k]
            falta = [c for c in ("tempo", "apres", "desc") if c not in d]
            if falta:
                erros.append(f"{log}: janela {k} sem as linhas de {', '.join(falta)}")
                continue
            t = d["tempo"]
            ini, fim, motivo, n = int(t[0]), int(t[1]), t[2], int(t[3])
            parede, razao, p50, p95, p99, maximo = float(t[4]), float(t[6]), float(t[7]), float(t[8]), float(t[9]), \
                float(t[10])
            a30, a33, espera, present = int(t[11]), int(t[12]), float(t[13]), float(t[14])
            linha = (f"{pathlib.Path(log).name} janela {k}: {n} passos (vblank {ini} a {fim}, fim {motivo}), parede "
                     f"{parede:.0f} ms (razão {razao:.4f}); trabalho p50 {p50:.2f} p95 {p95:.2f} p99 {p99:.2f} máximo "
                     f"{maximo:.2f} ms, acima de 30 ms {a30}, acima do período {a33}; espera {espera:.0f} ms, Present "
                     f"{present:.0f} ms")
            if d["apres"]:
                ap_ = d["apres"]
                nint, rep, ap99, apmax, a34, pct34, a50 = int(ap_[0]), int(ap_[1]), float(ap_[3]), float(ap_[4]), \
                    int(ap_[5]), float(ap_[6]), int(ap_[7])
                linha += (f"; apresentação: {nint} intervalos, {rep} repetidas, p99 {ap99:.2f} máximo {apmax:.2f} ms, "
                          f"acima de período+1 ms {a34} ({pct34:.2f}%), acima de 3 vblanks {a50}")
            else:
                linha += "; apresentação: sem backend de vídeo"
            if d["desc"]:
                ds = [int(x) for x in d["desc"]]
                linha += (f"; descartes VU1 {ds[0]}, nativos {ds[1]}/{ds[2]}/{ds[3]}/{ds[4]}, direto {ds[5]}, "
                          f"texturas {ds[6]}, só do PS2 {ds[7]}")
            else:
                linha += "; descartes: sem backend D3D"
            print(linha)
            if not a.console or n < a.minimo:
                continue
            ruins = []
            if not 0.99 <= razao <= 1.01:
                ruins.append(f"parede {razao:.4f} do esperado (fora de 1%)")
            if p99 > 30.0:
                ruins.append(f"p99 do trabalho {p99:.2f} ms > 30")
            if a33:
                ruins.append(f"{a33} passos acima de um período ({PERIODO:.2f} ms), o pior {maximo:.2f}")
            if not d["apres"]:
                ruins.append("sem apresentação medida")
            else:
                if pct34 > 1.0:
                    ruins.append(f"{pct34:.2f}% dos intervalos acima de um período mais 1 ms (aceite: até 1%)")
                if a50:
                    ruins.append(f"{a50} intervalos acima de três vblanks")
                if nint < n - 1:
                    ruins.append(f"{nint} intervalos entre imagens novas para {n} passos")
            if not d["desc"]:
                ruins.append("sem descartes medidos (sem backend D3D)")
            elif any(ds[:7]):
                ruins.append(f"descartes: VU1 {ds[0]}, nativos {ds[1:5]}, direto {ds[5]}, texturas {ds[6]}")
            for r in ruins:
                erros.append(f"{log}: janela {k}: {r}")
            boas += not ruins
    if a.console and boas < a.janelas:
        erros.append(f"{boas} janela(s) de pelo menos {a.minimo} passos dentro do aceite, de {a.janelas} exigidas")
    for e in erros:
        print(f"FALHA: {e}")
    if a.console:
        print("aceite do console: " + ("tudo certo" if not erros else f"{len(erros)} falha(s)"))
    else:
        print("estrutura da medida: " + ("ok (sem o 360, os números são do emulador)" if not erros else
                                         f"{len(erros)} falha(s)"))
    return 1 if erros else 0


sys.exit(main())
