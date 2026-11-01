/* D3D falso da prova host da tela de conversão: Clear pinta num buffer 1280x720 ARGB, sem GPU. */
#ifndef PROVA_TELA_XTL_H
#define PROVA_TELA_XTL_H
#include <cstdio>
#include <cstdlib>
#include <cstring>
typedef int LONG;
struct D3DRECT {
    LONG x1, y1, x2, y2;
};
#define D3DCLEAR_TARGET 1
struct D3DDevice {
    unsigned px[1280 * 720];
    unsigned recusados;
    void Clear(unsigned n, const D3DRECT *r, unsigned flags, unsigned cor, float, unsigned) {
        int x, y;
        D3DRECT t = {0, 0, 1280, 720};
        if (flags != D3DCLEAR_TARGET) {
            std::fprintf(stderr, "FALHA: Clear com flags inesperadas\n");
            std::exit(1);
        }
        if (n) {
            if (!r || r->x1 < 0 || r->x2 > 1280 || r->y1 < 0 || r->y2 > 720 || r->x1 >= r->x2 || r->y1 >= r->y2) {
                std::fprintf(stderr, "FALHA: retângulo fora da tela ou vazio\n");
                std::exit(1);
            }
            t = *r;
        }
        for (y = t.y1; y < t.y2; y++)
            for (x = t.x1; x < t.x2; x++)
                px[y * 1280 + x] = cor;
    }
};
#endif
