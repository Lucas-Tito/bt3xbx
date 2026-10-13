// F3, etapa 7: os shaders do backend D3D9 do 360 (gs_d3d.cpp), compilados pelo fxc do XDK no build
// (port/x360/tools/build_f2.sh), nunca no boot. Tradução de port/src/gs/shaders/gs.vert e gs.frag, sem a saída do
// byte de alfa à parte (outAux): no 360 o alvo é um só (ver o cabeçalho do gs_d3d.cpp).

// ---- primitivas do GS (Vtx de gs_draw.h: posição em pixels do GS, cor em 4 bytes, s, t, q)

struct GsEntrada { float3 pos : POSITION; float4 cor : COLOR0; float3 stq : TEXCOORD0; };
struct GsSaida { float4 pos : POSITION; float4 cor : COLOR0; float3 stq : TEXCOORD0; };

GsSaida vs_gs(GsEntrada e) {
    GsSaida s;
    // 0..1024 pixels do GS cobrem o alvo inteiro; +Y do recorte é o alto da imagem, como na SDL GPU
    // A profundidade presa em 0..1: o GS aceita z além do formato do buffer (os glifos do texto do cartão vêm com
    // 0x0FFFFFF0 num Z24, z / zmax perto de 16), o PC liga GL_DEPTH_CLAMP (e a SDL GPU desliga o corte), e o D3D9 do
    // 360 cortaria a primitiva inteira.
    s.pos = float4(e.pos.x / 512.0f - 1.0f, 1.0f - e.pos.y / 512.0f, saturate(e.pos.z), 1.0f);
    // UBYTE4N no Xenos: os bytes r, g, b, a da memória chegam como (a, b, g, r) (medido na etapa 6)
    s.cor = e.cor.wzyx;
    s.stq = e.stq;
    return s;
}

float4 modo : register(c0); // x: textura (0 nenhuma, 1 da memória do GS, 2 um alvo, 3 pacote), y: TFX, z: TCC, w: teste de alfa (0 desligado, senão ATST + 1)
float4 misc : register(c1); // x: AREF (0..255); y: DATE (0, 1 passa com o bit 7 zerado, 2 com ele ligado); z: FBA; w: pixels de saída por pixel do GS na arte 2D (0: não)
float4 rect : register(c2); // modo.x == 2: a faixa de uv que pode ser lida
float4 alvo : register(c3); // xy: 1 / tamanho do alvo em pixels (a cópia para o DATE); zw: tamanho da textura em texels

sampler2D tex : register(s0);
sampler2D dateTex : register(s1); // cópia da cor do alvo, para o teste de alfa do destino

struct GsPixel { float4 cor : COLOR0; float3 stq : TEXCOORD0; float2 vpos : VPOS; };

float4 ps_gs(GsPixel e) : COLOR0 {
    float k = 255.0f / 128.0f;
    // o centro do pixel em qualquer convenção do VPOS (inteiro ou com o meio pixel): o gl_FragCoord do GL
    float2 f = floor(e.vpos) + 0.5f;
    if (misc.y != 0.0f) {
        // TEST.DATE: o bit 7 do alfa já gravado. Sem o byte à parte, vale o alfa da cor: ela guarda min(A / 0x80, 1),
        // e o bit 7 está ligado exatamente quando ela chega a 1.
        // (o microcódigo do Xenos não tem `discard`: o fxc do XDK só aceita clip)
        bool ligado = tex2D(dateTex, f * alvo.xy).a >= 254.5f / 255.0f;
        clip(ligado != (misc.y > 1.5f) ? -1.0f : 1.0f);
    }
    float3 rgb = e.cor.rgb;
    float a = e.cor.a * k;
    // o gradiente fora de qualquer desvio (o fxc recusa tex2D com gradiente dentro de desvio dinâmico)
    float2 uv0 = e.stq.xy / e.stq.z;
    float2 dx = ddx(uv0), dy = ddy(uv0);
    float2 uv = uv0;
    if (misc.w != 0.0f) {
        // arte 2D: a coordenada onde o GS a toma, no canto do pixel do GS a que este pixel pertence (gs.frag).
        // (o caminho do pacote de texturas, misc.w < 0, não existe no 360: sem pacote, ver TexPack_Count)
        float s = abs(misc.w);
        float2 g = floor(f / s) * s;
        uv = uv0 - (dx * (f.x - g.x) + dy * (f.y - g.y));
        // Desenhada texel por pixel do GS, a arte 2D é lida no ponto, como o rasterizador de referência faz (o texel
        // u >> 4 do pixel inteiro): a coordenada do GS cai na borda do texel, e o filtro linear ali faz a média de
        // quatro (o título saía macio). Ampliada ou reduzida, segue com o filtro que o jogo pediu.
        float2 passo = float2(abs(dx.x) + abs(dy.x), abs(dx.y) + abs(dy.y)) * s * alvo.zw; // texels por pixel do GS
        if (abs(passo.x - 1.0f) < 0.02f && abs(passo.y - 1.0f) < 0.02f) {
            uv = (floor(uv * alvo.zw + 1.0f / 64.0f) + 0.5f) / alvo.zw;
        }
    }
    if (modo.x == 2.0f) {
        uv = clamp(uv, rect.xy, rect.zw);
    }
    float4 t = tex2Dgrad(tex, uv, dx, dy);
    if (modo.x != 0.0f) {
        if (modo.x != 2.0f) {
            t.a *= k; // textura da memória do GS: o alfa do GS (0x80 opaco, até 0xFF)
        }
        if (modo.y == 0.0f) {
            rgb = t.rgb * e.cor.rgb * k;
            if (modo.z != 0.0f) a = t.a * a;
        } else if (modo.y == 1.0f) {
            rgb = t.rgb;
            if (modo.z != 0.0f) a = t.a;
        } else {
            rgb = t.rgb * e.cor.rgb * k + e.cor.aaa;
            if (modo.z != 0.0f) a = (modo.y == 2.0f) ? t.a + a : t.a;
        }
    }
    if (modo.w != 0.0f) {
        // o GS compara inteiros: arredonda (um valor igual ao AREF falharia por erro de float)
        float ag = floor(a * 128.0f + 0.5f);
        float fn = modo.w - 1.0f, ref = misc.x;
        bool passa = fn == 0.0f ? false : fn == 1.0f ? true : fn == 2.0f ? ag < ref : fn == 3.0f ? ag <= ref :
                     fn == 4.0f ? abs(ag - ref) < 0.5f : fn == 5.0f ? ag >= ref : fn == 6.0f ? ag > ref : abs(ag - ref) >= 0.5f;
        clip(passa ? 1.0f : -1.0f);
    }
    // FBA: o GS liga o bit 7 do alfa gravado, ou seja, alfa >= 0x80, que na cor é 1
    if (misc.z != 0.0f) {
        a = 1.0f;
    }
    return float4(saturate(rgb), saturate(a));
}

// ---- cópia de uma textura para a tela: a volta de um alvo para a EDRAM e o present

struct CopiaEntrada { float2 pos : POSITION; float2 uv : TEXCOORD0; };
struct CopiaSaida { float4 pos : POSITION; float2 uv : TEXCOORD0; };

CopiaSaida vs_copia(CopiaEntrada e) {
    CopiaSaida s;
    s.pos = float4(e.pos, 0.0f, 1.0f);
    s.uv = e.uv;
    return s;
}

float4 ps_copia(float2 uv : TEXCOORD0) : COLOR0 {
    return tex2D(tex, uv);
}
