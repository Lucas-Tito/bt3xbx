/* Importação de saves do PS2 antes do jogo (depois das conferências da ISO). A lógica é de conv/save_importa.c; aqui
   só a tela e o controle, no mesmo modelo da pergunta da conversão (iso_tela.cpp): controle direto do XInput, um botão
   só vale visto solto e apertado de novo, e a tela é apresentada a cada volta (a Guide congela o console se ninguém
   apresenta). A entrada de arquivos é pequena (três arquivos de até 40 KB por .psu): feita na própria thread, entre
   duas apresentações. */
#include <xtl.h>
#include <stdio.h>
#include <string.h>
#include "importa_tela.h"
#include "save_importa_360.h"
#include "iso_avisos.h"
#include "diario.h"
#include "importa_tela_desenho.h"
#ifdef BT3_VERSAO_NUM
#define IMP_STR_(x) #x
#define IMP_STR(x) IMP_STR_(x)
static const char *const kVersaoImp = IMP_STR(BT3_VERSAO_NUM);
#else
static const char *const kVersaoImp = "DEV";
#endif
#define PAD_ESQ 1
#define PAD_DIR 2
#define PAD_A 4
#define PAD_B 8
#define PAD_CIMA 16
#define PAD_BAIXO 32
#define PAD_LIMIAR 16000
#define PAD_PORTAS 4
static unsigned padLer(DWORD porta) {
    XINPUT_STATE e;
    unsigned m = 0;
    if (XInputGetState(porta, &e) != ERROR_SUCCESS)
        return 0;
    if ((e.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) || e.Gamepad.sThumbLX < -PAD_LIMIAR)
        m |= PAD_ESQ;
    if ((e.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) || e.Gamepad.sThumbLX > PAD_LIMIAR)
        m |= PAD_DIR;
    if ((e.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) || e.Gamepad.sThumbLY > PAD_LIMIAR)
        m |= PAD_CIMA;
    if ((e.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) || e.Gamepad.sThumbLY < -PAD_LIMIAR)
        m |= PAD_BAIXO;
    if (e.Gamepad.wButtons & XINPUT_GAMEPAD_A)
        m |= PAD_A;
    if (e.Gamepad.wButtons & XINPUT_GAMEPAD_B)
        m |= PAD_B;
    return m;
}

/* O estado do controle entre voltas: só vale o botão visto solto antes (por porta). */
struct Entrada {
    unsigned armado[PAD_PORTAS];
};
static void entradaAbrir(Entrada &e) {
    for (DWORD p = 0; p < PAD_PORTAS; p++)
        e.armado[p] = ~padLer(p); /* a leitura da abertura já conta como um quadro: o que está apertado não vale */
}
/* As bordas de descida desta volta, de todas as portas juntas. */
static unsigned entradaBordas(Entrada &e) {
    unsigned borda = 0;
    for (DWORD p = 0; p < PAD_PORTAS; p++) {
        unsigned agora = padLer(p);
        borda |= agora & e.armado[p];
        e.armado[p] = ~agora;
    }
    return borda;
}

/* Espera o A ou o B numa tela de mensagem. */
static void mensagem(D3DDevice *dev, const ImpMsg &m) {
    Entrada e;
    entradaAbrir(e);
    for (;;) {
        unsigned b = entradaBordas(e);
        telaImportaMsg(dev, kVersaoImp, m);
        dev->Present(NULL, NULL, NULL, NULL);
        if (b & (PAD_A | PAD_B))
            return;
    }
}

void ImportaTela_Executar(D3DDevice *dev) {
    static SiLista lista;
    SiFalha falha;
    Entrada e;
    int n = Importa360_Procurar(&lista), arq = 0, acao = 1, i, r;
    bool aviso = false, fim = false;
    if (n <= 0)
        return;
    for (i = 0; i < n; i++) /* abre no primeiro arquivo bom, se houver */
        if (lista.item[i].erro == 0) {
            arq = i;
            break;
        }
    Diario_Escrever("importa: tela aberta com %d arquivo(s)", n);
    entradaAbrir(e);
    while (!fim) {
        unsigned b = entradaBordas(e);
        if (b & PAD_CIMA) {
            if (arq > 0)
                arq--;
            aviso = false;
        } else if (b & PAD_BAIXO) {
            if (arq < n - 1)
                arq++;
            aviso = false;
        }
        if (b & PAD_ESQ) {
            acao = 0;
            aviso = false;
        } else if (b & PAD_DIR) {
            acao = 1;
            aviso = false;
        }
        if (b & PAD_B) {
            acao = 1;
            fim = true;
        } else if (b & PAD_A) {
            if (acao == 1)
                fim = true;
            else if (lista.item[arq].erro != 0)
                aviso = true; /* IMPORT só vale num arquivo OK */
            else
                fim = true;
        }
        if (!fim) {
            telaImportar(dev, kVersaoImp, lista, arq, acao, aviso);
            dev->Present(NULL, NULL, NULL, NULL);
        }
    }
    {
        ImpMsg m;
        memset(&m, 0, sizeof(m));
        if (acao == 1) {
            Diario_Escrever("importa: SKIP");
            r = Importa360_Pular(&lista, &falha);
            if (r > 0) {
                impResultado(m, r, falha, true);
                mensagem(dev, m);
            }
            return;
        }
        Diario_Escrever("importa: IMPORT de '%s'", lista.item[arq].nome);
        m.titulo = ISO_IMP_T_TRABALHANDO_TITULO;
        m.cor = TELA_TEXTO;
        strcpy(m.l[0], ISO_IMP_T_TRABALHANDO);
        m.n = 1;
        m.ajuda = NULL;
        telaImportaMsg(dev, kVersaoImp, m);
        dev->Present(NULL, NULL, NULL, NULL);
        r = Importa360_Importar(&lista, arq, &falha);
        impResultado(m, r, falha, false);
        mensagem(dev, m);
    }
}
