#!/usr/bin/env python3
"""Negativos da F7 em cópias privadas, sem emulador e sem alterar a rodada.

--coleta aponta ao instantâneo de f7_cobertura.py; --saida deve ser nova e fora do git.
As justificativas de laboratório ficam só nessa saída e nunca aprovam conteúdo.
"""
import argparse
import copy
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

from f7_cobertura import conferir, destino_externo
from f7_dados import Dados, exigir
import roteiros

FERRAMENTA = Path(__file__).with_name('f7_cobertura.py')


def executar(coleta, just, pasta, nome, esperado):
    log = pasta / f'{nome}.log'
    with log.open('w') as f:
        r = subprocess.run([sys.executable, str(FERRAMENTA), 'conferir', '--coleta', str(coleta),
                            '--justificativas', str(just), '--saida', str(pasta / f'{nome}.json')],
                           stdout=f, stderr=subprocess.STDOUT)
    exigir(r.returncode == esperado, f'{nome}: saída {r.returncode}, esperada {esperado}; veja {log}')
    print(f'{nome}: saída {r.returncode}, conforme', flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--coleta', type=Path, required=True)
    p.add_argument('--dados', type=Path, required=True)
    p.add_argument('--saida', type=Path, required=True)
    a = p.parse_args()
    try:
        destino_externo(a.saida)
        exigir(not a.saida.exists(), 'pasta de testes já existe')
        a.saida.mkdir(parents=True)
        vazio = a.saida / 'vazio.json'
        vazio.write_text(json.dumps(dict(chamadas={}, arquivos={})))
        rc, base = conferir(a.coleta, vazio)
        exigir(rc == 1 and base['ausentes']['chamadas'] and base['ausentes']['arquivos'], 'coleta precisa ter lacunas conhecidas')
        j = {g: {n: dict(motivo='somente controle de laboratório, lacuna continua aberta',
                         prova='instantâneo local e diferença nominal, sem execução nova', tratamento='pendente')
                 for n in base['ausentes'][g]} for g in ('chamadas', 'arquivos')}
        just = a.saida / 'justificativas-laboratorio.json'
        just.write_text(json.dumps(j))
        executar(a.coleta, just, a.saida, 'intacto-justificado', 0)
        prog = base['coleta']['prog']
        alvo = next(k for k in base['nomes_chamadas'] if k not in base['ausentes']['chamadas'])
        for nome in ('sem-chamada', 'sem-arquivo', 'sem-disco'):
            pasta = a.saida / nome
            shutil.copytree(a.coleta, pasta)
            for gdb in (pasta / 'roteiros').glob('*/gdb.txt'):
                linhas = gdb.read_text().splitlines(keepends=True)
                if nome == 'sem-chamada':
                    eventos = iter(roteiros.chamadas(gdb, prog))
                    novas = []
                    for l in linhas:
                        if re.fullmatch(r'UNPACK \d+ -?\d+ 0x[0-9A-Fa-f]+', l.strip()):
                            if next(eventos)[2] == alvo:
                                continue
                        novas.append(l)
                else:
                    novas = [l for l in linhas if l.strip() != 'LEU pzs3us1/00001.bin'] if nome == 'sem-arquivo' else linhas
                gdb.write_text(''.join(novas))
            if nome == 'sem-disco':
                disco = pasta / 'disco.log'
                disco.write_text(''.join(l for l in disco.read_text().splitlines(keepends=True)
                                         if not l.startswith('pzs3us1/00001.bin:')))
            executar(pasta, just, a.saida, nome, 1)
            resultado = json.loads((a.saida / f'{nome}.json').read_text())
            if nome == 'sem-chamada':
                exigir(alvo in resultado['sem_justificativa']['chamadas'], 'ausência da chamada não foi nomeada')
            elif nome == 'sem-arquivo':
                exigir('pzs3us1/00001.bin' in resultado['sem_justificativa']['arquivos'], 'ausência de LEU não foi nomeada')
            else:
                exigir('pzs3us1/00001.bin' in resultado['faltam_disco'], 'ausência de --disco não foi nomeada')
        alterado = copy.deepcopy(j)
        del alterado['chamadas'][base['ausentes']['chamadas'][0]]
        ausente = a.saida / 'justificativa-apagada.json'
        ausente.write_text(json.dumps(alterado))
        executar(a.coleta, ausente, a.saida, 'sem-justificativa', 1)
        exigir(a.dados.is_dir(), 'negativo do manifesto exige a árvore extraída')
        d = Dados(a.dados)
        # Só o arquivo alterado é copiado; os demais enlaces apontam aos originais somente para leitura.
        comum = bytearray(d.arquivo(3))
        pos = d.u32(comum, 4) + 10
        comum[pos:pos + 2] = b'\0\0'
        dados_ruins = a.saida / 'dados-traje-zero'
        pt = dados_ruins / 'pzs3us1'
        pt.mkdir(parents=True)
        for arq in (a.dados / 'pzs3us1').iterdir():
            if arq.name != '00003.bin':
                (pt / arq.name).symlink_to(arq.resolve())
        (pt / '00003.bin').write_bytes(comum)
        with (a.saida / 'traje-zero.log').open('w') as f:
            r = subprocess.run([sys.executable, str(Path(__file__).with_name('f7_manifesto.py')),
                                '--dados', str(dados_ruins), '--saida', str(a.saida / 'manifesto-invalido.jsonl')],
                               stdout=f, stderr=subprocess.STDOUT)
        exigir(r.returncode == 1 and 'trajes' in (a.saida / 'traje-zero.log').read_text(), 'manifesto não recusou traje zero pelo motivo esperado')
        print('manifesto com traje zero: saída 1, conforme', flush=True)
        # O cabeçalho BPE da ISO convertida é big-endian, o conteúdo expandido continua little-endian.
        dado = Dados.__new__(Dados)
        dado.ordem = '>'
        carga = struct.pack('<I', 0x01020304)
        fluxo = bytes([255, 128, 254, 0, len(carga)]) + carga
        bpe = struct.pack('>II', len(carga), len(fluxo)) + fluxo
        exigir(dado.u32(dado.bpe(bpe)) == 0x01020304, 'ordem do conteúdo BPE incorreta')
        try:
            dado.bpe(bpe[:-1])
        except ValueError:
            print('BPE truncado em cópia: recusado', flush=True)
        else:
            raise ValueError('BPE truncado aceito')
        print('tudo certo; justificativas de laboratório não são dispensa da cobertura real')
        return 0
    except (ValueError, OSError, KeyError) as e:
        print(f'ERRO: {e}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
