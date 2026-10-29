#!/usr/bin/env python3
"""Inventário das chaves do título, padrões explícitos e ambientes de teste; acusa deriva e sabotagens."""
import argparse
import importlib.util
import pathlib
import re
import shutil
import subprocess
import tempfile

RAIZ=pathlib.Path(__file__).resolve().parents[3]
TABELA=pathlib.Path('docs/x360/plano/F8/04-chaves.tsv')


def levantar(raiz):
    spec=importlib.util.spec_from_file_location('c89',RAIZ/'port/x360/tools/c89.py')
    c89=importlib.util.module_from_spec(spec);spec.loader.exec_module(c89)
    nomes={str(p.relative_to(RAIZ)) for _,p,_,_ in c89.grupos()}
    for pasta in ('f2','gs','som'):
        nomes.update(str(p.relative_to(raiz)) for p in (raiz/'port/x360/src'/pasta).glob('*') if p.suffix in ('.c','.cpp','.h'))
    nomes.add('port/x360/src/iso360.c')
    # Cabeçalhos de diagnóstico incluídos pelos fontes selecionados.
    nomes.update(('port/x360/src/diagnostico360.h','port/x360/src/gs/cadeia_cpu.h','port/x360/src/gs/cortes_impl.h'))
    chaves={}
    for nome in sorted(nomes):
        t=(raiz/nome).read_text()
        for m in re.finditer(r'\b(?:getenv|env_num|definir)\(\s*"(BT3_\w+)"',t):
            chaves.setdefault(m[1],set()).add(nome)
    # plat_extras.c recebe estes nomes pelo argumento env.
    for nome in ('BT3_SONGS','BT3_STAGES'):chaves[nome]={'port/src/plat_extras.c'}
    diag=set(re.findall(r'"(BT3_\w+)"',(raiz/'port/x360/src/diagnostico360.h').read_text()))
    padroes=dict(re.findall(r'definir\("(BT3_\w+)", "([^"\n]*)"\)',(raiz/'port/x360/src/f2/posix360.c').read_text()))
    linhas=['chave\tclasse\ttabela_do_360\tconsumidores']
    for chave,origens in sorted(chaves.items()):
        classe='diagnóstico (exige BT3_LOG=1)' if chave in diag else 'configuração ou teste explícito'
        if chave=='BT3_LOG':classe='liga o diagnóstico somente com valor 1'
        linhas.append('\t'.join((chave,classe,padroes.get(chave,'ausente; padrão local no consumidor'),','.join(sorted(origens)))))
    return '\n'.join(linhas)+'\n',nomes


def ambientes(raiz):
    return [p for pasta in ('docs/x360/plano','port/x360/ambientes') for p in (raiz/pasta).rglob('*.txt')
            if ('/ambientes/' in str(p) or '-ambientes/' in str(p)) and 'BT3_' in p.read_text()]


def conferir(raiz):
    texto,_=levantar(raiz)
    if (raiz/TABELA).read_text()!=texto:raise ValueError('inventário de chaves divergiu')
    for p in ambientes(raiz):
        if 'BT3_LOG=1' not in p.read_text().splitlines():raise ValueError('ambiente sem BT3_LOG=1: '+str(p))
    print(f'inventário: {len(texto.splitlines())-1} chaves; {len(ambientes(raiz))} ambientes com log explícito')


def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--atualizar',action='store_true');ap.add_argument('--mutantes',action='store_true');a=ap.parse_args()
    texto,nomes=levantar(RAIZ)
    if a.atualizar:(RAIZ/TABELA).write_text(texto)
    conferir(RAIZ)
    with tempfile.TemporaryDirectory(prefix='audita-logs-') as d:
        p=pathlib.Path(d)
        # Rodar cada gerador e conferir todos os ambientes realmente emitidos.
        for nome in ('comum_console.py','cortes_console.py','reprojeto_console.py'):
            saida=p/nome
            subprocess.run(['python3',str(RAIZ/'port/x360/tools'/nome),str(saida),'--rodada','logs360'],check=True,capture_output=True)
            gerados=list(saida.glob('ambiente*.txt'))
            if not gerados or any('BT3_LOG=1' not in f.read_text().splitlines() for f in gerados):raise ValueError('gerador sem log: '+nome)
            print(f'gerador {nome}: {len(gerados)} ambientes conferidos')
        if a.mutantes:
            copia=p/'copia'
            for nome in nomes|{str(TABELA)}:
                dest=copia/nome;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(RAIZ/nome,dest)
            amb=copia/'port/x360/ambientes/teste/ambiente.txt';amb.parent.mkdir(parents=True);amb.write_text('BT3_GS=gpu\n')
            try:conferir(copia)
            except ValueError as e:print('mutante ambiente sem log: acusado:',e)
            else:raise ValueError('mutante ambiente não acusado')
            amb.write_text('BT3_LOG=1\n')
            fonte=copia/'port/x360/src/f2/posix360.c';fonte.write_text(fonte.read_text()+'\nvoid nova(void) { getenv("BT3_DIAGNOSTICO_NOVO"); }\n')
            try:conferir(copia)
            except ValueError as e:print('mutante chave nova: acusado:',e)
            else:raise ValueError('mutante chave nova não acusado')
    print('varredura de chaves: tudo certo')


if __name__=='__main__':
    main()
