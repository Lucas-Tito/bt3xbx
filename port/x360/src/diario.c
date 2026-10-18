/*
 * O diário (game:\bt3xbx.log): as linhas vão para um anel em memória, e uma thread de baixa prioridade, noutro núcleo,
 * grava em lote. Quem chama Diario_Escrever (a thread do jogo, a de desenho, a main) formata a linha, copia para o
 * anel sob uma trava curta e volta: nenhum I/O de disco na thread de quem escreve. Antes, cada linha era um WriteFile
 * mais um FlushFileBuffers dentro da thread do jogo, e no FATX do HD um flush custa de 30 a 40 ms (F5/08.md, seção "Os
 * logs e os engasgos": 8 a 11 linhas a cada 60 quadros eram os picos de 210 a 420 ms).
 *
 * O que a regra do CLAUDE.md ("numa queda o que já foi escrito tem de estar no disco") exige continua valendo:
 *   - a thread escritora grava a cada LOTE_MS (WriteFile, que vai para o cache do sistema de arquivos) e faz o
 *     FlushFileBuffers no máximo a cada FLUSH_MS, só se houve linha nova: no pior caso um travamento do console
 *     (sem exceção) perde as últimas centenas de ms, não a execução;
 *   - Diario_Descarregar é síncrono (grava o anel e faz o flush antes de voltar): o filtro de exceção a chama logo
 *     depois da linha CRASH, e Port_Sair ao sair; Diario_Fechar também;
 *   - BT3_LOG_SINCRONO=1 no ambiente.txt volta ao jeito antigo (gravar e dar flush na própria chamada), para medir uma
 *     coisa contra a outra com a mesma build; sem a thread (CreateThread falhou) o diário também cai nesse jeito.
 * Anel cheio: a linha é descartada e contada, e a escritora grava "[... N linhas perdidas ...]" (o jogo nunca espera
 * pelo disco). Seguro entre threads. Chamável de C e de C++. Nada aqui usa getenv: quem lê o ambiente (posix360.c)
 * chama Diario_Configurar.
 */
#include <xtl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "diario.h"

#define ANEL_TAM (256 * 1024) /* potência de 2: os contadores dão a volta em 2^32 sem quebrar a conta */
#define LINHA_MAX 1024
#define LOTE_MS 50             /* de quanto em quanto a escritora olha o anel */
#define FLUSH_MS_PADRAO 250    /* idade máxima de uma linha gravada que ainda não passou pelo FlushFileBuffers */
#define ESTATISTICA_MS 60000   /* uma linha de contas por minuto, se houve linha nova */
#define ESPERA_ARQUIVO_MS 3000 /* quanto quem grava espera outra gravação em curso antes de gravar mesmo assim */
#define PROCESSADOR 1          /* thread de hardware da escritora: outro núcleo que o do jogo (2), longe do XAudio2 (4 e 5) */

static HANDLE sArquivo = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION sTrava; /* só o anel e os contadores dele: nunca I/O de disco aqui dentro */
static int sTravaPronta = 0;

static char sAnel[ANEL_TAM];
static unsigned sCabeca, sCauda;  /* contadores que só crescem; a posição no anel é o resto por ANEL_TAM. Sob sTrava */
static unsigned sPerdidas;        /* linhas descartadas desde a última gravação do aviso. Sob sTrava */
static unsigned sPerdidasTotal, sLinhas, sBytes, sAnelMax, sLinhasNaUltima; /* contas. Sob sTrava */

static volatile LONG sArqOcupado = 0; /* uma gravação de cada vez no arquivo (a ordem das linhas) */
static HANDLE sThread = NULL;
static volatile LONG sParar = 0;
static volatile LONG sSincrono = 0;
static volatile LONG sFlushMs = FLUSH_MS_PADRAO;
static void (*volatile sGancho)(void) = NULL;

/* as contas de tempo (microssegundos); escritas só por quem está gravando, lidas na linha de estatística */
static LARGE_INTEGER sFreq;
static unsigned sLotes, sDescargas, sPiorLoteUs, sPiorDescargaUs;

static unsigned UsDesde(const LARGE_INTEGER *t0) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (unsigned)((t.QuadPart - t0->QuadPart) * 1000000 / sFreq.QuadPart);
}

/* Uma gravação de cada vez. Se outra passar de ESPERA_ARQUIVO_MS (disco parado), grava mesmo assim: no filtro de
   exceção a linha CRASH vale mais que a ordem perfeita (o pior caso é uma linha repetida). Devolve se tomou a vez. */
static int ArquivoTomar(void) {
    DWORD ini = GetTickCount();
    while (InterlockedCompareExchange(&sArqOcupado, 1, 0) != 0) {
        if (GetTickCount() - ini >= ESPERA_ARQUIVO_MS) {
            return 0;
        }
        Sleep(1);
    }
    return 1;
}

