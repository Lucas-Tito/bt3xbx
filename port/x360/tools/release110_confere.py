#!/usr/bin/env python3
"""Comparações da 1.1.0 contra bases aprovadas, com negativos em cópias privadas."""
from pathlib import Path
import argparse
import hashlib
import re
import subprocess
import tempfile

TEXT = '94fe2737c0b9f6663ca659685be77a7cd1d36659'


def exige(condicao, motivo):
    if not condicao:
        raise ValueError(motivo)


def arquivos(a, b):
    aa = {p.name: p for p in a.glob('*.ppm')}
    bb = {p.name: p for p in b.glob('*.ppm')}
    exige(aa and aa.keys() == bb.keys(), 'conjunto de imagens ausente ou diferente')
    for n in aa:
        exige(aa[n].read_bytes() == bb[n].read_bytes(), 'imagem diferente: '+n)
    print(f'Imagens: {len(aa)} arquivos idênticos a {a}')


def estado(a, b):
    # Só eventos determinísticos, sem tempos de host nem origem do executável.
    def linhas(p):
        return [l for l in p.read_text(errors='replace').splitlines()
                if re.match(r'^roteiro: (?:\S+:\d+ |janela |resultado )', l)]
    aa, bb = linhas(a), linhas(b)
    exige(aa and any(' fim ' in l for l in aa), 'estado sem fim de roteiro')
    exige(aa == bb, 'estado ou ações da luta diferentes')
    print(f'Estado e ações: {len(aa)} linhas determinísticas idênticas a {a}')


def objetos(base, novo):
    def lista(p):
        return {str(q.relative_to(p)): q for sub in ('obj','obj_data','ligar')
                for q in (p/sub).glob('*.o')}
    aa, bb = lista(base), lista(novo)
    exige(len(aa)>=398 and aa.keys()==bb.keys(), 'lista de objetos diferente ou incompleta')
    iguais, dif = 0, []
    with tempfile.TemporaryDirectory(dir=Path.home()) as d:
        d = Path(d)
        for n in sorted(aa):
            for src, dest in ((aa[n],d/'a'),(bb[n],d/'b')):
                subprocess.run(['objcopy','--strip-debug',src,dest],check=True)
            if subprocess.run(['cmp','-s',d/'a',d/'b']).returncode:
                dif.append(n)
            else:
                iguais += 1
        exige(set(dif) <= {'obj/x360_passos.o'}, 'objetos fora do teste alterados: '+str(dif))
        for nome, pasta in [('base',base),('final',novo)]:
            subprocess.run(['objcopy','-O','binary','--only-section=.text',pasta/'bt3_x86',d/nome],check=True)
            sha = hashlib.sha1((d/nome).read_bytes()).hexdigest()
            exige(sha==TEXT, '.text '+nome+' diferente: '+sha)
            print('.text '+nome+': '+sha)
        subprocess.run(['cmp',d/'base',d/'final'],check=True)
    print(f'Objetos sem depuração: {iguais} iguais; diferentes {dif}; .text idêntica')


def negativos():
    with tempfile.TemporaryDirectory(dir=Path.home()) as d:
        p = Path(d)
        a, b = p/'a', p/'b'
        a.mkdir(); b.mkdir()
        for pasta in (a,b): (pasta/'quadro.ppm').write_bytes(b'P6\n1 1\n255\n\x01\x02\x03')
        arquivos(a,b)
        for nome in ('pixel','ausente','extra'):
            (b/'quadro.ppm').write_bytes((a/'quadro.ppm').read_bytes())
            if nome=='pixel': (b/'quadro.ppm').write_bytes(b'P6\n1 1\n255\n\x01\x02\x04')
            if nome=='ausente': (b/'quadro.ppm').unlink()
            if nome=='extra': (b/'extra.ppm').write_bytes(b'x')
            try:
                arquivos(a,b)
            except ValueError:
                print('Cópia de imagens recusada: '+nome)
            else:
                raise ValueError('negativo aceito: '+nome)
        (a/'estado.txt').write_text('roteiro: teste.txt:1 fim vblank 20 quadro 18\n')
        (b/'estado.txt').write_text((a/'estado.txt').read_text())
        estado(a/'estado.txt',b/'estado.txt')
        (b/'estado.txt').write_text('roteiro: teste.txt:1 fim vblank 21 quadro 18\n')
        try:
            estado(a/'estado.txt',b/'estado.txt')
        except ValueError:
            print('Cópia de estado recusada: vblank alterado')
        else:
            raise ValueError('estado adulterado aceito')
    print('Comparações da release e negativos: tudo certo')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('modo',choices=('imagens','estado','objetos','negativos'))
    ap.add_argument('pastas',nargs='*',type=Path)
    a = ap.parse_args()
    try:
        if a.modo=='negativos': negativos()
        else: globals()[a.modo](*a.pastas)
    except (OSError,ValueError,subprocess.CalledProcessError) as e:
        ap.exit(1,'FALHA: '+str(e)+'\n')


if __name__=='__main__':
    main()
