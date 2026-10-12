/*
 * F3, etapa 6: o esqueleto do D3D9 do 360 que o GsBackend vai usar, num .xex à parte, antes do renderizador.
 * Confere no console o que o PC nunca precisou saber:
 *   - textura D3DFMT_LIN_A8B8G8R8 recebendo o u32 nativo que o gs_draw.c produz (R | G<<8 | B<<16 | A<<24):
 *     o quadro de cima à esquerda tem de sair vermelho, verde, azul e branco, nessa ordem, em sentido horário;
 *   - cor de vértice em 4 bytes na ordem da memória (r, g, b, a: o Vtx de gs_draw.h) com UBYTE4N: o quadro de cima
 *     à direita tem os cantos vermelho (sup. esq.), verde (sup. dir.), azul (inf. dir.) e amarelo (inf. esq.).
 *     O Xenos entrega (a, b, g, r), e o shader inverte (teste.hlsl);
 *   - o meio pixel: dois xadrezes de 1 pixel com filtro linear, um com D3DRS_HALFPIXELOFFSET ligado e outro
 *     desligado. O que estiver nítido diz a convenção certa; o outro sai cinza;
 *   - shaders do fxc do XDK (compilados no build, port/x360/tools/build_teste_d3d.sh), nunca no boot;
 *   - a thread de desenho nunca para (a Guide congela o console se parar).
 * No quadro 120 a tela vai para game:\tela.bmp (resolve para textura linear), e o log diz o que foi feito.
 * Qualquer botão volta ao dashboard.
 */
#include <xtl.h>
#include <xgraphics.h>
#include <stdio.h>
#include "diario.h"
#include "teste_vs.h" /* gerados pelo fxc: kVs, kPs */
#include "teste_ps.h"

struct Vtx { float x, y, z; unsigned char rgba[4]; float u, v; }; /* como o Vtx de gs_draw.h, sem o q */

static D3DDevice *gDev;

static void Quad(float x0, float y0, float x1, float y1, const unsigned char cor[4][4], float u1, float v1) {
    /* cantos: 0 sup. esq., 1 sup. dir., 2 inf. dir., 3 inf. esq.; dois triângulos */
    static const int ordem[6] = {0, 1, 3, 1, 2, 3};
    const float px[4] = {x0, x1, x1, x0}, py[4] = {y0, y0, y1, y1};
    const float pu[4] = {0, u1, u1, 0}, pv[4] = {0, 0, v1, v1};
    Vtx v[6];
    for (int i = 0; i < 6; i++) {
        int k = ordem[i];
        v[i].x = px[k];
        v[i].y = py[k];
        v[i].z = 0;
        for (int c = 0; c < 4; c++) {
            v[i].rgba[c] = cor[k][c];
        }
        v[i].u = pu[k];
        v[i].v = pv[k];
    }
    gDev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, v, sizeof(Vtx));
}

static D3DTexture *Textura(int w, int h, unsigned (*pixel)(int x, int y)) {
    D3DTexture *t = NULL;
    D3DLOCKED_RECT r;
    if (FAILED(gDev->CreateTexture(w, h, 1, 0, D3DFMT_LIN_A8B8G8R8, D3DPOOL_DEFAULT, &t, NULL)) ||
        FAILED(t->LockRect(0, &r, NULL, 0))) {
        Diario_Escrever("ERRO: textura %dx%d", w, h);
        return t;
    }
    for (int y = 0; y < h; y++) {
        unsigned *linha = (unsigned *)((BYTE *)r.pBits + y * r.Pitch);
        for (int x = 0; x < w; x++) {
            linha[x] = pixel(x, y);
        }
    }
    t->UnlockRect(0);
    return t;
}

/* o u32 nativo do gs_draw.c: R | G<<8 | B<<16 | A<<24 */
static unsigned Rgba(unsigned r, unsigned g, unsigned b, unsigned a) { return r | g << 8 | b << 16 | a << 24; }

static unsigned Quadrantes(int x, int y) {
    /* vermelho, verde / branco, azul (horário a partir do canto de cima à esquerda) */
    if (y < 2) {
        return x < 2 ? Rgba(255, 0, 0, 255) : Rgba(0, 255, 0, 255);
    }
    return x < 2 ? Rgba(255, 255, 255, 255) : Rgba(0, 0, 255, 255);
}

static unsigned Xadrez(int x, int y) { return ((x ^ y) & 1) ? Rgba(255, 255, 255, 255) : Rgba(0, 0, 0, 255); }
static unsigned Branco(int x, int y) { (void)x; (void)y; return Rgba(255, 255, 255, 255); }

/* A tela resolvida para uma textura e gravada em BMP (little-endian, 24 bits, de baixo para cima). O resolve entrega
   a memória em blocos (tiled) mesmo numa textura LIN_ (medido no console): cada pixel se lê pelo
   XGAddress2DTiledOffset. */
