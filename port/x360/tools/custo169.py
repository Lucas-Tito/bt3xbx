#!/usr/bin/env python3
"""F8, 16:9, etapa 1 (plano/F8/16x9.md): o custo do 16:9 por quadro, pela build --gpu (x86gv), 4:3 contra 16:9.

    custo169.py LOG_43 LOG_169 [--portao 5]

Os dois logs são o mesmo roteiro na x86gv com BT3_GPU_LISTA=1 BT3_GPU_BYTES=1 BT3_GPU_PIVOS=1, sem a divisão
(BT3_GS_THREAD desligado: o contador de pivôs é exato só assim), um com BT3_WIDE=0 e outro com BT3_WIDE=1 (custo169.sh).
Confere, e sai com 1 se algo falhar:
  - os dois logs completos (a linha do total do gpu_falso), o mesmo número de quadros, e cada quadro com as três linhas
    (lista, bytes, pivôs);
  - o rastro do roteiro igual linha a linha (telas, mcflow, passos do roteiro com vblank e quadro, o resultado e a matriz
    da luta): o 16:9 não muda a luta nem os passos, e os quadros dos dois logs se correspondem;
  - o 4:3 sem nenhum desenho no bloco do 16:9 (o BT3_WIDE=0 chegou), e o 16:9 com os painéis do HUD estreitados nas
    bordas (esquerda e direita) em pelo menos metade dos quadros da luta (o estreitamento roda na build do 360).
Depois, nos quadros da luta (da tela "luta" até a tela seguinte do rastro) e nos pesados (os de mais desenhos no 4:3, o
décimo de cima), a distribuição (mediana, p95, máximo) de cada medida nos dois e o aumento; e os pivôs do 16:9.
O portão do plano: os desenhos dos quadros pesados crescendo mais que --portao por cento (padrão 5) é dito na última
linha ("PORTÃO: ..."); não muda o código de saída, que diz só se a medida vale.
"""
import re
import sys

LISTA = re.compile(r"^gpu_falso: quadro (\d+) lista \S+ desenhos (\d+) vértices (\d+) vu (\d+)/(\d+)/(\d+) texturas (\d+) secoes")
BYTES = re.compile(r"^gpu_falso: bytes quadro (\d+) tags (\d+) lista (\d+) externa (\d+) ref (\d+) refunico (\d+) imagem (\d+) vu")
PIVOS = re.compile(r"^gpu_falso: pivos quadro (\d+) tela (\d+) largo (\d+) esquerda (\d+) direita (\d+) meio (\d+) posicao (\d+) centro (\d+)$")
TOTAL = re.compile(r"^gpu_falso: (\d+) quadros, hash do jogo")
TELA = re.compile(r"^tela (\S+) vblank (\d+) quadro (\d+)$")
RASTRO = ("roteiro: ", "tela ", "mcflow ")
MEDIDAS = ["desenhos", "vértices", "vu vértices", "vu índices", "vu lotes", "texturas novas",
           "bytes tags", "bytes lista", "bytes externa", "bytes ref", "bytes refunico", "bytes imagem"]
PIVO_NOMES = ["tela", "largo", "esquerda", "direita", "meio", "posicao", "centro"]


def le(caminho):
    q, b, p, rastro, total = {}, {}, {}, [], None
    with open(caminho, encoding="utf-8", errors="replace") as f:
        for linha in f:
            linha = linha.rstrip("\n")
            m = LISTA.match(linha)
            if m:
                v = [int(x) for x in m.groups()]
                q[v[0]] = v[1:]
                continue
            m = BYTES.match(linha)
            if m:
                v = [int(x) for x in m.groups()]
                b[v[0]] = v[1:]
                continue
            m = PIVOS.match(linha)
            if m:
                v = [int(x) for x in m.groups()]
                p[v[0]] = v[1:]
                continue
            m = TOTAL.match(linha)
            if m:
                total = int(m.group(1))
                continue
            if linha.startswith(RASTRO):
                rastro.append(linha)
    return q, b, p, rastro, total


def medidas(q, b):
    """por quadro: as 12 medidas; as texturas novas pela diferença do acumulado do gpu_falso"""
    out, ant = {}, 0
    for n in sorted(q):
        d, v, vv, vi, vu, tex = q[n]
        out[n] = [d, v, vv, vi, vu, tex - ant] + b[n]
        ant = tex
    return out


def pct(xs, p):
    s = sorted(xs)
    return s[min(len(s) - 1, max(0, int(round(p / 100.0 * len(s) + 0.5)) - 1))]


def aumento(a, b):
    return "   n/a" if a == 0 else "%+6.1f%%" % (100.0 * (b - a) / a)


