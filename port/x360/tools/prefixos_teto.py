#!/usr/bin/env python3
"""F5, seção 36: o teto dos prefixos por partição, com os dados que já existem.

Entradas (nenhuma execução do jogo):
  LISTAS  pasta com Q.jsonl da x86gv (BT3_GPU_COMUM, seção 30): ordem, alvo, pipeline, família de cada desenho;
  DADOS   dados.json do perfil fino do console (seção 31): GPU e CPU do frame_end por classe, trechos do comum por
          faixa de desenhos, das aberturas g6930_N/g6960_N.
Os carimbos sem marcas (J, G, P, U) e as parcelas da gravação (seções 33 e 34) entram como constantes publicadas.

Para cada desenho i: g_i (GPU líquida calibrada), c_i (CPU do frame_end), e a dependência real que o libera:
  fusão      o desenho seguinte já existe (o último só no fim);
  plano      ed_plano (BT3_EDRAM_RESIDENTE, ligada por padrão) lê a lista inteira: tudo no fim, salvo plano previsto;
  z          usa_z na entrada de uma visita: o primeiro desenho com z da visita ou o fim dela;
  DATE       date_area no marcador 5: o próximo marcador ou o fim do quadro;
  VU1 direto vu_plano é por desenho (sVuIni = first), sem consulta ao futuro.
Simula C_i = max(R_i, C_i-1) + c_i e U_i = max(C_i, U_i-1) + g_i, com o fim (Resolve final, tela, FXAA, Present)
depois do último, e calibra uma escala de CPU e outra de GPU para reproduzir P e U medidos sem marcas na partição
atual (tudo liberado em G). Teto é idealizado: fila, cópias, disputa SMT e disputa de memória com a GPU não entram; o
DATE sem consulta paga a cópia da área inteira (DATE_PX). O progresso da gravação até cada desenho não tem medida no
Xenon: U (uniforme por índice), W (as parcelas da gravação do console por tipo de desenho) e, com --curva LOG, X (o relógio
da gravação na x86/PPC do falso, linhas "prefixos2: curva" do BT3_GS_PREFIXOS=2); mais a sensibilidade.

    prefixos_teto.py LISTAS DADOS [--aberturas g6930_1,g6960_1] [--curva LOG]
    prefixos_teto.py --mutantes     (dados sintéticos: o verificador tem de recusar as cópias estragadas)
"""
import argparse, json, re, sys
from pathlib import Path

# Sem marcas, base, relativos a J (seções 33/34): G, P, U e prazo. 6930: J/G/P/U de folga 32,9/18,5/11,7/-2,6.
MEDIDO = {6930: dict(prazo=32.9, G=32.9 - 18.5, P=32.9 - 11.7, U=32.9 + 2.6),
          6960: dict(prazo=32.8, G=32.8 - 13.0, P=32.8 - 6.4, U=32.8 + 9.6)}
# Gravação instrumentada, abertura b (seção 34), ms: as parcelas pesam o progresso no modelo W.
GRAV = {6930: dict(dma=0.189682, vif=1.392778, gif=1.312737, cpu=1.815443, vu1=2.598013, unpack=1.195595,
                   envio=3.462325, entrega=0.069481, hash=2.029666, constantes=0.563887, fecho=0.920812),
        6960: dict(dma=0.238580, vif=1.930560, gif=2.074780, cpu=2.366400, vu1=4.480780, unpack=1.839620,
                   envio=3.467300, entrega=0.413900, hash=2.272480, constantes=1.265580, fecho=1.279740)}
# Sem o date_area, o marcador copia a área inteira que o Resolve copiaria: no console, por quadro de luta, 479 mil de
# 4.337 mil pixels com a consulta (9,4 marcadores, nova1 da rp33). O custo extra por marcador convertido é a classe DATE
# vezes (4337/479 - 1), dividida pelos marcadores do quadro. DATE_CURTO: até quantos desenhos a consulta ainda espera.
DATE_PX = 4337.0 / 479.0
DATE_CURTO = 64
FIM_G = ('tela', 'FXAA', 'outros', 'medição')
FIM_C = ('tela', 'FXAA', 'medição', 'Present', 'limpeza')
INICIO_C = ('outros', 'plano', 'preparo', 'cópia da VU1')  # antes do laço (outros: a estatística de alfa no 6930)


def familia(d):
    n = d['native']
    if n == 5: return 'DATE'
    if n == 1: return 'contorno'
    if n == 3: return 'tabela pela profundidade' if d['src'] == 0 else 'tabela pelos ids'
    if n == 4: return 'cópia dos ids'
    if n >= 100: return 'filme'
    if n: return 'nativa?'
    return {1: 'vu0', 2: 'vu4', 3: 'vu6'}.get(d['vu'], 'comum')


