/*
 * F0: medições de aritmética e memória no Xenon, para decidir como o port faz o ponto flutuante.
 *
 * O port de PC compila o jogo com -msoft-float e faz cada operação em software, igual ao PS2 sob o
 * PCSX2 (port/src/softfloat_ps2.c): soma e multiplicação truncadas, divisão da FPU arredondada ao mais
 * próximo, sem denormais, sem infinito. O MSVC do XDK não tem -msoft-float. As perguntas:
 *   1. A FPU do Xenon com soma/multiplicação em truncamento e divisão ao mais próximo dá o mesmo
 *      resultado? Em quantos casos não, e em quais (perto do denormal, perto do estouro)?
 *   2. Quanto custa cada operação em hardware e em software? E trocar o modo só para a divisão?
 *   3. Quanto custa lwbrx contra lwz, int -> float, e página de 4 KB contra 64 KB?
 *
 * fenv_access: sem ele o compilador dobra expressões de constantes ao mais próximo, sem passar pela FPU
 * (1.0f / 3.0f vira 3EAAAAAB no binário), e o teste do modo não testaria nada. Vem DEPOIS dos
 * headers do XDK: o xnamath.h tem inicializadores estáticos que precisam da dobra (C2099).
 */
#include <xtl.h>
#include <float.h>
#include <string.h>
#include "diario.h"
#include "f0/sonda.h"

#pragma fenv_access(on)

extern unsigned int __addsf3(unsigned int a, unsigned int b);
extern unsigned int __mulsf3(unsigned int a, unsigned int b);
extern unsigned int __divsf3(unsigned int a, unsigned int b);

/* softfloat_ps2.c referencia estas do vu0_a.c do jogo, que a sonda não usa. Se alguma for chamada,
   o resultado estaria errado em silêncio: melhor parar e dizer. */
static void NaoUsar(const char *nome) {
    Diario_Escrever("ERRO: %s chamada na sonda (stub)", nome);
    for (;;) {
    }
}
unsigned int RefVu0_ItofBits(int v, int fracBits) { (void)v; (void)fracBits; NaoUsar("RefVu0_ItofBits"); return 0; }
unsigned int RefVu0_SubBits(unsigned int a, unsigned int b) { (void)a; (void)b; NaoUsar("RefVu0_SubBits"); return 0; }
int RefVu0_LtBits(unsigned int a, unsigned int b) { (void)a; (void)b; NaoUsar("RefVu0_LtBits"); return 0; }

typedef union { unsigned int u; float f; } Bits;

/* ---------------------------------------------------------------- correção */

#define N_PARES 1000000
static unsigned int sA[N_PARES], sB[N_PARES];

static unsigned int sSemente = 0x12345678u;
static unsigned int Aleatorio(void) {
    sSemente ^= sSemente << 13;
    sSemente ^= sSemente >> 17;
    sSemente ^= sSemente << 5;
    return sSemente;
}

static unsigned int Float(unsigned int expoente) {
    return (Aleatorio() & 0x807FFFFFu) | (expoente << 23);
}

static unsigned int Entre(int lo, int hi) {
    int e = lo + (int)(Aleatorio() % (unsigned int)(hi - lo + 1));
    return (unsigned int)(e < 1 ? 1 : e > 254 ? 254 : e);
}

/* Operandos comuns: expoente em [lo, hi] dos dois lados. */
static void GerarComuns(int lo, int hi) {
    int i;
    for (i = 0; i < N_PARES; i++) {
        sA[i] = Float(Entre(lo, hi));
        sB[i] = Float(Entre(lo, hi));
    }
}

/* Pares cujo RESULTADO cai com expoente perto de `alvo` (0 = denormal, 255 = estouro), para cada
   operação: na multiplicação ea + eb - 127, na divisão ea - eb + 127, na soma cancelamento de dois
   números quase iguais de sinais opostos. */
