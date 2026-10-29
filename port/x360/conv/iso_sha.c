/* SHA256 (FIPS 180-4), aritmética u32 e leitura byte a byte nos dois hosts. */
#include <string.h>
#include "iso_conv.h"
static conv_u32 rod(conv_u32 x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}
static void bloco(IsoSha *s) {
    static const conv_u32 k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
    conv_u32 w[64], a, b, c, d, e, f, g, h, t, u;
    unsigned i;
    for (i = 0; i < 16; i++) {
        const conv_u8 *p = s->bloco + i * 4;
        w[i] = (conv_u32)p[0] << 24 | (conv_u32)p[1] << 16 | (conv_u32)p[2] << 8 | p[3];
    }
    for (i = 16; i < 64; i++)
        w[i] = w[i - 16] + (rod(w[i - 15], 7) ^ rod(w[i - 15], 18) ^ (w[i - 15] >> 3)) + w[i - 7] +
               (rod(w[i - 2], 17) ^ rod(w[i - 2], 19) ^ (w[i - 2] >> 10));
    a = s->h[0];
    b = s->h[1];
    c = s->h[2];
    d = s->h[3];
    e = s->h[4];
    f = s->h[5];
    g = s->h[6];
    h = s->h[7];
    for (i = 0; i < 64; i++) {
        t = h + (rod(e, 6) ^ rod(e, 11) ^ rod(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
        u = (rod(a, 2) ^ rod(a, 13) ^ rod(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g;
        g = f;
        f = e;
        e = d + t;
        d = c;
        c = b;
        b = a;
        a = t + u;
    }
    s->h[0] += a;
    s->h[1] += b;
    s->h[2] += c;
    s->h[3] += d;
    s->h[4] += e;
    s->h[5] += f;
    s->h[6] += g;
    s->h[7] += h;
}
void IsoSha_Iniciar(IsoSha *s) {
    static const conv_u32 ini[8] = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    memset(s, 0, sizeof(*s));
    memcpy(s->h, ini, sizeof(ini));
}
void IsoSha_Atualizar(IsoSha *s, const void *buf, unsigned n) {
    const conv_u8 *p = (const conv_u8 *)buf;
    conv_u32 antes = s->baixo;
    s->baixo += (conv_u32)n << 3;
    s->alto += ((conv_u32)n >> 29) + (s->baixo < antes);
    while (n) {
        unsigned z = 64 - s->usado;
        if (z > n)
            z = n;
        memcpy(s->bloco + s->usado, p, z);
        s->usado += z;
        p += z;
        n -= z;
        if (s->usado == 64) {
            bloco(s);
            s->usado = 0;
        }
    }
}
void IsoSha_Final(IsoSha *s, conv_u8 hash[32]) {
    conv_u8 fim[72];
    unsigned n = s->usado < 56 ? 56 - s->usado : 120 - s->usado, i;
    memset(fim, 0, sizeof(fim));
    fim[0] = 0x80;
    for (i = 0; i < 4; i++) {
        fim[n + i] = (conv_u8)(s->alto >> (24 - i * 8));
        fim[n + 4 + i] = (conv_u8)(s->baixo >> (24 - i * 8));
    }
    IsoSha_Atualizar(s, fim, n + 8);
    for (i = 0; i < 32; i++)
        hash[i] = (conv_u8)(s->h[i / 4] >> (24 - (i % 4) * 8));
}
