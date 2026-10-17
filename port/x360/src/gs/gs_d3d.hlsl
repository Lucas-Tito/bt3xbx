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

// ---- F5, etapa 7: a profundidade de um alvo de volta para a EDRAM (os dois restauros do exemplo FastDepthRestore
// do XDK). O vs_copia leva o retângulo; uv é o pixel sobre 1024 (o lado do alvo, TAM), no centro do pixel.

// Como cor (modo 1, exato em todos os bits): a superfície de cor posta na mesma base da profundidade da EDRAM recebe os
// bits da textura D24S8 lida como A8B8G8R8. Dentro de cada tile de 80 pixels, as metades de 40 colunas da
// profundidade e da cor estão trocadas: o pixel x da cor é o x + 40 da profundidade na metade esquerda e o x - 40 na
// direita. A amostragem é no ponto, presa na borda.
float4 ps_zcor(float2 uv : TEXCOORD0) : COLOR0 {
    float x = uv.x * 1024.0f;
    x += frac(x / 80.0f) < 0.5f ? 40.0f : -40.0f;
    return tex2D(tex, float2(x / 1024.0f, uv.y));
}

// Pelo oDepth (modo 2): a textura D24S8 lida como profundidade (no .x) e escrita como profundidade; o stencil não
// volta (o jogo não usa). Exato se a conversão de 24 bits para float e de volta não arredondar diferente: o
// autoteste do boot mede.
struct ZSaida { float4 cor : COLOR0; float z : DEPTH; };
ZSaida ps_zprof(float2 uv : TEXCOORD0) {
    ZSaida s;
    s.cor = 0.0f;
    s.z = tex2D(tex, uv).x;
    return s;
}

// ---- programas de vértice da VU1, família vu0 (F5, etapa 6): tradução de port/src/gs/shaders/vu0.vert
// Usada pelos programas 0, 1, 2a/2b, 7 e 8 (camadas 0 a 5 em misc.w). O pixel shader é o ps_gs acima.

// pos: xyz e o peso em w; nrm: a normal (ou a cor por vértice nas camadas 3 e 4); st: s, t, 1. Três FLOAT4, passo de 48
struct Vu0Entrada { float4 pos : POSITION; float4 nrm : NORMAL; float4 st : TEXCOORD0; };

// c0..c3 são do pixel shader: o vetor começa em c16. Layout da struct C Vu0Uniform (std140), sem tipo matriz do HLSL
// (evita a ambiguidade de linha e coluna): cada matriz são 4 COLUNAS seguidas.
//   [0..3] boneA, [4..7] boneB, [8] pivotA, [9] pivotB, [10..13] screen, [14] light, [15] color0, [16] color1,
//   [17] misc (x, y: XYOFFSET; z: máximo da profundidade; w: camada), [18] light2
float4 vu0c[19] : register(c16);

#define VU0_BONEA 0
#define VU0_BONEB 4
#define VU0_SCREEN 10
// mat4 M * vec4 v = M0*v.x + M1*v.y + M2*v.z + M3*v.w, com Mk a coluna k
#define VU0_M4(b, v) (vu0c[(b) + 0] * (v).x + vu0c[(b) + 1] * (v).y + vu0c[(b) + 2] * (v).z + vu0c[(b) + 3] * (v).w)
// mat3(M) * n = M0.xyz*n.x + M1.xyz*n.y + M2.xyz*n.z
#define VU0_M3(b, n) (vu0c[(b) + 0].xyz * (n).x + vu0c[(b) + 1].xyz * (n).y + vu0c[(b) + 2].xyz * (n).z)

