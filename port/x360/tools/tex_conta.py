#!/usr/bin/env python3
"""F5, etapa 8, texturas (docs/x360/plano/F5/08-gravacao.md, seção 17): o que o texture_get decodifica, quadro a quadro.

Lê o arquivo do BT3_TEX_CONTA (uma linha por textura decodificada, gs_draw.c: quadro, largura, altura, psm, cpsm, TEX0,
TEXA, gen, hash do conteúdo, ns, conferência) e, se vier, o log da rodada (as linhas "roteiro:" e "gpu_falso: quadro", para
dizer em que ponto do roteiro cada rajada caiu). Separa cada decodificação em:
  primeira     o conteúdo (o hash dos pixels com o tamanho) nunca foi decodificado antes;
  descartada   a mesma chave do cache (TEX0, TEXA, gen) já tinha sido decodificada: a entrada saiu do cache e voltou;
  pagina       o mesmo TEX0 e TEXA com o mesmo conteúdo, outro gen: uma página que a textura ocupa mudou sem mudar a textura;
  outro_tex0   o mesmo conteúdo, já decodificado com outro TEX0 ou TEXA (outro endereço ou outra paleta igual).
O custo no console é estimado pelos pixels a NS_PX ns por pixel (110: o medido no registro dos picos, seção 13).

Com o cache pela origem (BT3_TEX_ORIGEM=1, seção 26) o arquivo tem também uma linha "R" por textura reusada sem decodificar
(quadro, largura, altura, psm, cpsm, TEX0, TEXA, gen, hash do conteúdo de agora, conferência: a decodificação de agora contra
os pixels guardados do recurso reusado, 1 iguais, 0 diferentes, 3 sem a cópia): contadas à parte, nunca como decodificação.

  tex_conta.py CONTA [LOG] [--ns-px 110] [--rajada-ms 5] [--quadros A:B] [--quadro N ...]
--quadros: só os quadros do GS de A a B (a luta do f5_duelo em 4:3: 4673:7758); --quadro: o detalhe desses quadros.
"""
import argparse
import collections
import re


def faixa(ms):
    for lim, nome in ((1, "<1"), (5, "1-5"), (10, "5-10"), (20, "10-20")):
        if ms < lim:
            return nome
    return ">=20"


FAIXAS = ["<1", "1-5", "5-10", "10-20", ">=20"]


def marcas(log):
    """[(quadro do GS, texto)] das linhas do roteiro, pelo último 'gpu_falso: quadro N' visto antes de cada uma"""
    out, q = [], 0
    rq = re.compile(r"^gpu_falso: quadro (\d+) ")
    with open(log, encoding="utf-8", errors="replace") as f:
        for linha in f:
            m = rq.match(linha)
            if m:
                q = int(m.group(1))
            elif linha.startswith("roteiro:") and ("marca ponto" in linha or " ok " in linha or "janela" in linha):
                out.append((q, linha.strip()[:110]))
    return out


