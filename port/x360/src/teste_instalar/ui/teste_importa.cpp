/* A tela SAVE IMPORT (f2/importa_tela.cpp) com o D3D e o XInput falsos (ui/xtl.h) e a lógica falsa: o que a tela chama
   é registrado e conferido contra o que o roteiro do controle tem de causar. Um caso por argumento. */
#include <xtl.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include "f2/importa_tela.h"
#include "f2/save_importa_360.h"
unsigned gApresentados, gCriadas, gConvertidas, gLimpas, gLidos, gPorta;
int gLimpar, gConverter, gThreadFalha;
const char *gRoteiro = "";
void TesteExigir(bool ok, const char *msg) {
    if (!ok) {
        std::fprintf(stderr, "FALHA: %s\n", msg);
        std::exit(1);
    }
}
extern "C" void Diario_Escrever(const char *formato, ...) {
    va_list a;
    va_start(a, formato);
    std::vprintf(formato, a);
    std::putchar('\n');
    va_end(a);
}
static int gN, gProcurar, gPular, gImportar, gIndice = -1, gRetornoImportar, gRetornoPular;
static SiFalha gFalhaImportar;
static const char *gNomes[4];
static int gErros[4];
extern "C" int Importa360_Procurar(SiLista *l) {
    std::memset(l, 0, sizeof(*l));
    gProcurar++;
    l->n = gN;
    for (int i = 0; i < gN; i++) {
        std::strcpy(l->item[i].nome, gNomes[i]);
        l->item[i].erro = gErros[i];
    }
    return gN;
}
extern "C" int Importa360_Importar(const SiLista *l, int indice, SiFalha *f) {
    gImportar++;
    gIndice = indice;
    TesteExigir(indice >= 0 && indice < l->n, "índice fora da lista");
    TesteExigir(l->item[indice].erro == 0, "importou um arquivo recusado");
    *f = gFalhaImportar;
    return gRetornoImportar;
}
extern "C" int Importa360_Pular(const SiLista *, SiFalha *f) {
    gPular++;
    std::memset(f, 0, sizeof(*f));
    if (gRetornoPular) {
        f->passo = SI_P_MOVER;
        f->sistema = 32;
    }
    return gRetornoPular;
}
/* caso: roteiro, itens (nome:erro), importados esperados (índice ou -1), pulados, quadros lidos (-1 = qualquer), procuras */
struct Caso {
    const char *nome, *roteiro;
    int n, erro0, erro1, erro2, indice, importar, pular, lidos;
};
static const Caso kCasos[] = {
    {"nenhum", ".A.A", 0, 0, 0, 0, -1, 0, 0, 0},
    {"skip_ao_abrir", ".A", 1, 0, 0, 0, -1, 0, 1, 2},
    {"import_valido", ".L.A.A", 1, 0, 0, 0, 0, 1, 0, 6},
    {"ja_apertado", "AA.A", 1, 0, 0, 0, -1, 0, 1, 4},
    {"b_pula", ".L.B", 1, 0, 0, 0, -1, 0, 1, 4},
    {"b_pula_sem_botao_mudar", ".B", 1, 0, 0, 0, -1, 0, 1, 2},
    {"recusado", ".L.A.R.A", 1, 9, 0, 0, -1, 0, 1, 8},
    {"abre_no_primeiro_bom", ".D.L.A.A", 3, 9, 0, 0, 2, 1, 0, 8},
    {"cima_baixo", ".U.U.L.A.R.A", 3, 9, 0, 0, -1, 0, 1, 12},
    {"borda_descida", ".DDDD.L.A.A", 3, 0, 0, 0, 1, 1, 0, 11},
    {"limites", ".D.D.D.L.A.A", 2, 0, 0, 0, 1, 1, 0, 12},
    {"analogico", ".d.l.A.A", 3, 0, 0, 0, 1, 1, 0, 8},
    {"falha_importar", ".L.A.A", 1, 0, 0, 0, 0, 1, 0, 6},
    {"falha_pular", ".A.A", 1, 0, 0, 0, -1, 0, 1, 4},
    {"apresenta", ".........................A", 1, 0, 0, 0, -1, 0, 1, 26},
};
int main(int argc, char **argv) {
    D3DDevice dev;
    const Caso *c = NULL;
    if (argc != 2)
        return 2;
    for (unsigned i = 0; i < sizeof(kCasos) / sizeof(kCasos[0]); i++)
        if (!std::strcmp(argv[1], kCasos[i].nome))
            c = &kCasos[i];
    if (!c)
        return 2;
    gRoteiro = c->roteiro;
    gN = c->n;
    gNomes[0] = "a.psu"; gNomes[1] = "b.psu"; gNomes[2] = "c.psu";
    gErros[0] = c->erro0; gErros[1] = c->erro1; gErros[2] = c->erro2;
    if (!std::strcmp(c->nome, "falha_importar")) {
        gRetornoImportar = SI_E_TROCA;
        gFalhaImportar.passo = SI_P_TROCA;
        gFalhaImportar.sistema = 5;
    }
    if (!std::strcmp(c->nome, "falha_pular"))
        gRetornoPular = 2;
    ImportaTela_Executar(&dev);
    TesteExigir(gProcurar == 1, "a procura não foi feita uma vez");
    TesteExigir(gImportar == c->importar, "importações diferentes do esperado");
    TesteExigir(c->importar == 0 || gIndice == c->indice, "o arquivo importado não é o escolhido");
    TesteExigir(gPular == c->pular, "SKIP diferente do esperado");
    TesteExigir(gLidos == (unsigned)c->lidos, "quadros do controle lidos diferentes do esperado");
    TesteExigir(c->lidos == 0 || gApresentados + 3 >= (unsigned)c->lidos, "nem um quadro apresentado por leitura");
    TesteExigir(c->n > 0 || gApresentados == 0, "sem .psu a tela apresentou");
    std::puts("teste_importa: tudo certo");
    return 0;
}