static void ArquivoSoltar(int tomou) {
    if (tomou) {
        InterlockedExchange(&sArqOcupado, 0);
    }
}

/* Copia a linha para o anel (ou conta que a perdeu). O único trabalho de quem escreve no diário além de formatar. */
static void Empurrar(const char *linha, int n) {
    unsigned pos, parte, usado;
    EnterCriticalSection(&sTrava);
    if ((unsigned)n > ANEL_TAM - (sCabeca - sCauda)) {
        sPerdidas++;
        sPerdidasTotal++;
    } else {
        pos = sCabeca & (ANEL_TAM - 1);
        parte = ANEL_TAM - pos;
        if (parte > (unsigned)n) {
            parte = (unsigned)n;
        }
        memcpy(sAnel + pos, linha, parte);
        if ((unsigned)n > parte) {
            memcpy(sAnel, linha + parte, (unsigned)n - parte);
        }
        sCabeca += (unsigned)n;
        sLinhas++;
        sBytes += (unsigned)n;
        usado = sCabeca - sCauda;
        if (usado > sAnelMax) {
            sAnelMax = usado;
        }
    }
    LeaveCriticalSection(&sTrava);
}

/* Grava no arquivo (WriteFile, sem flush) tudo o que o anel tem. Devolve se gravou algo. A thread escritora, o
   Diario_Descarregar e o jeito síncrono chamam isto; o anel só é lido fora da trava porque quem escreve só mexe no
   espaço livre, e a cauda só avança depois da gravação. */
static int Drenar(void) {
    unsigned ca, ta, perdidas, n, pos, parte;
    DWORD escritos = 0;
    LARGE_INTEGER t0;
    unsigned us;
    int tomou;
    if (sArquivo == INVALID_HANDLE_VALUE) {
        return 0;
    }
    tomou = ArquivoTomar();
    EnterCriticalSection(&sTrava);
    ca = sCabeca;
    ta = sCauda;
    perdidas = sPerdidas;
    sPerdidas = 0;
    LeaveCriticalSection(&sTrava);
    n = ca - ta;
    if (n == 0 && perdidas == 0) {
        ArquivoSoltar(tomou);
        return 0;
    }
    QueryPerformanceCounter(&t0);
    if (perdidas != 0) {
        char aviso[96];
        int na = _snprintf(aviso, sizeof(aviso) - 1, "[... %u linhas perdidas: o anel do diário estava cheio ...]\n",
                           perdidas);
        if (na < 0 || na > (int)sizeof(aviso) - 1) {
            na = (int)sizeof(aviso) - 1;
        }
        WriteFile(sArquivo, aviso, (DWORD)na, &escritos, NULL);
    }
    if (n != 0) {
        pos = ta & (ANEL_TAM - 1);
        parte = ANEL_TAM - pos;
        if (parte > n) {
            parte = n;
        }
        WriteFile(sArquivo, sAnel + pos, (DWORD)parte, &escritos, NULL);
        if (n > parte) {
            WriteFile(sArquivo, sAnel, (DWORD)(n - parte), &escritos, NULL);
        }
    }
    us = UsDesde(&t0);
    sLotes++;
    if (us > sPiorLoteUs) {
        sPiorLoteUs = us;
    }
    EnterCriticalSection(&sTrava);
    if ((int)(ca - sCauda) > 0) { /* (só para a frente: duas gravações simultâneas, depois do tempo de espera) */
        sCauda = ca;
    }
    LeaveCriticalSection(&sTrava);
    ArquivoSoltar(tomou);
    return 1;
}

/* O FlushFileBuffers: o que foi gravado vai do cache do sistema de arquivos para o disco. Sem a vez do arquivo: o
   WriteFile de outra thread espera o flush no próprio sistema de arquivos, e o filtro de exceção não fica preso. */
static void Comitar(void) {
    LARGE_INTEGER t0;
    unsigned us;
    if (sArquivo == INVALID_HANDLE_VALUE) {
        return;
    }
    QueryPerformanceCounter(&t0);
    FlushFileBuffers(sArquivo);
    us = UsDesde(&t0);
    sDescargas++;
    if (us > sPiorDescargaUs) {
        sPiorDescargaUs = us;
    }
}

void Diario_Escrever(const char *formato, ...) {
    char linha[LINHA_MAX];
    va_list args;
    int n;

    if (sArquivo == INVALID_HANDLE_VALUE || !sTravaPronta) {
        return;
    }
    va_start(args, formato);
    n = _vsnprintf(linha, sizeof(linha) - 2, formato, args);
    va_end(args);
    /* Truncando, o _vsnprintf devolve -1 e NÃO termina a string. */
    if (n < 0 || n > (int)sizeof(linha) - 2) {
        n = (int)sizeof(linha) - 2;
    }
    linha[n++] = '\n';

    Empurrar(linha, n);
    if (sThread == NULL || sSincrono) { /* o jeito antigo: a linha no disco antes de voltar */
        Drenar();
        Comitar();
    }
}

