#!/usr/bin/env python3
"""F7, etapa 4: o veredito de cada caso do lote e o resumo por família contra os denominadores (plano/F7/04.md).

  f7_lote.py caso --pasta DIR [--saida veredito.json]
      DIR é a pasta de um caso que o f7_lote.sh preservou: rc.txt (a saída do roteiros.sh), roteiros.log, roteiro.txt e
      caso.json (as cópias do que rodou) e as saídas textuais do roteiros.sh (x86_a.txt, ppcsm.txt, gdb.txt,
      mem_*/memoria.txt e memdiff.txt, imagens.txt). Sai 0 só com o caso aprovado.
  f7_lote.py resumo --casos DIR [DIR ...] --manifesto MANIFESTO.jsonl [--completo] [--saida resumo.json]
      os vereditos (DIR/*/veredito.json, de uma ou das duas máquinas) contra o manifesto da etapa 1 (f7_manifesto.py,
      fora do git): por família, o que passou, o que falhou e o que falta. Sai 1 com qualquer falha; com --completo,
      também com qualquer item faltando.
  f7_lote.py negativos --pasta DIR --saida DIR_NOVO [--manifesto MANIFESTO.jsonl]
      estraga cópias de um caso aprovado, uma coisa por vez, e exige que o "caso" saia 1 em cada uma; com o
      manifesto, também o resumo (um caso reprovado, e --completo com itens faltando).

Créditos de um caso do censo do elenco: "selecao" (cada membro escolhido pela entrada na TeamSel, conferido no
estado da tela), "carga" (o jogo carregou o par pelo BtlLoad_StepChara, na abertura da luta ou na troca depois do KO)
e nunca "combate": o KO é dado pelo teste (termina luta). Um roteiro com passo que força estado do jogo (receita,
prepara, funde, libera, danifica, força) não ganha crédito de entrada.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

from f7_dados import exigir

AQUI = Path(__file__).resolve()
N_IDS = 161
MODELO, VOZ = 0x590, {'normal': 0xBDA, 'alt': 0xC7B}
FORCADOS = ('receita', 'prepara', 'funde', 'libera', 'danifica', 'forca')


def arquivo(file_id):
    """o nome do arquivo de um id de arquivo do jogo (id = índice + 1)"""
    return f'pzs3us1/{file_id - 1:05d}.bin'


def ler(p):
    return Path(p).read_text(errors='replace')


def esperado_elenco(caso):
    """as cargas, os arquivos e as linhas de time que o caso do censo tem de produzir"""
    cargas = Counter()
    leu = set()
    for lado, membros in enumerate(caso['lados']):
        for i, t in membros:
            cargas[(lado, i, t, 0)] += 1
            leu |= {arquivo(MODELO + i * 10 + t), arquivo(MODELO + 8 + i * 10), arquivo(MODELO + 9 + i * 10),
                    arquivo(8 + i * 2 + lado), arquivo(VOZ[caso['voz']] + i)}
    leu.add(arquivo(0x171 + caso['estagio']))
    return cargas, leu


def caso_elenco(pasta, caso):
    erros = []
    falta = lambda c, m: erros.append(m) if not c else None
    rot = ler(pasta / 'roteiro.txt')
    passos = [l.split('#')[0].split() for l in rot.splitlines()]
    forcados = sorted({p[0] for p in passos if p and p[0].lower() in FORCADOS})
    falta(not forcados, f'roteiro com passo que força o estado: {forcados}')
    falta(any(p[:2] == ['rastreia', 'elenco'] for p in passos), 'roteiro sem "rastreia elenco"')
    falta(ler(pasta / 'rc.txt').strip() == '0', f'roteiros.sh saiu com {ler(pasta / "rc.txt").strip()}')
    log = ler(pasta / 'roteiros.log')
    falta(re.search(r'^== tudo certo$', log, re.M) and 'NÃO VALE' not in log, 'roteiros.sh sem "== tudo certo"')
    cargas_esp, leu_esp = esperado_elenco(caso)
    linhas = {}
    for nome in ('x86_a.txt', 'ppcsm.txt'):
        texto = ler(pasta / nome)
        falta(re.search(r'^roteiro: \S+ fim vblank', texto, re.M), f'{nome}: sem fim')
        falta('FALHA' not in texto, f'{nome}: com FALHA')
        times = re.findall(r'^roteiro: elenco time lado (\d) membros (\d+):((?: -?\d+/-?\d+/-?\d+)*) estagio (\d+) '
                           r'dividida (\d+) vblank', texto, re.M)
        esp = [(str(l), str(len(m)), ''.join(f' {i}/{t}/0' for i, t in m), str(caso['estagio']), '0')
               for l, m in enumerate(caso['lados'])]
        falta(times == esp, f'{nome}: luta montada {times}, esperada {esp}')
        cargas = re.findall(r'^roteiro: elenco carga lado (\d) id (\d+) traje (\d+) variante (\d+) modelo (\d+) '
                            r'anim (\d+) (\d+) vblank', texto, re.M)
        vistas = Counter((int(l), int(i), int(t), int(v)) for l, i, t, v, *_ in cargas)
        falta(vistas == cargas_esp, f'{nome}: cargas {sorted(vistas.elements())}, esperadas {sorted(cargas_esp.elements())}')
        for l, i, t, v, m, a0, a1 in cargas:
            i, t, v = int(i), int(t), int(v)
            falta((int(m), int(a0), int(a1)) == (MODELO + i * 10 + t + 4 * (v != 0), MODELO + 8 + i * 10, MODELO + 9 + i * 10),
                  f'{nome}: arquivos da carga {l} {i}/{t}/{v} fora da fórmula do BtlLoad_StepChara')
        # cada carga depois da abertura vem depois de um KO daquele lado (a troca do jogo, não um pedido do teste)
        abertura = re.search(r'^roteiro: elenco time lado 1 .* vblank (\d+) ', texto, re.M)
        for lado in (0, 1):
            ordem = re.findall(rf'^roteiro: (?:elenco carga lado {lado} .*vblank (\d+) |\S+ derrubado lado {lado} vblank (\d+) )',
                               texto, re.M)
            kos = 0
            for carga, ko in ordem:
                if ko:
                    kos += 1
                elif abertura and carga != abertura.group(1):
                    falta(kos > 0, f'{nome}: carga do lado {lado} no vblank {carga} sem KO antes')
                    kos -= 1
        for lado, membros in enumerate(caso['lados']):
            for i, t in membros:
                falta(re.search(rf'^roteiro: \S+ ok time {lado} chara {i} ', texto, re.M),
                      f'{nome}: sem a escolha do id {i} no lado {lado} pela TeamSel')
            cores = Counter(int(c) for c in re.findall(rf'^roteiro: \S+ ok time {lado} cor (\d+) ', texto, re.M))
            falta(cores == Counter(t for _, t in membros), f'{nome}: trajes escolhidos no lado {lado} {dict(cores)}')
        if caso['voz'] == 'normal':
            falta(re.search(r'^roteiro: \S+ ok voz 0 ', texto, re.M), f'{nome}: sem a outra voz conferida')
        else:
            falta(re.search(r'^roteiro: \S+ ok voz 1 ', texto, re.M), f'{nome}: sem a voz do New Game conferida')
        linhas[nome] = [c for c in texto.splitlines() if c.startswith('roteiro: elenco ')]
    falta(linhas.get('x86_a.txt') == linhas.get('ppcsm.txt'), 'linhas do elenco diferentes entre a x86 e a PPC sem mapa')
    leu = set(re.findall(r'^LEU (\S+)', ler(pasta / 'gdb.txt'), re.M))
    faltam = sorted(leu_esp - leu)
    falta(not faltam, f'gdb.txt: arquivos não lidos {faltam[:12]}')
    # a memória no resultado: os dois últimos modelos vivos e conformes, e o julgamento do roteiros.py
    mem = pasta / 'mem_ponto_resultado'
    falta((mem / 'memoria.txt').exists() and re.search(r'^memória igual$', ler(mem / 'memoria.txt'), re.M),
          'memória do ponto_resultado sem "memória igual"')
    md = ler(mem / 'memdiff.txt') if (mem / 'memdiff.txt').exists() else ''
    for lado in (0, 1):
        i, t = caso['lados'][lado][-1]
        nome = arquivo(MODELO + i * 10 + t).replace('/', '_')
        falta(re.search(rf'^marcas heap#\d+ {re.escape(nome)}\.marcas: conforme [1-9]\d*, fora-do-formato 0$', md, re.M),
              f'memória do resultado sem o modelo {nome} do lado {lado} vivo e conforme')
    imagens = ler(pasta / 'imagens.txt') if (pasta / 'imagens.txt').exists() else ''
    falta(imagens.count('carga_lado_') == sum(cargas_esp.values()), 'imagens: falta a imagem de alguma carga')
    cob = dict(pares=sorted({(i, t) for m in caso['lados'] for i, t in m}),
               rostos=sorted({(i, l) for l, m in enumerate(caso['lados']) for i, _ in m}),
               vozes=sorted({(i, caso['voz']) for m in caso['lados'] for i, _ in m}),
               animacoes=sorted({i for m in caso['lados'] for i, _ in m}),
               estagios=[caso['estagio']])
    return erros, cob, dict(selecao=not forcados, carga=not forcados, combate=False)


def veredito(pasta):
    pasta = Path(pasta)
    caso = json.loads(ler(pasta / 'caso.json'))
    exigir(caso['nome'].startswith('f7_elenco_'), f'família sem verificador: {caso["nome"]}')
    try:
        erros, cob, creditos = caso_elenco(pasta, caso)
    except (OSError, ValueError, IndexError) as e:
        erros, cob, creditos = [f'saída incompleta: {e}'], {}, {}
    return dict(nome=caso['nome'], familia='elenco', resultado='ok' if not erros else 'falha', erros=erros,
                creditos=creditos, cobertura=cob)


def denominadores(manifesto):
    fam = defaultdict(set)
    for l in Path(manifesto).read_text().splitlines():
        x = json.loads(l)
        if x['tipo'] == 'modelo':
            fam['elenco_carga_v0' if x['variante'] == 0 else 'elenco_carga_v1'].add((x['personagem'], x['traje']))
        elif x['tipo'] == 'personagem':
            fam['elenco_animacoes'].add(x['id'])
            fam['elenco_luta_entrada'].add(x['id'])
            for lado in (0, 1):
                fam['elenco_rostos'].add((x['id'], lado))
            for voz in VOZ:
                fam['elenco_vozes'].add((x['id'], voz))
        elif x['tipo'] == 'estagio':
            fam['estagios_' + x['variante']].add(x['estagio'])
        elif x['tipo'] in ('transformacao', 'fusao'):
            fam[x['tipo']].add(x['id'])
    exigir(len(fam['elenco_carga_v0']) == 349 and len(fam['elenco_rostos']) == 322 and len(fam['estagios_normal']) == 35,
           'manifesto com denominadores diferentes dos da 01.md')
    return fam


def resumo(dirs, manifesto, completo, saida):
    fam = denominadores(manifesto)
    mapa = dict(pares='elenco_carga_v0', rostos='elenco_rostos', vozes='elenco_vozes', animacoes='elenco_animacoes',
                estagios='estagios_normal')
    passou, falhou = defaultdict(set), defaultdict(set)
    casos = {}
    for d in dirs:
        for v in sorted(Path(d).glob('*/veredito.json')):
            r = json.loads(v.read_text())
            exigir(r['nome'] not in casos or casos[r['nome']] == r['resultado'], f'{r["nome"]}: vereditos divergentes')
            casos[r['nome']] = r['resultado']
            alvo = passou if r['resultado'] == 'ok' else falhou
            for chave, itens in r['cobertura'].items():
                for it in itens:
                    alvo[mapa[chave]].add(tuple(it) if isinstance(it, list) else it)
    linhas, tabela, rc = [], {}, 0
    for f in sorted(fam):
        ok = passou[f] & fam[f]
        ruim = (falhou[f] & fam[f]) - ok
        resta = fam[f] - ok - ruim
        tabela[f] = dict(total=len(fam[f]), passou=len(ok), falhou=len(ruim), falta=len(resta),
                         falhas=sorted(map(str, ruim))[:40], faltas=sorted(map(str, resta))[:40])
        linhas.append(f'{f:22s} {len(fam[f]):4d} total  {len(ok):4d} passou  {len(ruim):4d} falhou  {len(resta):4d} falta')
        if ruim or (completo and resta):
            rc = 1
    nok = sum(r == 'ok' for r in casos.values())
    linhas.append(f'casos: {nok} aprovados, {len(casos) - nok} reprovados: '
                  + ', '.join(sorted(n for n, r in casos.items() if r != 'ok')))
    print('\n'.join(linhas))
    if saida:
        Path(saida).write_text(json.dumps(dict(familias=tabela, casos=casos), indent=1, ensure_ascii=False) + '\n')
    if any(r != 'ok' for r in casos.values()):
        rc = 1
    faltam = sum(t['falta'] for t in tabela.values())
    if rc:
        print('resumo: REPROVADO' + (' (ou incompleto, com --completo)' if completo else ''))
    else:
        print('resumo: sem falhas' + (f'; faltam {faltam} itens (parcial)' if faltam else '; completo'))
    return rc


def _troca(alvo, padrao, novo):
    def f(copia, caso):
        a = copia / alvo
        texto = ler(a)
        mudado = re.sub(padrao, novo, texto, count=1, flags=re.M)
        exigir(mudado != texto, f'{alvo}: o estrago não achou o alvo')
        a.write_text(mudado)
    return f


def _sem_linha(alvo, achar):
    """apaga a linha que `achar(caso)` devolve (exata)"""
    def f(copia, caso):
        a = copia / alvo
        linhas = ler(a).splitlines(keepends=True)
        alvo_l = achar(caso, linhas)
        exigir(alvo_l in linhas, f'{alvo}: o estrago não achou a linha')
        linhas.remove(alvo_l)
        a.write_text(''.join(linhas))
    return f


def _traje_do_caso(copia, caso):
    c = json.loads(ler(copia / 'caso.json'))
    i, t = c['lados'][0][0]
    c['lados'][0][0] = [i, t + 1]
    (copia / 'caso.json').write_text(json.dumps(c))


# Cada estrago, numa cópia do caso aprovado, tem de levar o "caso" a sair com 1.
ESTRAGOS = [
    # a segunda carga do lado 1 (a primeira troca depois de um KO) some do rastro da x86
    ('sem-carga', _sem_linha('x86_a.txt', lambda c, ls: [l for l in ls if l.startswith('roteiro: elenco carga lado 1 ')][1])),
    ('carga-trocada', _troca('ppcsm.txt', r'^(roteiro: elenco carga lado 0 id )(\d+)', r'\g<1>9\2')),
    ('sem-rosto', _sem_linha('gdb.txt', lambda c, ls: f'LEU {arquivo(8 + c["lados"][1][0][0] * 2 + 1)}\n')),
    ('sem-voz', _sem_linha('gdb.txt', lambda c, ls: f'LEU {arquivo(VOZ[c["voz"]] + c["lados"][0][-1][0])}\n')),
    ('rc-1', _troca('rc.txt', r'^0', '1')),
    ('sem-tudo-certo', _troca('roteiros.log', r'^== tudo certo$', '== 1 falha(s):')),
    ('sem-fim-ppc', _troca('ppcsm.txt', r'^(roteiro: \S+) fim vblank', r'\1 fim_apagado vblank')),
    ('forcado', _troca('roteiro.txt', r'\Z', 'receita aplica\n')),
    ('sem-selecao', _troca('x86_a.txt', r'^roteiro: \S+ ok time 0 cor \d+ .*\n', '')),
    ('traje-do-caso', _traje_do_caso),
    ('sem-modelo-na-memoria', _sem_linha('mem_ponto_resultado/memdiff.txt', lambda c, ls: next(
        l for l in ls if re.match(rf'marcas heap#\d+ {arquivo(MODELO + c["lados"][1][-1][0] * 10 + c["lados"][1][-1][1]).replace("/", "_")}\.marcas: conforme', l)))),
    ('memoria-diferente', _troca('mem_ponto_resultado/memoria.txt', r'^memória igual$', 'memória DIFERENTE')),
    ('sem-imagem', _troca('imagens.txt', r'^.*carga_lado_.*\n', '')),
]


def negativos(pasta, saida, manifesto=None):
    pasta, saida = Path(pasta), Path(saida)
    saida.mkdir(parents=True, exist_ok=False)
    eu = [sys.executable, str(AQUI), 'caso']
    r = subprocess.run(eu + ['--pasta', str(pasta)], capture_output=True, text=True)
    exigir(r.returncode == 0, f'o caso íntegro não passa: {r.stdout[-400:]}')
    caso = json.loads(ler(pasta / 'caso.json'))
    for nome, estraga in ESTRAGOS:
        copia = saida / nome
        shutil.copytree(pasta, copia)
        estraga(copia, caso)
        r = subprocess.run(eu + ['--pasta', str(copia)], capture_output=True, text=True)
        exigir(r.returncode == 1, f'{nome}: o caso estragado saiu com {r.returncode}: {r.stdout[-300:]}')
        motivo = next((l for l in r.stdout.splitlines() if l.startswith('  - ')), '').strip()
        print(f'negativo {nome}: recusado ({motivo[:160]})')
        shutil.rmtree(copia)
    n = len(ESTRAGOS)
    if manifesto:
        # o resumo: o caso íntegro sozinho passa sem --completo e reprova com ele (faltam itens); o veredito do caso
        # mudado para falha reprova sem --completo
        lote = saida / 'lote'
        shutil.copytree(pasta, lote / pasta.name)
        (lote / pasta.name / 'veredito.json').write_text(json.dumps(veredito(pasta)))
        base = [sys.executable, str(AQUI), 'resumo', '--casos', str(lote), '--manifesto', str(manifesto)]
        exigir(subprocess.run(base, capture_output=True).returncode == 0, 'resumo do caso íntegro reprovado')
        exigir(subprocess.run(base + ['--completo'], capture_output=True).returncode == 1, 'resumo --completo aceitou faltas')
        print('negativo resumo-completo: recusado (itens faltando)')
        v = json.loads(ler(lote / pasta.name / 'veredito.json'))
        v['resultado'] = 'falha'
        (lote / pasta.name / 'veredito.json').write_text(json.dumps(v))
        exigir(subprocess.run(base, capture_output=True).returncode == 1, 'resumo aceitou um caso reprovado')
        print('negativo resumo-falha: recusado (caso reprovado)')
        shutil.rmtree(lote)
        n += 2
    print(f'negativos: {n} estragos recusados, tudo certo')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('acao', choices=['caso', 'resumo', 'negativos'])
    p.add_argument('--pasta', type=Path)
    p.add_argument('--saida', type=Path)
    p.add_argument('--casos', type=Path, nargs='+')
    p.add_argument('--manifesto', type=Path)
    p.add_argument('--completo', action='store_true')
    a = p.parse_args()
    try:
        if a.acao == 'caso':
            r = veredito(a.pasta)
            if a.saida:
                a.saida.write_text(json.dumps(r, indent=1, ensure_ascii=False) + '\n')
            print(f'{r["nome"]}: {r["resultado"]}')
            for e in r['erros']:
                print(f'  - {e}')
            return 0 if r['resultado'] == 'ok' else 1
        if a.acao == 'resumo':
            return resumo(a.casos, a.manifesto, a.completo, a.saida)
        negativos(a.pasta, a.saida, a.manifesto)
        return 0
    except (ValueError, OSError, KeyError) as e:
        print(f'FALHA: {e}')
        return 1


if __name__ == '__main__':
    sys.exit(main())