def onde(mk, q):
    t = "(antes do roteiro)"
    for mq, txt in mk:
        if mq > q:
            break
        t = txt
    return t


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("conta")
    ap.add_argument("log", nargs="?")
    ap.add_argument("--ns-px", type=float, default=110.0)
    ap.add_argument("--rajada-ms", type=float, default=5.0)
    ap.add_argument("--quadros", default=None)
    ap.add_argument("--quadro", type=int, action="append", default=[])
    a = ap.parse_args()
    q0, q1 = (int(x) for x in a.quadros.split(":")) if a.quadros else (0, 1 << 40)
    reuso = collections.Counter()
    reuso_conf = collections.Counter()

    vistos, chaves, por_tex0 = set(), set(), collections.defaultdict(set)
    classes = collections.Counter()
    classes_px = collections.Counter()
    fmt = collections.Counter()
    fmt_px = collections.Counter()
    conf = collections.Counter()
    quadro = collections.defaultdict(lambda: collections.Counter())
    ns_tot = px_tot = n = 0
    with open(a.conta) as f:
        for linha in f:
            c = linha.split()
            if c and c[0] == "R":
                if len(c) >= 11 and q0 <= int(c[1]) <= q1:
                    q, px = int(c[1]), int(c[2]) * int(c[3])
                    reuso["n"] += 1
                    reuso["px"] += px
                    reuso_conf[c[10]] += 1
                    quadro[q]["reuso_n"] += 1
                    quadro[q]["reuso_px"] += px
                continue
            if len(c) < 10:
                continue
            q, w, h, psm, cpsm, t0, texa, gen, ch, ns = c[:10]
            q, px = int(q), int(w) * int(h)
            conteudo = (w, h, ch)
            chave = (t0, texa, gen)
            if chave in chaves:
                k = "descartada"
            elif conteudo not in vistos:
                k = "primeira"
            elif conteudo in por_tex0[(t0, texa)]:
                k = "pagina"
            else:
                k = "outro_tex0"
            vistos.add(conteudo)
            chaves.add(chave)
            por_tex0[(t0, texa)].add(conteudo)
            if not q0 <= q <= q1:  # (a classe vem de todo o arquivo; só a conta é do trecho)
                continue
            classes[k] += 1
            classes_px[k] += px
            fmt[psm] += 1
            fmt_px[psm] += px
            if len(c) > 10:
                conf[c[10]] += 1
            quadro[q]["n"] += 1
            quadro[q]["px"] += px
            quadro[q][k] += px
            ns_tot += int(ns)
            px_tot += px
            n += 1

    print(f"{n} texturas decodificadas, {px_tot / 1e6:.2f} M pixels, em {sum(1 for c in quadro.values() if c['n'])} quadros; "
          f"no host {ns_tot / max(px_tot, 1):.2f} ns por pixel")
    if reuso["n"]:
        print(f"reusadas pela origem, sem decodificar: {reuso['n']} texturas, {reuso['px'] / 1e6:.2f} M pixels; conferência "
              "(1 iguais, 0 diferentes, 3 sem a cópia): " + ", ".join(f"{k}: {v}" for k, v in sorted(reuso_conf.items())))
    print("conferência da decodificação por linha (1 iguais, 0 diferentes, 2 formato que ela não faz): " +
          ", ".join(f"{k}: {v}" for k, v in sorted(conf.items())))
    print("por formato (psm: texturas, M pixels): " +
          ", ".join(f"{p}: {fmt[p]}, {fmt_px[p] / 1e6:.2f}" for p in sorted(fmt, key=lambda p: -fmt_px[p])))
    print("por classe (texturas, M pixels, % dos pixels): " +
          ", ".join(f"{k}: {classes[k]}, {classes_px[k] / 1e6:.2f}, {100 * classes_px[k] / max(px_tot, 1):.1f}%"
                    for k in ("primeira", "descartada", "pagina", "outro_tex0")))

    hist = collections.Counter()
    hist_px = collections.Counter()
    hist_rep = collections.Counter()
    for q, c in quadro.items():
        if not c["n"]:
            continue
        fx = faixa(c["px"] * a.ns_px / 1e6)
        hist[fx] += 1
        hist_px[fx] += c["px"]
        hist_rep[fx] += c["px"] - c["primeira"]
    print(f"quadros com decodificação, pela estimativa no console ({a.ns_px:g} ns por pixel): faixa ms: quadros, M pixels, % repetida")
    for fx in FAIXAS:
        if hist[fx]:
            print(f"  {fx:>5}: {hist[fx]:5d}, {hist_px[fx] / 1e6:6.2f}, {100 * hist_rep[fx] / hist_px[fx]:5.1f}%")

    mk = marcas(a.log) if a.log else []
    raj = sorted((q for q, c in quadro.items() if c["px"] * a.ns_px / 1e6 >= a.rajada_ms))
    print(f"rajadas (>= {a.rajada_ms:g} ms estimados): {len(raj)} quadros")
    trechos = collections.Counter()
    for q in raj:
        trechos[onde(mk, q)] += 1
    for t, k in trechos.most_common(12):
        print(f"  {k:4d}  depois de: {t}")
    print("as 15 maiores: quadro, texturas, mil pixels, ms estimados, % repetida (pagina / descartada / outro_tex0)")
    for q in sorted((q for q in quadro if quadro[q]["n"]), key=lambda q: -quadro[q]["px"])[:15]:
        c = quadro[q]
        print(f"  {q:6d} {c['n']:4d} {c['px'] / 1e3:8.1f} {c['px'] * a.ns_px / 1e6:6.1f}  "
              f"{100 * (c['px'] - c['primeira']) / c['px']:5.1f}% ({c['pagina'] // 1000} / {c['descartada'] // 1000} / "
              f"{c['outro_tex0'] // 1000} mil)  {onde(mk, q)[:70] if mk else ''}")
    for q in a.quadro:
        c = quadro.get(q, collections.Counter())
        print(f"quadro {q}: decodificadas {c['n']} ({c['px'] / 1e3:.1f} mil pixels, {c['px'] * a.ns_px / 1e6:.2f} ms estimados; "
              f"primeira {c['primeira'] // 1000}, pagina {c['pagina'] // 1000}, outro_tex0 {c['outro_tex0'] // 1000}, "
              f"descartada {c['descartada'] // 1000} mil); reusadas {c['reuso_n']} ({c['reuso_px'] / 1e3:.1f} mil pixels)")


if __name__ == "__main__":
    main()
