/* F5, decisão 29: rateio exclusivo da CPU, só na dona do dispositivo.
   Não insere comandos, cercas ou callbacks. O tempo de uma API inclui eventual
   espera interna; não distingue execução da CPU de bloqueio no driver.
   Incluído depois de sFreq. O teste nativo fornece o mesmo relógio por contrato.
   Só existe na build de perfil (-D BT3_PERFIL_CADEIA, build_f2.sh com BT3_PERFIL_CADEIA=1): mesmo desligadas, as marcas
   custavam ~1,8 ms no fim do quadro 7170 e criaram uma imagem longa (seção 35 do 08-gravacao.md). Na build normal os
   escopos são vazios e o código gerado é o de antes do diagnóstico; BT3_CADEIA_CPU é ignorada. */
enum { CA_QUADRO, CA_COMUM, CA_VU0, CA_VU4, CA_VU6, CA_NATIVA, CA_GRUPOS };
enum { CA_RESTO, CA_ALFA, CA_BUFFERS, CA_PLANO, CA_EDRAM, CA_PREPARA,
       CA_PIPE, CA_SAMPLER, CA_ESTADO, CA_TEXTURA, CA_UNIFORME, CA_INDICES,
       CA_UP, CA_PRESENT, CA_LIMPEZA, CA_FXAA, CA_TELA, CA_ETAPAS };
#ifndef BT3_PERFIL_CADEIA
class CadeiaEscopo {
public:
    CadeiaEscopo(int, int) {}
};
#define CADEIA_CHAMADA(e, chamada) do { chamada; } while (0)
static void cadeia_inicio(unsigned) {}
static void cadeia_fim(int) {}
#else
static const char *sCaGrupos[] = {"quadro", "comum", "vu0", "vu4", "vu6", "nativa"};
static const char *sCaEtapas[] = {"resto", "alfa", "buffers", "plano", "edram", "prepara",
    "pipe", "sampler", "estado", "textura", "uniforme", "indices", "UP", "Present", "limpeza", "FXAA", "tela"};
static struct {
    int lida, ligada, ativo, grupo, etapa, nivel;
    unsigned a, b, quadro;
    unsigned long long inicio, desde, tiques[CA_GRUPOS][CA_ETAPAS];
    unsigned chamadas[CA_GRUPOS][CA_ETAPAS];
} sCa;

static unsigned long long cadeia_tique(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (unsigned long long)t.QuadPart;
}

static void cadeia_inicio(unsigned quadro) {
    if (!sCa.lida) {
        const char *e = getenv("BT3_CADEIA_CPU");
        unsigned a, b;
        char sobra;
        sCa.lida = 1;
        if (e != NULL && strcmp(e, "0") != 0) {
            if (strcmp(e, "1") == 0) { sCa.a = sCa.b = 6960; sCa.ligada = 1; }
            else if (sscanf(e, "%u,%u%c", &a, &b, &sobra) == 2 && a <= b) {
                sCa.a = a; sCa.b = b; sCa.ligada = 1;
            } else Diario_Escrever("cadeia: BT3_CADEIA_CPU invalida; use A,B ou 1; desligada");
        }
    }
    sCa.ativo = sCa.ligada && quadro >= sCa.a && quadro <= sCa.b;
    if (!sCa.ativo) return;
    memset(sCa.tiques, 0, sizeof(sCa.tiques));
    memset(sCa.chamadas, 0, sizeof(sCa.chamadas));
    sCa.quadro = quadro;
    sCa.grupo = CA_QUADRO; sCa.etapa = CA_RESTO; sCa.nivel = 0;
    sCa.inicio = sCa.desde = cadeia_tique();
}

/* O objeto restaura o pai inclusive no return antecipado de prepara/chamadas.
   grupo -1 herda a família do desenho; os filhos são descontados do pai. */
class CadeiaEscopo {
    int ativo, grupo, etapa;
    CadeiaEscopo(const CadeiaEscopo &);
    CadeiaEscopo &operator=(const CadeiaEscopo &);
public:
    CadeiaEscopo(int g, int e) : ativo(sCa.ativo), grupo(0), etapa(0) {
        if (ativo) {
            unsigned long long t = cadeia_tique();
            grupo = sCa.grupo; etapa = sCa.etapa;
            sCa.tiques[grupo][etapa] += t - sCa.desde;
            sCa.grupo = g < 0 ? grupo : g; sCa.etapa = e;
            sCa.chamadas[sCa.grupo][e]++;
            sCa.desde = t; sCa.nivel++;
        }
    }
    ~CadeiaEscopo() {
        if (ativo) {
            unsigned long long t = cadeia_tique();
            sCa.tiques[sCa.grupo][sCa.etapa] += t - sCa.desde;
            sCa.grupo = grupo; sCa.etapa = etapa;
            sCa.desde = t; sCa.nivel--;
        }
    }
};
#define CADEIA_CHAMADA(e, chamada) do { CadeiaEscopo ca(-1, e); chamada; } while (0)

static void cadeia_fim(int captura) {
    int g, e;
    unsigned long long t, soma = 0;
    double f;
    if (!sCa.ativo) return;
    t = cadeia_tique();
    sCa.tiques[sCa.grupo][sCa.etapa] += t - sCa.desde;
    sCa.ativo = 0; /* o diário, depois da amostra, não entra nas parcelas */
    if (captura || sCa.nivel != 0 || sFreq.QuadPart <= 0) {
        Diario_Escrever("cadeia: cpu quadro %u indisponivel", sCa.quadro);
        return;
    }
    f = 1000.0 / (double)sFreq.QuadPart;
    Diario_Escrever("cadeia: cpu quadro %u total %.6f ms", sCa.quadro, (double)(t - sCa.inicio) * f);
    for (g = 0; g < CA_GRUPOS; g++) for (e = 0; e < CA_ETAPAS; e++) {
        soma += sCa.tiques[g][e];
        if (sCa.tiques[g][e] || sCa.chamadas[g][e])
            Diario_Escrever("cadeia: cpu quadro %u grupo %s etapa %s ms %.6f chamadas %u",
                sCa.quadro, sCaGrupos[g], sCaEtapas[e], (double)sCa.tiques[g][e] * f, sCa.chamadas[g][e]);
    }
    Diario_Escrever("cadeia: cpu quadro %u soma %.6f ms", sCa.quadro, (double)soma * f);
}
#endif
