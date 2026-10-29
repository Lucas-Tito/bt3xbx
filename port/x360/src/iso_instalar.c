/* Adaptador FATX do núcleo C89. Nenhuma E/S na thread que apresenta ou nas 4 e 5. */
#include <xtl.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include "iso_instalar.h"
#include "iso360.h"
#include "iso_conv.h"
#include "iso_selo.h"
#include "diario.h"
/* A leitura dos índices é antecipada, mas os bytes ficam em cache e não são
   relidos da origem na passagem sequencial. Cache imutável após iniciar(). */
#define MAX_META 16
typedef struct Meta {
    conv_u32 pos;
    unsigned n;
    conv_u8 *buf;
} Meta;
typedef struct Operacao {
    HANDLE pedir, pronto, thread;
    int parar, pendente, ok, escrita;
    conv_u32 pos;
    unsigned n;
    void *buf;
    void *dono;
    IsoSha sha;
    DWORD ms, hash_ms;
} Operacao;
typedef struct Instalar {
    HANDLE origem, tmp;
    Meta meta[MAX_META];
    unsigned nmeta;
    int fluxo;
    Operacao leitora, escritora;
} Instalar;
static volatile LONG sFase, sPorcento;
static int tamanho(void *ctx, int qual, conv_u32 *n) {
    Instalar *c = (Instalar *)ctx;
    LARGE_INTEGER t;
    if (!GetFileSizeEx(qual ? c->tmp : c->origem, &t) || t.QuadPart < 0 || t.QuadPart > ((__int64)0xffffffffU))
        return 0;
    *n = (conv_u32)t.QuadPart;
    return 1;
}
static int ler_direto(Instalar *c, int qual, conv_u32 pos, void *buf, unsigned n) {
    LARGE_INTEGER p;
    DWORD lidos = 0;
    HANDLE h = qual ? c->tmp : c->origem;
    p.QuadPart = pos;
    return SetFilePointerEx(h, p, NULL, FILE_BEGIN) && ReadFile(h, buf, n, &lidos, NULL) && lidos == n;
}
static int ler(void *ctx, int qual, conv_u32 pos, void *buf, unsigned n) {
    Instalar *c = (Instalar *)ctx;
    unsigned i;
    conv_u8 *dest = (conv_u8 *)buf;
    if (qual)
        return ler_direto(c, qual, pos, buf, n);
    if (!c->fluxo) {
        Meta *m;
        for (i = 0; i < c->nmeta; i++)
            if (c->meta[i].pos == pos && c->meta[i].n == n) {
                memcpy(buf, c->meta[i].buf, n);
                return 1;
            }
        if (c->nmeta == MAX_META || n > 1024U * 1024U)
            return 0;
        m = c->meta + c->nmeta;
        m->buf = (conv_u8 *)malloc(n);
        if (!m->buf)
            return 0;
        m->pos = pos;
        m->n = n;
        c->nmeta++;
        if (!ler_direto(c, 0, pos, m->buf, n))
            return 0;
        memcpy(buf, m->buf, n);
        return 1;
    }
    while (n) {
        unsigned z = n;
        const Meta *achou = NULL;
        for (i = 0; i < c->nmeta; i++) {
            const Meta *m = c->meta + i;
            if (pos >= m->pos && pos - m->pos < m->n) {
                achou = m;
                z = m->n - (pos - m->pos) < z ? m->n - (pos - m->pos) : z;
                break;
            }
            if (m->pos > pos && m->pos - pos < z)
                z = m->pos - pos;
        }
        if (achou)
            memcpy(dest, achou->buf + pos - achou->pos, z);
        else if (!ler_direto(c, 0, pos, dest, z))
            return 0;
        pos += z;
        dest += z;
        n -= z;
    }
    return 1;
}
static int gravar(void *ctx, conv_u32 pos, const void *buf, unsigned n) {
    Instalar *c = (Instalar *)ctx;
    LARGE_INTEGER p;
    DWORD escritos = 0;
    p.QuadPart = pos;
    return SetFilePointerEx(c->tmp, p, NULL, FILE_BEGIN) && WriteFile(c->tmp, buf, n, &escritos, NULL) && escritos == n;
}
/* Chamadas síncronas em handles distintos, sobrepostas por duas threads:
   não dependem de extensões OVERLAPPED de arquivo ausentes em alguns XDKs. */
