#!/usr/bin/env python3
"""Diferenças nominais de chamadas/arquivos da F7, sem executar jogo.

coletar --rodada DIR --saida DIR: instantâneo dos gdb.txt, disco.log e proveniência.
conferir --coleta DIR --justificativas JSON --saida JSON: toda ausência sem motivo/prova sai 1.
Justificar não equivale a executar; a saída discrimina pendente, inativo e dispensa aprovada.
"""
import argparse
import ast
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import sys

from f7_dados import exigir
import roteiros
import unpacks

RAIZ = Path(__file__).resolve().parents[3]


def hash_arq(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as f:
        while b := f.read(1024 * 1024):
            h.update(b)
    return h.hexdigest()


def numero(s):
    def calcular(n):
        if isinstance(n, ast.Constant) and type(n.value) is int:
            return n.value
        if isinstance(n, ast.BinOp) and isinstance(n.op, (ast.Add, ast.Sub)):
            a, b = calcular(n.left), calcular(n.right)
            return a + b if isinstance(n.op, ast.Add) else a - b
        raise ValueError(f'expressão de faixa desconhecida: {s}')
    return calcular(ast.parse(s.strip(), mode='eval').body)


def faixas():
    texto = (RAIZ / 'port/x360/conv/conv.c').read_text()
    texto = texto.split('static const ConvFaixa sFaixas[] = {', 1)[1].split('};', 1)[0]
    texto = re.sub(r'/\*.*?\*/', '', texto, flags=re.S)
    arquivos = {}
    entradas = re.findall(r'\{([^,]+),([^,]+),([^,]+),\s*"([\w]+)"\s*\}', texto)
    exigir(len(entradas) == texto.count('{'), 'faixa não reconhecida em sFaixas')
    for a, b, secao, formato in entradas:
        if secao.strip() != 'CONV_ARQ':
            continue
        ini, fim = numero(a), numero(b)
        exigir(0 <= ini <= fim < 3399, 'faixa fora da partição 1')
        for i in range(ini, fim + 1):
            nome = f'pzs3us1/{i:05d}.bin'
            exigir(nome not in arquivos, f'faixa repetida: {nome}')
            arquivos[nome] = formato
    # Conv_FormatoDe também chama do_lutador: a faixa é calculada, fora de sFaixas.
    todo = (RAIZ / 'port/x360/conv/conv.c').read_text()
    regra = todo.split('static const char *do_lutador(int i) {', 1)[1].split('\n}', 1)[0]
    exigir(re.search(r'int k = i \+ 1 - 0x590;', regra) and re.search(r'k < 161 \* 10', regra),
           'do_lutador mudou; reveja a regra de seleção de arquivos')
    for k in range(161 * 10):
        nome = f'pzs3us1/{0x590 - 1 + k:05d}.bin'
        exigir(nome not in arquivos, f'faixa do lutador sobreposta: {nome}')
        arquivos[nome] = {8: 'personagem_anim', 9: 'personagem_efeitos'}.get(k % 10, 'personagem')
    exigir(arquivos, 'nenhuma faixa de arquivo')
    return arquivos


def destino_externo(p):
    exigir(not p.resolve().is_relative_to(RAIZ), 'saída deve ficar fora do repositório')


def coletar(rodada, saida):
    destino_externo(saida)
    exigir(not saida.exists(), 'coleta já existe; preserve o instantâneo anterior')
    saida.mkdir(parents=True)
    inicio = datetime.now(timezone.utc).isoformat()
    arquivos = []
    for nome in ('origem.json', 'casos.json', 'disco.log'):
        p = rodada / nome
        b = p.read_bytes()
        (saida / nome).write_bytes(b)
        arquivos.append(dict(nome=nome, tamanho=len(b), sha256=hashlib.sha256(b).hexdigest()))
    logs = sorted((rodada / 'roteiros').glob('*/gdb.txt'))
    exigir(logs, 'rodada sem gdb.txt')
    for p in logs:
        rel = p.relative_to(rodada)
        # Uma única leitura por arquivo. Não espera nem interfere na execução alheia.
        b = p.read_bytes()
        (saida / rel).parent.mkdir(parents=True, exist_ok=True)
        (saida / rel).write_bytes(b)
        arquivos.append(dict(nome=str(rel), tamanho=len(b), sha256=hashlib.sha256(b).hexdigest()))
    prog = rodada / 'final/port/build/x86/bt3_x86'
    exigir(prog.is_file(), 'binário x86 da rodada ausente')
    meta = dict(inicio_utc=inicio, fim_utc=datetime.now(timezone.utc).isoformat(), rodada=str(rodada.resolve()),
                prog=str(prog.resolve()), sha256_prog=hash_arq(prog), arquivos=arquivos,
                limite='instantâneo sequencial; logs parciais não aprovam roteiros; só leitura da rodada')
    (saida / 'coleta.json').write_text(json.dumps(meta, indent=2, ensure_ascii=False) + '\n')
    print(f'coleta: {len(logs)} logs; {inicio}; {meta["fim_utc"]}')


def conferir(coleta, justificativas):
    meta = json.loads((coleta / 'coleta.json').read_text())
    exigir(hash_arq(meta['prog']) == meta['sha256_prog'], 'binário da coleta mudou')
    res, erros, _ = unpacks.levantar()
    exigir(not erros and not any(s == 'pendente' for s, _, _ in res), 'inventário estático reprovado')
    chamadas = {roteiros.chave(o): o for s, o, _ in res if s != 'bpe-direto'}
    arquivos = faixas()
    observadas, lidos = set(), set()
    por_log = []
    nomes_logs = {a['nome'] for a in meta['arquivos'] if a['nome'].endswith('/gdb.txt')}
    exigir(nomes_logs, 'coleta sem lista de logs')
    for nome in sorted(nomes_logs):
        p = coleta / nome
        texto = p.read_text(errors='replace')
        exigir(not any(l.startswith('UNPACK ') and not re.fullmatch(r'UNPACK \d+ -?\d+ 0x[0-9A-Fa-f]+', l.strip())
                       for l in texto.splitlines()), f'{nome}: linha UNPACK inválida/incompleta')
        cs = {k for _, _, k in roteiros.chamadas(p, meta['prog'])}
        ls = {n.strip().replace('\\', '/') for n in roteiros.leu(p)}
        exigir(cs and ls, f'{nome}: sem chamadas ou LEU')
        exigir(not (cs - chamadas.keys()), f'{nome}: chamadas fora do inventário: {sorted(cs - chamadas.keys())}')
        observadas |= cs
        lidos |= ls
        por_log.append(dict(nome=nome, chamadas=len(cs), arquivos=len(ls), sha256=hash_arq(p)))
    disco = (coleta / 'disco.log').read_text()
    convertidos = set(re.findall(r'^(pzs3us1/\d{5}\.bin): convertido;', disco, re.M))
    erros_disco = [l for l in disco.splitlines() if re.match(r'pzs3us1/\d{5}\.bin:', l) and ': convertido;' not in l]
    exigir(not erros_disco, f'--disco: arquivos com erro: {erros_disco[:3]}')
    j = json.loads(justificativas.read_text())
    exigir(set(j) == {'chamadas', 'arquivos'}, 'justificativas: campos esperados chamadas e arquivos')
    ausentes, justificadas, invalidas = {}, {}, []
    for grupo, esperado, visto in [('chamadas', set(chamadas), observadas), ('arquivos', set(arquivos), lidos)]:
        exigir(isinstance(j[grupo], dict), f'justificativas {grupo}: esperado objeto nominal')
        exigir(not (j[grupo].keys() - esperado), f'justificativas {grupo}: nomes fora do inventário')
        ausentes[grupo] = sorted(esperado - visto)
        justificadas[grupo] = {}
        for nome in ausentes[grupo]:
            r = j[grupo].get(nome)
            if r is None:
                continue
            if not isinstance(r, dict) or not all(isinstance(r.get(k), str) and r[k].strip() for k in ('motivo', 'prova', 'tratamento')):
                invalidas.append(f'{grupo}/{nome}: motivo, prova e tratamento obrigatórios')
                continue
            if r['tratamento'] not in ('pendente', 'inativo', 'dispensa aprovada'):
                invalidas.append(f'{grupo}/{nome}: tratamento inválido')
                continue
            if r['tratamento'] == 'dispensa aprovada' and not r.get('aprovacao'):
                invalidas.append(f'{grupo}/{nome}: falta aprovação identificada')
                continue
            justificadas[grupo][nome] = r
    faltam_disco = sorted(set(arquivos) - convertidos)
    extras_disco = sorted(convertidos - set(arquivos))
    sem_justificativa = {g: sorted(set(ausentes[g]) - justificadas[g].keys()) for g in ausentes}
    rc = int(any(sem_justificativa.values()) or bool(invalidas or faltam_disco or extras_disco))
    casos = json.loads((coleta / 'casos.json').read_text())
    return rc, dict(origem=json.loads((coleta / 'origem.json').read_text()), coleta=meta,
                    esperados=dict(chamadas=len(chamadas), arquivos=len(arquivos)),
                    observados=dict(chamadas=len(observadas), arquivos=len(lidos & arquivos.keys())),
                    ausentes=ausentes, justificadas=justificadas, sem_justificativa=sem_justificativa,
                    justificativas_invalidas=invalidas, faltam_disco=faltam_disco, extras_disco=extras_disco,
                    nomes_chamadas=chamadas, formatos=arquivos, por_log=por_log,
                    casos=casos, rc=rc,
                    limite='chamada observada/LEU não prova ação, resultado, save, imagem nem conclusão da F6')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    s = p.add_subparsers(dest='acao', required=True)
    c = s.add_parser('coletar')
    c.add_argument('--rodada', type=Path, required=True)
    c.add_argument('--saida', type=Path, required=True)
    c = s.add_parser('conferir')
    c.add_argument('--coleta', type=Path, required=True)
    c.add_argument('--justificativas', type=Path, required=True)
    c.add_argument('--saida', type=Path, required=True)
    a = p.parse_args()
    try:
        if a.acao == 'coletar':
            coletar(a.rodada, a.saida)
            return 0
        destino_externo(a.saida)
        rc, r = conferir(a.coleta, a.justificativas)
        a.saida.parent.mkdir(parents=True, exist_ok=True)
        a.saida.write_text(json.dumps(r, indent=2, ensure_ascii=False) + '\n')
        for g in ('chamadas', 'arquivos'):
            print(f'{g}: {r["observados"][g]}/{r["esperados"][g]}; {len(r["ausentes"][g])} ausentes; '
                  f'{len(r["justificadas"][g])} justificadas; {len(r["sem_justificativa"][g])} sem justificativa')
            for n in r['sem_justificativa'][g]:
                print(f'  FALTA {g}: {n}')
        for n in r['faltam_disco']:
            print(f'  FALTA --disco: {n}')
        for n in r['extras_disco']:
            print(f'  FORA DAS FAIXAS --disco: {n}')
        print('cobertura: faltam provas/justificativas' if rc else 'cobertura: tudo certo nas diferenças nominais; conferir os tratamentos pendentes')
        return rc
    except (ValueError, OSError, KeyError, IndexError) as e:
        print(f'ERRO: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