enum { OP_SOMA, OP_MUL, OP_DIV };
static void GerarPerto(int op, int alvo) {
    int i;
    for (i = 0; i < N_PARES; i++) {
        int ea = (int)Entre(64, 190), et = alvo - 3 + (int)(Aleatorio() % 7u);
        sA[i] = Float((unsigned int)ea);
        if (op == OP_MUL) {
            sB[i] = Float(Entre(et - ea + 127, et - ea + 127));
        } else if (op == OP_DIV) {
            sB[i] = Float(Entre(ea - et + 127, ea - et + 127));
        } else {
            /* a e -a com a mantissa mexida nos bits de baixo: a soma cai muitos expoentes abaixo */
            unsigned int e = Entre(1, alvo + 24);
            sA[i] = Float(e);
            sB[i] = (sA[i] ^ 0x80000000u) ^ (Aleatorio() & 0xFFu);
        }
    }
}

/* Cada operação passa os bits por uma union (load-hit-store). Serve para conferir o resultado, não
   para medir tempo: isso é a parte de custo, mais abaixo. */
static unsigned int HwAdd(unsigned int a, unsigned int b) { Bits x, y, r; x.u = a; y.u = b; r.f = x.f + y.f; return r.u; }
static unsigned int HwMul(unsigned int a, unsigned int b) { Bits x, y, r; x.u = a; y.u = b; r.f = x.f * y.f; return r.u; }
static unsigned int HwDiv(unsigned int a, unsigned int b) { Bits x, y, r; x.u = a; y.u = b; r.f = x.f / y.f; return r.u; }

/* A soma do PS2 guarda só um bit do operando menor abaixo da última casa do maior (fpu_add em
   softfloat_ps2_inl.h). Emulada com máscara inteira antes da soma de hardware. */
static void Mascarar(unsigned int *pa, unsigned int *pb) {
    unsigned int a = *pa, b = *pb;
    int d = (int)((a >> 23) & 0xFF) - (int)((b >> 23) & 0xFF);
    if (d >= 25) b &= 0x80000000u;
    else if (d > 0) b &= 0xFFFFFFFFu << (d - 1);
    else if (d <= -25) a &= 0x80000000u;
    else if (d < 0) a &= 0xFFFFFFFFu << (-d - 1);
    *pa = a;
    *pb = b;
}
static unsigned int HwAddMascara(unsigned int a, unsigned int b) { Mascarar(&a, &b); return HwAdd(a, b); }

typedef unsigned int (*Op)(unsigned int, unsigned int);

static int EhEspecial(unsigned int x) {
    unsigned int e = (x >> 23) & 0xFF;
    return e == 0xFF || (e == 0 && (x & 0x7FFFFFu) != 0); /* inf/nan ou denormal (zero não conta) */
}

/* rc: modo da FPU durante a operação de hardware. */
static int Comparar(const char *nome, Op hw, Op sw, unsigned int rc) {
    int i, dif = 0, dif1ulp = 0, difEsp = 0;
    unsigned int exA = 0, exB = 0, exHw = 0, exSw = 0;
    unsigned int cw = _controlfp(0, 0);

    for (i = 0; i < N_PARES; i++) {
        unsigned int h, s;
        _controlfp(rc, _MCW_RC);
        h = hw(sA[i], sB[i]);
        _controlfp(cw, _MCW_RC);
        s = sw(sA[i], sB[i]);
        if (h != s) {
            dif++;
            if ((h ^ s) < 0x80000000u && (h - s == 1 || s - h == 1)) dif1ulp++;
            if (EhEspecial(h)) difEsp++;
            if (dif == 1) { exA = sA[i]; exB = sB[i]; exHw = h; exSw = s; }
        }
    }
    Diario_Escrever("  %-24s %s: difere em %7d de %d (%d por 1 ulp, %d dão inf/nan/denormal)", nome,
                    rc == _RC_CHOP ? "trunc" : "perto", dif, N_PARES, dif1ulp, difEsp);
    if (dif) {
        Diario_Escrever("    ex.: %08X op %08X -> hw %08X, ps2 %08X", exA, exB, exHw, exSw);
    }
    return dif;
}

/* O modelo que o port usaria: soma e multiplicação truncadas, divisão ao mais próximo. */
static void CompararModelo(const char *titulo) {
    Diario_Escrever(" %s", titulo);
    Comparar("soma", HwAdd, __addsf3, _RC_CHOP);
    Comparar("soma com máscara", HwAddMascara, __addsf3, _RC_CHOP);
    Comparar("multiplicação", HwMul, __mulsf3, _RC_CHOP);
    Comparar("divisão", HwDiv, __divsf3, _RC_NEAR);
}

