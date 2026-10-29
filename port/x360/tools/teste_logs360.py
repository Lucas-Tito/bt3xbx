#!/usr/bin/env python3
"""Compila ambiente, saída, estado e filme reais em ensaios nativos, com sabotagens."""
import pathlib
import re
import subprocess
import tempfile

RAIZ = pathlib.Path(__file__).resolve().parents[3]
SRC = RAIZ/'port/x360/src'


def funcao(texto, nome):
    inicio = re.search(r'^(?:static )?[^\n;{}]+\b'+nome+r'\([^;]*?\)\s*\{', texto, re.M).start()
    a = texto.index('{', inicio)
    nivel = 1
    b = a+1
    while nivel:
        nivel += (texto[b] == '{') - (texto[b] == '}')
        b += 1
    return texto[inicio:b]+'\n'


PRE = r'''
#include "xtl.h"
#include <cassert>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sstream>
#define __data_start TesteDataStart
#define _end TesteDataEnd
#define getenv Ambiente_Getenv
char *Ambiente_Getenv(const char *);
int gX360Log, clocks, abertos, formatacoes, alocacoes, sincrono;
struct LARGE_INTEGER { long long QuadPart; };
static void QueryPerformanceCounter(LARGE_INTEGER *p) { ++clocks; p->QuadPart=clocks*100; }
static void QueryPerformanceFrequency(LARGE_INTEGER *p) { ++clocks; p->QuadPart=1000; }
static void Diario_Abrir(const char *p) { ++abertos; FILE *f=fopen(p,"w"); fclose(f); }
static void Diario_Gancho(void (*)(void)) {}
static void Diario_Configurar(int s,int) { sincrono=s; }
static void Diario_Escrever(const char *,...) {}
static int Diario_Assincrono() { return 1; }
static void Port_X360FecharSaida() {}
static void mods_no_boot() {}
static void *TesteMalloc(size_t n) { ++alocacoes; return malloc(n); }
static int TesteVfprintf(FILE *f,const char *s,va_list a) { ++formatacoes; return vfprintf(f,s,a); }
#define vfprintf TesteVfprintf
#define malloc TesteMalloc
'''

AMBIENTE_POS = r'''
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    bool ligado=strcmp(argv[1],"ligado")==0;
    bool vazio=strcmp(argv[1],"ausente")==0;
    if(!vazio) {
        FILE *f=fopen("environment.txt","w");
        fprintf(f,"BT3_LOG=%s\nBT3_LOG_SINCRONO=1\nBT3_TEMPOS=1\nBT3_SHOT=1\nBT3_HASH=hash.txt\n",ligado?"1":argv[1]);
        fclose(f);
    }
    Ambiente_Carregar("environment.txt");
    for(int i=0;i<100;++i) {
        Port_Printf("teste %d",i);
        Port_X360Fprintf(Port_X360Saida(),"erro %d",i);
        Port_X360Fputs("teste",Port_X360Saida());
        Port_X360Fputc('x',Port_X360Saida());
    }
    assert((gX360Log!=0)==ligado);
    assert(abertos==(ligado?1:0));
    assert(sincrono==(ligado?1:0));
    assert(formatacoes==(ligado?200:0));
    assert((getenv("BT3_SHOT")!=NULL)==ligado);
    assert((getenv("BT3_TEMPOS")!=NULL)==ligado);
    assert(strcmp(getenv("BT3_SAVES"),"game:\\saves")==0);
    FILE *f=Port_X360Fopen("unexpected.txt","wb");
    assert((f!=NULL)==ligado);
    if(f) { fclose(f); remove("game:\\unexpected.txt"); }
    f=Port_X360Fopen("game:\\saves\\card1\\test-save","wb");
    assert(f!=NULL); fclose(f); remove("game:\\saves\\card1\\test-save");
    FILE *f=Port_X360Fopen; // MARCA
    return 0;
}
'''.replace('    FILE *f=Port_X360Fopen; // MARCA\n','')