GsSaida vs_vu0(Vu0Entrada e) {
    GsSaida s;
    float4 pivotA = vu0c[8], pivotB = vu0c[9];
    float4 light = vu0c[14], color0 = vu0c[15], color1 = vu0c[16], misc = vu0c[17], light2 = vu0c[18];
    float w = misc.w == 4.0f ? 1.0f : e.pos.w; // camada 4 (destroços, rígido): a 4ª palavra é um sinalizador, não um peso
    float3 pa = VU0_M4(VU0_BONEA, float4(e.pos.xyz - pivotA.xyz, 1.0f)).xyz;
    float3 pb = VU0_M4(VU0_BONEB, float4(e.pos.xyz - pivotB.xyz, 1.0f)).xyz;
    float4 sc = VU0_M4(VU0_SCREEN, float4(lerp(pb, pa, w), 1.0f));
    // Programas 2a/2b: a matriz da câmera da sombra é ortográfica com w constante NEGATIVO (medido: -862). O PS2 só
    // divide por ele; a GPU cortaria tudo como "atrás do olho". O mesmo ponto com w > 0:
    if (misc.w == 2.0f && sc.w < 0.0f) sc = -sc;
    // x, y e w idênticos ao GLSL (+Y para cima, o HALFPIXELOFFSET do dispositivo já faz o papel do meio pixel).
    // A profundidade fica presa em 0..w em TODAS as camadas: o PC desliga o corte por profundidade (depth clamp) e o
    // 360 não tem isso, então um z fora da faixa cortaria o triângulo inteiro. O vs_gs faz o mesmo com saturate.
    s.pos = float4((sc.x - misc.x * sc.w) / 512.0f - sc.w, sc.w - (sc.y - misc.y * sc.w) / 512.0f,
                   clamp(sc.z / misc.z, 0.0f, sc.w), sc.w);
    if (misc.w == 5.0f) {
        // programa 1, camada 1: a segunda textura do modelo lida pela direção da normal num quadro alinhado à câmera
        // (uma matriz só, sem mistura entre os ossos): (u, v) = 0.5 + 0.5 * (M * (n, 1)).xy
        float4 n1 = float4(e.nrm.xyz, 1.0f);
        s.cor = color1 / 255.0f;
        s.stq = float3(dot(light, n1) * 0.5f + 0.5f, dot(light2, n1) * 0.5f + 0.5f, 1.0f);
    } else if (misc.w >= 3.0f) {
        // programas 8 e 7: a cor de cada vértice (floats 0..255 na segunda quadword); o 7 leva o alfa da malha
        s.cor = clamp(floor(float4(e.nrm.rgb, misc.w == 4.0f ? color0.a : e.nrm.a)), 0.0f, 255.0f) / 255.0f;
        s.stq = float3(e.st.xy, 1.0f);
    } else if (misc.w != 1.0f) {
        s.cor = color0 / 255.0f;
        s.stq = float3(e.st.xy, 1.0f);
    } else {
        float na = dot(light.xyz, VU0_M3(VU0_BONEA, e.nrm.xyz)), nb = dot(light.xyz, VU0_M3(VU0_BONEB, e.nrm.xyz));
        s.cor = color1 / 255.0f;
        s.stq = float3((lerp(nb, na, w) + light.w) * 0.5f + 0.5f, 0.0f, 1.0f);
    }
    return s;
}

// ---- programas de vértice da VU1, famílias vu4 (o estágio, programa 4) e vu6 (a sombra no chão, programa 6) (F5,
// etapa 6, segunda parte): tradução de port/src/gs/shaders/vu4.vert e vu6.vert, com as mesmas entradas, constantes e
// convenções do vs_vu0 (a profundidade presa em 0..w). O pixel shader é o ps_gs.


// Tradução de port/src/gs/shaders/vu4.vert (o estágio, programa 4): só usa screen em [10..13] e misc em [17]. A cor
// vem em e.nrm (segunda quadword, floats 0..255) e o st segue com q = 1. O recorte de tiras da VU fica com a GPU.
GsSaida vs_vu4(Vu0Entrada e) {
    GsSaida s;
    float4 misc = vu0c[17];
    float4 sc = VU0_M4(VU0_SCREEN, e.pos);
    s.pos = float4((sc.x - misc.x * sc.w) / 512.0f - sc.w, sc.w - (sc.y - misc.y * sc.w) / 512.0f,
                   clamp(sc.z / misc.z, 0.0f, sc.w), sc.w);
    s.cor = clamp(floor(e.nrm), 0.0f, 255.0f) / 255.0f;
    s.stq = float3(e.st.xy, 1.0f);
    return s;
}

#define VU6_SHADOWCAM 0

// Tradução de port/src/gs/shaders/vu6.vert (a sombra no chão, programa 6): shadowCam em [0..3], scale.x em [8],
// texScale.xy em [9], screen em [10..13], misc em [17]. A cor vem em e.nrm; a terceira quadword não é usada.
GsSaida vs_vu6(Vu0Entrada e) {
    GsSaida s;
    float4 scale = vu0c[8], texScale = vu0c[9];
    float4 misc = vu0c[17];
    float4 p = float4(e.pos.xyz, 1.0f);
    float4 sc = VU0_M4(VU0_SCREEN, p);
    float2 st = (VU0_M4(VU6_SHADOWCAM, p).xy * scale.x + 1.0f) * 0.5f;
    s.pos = float4((sc.x - misc.x * sc.w) / 512.0f - sc.w, sc.w - (sc.y - misc.y * sc.w) / 512.0f,
                   clamp(sc.z / misc.z, 0.0f, sc.w), sc.w);
    s.cor = clamp(floor(e.nrm), 0.0f, 255.0f) / 255.0f;
    s.stq = float3(st * texScale.xy, 1.0f);
    return s;
}