/* Efeito direto dos modos de denormal, com volatile para nada ser dobrado na compilação. */
static void TestarDenormal(void) {
    static const struct { unsigned int dn; const char *nome; } kModos[] = {
        {_DN_SAVE, "mantém"},
        {_DN_FLUSH, "zera tudo"},
        {_DN_FLUSH_OPERANDS_SAVE_RESULTS, "zera entrada"},
        {_DN_SAVE_OPERANDS_FLUSH_RESULTS, "zera saída"},
    };
    unsigned int cw = _controlfp(0, 0);
    int i;

    for (i = 0; i < 4; i++) {
        volatile Bits menor, zero, minimo, meio, r1, r2;
        menor.u = 0x00000001u; /* menor denormal */
        zero.u = 0;
        minimo.u = 0x00800000u; /* menor normal */
        meio.f = 0.5f;
        _controlfp(kModos[i].dn, _MCW_DN);
        r1.f = menor.f + zero.f;  /* denormal na entrada */
        r2.f = minimo.f * meio.f; /* denormal na saída: 00400000 se mantém, 0 se zera */
        _controlfp(cw, _MCW_DN);
        Diario_Escrever("  denormal %-13s controlfp=%08X  1e-45+0=%08X  min*0.5=%08X", kModos[i].nome,
                        kModos[i].dn, r1.u, r2.u);
    }
}

/* ---------------------------------------------------------------- custo */

static double Agora(void) {
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (double)c.QuadPart / (double)f.QuadPart;
}

/* Bloco que cabe na L1/L2 (3 x 16 KB), repetido. Só lfs/stfs e a operação: nada de union. */
#define BLOCO 4096
#define VOLTAS 1000
static float sFa[BLOCO], sFb[BLOCO], sFo[BLOCO];
static unsigned int sUa[BLOCO], sUb[BLOCO], sUo[BLOCO];
static volatile float sAfundaF;
static volatile unsigned int sAfundaU;

static double PorOperacao(double t) { return t * 1e9 / ((double)BLOCO * VOLTAS); }

static void CustoHardware(void) {
    int v, i;
    double t0, t;
    float acc;

    _controlfp(_RC_CHOP, _MCW_RC);

    t0 = Agora();
    for (v = 0; v < VOLTAS; v++) for (i = 0; i < BLOCO; i++) sFo[i] = sFa[i] + sFb[i];
    t = Agora() - t0;
    Diario_Escrever("  hw soma (vazão)             %6.2f ns", PorOperacao(t));

    t0 = Agora();
    for (v = 0; v < VOLTAS; v++) for (i = 0; i < BLOCO; i++) sFo[i] = sFa[i] * sFb[i];
    t = Agora() - t0;
    Diario_Escrever("  hw multiplicação (vazão)    %6.2f ns", PorOperacao(t));

    t0 = Agora();
    for (v = 0; v < VOLTAS; v++) for (i = 0; i < BLOCO; i++) sFo[i] = sFa[i] / sFb[i];
    t = Agora() - t0;
    Diario_Escrever("  hw divisão (vazão)          %6.2f ns", PorOperacao(t));

    acc = 0.0f;
    t0 = Agora();
    for (v = 0; v < VOLTAS; v++) for (i = 0; i < BLOCO; i++) acc = acc * sFa[i] + sFb[i];
    t = Agora() - t0;
    sAfundaF = acc;
    Diario_Escrever("  hw mul+soma em cadeia       %6.2f ns (latência)", PorOperacao(t));

    /* divisão ao mais próximo no meio de código truncado: troca o modo a cada divisão */
    t0 = Agora();
    for (v = 0; v < VOLTAS / 10; v++) {
        for (i = 0; i < BLOCO; i++) {
            _controlfp(_RC_NEAR, _MCW_RC);
            sFo[i] = sFa[i] / sFb[i];
            _controlfp(_RC_CHOP, _MCW_RC);
        }
    }
    t = Agora() - t0;
    Diario_Escrever("  hw divisão trocando o modo  %6.2f ns", PorOperacao(t) * 10);

    /* soma com a máscara do PS2: exige ver os bits, ou seja, passar pela memória */
    t0 = Agora();
    for (v = 0; v < VOLTAS / 10; v++) {
        for (i = 0; i < BLOCO; i++) {
            unsigned int a = sUa[i], b = sUb[i];
            Bits x, y;
            Mascarar(&a, &b);
            x.u = a;
            y.u = b;
            sFo[i] = x.f + y.f;
        }
    }
    t = Agora() - t0;
    Diario_Escrever("  hw soma com máscara         %6.2f ns", PorOperacao(t) * 10);

    _controlfp(_RC_NEAR, _MCW_RC);
}

