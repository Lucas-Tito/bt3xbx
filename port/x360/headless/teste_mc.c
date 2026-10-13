/*
 * F4, etapa 3: teste da gravação atômica do cartão de memória (port/src/plat_mc.c com PORT_MC_ATOMIC) sem o jogo, pela
 * interface da libmc que o jogo usa. Roda na x86 e na PPC (qemu), cada uma numa pasta vazia:
 *     teste_mc PASTA        (port/x360/tools/save_mc.sh compila e roda)
 * Casos: criar, regravar um trecho (o arquivo inteiro vai de novo), abrir sem criar, abrir para gravar e fechar sem
 * gravar (o arquivo não é trocado), falha de escrita (o jogo recebe -5 e o anterior fica), quedas de energia em três
 * pontos (processo filho que termina no meio; a leitura seguinte termina ou desfaz a gravação), apagar do anterior
 * falhando (-5, e o próximo uso põe o .new no lugar), temporário truncado
 * deixado à mão, criação interrompida e arquivo maior que o primeiro. Sai com 1 se algum caso falhar.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int func_002A1E58(int port, int slot, char *name, int mode); /* sceMcOpen */
int func_002A1F80(int port, int slot, char *name);           /* sceMcMkdir */
int func_002A1FB8(int fd);                                   /* sceMcClose */
int func_002A2078(int fd, int offset, int origin);           /* sceMcSeek */
int func_002A2208(int fd, void *buf, int size);              /* sceMcRead */
int func_002A2320(int fd, void *buf, int size);              /* sceMcWrite */
int func_002A2498(int mode, int *cmd, int *result);          /* sceMcSync */

void Port_StateTouch(void *p, unsigned long n) { (void)p; (void)n; }

static char sRaiz[400];
static int sFalhas;

static int sync_(void) {
    int cmd = 0, r = 0;
    func_002A2498(0, &cmd, &r);
    return r;
}
static int abrir(const char *nome, int modo) { func_002A1E58(0, 0, (char *)nome, modo); return sync_(); }
static int fechar(int fd) { func_002A1FB8(fd); return sync_(); }
static int posicionar(int fd, int d, int o) { func_002A2078(fd, d, o); return sync_(); }
static int ler(int fd, void *b, int n) { func_002A2208(fd, b, n); return sync_(); }
static int gravar(int fd, const void *b, int n) { func_002A2320(fd, (void *)b, n); return sync_(); }

static void caso(const char *nome, int ok) {
    printf("%s %s\n", ok ? "ok    " : "FALHOU", nome);
    sFalhas += !ok;
}

static const char *hospedeiro(const char *rel) {
    static char c[600];
    snprintf(c, sizeof(c), "%s/card1/%s", sRaiz, rel);
    return c;
}

static int existe(const char *rel) {
    struct stat st;
    return stat(hospedeiro(rel), &st) == 0;
}

static long inode(const char *rel) {
    struct stat st;
    return stat(hospedeiro(rel), &st) == 0 ? (long)st.st_ino : -1;
}

/* O conteúdo do arquivo (pela libmc, então a leitura passa pela recuperação) igual a `esperado`. */
static int conteudo(const char *nome, const unsigned char *esperado, int n) {
    static unsigned char b[200000];
    int fd = abrir(nome, 1), r;
    if (fd < 0) {
        return 0;
    }
    r = ler(fd, b, sizeof(b));
    fechar(fd);
    return r == n && memcmp(b, esperado, (size_t)n) == 0;
}

/* Grava n bytes de `c` em `nome` com a falha de teste `falha` num processo filho; devolve o código de saída dele. */
static int gravar_com_falha(const char *nome, int modo, int c, int n, const char *falha) {
    pid_t p;
    int st;
    fflush(stdout);
    p = fork();
    if (p == 0) {
        unsigned char b[1000];
        int fd;
        memset(b, c, sizeof(b));
        setenv("BT3_MC_FALHA", falha, 1);
        fd = abrir(nome, modo);
        gravar(fd, b, n);
        _exit(fechar(fd) == 0 ? 0 : 1);
    }
    waitpid(p, &st, 0);
    return WIFEXITED(st) ? WEXITSTATUS(st) : -1;
}

