#include <xtl.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include "f2/iso_tela.h"
#include "iso_instalar.h"
#include "iso_conv.h"
unsigned gApresentados, gCriadas, gConvertidas, gLimpas, gLidos, gPorta;
int gLimpar = 1, gConverter, gThreadFalha;
const char *gRoteiro = "";
void TesteExigir(bool ok, const char *msg) {
    if (!ok) {
        std::fprintf(stderr, "FALHA: %s\n", msg);
        std::exit(1);
    }
}
/* O diário do teste vai para o stdout (o runner só o mostra se o caso falhar). */
extern "C" void Diario_Escrever(const char *formato, ...) {
    va_list a;
    va_start(a, formato);
    std::vprintf(formato, a);
    std::putchar('\n');
    va_end(a);
}
extern "C" int IsoInstalar_Limpar(void) {
    gLimpas++;
    return gLimpar;
}
extern "C" int IsoInstalar_Executar(const char *origem) {
    TesteExigir(!std::strcmp(origem, "game:\\bt3.iso"), "origem diferente da escolhida");
    gConvertidas++;
    return gConverter;
}
extern "C" void IsoInstalar_Progresso(int *fase, unsigned *pc) {
    *fase = IC_COPIAR;
    *pc = 42;
}
extern "C" void IsoInstalar_Contas(IcContas *c) {
    std::memset(c, 0, sizeof(*c));
    c->atual = -1;
}
/* Cada caso: o roteiro do controle, o retorno de IsoTela_Converter, as conversões feitas e quantos quadros do
   roteiro tinham de ser lidos (-1: qualquer número). O roteiro é gasto pela pergunta e depois pela caixa final. */
struct Caso {
    const char *nome, *roteiro;
    int retorno, convertidas, lidos;
};
static const char kPresos[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA.L.A.A";
static const char kEspera[] = "........................................A";
static const Caso kCasos[] = {
    {"sucesso", ".L.A.A", 1, 1, 6},
    {"esquerda_direita", ".R.L.A.A", 1, 1, 8},
    {"analogico", ".l.A.A", 1, 1, 6},
    {"analogico_fraco", ".w.A", 0, 0, 4},
    {"cancelar", ".A", 0, 0, 2},
    {"cancelar_b", ".B", 0, 0, 2},
    {"direita_cancela", ".L.R.A", 0, 0, 6},
    {"ja_apertado", "AA.A", 0, 0, 4},
    {"preso", kPresos, 1, 1, 36},
    {"outra_porta", ".L.A.A", 1, 1, 6},
    {"apresenta", kEspera, 0, 0, 41},
    {"sucesso_b", ".L.A.B", 1, 1, 6},
    {"limpeza", ".A", 0, 0, 2},
    {"thread", ".A", 0, 0, 2},
    {"espaco", ".L.A.A", 0, 1, 6},
    {"destino", ".L.A.A", 0, 1, 6},
    {"falha", ".L.A.A", 0, 1, 6},
    {"falha_b", ".L.A.B", 0, 1, 6},
};
int main(int argc, char **argv) {
    D3DDevice dev;
    const Caso *c = NULL;
    int r;
    if (argc != 2)
        return 2;
    for (unsigned i = 0; i < sizeof(kCasos) / sizeof(kCasos[0]); i++)
        if (!std::strcmp(argv[1], kCasos[i].nome))
            c = &kCasos[i];
    if (!c)
        return 2;
    gRoteiro = c->roteiro;
    if (!std::strcmp(c->nome, "outra_porta"))
        gPorta = 2;
    if (!std::strcmp(c->nome, "limpeza"))
        gLimpar = 0;
    else if (!std::strcmp(c->nome, "thread"))
        gThreadFalha = 1;
    else if (!std::strcmp(c->nome, "espaco"))
        gConverter = IC_ESPACO;
    else if (!std::strcmp(c->nome, "destino"))
        gConverter = IC_DESTINO;
    else if (!std::strncmp(c->nome, "falha", 5))
        gConverter = IC_IO;
    r = IsoTela_Converter(&dev, "game:\\bt3.iso");
    TesteExigir(r == c->retorno, "reinício incorreto");
    TesteExigir(gLimpas == (gThreadFalha ? 0U : 1U), "não limpou o parcial na próxima abertura");
    TesteExigir(gConvertidas == (unsigned)c->convertidas, "conversões diferentes do esperado");
    TesteExigir(c->lidos < 0 || gLidos == (unsigned)c->lidos, "quadros do controle lidos diferentes do esperado");
    TesteExigir(gApresentados >= (c->lidos > 1 ? (unsigned)c->lidos - 1 : 1U), "nem um quadro apresentado por leitura");
    std::puts("teste_iso_tela: tudo certo");
    return 0;
}
