/*
 * Os dados do jogo direto da ISO do usuário (decisão 18): sem instalador e sem pastas de milhares de arquivos.
 *
 * O código do jogo e do port abre os dados por caminho sob a raiz BT3_DATA, como no PC (port/src/plat_file.c,
 * plat_fcache.c, plat_mem.c, snd_adx.c): <raiz>/disc/<arquivo do disco>, <raiz>/pzs3usN/NNNNN.bin (a entrada NNNNN
 * do AFS DATA/PZS3USN.AFS) e <raiz>/mods/<o mesmo caminho>. Com uma ISO na pasta do .xex, o fopen do shim
 * (port/x360/shim/stdio.h, Port_X360Fopen em posix360.c) entrega esses caminhos a Iso_Abrir:
 *   - mods/...    arquivo de verdade, como no PC;
 *   - disc/...    o arquivo da ISO (ISO 9660, sem extensões: nomes em maiúsculas com ";1");
 *   - pzsN/...    a entrada do AFS dentro da ISO.
 * A ISO tem de ser a convertida no PC (decisão 23, port/x360/tools/iso_convertida.py): Iso_Iniciar procura a marca
 * dela (docs/x360/formatos/iso-convertida.md) e, sem a marca, recusa a imagem (Iso_Recusada; a main mostra a caixa
 * do sistema e o jogo não começa). Da ISO convertida, a entrada com conversor (Conv_FormatoDe, port/x360/conv) já
 * vem na ordem do 360: é lida inteira na abertura, sem Conv_Arquivo; o resto (fluxos de som, filmes) é lido da ISO
 * a cada fread. Os conteúdos comprimidos (BPE) continuam convertidos depois de descomprimidos, pelos ganchos do
 * jogo (port/x360/conv/conv_jogo.c). Um arquivo de mods/ ou de uma pasta solta (sem ISO) está no formato do PS2 e
 * o que tem conversor se converte ao abrir (Iso_DeArquivo; os mods/ ficam assim até as últimas fases). O arquivo
 * aberto é um IsoArq do vetor sAbertos, e as funções de stdio do shim (Port_X360Fread...) reconhecem um IsoArq
 * pelo endereço e mandam o resto para a CRT. Caminho sob a raiz que não é do disco (fases e músicas de fora,
 * port/src/plat_stages.c, plat_songs.c) vai para o arquivo de verdade.
 * Só leitura. Mais de uma thread lê: Iso_Iniciar roda na main antes das threads. A ISO tem dois handles, cada um
 * com a sua trava: o comum (o jogo, o filme, o boot) e o da thread de leitura dos fluxos de som (som/fluxo.c,
 * Iso_LeitoraPropria), para que a leitura de um bloco de som não segure a trava que a thread do jogo pega. Abrir uma
 * entrada do AFS na thread do jogo (o Port_FilePath de cada ADXT_Start*) não espera leitura de outra thread: a tabela
 * lida não tem trava e a tabela dos abertos tem a trava dela (F6, plano/F6/05.md, "A thread do jogo sem esperar").
 */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diario.h"
#include "conv.h"
#include "f2/blocos.h"

void Port_Abortar(void);
int Filme360_BlocoKb(void); /* f2/filme360.c: BT3_FILME e BT3_FILME_BLOCO_KB */

#define SETOR 2048
#define MAX_ABERTOS 64
#define MARCA_POS 0x3DF400 /* a marca da ISO convertida: na folga do cabeçalho de DATA/PZS3US0.AFS, zerada na original */
#define MARCA_TAM 512

typedef struct IsoArq {
    int usado;
    __int64 ini;            /* posição na ISO */
    long tam, pos;
    unsigned char *dados;   /* convertido (lido inteiro), ou NULL: lido da ISO a cada fread */
    Blocos *blocos;         /* (F6, etapa 6) um filme com BT3_FILME=1: lido em blocos, o seguinte adiantado (f2/blocos.c) */
    int filme;              /* 1: um .PSS com BT3_FILME=1 (os blocos se abrem na primeira leitura; 2: não abriram) */
    char nome[16];
} IsoArq;

typedef struct Afs { volatile int lido; __int64 ini; unsigned n; unsigned *tab; } Afs; /* tab: deslocamento e tamanho */