ESTADO_PRE = r'''
unsigned gGsFrame;
unsigned gPortVBlanks;
int *gProgress;
const char *volatile gPortStage;
static void Port_GsInterromperJogo360() {}
static void Port_QuedaTeste360(int,int,unsigned) {}
static void Port_Sair(int) {}
static int Passos_Medindo() { return 0; }
static long Port_PadPlayPos() { return 0; }
static FILE *Port_X360Fopen(const char *p,const char *m) { ++abertos; return fopen(p,m); }
'''
FILME_PRE = r'''
#define __sync() ((void)0)
struct Mpeg2 {};
unsigned long long gPicosIsoTiques;
char gPortMoviePath[512];
static unsigned char planos[512*448];
static unsigned char destino[512*672];
static int fimFilme;
int Mpeg2_NextPlanes(Mpeg2 *,const unsigned char **y,const unsigned char **b,const unsigned char **r,int *s) { *y=*b=*r=planos; *s=512; return 1; }
int Mpeg2_Width(const Mpeg2 *) { return 512; }
int Mpeg2_Height(const Mpeg2 *) { return 448; }
int GsD3d_FilmeVaga(unsigned,unsigned,unsigned,unsigned char **d,unsigned *p,double *) { *d=destino;*p=512; return 1; }
void GsD3d_FilmeFim() { ++fimFilme; }
void GsD3d_FilmeEsperaCaptura() {}
int Iso_FilmeEspera(double *,unsigned *) { return 0; }
static unsigned GetCurrentProcessorNumber() { return 2; }
'''


def compilar(pasta, nome, codigo):
    fonte = pasta/(nome+'.cpp'); fonte.write_text(codigo)
    exe = pasta/nome
    subprocess.run(['g++','-std=c++11','-O2','-Wall','-Wextra','-Werror',
                    '-Wno-unknown-pragmas','-Wno-unused-function',
                    '-I'+str(RAIZ/'port/x360/tools/teste_queda'), '-I'+str(SRC),
                    str(fonte),'-o',str(exe)],check=True)
    return exe


def executar(exe, pasta, caso):
    pasta.mkdir()
    return subprocess.run([str(exe),caso],cwd=pasta,capture_output=True,text=True,timeout=5)


def conferir_arquivos(pasta, nome, caso):
    criados={a.name for a in pasta.iterdir()}
    esperados=set() if caso=='ausente' or nome!='ambiente' else {'environment.txt'}
    if caso=='ligado': esperados|={'game:\\bt3xbx.log','game:\\output.txt'}
    if criados!=esperados:
        raise ValueError(f'FALHA arquivos {nome}/{caso}: {criados} != {esperados}')
    return sorted(criados)


def codigos():
    posix = (SRC/'f2/posix360.c').read_text()
    a = posix.index('#define MAX_VARS'); b=posix.index('/* ------------------------------------------------------------------ arquivos */')
    env = PRE+'#include "diagnostico360.h"\n'+posix[a:b]
    env += 'FILE *Port_X360Saida();\n'+funcao(posix,'depois_do_ambiente')
    env += funcao(posix,'caminho360')+funcao(posix,'escrita_permitida')
    env += 'static int sSemMods;\n'
    env += 'static void *Iso_Abrir(const char *) { return (void *)-1; }\n'
    env += 'static const char *Conv_FormatoDe(const char *) { return NULL; }\n'
    env += 'static void *Iso_DeArquivo(FILE *,const char *) { return NULL; }\n'
    env += funcao(posix,'Port_X360Fopen')
    env += 'static FILE *sSaida;\n'
    for nome in ('Port_X360Saida','Port_Printf','Port_X360Fprintf','Port_X360Fputs','Port_X360Fputc','Port_X360Fwrite','Port_X360Fflush'):
        env += funcao(posix,nome)
    estado = (SRC/'f2/estado360.c').read_text().replace('#include <xtl.h>','').replace('#include "diario.h"','')
    filme = (SRC/'f2/filme360.c').read_text().replace('#include <xtl.h>','').replace('#include "diario.h"','')
    fim = r'''
static void depois_do_ambiente() {}
char *Ambiente_Getenv(const char *) { return NULL; }
int main() {
    CHAMADAS
    assert(clocks==0 && abertos==0 && alocacoes==0);
    VERIFICACOES
    puts("medidas desligadas: tudo certo");
}
'''
    return {
        'ambiente':env+AMBIENTE_POS,
        'estado':PRE+ESTADO_PRE+estado+fim.replace('static void depois_do_ambiente() {}','').replace('CHAMADAS','for(unsigned i=1;i<=120;++i) { Port_StateLog(i,(void *)1,0); } Estado_Resumo();').replace('VERIFICACOES','assert(gX360Luta==1); assert(strcmp(gPortStage,"battle")==0);'),
        'filme':PRE+FILME_PRE+filme+fim.replace('static void depois_do_ambiente() {}','').replace('CHAMADAS','for(int i=0;i<120;++i) { Filme360_Imagem(NULL); } Filme360_Fechou();').replace('VERIFICACOES','assert(fimFilme==1);'),
    }