/* A linha de contas: o que a escritora fez, para a medição no console (o pior flush é o custo real do FATX) */
static void Estatistica(const char *quando) {
    unsigned linhas, kb, max, perdidas;
    EnterCriticalSection(&sTrava);
    linhas = sLinhas;
    kb = sBytes >> 10;
    max = sAnelMax >> 10;
    perdidas = sPerdidasTotal;
    LeaveCriticalSection(&sTrava);
    Diario_Escrever("diario: %s: %u linhas (%u KB), %u gravações (a pior %u us), %u flushes (o pior %u us), anel até %u KB "
                    "de %u, %u linhas perdidas", quando, linhas, kb, sLotes, sPiorLoteUs, sDescargas, sPiorDescargaUs,
                    max, ANEL_TAM >> 10, perdidas);
    EnterCriticalSection(&sTrava);
    sLinhasNaUltima = sLinhas;
    LeaveCriticalSection(&sTrava);
}

static DWORD WINAPI Escritora(LPVOID arg) {
    DWORD ultimoFlush = GetTickCount(), ultimaEstatistica = ultimoFlush, agora;
    int sujo = 0, nova;
    (void)arg;
    while (!sParar) {
        Sleep(LOTE_MS);
        if (Drenar()) {
            sujo = 1;
        }
        if (sGancho != NULL) {
            sGancho(); /* (posix360.c: o que o jogo imprimiu em game:\saida.txt vai ao disco aqui, fora da thread dele) */
        }
        agora = GetTickCount();
        if (sujo && agora - ultimoFlush >= (DWORD)sFlushMs) {
            Comitar();
            sujo = 0;
            ultimoFlush = GetTickCount();
        }
        if (agora - ultimaEstatistica >= ESTATISTICA_MS) {
            ultimaEstatistica = agora;
            EnterCriticalSection(&sTrava);
            nova = sLinhas != sLinhasNaUltima;
            LeaveCriticalSection(&sTrava);
            if (nova) {
                Estatistica("parcial");
            }
        }
    }
    return 0;
}

void Diario_Abrir(const char *caminho) {
    if (!sTravaPronta) {
        InitializeCriticalSection(&sTrava);
        QueryPerformanceFrequency(&sFreq);
        sTravaPronta = 1;
    }
    sCabeca = sCauda = 0;
    sArquivo = CreateFile(caminho, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (sArquivo == INVALID_HANDLE_VALUE) {
        return;
    }
    InterlockedExchange(&sParar, 0);
    sThread = CreateThread(NULL, 64 * 1024, Escritora, NULL, CREATE_SUSPENDED, NULL);
    if (sThread != NULL) {
        XSetThreadProcessor(sThread, PROCESSADOR);
        SetThreadPriority(sThread, THREAD_PRIORITY_BELOW_NORMAL);
        ResumeThread(sThread);
        Diario_Escrever("diario: anel de %u KB, escritora na thread de hardware %d (lote de %u ms, flush a cada %u ms)",
                        ANEL_TAM >> 10, PROCESSADOR, LOTE_MS, (unsigned)sFlushMs);
    } else {
        Diario_Escrever("diario: sem a thread escritora (erro %u): gravação síncrona", (unsigned)GetLastError());
    }
}

void Diario_Configurar(int sincrono, int flushMs) {
    if (flushMs <= 0) {
        flushMs = FLUSH_MS_PADRAO;
    }
    if (flushMs > 60000) {
        flushMs = 60000;
    }
    InterlockedExchange(&sFlushMs, flushMs);
    InterlockedExchange(&sSincrono, sincrono != 0);
    Diario_Escrever("diario: %s, flush a cada %d ms", sincrono != 0 || sThread == NULL ? "síncrono (uma linha, um flush)"
                                                                                       : "em lote, por thread própria",
                    flushMs);
}

void Diario_Gancho(void (*funcao)(void)) {
    sGancho = funcao;
}

int Diario_Assincrono(void) {
    return sThread != NULL && !sSincrono && !sParar;
}

/* Síncrono: o que está no anel está no disco (com flush) quando isto volta. Para o filtro de exceção e a saída. */
void Diario_Descarregar(void) {
    if (sArquivo == INVALID_HANDLE_VALUE || !sTravaPronta) {
        return;
    }
    Drenar();
    Comitar();
}

void Diario_Fechar(void) {
    if (sArquivo == INVALID_HANDLE_VALUE) {
        return;
    }
    if (sThread != NULL) {
        InterlockedExchange(&sParar, 1);
        WaitForSingleObject(sThread, 2000);
        CloseHandle(sThread);
        sThread = NULL;
        Drenar(); /* (o anel vazio, para a linha de contas do fim caber mesmo depois de uma perda) */
        Estatistica("fim");
    }
    Drenar();
    Comitar();
    CloseHandle(sArquivo);
    sArquivo = INVALID_HANDLE_VALUE;
}
