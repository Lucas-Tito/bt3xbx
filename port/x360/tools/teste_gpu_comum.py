#!/usr/bin/env python3
"""Prova o perfil real com dispositivo simulado e controles negativos obrigatórios."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
s = (RAIZ / 'port/x360/src/gs/gs_d3d.cpp').read_text()
a = s.index('enum {\n    T_OUTROS, T_COMUM')
b = s.index('\nstatic void ret_une(', a)
perfil = s[a:b]
a = s.index('static int comum_classe(')
b = s.index('\nstatic void desenha_comum(', a)
comum = s[a:b]
pre = r'''
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <string>
#define __int64 long long
#define WINAPI
#define D3DCALLBACK_IDLE 1
#define D3DCALLBACK_CPU1 2
typedef unsigned DWORD;
static unsigned long long relogio;
static unsigned long long __mftb() { return ++relogio; }
static void __lwsync() {}
static int Passos_Medindo() { return 1; }
static std::string log;
static void Diario_Escrever(const char *f, ...) {
 char buf[2048]; va_list a; va_start(a,f); vsnprintf(buf,sizeof(buf),f,a); va_end(a); log += buf; log += '\n';
}
struct Quadro { unsigned quadro; } q;
static Quadro *sQ = &q;
struct Dispositivo {
 void InsertCallback(int, void (*f)(DWORD), DWORD i) { f(i); }
 DWORD InsertFence() { return 1; }
 int IsFencePending(DWORD) { return 0; }
 void BlockOnFence(DWORD) { assert(false); }
} dev, *sDev = &dev;
struct GsDraw { int vu, target, mode[4]; float misc[4]; };
static int afiado_usa(const GsDraw *, int) { return 0; }
static unsigned sOtim = 0, sPsTrivial = 0;
#define OT_PS 1
'''
main = r'''
int main() {
 GsDraw d = {}; d.mode[0]=2; d.target=3;
 assert(comum_classe(&d,0,1,0)==T_C_ALVO);
 assert(comum_classe(&d,1,1,0)==T_C_AF);
 assert(comum_classe(&d,1,1,1)==T_C_TRIV);
 d.mode[0]=0; assert(comum_classe(&d,0,1,0)==T_C_RAP_SEM);
 d.mode[0]=1; assert(comum_classe(&d,0,1,0)==T_C_RAP_TEX);
 d.misc[1]=1; assert(comum_classe(&d,0,1,0)==T_C_VRAM);
 d.misc[1]=0; d.mode[0]=3; assert(comum_classe(&d,0,0,0)==T_C_PACOTE);
 d.mode[0]=2;
 sTempo.ligado=sCortesGpu=sComumGpu=1; sTempo.freq=1;
 sCortesGpuA=sCortesGpuB=6960;
 for(q.quadro=6959;q.quadro<=6962;q.quadro++) {
  tempo_comeca(); tempo_abre();
  if(q.quadro==6960) {
   sComumDesenho=110; comum_poe(&d); unsigned n=sTempo.n[sTempo.buf];
   sComumDesenho=111; comum_poe(&d); assert(sTempo.n[sTempo.buf]==n);
   d.target=4; sComumDesenho=112; comum_poe(&d); assert(sTempo.n[sTempo.buf]==++n);
   sComumDesenho=128; comum_poe(&d); assert(sTempo.n[sTempo.buf]==++n);
   sComumApi[C_RESOLVE]=7;
   d.vu=1; comum_poe(&d); assert(sTempo.cpu==T_VU0); d.vu=0;
  }
  tempo_fecha_gpu(0); tempo_fecha_cpu(20,1);
 }
 assert(log.find("comum quadro 6960 classe comum alvo alvo 3 desenhos 110:111")!=std::string::npos);
 assert(log.find("comum quadro 6960 classe comum alvo alvo 4 desenhos 112:112")!=std::string::npos);
 assert(log.find("comum quadro 6960 classe comum alvo alvo 4 desenhos 128:128")!=std::string::npos);
 assert(log.find("quadro 6962")==std::string::npos);
 assert(log.find("API Resolves 7 Clear 0")!=std::string::npos);
 // O trecho vazio custa 3. Todos os trechos e o total conservam bruto-custo=líquido.
 unsigned b=sTempo.buf; unsigned long long bruto=0; unsigned marcas=0;
 for(int c=0;c<T_GPU_N;c++){ bruto+=sTempo.gSoma[c]; marcas+=sTempo.gTrechos[c]; }
 assert(bruto==sTempo.quadroSoma && marcas==6 && sTempo.lidos==1);
 // Desligada, mantém a classe antiga, sem marcas por alvo ou faixa.
 sComumGpu=0; q.quadro=6960; tempo_comeca(); tempo_abre(); comum_poe(&d);
 assert(sTempo.cpu==T_COMUM);
 puts("tudo certo: seleção, shader, alvo, faixa, VU, dreno, calibração e conservação");
}
'''
with tempfile.TemporaryDirectory(prefix='bt3-gpu-comum-') as td:
    td = Path(td)
    casos = [
        ('original', perfil, comum, False),
        ('fonte', perfil, comum.replace('d->mode[0] == 2 ? T_C_ALVO', 'd->mode[0] == 2 ? T_C_VRAM'), True),
        ('alvo', perfil, comum.replace('sComumAlvo[i] != d->target', '0'), True),
        ('faixa', perfil, comum.replace('sComumIni[i] / 128 != sComumDesenho / 128', '0'), True),
        ('quadro', perfil.replace('sCortesGpuQ[b]', 'sQ->quadro'), comum, True),
        ('calibracao', perfil.replace('((double)t - vazio) / f', '(double)t / f'), comum, True),
    ]
    # O controle da calibração verifica a linha detalhada, além da soma global.
    main = main.replace('assert(log.find("quadro 6962")', 'assert(log.find("desenhos 128:128 bruto 4.000000 liquido 1.000000")!=std::string::npos);\n assert(log.find("quadro 6962")')
    for nome, p, c, negativo in casos:
        fonte = td / (nome+'.cpp'); exe = td / nome
        fonte.write_text(pre+p+c+main)
        subprocess.run(['g++', '-O2', str(fonte), '-o', str(exe)], check=True)
        r = subprocess.run([str(exe)], capture_output=True, text=True)
        if (r.returncode != 0) != negativo:
            print(r.stdout+r.stderr)
            raise SystemExit(f'FALHA {nome}: {r.returncode}')
        print(nome, 'mutante recusado' if negativo else r.stdout.strip(), r.returncode)

spec = importlib.util.spec_from_file_location('censo', RAIZ/'port/x360/tools/gpu_comum.py')
censo = importlib.util.module_from_spec(spec); spec.loader.exec_module(censo)
# Dois retângulos sobrepostos de 8x4, deslocados 4 pixels: 64 fragmentos, união 48.
d = dict(n=0, native=0, vu=0, alvo=0, pipe=0xf0400, tex=0, sampler=0, count=6,
         modo=[0,0,0,0], misc=[0,0,0,0], tesoura=[0,0,640,448], blendc=0, rect=[0,0,1,1], xy=[[0,0],[8,0],[0,4],[8,0],[8,4],[0,4]])
e = dict(d, n=1, xy=[[x+4,y] for x,y in d['xy']])
h = dict(quadro=6960, desenhos=2, alvos=[0], fbp=0, escala=1)
r = censo.censo(h, [d,e], 1,1)
assert (r['fragmentos'], r['uniao'], r['sobreposicao']) == (64,48,16)
assert sum(g['escritas_min'] for g in r['grupos'].values()) == 64
with tempfile.TemporaryDirectory(prefix='bt3-censo-') as td:
    import copy, json
    p = Path(td)/'6960.jsonl'
    for nome in ['original','desenho','vertices','ordem','alvo']:
        hs, ds = copy.deepcopy(h), copy.deepcopy([d,e])
        if nome=='desenho': ds.pop()
        if nome=='vertices': ds[0]['xy'].pop()
        if nome=='ordem': ds.reverse()
        if nome=='alvo': ds[0]['alvo']=8
        p.write_text('\n'.join(json.dumps(x) for x in [hs]+ds)+'\n')
        try:
            censo.le(p)
        except ValueError:
            assert nome!='original'; print('censo',nome,'cópia recusada')
        else:
            assert nome=='original'
print('tudo certo')
