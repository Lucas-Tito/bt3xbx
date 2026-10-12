// F3, etapa 6: os shaders do teste do D3D9 (teste_d3d.cpp), compilados pelo fxc do XDK no build.
// Posição em pixels do GS (como o Vtx de gs_draw.h), cor do vértice em UBYTE4N, coordenada de textura.
float4 escala : register(c0); // 2/W, -2/H, -1, 1

struct Entrada { float3 pos : POSITION; float4 cor : COLOR0; float2 uv : TEXCOORD0; };
struct Saida { float4 pos : POSITION; float4 cor : COLOR0; float2 uv : TEXCOORD0; };

Saida vs(Entrada e) {
    Saida s;
    s.pos = float4(e.pos.x * escala.x + escala.z, e.pos.y * escala.y + escala.w, 0.0f, 1.0f);
    // UBYTE4N no Xenos lê os 4 bytes como palavra big-endian: os bytes r, g, b, a da memória chegam como
    // (a, b, g, r). Medido no console (F3, etapa 6).
    s.cor = e.cor.wzyx;
    s.uv = e.uv;
    return s;
}

sampler2D tex : register(s0);

float4 ps(Saida e) : COLOR0 {
    return tex2D(tex, e.uv) * e.cor;
}
