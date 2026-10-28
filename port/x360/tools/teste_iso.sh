#!/usr/bin/env bash
# Teste do leitor de ISO do 360 (port/x360/src/iso360.c, decisões 18 e 23) no WSL, contra a ISO do usuário
# convertida no PC (port/x360/tools/convert_iso.py; a original é recusada pela falta da marca e dá tudo FALTA):
#   port/x360/tools/teste_iso.sh CAMINHO-convertida.iso
# Compila iso360.c e os conversores (port/x360/conv) para o Linux, com uma camada mínima da API do Xbox, e confere
# cada arquivo lido de dentro da ISO contra o que extract_disc.py e conv_tool já deixaram em disco:
#   - disc/SLUS_216.78, disc/BIN/DBZP.BIN e disc/DATA/* contra gamedata/disc;
#   - todas as entradas dos três AFS: as que têm conversor contra a conversão do conv_tool ($T/ref),
#     as outras contra gamedata/pzs3usN.
# Só imprime nomes e contagens: nenhum byte do jogo sai daqui.
set -euo pipefail
ISO=${1:?"uso: teste_iso.sh CAMINHO.iso"}
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$RAIZ"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cat > "$T/teste.c" <<'EOF'
#include <xtl.h>
#include "f2/blocos.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
long long gTesteLentaIni, gTesteLentaFim;
long gTesteLentaUs;
volatile int gTesteDentro;
int Filme360_BlocoKb(void) { return 0; }
Blocos *Blocos_Abrir(BlocosLer ler, void *ctx, long tam, long bloco, int hw) {
    (void)ler; (void)ctx; (void)tam; (void)bloco; (void)hw; return NULL;
}
long Blocos_Ler(Blocos *b, long pos, void *dst, long n) { (void)b; (void)pos; (void)dst; (void)n; return -1; }
void Blocos_Conta(Blocos *b, BlocosConta *c) { (void)b; memset(c, 0, sizeof(*c)); }
void Blocos_Fechar(Blocos *b) { (void)b; }
void *Iso_Abrir(const char *rel);
size_t Iso_Ler(void *buf, size_t tam, size_t n, void *f);
int Iso_Fechar(void *f);
int Iso_Posicionar(void *f, long desl, int origem);
long Iso_Posicao(void *f);
void Diario_Escrever(const char *formato, ...) {
    va_list ap; va_start(ap, formato); vprintf(formato, ap); va_end(ap); putchar('\n');
}
void Port_Abortar(void) { printf("Port_Abortar\n"); exit(3); }
static unsigned char *tudo(const char *p, long *n) {
    FILE *f = fopen(p, "rb"); unsigned char *b;
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc(*n + 1); if (fread(b, 1, *n, f) != (size_t)*n) { *n = -1; } fclose(f); return b;
}
/* argumentos: pares "rel caminho-de-referência" pela entrada padrão */
int main(void) {
    char rel[256], ref[512];
    long ok = 0, dif = 0, falta = 0;
    while (scanf("%255s %511s", rel, ref) == 2) {
        long n = 0, m, pos;
        unsigned char *a = tudo(ref, &n), *b;
        void *f = Iso_Abrir(rel);
        if (f == NULL || f == (void *)-1 || a == NULL) { printf("FALTA %s\n", rel); falta++; free(a); continue; }
        Iso_Posicionar(f, 0, SEEK_END); m = Iso_Posicao(f); Iso_Posicionar(f, 0, SEEK_SET);
        b = malloc(m + 1);
        /* lido em pedaços de 2048, como o jogo */
        for (pos = 0; pos < m; pos += 2048) Iso_Ler(b + pos, 1, (size_t)(m - pos < 2048 ? m - pos : 2048), f);
        if (m != n || memcmp(a, b, n) != 0) { printf("DIFERENTE %s (%ld e %ld bytes)\n", rel, m, n); dif++; } else ok++;
        Iso_Fechar(f); free(a); free(b);
    }
    printf("iso: %ld iguais, %ld diferentes, %ld faltando\n", ok, dif, falta);
    return dif || falta;
}
EOF
# referência das entradas com conversor: o conv_tool sobre os arquivos extraídos
gcc -std=c89 -O2 -w -o "$T/conv_tool" $(ls port/x360/conv/*.c | grep -v conv_jogo)
lista=$(cd gamedata && ls pzs3us1/*.bin)
mkdir -p "$T/ref"
"$T/conv_tool" --icones360 --saida "$T/ref" gamedata $lista > "$T/conv.txt" 2>&1
gcc -std=gnu99 -O2 -w -Iport/x360/src/som/teste/xtl -Iport/x360/src -Iport/x360/conv -o "$T/teste" "$T/teste.c" port/x360/src/iso360.c \
    $(ls port/x360/conv/*.c | grep -v -e conv_tool -e conv_jogo) -lpthread
{
    for f in SLUS_216.78 BIN/DBZP.BIN $(cd gamedata/disc && ls DATA/*); do echo "disc/$f gamedata/disc/$f"; done
    for p in pzs3us0 pzs3us1 pzs3us2; do
        for f in $(cd gamedata && ls $p); do
            if [ -f $T/ref/$p/$f ]; then echo "$p/$f $T/ref/$p/$f"; else echo "$p/$f gamedata/$p/$f"; fi
        done
    done
} > "$T/pares.txt"
echo "conferindo $(wc -l < "$T/pares.txt") arquivos; com conversor: $(ls $T/ref/pzs3us1 | wc -l)"
status=0
BT3_ISO="$ISO" "$T/teste" < "$T/pares.txt" > "$T/saida.txt" || status=$?
grep -v "^FALTA\|^DIFERENTE\|^iso: pzs" "$T/saida.txt" | head -20 || true
grep "^FALTA\|^DIFERENTE" "$T/saida.txt" | head -10 || true
exit $status