def usa_z(d):
    k = d['pipe']
    return d['native'] == 0 and (((k >> 12) & 1) or ((k >> 10) & 3) >= 2)


def custos(ds, g, c, trechos):
    """g_i e c_i por desenho, mais o início e o fim; conserva os totais das classes (conferido abaixo)."""
    N = len(ds)
    G = [0.0] * N; C = [0.0] * N
    fam = [familia(d) for d in ds]
    visita = []  # entradas em alvo (o que paga entrada/volta), na ordem do laço do frame_end
    cur = None
    for i, d in enumerate(ds):
        if d['native'] == 5 or d['native'] >= 100 or (d['native'] == 0 and d['count'] == 0):
            continue
        if d['alvo'] != cur:
            visita.append(i); cur = d['alvo']
    leitura = [i for i, d in enumerate(ds) if d['native'] == 0 and d['modo'][0] == 2]
    fba = [i for i, d in enumerate(ds) if d['native'] == 0 and d['misc'][2] != 0]
    zvis = [i for i in visita if any(usa_z(ds[j]) for j in range(i, (visita + [N])[visita.index(i) + 1]))]
    ini_g = fim_g = ini_c = fim_c = 0.0

    def espalha(v, alvo, idx, peso=None):
        if not idx:
            return v
        tot = sum(peso[i] for i in idx) if peso else len(idx)
        for i in idx:
            alvo[i] += v * ((peso[i] / tot) if peso else 1.0 / len(idx))
        return 0.0

    sobra = {}
    for nome, tab, V in (('g', G, {k: v[1] for k, v in g.items()}), ('c', C, {k[7:]: v for k, v in c.items() if k.startswith('classe ')})):
        resto_ini = resto_fim = 0.0
        for cl, v in V.items():
            if v == 0.0:
                continue
            if cl in (FIM_G if nome == 'g' else FIM_C):
                resto_fim += v; continue
            if nome == 'c' and cl in INICIO_C:
                resto_ini += v; continue
            if cl in ('vu0', 'vu4', 'vu6'):
                idx = [i for i in range(N) if fam[i] == cl]
                v = espalha(v, tab, idx, {i: max(ds[i]['count'], 1) for i in idx})
            elif cl in ('contorno', 'tabela pela profundidade', 'tabela pelos ids', 'cópia dos ids', 'DATE'):
                v = espalha(v, tab, [i for i in range(N) if fam[i] == cl])
            elif cl == 'FBA':
                v = espalha(v, tab, fba)
            elif cl in ('entradas na EDRAM', 'volta da cor'):
                v = espalha(v, tab, visita)
            elif cl == 'resolves de cor':  # uma parte por leitura de alvo e uma no fim (ed_fim)
                resto_fim += v / (len(leitura) + 1)
                v = espalha(v * len(leitura) / (len(leitura) + 1), tab, leitura)
            elif cl == 'profundidade':  # entradas com z e a entrega da profundidade no fim
                resto_fim += v / 3
                v = espalha(v * 2 / 3, tab, zvis)
            elif cl.startswith('comum '):
                if nome == 'c':
                    v = espalha(v, tab, [i for i in range(N) if fam[i] == 'comum'])
                else:  # GPU: pelos trechos do console (início:fim efetivos)
                    for t in trechos:
                        if t['classe'] != cl:
                            continue
                        idx = [i for i in range(t['ini'], t['fim'] + 1) if i < N and fam[i] == 'comum'] or \
                              [i for i in range(t['ini'], t['fim'] + 1) if i < N]
                        espalha(t['tempos'][1], tab, idx)
                    v = 0.0
            if v:
                sobra[cl] = sobra.get(cl, 0.0) + v
                resto_fim += v
        if nome == 'g':
            ini_g, fim_g = resto_ini, resto_fim
        else:
            ini_c, fim_c = resto_ini, resto_fim
    # o resto do frame_end que nenhuma classe nomeia, por desenho
    r = c['frame_end'] - sum(v for k, v in c.items() if k.startswith('classe '))
    for i in range(N):
        C[i] += max(r, 0.0) / N
    return G, C, ini_g, fim_g, ini_c, fim_c, sobra


