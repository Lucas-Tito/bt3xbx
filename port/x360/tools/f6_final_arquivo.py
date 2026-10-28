#!/usr/bin/env python3
"""Confere a retomada e conserva só relatórios textuais dos casos já julgados."""
import hashlib
import json
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import threading


def atomico(p, valor):
    tmp = p.with_suffix(p.suffix + '.novo')
    tmp.write_text(json.dumps(valor, indent=2) + '\n')
    tmp.replace(p)


def conferir(p, commit, base):
    meta = json.loads((p / 'origem.json').read_text())
    if meta['commit'] != commit or meta['base'] != base:
        raise ValueError('origem diferente da solicitada')
    casos = json.loads((p / 'casos.json').read_text())
    nomes = set()
    for c in casos:
        nome = c['nome']
        if nome in nomes or '/' in nome or not nome:
            raise ValueError('caso duplicado ou nome inválido')
        nomes.add(nome)
        log = p / (nome + '.log')
        if not log.is_file() or hashlib.sha256(log.read_bytes()).hexdigest() != c['sha256']:
            raise ValueError('log alterado ou ausente: ' + nome)
    print(f'retomada: {len(casos)} casos íntegros, inclusive os reprovados')
    return casos


def compactar(p, nome):
    # Não seguir os links de dados. O manifesto descreve o descarte, não substitui julgamento.
    d = p / 'roteiros' / nome.removeprefix('roteiro-')
    if not d.is_dir():
        return
    manifesto = d / 'retencao.json'
    if manifesto.exists():
        return
    mantidos, removidos = [], []
    def visita(raiz):
        for f in sorted(raiz.iterdir()):
            rel = str(f.relative_to(d))
            if f.is_symlink():
                removidos.append(dict(caminho=rel, link=True, bytes=0))
                f.unlink()
            elif f.is_dir():
                visita(f)
                if not any(f.iterdir()):
                    f.rmdir()
            elif f.suffix in ('.txt', '.log', '.telas', '.codigo', '.md', '.json'):
                mantidos.append(dict(caminho=rel, bytes=f.stat().st_size,
                                     sha256=hashlib.sha256(f.read_bytes()).hexdigest()))
            else:
                removidos.append(dict(caminho=rel, bytes=f.stat().st_size))
                f.unlink()
    visita(d)
    atomico(manifesto, dict(caso=nome, mantidos=mantidos, removidos=removidos))
    print(f'retenção {nome}: removidos {sum(f["bytes"] for f in removidos)} bytes; '
          f'conservados {sum(f["bytes"] for f in mantidos)} bytes de relatórios')


def limpar(p, nome):
    if nome.startswith('roteiro-'):
        compactar(p, nome)
    elif nome == 'leitor-iso':
        shutil.rmtree(p / 'final/port/build/iso_ref', ignore_errors=True)
        shutil.rmtree(p / 'iso', ignore_errors=True)


def medir(p):
    parar = threading.Event()
    signal.signal(signal.SIGTERM, lambda *_: parar.set())
    arquivo = p / 'espaco.json'
    valores = json.loads(arquivo.read_text()) if arquivo.exists() else dict(pico_kib=0, amostras=0)
    while True:
        kib = int(subprocess.check_output(['du', '-sk', str(p)]).split()[0])
        valores.update(pico_kib=max(kib, valores['pico_kib']), ultimo_kib=kib,
                       amostras=valores['amostras'] + 1)
        atomico(arquivo, valores)
        if parar.wait(20):
            break


if __name__ == '__main__':
    try:
        modo, pasta, *args = sys.argv[1:]
        p = Path(pasta)
        if modo == 'conferir':
            conferir(p, *args)
        elif modo == 'limpar':
            limpar(p, args[0])
        elif modo == 'limpar-julgados':
            for c in json.loads((p / 'casos.json').read_text()):
                limpar(p, c['nome'])
        elif modo == 'medir':
            medir(p)
        else:
            raise ValueError('modo desconhecido')
    except (OSError, ValueError, KeyError) as e:
        print('FALHA: ' + str(e))
        sys.exit(1)
