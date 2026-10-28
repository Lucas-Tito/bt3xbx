#!/usr/bin/env python3
"""F5, seção 38: prova nativa do relógio da gravação, sem jogo, XDK ou emulador; exige sabotagens.

Tira do gs_draw.c o GS_RELOGIO/gs_relogio e do gs_d3d.cpp o bloco do relógio (Relogio_Ligado, Relogio_Gravou,
relogio_escreve), compila os dois com g++ e um mftb de mentira, grava quadros sintéticos (anexos, um descarte no meio,
quadros fora da lista, vagas cheias) e passa o diário pelo leitor do prefixos_teto.py (relogios), que tem de devolver
os mesmos carimbos. Cada sabotagem do código tem de ser acusada (pelo programa ou pelo leitor).

    teste_relogio.py --saida PASTA
"""
import argparse
import os
import importlib.util
from pathlib import Path
import re
import subprocess

RAIZ = Path(__file__).resolve().parents[3]

PRE = r'''
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cassert>
#include <stdint.h>
#define __int64 long long
static unsigned long long relogio;
static unsigned long long __mftb() { return relogio; }
static void __lwsync() {}
static void Diario_Escrever(const char *f, ...) {
    va_list a; va_start(a, f); vprintf(f, a); va_end(a); putchar('\n');
}
#define PICOS_N 16
struct PicoJ { unsigned q; unsigned long long tb; };
struct PicoG { unsigned q, qJogo; unsigned long long tb; };
struct PicoD { unsigned q; unsigned long long marca; };
static PicoJ sPJ[PICOS_N];
static PicoG sPG[PICOS_N];
static PicoD sPD[PICOS_N];
static volatile unsigned long long sPicoGpu[PICOS_N];
static unsigned gPicos = 36;
static double sPicoTbMs = 50000.0; /* 50 tiques por us */
#define MAX_DRAWS 16384
static unsigned gsDrawCount;
'''
POS = r'''
/* um quadro: o jogo entrega em J, a gravação anexa n desenhos com passos conhecidos e entrega em G */
static void quadro(unsigned q, unsigned n, unsigned long long J, int descarte, int gpu) {
    unsigned i;
    sPJ[q % PICOS_N].q = q; sPJ[q % PICOS_N].tb = J;
    relogio = J + 50 * 300; /* o primeiro desenho anexado 300 us depois de J */
    for (i = 0; i < n; i++) {
        if (descarte && i == n / 2) { gsDrawCount = 0; } /* (um descarte fora da entrega: recomeça) */
        gsDrawCount++;
        GS_RELOGIO();
        relogio += 50 * (10 + (i * 7) % 23);
    }
    sPG[q % PICOS_N].q = q; sPG[q % PICOS_N].qJogo = q; sPG[q % PICOS_N].tb = relogio;
    relogio += 50 * 7; /* (o Picos_Gravou carimbou G um pouco antes) */
    if (sRelLiga < 0) sRelLiga = Relogio_Ligado();
    else if (sRelLiga > 0) Relogio_Gravou(q, gsDrawCount, sRelN, sRelT, sRelPrimeiro);
    sRelN = 0; sRelPrimeiro = 0; gsDrawCount = 0;
    sPD[q % PICOS_N].q = q; sPD[q % PICOS_N].marca = sPG[q % PICOS_N].tb + 50 * 500; /* P: 0,5 ms depois de G */
    sPicoGpu[q % PICOS_N] = gpu ? sPG[q % PICOS_N].tb + 50 * 8500 : 0;         /* U: 8,5 ms depois de G */
}
int main() {
    unsigned k;
    setenv("BT3_RELOGIO_GRAV", "6930,6960,6961,6963,6964", 1);
    quadro(6900, 50, 1000000, 0, 1);  /* (o primeiro só liga) */
    assert(sRelLiga == (getenv("BT3_RELOGIO_MARCAS") ? 1 : 2));
    quadro(6929, 213, 2000000, 0, 1); /* fora da lista: nada */
    quadro(6930, 213, 3000000, 0, 1);
    quadro(6960, 1408, 4000000, 0, 1);
    quadro(6961, 40, 9000000, 1, 1); /* (com um descarte no meio: o relógio recomeça junto com a lista) */
    quadro(6963, 40, 10000000, 0, 0); /* (a GPU ainda não chegou à marca: o fim espera) */
    quadro(6964, 40, 11000000, 0, 1); /* a quinta vaga cheia: perdida e contada */
    for (k = 0; k < 12; k++) relogio_escreve(6965);
    sPicoGpu[6963 % PICOS_N] = sPG[6963 % PICOS_N].tb + 50 * 8500; /* (agora chegou) */
    for (k = 0; k < 4; k++) relogio_escreve(6970);
    puts("fim do teste");
    return 0;
}
'''