static HANDLE sIso = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION sTrava;       /* o handle comum (cada ler()) e a carga das tabelas dos AFS */
/* (F6) o handle da thread de leitura dos fluxos de som: só ela lê por ele (ler() reconhece a thread), com a trava
   dele; a leitura de um bloco (até 98 ms medidos no console) não segura a sTrava */
static HANDLE sIsoLeitora = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION sTravaLeitora;
static volatile int sTemLeitora;
static volatile DWORD sLeitora;       /* a thread dela (GetCurrentThreadId) */
static CRITICAL_SECTION sTravaTabela; /* sAbertos: só a escolha e a volta de um lugar, sem E/S */
static int sIniciado;
static int sRecusada; /* a ISO aberta não tem a marca da convertida: o jogo não começa */
static unsigned sRaizLba, sRaizTam;
static Afs sAfs[3];
static IsoArq sAbertos[MAX_ABERTOS];
static char sNomeIso[512];

static double ms_desde(LARGE_INTEGER antes) {
    LARGE_INTEGER agora, f;
    QueryPerformanceCounter(&agora);
    QueryPerformanceFrequency(&f);
    return (double)(agora.QuadPart - antes.QuadPart) * 1000.0 / (double)f.QuadPart;
}

/* (F5, etapa 8, picos) as leituras do disco desde a última entrega do jogo: quantas, os bytes e o tempo (o relógio do
   QueryPerformanceCounter, com a trava); o gs_d3d.cpp as junta no registro dos quadros lentos (gs_internal.h) */
unsigned gPicosIsoN;
unsigned __int64 gPicosIsoBytes, gPicosIsoTiques;

static int ler(__int64 pos, void *buf, unsigned long n) {
    LARGE_INTEGER p, t0, t1;
    DWORD lidos = 0;
    BOOL ok;
    /* a thread de leitura dos fluxos pelo handle dela; as outras pelo comum */
    int propria = sTemLeitora && GetCurrentThreadId() == sLeitora;
    HANDLE h = propria ? sIsoLeitora : sIso;
    CRITICAL_SECTION *trava = propria ? &sTravaLeitora : &sTrava;
    p.QuadPart = pos;
    QueryPerformanceCounter(&t0);
    EnterCriticalSection(trava);
    ok = SetFilePointerEx(h, p, NULL, FILE_BEGIN) && ReadFile(h, buf, n, &lidos, NULL);
    LeaveCriticalSection(trava);
    QueryPerformanceCounter(&t1);
    gPicosIsoN++;
    gPicosIsoBytes += n;
    gPicosIsoTiques += (unsigned __int64)(t1.QuadPart - t0.QuadPart);
    return ok && lidos == n;
}

