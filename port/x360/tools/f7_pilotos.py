#!/usr/bin/env python3
"""Confere comportamento e cartão dos pilotos da F7; saídas sempre fora do git.

O critério de imagem, memória e equivalência continua sendo roteiros.sh.
Este complemento não concede cobertura aos casos ausentes do manifesto.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import tempfile

from f7_dados import exigir
from f7_cobertura import destino_externo
import save_entre

LOGS = ('x86_a.txt', 'x86_b.txt', 'gdb.txt', 'x86r.txt', 'ppcsm.txt',
        'img/x86/saida.txt', 'img/ppc/saida.txt')


def conferir(caso, pasta):
    observados = {}
    for nome in LOGS:
        texto = (pasta / nome).read_text(errors='replace')
        exigir(re.search(r'^roteiro: \S+ fim ', texto, re.M), f'{nome}: sem fim')
        exigir('FALHA' not in texto, f'{nome}: contém FALHA')
        if caso == 'historia_novo':
            exigir('desbloqueado (Save_UnlockAll)' not in texto, f'{nome}: desbloqueio indevido')
            for acao in (10001, 10002):
                exigir(re.search(rf'gsc acao {acao}\b', texto), f'{nome}: falta evento {acao}')
            for marca in ('resultado', 'recompensa'):
                exigir(f'marca {marca} ' in texto, f'{nome}: falta {marca}')
        elif caso.startswith('f7_missao'):
            for modo in (13, 14, 15, 16):
                exigir(re.search(rf'^tela {modo} ', texto, re.M), f'{nome}: falta tela {modo}')
            exigir(len(re.findall(r'derrubado lado 1 ', texto)) == 4, f'{nome}: não derrubou quatro adversários')
            for valor in (0, 1):
                exigir(f'ok save +0x28C {valor} ' in texto, f'{nome}: falta missão {valor}')
            exigir('marca salvo ' in texto, f'{nome}: falta gravação')
            if caso == 'f7_missao29':
                exigir('Save_UnlockAll' not in texto and 'receita 29 missoes' in texto, f'{nome}: receita não nativa')
                for v in (0, 1):
                    exigir(f'ok save +0x208 {v} ' in texto, f'{nome}: falta marco 29/30 {v}')
        else:
            exigir(caso == 'f7_transformacao', 'caso desconhecido')
            exigir('Save_UnlockAll' not in texto, f'{nome}: desbloqueio indevido')
            neg = texto.split('marca negativo_antes ', 1)[1].split('marca negativo_depois ', 1)[0]
            exigir(re.search(r'ok membro lado 0 id 1 traje 0 variante 0 blast \d+ ', neg), f'{nome}: negativo sem id 1')
            exigir(not re.search(r'f7: membro id 2\b', neg), f'{nome}: negativo transformou')
            exigir(re.search(r'ok acao 0 0xec-0xf0 \(0x(?:e[c-f]|f0)\)', texto), f'{nome}: sem ação de troca')
            exigir(re.search(r'ok membro lado 0 id 2 traje 0 variante 0 blast \d+ ', texto), f'{nome}: sem forma final')
            exigir(re.search(r'pedido 2 custo 100000 classe 2 ', texto), f'{nome}: pedido/custo/classe diferente')
            debitos = re.findall(r'f7: debito custo (\d+) antes (\d+) depois (\d+)', texto)
            exigir(any(int(c) == 100000 and int(a) - int(b) == 100000 for c, a, b in debitos), f'{nome}: falta débito exato')
            exigir(re.search(r'ok acao 0 0xd-0x35 ', texto), f'{nome}: sem retorno controlável')
        observados[nome] = hashlib.sha256((pasta / nome).read_bytes()).hexdigest()
    campos = {}
    if caso != 'f7_transformacao':
        for plataforma, cartao, ordem in [('x86', 'sa', '<'), ('ppc', 'sq', '>')]:
            arq = next(p for p in (pasta / cartao).rglob(save_entre.DADOS) if p.is_file())
            exigir(save_entre.preve(arq, plataforma)[0] == 'carrega', f'{plataforma}: cartão inválido')
            b = arq.read_bytes()
            i32 = lambda pos: struct.unpack_from(ordem + 'i', b, pos)[0]
            if caso == 'historia_novo':
                estado = dict(flags=i32(0x10), abertos=i32(0x14), ganhos=i32(0x18), dinheiro=i32(0x3028))
                exigir(estado['flags'] & 1 and estado['abertos'] == 3 and estado['ganhos'] == 1 and estado['dinheiro'] == 1000,
                       f'{plataforma}: episódio/recompensa incorretos: {estado}')
            else:
                estado = dict(missao0=b[0x28C], concluidas=sum(b[0x28C + 12*k] != 0 for k in range(100)), flags=i32(0x208))
                exigir(estado['missao0'] == 1, f'{plataforma}: missão não persistiu')
                if caso == 'f7_missao29':
                    exigir(estado['concluidas'] == 30 and estado['flags'] & 1, f'{plataforma}: marco 30 incorreto')
            reabertura = pasta / f'reabre_{plataforma}.txt'
            exigir(save_entre.julga(reabertura, 'carrega', 0) == 0, f'{plataforma}: não reabriu normalmente')
            campos[plataforma] = estado
        exigir(campos['x86'] == campos['ppc'], 'campos persistidos divergem')
    return dict(caso=caso, logs=observados, campos=campos, resultado='tudo certo')


def negativos(caso, pasta, saida):
    """Estraga cópias mínimas dos insumos efetivamente conferidos, uma mudança por vez."""
    saida.mkdir(exist_ok=False, parents=True)
    destinos = [('sem-fim', ' fim ', ' fim_apagado ')]
    if caso == 'historia_novo':
        destinos += [('sem-evento', 'gsc acao 10002', 'gsc acao 99999')]
    elif caso == 'f7_transformacao':
        destinos += [('sem-debito', 'f7: debito custo', 'f7: debito_apagado custo'),
                     ('forma-errada', 'ok membro lado 0 id 2 ', 'ok membro lado 0 id 3 ')]
    else:
        destinos += [('sem-ganho', 'ok save +0x28C 1 ', 'ok save +0x28C 0 ')]
    for nome, origem, novo in destinos:
        with tempfile.TemporaryDirectory(dir=saida) as td:
            copia = Path(td)
            for rel in (*LOGS, 'reabre_x86.txt', 'reabre_ppc.txt'):
                if (pasta / rel).exists():
                    (copia / rel).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(pasta / rel, copia / rel)
            for cartao in ('sa', 'sq'):
                if (pasta / cartao).exists(): shutil.copytree(pasta / cartao, copia / cartao)
            p = copia / 'x86_a.txt'
            exigir(origem in p.read_text(), f'{nome}: mutação não encontrou alvo')
            p.write_text(p.read_text().replace(origem, novo))
            try:
                conferir(caso, copia)
            except ValueError as e:
                (saida / f'{nome}.txt').write_text(str(e) + '\n')
                print(f'negativo {nome}: recusado ({e})', flush=True)
            else:
                raise ValueError(f'negativo {nome} aceito')


def tempos(arquivo):
    linhas = [l.split('\t') for l in arquivo.read_text().splitlines()]
    resultado = {}
    for (caso, rede, inicio), (seguinte, _, fim) in zip(linhas, linhas[1:]):
        if caso != seguinte or rede == 'fim': continue
        segundos = float(fim) - float(inicio)
        exigir(segundos >= 0, 'relógio voltou no tempo')
        resultado[rede] = resultado.get(rede, 0) + segundos
    exigir(linhas and linhas[-1][1] == 'fim', 'medição incompleta')
    return resultado


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--caso', choices=['f7_missao', 'f7_missao29', 'historia_novo', 'f7_transformacao'], required=True)
    p.add_argument('--pasta', type=Path, required=True)
    p.add_argument('--saida', type=Path, required=True)
    p.add_argument('--negativos', type=Path)
    p.add_argument('--tempos', type=Path)
    a = p.parse_args()
    try:
        destino_externo(a.saida)
        r = conferir(a.caso, a.pasta)
        if a.tempos: r['segundos_por_rede'] = tempos(a.tempos)
        if a.negativos:
            destino_externo(a.negativos)
            negativos(a.caso, a.pasta, a.negativos)
        a.saida.parent.mkdir(exist_ok=True, parents=True)
        a.saida.write_text(json.dumps(r, ensure_ascii=False, indent=2) + '\n')
        print('piloto: tudo certo')
        return 0
    except (ValueError, OSError, StopIteration, IndexError) as e:
        print(f'FALHA: {e}')
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
