#!/usr/bin/env python3
"""F5, etapa 3: as janelas da medida de tempo por passo da luta (port/x360/headless/passos.c) num ou mais logs: a
saída da x86 ou da PPC sem janela, ou o bt3xbx.log do 360. O que ler no console está em docs/x360/plano/F5/03.md.

    passos.py LOG... [--carimbo C]                  a estrutura: cada log diz de que build saiu ("passos: origem C",
                                                    igual a C; mesma_origem), e cada janela tem as três linhas (tempo, apresentação,
                                                    descartes) com números, mais a da tela quando houver (só o 360 a
                                                    escreve); os números sem janela são do emulador
    passos.py LOG... --console [--janelas N] [--minimo P] [--carimbo C]
                                                    o aceite do console, em cada janela de pelo menos P passos (2.700),
                                                    e pelo menos N delas (3: uma rodada fria e duas aquecidas). Desde a
                                                    F5, etapa 8 (plano/F5/08-gravacao.md, seção 18), pela tela:
                                                      - parede dentro de 1% do esperado (2 vblanks de 16,6833 ms por passo);
                                                      - a tela (a linha "tela", contada no vblank de hardware): no máximo
                                                        1% das imagens 3 vblanks ou mais na tela, nenhuma 4 ou mais;
                                                      - pelo menos uma imagem nova por passo (os intervalos entre imagens
                                                        novas, n - 1);
                                                      - zero descartes de VU1, nativos, direto no quadro e texturas
                                                        recusadas (as primitivas só do PS2 se listam: o escopo não exige
                                                        zero nesse contador bruto)
                                                    O trabalho (o intervalo menos a espera de vblank) e os intervalos
                                                    entre imagens medidos na CPU depois do Present se listam, sem
                                                    aceite: o primeiro só vale com a espera acumulada do 360 (antes era o
                                                    intervalo inteiro), o segundo não diz quando a tela trocou.
Sai com 0 só com tudo certo; cada falha diz o log, a janela e o número.
"""
import argparse, pathlib, re, sys

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
TELA = re.compile(r"passos: janela (\d+) tela: (\d+) imagens na tela, por vblanks na tela 1 (\d+), 2 (\d+), 3 (\d+), 4 ou "
                  r"mais (\d+) \(3 ou mais ([\d.]+)%\); trocas perdidas (\d+), imagens puladas (\d+), vblanks (\d+); "
                  r"esperas de vblank (\d+) \((\d+) pelo vblank de hardware\), atraso ao acordar por passo p50 ([\d.]+) "
                  r"p99 ([\d.]+) máximo ([\d.]+) ms, esperas com 1 ms ou mais (\d+)")
SEM_TELA = re.compile(r"passos: janela (\d+) tela: sem a medida")
TELA_MAX_3 = 1.0  # % das imagens com 3 vblanks ou mais na tela (o "99% até um período mais 1 ms" de antes, pela tela)


def mesma_origem(origem, esperado):
    """A origem de um log ("passos: origem") é a esperada ("COMMIT diff:HASH") pelo commit e pelo hash da diferença.
    A build que veio pelo wsl.sh enviar carimba "COMMIT diff:HASH (N alterados, M apagados)": o sufixo só vale com N e
    M zero (a conferência final da etapa 5 da F5 recusou os próprios logs por esse sufixo; plano/F5/05.md)."""
    m = re.fullmatch(r"(\S+ diff:[0-9a-f]+)(?: \((\d+) alterados, (\d+) apagados\))?", origem.strip())
    return bool(m) and m.group(1) == esperado.strip() and (m.group(2) is None or m.group(2) == m.group(3) == "0")


def ler(caminho):
    """(origem, {janela: dados}) de um log"""
    origem, jan = None, {}
    for l in pathlib.Path(caminho).read_text(errors="replace").splitlines():
        m = re.search(r"passos: origem (.*) \(tempo por passo", l)
        if m and origem is None:
            origem = m.group(1).strip()
        for rx, chave in ((TEMPO, "tempo"), (APRES, "apres"), (SEM_IMAGEM, "apres"), (DESC, "desc"), (SEM_DESC, "desc"),
                          (TELA, "tela"), (SEM_TELA, "tela")):
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
        elif a.carimbo is not None and not mesma_origem(origem, a.carimbo):
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
            tela = d.get("tela")
            if tela:
                img, v1, v2, v3, v4, pct3 = int(tela[0]), int(tela[1]), int(tela[2]), int(tela[3]), int(tela[4]), \
                    float(tela[5])
                perd, pul, vbs, esp, hw = int(tela[6]), int(tela[7]), int(tela[8]), int(tela[9]), int(tela[10])
                at50, at99, atmax, at1 = float(tela[11]), float(tela[12]), float(tela[13]), int(tela[14])
                linha += (f"; tela: {img} imagens, por vblanks 1/2/3/4+ {v1}/{v2}/{v3}/{v4} (3 ou mais {pct3:.2f}%), "
                          f"perdidas {perd}, puladas {pul}, {vbs} vblanks; {esp} esperas ({hw} pelo hardware), atraso ao "
                          f"acordar p50 {at50:.2f} p99 {at99:.2f} máximo {atmax:.2f} ms, {at1} com 1 ms ou mais")
            elif "tela" in d:
                linha += "; tela: sem a medida"
            print(linha)
            if not a.console or n < a.minimo:
                continue
            ruins = []
            if not 0.99 <= razao <= 1.01:
                ruins.append(f"parede {razao:.4f} do esperado (fora de 1%)")
            if not tela:
                ruins.append("sem a medida da tela (a linha \"tela\" do 360, com a callback de vblank)")
            else:
                if img == 0:
                    ruins.append("nenhuma imagem contada na tela")
                elif 100.0 * (v3 + v4) / img > TELA_MAX_3:  # (pelas contagens, não pelo % arredondado da linha)
                    ruins.append(f"{pct3:.2f}% das imagens 3 vblanks ou mais na tela ({v3 + v4} de {img}; aceite: até "
                                 f"{TELA_MAX_3:.0f}%)")
                if v4:
                    ruins.append(f"{v4} imagens 4 vblanks ou mais na tela")
            if not d["apres"]:
                ruins.append("sem apresentação medida")
            elif nint < n - 1:
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


if __name__ == "__main__":  # (o roteiros.py importa mesma_origem daqui)
    sys.exit(main())