int main(int argc, char **argv) {
    unsigned char a[1000], esperado[1000];
    static unsigned char grande[150000];
    int fd, r, i;
    long ino;

    if (argc != 2) {
        fprintf(stderr, "uso: teste_mc PASTA (vazia)\n");
        return 2;
    }
    snprintf(sRaiz, sizeof(sRaiz), "%s", argv[1]);
    setenv("BT3_SAVES", sRaiz, 1);
    unsetenv("BT3_MC_FALHA");
    func_002A1F80(0, 0, "D");
    caso("pasta criada", sync_() == 0 && existe("D"));

    /* 1. criar */
    memset(a, 'A', sizeof(a));
    fd = abrir("/D/F", 0x202);
    r = gravar(fd, a, 100);
    caso("criar: gravou 100 bytes e fechou com 0", fd >= 0 && r == 100 && fechar(fd) == 0);
    caso("criar: conteúdo e nenhum .tmp/.new", conteudo("/D/F", a, 100) && !existe("D/F.tmp") && !existe("D/F.new"));

    /* 2. um trecho no meio: o arquivo inteiro vai de novo */
    fd = abrir("/D/F", 2);
    posicionar(fd, 10, 0);
    r = gravar(fd, "BBBB", 4);
    caso("trecho: fechou com 0", r == 4 && fechar(fd) == 0);
    memcpy(esperado, a, 100);
    memcpy(esperado + 10, "BBBB", 4);
    caso("trecho: 100 bytes, só o trecho mudou", conteudo("/D/F", esperado, 100));

    /* 3. abrir para gravar sem criar um arquivo que não existe */
    caso("sem criar: -4", abrir("/D/X", 2) == -4 && !existe("D/X"));

    /* 4. abrir para ler e gravar, medir e fechar sem gravar: o arquivo não é trocado (o mesmo inode) */
    ino = inode("D/F");
    fd = abrir("/D/F", 3);
    r = posicionar(fd, 0, 2);
    caso("sem gravar: tamanho 100, fechou com 0, arquivo intocado", r == 100 && fechar(fd) == 0 && inode("D/F") == ino);

    /* 5. falha de escrita: o jogo recebe -5 e o anterior fica */
    caso("falha de escrita: o fechamento devolve -5", gravar_com_falha("/D/F", 2, 'C', 100, "escrita") == 1);
    caso("falha de escrita: o anterior fica, sem .tmp", conteudo("/D/F", esperado, 100) && !existe("D/F.tmp"));
    setenv("BT3_MC_FALHA", "escrita:G", 1); /* (o filtro: só caminhos que terminam em G) */
    fd = abrir("/D/F", 2);
    gravar(fd, "Z", 1);
    caso("falha só no caminho filtrado", fechar(fd) == 0);
    unsetenv("BT3_MC_FALHA");
    esperado[0] = 'Z';

    /* 6. queda no meio do temporário: o anterior fica */
    caso("queda1: o processo terminou na queda", gravar_com_falha("/D/F", 2, 'D', 100, "queda1") == 86);
    caso("queda1: sobrou o .tmp, o arquivo está lá", existe("D/F.tmp") && existe("D/F") && !existe("D/F.new"));
    caso("queda1: a leitura seguinte dá o anterior, sem .tmp", conteudo("/D/F", esperado, 100) && !existe("D/F.tmp"));

    /* 7. queda depois do .new (o ponto de compromisso): vale o novo */
    caso("queda2: o processo terminou na queda", gravar_com_falha("/D/F", 2, 'E', 100, "queda2") == 86);
    caso("queda2: .new e o arquivo antigo", existe("D/F.new") && existe("D/F"));
    memset(esperado, 'E', 100);
    caso("queda2: a leitura seguinte dá o novo, sem .new", conteudo("/D/F", esperado, 100) && !existe("D/F.new"));

    /* 8. queda entre apagar o arquivo e trocar o nome: vale o novo */
    caso("queda3: o processo terminou na queda", gravar_com_falha("/D/F", 2, 'G', 100, "queda3") == 86);
    caso("queda3: só o .new", existe("D/F.new") && !existe("D/F"));
    memset(esperado, 'G', 100);
    caso("queda3: a leitura seguinte dá o novo, sem .new", conteudo("/D/F", esperado, 100) && !existe("D/F.new"));

    /* 8b. o apagar do arquivo antigo falha (e a troca de nome não o substitui, como o MoveFile do 360): o jogo recebe
       -5 em vez de um "gravou ok" com o conteúdo antigo, e o próximo uso põe o .new no lugar */
    caso("apaga: o fechamento devolve -5", gravar_com_falha("/D/F", 2, 'I', 100, "apaga") == 1);
    caso("apaga: o antigo e o .new ficam", existe("D/F") && existe("D/F.new") && !existe("D/F.tmp"));
    memset(esperado, 'I', 100);
    caso("apaga: a leitura seguinte dá o novo, sem .new", conteudo("/D/F", esperado, 100) && !existe("D/F.new"));
    memset(esperado, 'G', 100);
    fd = abrir("/D/F", 2);
    r = gravar(fd, esperado, 100);
    caso("apaga: regravado o conteúdo do caso 8", r == 100 && fechar(fd) == 0 && conteudo("/D/F", esperado, 100));

    /* 9. um temporário truncado deixado à mão */
    {
        FILE *f = fopen(hospedeiro("D/F.tmp"), "wb");
        fwrite("lixo", 1, 4, f);
        fclose(f);
    }
    caso("tmp à mão: a leitura dá o arquivo e apaga o .tmp", conteudo("/D/F", esperado, 100) && !existe("D/F.tmp"));

    /* 10. criação interrompida: nada aparece (como antes de gravar) */
    caso("criação interrompida: terminou na queda", gravar_com_falha("/D/N", 0x202, 'H', 100, "queda1") == 86);
    caso("criação interrompida: o arquivo não existe e o .tmp some", abrir("/D/N", 1) == -4 && !existe("D/N.tmp"));

    /* 11. arquivo maior, em pedaços, com um buraco */
    for (i = 0; i < (int)sizeof(grande); i++) {
        grande[i] = (unsigned char)(i * 7);
    }
    fd = abrir("/D/F", 2);
    r = gravar(fd, grande, 100000);
    posicionar(fd, 120000, 0);
    r += gravar(fd, grande + 120000, 30000);
    caso("grande: fechou com 0", r == 130000 && fechar(fd) == 0);
    memset(grande + 100000, 0, 20000);
    caso("grande: 150000 bytes com o buraco zerado", conteudo("/D/F", grande, 150000));

    printf("%s: %d falha(s)\n", sFalhas ? "FALHOU" : "ok", sFalhas);
    return sFalhas ? 1 : 0;
}
