#!/usr/bin/env python3
"""F5, etapa 8, degrau 1 (no WSL): quantos quadros de duas rodadas --gpu têm a lista diferente, e onde.

    divergencias.py BASE OUTRO [--secoes 2,4,5,6 | --normal]

Casa as linhas `gpu_falso: quadro N lista ...` dos dois logs pelo número do quadro (não pela posição: uma rodada que
cai no meio ou pula um quadro não desloca as outras). Diz: quadros de cada lado, quadros só de um lado, quadros com a lista
diferente (e as seções que diferem), o primeiro e o último, um histograma de mil em mil quadros, e as linhas `race:` do OUTRO
(o BT3_GS_RACE: os quadros em que o jogo mexeu no que a lista ainda apontava), e se o OUTRO terminou (a linha
`gpu_falso: N quadros, hash do jogo`) ou caiu. Sai com 0 só se os dois têm os mesmos quadros, todos iguais, e o OUTRO
terminou sem linha `race:` (e, se o log tem a linha `contador de quadros do jogo`, com os dois contadores iguais, e, se tem
a linha `desenhos com textura já solta`, com 0: a fila de liberação do degrau 2); com 1 em qualquer outro caso.

--secoes: só essas seções do hash (contadas de 0: texturas, contagens, desenhos, vértices de CPU, vértices da VU1, índices,
constantes), como o SECOES do vu1_perfil.sh compara.
--normal: compara as linhas `gpu_falso: normal quadro N HASH` (BT3_GPU_NORMAL=1), o hash do quadro em que cada textura do
desenho vale pelo conteúdo decodificado e não pelo número de criação: duas rodadas que decodificam um número diferente de
texturas se comparam pelo que cada desenho amostra de fato (as listas "de sempre" divergem desde a primeira textura a menos).
"""
import collections
import re
import sys

Q = re.compile(r"^gpu_falso: quadro (\d+) lista (\w+) .* secoes (.*)$")
FIM = re.compile(r"^gpu_falso: (\d+) quadros, hash do jogo (\w+)")
QN = re.compile(r"^gpu_falso: normal quadro (\d+) (\w+)$")
RACE = re.compile(r"^race: frame (\d+): (.*)$")
CONTADOR = re.compile(r"^gpu_falso: contador de quadros do jogo (\d+), do desenho (\d+)$")
SOLTAS = re.compile(r"^gpu_falso: (\d+) desenhos com textura já solta$")


def le(caminho, secoes, normal=False):
    quadros, corridas, fim, contador = {}, [], None, None
    global soltas
    soltas = None
    with open(caminho, errors="replace") as f:
        for linha in f:
            linha = linha.rstrip("\n")
            if normal:
                m = QN.match(linha)
                if m:
                    quadros[int(m.group(1))] = (m.group(2),)
                    continue
            m = None if normal else Q.match(linha)
            if m:
                s = m.group(3).split()
                quadros[int(m.group(1))] = tuple(s[i] for i in secoes) if secoes else (m.group(2),) + tuple(s)
                continue
            m = FIM.match(linha)
            if m:
                fim = (int(m.group(1)), m.group(2))
                continue
            m = RACE.match(linha)
            if m:
                corridas.append((int(m.group(1)), m.group(2)))
                continue
            m = CONTADOR.match(linha)
            if m:
                contador = (int(m.group(1)), int(m.group(2)))
                continue
            m = SOLTAS.match(linha)
            if m:
                soltas = int(m.group(1))
    return quadros, corridas, fim, contador


def main():
    args = sys.argv[1:]
    secoes = None
    normal = "--normal" in args
    if normal:
        args.remove("--normal")
    if "--secoes" in args:
        i = args.index("--secoes")
        secoes = [int(x) for x in args[i + 1].split(",")]
        del args[i:i + 2]
    if len(args) != 2:
        sys.exit(__doc__)
    a, _, fim_a, _ = le(args[0], secoes, normal)
    b, corridas, fim_b, contador = le(args[1], secoes, normal)
    soltas_b = soltas
    if normal and (not a or not b):
        sys.exit("ERRO: nenhuma linha 'gpu_falso: normal quadro' (faltou BT3_GPU_NORMAL=1) em " + (args[0] if not a else args[1]))
    comuns = sorted(set(a) & set(b))
    so_a, so_b = sorted(set(a) - set(b)), sorted(set(b) - set(a))
    dif = [n for n in comuns if a[n] != b[n]]
    print(f"base: {len(a)} quadros{'' if fim_a else ' (SEM a linha final: caiu)'}; outro: {len(b)} quadros"
          f"{'' if fim_b else ' (SEM a linha final: caiu)'}")
    print(f"só na base: {len(so_a)}" + (f" (de {so_a[0]} a {so_a[-1]})" if so_a else "") +
          f"; só no outro: {len(so_b)}" + (f" (de {so_b[0]} a {so_b[-1]})" if so_b else ""))
    print(f"quadros comuns: {len(comuns)}; com a lista diferente: {len(dif)}" +
          (f" (primeiro {dif[0]}, último {dif[-1]})" if dif else ""))
    if dif:
        hist = collections.Counter(n // 1000 for n in dif)
        print("por mil quadros: " + ", ".join(f"{k * 1000}:{hist[k]}" for k in sorted(hist)))
        qual = collections.Counter()
        for n in dif:
            for k, (x, y) in enumerate(zip(a[n], b[n])):
                if x != y:
                    qual[k - (0 if secoes or normal else 1)] += 1
        print("seções que diferem (-1 = o hash do quadro; 0 texturas, 1 contagens, 2 desenhos, 3 vértices de CPU, 4 vértices da VU1, "
              "5 índices, 6 constantes): " + ", ".join(f"{k}:{v}" for k, v in sorted(qual.items())))
    if corridas:
        quadros_race = sorted({n for n, _ in corridas})
        print(f"linhas race: {len(corridas)}, em {len(quadros_race)} quadros: {quadros_race[:12]}{' ...' if len(quadros_race) > 12 else ''}")
        for n, t in corridas[:3]:
            print(f"  race: frame {n}: {t[:150]}")
    else:
        print("linhas race: 0")
    if fim_a and fim_b:
        print(f"hash do jogo: base {fim_a[1]}, outro {fim_b[1]} ({'iguais' if fim_a[1] == fim_b[1] else 'DIFERENTES'})")
    # (degrau 1) o contador de quadros do jogo, que avança na entrega, tem de ter chegado ao mesmo número que o do desenho
    contador_ok = contador is None or contador[0] == contador[1]
    if contador is not None:
        print(f"contador de quadros: jogo {contador[0]}, desenho {contador[1]} ({'iguais' if contador_ok else 'DIFERENTES'})")
    # (degrau 2, etapa 3) o falso conta todo desenho que use uma textura já solta (a fila de liberação): tem de ser 0
    if soltas_b is not None:
        print(f"desenhos com textura já solta: {soltas_b}")
    ok = not so_a and not so_b and not dif and fim_b is not None and not corridas and contador_ok and not soltas_b
    print("iguais" if ok else "DIFERENTES")
    sys.exit(0 if ok else 1)


main()
