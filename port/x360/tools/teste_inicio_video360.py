#!/usr/bin/env python3
"""Exercita gs_on e a saída por falha reais com APIs simuladas, sem XDK."""
from pathlib import Path
import subprocess
import tempfile
import resource

RAIZ = Path(__file__).resolve().parents[3]


def funcao(texto, assinatura):
    a = texto.index(assinatura)
    b = texto.index('{', a) + 1
    nivel = 1
    while nivel:
        nivel += (texto[b] == '{') - (texto[b] == '}')
        b += 1
    return texto[a:b]


PRE = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define BT3_GS_ESPERAS
#define PORT_X360
typedef long LONG;
#define pthread_attr_t AtributoSimulado
typedef int pthread_attr_t;

static int sGpu, sInitResult, sThreaded, sThread, gGsFrameJogo, gGsFrame;
static int sDrawFrom, sDumpFrame, sProbeX, sProbeY, iniciou, esperou, logou, descarregou;
static int resultado;
static struct { int prmodecont; } gs;
static jmp_buf saida;
static int pthread_attr_init(pthread_attr_t *p) { *p=0; return 0; }
static int pthread_attr_setstacksize(pthread_attr_t *p, unsigned n) { (void)p; (void)n; return 0; }
static void *render_thread(void *p) { return p; }
static int pthread_create(int *t, const pthread_attr_t *a, void *(*f)(void *), void *p) {
    (void)t; (void)a; (void)f; (void)p; return 0;
}
static void tables_init(void) {}
static const char *gs_mode(void) { return getenv("BT3_GS"); }
static int GsGpu_Init(void) { iniciou++; return resultado; }
static void render_post(int a, int b, int c) { assert(a==1 && !b && !c); sInitResult=GsGpu_Init(); }
static void render_wait(void) { esperou++; }
static void Diario_Escrever(const char *s) { assert(strstr(s,"vídeo:")); logou++; }
static void Diario_Descarregar(void) { descarregou++; }
static void InterlockedExchange(LONG *p, LONG n) { *p=n; }
static void Port_Sair(int codigo) { assert(codigo==7); longjmp(saida,1); }
'''
FIM = r'''
int main(int argc, char **argv) {
    int falha;
    assert(argc==4);
    resultado=atoi(argv[1]); falha=atoi(argv[3]);
    setenv("BT3_GS", argv[2], 1);
    if (falha & 2) setenv("BT3_GS_THREAD","1",1);
    if (setjmp(saida)==0) {
        gs_on();
        assert(!(falha & 1));
        assert(!logou && !descarregou);
    } else {
        assert((falha & 1) && logou==1 && descarregou==1);
        assert(!sThreaded || esperou==1);
    }
    if (!strcmp(argv[2],"gpu")) assert(iniciou==1);
    else assert(!iniciou);
    return 0;
}
'''


def main():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    core = (RAIZ / 'port/src/gs/gs_core.c').read_text()
    principal = (RAIZ / 'port/x360/src/f2/main360.cpp').read_text()
    on = funcao(core, 'static int gs_on(void)')
    falha = funcao(principal, 'extern "C" void Port_FalhaVideo360(void)')
    falha = falha.replace('extern "C" ', '')
    # A decisão de sair fica depois da desmontagem, antes da espera por botão.
    a = principal.index('if (gTerminou && !desmontado)')
    b = principal.index('if (gTerminou && !BT3_TESTE_FALHAS)', a)
    assert principal.index('gpu = false;', a) < b
    c = principal.index('if (gTerminou) {', b)
    assert a < b < c
    casos = [('1','gpu','0'), ('1','gpu','2'), ('0','gpu','1'), ('0','gpu','3'), ('0','none','0')]
    mutantes = [
        ('íntegro', on, falha, False),
        ('falha ignorada', on.replace('if (!sGpu)', 'if (0)'), falha, True),
        ('diário sem descarregar', on, falha.replace('Diario_Descarregar();',''), True),
        ('sem saída automática', on, falha.replace('Port_Sair(7);',''), True),
    ]
    with tempfile.TemporaryDirectory(prefix='bt4-video-', dir=Path.home()) as tmp:
        pasta = Path(tmp)
        for nome, inicia, sai, negativo in mutantes:
            f = pasta / 'teste.c'
            f.write_text(PRE + sai + inicia + FIM)
            exe = pasta / 'teste'
            subprocess.run(['gcc','-std=gnu89','-O2',str(f),'-o',str(exe)], check=True)
            resultados = [subprocess.run([str(exe), *caso], capture_output=True).returncode for caso in casos]
            assert any(resultados) == negativo, (nome, resultados)
            print(nome + (': recusado' if negativo else ': cinco caminhos aprovados'))
    print('Início do vídeo: tudo certo; desmontagem completa tem prova separada em teste_saida360.py')


if __name__ == '__main__':
    main()