static void CustoSoftware(void) {
    int v, i;
    double t0, t;

    t0 = Agora();
    for (v = 0; v < VOLTAS / 10; v++) for (i = 0; i < BLOCO; i++) sUo[i] = __addsf3(sUa[i], sUb[i]);
    t = Agora() - t0;
    Diario_Escrever("  ps2 soma (software)         %6.2f ns", PorOperacao(t) * 10);

    t0 = Agora();
    for (v = 0; v < VOLTAS / 10; v++) for (i = 0; i < BLOCO; i++) sUo[i] = __mulsf3(sUa[i], sUb[i]);
    t = Agora() - t0;
    Diario_Escrever("  ps2 multiplicação (software)%6.2f ns", PorOperacao(t) * 10);

    t0 = Agora();
    for (v = 0; v < VOLTAS / 10; v++) for (i = 0; i < BLOCO; i++) sUo[i] = __divsf3(sUa[i], sUb[i]);
    t = Agora() - t0;
    Diario_Escrever("  ps2 divisão (software)      %6.2f ns", PorOperacao(t) * 10);
}

int Sonda_Float(void) {
    unsigned int cw = _controlfp(0, 0);
    volatile Bits um, tres, r;
    int i;

    Diario_Escrever("== aritmética (controlfp inicial=%08X)", cw);
    um.f = 1.0f;
    tres.f = 3.0f;
    r.f = um.f / tres.f;
    Diario_Escrever(" 1/3 perto=%08X (esperado 3EAAAAAB)", r.u);
    _controlfp(_RC_CHOP, _MCW_RC);
    r.f = um.f / tres.f;
    _controlfp(cw, _MCW_RC);
    Diario_Escrever(" 1/3 truncado=%08X (esperado 3EAAAAAA se o modo pegou)", r.u);
    TestarDenormal();

    GerarComuns(100, 154);
    CompararModelo("-- números comuns (expoente 100..154)");
    Diario_Escrever(" -- para comparar: tudo ao mais próximo");
    Comparar("soma", HwAdd, __addsf3, _RC_NEAR);
    Comparar("multiplicação", HwMul, __mulsf3, _RC_NEAR);
    Comparar("divisão truncada", HwDiv, __divsf3, _RC_CHOP);

    _controlfp(_DN_FLUSH, _MCW_DN);
    Diario_Escrever("-- resultado perto do denormal, denormais zerados");
    GerarPerto(OP_SOMA, 0); Comparar("soma com cancelamento", HwAddMascara, __addsf3, _RC_CHOP);
    GerarPerto(OP_MUL, 0);  Comparar("multiplicação", HwMul, __mulsf3, _RC_CHOP);
    GerarPerto(OP_DIV, 0);  Comparar("divisão", HwDiv, __divsf3, _RC_NEAR);
    Diario_Escrever("-- resultado perto do estouro");
    GerarPerto(OP_MUL, 255); Comparar("multiplicação", HwMul, __mulsf3, _RC_CHOP);
    GerarPerto(OP_DIV, 255); Comparar("divisão", HwDiv, __divsf3, _RC_NEAR);
    _controlfp(_DN_SAVE, _MCW_DN);
    Diario_Escrever("-- o mesmo, denormais mantidos");
    GerarPerto(OP_SOMA, 0); Comparar("soma com cancelamento", HwAddMascara, __addsf3, _RC_CHOP);
    GerarPerto(OP_MUL, 0);  Comparar("multiplicação", HwMul, __mulsf3, _RC_CHOP);
    _controlfp(cw, 0xFFFFFFFFu);

    Diario_Escrever("-- custo por operação (bloco de 16 KB na cache)");
    for (i = 0; i < BLOCO; i++) {
        Bits x, y;
        sUa[i] = x.u = Float(Entre(100, 154));
        sUb[i] = y.u = Float(Entre(100, 154));
        sFa[i] = x.f;
        sFb[i] = y.f;
    }
    CustoHardware();
    CustoSoftware();
    sAfundaU = sUo[BLOCO - 1];
    sAfundaF = sFo[BLOCO - 1];
    _controlfp(cw, 0xFFFFFFFFu);
    return 0;
}