def libera(ds, plano_previsto, consultas):
    """O índice de gravação que libera cada desenho (N: o fim do quadro). consultas: conjunto com 'z' e/ou 'date'."""
    N = len(ds)
    rel = [min(i + 1, N) if i < N - 1 else N for i in range(N)]  # fusão
    if not plano_previsto:
        return [N] * N
    if 'z' in consultas:
        cur = None
        for i, d in enumerate(ds):  # usa_z na entrada de cada visita
            if d['native'] == 5 or d['native'] >= 100:
                continue
            if d['alvo'] != cur:
                cur = d['alvo']; j = i
                while j < N:
                    e = ds[j]
                    if e['native'] == 0 and e['alvo'] != cur:
                        break
                    if usa_z(e):
                        break
                    j += 1
                rel[i] = max(rel[i], min(j + 1, N) if j < N else N)
    for i, d in enumerate(ds):  # date_area no marcador: até o próximo marcador (o fim, no último)
        if d['native'] == 5 and ('date' in consultas or 'date_curto' in consultas):
            j = i + 1
            while j < N and ds[j]['native'] != 5:
                j += 1
            if 'date' in consultas or j - i <= DATE_CURTO:
                rel[i] = max(rel[i], j if j < N else N)
    out, m = [], 0
    for x in rel:  # em ordem: ninguém passa o anterior
        m = max(m, x); out.append(m)
    return out


def progresso(ds, q, modelo, curva=None):
    """fração da gravação feita quando o desenho i fecha (o índice N é o fim)."""
    N = len(ds)
    if modelo == 'X':  # o relógio medido a cada 1/16 da lista, interpolado
        assert curva and len(curva) == 17 and curva[-1] == 1000 and all(a <= b for a, b in zip(curva, curva[1:])), 'curva'
        f = []
        for i in range(N):
            x = (i + 1) * 16 / N
            k = min(int(x), 15)
            f.append((curva[k] + (curva[k + 1] - curva[k]) * (x - k)) / 1000)
        return f + [1.0]
    if modelo == 'U':
        w = [1.0] * N
    else:
        p = GRAV[q]
        wvu = p['vu1'] + p['unpack'] + p['constantes'] + p['fecho'] + p['vif']
        wgs = p['gif'] + p['cpu']
        wun = p['envio'] + p['hash'] + p['dma']
        vu = [i for i, d in enumerate(ds) if d['vu']]
        gs = [i for i, d in enumerate(ds) if not d['vu']]
        sv = sum(max(ds[i]['count'], 1) for i in vu) or 1
        sg = sum(max(ds[i]['count'], 1) for i in gs) or 1
        w = [wun / N] * N
        for i in vu: w[i] += wvu * max(ds[i]['count'], 1) / sv
        for i in gs: w[i] += wgs * max(ds[i]['count'], 1) / sg
    tot = sum(w); acc = 0.0; f = []
    for x in w:
        acc += x; f.append(acc / tot)
    return f + [1.0]


def simula(G, C, ini_c, fim_c, fim_g, R, Gfim, sc, sg):
    t_c = R[0] if R else Gfim
    t_c += ini_c * sc
    u = 0.0; u0 = None
    for i in range(len(G)):
        t_c = max(t_c, R[i]) + C[i] * sc
        u = max(u, t_c) + G[i] * sg
        if u0 is None and G[i] > 0:
            u0 = u - G[i] * sg
    t_c = max(t_c, Gfim) + fim_c * sc
    u = max(u, t_c) + fim_g * sg
    return t_c, u, u0