def main():
    args = sys.argv[1:]
    portao = 5.0
    if "--portao" in args:
        i = args.index("--portao")
        portao = float(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    falhas = []
    q43, b43, p43, r43, t43 = le(args[0])
    q169, b169, p169, r169, t169 = le(args[1])
    for nome, q, b, p, t in (("4:3", q43, b43, p43, t43), ("16:9", q169, b169, p169, t169)):
        if t is None:
            falhas.append("%s: log sem a linha do total do gpu_falso (a rodada não terminou)" % nome)
        elif len(q) != t:
            falhas.append("%s: %d quadros com lista, o total diz %d" % (nome, len(q), t))
        if set(q) != set(b) or set(q) != set(p):
            falhas.append("%s: quadros sem as três linhas (lista %d, bytes %d, pivôs %d; BT3_GPU_BYTES e BT3_GPU_PIVOS ligados?)"
                          % (nome, len(q), len(b), len(p)))
    if set(q43) != set(q169):
        falhas.append("quadros diferentes: %d só no 4:3, %d só no 16:9" % (len(set(q43) - set(q169)), len(set(q169) - set(q43))))
    if r43 != r169:
        k = next((i for i in range(min(len(r43), len(r169))) if r43[i] != r169[i]), min(len(r43), len(r169)))
        falhas.append("rastro do roteiro diferente na linha %d de %d/%d: 4:3 [%s] 16:9 [%s]" % (
            k + 1, len(r43), len(r169), r43[k] if k < len(r43) else "fim", r169[k] if k < len(r169) else "fim"))
    luta = None
    telas = [(m.group(1), int(m.group(3))) for m in (TELA.match(x) for x in r43) if m]
    for i, (nome, quadro) in enumerate(telas):
        if nome == "luta":
            fim = telas[i + 1][1] if i + 1 < len(telas) else max(q43) + 1
            luta = range(quadro, fim)
            break
    if luta is None:
        falhas.append("o rastro do 4:3 não tem a tela da luta")
    comuns = sorted(set(q43) & set(q169) & set(b43) & set(b169) & set(p43) & set(p169))
    com43 = [n for n in comuns if any(p43[n])]
    if com43:
        falhas.append("o 4:3 tem desenhos no bloco do 16:9 em %d quadros (o BT3_WIDE=0 não chegou?), o primeiro %d" % (len(com43), com43[0]))
    if luta is not None:
        ql = [n for n in comuns if n in luta]
        hud = [n for n in ql if p169[n][2] > 0 and p169[n][3] > 0]
        if not ql or len(hud) * 2 < len(ql):
            falhas.append("o 16:9 estreitou os dois painéis do HUD em %d de %d quadros da luta (o BT3_WIDE=1 não chegou?)" % (len(hud), len(ql)))
    print("custo169: 4:3 %s, 16:9 %s" % (args[0], args[1]))
    print("custo169: quadros 4:3 %s e 16:9 %s; rastro do roteiro: %d e %d linhas, %s" % (
        t43, t169, len(r43), len(r169), "iguais" if r43 == r169 else "DIFERENTES"))
    for x in r43:
        if x.startswith("roteiro: resultado"):
            print("custo169: " + x)
    if falhas or luta is None:
        for x in falhas:
            print("custo169: FALHA: " + x)
        return 1
    m43, m169 = medidas(q43, b43), medidas(q169, b169)
    ql = [n for n in comuns if n in luta]
    corte = pct([m43[n][0] for n in ql], 90)
    pesados = [n for n in ql if m43[n][0] >= corte]
    print("custo169: luta nos quadros %d a %d (%d quadros); pesados: %d quadros com %d desenhos ou mais no 4:3" % (
        luta.start, luta.stop - 1, len(ql), len(pesados), corte))
    print("custo169: %-15s | %-27s | %-27s | aumento: soma   p50     p95     máx  | pesados (soma)" % (
        "medida", "4:3 p50 / p95 / máx", "16:9 p50 / p95 / máx"))
    aum_pesados = None
    for k, nome in enumerate(MEDIDAS):
        a = [m43[n][k] for n in ql]
        b = [m169[n][k] for n in ql]
        pa = sum(m43[n][k] for n in pesados)
        pb = sum(m169[n][k] for n in pesados)
        if k == 0:
            aum_pesados = None if pa == 0 else 100.0 * (pb - pa) / pa
            aum_soma = 100.0 * (sum(b) - sum(a)) / sum(a)
        print("custo169: %-15s | %8d %8d %9d | %8d %8d %9d | %s %s %s %s | %s" % (
            nome, pct(a, 50), pct(a, 95), max(a), pct(b, 50), pct(b, 95), max(b),
            aumento(sum(a), sum(b)), aumento(pct(a, 50), pct(b, 50)), aumento(pct(a, 95), pct(b, 95)),
            aumento(max(a), max(b)), aumento(pa, pb)))
    tot = [sum(p169[n][i] for n in ql) for i in range(7)]
    print("custo169: pivôs no 16:9, na luta (desenhos 2D do buffer da imagem; média por quadro): " + ", ".join(
        "%s %d (%.1f)" % (PIVO_NOMES[i], tot[i], tot[i] / float(len(ql))) for i in range(7)))
    fora = [sum(p169[n][i] for n in comuns if n not in luta) for i in range(7)]
    print("custo169: pivôs no 16:9, fora da luta: " + ", ".join("%s %d" % (PIVO_NOMES[i], fora[i]) for i in range(7)))
    pior = max(aum_soma, aum_pesados if aum_pesados is not None else aum_soma)
    print("custo169: PORTÃO: desenhos %+.1f%% na luta e %+.1f%% nos pesados: %s de %.0f%%" % (
        aum_soma, aum_pesados if aum_pesados is not None else 0.0, "ACIMA" if pior > portao else "dentro", portao))
    return 0


if __name__ == "__main__":
    sys.exit(main())