def importa(nome):
    spec = importlib.util.spec_from_file_location(nome, Path(__file__).with_name(nome + '.py'))
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def extrai():
    draw = (RAIZ / 'port/src/gs/gs_draw.c').read_text()
    m = re.search(r'#if defined\(PORT_X360\) && defined\(BT3_PERFIL_CADEIA\)\n(/\* \(F5, seção 38[\s\S]*?)#else\n#define GS_RELOGIO\(\)', draw)
    assert m, 'bloco do relógio no gs_draw.c'
    d3d = (RAIZ / 'port/x360/src/gs/gs_d3d.cpp').read_text()
    n = re.search(r'#ifdef BT3_PERFIL_CADEIA\n(/\* ---- o relógio da gravação[\s\S]*?)#define RELOGIO_ESCREVE\(q\) relogio_escreve\(q\)\n', d3d)
    assert n, 'bloco do relógio no gs_d3d.cpp'
    corpo = m[1].replace('void Relogio_Gravou(', 'extern "C" void Relogio_Gravou(').replace('int Relogio_Ligado(', 'extern "C" int Relogio_Ligado(')
    return corpo, n[1]


def esperado(q, n, J, descarte=False):
    """os carimbos em us desde J que o quadro sintético tem de dar (o i pronto quando o i+1 foi anexado)"""
    t = J + 50 * 300; anexo = []
    ini = 0
    for i in range(n):
        if descarte and i == n // 2:
            ini = i
        anexo.append(t)
        t += 50 * (10 + (i * 7) % 23)
    return [round((x - J) / 50) for x in anexo[ini + 1:]], round((t - J) / 50) / 1000


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--saida', type=Path, required=True)
    a = p.parse_args()
    a.saida.mkdir(parents=True, exist_ok=True)
    teto = importa('prefixos_teto')
    draw, d3d = extrai()
    mutantes = {
        'fecha um a mais': (draw, 'while (sRelN + 1 < gsDrawCount)', 'while (sRelN < gsDrawCount)'),
        'sem recomeço': (draw, 'if (sRelN >= gsDrawCount) {', 'if (0) {'),
        'J trocado': (d3d, 'v->j = j->tb;', 'v->j = j->tb + 50;'),
        'G trocado': (d3d, 'v->g = g->tb;', 'v->g = __mftb();'),
        'linha pulada': (d3d, 'v->escritos = fim;', 'v->escritos = fim + 1;'),
        'vaga sem fim': (d3d, 'v->estado = 0;\n    }\n}', 'v->estado = 1;\n    }\n}'),
        'quadro fora da lista': (d3d, 'if (i == sRelQN) {\n        return;', 'if (0) {\n        return;'),
        'U da marca': (d3d, 'temU ? (double)(__int64)(sPicoGpu[kq] - v->j)', 'temU ? (double)(__int64)(sPD[kq].marca - v->j)'),
        'fim sem esperar a GPU': (d3d, 'if (!temU && quadro < v->quadro + 6) {', 'if (0) {'),
        'carimbos sem as marcas': (draw, 'if (sRelLiga > 1) gs_relogio();', 'if (sRelLiga > 0) gs_relogio();'),
    }
    for nome, troca in [('boa', None), *mutantes.items()]:
        dr, dd = draw, d3d
        if troca:
            alvo, de, para = troca
            assert alvo.count(de) == 1, (nome, de)
            if alvo is draw:
                dr = draw.replace(de, para)
            else:
                dd = d3d.replace(de, para)
        cpp = a.saida / (re.sub(r'\W', '_', nome) + '.cpp'); exe = cpp.with_suffix('')
        cpp.write_text(PRE + dr + dd + POS)
        subprocess.run(['g++', '-std=c++03', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function', '-O2', str(cpp), '-o', str(exe)], check=True)
        erro = None
        for marcas in (1, 0):
            amb = dict(os.environ)
            amb.pop('BT3_RELOGIO_MARCAS', None)
            if not marcas:
                amb['BT3_RELOGIO_MARCAS'] = '0'
            r = subprocess.run([str(exe)], capture_output=True, text=True, env=amb)
            cpp.with_suffix(f'.m{marcas}.log').write_text(r.stdout + r.stderr)
            try:
                assert r.returncode == 0, f'programa saiu com {r.returncode}'
                con = teto.relogios(r.stdout)
                assert sorted(con) == [6930, 6960, 6961, 6963], f'quadros {sorted(con)}'
                for q, n, J, desc in ((6930, 213, 3000000, 0), (6960, 1408, 4000000, 0), (6961, 40, 9000000, 1), (6963, 40, 10000000, 0)):
                    us, g = esperado(q, n, J, desc)
                    assert con[q]['us'] == (us if marcas else None), f'carimbos do {q} (marcas {marcas})'
                    assert abs(con[q]['G'] - g) < 1e-6, f'G do {q}'
                    assert abs(con[q]['P'] - g - 0.5) < 2e-3 and abs(con[q]['U'] - g - 8.5) < 2e-3, f'P/U do {q}'
                    assert not marcas or desc or abs(con[q]['primeiro'] - 0.3) < 1e-6, f'primeiro do {q}'
                assert '1 vagas perdidas' in r.stdout, 'a vaga perdida não foi contada'
                assert 'relogio: quadro 6929' not in r.stdout and 'relogio: quadro 6900' not in r.stdout, 'quadro fora da lista'
            except AssertionError as e:
                erro = erro or str(e)
        if troca:
            assert erro, f'sabotagem aceita: {nome}'
            print(f'sabotagem acusada: {nome} ({erro})')
        else:
            assert not erro, erro
            print('tudo certo: o relógio da gravação lido igual ao gravado, quadros fora da lista ignorados, vaga perdida contada')
    print('tudo certo no teste do relógio')


if __name__ == '__main__':
    main()