def analisa(q, ds, ab, sens, curva=None, medido=None):
    g = ab['gpu'][str(q)]['classes']; c = ab['cpu'][str(q)]
    trechos = [t for t in ab['trechos'] if t['quadro'] == q]
    G, C, ini_g, fim_g, ini_c, fim_c, sobra = custos(ds, g, c, trechos)
    totg = sum(v[1] for v in g.values()); somag = sum(G) + ini_g + fim_g
    assert abs(totg - somag) < 1e-4, ('GPU não conservada', totg, somag)
    m = (medido or MEDIDO)[q]; N = len(ds)
    # calibração na partição atual (tudo em G): P e U medidos sem marcas
    base_R = [m['G']] * N
    sc = (m['P'] - m['G']) / (sum(C) + ini_c + fim_c)
    lo, hi = 0.1, 2.0
    for _ in range(80):
        sg = (lo + hi) / 2
        if simula(G, C, ini_c, fim_c, fim_g, base_R, m['G'], sc, sg)[1] < m['U']: lo = sg
        else: hi = sg
    sg = (lo + hi) / 2
    P0, U0, _ = simula(G, C, ini_c, fim_c, fim_g, base_R, m['G'], sc, sg)
    linhas = [f'quadro {q}: {N} desenhos, GPU líquida {totg:.3f} ms (fim {fim_g:.3f}), CPU frame_end {c["frame_end"]:.3f} '
              f'(início {ini_c:.3f}, fim {fim_c:.3f}); escalas sem marcas CPU {sc:.3f} GPU {sg:.3f}; '
              f'calibrado P {P0:.2f} U {U0:.2f} (medido {m["P"]:.1f}/{m["U"]:.1f}), meta U <= {m["prazo"] - 1:.1f}']
    if sobra:
        linhas.append(f'  classes sem desenho no quadro, no fim: {sobra}')
    res = {}
    for nome, pp, cons in (('atual', False, {'z', 'date'}), ('plano integral', False, set()),
                           ('previsto, z e DATE adiante', True, {'z', 'date'}), ('previsto, só z adiante', True, {'z'}),
                           ('previsto, só DATE adiante', True, {'date'}), ('previsto, z e DATE curto', True, {'z', 'date_curto'}),
                           ('previsto, nenhuma adiante', True, set())):
        rel = libera(ds, pp, cons)
        for mod in ('U',) + (('W',) if q in GRAV else ()) + (('X',) if curva else ()):
            f = progresso(ds, q, mod, curva)
            R = [f[k] * m['G'] if k < N else m['G'] for k in rel] if nome != 'atual' else base_R
            Gx = list(G)
            marc = [i for i, d in enumerate(ds) if d['native'] == 5]
            extra = g['DATE'][1] * (DATE_PX - 1) / max(len(marc), 1)
            for i in marc:  # o marcador sem a consulta copia a área inteira
                j = i + 1
                while j < N and ds[j]['native'] != 5:
                    j += 1
                if pp and 'date' not in cons and not ('date_curto' in cons and j - i <= DATE_CURTO):
                    Gx[i] += extra
            P, U, u0 = simula(Gx, C, ini_c, fim_c, fim_g, R, m['G'], sc, sg)
            antes = sum(1 for k in rel if k < N)
            prim = next((i for i, k in enumerate(rel) if k >= N), N)
            res[(nome, mod)] = U
            linhas.append(f'  {nome:30s} progresso {mod}: liberados antes de G {antes:4d}/{N} (o {prim} espera o fim), GPU começa {u0:5.1f}, '
                          f'P {P:5.1f}, U {U:5.1f}, ganho {U0 - U:5.1f}, folga {m["prazo"] - U:5.1f} ms')
    # sensibilidade: a fração da gravação em que a última grande região começa (o modelo W desloca tudo)
    if sens and q in GRAV:
        rel = libera(ds, True, {'z', 'date_curto'})
        for frac in (0.5, 0.6, 0.7, 0.8, 0.9):
            f0 = progresso(ds, q, 'W')
            k0 = sens[q]
            esc = frac / f0[k0]
            f = [min(1.0, x * esc) if j <= k0 else frac + (x - f0[k0]) * (1 - frac) / (1 - f0[k0]) for j, x in enumerate(f0)]
            R = [f[k] * m['G'] if k < N else m['G'] for k in rel]
            Gx = list(G)
            for i in [i for i, d in enumerate(ds) if d['native'] == 5]:
                j = i + 1
                while j < N and ds[j]['native'] != 5:
                    j += 1
                if j - i > DATE_CURTO:
                    Gx[i] += g['DATE'][1] * (DATE_PX - 1) / sum(1 for d in ds if d['native'] == 5)
            P, U, u0 = simula(Gx, C, ini_c, fim_c, fim_g, R, m['G'], sc, sg)
            linhas.append(f'  sensibilidade (z e DATE curto), desenho {k0} gravado a {frac:.0%} de G ({frac * m["G"]:4.1f} ms): U {U:5.1f}, '
                          f'folga {m["prazo"] - U:5.1f}')
    return linhas, res


def le_lista(texto, q):
    L = [json.loads(x) for x in texto.splitlines() if x.strip()]
    h, ds = L[0], L[1:]
    assert h['quadro'] == q and h['desenhos'] == len(ds) and [d['n'] for d in ds] == list(range(len(ds))), 'lista'
    return ds


def curvas(texto):
    out = {}
    for q, v in re.findall(r'^prefixos2: curva quadro (\d+) desenhos \d+ ms [0-9.]+ milesimos((?: \d+){17})$', texto, re.M):
        out[int(q)] = [int(x) for x in v.split()]
    return out


