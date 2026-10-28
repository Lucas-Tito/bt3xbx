#!/usr/bin/env python3
"""Prova nativa do diagnóstico, sem jogo, XDK ou emulador; exige sabotagens."""
import argparse
import importlib.util
from pathlib import Path
import re
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]


def importa(nome):
    spec = importlib.util.spec_from_file_location(nome, Path(__file__).with_name(nome+'.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


PRE = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cassert>
struct LARGE_INTEGER { long long QuadPart; };
static LARGE_INTEGER sFreq = {1000};
static unsigned long long relogio;
static unsigned leituras, efeitos;
static void QueryPerformanceCounter(LARGE_INTEGER *t) { leituras++; t->QuadPart = relogio; }
static void Diario_Escrever(const char *f, ...) {
    va_list a; va_start(a, f); vprintf(f, a); va_end(a); puts("");
}
'''
POS = r'''
static void api() { efeitos++; relogio += 7; }
static void antecipado() {
    CadeiaEscopo ca(-1, CA_PREPARA);
    relogio += 5;
    CADEIA_CHAMADA(CA_ESTADO, api());
    return;
}
int main() {
    const char *desligados[] = {NULL, "0", "invalida", "6961,6959"};
    for (unsigned i=0; i<sizeof(desligados)/sizeof(*desligados); i++) {
        memset(&sCa, 0, sizeof(sCa)); leituras=0;
        if (desligados[i]) setenv("BT3_CADEIA_CPU", desligados[i], 1);
        else unsetenv("BT3_CADEIA_CPU");
        cadeia_inicio(6960);
        { CadeiaEscopo ca(CA_VU0, CA_RESTO); antecipado(); }
        cadeia_fim(0);
        assert(!sCa.ativo && leituras == 0 && efeitos == i+1);
    }
    memset(&sCa, 0, sizeof(sCa));
    setenv("BT3_CADEIA_CPU", "6960,6960", 1);
    cadeia_inicio(6959); assert(!sCa.ativo);
    relogio=0; cadeia_inicio(6960); assert(sCa.ativo);
    relogio+=2;
    { CadeiaEscopo ca(CA_VU0, CA_RESTO);
      relogio+=3; antecipado(); relogio+=11; }
    { CadeiaEscopo ca(CA_VU4, CA_PREPARA); relogio+=17;
      { CadeiaEscopo filho(-1, CA_PREPARA); relogio+=19; } }
    relogio+=13;
    cadeia_fim(0);
    assert(sCa.nivel == 0 && sCa.grupo == CA_QUADRO);
    assert(sCa.tiques[CA_QUADRO][CA_RESTO] == 15);
    assert(sCa.tiques[CA_VU0][CA_RESTO] == 14);
    assert(sCa.tiques[CA_VU0][CA_PREPARA] == 5);
    assert(sCa.tiques[CA_VU0][CA_ESTADO] == 7);
    assert(sCa.tiques[CA_VU4][CA_PREPARA] == 36);
    assert(sCa.chamadas[CA_VU4][CA_PREPARA] == 2 && efeitos == 5);
    cadeia_inicio(6961); assert(!sCa.ativo);
    memset(&sCa, 0, sizeof(sCa)); setenv("BT3_CADEIA_CPU", "1", 1);
    cadeia_inicio(6959); assert(!sCa.ativo);
    cadeia_inicio(6960); assert(sCa.ativo); cadeia_fim(1);
    puts("roteiro: f5_duelo.txt:99 fim vblank 12570 quadro 8378");
}
'''


def tira_marcas(s):
    s = s.replace('#include "cadeia_cpu.h"\n', '')
    s = re.sub(r'^    CadeiaEscopo ca\([^\n]+\);\n', '', s, flags=re.M)
    s = re.sub(r'^    cadeia_(inicio|fim)\([^\n]+\);\n', '', s, flags=re.M)
    # A expressão inteira volta, com seus argumentos e efeitos, sem reordená-la.
    s = re.sub(r'CADEIA_CHAMADA\(CA_\w+, (sDev->[\s\S]*?);',
               lambda m: m[1][:-1]+';', s)
    return s


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--saida', type=Path, required=True)
    p.add_argument('--base', default='bbc07118')
    a = p.parse_args()
    a.saida.mkdir(parents=True, exist_ok=True)
    cab = (RAIZ/'port/x360/src/gs/cadeia_cpu.h').read_text()
    mutantes = {
        'soma': ('sCa.tiques[grupo][etapa] +=', 'sCa.tiques[grupo][etapa] ='),
        'familia': ('g < 0 ? grupo : g', 'g < 0 ? CA_QUADRO : g'),
        'retorno': ('sCa.grupo = grupo; sCa.etapa = etapa;', 'sCa.grupo = CA_QUADRO; sCa.etapa = etapa;'),
        'faixa': ('quadro <= sCa.b', 'quadro < sCa.b'),
        'desligada': ('if (!sCa.ativo) return;', '(void)cadeia_tique(); if (!sCa.ativo) return;'),
    }
    for nome, troca in [('boa', None), *mutantes.items()]:
        h = cab
        if troca:
            assert troca[0] in h
            h = h.replace(*troca)
        cpp = a.saida/(nome+'.cpp'); exe = a.saida/nome
        cpp.write_text(PRE+h+POS)
        subprocess.run(['g++', '-std=c++03', '-Wall', '-Wextra', '-Werror', '-O2', str(cpp), '-o', str(exe)], check=True)
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        (a.saida/(nome+'.log')).write_text(r.stdout+r.stderr)
        assert (r.returncode != 0) == bool(troca), (nome, r.returncode)
        print(nome, 'sabotagem acusada' if troca else 'tudo certo', r.returncode)
    fonte = RAIZ/'port/x360/src/gs/gs_d3d.cpp'
    base = subprocess.check_output(['git', 'show', a.base+':port/x360/src/gs/gs_d3d.cpp'], cwd=RAIZ, text=True)
    atual = fonte.read_text()
    assert tira_marcas(atual) == base, 'as chamadas originais mudaram'
    ruim = atual.replace('sDev->DrawIndexedVertices(D3DPT_TRIANGLELIST, 0, ini, d->count)',
                         'sDev->DrawIndexedVertices(D3DPT_TRIANGLELIST, 0, ini, 0)', 1)
    assert ruim != atual and tira_marcas(ruim) != base
    print('fonte: chamadas, argumentos e ordem originais iguais; chamada adulterada recusada')
    leitor = importa('cadeia_cpu')
    bom = (a.saida/'boa.log').read_text()
    # A segunda amostra é deliberadamente uma captura inválida.
    invalida = 'cadeia: cpu quadro 6960 indisponivel\n'
    assert invalida in bom
    try:
        leitor.confere(bom, [6960])
    except ValueError:
        print('leitor: captura recusada')
    else:
        raise AssertionError('captura aceita')
    bom = bom.replace(invalida, '')
    assert leitor.confere(bom, [6960])[6960]['total'] == 77
    (a.saida/'rateio-bom.log').write_text(bom)
    negativos = {
        'fim': bom.replace('f5_duelo.txt:99 fim', 'f5_duelo.txt:99 incompleto'),
        'total': bom.replace('total 77.000000', 'total 78.000000'),
        'parcela': re.sub(r'^cadeia: cpu .*grupo vu0 etapa estado .*\n', '', bom, flags=re.M),
        'quadro': bom.replace('quadro 6960', 'quadro 6959'),
        'classe': bom.replace('grupo vu0', 'grupo vu7'),
        'repetida': bom.replace('soma 77.000000 ms', 'soma 77.000000 ms\ncadeia: cpu quadro 6960 soma 77.000000 ms'),
    }
    for nome, t in negativos.items():
        assert t != bom
        arq = a.saida/('leitor-'+nome+'.log'); arq.write_text(t)
        r = subprocess.run(['python3', str(Path(__file__).with_name('cadeia_cpu.py')), str(arq), '--quadros', '6960'], capture_output=True, text=True)
        assert r.returncode == 1, (nome, r.stdout, r.stderr)
        (a.saida/('leitor-'+nome+'.saida')).write_text(r.stdout+r.stderr)
        print('leitor', nome, 'saída 1')
    gerador = importa('reprojeto_console')
    with tempfile.TemporaryDirectory(prefix='bt3-ambientes-') as d:
        pasta = Path(d)/'nova'
        assert gerador.prepara(pasta, 'prova32') == 34
        saves = [next(l for l in arq.read_text().splitlines() if l.startswith('BT3_SAVES=')) for arq in pasta.glob('ambiente_*.txt')]
        assert len(saves) == len(set(saves)) == 34
        for rodada, destino in [('prova32', pasta), ('nome_invalido', Path(d)/'outra')]:
            try: gerador.prepara(destino, rodada)
            except (ValueError, FileExistsError): print('ambientes: entrada recusada', rodada)
            else: raise AssertionError('sobrescrita ou identificador inválido aceito')
    print('tudo certo')


if __name__ == '__main__':
    main()
