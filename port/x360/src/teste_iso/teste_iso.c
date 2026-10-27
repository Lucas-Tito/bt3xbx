/* F8: o iso360.c real sobre a API POSIX de teste, sem bytes do jogo. Um processo por tentativa de boot. */
#include <xtl.h>
#include "iso360.h"
#include "diario.h"
#include "conv.h"
#include "f2/blocos.h"
#include <stdarg.h>

long long gTesteLentaIni, gTesteLentaFim;
long gTesteLentaUs;
volatile int gTesteDentro;

void Diario_Escrever(const char *formato, ...) {
    va_list ap;
    va_start(ap, formato);
    vprintf(formato, ap);
    puts("");
    va_end(ap);
}
const char *Conv_FormatoDe(const char *nome) { (void)nome; return NULL; }
int Conv_Arquivo(const char *nome, conv_u8 *buf, conv_ofs tam, conv_u8 *feito, char *msg, int nmsg) {
    (void)nome; (void)buf; (void)tam; (void)feito; (void)msg; (void)nmsg;
    return -1;
}
int Filme360_BlocoKb(void) { return 0; }
void Port_Abortar(void) { abort(); }
Blocos *Blocos_Abrir(BlocosLer ler, void *ctx, long tam, long bloco, int hw) {
    (void)ler; (void)ctx; (void)tam; (void)bloco; (void)hw; return NULL;
}
long Blocos_Ler(Blocos *b, long pos, void *dst, long n) { (void)b; (void)pos; (void)dst; (void)n; return -1; }
void Blocos_Conta(Blocos *b, BlocosConta *c) { (void)b; memset(c, 0, sizeof(*c)); }
void Blocos_Fechar(Blocos *b) { (void)b; }

void *Iso_Abrir(const char *rel);
size_t Iso_Ler(void *buf, size_t tam, size_t n, void *f);
int Iso_Fechar(void *f);

int main(int argc, char **argv) {
    void *f;
    char b[9];
    int motivo;
    if (argc != 3) {
        puts("FALHA: informe motivo e caminho esperados");
        return 2;
    }
    Iso_Iniciar();
    Iso_Iniciar(); /* a segunda chamada não troca a escolha nem reinicia as tabelas */
    motivo = Iso_Recusada();
    if (motivo != atoi(argv[1]) || strcmp(Iso_Pedida(), argv[2]) != 0) {
        printf("FALHA: motivo %d, caminho '%s'; esperados %s, '%s'\n", motivo, Iso_Pedida(), argv[1], argv[2]);
        return 1;
    }
    f = Iso_Abrir("pzs3us0/00000.bin");
    if (motivo != ISO_OK) {
        if (f != NULL) {
            puts("FALHA: ISO recusada ainda entrega arquivos");
            return 1;
        }
    } else if (argv[2][0] == '\0') {
        if (f != (void *)-1) {
            puts("FALHA: sem ISO não recorre aos arquivos soltos");
            return 1;
        }
    } else {
        if (f == NULL || f == (void *)-1 || Iso_Ler(b, 1, sizeof(b), f) != sizeof(b) || memcmp(b, "TESTE-ISO", 9) != 0) {
            puts("FALHA: a entrada sintética do AFS não veio da ISO escolhida");
            return 1;
        }
        Iso_Fechar(f);
    }
    puts("teste_iso_avisos: tudo certo");
    return 0;
}