def main():
    fontes=codigos()
    with tempfile.TemporaryDirectory(prefix='logs360-') as d:
        p=pathlib.Path(d)
        for nome,codigo in fontes.items():
            exe=compilar(p,nome,codigo)
            casos=('ausente','0','01','true','1x','ligado') if nome=='ambiente' else ('padrao',)
            for caso in casos:
                pasta=p/(nome+'-'+caso)
                r=executar(exe,pasta,caso)
                if r.returncode: raise SystemExit(f'{nome}/{caso}: {r.stderr}')
                criados=conferir_arquivos(pasta,nome,caso)
                print(f'{nome}/{caso}: tudo certo; arquivos={criados}')
        codigo=fontes['ambiente'].replace('output.txt','saida'+'.txt')
        exe=compilar(p,'nome-antigo',codigo)
        pasta=p/'caso-nome-antigo'; r=executar(exe,pasta,'ligado')
        if r.returncode: raise SystemExit(f'sabotagem não chegou à conferência dos arquivos: {r.stderr}')
        try: conferir_arquivos(pasta,'ambiente','ligado')
        except ValueError as e: print('mutante saída com nome antigo: acusado:',e)
        else: raise SystemExit('mutante saída com nome antigo não acusado')
        mutantes=(
            ('log ligado por padrão','ambiente','gX360Log = l != NULL && strcmp(l, "1") == 0;', 'gX360Log = l == NULL || strcmp(l, "1") == 0;', 'ausente'),
            ('saída sem guarda','ambiente','if (!gX360Log) return NULL;', '', 'ausente'),
            ('chave avulsa sem log','ambiente','if (!gX360Log && Diagnostico360_Chave(nome)) return NULL;', '', '0'),
            ('escrita auxiliar sem guarda','ambiente','if (!escrita_permitida(caminho, modo)) return NULL;', '(void)&escrita_permitida;', 'ausente'),
            ('tempo sem log','estado','if (!gX360Log) return;', '', 'padrao'),
            ('medida de filme por padrão','filme','sMedir = gX360Log && e != NULL && atoi(e) == 1;', 'sMedir = 1; (void)e;', 'padrao'),
            ('filme sem liberação','filme','        GsD3d_FilmeFim();', '', 'padrao'),
        )
        for i,(nome,alvo,antes,depois,caso) in enumerate(mutantes):
            codigo=fontes[alvo]; assert antes in codigo,nome
            exe=compilar(p,'mut'+str(i),codigo.replace(antes,depois))
            pasta=p/('caso-mut'+str(i));r=executar(exe,pasta,caso)
            if r.returncode != -6 or 'Assertion' not in r.stderr:
                raise SystemExit(f'mutante {nome} não acusado por uma asserção: {r.stderr}')
            print(f'mutante {nome}: acusado ({r.returncode})')
    print('logs e padrões: tudo certo')


if __name__=='__main__':
    main()