static unsigned le32(const unsigned char *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

static void hexa(const unsigned char *p, unsigned n, char *s) {
    static const char dig[] = "0123456789abcdef";
    unsigned i;
    for (i = 0; i < n; i++) {
        s[i * 2] = dig[p[i] >> 4];
        s[i * 2 + 1] = dig[p[i] & 15];
    }
    s[n * 2] = '\0';
}

/* A marca da ISO convertida no PC (decisão 23, docs/x360/formatos/iso-convertida.md): 512 bytes em 0x3DF400 que
   começam por "BT3X360-ISO" e cinco zeros. Sem política de versão por enquanto (resposta do Lucas no
   plano/F5/02.md): a convertida se reconhece pela assinatura, e o resto da marca só vai para o log. */
static int tem_marca(const char *caminho) {
    static unsigned char m[MARCA_TAM];
    static char md5[33], fontes[65], exe[65];
    unsigned i, zeros = 0;
    if (!ler(MARCA_POS, m, MARCA_TAM)) {
        Diario_Escrever("iso: %s: a leitura da marca em 0x%X falhou (imagem menor que o esperado?)", caminho, MARCA_POS);
        return 0;
    }
    if (memcmp(m, "BT3X360-ISO\0\0\0\0\0", 16) != 0) {
        for (i = 0; i < MARCA_TAM; i++) {
            zeros += m[i] == 0;
        }
        Diario_Escrever("iso: %s sem a marca da ISO convertida em 0x%X (%s): recusada", caminho, MARCA_POS,
                        zeros == MARCA_TAM ? "a região está zerada, como na ISO original" : "bytes desconhecidos");
        return 0;
    }
    hexa(m + 32, 16, md5);
    hexa(m + 80, 32, fontes);
    hexa(m + 112, 32, exe);
    Diario_Escrever("iso: marca da ISO convertida em 0x%X: versão %u, marca de %u bytes, origem de %.0f bytes com MD5 "
                    "%s; fontes dos conversores %s; conv_tool %s", MARCA_POS, le32(m + 16), le32(m + 20),
                    (double)le32(m + 24) + (double)le32(m + 28) * 4294967296.0, md5, fontes, exe);
    return 1;
}

/* A primeira .iso da pasta do .xex (ou BT3_ISO). Nome no FATX: até 42 caracteres. Chamada pela main antes de criar
   threads (main360.cpp), depois do ambiente. */
void Iso_Iniciar(void) {
    WIN32_FIND_DATA d;
    HANDLE h;
    unsigned char pvd[SETOR];
    char caminho[512];
    const char *escolhido = getenv("BT3_ISO");
    int pedida = escolhido != NULL && escolhido[0] != '\0';

    if (sIniciado) {
        return;
    }
    InitializeCriticalSection(&sTrava);
    InitializeCriticalSection(&sTravaLeitora);
    InitializeCriticalSection(&sTravaTabela);
    sIniciado = 1;
    if (escolhido != NULL && escolhido[0] != '\0') {
        strncpy(caminho, escolhido, sizeof(caminho) - 1);
    } else {
        h = FindFirstFile("game:\\*.iso", &d);
        if (h == INVALID_HANDLE_VALUE) {
            Diario_Escrever("iso: nenhuma .iso em game:\\, os dados vêm de arquivos soltos");
            return;
        }
        FindClose(h);
        _snprintf(caminho, sizeof(caminho) - 1, "game:\\%s", d.cFileName);
    }
    caminho[sizeof(caminho) - 1] = '\0';
    sIso = CreateFile(caminho, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (sIso == INVALID_HANDLE_VALUE) {
        Diario_Escrever("iso: não abriu %s (erro %u)", caminho, (unsigned)GetLastError());
        if (pedida) {
            Diario_Escrever("iso: o BT3_ISO foi dado e o arquivo não existe: recusada (caso ISO não encontrada)");
            strncpy(sNomeIso, caminho, sizeof(sNomeIso) - 1);
            sRecusada = 2;
        }
        return;
    }
    if (!ler(16 * SETOR, pvd, SETOR) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        Diario_Escrever("iso: %s não é uma imagem ISO 9660", caminho);
        CloseHandle(sIso);
        sIso = INVALID_HANDLE_VALUE;
        if (pedida) {
            Diario_Escrever("iso: o BT3_ISO foi dado e o arquivo não é uma ISO: recusada (caso ISO não encontrada)");
            strncpy(sNomeIso, caminho, sizeof(sNomeIso) - 1);
            sRecusada = 2;
        }
        return;
    }
    sRaizLba = le32(pvd + 156 + 2);
    sRaizTam = le32(pvd + 156 + 10);
    strncpy(sNomeIso, caminho, sizeof(sNomeIso) - 1);
    Diario_Escrever("iso: %s", caminho);
    if (!tem_marca(caminho)) {
        CloseHandle(sIso);
        sIso = INVALID_HANDLE_VALUE;
        sRecusada = 1; /* caso ISO original */
        return;
    }
    Diario_Escrever("iso: os arquivos com conversor já vêm convertidos: a carga só lê (sem Conv_Arquivo)");
    sIsoLeitora = CreateFile(caminho, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (sIsoLeitora == INVALID_HANDLE_VALUE) {
        Diario_Escrever("iso: o segundo handle (o da leitura dos fluxos de som) não abriu (erro %u): ela lê pelo comum",
                        (unsigned)GetLastError());
    }
}

/* (F6) Chamada pela thread de leitura dos fluxos de som (som/fluxo.c) ao começar: daí em diante o que ela lê da ISO vai
   pelo segundo handle, com a trava dele. Sem o segundo handle, nada muda (o comum, com a sTrava). */
void Iso_LeitoraPropria(void) {
    if (sIsoLeitora != INVALID_HANDLE_VALUE && !sTemLeitora) {
        sLeitora = GetCurrentThreadId();
        __lwsync(); /* a thread gravada antes da marca */
        sTemLeitora = 1;
        Diario_Escrever("iso: a leitura dos fluxos de som lê pelo handle dela, sem a trava comum");
    }
}

/* Diferente de 0 se a ISO foi recusada (a main mostra a recusa e não começa o jogo): 1 = a ISO aberta não é a
   convertida (a original); 2 = o BT3_ISO foi dado e o arquivo não existe ou não é uma ISO (Iso_Pedida diz o caminho). */
int Iso_Recusada(void) {
    return sRecusada;
}

/* O caminho do BT3_ISO que falhou (só vale com Iso_Recusada() == 2). */
const char *Iso_Pedida(void) {
    return sNomeIso;
}

/* Compara um nome do caminho com o do registro (sem caixa, sem o ";1" da versão nem o "." de quem não tem
   extensão). */
static int mesmo_nome(const char *a, size_t na, const unsigned char *b, unsigned nb) {
    size_t i;
    const unsigned char *v = (const unsigned char *)memchr(b, ';', nb);
    if (v != NULL) {
        nb = (unsigned)(v - b);
    }
    if (nb > 0 && b[nb - 1] == '.') {
        nb--;
    }
    if (nb != na) {
        return 0;
    }
    for (i = 0; i < na; i++) {
        char x = a[i], y = (char)b[i];
        if (x >= 'a' && x <= 'z') {
            x = (char)(x - 32);
        }
        if (y >= 'a' && y <= 'z') {
            y = (char)(y - 32);
        }
        if (x != y) {
            return 0;
        }
    }
    return 1;
}

/* "DATA/PZS3US1.AFS" -> posição e tamanho na ISO; 0 se não há. Os registros não cruzam setores. */
static int achar(const char *caminho, __int64 *ini, long *tam) {
    unsigned lba = sRaizLba, dirTam = sRaizTam;
    unsigned char setor[SETOR];
    const char *p = caminho;

    for (;;) {
        const char *fim = p;
        size_t n;
        unsigned s, achou = 0, ultimo;
        while (*fim != '\0' && *fim != '/') {
            fim++;
        }
        n = (size_t)(fim - p);
        ultimo = *fim == '\0';
        for (s = 0; s < (dirTam + SETOR - 1) / SETOR && !achou; s++) {
            unsigned k = 0;
            if (!ler((__int64)(lba + s) * SETOR, setor, SETOR)) {
                return 0;
            }
            while (k < SETOR && setor[k] != 0) {
                unsigned len = setor[k], nl = setor[k + 32];
                if (k + 33 + nl <= SETOR && mesmo_nome(p, n, setor + k + 33, nl)) {
                    int dir = (setor[k + 25] & 2) != 0;
                    if (ultimo != (unsigned)!dir) {
                        return 0; /* pasta onde se queria arquivo, ou o contrário */
                    }
                    lba = le32(setor + k + 2);
                    dirTam = le32(setor + k + 10);
                    achou = 1;
                    break;
                }
                k += len;
            }
        }
        if (!achou) {
            return 0;
        }
        if (ultimo) {
            if (dirTam > 0x7FFFFFFFu) {
                return 0; /* tamanho em long de 32 bits: nenhum arquivo do disco passa disso (os AFS se leem por entrada) */
            }
            *ini = (__int64)lba * SETOR;
            *tam = (long)dirTam;
            return 1;
        }
        p = fim + 1;
    }
}

/* A tabela do AFS DATA/PZS3USn.AFS ("AFS\0", n, depois deslocamento e tamanho de cada entrada). Depois de lida, sem
   trava (a thread do jogo abre uma entrada a cada ADXT_Start* e não pode esperar a leitura de outra thread). A carga é
   sob a trava (duas threads podem pedir a primeira entrada ao mesmo tempo), e a tabela só aparece já na ordem do host,
   antes do `lido`. A trava é recursiva (seção crítica): ler() a toma de novo. */
static Afs *afs(int n) {
    static const char *nomes[3] = {"DATA/PZS3US0.AFS", "DATA/PZS3US1.AFS", "DATA/PZS3US2.AFS"};
    Afs *a = &sAfs[n];
    unsigned char cab[8];
    unsigned *tab = NULL;
    unsigned i, cont = 0;
    __int64 ini = 0;
    long tam;
    if (a->lido) { /* já lida: sem trava */
        __lwsync(); /* a tabela gravada antes do `lido` */
        return a->tab != NULL ? a : NULL;
    }
    EnterCriticalSection(&sTrava);
    if (a->lido) {
        LeaveCriticalSection(&sTrava);
        return a->tab != NULL ? a : NULL;
    }
    if (!achar(nomes[n], &ini, &tam) || !ler(ini, cab, 8) || memcmp(cab, "AFS", 4) != 0) {
        Diario_Escrever("iso: %s não está na ISO ou não é AFS", nomes[n]);
    } else {
        cont = le32(cab + 4);
        tab = (unsigned *)malloc((size_t)cont * 8);
        if (tab == NULL || !ler(ini + 8, tab, (unsigned long)cont * 8)) {
            Diario_Escrever("iso: tabela de %s ilegível", nomes[n]);
            free(tab);
            tab = NULL;
        } else {
            for (i = 0; i < cont * 2; i++) {
                tab[i] = le32((unsigned char *)tab + i * 4);
            }
            Diario_Escrever("iso: %s com %u entradas", nomes[n], cont);
        }
    }
    a->ini = ini;
    a->n = cont;
    a->tab = tab;
    __lwsync(); /* a tabela antes do `lido`: quem a vê sem trava a vê inteira */
    a->lido = 1;
    LeaveCriticalSection(&sTrava);
    return tab != NULL ? a : NULL;
}

/* Conv_Arquivo sobre um arquivo solto inteiro (mods/, ou dados sem ISO); falha é fatal (decisão 12: formato que não bate não segue). */
static void converter(const char *rel, unsigned char *dados, long tam, double msLer) {
    char msg[240];
    LARGE_INTEGER t0;
    unsigned char *feito = (unsigned char *)calloc((size_t)tam + 1, 1);
    QueryPerformanceCounter(&t0);
    if (feito == NULL) {
        Diario_Escrever("iso: %s (%ld bytes): sem memória para a marcação", rel, tam);
        Port_Abortar();
    }
    if (Conv_Arquivo(rel, (conv_u8 *)dados, (conv_ofs)tam, feito, msg, sizeof(msg)) < 0) {
        Diario_Escrever("iso: conversão de %s: %s", rel, msg);
        Port_Abortar();
    }
    free(feito);
    /* de onde vem a pausa da carga (decisão 18): leitura ou conversão */
    Diario_Escrever("iso: %s, %ld bytes: leitura %.1f ms, conversão %.1f ms", rel, tam, msLer, ms_desde(t0));
}

static IsoArq *novo(__int64 ini, long tam) {
    int i;
    IsoArq *r = NULL;
    EnterCriticalSection(&sTravaTabela); /* um lugar (não a sTrava: ela fica pega durante uma leitura) */
    for (i = 0; i < MAX_ABERTOS; i++) {
        if (!sAbertos[i].usado) {
            r = &sAbertos[i];
            r->usado = 1;
            break;
        }
    }
    LeaveCriticalSection(&sTravaTabela);
    if (r == NULL) {
        Diario_Escrever("iso: mais de %d arquivos abertos", MAX_ABERTOS);
        return NULL;
    }
    r->ini = ini;
    r->tam = tam;
    r->pos = 0;
    r->dados = NULL;
    r->blocos = NULL;
    r->filme = 0;
    r->nome[0] = '\0';
    return r;
}

/* (F6, etapa 6) Um filme do disco (disc/DATA/<nome>.PSS): com BT3_FILME=1, lido em blocos de BT3_FILME_BLOCO_KB (256) pela
   thread de leitura do blocos.c, na thread de hardware 1 (a irmã da main, que só vigia; a 4 e a 5 são do áudio e não
   fazem E/S). Cada pedaço de 64 KB de um bloco é um ler(), com a trava: as outras leituras da ISO esperam no máximo
   um pedaço. Quem lê (o decodificador, na thread do jogo) só copia da memória, e espera só se o bloco não chegou. */
#define FILME_HW 1
static int eh_filme(const char *rel) {
    size_t n = strlen(rel);
    return strncmp(rel, "disc/", 5) == 0 && n > 4 && _stricmp(rel + n - 4, ".PSS") == 0 && strrchr(rel, '/') != NULL;
}
static int ler_blocos(void *ctx, long pos, void *buf, long n) {
    IsoArq *r = (IsoArq *)ctx;
    return ler(r->ini + pos, buf, (unsigned long)n);
}

/* a espera da thread do jogo pelos blocos do filme aberto, somada (Filme360: a parte "leitura" de cada imagem); 0 sem
   filme em blocos. Da thread que lê (os contadores da espera são dela). */
static IsoArq *sFilmeBlocos;
int Iso_FilmeEspera(double *ms, unsigned *esperas) {
    BlocosConta c;
    if (sFilmeBlocos == NULL || sFilmeBlocos->blocos == NULL) {
        return 0;
    }
    Blocos_Conta(sFilmeBlocos->blocos, &c);
    *ms = c.msEspera;
    *esperas = c.esperas;
    return 1;
}

/* rel: o caminho sob a raiz de dados, com '/' ("pzs3us1/00042.bin", "disc/SLUS_216.78"). NULL se não há, e
   ISO_SEM se não há ISO (aí o chamador abre o arquivo de verdade). */
void *Iso_Abrir(const char *rel) {
    __int64 ini;
    long tam;
    int part, ent;
    char resto[8];
    IsoArq *r;

    if (!sIniciado) {
        Iso_Iniciar();
    }
    if (sRecusada) {
        return NULL; /* (o jogo nem começa; por garantia, nada da ISO recusada) */
    }
    if (sIso == INVALID_HANDLE_VALUE || strncmp(rel, "mods/", 5) == 0) {
        return (void *)-1;
    }
    if (strncmp(rel, "disc/", 5) == 0) {
        if (!achar(rel + 5, &ini, &tam)) {
            return NULL;
        }
    } else if (sscanf(rel, "pzs3us%1d/%5d.%3s", &part, &ent, resto) == 3 && part >= 0 && part <= 2 &&
               strcmp(resto, "bin") == 0) {
        Afs *a = afs(part);
        if (a == NULL || ent < 0 || (unsigned)ent >= a->n) {
            return NULL;
        }
        ini = a->ini + a->tab[ent * 2];
        tam = (long)a->tab[ent * 2 + 1];
    } else {
        return (void *)-1; /* sob a raiz mas não do disco (fases, músicas de fora): o arquivo de verdade */
    }
    r = novo(ini, tam);
    if (r == NULL) {
        return NULL;
    }
    if (Conv_FormatoDe(rel) != NULL) { /* lido inteiro como antes, para a carga ler da memória; já convertido */
        LARGE_INTEGER t0;
        QueryPerformanceCounter(&t0);
        r->dados = (unsigned char *)malloc((size_t)tam + 1);
        if (r->dados == NULL || !ler(ini, r->dados, (unsigned long)tam)) {
            Diario_Escrever("iso: %s (%ld bytes): sem memória ou leitura falhou", rel, tam);
            Port_Abortar();
        }
        Diario_Escrever("iso: %s, %ld bytes: leitura %.1f ms (da ISO convertida, sem conversão)", rel, tam,
                        ms_desde(t0));
    } else if (eh_filme(rel) && Filme360_BlocoKb() > 0) {
        r->filme = 1; /* (os blocos na primeira leitura: o handle do jogo, aberto pelo ADXF, não lê com BT3_FILME) */
        _snprintf(r->nome, sizeof(r->nome) - 1, "%s", strrchr(rel, '/') + 1);
    }
    return r;
}

/* Um arquivo de verdade sob a raiz (mods/, ou dados soltos sem ISO) que tem conversor: lido inteiro e convertido,
   como a entrada da ISO. NULL se não deu para ler; o chamador fecha f. */
void *Iso_DeArquivo(FILE *f, const char *rel) {
    LARGE_INTEGER t0;
    IsoArq *r;
    long tam;
    if (!sIniciado) {
        Iso_Iniciar();
    }
    QueryPerformanceCounter(&t0);
    if (fseek(f, 0, SEEK_END) != 0 || (tam = ftell(f)) < 0 || fseek(f, 0, SEEK_SET) != 0) {
        return NULL;
    }
    r = novo(0, tam);
    if (r == NULL) {
        return NULL;
    }
    r->dados = (unsigned char *)malloc((size_t)tam + 1);
    if (r->dados == NULL || fread(r->dados, 1, (size_t)tam, f) != (size_t)tam) {
        Diario_Escrever("iso: %s (%ld bytes, arquivo solto): sem memória ou leitura falhou", rel, tam);
        Port_Abortar();
    }
    converter(rel, r->dados, tam, ms_desde(t0));
    return r;
}

int Iso_Eh(const void *f) {
    return (const IsoArq *)f >= sAbertos && (const IsoArq *)f < sAbertos + MAX_ABERTOS;
}

size_t Iso_Ler(void *buf, size_t tam, size_t n, void *f) {
    IsoArq *r = (IsoArq *)f;
    size_t quer = tam * n, tem;
    if (tam == 0 || n == 0 || r->pos >= r->tam) {
        return 0;
    }
    tem = (size_t)(r->tam - r->pos);
    if (quer > tem) {
        quer = tem - tem % tam; /* só itens inteiros */
    }
    if (r->filme == 1 && r->blocos == NULL) {
        r->blocos = Blocos_Abrir(ler_blocos, r, r->tam, (long)Filme360_BlocoKb() * 1024, FILME_HW);
        r->filme = r->blocos != NULL ? 1 : 2;
        sFilmeBlocos = r->blocos != NULL ? r : sFilmeBlocos;
        Diario_Escrever("iso: %s, %ld bytes: em blocos de %d KB, o seguinte adiantado na thread de hardware %d%s", r->nome,
                        r->tam, Filme360_BlocoKb(), FILME_HW, r->blocos == NULL ? " (SEM MEMÓRIA: lido a cada fread)" : "");
    }
    if (r->dados != NULL) {
        memcpy(buf, r->dados + r->pos, quer);
    } else if (r->blocos != NULL) {
        long g = Blocos_Ler(r->blocos, r->pos, buf, (long)quer);
        if (g < 0) {
            return 0;
        }
        quer = (size_t)g - (size_t)g % tam;
    } else if (quer > 0 && !ler(r->ini + r->pos, buf, (unsigned long)quer)) {
        return 0;
    }
    r->pos += (long)quer;
    return quer / tam;
}

int Iso_Posicionar(void *f, long desl, int origem) {
    IsoArq *r = (IsoArq *)f;
    long p = origem == SEEK_SET ? desl : origem == SEEK_CUR ? r->pos + desl : r->tam + desl;
    if (p < 0) {
        return -1;
    }
    r->pos = p;
    return 0;
}

long Iso_Posicao(void *f) {
    return ((IsoArq *)f)->pos;
}

int Iso_Fechar(void *f) {
    IsoArq *r = (IsoArq *)f;
    if (r->blocos != NULL) {
        BlocosConta c;
        Blocos_Conta(r->blocos, &c);
        Diario_Escrever("iso: %s em blocos: %u blocos lidos na thread de leitura (%.1f ms por bloco, pior %.1f), %u faltas; a "
                        "thread do jogo esperou %u vezes, %.1f ms ao todo (pior %.1f)", r->nome, c.blocos,
                        c.blocos ? c.msLeitura / c.blocos : 0.0, c.msLeituraPior, c.faltas, c.esperas, c.msEspera,
                        c.msEsperaPior);
        Blocos_Fechar(r->blocos);
        r->blocos = NULL;
        if (sFilmeBlocos == r) {
            sFilmeBlocos = NULL;
        }
    }
    free(r->dados);
    r->dados = NULL;
    EnterCriticalSection(&sTravaTabela); /* o lugar volta (não a sTrava, como no novo()) */
    r->usado = 0;
    LeaveCriticalSection(&sTravaTabela);
    return 0;
}

int Iso_Caractere(void *f) {
    unsigned char c;
    return Iso_Ler(&c, 1, 1, f) == 1 ? c : EOF;
}