static void Print(const char *caminho) {
    D3DTexture *t = NULL;
    D3DLOCKED_RECT r;
    HANDLE h;
    DWORD n;
    const int W = 1280, H = 720;
    if (FAILED(gDev->CreateTexture(W, H, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &t, NULL))) {
        Diario_Escrever("ERRO: textura do print");
        return;
    }
    gDev->Resolve(D3DRESOLVE_RENDERTARGET0, NULL, t, NULL, 0, 0, NULL, 0.0f, 0, NULL);
    gDev->BlockUntilIdle();
    t->LockRect(0, &r, NULL, D3DLOCK_READONLY);
    h = CreateFile(caminho, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        unsigned char cab[54] = {'B', 'M'};
        unsigned tam = 54 + W * H * 3, campos[] = {2, tam, 10, 54, 14, 40, 18, W, 22, H, 34, W * H * 3};
        static unsigned char linha[1280 * 3];
        for (int i = 0; i < 12; i += 2) { /* inteiros little-endian nas posições do cabeçalho */
            for (int b = 0; b < 4; b++) {
                cab[campos[i] + b] = (unsigned char)(campos[i + 1] >> (8 * b));
            }
        }
        cab[26] = 1;  /* planos */
        cab[28] = 24; /* bits por pixel */
        WriteFile(h, cab, 54, &n, NULL);
        for (int y = H - 1; y >= 0; y--) {
            const unsigned *p = (const unsigned *)r.pBits;
            for (int x = 0; x < W; x++) { /* A8R8G8B8 lido como u32 nativo */
                unsigned c = p[XGAddress2DTiledOffset(x, y, W, 4)];
                linha[x * 3 + 0] = (unsigned char)c;
                linha[x * 3 + 1] = (unsigned char)(c >> 8);
                linha[x * 3 + 2] = (unsigned char)(c >> 16);
            }
            WriteFile(h, linha, W * 3, &n, NULL);
        }
        FlushFileBuffers(h);
        CloseHandle(h);
        Diario_Escrever("print: %s (pitch %d)", caminho, r.Pitch);
    }
    t->UnlockRect(0);
    t->Release();
}

static DWORD WINAPI VoltarAoDashboard(LPVOID arg) {
    (void)arg;
    XLaunchNewImage(NULL, 0);
    return 0;
}

void __cdecl main() {
    IDirect3D9 *d3d;
    D3DPRESENT_PARAMETERS pp;
    D3DVertexShader *vs = NULL;
    D3DPixelShader *ps = NULL;
    D3DVertexDeclaration *decl = NULL;
    D3DTexture *quad, *xadrez, *branco;
    static const D3DVERTEXELEMENT9 elementos[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()};
    static const unsigned char kBranco[4][4] = {{255, 255, 255, 255}, {255, 255, 255, 255}, {255, 255, 255, 255},
                                                {255, 255, 255, 255}};
    static const unsigned char kCantos[4][4] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 255},
                                                {255, 255, 0, 255}};
    unsigned quadro = 0;
    bool sair = false;

    Diario_Abrir("game:\\teste_d3d.log");
    Diario_Escrever("teste do D3D9 (" __DATE__ " " __TIME__ ")");
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = 1280;
    pp.BackBufferHeight = 720;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferCount = 1;
    pp.EnableAutoDepthStencil = FALSE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    if (FAILED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, NULL, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &gDev))) {
        Diario_Escrever("ERRO: CreateDevice");
        for (;;) {
            Sleep(1000);
        }
    }
    if (FAILED(gDev->CreateVertexShader((const DWORD *)kVs, &vs)) ||
        FAILED(gDev->CreatePixelShader((const DWORD *)kPs, &ps)) ||
        FAILED(gDev->CreateVertexDeclaration(elementos, &decl))) {
        Diario_Escrever("ERRO: shaders ou declaração (vs %p, ps %p, decl %p)", vs, ps, decl);
    }
    quad = Textura(4, 4, Quadrantes);
    xadrez = Textura(64, 64, Xadrez);
    branco = Textura(4, 4, Branco);
    Diario_Escrever("device, shaders e texturas prontos");

    while (!sair) {
        /* pixels do GS -> clip: x / 640 - 1, 1 - y / 360 */
        const float escala[4] = {2.0f / 1280.0f, -2.0f / 720.0f, -1.0f, 1.0f};
        gDev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(40, 40, 48), 1.0f, 0);
        gDev->SetVertexDeclaration(decl);
        gDev->SetVertexShader(vs);
        gDev->SetPixelShader(ps);
        gDev->SetVertexShaderConstantF(0, escala, 1);
        gDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        gDev->SetRenderState(D3DRS_ZENABLE, FALSE);
        gDev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

        /* textura de quadrantes, ponto a ponto */
        gDev->SetRenderState(D3DRS_HALFPIXELOFFSET, TRUE);
        gDev->SetTexture(0, quad);
        gDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        gDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        Quad(80, 80, 400, 400, kBranco, 1, 1);
        /* cor de vértice sobre textura branca */
        gDev->SetTexture(0, branco);
        Quad(480, 80, 800, 400, kCantos, 1, 1);
        /* xadrez de 1 pixel, 1:1, filtro linear: com e sem o meio pixel */
        gDev->SetTexture(0, xadrez);
        gDev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        gDev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        gDev->SetRenderState(D3DRS_HALFPIXELOFFSET, TRUE);
        Quad(880, 80, 944, 144, kBranco, 1, 1);
        gDev->SetRenderState(D3DRS_HALFPIXELOFFSET, FALSE);
        Quad(1040, 80, 1104, 144, kBranco, 1, 1);
        gDev->SetRenderState(D3DRS_HALFPIXELOFFSET, TRUE);

        if (++quadro == 120) {
            Print("game:\\tela.bmp");
        }
        gDev->Present(NULL, NULL, NULL, NULL);

        for (DWORD porta = 0; porta < XUSER_MAX_COUNT && quadro > 120; porta++) {
            XINPUT_STATE e;
            if (XInputGetState(porta, &e) == ERROR_SUCCESS && e.Gamepad.wButtons != 0) {
                sair = true;
            }
        }
    }
    Diario_Escrever("voltando ao dashboard depois de %u quadros", quadro);
    gDev->BlockUntilIdle();
    Diario_Fechar();
    CloseHandle(CreateThread(NULL, 64 * 1024, VoltarAoDashboard, NULL, 0, NULL));
    for (;;) {
        Sleep(1000);
    }
}