static DWORD WINAPI trabalhar(LPVOID p) {
    Operacao *o = (Operacao *)p;
    for (;;) {
        DWORD inicio;
        if (WaitForSingleObject(o->pedir, INFINITE) != WAIT_OBJECT_0)
            return 1;
        if (o->parar)
            return 0;
        inicio = GetTickCount();
        if (o->escrita) {
            IsoSha_Atualizar(&o->sha, o->buf, o->n);
            o->hash_ms += GetTickCount() - inicio;
            inicio = GetTickCount();
            o->ok = gravar(o->dono, o->pos, o->buf, o->n);
        } else {
            o->ok = ler(o->dono, 0, o->pos, o->buf, o->n);
        }
        o->ms += GetTickCount() - inicio;
        SetEvent(o->pronto);
    }
}
static int abrir_operacao(Instalar *c, Operacao *o, int escrita) {
    o->dono = c;
    o->escrita = escrita;
    IsoSha_Iniciar(&o->sha);
    o->pedir = CreateEvent(NULL, FALSE, FALSE, NULL);
    o->pronto = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!o->pedir || !o->pronto)
        return 0;
    o->thread = CreateThread(NULL, 64 * 1024, trabalhar, o, CREATE_SUSPENDED, NULL);
    if (!o->thread)
        return 0;
    /* A 1 é irmã da main (0); a 3 é irmã da conversão (2). O hash pesado
       da saída fica no outro núcleo físico, enquanto a 3 espera E/S. */
    XSetThreadProcessor(o->thread, escrita ? 1 : 3);
    ResumeThread(o->thread);
    return 1;
}
static int esperar(Operacao *o) {
    int ok;
    if (!o->pendente)
        return 1;
    ok = WaitForSingleObject(o->pronto, INFINITE) == WAIT_OBJECT_0 && o->ok;
    o->pendente = 0;
    return ok;
}
static int pedir(Operacao *o, conv_u32 pos, void *buf, unsigned n) {
    if (o->pendente)
        return 0;
    o->pos = pos;
    o->buf = buf;
    o->n = n;
    o->pendente = 1;
    if (!SetEvent(o->pedir)) {
        o->pendente = 0;
        return 0;
    }
    return 1;
}
static int antecipar(void *ctx, conv_u32 pos, void *buf, unsigned n) {
    return pedir(&((Instalar *)ctx)->leitora, pos, buf, n);
}
static int esperar_leitura(void *ctx) { return esperar(&((Instalar *)ctx)->leitora); }
static int enviar(void *ctx, conv_u32 pos, const void *buf, unsigned n) {
    return pedir(&((Instalar *)ctx)->escritora, pos, (void *)buf, n);
}
static int esperar_gravacao(void *ctx) { return esperar(&((Instalar *)ctx)->escritora); }
static void hash_saida(void *ctx, conv_u8 *hash) { IsoSha_Final(&((Instalar *)ctx)->escritora.sha, hash); }
static void *alocar(unsigned n) { return _aligned_malloc(n, 128); }
static void liberar(void *p) { _aligned_free(p); }
static conv_u32 relogio(void) { return (conv_u32)GetTickCount(); }
static void fechar_operacao(Operacao *o) {
    if (o->thread) {
        esperar(o);
        o->parar = 1;
        SetEvent(o->pedir);
        WaitForSingleObject(o->thread, INFINITE);
        CloseHandle(o->thread);
    }
    if (o->pedir)
        CloseHandle(o->pedir);
    if (o->pronto)
        CloseHandle(o->pronto);
}
static int espaco(void *ctx) {
    ULARGE_INTEGER livre, total, disponivel;
    (void)ctx;
    return GetDiskFreeSpaceEx("game:\\", &livre, &total, &disponivel) &&
           livre.QuadPart >= ((unsigned __int64)3221225472U);
}
static int iniciar(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    if (GetFileAttributes("game:\\" ISO_NOME) != (DWORD)-1)
        return 0;
    c->tmp =
        CreateFile("game:\\" IC_TEMP, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (c->tmp == INVALID_HANDLE_VALUE)
        return 0;
    c->fluxo = 1;
    return 1;
}
static int reabrir(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    BOOL ok = FlushFileBuffers(c->tmp), fechou = CloseHandle(c->tmp);
    c->tmp = INVALID_HANDLE_VALUE;
    if (!ok || !fechou)
        return 0;
    c->tmp =
        CreateFile("game:\\" IC_TEMP, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    return c->tmp != INVALID_HANDLE_VALUE;
}
/* Compara literalmente todos os índices antecipados, inclusive a tabela de
   65.201 entradas do AFS2, mesmo onde a amostragem de lacunas não passa. */
static int conferir_indices(void *ctx, conv_u32 *relidos) {
    Instalar *c = (Instalar *)ctx;
    conv_u8 buf[2048];
    unsigned i;
    for (i = 0; i < c->nmeta; i++) {
        const Meta *m = c->meta + i;
        unsigned p = 0;
        while (p < m->n) {
            unsigned z = m->n - p > sizeof(buf) ? sizeof(buf) : m->n - p;
            if (!ler_direto(c, 1, m->pos + p, buf, z) || memcmp(buf, m->buf + p, z))
                return 0;
            p += z;
            *relidos += z;
        }
    }
    return 1;
}
static int publicar(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    BOOL ok = CloseHandle(c->tmp);
    c->tmp = INVALID_HANDLE_VALUE;
    /* MoveFile, sem MOVEFILE_REPLACE_EXISTING: recusa também um destino que
   * apareceu depois. */
    return ok && MoveFile("game:\\" IC_TEMP, "game:\\" ISO_NOME);
}
static void descartar(void *ctx) {
    Instalar *c = (Instalar *)ctx;
    if (c->tmp != INVALID_HANDLE_VALUE) {
        CloseHandle(c->tmp);
        c->tmp = INVALID_HANDLE_VALUE;
    }
    if (!DeleteFile("game:\\" IC_TEMP))
        Diario_Escrever("conversão: temporário não removido, erro %u", (unsigned)GetLastError());
}
static void progresso(void *ctx, int fase, conv_u32 p, conv_u32 n) {
    static int ultima = -1;
    static DWORD inicio;
    unsigned porcento = (unsigned)((double)p * 100.0 / (n ? n : 1));
    (void)ctx;
    if (fase != ultima) {
        DWORD agora = GetTickCount();
        if (ultima >= 0)
            Diario_Escrever("conversão: fase %d em %u ms", ultima, (unsigned)(agora - inicio));
        ultima = fase;
        inicio = agora;
    }
    InterlockedExchange(&sFase, fase);
    InterlockedExchange(&sPorcento, (LONG)porcento);
}
void IsoInstalar_Progresso(int *fase, unsigned *porcento) {
    *fase = (int)InterlockedCompareExchange(&sFase, 0, 0);
    *porcento = (unsigned)InterlockedCompareExchange(&sPorcento, 0, 0);
}
int IsoInstalar_Limpar(void) {
    DWORD erro;
    if (DeleteFile("game:\\" IC_TEMP))
        return 1;
    erro = GetLastError();
    return erro == ERROR_FILE_NOT_FOUND || erro == ERROR_PATH_NOT_FOUND;
}
int IsoInstalar_Executar(const char *origem) {
    Instalar c;
    IsoConvIO io;
    int r;
    unsigned i;
    char erro[240], hash[65];
    DWORD inicio = GetTickCount();
    memset(&c, 0, sizeof(c));
    c.tmp = INVALID_HANDLE_VALUE;
    c.origem = CreateFile(origem, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (c.origem == INVALID_HANDLE_VALUE)
        return IC_ORIGEM;
    memset(&io, 0, sizeof(io));
    io.ctx = &c;
    io.tamanho = tamanho;
    io.ler = ler;
    io.antecipar = antecipar;
    io.esperar_leitura = esperar_leitura;
    io.enviar = enviar;
    io.esperar_gravacao = esperar_gravacao;
    io.hash_saida = hash_saida;
    io.relogio = relogio;
    io.alocar = alocar;
    io.liberar = liberar;
    io.espaco = espaco;
    io.iniciar = iniciar;
    io.reabrir = reabrir;
    io.conferir_indices = conferir_indices;
    io.publicar = publicar;
    io.descartar = descartar;
    io.progresso = progresso;
    progresso(&c, IC_IDENTIFICAR, 0, 1);
    if (!abrir_operacao(&c, &c.leitora, 0) || !abrir_operacao(&c, &c.escritora, 1)) {
        r = IC_IO;
        strcpy(erro, "falha ao criar as threads de E/S");
    } else {
        r = IsoConv_Executar(&io, sIsoSelo, erro, sizeof(erro));
    }
    fechar_operacao(&c.leitora);
    fechar_operacao(&c.escritora);
    for (i = 0; i < c.nmeta; i++)
        free(c.meta[i].buf);
    if (r == IC_OK) {
        for (i = 0; i < 32; i++)
            sprintf(hash + i * 2, "%02x", (unsigned)io.sha_saida[i]);
        Diario_Escrever("conversão: SHA256 saída %s, relidos %u bytes", hash, (unsigned)io.relidos);
    }
    Diario_Escrever("conversão: hash origem %u ms, conversores %u ms", (unsigned)io.hash_origem_ms,
                    (unsigned)io.conversao_ms);
    Diario_Escrever("conversão: leitura %u ms, gravação %u ms, hash saída %u ms", (unsigned)c.leitora.ms,
                    (unsigned)c.escritora.ms, (unsigned)c.escritora.hash_ms);
    CloseHandle(c.origem);
    Diario_Escrever("conversão: resultado %d, %s, total %u ms", r, erro, (unsigned)(GetTickCount() - inicio));
    return r;
}
