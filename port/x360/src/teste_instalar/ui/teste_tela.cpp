#include <xtl.h>
#include <cstdio>
#include <cstdlib>
#include "f2/iso_tela.h"
#include "iso_instalar.h"
#include "iso_conv.h"
unsigned gApresentados, gCriadas, gConvertidas, gLimpas, gCaixas;
int gEscolha, gLimpar = 1, gConverter, gThreadFalha, gNegar;
void TesteExigir(bool ok, const char *msg) {
    if (!ok) {
        std::fprintf(stderr, "FALHA: %s\n", msg);
        std::exit(1);
    }
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
int main(int argc, char **argv) {
    D3DDevice dev;
    int r;
    if (argc != 2)
        return 2;
    if (!std::strcmp(argv[1], "cancelar"))
        gEscolha = 1;
    else if (!std::strcmp(argv[1], "limpeza"))
        gLimpar = 0;
    else if (!std::strcmp(argv[1], "thread"))
        gThreadFalha = 1;
    else if (!std::strcmp(argv[1], "espaco"))
        gConverter = IC_ESPACO;
    else if (!std::strcmp(argv[1], "destino"))
        gConverter = IC_DESTINO;
    else if (!std::strcmp(argv[1], "falha"))
        gConverter = IC_IO;
    else if (!std::strcmp(argv[1], "guide"))
        gNegar = 3;
    r = IsoTela_Converter(&dev, "game:\\bt3.iso");
    TesteExigir(r == ((!std::strcmp(argv[1], "sucesso") || !std::strcmp(argv[1], "guide")) ? 1 : 0),
                "reinício incorreto");
    TesteExigir(gLimpas == (gThreadFalha ? 0U : 1U), "não limpou o parcial na próxima abertura");
    TesteExigir(gConvertidas == ((gThreadFalha || !gLimpar || gEscolha) ? 0U : 1U),
                "conversão após cancelar ou falhar");
    TesteExigir(gApresentados >= 4, "não apresentou durante a UI");
    std::puts("teste_iso_tela: tudo certo");
    return 0;
}