/* ---------------------------------------------------------------- memória */

static void LerBloco(void) {
    enum { PALAVRAS = 4096, VEZES = 4000 };
    static unsigned int buf[PALAVRAS];
    unsigned int i, v, s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    double t0, t1, t2, t3;

    for (i = 0; i < PALAVRAS; i++) buf[i] = i * 2654435761u;

    /* mesmo desenrolamento e 4 acumuladores nos dois: a diferença tem de ser só lwz contra lwbrx */
    t0 = Agora();
    for (v = 0; v < VEZES; v++) {
        for (i = 0; i < PALAVRAS; i += 4) {
            s0 += buf[i]; s1 += buf[i + 1]; s2 += buf[i + 2]; s3 += buf[i + 3];
        }
    }
    t1 = Agora();
    for (v = 0; v < VEZES; v++) {
        for (i = 0; i < PALAVRAS; i += 4) {
            s0 += __loadwordbytereverse(0, &buf[i]);
            s1 += __loadwordbytereverse(4, &buf[i]);
            s2 += __loadwordbytereverse(8, &buf[i]);
            s3 += __loadwordbytereverse(12, &buf[i]);
        }
    }
    t2 = Agora();
    for (v = 0; v < VEZES / 4; v++) {
        for (i = 0; i < PALAVRAS; i++) sFo[i & (BLOCO - 1)] = (float)(int)buf[i];
    }
    t3 = Agora();
    sAfundaU = s0 ^ s1 ^ s2 ^ s3;
    Diario_Escrever(" lwz         %6.2f ns por palavra (bloco de 16 KB na cache)", (t1 - t0) * 1e9 / ((double)PALAVRAS * VEZES));
    Diario_Escrever(" lwbrx       %6.2f ns por palavra", (t2 - t1) * 1e9 / ((double)PALAVRAS * VEZES));
    Diario_Escrever(" int->float  %6.2f ns por palavra (cada uma num float independente)", (t3 - t2) * 1e9 / ((double)PALAVRAS * VEZES / 4));
}

/* Leitura aleatória em 32 MB: mostra o custo de TLB das páginas de 4 KB (a faixa abaixo de 0x40000000
   só aceita essas, e é nela que fica a RAM do PS2 em endereço fixo) contra as de 64 KB. */
static int LerAleatorio(DWORD tipoExtra, const char *nome) {
    enum { TAM = 32 * 1024 * 1024, LEITURAS = 4000000 };
    unsigned int *buf = (unsigned int *)VirtualAlloc(NULL, TAM, MEM_COMMIT | tipoExtra, PAGE_READWRITE);
    unsigned int i, s = 0, idx = 1;
    double t0, t;

    if (buf == NULL) {
        Diario_Escrever(" páginas de %s: sem memória (erro %u)", nome, (unsigned)GetLastError());
        return 1;
    }
    for (i = 0; i < TAM / 4; i += 1024) buf[i] = i;
    t0 = Agora();
    for (i = 0; i < LEITURAS; i++) {
        idx = idx * 1664525u + 1013904223u;
        s += buf[(idx >> 7) & (TAM / 4 - 1)];
    }
    t = Agora() - t0;
    sAfundaU = s;
    Diario_Escrever(" leitura aleatória, páginas de %s em %08X: %6.2f ns", nome, (unsigned)(DWORD)buf, t * 1e9 / LEITURAS);
    VirtualFree(buf, 0, MEM_RELEASE);
    return 0;
}

int Sonda_Memoria(void) {
    int falhas = 0;
    Diario_Escrever("== leitura de memória");
    LerBloco();
    falhas += LerAleatorio(0, "4 KB");
    falhas += LerAleatorio(MEM_LARGE_PAGES, "64 KB");
    return falhas;
}