def mutantes():
    """Um quadro sintético: GPU 10 ms, CPU 3; calibra, conserva e ordena as partições; cópias estragadas recusadas."""
    tipos = [(0, 0, 1), (1, 0, 0), (0, 0, 0), (0, 5, 0), (0, 0, 0), (2, 0, 0), (0, 0, 0), (0, 0, 0)]
    ds = []
    for n, (vu, nat, z) in enumerate(tipos):
        ds.append({'n': n, 'vu': vu, 'native': nat, 'src': 0, 'alvo': 1 if n == 4 else 0, 'count': 3,
                   'pipe': 0x1400 if z else 0x0400, 'modo': [0, 0, 0, 0], 'misc': [0, 0, 0, 0]})
    texto = json.dumps({'quadro': 1, 'desenhos': len(ds)}) + '\n' + '\n'.join(json.dumps(d) for d in ds)
    g = {'vu0': [1, 1.0, 0, 1], 'vu4': [1, 1.0, 0, 1], 'DATE': [0.5, 0.5, 0, 1], 'comum sem': [6, 6.0, 0, 2],
         'entradas na EDRAM': [0.5, 0.5, 0, 3], 'tela': [1, 1.0, 0, 1]}
    c = {'frame_end': 3.0, 'classe vu0': 1.0, 'classe comum sem': 1.0, 'classe plano': 0.2, 'classe Present': 0.1}
    trechos = [{'quadro': 1, 'classe': 'comum sem', 'ini': 0, 'fim': 4, 'tempos': [3, 3.0, 0, 1]},
               {'quadro': 1, 'classe': 'comum sem', 'ini': 6, 'fim': 7, 'tempos': [3, 3.0, 0, 1]}]
    ab = {'gpu': {'1': {'classes': g}}, 'cpu': {'1': c}, 'trechos': trechos}
    medido = {1: dict(prazo=20.0, G=10.0, P=13.0, U=24.0)}

    def roda(ds_, ab_, curva=None, med=medido):
        linhas, res = analisa(1, ds_, ab_, None, curva, med)
        assert abs(res[('atual', 'U')] - med[1]['U']) < 1e-6, 'calibração'
        assert res[('previsto, z e DATE adiante', 'U')] <= res[('atual', 'U')] + 1e-9, 'liberar antes nunca atrasa U'
        assert res[('plano integral', 'U')] == res[('atual', 'U')], 'plano integral'
        return res
    roda(le_lista(texto, 1), ab, [0] + [60 * k for k in range(1, 16)] + [1000])
    estragos = [('lista sem um desenho', lambda: le_lista(texto.replace(json.dumps(ds[3]) + '\n', ''), 1)),
                ('trecho do comum perdido', lambda: roda(ds, dict(ab, trechos=trechos[:1]))),
                ('curva fora de ordem', lambda: roda(ds, ab, [0, 500, 400] + [600] * 13 + [1000])),
                ('curva sem fim', lambda: roda(ds, ab, list(range(0, 1020, 60))[:17])),
                ('U medido antes do possível', lambda: roda(ds, ab, None, {1: dict(medido[1], U=11.0)}))]
    for nome, f in estragos:
        try:
            f()
        except AssertionError as e:
            print(f'mutante recusado: {nome} ({e})')
        else:
            raise AssertionError(f'mutante aceito: {nome}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('listas', type=Path, nargs='?'); ap.add_argument('dados', type=Path, nargs='?')
    ap.add_argument('--aberturas', default='g6930_1,g6960_1,g6930_2,g6960_2')
    ap.add_argument('--curva', type=Path, help='log do falso com BT3_GS_PREFIXOS=2 (linhas "prefixos2: curva")')
    ap.add_argument('--mutantes', action='store_true')
    a = ap.parse_args()
    try:
        if a.mutantes:
            mutantes(); print('tudo certo nos mutantes do teto'); return
        if a.listas is None or a.dados is None:
            ap.error('informe LISTAS e DADOS')
        dados = json.loads(a.dados.read_text())
        cv = curvas(a.curva.read_text(errors='replace')) if a.curva else {}
        for nome in a.aberturas.split(','):
            ab = dados[nome]; q = int(list(ab['gpu'])[0])
            ds = le_lista((a.listas / f'{q}.jsonl').read_text(), q)
            if a.curva:
                assert q in cv, f'curva do {q} ausente em {a.curva}'
            linhas, _ = analisa(q, ds, ab, {6960: 1152, 6930: 128}, cv.get(q))
            print(f'== {nome}'); print('\n'.join(linhas))
        print('tudo certo; teto idealizado, não é ganho medido')
    except (OSError, AssertionError, KeyError) as e:
        ap.exit(1, f'FALHA: {e}\n')


if __name__ == '__main__':
    main()
