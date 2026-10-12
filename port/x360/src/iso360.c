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
 * Entrada com conversor (Conv_Arquivo, port/x360/conv) é lida inteira e convertida na abertura; o resto (fluxos
 * de som, filmes) é lido da ISO a cada fread. O arquivo aberto é um IsoArq do vetor sAbertos, e as funções de
 * stdio do shim (Port_X360Fread...) reconhecem um IsoArq pelo endereço e mandam o resto para a CRT.
 * Só leitura. Mais de uma thread lê (a do jogo e a do som): a ISO tem um handle só, com trava.
 */
#include <xtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "diario.h"
#include "conv.h"

void Port_Abortar(void);

#define SETOR 2048
#define MAX_ABERTOS 64

typedef struct IsoArq {
    int usado;
    __int64 ini;            /* posição na ISO */
    long tam, pos;
    unsigned char *dados;   /* convertido (lido inteiro), ou NULL: lido da ISO a cada fread */
} IsoArq;

typedef struct Afs { int lido; __int64 ini; unsigned n; unsigned *tab; } Afs; /* tab: deslocamento e tamanho */

static HANDLE sIso = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION sTrava;
static int sIniciado;
static unsigned sRaizLba, sRaizTam;
static Afs sAfs[3];
static IsoArq sAbertos[MAX_ABERTOS];
static char sNomeIso[512];

static int ler(__int64 pos, void *buf, unsigned long n) {
    LARGE_INTEGER p;
    DWORD lidos = 0;
    BOOL ok;
    p.QuadPart = pos;
    EnterCriticalSection(&sTrava);
    ok = SetFilePointerEx(sIso, p, NULL, FILE_BEGIN) && ReadFile(sIso, buf, n, &lidos, NULL);
    LeaveCriticalSection(&sTrava);
    return ok && lidos == n;
}

static unsigned le32(const unsigned char *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}

/* A primeira .iso da pasta do .xex (ou BT3_ISO). Nome no FATX: até 42 caracteres. */
static void iniciar(void) {
    WIN32_FIND_DATA d;
    HANDLE h;
    unsigned char pvd[SETOR];
    char caminho[512];
    const char *escolhido = getenv("BT3_ISO");

    sIniciado = 1;
    InitializeCriticalSection(&sTrava);
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
        return;
    }
    if (!ler(16 * SETOR, pvd, SETOR) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        Diario_Escrever("iso: %s não é uma imagem ISO 9660", caminho);
        CloseHandle(sIso);
        sIso = INVALID_HANDLE_VALUE;
        return;
    }
    sRaizLba = le32(pvd + 156 + 2);
    sRaizTam = le32(pvd + 156 + 10);
    strncpy(sNomeIso, caminho, sizeof(sNomeIso) - 1);
    Diario_Escrever("iso: %s", caminho);
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
            *ini = (__int64)lba * SETOR;
            *tam = (long)dirTam;
            return 1;
        }
        p = fim + 1;
    }
}

/* A tabela do AFS DATA/PZS3USn.AFS ("AFS\0", n, depois deslocamento e tamanho de cada entrada). */
static Afs *afs(int n) {
    static const char *nomes[3] = {"DATA/PZS3US0.AFS", "DATA/PZS3US1.AFS", "DATA/PZS3US2.AFS"};
    Afs *a = &sAfs[n];
    unsigned char cab[8];
    long tam;
    if (a->lido) {
        return a->tab != NULL ? a : NULL;
    }
    a->lido = 1;
    if (!achar(nomes[n], &a->ini, &tam) || !ler(a->ini, cab, 8) || memcmp(cab, "AFS", 4) != 0) {
        Diario_Escrever("iso: %s não está na ISO ou não é AFS", nomes[n]);
        return NULL;
    }
    a->n = le32(cab + 4);
    a->tab = (unsigned *)malloc((size_t)a->n * 8);
    if (a->tab == NULL || !ler(a->ini + 8, a->tab, (unsigned long)a->n * 8)) {
        Diario_Escrever("iso: tabela de %s ilegível", nomes[n]);
        free(a->tab);
        a->tab = NULL;
        return NULL;
    }
    {
        unsigned i;
        unsigned char *b = (unsigned char *)a->tab;
        for (i = 0; i < a->n * 2; i++) {
            a->tab[i] = le32(b + i * 4);
        }
    }
    Diario_Escrever("iso: %s com %u entradas", nomes[n], a->n);
    return a;
}

static IsoArq *novo(__int64 ini, long tam) {
    int i;
    IsoArq *r = NULL;
    EnterCriticalSection(&sTrava);
    for (i = 0; i < MAX_ABERTOS; i++) {
        if (!sAbertos[i].usado) {
            r = &sAbertos[i];
            r->usado = 1;
            break;
        }
    }
    LeaveCriticalSection(&sTrava);
    if (r == NULL) {
        Diario_Escrever("iso: mais de %d arquivos abertos", MAX_ABERTOS);
        return NULL;
    }
    r->ini = ini;
    r->tam = tam;
    r->pos = 0;
    r->dados = NULL;
    return r;
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
        iniciar();
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
        return NULL;
    }
    r = novo(ini, tam);
    if (r == NULL) {
        return NULL;
    }
    if (Conv_FormatoDe(rel) != NULL) {
        char msg[240];
        unsigned char *feito = (unsigned char *)calloc((size_t)tam + 1, 1);
        r->dados = (unsigned char *)malloc((size_t)tam + 1);
        if (r->dados == NULL || feito == NULL || !ler(ini, r->dados, (unsigned long)tam)) {
            Diario_Escrever("iso: %s (%ld bytes): sem memória ou leitura falhou", rel, tam);
            Port_Abortar();
        }
        if (Conv_Arquivo(rel, (conv_u8 *)r->dados, (conv_ofs)tam, feito, msg, sizeof(msg)) < 0) {
            Diario_Escrever("iso: conversão de %s: %s", rel, msg);
            Port_Abortar();
        }
        free(feito);
    }
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
    if (r->dados != NULL) {
        memcpy(buf, r->dados + r->pos, quer);
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
    free(r->dados);
    r->dados = NULL;
    EnterCriticalSection(&sTrava);
    r->usado = 0;
    LeaveCriticalSection(&sTrava);
    return 0;
}

int Iso_Caractere(void *f) {
    unsigned char c;
    return Iso_Ler(&c, 1, 1, f) == 1 ? c : EOF;
}
