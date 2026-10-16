#!/usr/bin/env python3
"""F5, etapa 4: confere as cores byte a byte dos pacotes da GPU contra o inventário (plano/F5/inventario.md, seção 1).

    cores.py [RAIZ] [--lista]     (em qualquer máquina; só lê src/ e include/ de RAIZ, padrão a raiz do repositório)

O contrato (formatos/gpu.md, seções 1, 2 e 6): no pacote, RGBAQ é uma palavra de 32 bits do host com R no byte de
menor peso; no big-endian, a memória da palavra é A, B, G, R. Repete a busca do inventário sobre o texto dos fontes
originais (os corpos em `#if 0` contam: o port os compila, port/tools/portsrc.py), com os dois ramos de cada
`#ifdef PORT_BIG_ENDIAN` separados, e exige:
  1. quartetos: todo membro de struct `u8 NOME[4]` com nome de cor (rgb, col, shadow, tint, ambient...) e todo
     `u8 r, g, b, a` (numa linha ou em quatro) tem classe em TIPOS. Pacote por índice (A) e CPU ficam fora de
     qualquer `#ifdef PORT_BIG_ENDIAN`; pacote por layout (B, e os já corrigidos antes, L) tem `u8 a, b, g, r` no
     ramo big-endian e o original no `#else`. Tipo novo ou tipo da lista que sumiu: falha.
  2. gravações: cada `->CAMPO[i] =` (campo de cor) e cada `->r/g/b/a =` de cada função (ou macro) tem classe e
     quantidade em ESCRITAS, contadas no texto original (fora do ramo big-endian). Pacote A: toda gravação no `#else`
     de um `#ifdef PORT_BIG_ENDIAN` cujo ramo big-endian é o mesmo texto com o índice i trocado por 3-i (a
     correção da proposta A); uma gravação de A fora disso é "cor de pacote sem a correção". B, L e CPU: fora de
     qualquer ramo (B e L já se corrigem no layout; uma troca a mais inverteria de volta). Gravação nova numa função
     sem classe, quantidade diferente ou função que sumiu: falha.
  3. imagens (CLUT e texels, que conservam os bytes do PS2): as funções de IMAGEM existem e não têm ramo big-endian;
  4. cópias de pacote: cada linha de COPIAS existe, fora de ramo big-endian (preservar os bytes, sem segunda troca).
Limites: é busca de texto, como a do inventário. Uma gravação por ponteiro de bytes com cast, por um campo
`unkNN[4]` sem nome de cor ou por `memset` não é vista; a revisão por tipo da seção 1.5 do inventário a complementa.
Sai com 1 em qualquer falha, inclusive fonte ausente ou nenhuma gravação encontrada.
"""
import collections, pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[3]

# (arquivo, dono, membro) -> classe. dono: a struct que contém o quarteto ("local" para variável de função).
# A: u8 rgba[4] de pacote, corrigido em cada gravação; B: quarteto com nome de pacote, permutado na declaração na F5;
# L: o mesmo, permutado antes da F5; CPU: cor semântica, lida por índice ou nome antes de virar número.
TIPOS = {}
for _cl, _txt in (
    ("A", """src/battle/eft_g.c EftGsVtx rgba; src/battle/eft_g.c EftStripPkt rgba; src/battle/eft_g.c EftLinePkt rgba;
             src/battle/eft_e.c EftSteamPkt rgba; src/battle/eft_d_b.c EftSteamPkt rgba;
             src/battle/eft_e.c EftWaterGsVtx rgba; src/battle/eft_e.c EftWaterSpritePkt rgba;
             src/battle/eft_ae.c EftAeGsVtx rgba; src/battle/eft_p_b.c EftPGsVtx rgba; src/battle/eft_s.c EftSGsVtx rgba;
             include/battle/eft_t.h EftTGsVtx rgba; src/battle/eft_t_c.c EftUGsVtx rgba"""),
    ("B", """src/battle/eft_a.c EftGfxRgbaq rgba; src/battle/eft_b.c EftBRgbaq rgba; src/battle/eft_c.c EftSurfRgbaq rgba;
             src/battle/eft_ad_b.c EftAdRgbaq rgba; include/battle/eft_q.h EftQRgbaq rgba;
             include/battle/eft_v.h EftVRgbaq rgba; src/battle/eft_w.c EftWRgbaq rgba; src/battle/eft_x.c EftXRgbaq rgba;
             include/battle/eft_y.h EftYRgbaq rgba; include/battle/eft_aa.h EftAaRgbaq rgba;
             include/battle/eft_ac.h EftAcRgbaq rgba"""),
    ("L", """src/battle/eft_d.c EftTriVtx rgba; src/battle/eft_n.c EftNVtx rgba; src/battle/eft_ad_c.c EftSprPkt rgba;
             src/sys/gfxm_a.c rgbaq rgba"""),
    ("CPU", """include/battle/eft_aa.h EftGroundRgba c; include/battle/eft_ad.h EftMesh rgba;
             include/battle/eft_ae.h EftSprKey color; include/battle/eft_b.h EftGeyserRec rgba;
             include/battle/eft_c.h StgSurfRec colorA; include/battle/eft_c.h StgSurfRec colorB;
             include/battle/eft_e.h EftStageMarker color; include/battle/eft_g.h EftSmokeSrc ambient;
             include/battle/eft_g.h EftSmokeSrc diffuse; include/battle/eft_o.h EftOColor rgba;
             include/battle/eft_t.h EftRay rgba; include/battle/eft_z.h EftZCol c;
             include/battle/eft_z.h EftGndDustArg colA; include/battle/eft_z.h EftGndDustArg colB;
             include/battle/eft_z.h EftGndDustPart colA; include/battle/eft_z.h EftGndDustPart colB;
             include/battle/eft_z.h EftGndDustStage colA; include/battle/eft_z.h EftGndDustStage colB;
             include/battle/hud_a.h HudSprite rgba; include/battle/hud_b.h HudBSprite rgba;
             include/battle/hud_c.h HudCSprite rgba; include/battle/hud_d.h HudDSprite rgba;
             include/battle/hud_e.h HudESprite rgba; include/battle/stg_c.h StgHazeParam color;
             include/battle/view_a.h TextBox color; include/battle/view_a.h TextBox color2;
             include/battle/view_b.h TextBoxDraw color; include/battle/view_b.h TextBoxDraw shadow;
             include/battle/view_b.h TextBoxFull color; include/battle/view_b.h TextBoxFull shadow;
             include/sys/color_fade.h ColorFade rgba; include/sys/gfxm_a.h StgDepthTintParam rgba;
             include/sys/gfxm_b.h GfxWaterStage rgba; src/battle/eft_n.c local al; src/sys/gfxm_d_c.c local c"""),
):
    for _e in _txt.split(";"):
        _a, _d, _m = _e.split()
        TIPOS[(_a, _d, _m)] = _cl

# (arquivo, função ou macro, "indice"|"nome") -> (classe, gravações no texto original). Os intervalos de linhas de
# cada uma estão no inventário (A: seção 1.2, B: 1.3, L e CPU: 1.4).
ESCRITAS = {}
for _cl, _txt in (
    ("A", """eft_g.c EftUtil_DrawTri 12; eft_g.c EFT_STORM_SPRITE 4; eft_g.c EftStorm_DrawRainLines 4;
             eft_e.c EftSteam_DrawQuad 4; eft_e.c EftWater_DrawBillboard 4; eft_e.c EftWater_DrawSprayQuad 16;
             eft_e.c EftWater_QueueTri 12; eft_ae.c EftSprAnim_QueueTri 12; eft_p_b.c EftGlow_DrawParts 16;
             eft_s.c EftChain_DrawStrand 16; eft_s.c EftRay_DrawQuad2D 16; eft_t_b.c EftStreak_DrawScreen 16;
             eft_t_c.c EftShotFx_DrawQuad 16"""),
    ("B", """eft_a.c EftGfx_DrawSprite 16; eft_b.c EftPrim_DrawBillboard 4; eft_b.c EftPrim_DrawTriangle 12;
             eft_b.c EftBubble_DrawOne 4; eft_b.c EftGeyser_DrawColumn 16; eft_c.c EftSurf_QueueTri 12;
             eft_ad_b.c EftMesh_QueueTri 24; eft_ad_b.c EftMesh_SendTri 12; eft_q.c EftFlash_Draw 4;
             eft_q.c EftTrail_DrawSprite 4; eft_u_b.c EftPtcl_DrawAxisQuads 16; eft_w.c EftLink_DrawQuad 16;
             eft_w.c EftLink_DrawBillboard 4; eft_x.c EftPart10_DrawQuad 16; eft_x.c EftPart10_DrawBillboard 4;
             eft_y.c EftQuad_DrawSprite 16; eft_z.c EftLine_DrawSprite 16; eft_aa.c EftGndDust_DrawQuad 16;
             eft_aa.c EftBlade_Draw 12; eft_aa.c EftBlind_DrawOverlay 4; eft_ab_c.c EftZap_DrawQuad 16"""),
    ("L", """eft_d.c EFT_SURF_FILL_PKT 12; eft_d.c EftSurf_DrawTriDirect 12; eft_n.c EftAura_DrawFlames 16;
             eft_n.c EftBolt_Draw 16; eft_ad_c.c EftSpr_DrawFlat 4; eft_ad_c.c EftSpr_DrawRot 4;
             ../sys/gfxm_a.c StgPanBlur_DrawView 4"""),
    ("CPU", """eft_a.c EftHit_SetShape2 2; eft_a.c EftHit_SetShape3 2; eft_a.c EftHit_SetShape4 2; eft_a.c EftHit_SetShape5 2;
             eft_a.c EftHit_SetShape6 2; eft_a.c EftHit_SetShapeBoxes 2; eft_a.c EftHit_SetShapeSpheres 2;
             eft_ad_b.c EftMesh_Init 4; eft_ad_b.c EftMesh_SetColor 4; eft_b.c EftBubble_Add 3;
             eft_c.c EftWeather_Init 4 indice; eft_c.c EftWeather_SetColor 3 indice; eft_e.c EftWaterDrop_Spawn 3;
             eft_e.c EftWaterMist_Spawn 3; eft_e.c EftWaterRing_Spawn 3; eft_e.c EftWaterSpray_Spawn 3;
             eft_h.c EftEmit_LoadSet 4; eft_n.c EftBolt_AddFlash 8; eft_n.c EftBolt_Shape 3;
             eft_n.c EftBolt_UpdateFlashes 2; eft_o.c EftRays_Init 4 indice; eft_o.c EftRays_LerpKey 3;
             eft_o.c EftRays_Step 3 indice; eft_s.c EFT_RAY_REROLL 1; eft_s.c EftRay_Setup 4;
             eft_s.c EftRay_StartHit 4 indice; eft_s.c EftRay_StartScreen 4 indice; eft_v_c.c EftLink_InitKeys 8;
             eft_v_c.c EftLink_UpdateKeys 8; eft_x.c EftPart10_SetKey 8; eft_x.c EftPart10_StartKeys 8;
             eft_x.c EftPart10_UpdateKeys 8; eft_y.c EftQuad_Animate 1; eft_y.c EftQuad_CalcKeyDeltas 1;
             eft_y.c EftQuad_SetLastKey 3; hud_c_c.c HudSprite_InitPlain 4; hud_c_c.c HudSprite_SetColor 4;
             hud_d_b.c HudNotice_UpdateSpinA 1; hud_d_b.c HudNotice_UpdateSpinB 1; hud_d_b.c HudNotice_UpdateTrail 2;
             hud_e.c HudNotice_UpdateBanner 2; view_a_e.c TextBox_SetColor 4 indice; view_b.c TB_ATTACH 16 indice;
             ../sys/color_fade.c ColorFade_Draw 1; ../sys/color_fade.c ColorFade_StartIn 3;
             ../sys/color_fade.c ColorFade_StartOut 3; ../sys/gfxm_a.c GfxPostQuad_Set 4;
             ../sys/gfxm_a.c GfxPostQuad_SetColor 4; ../sys/gfxm_a.c StgDepthTint_LoadParams 8;
             ../sys/gfxm_c.c FlashDraw_TransformQuad 12 indice; ../sys/gfxm_c.c FlashShape_Draw 4 indice;
             ../sys/loading.c SPRITE 4; eft_ab_c.c EftZap_InitLine 6 indice; eft_c.c EftSurf_DrawMeshes 2 indice;
             eft_c.c EftSurf_Init 8 indice; eft_z.c EftGndDust_InitTemplate 16 indice;
             view_a_e.c TextBox_SetColor2 4 indice; ../menu/menu_c_e.c CharSel_Input 4 indice;
             ../menu/menu_e_b.c TeamSel_Input 4 indice"""),
):
    for _e in _txt.split(";"):
        _p = _e.split()
        _a = str(pathlib.PurePosixPath("src/battle", _p[0])).replace("src/battle/../", "src/")
        _k = _p[3] if len(_p) > 3 else ("indice" if _cl == "A" else "nome")
        ESCRITAS[(_a, _p[1], _k)] = (_cl, int(_p[2]))

# Texels e CLUT (bytes IMAGE, gpu.md seções 1 e 4): não recebem correção nenhuma.
IMAGEM = [tuple(e.split()) for e in """src/battle/eft_a.c EftGfx_LightClutDiffuse; src/battle/eft_a.c EftGfx_LightClutSpecular;
    src/battle/eft_a.c EftGfx_LerpClut; src/sys/gfxm_a.c StgPanBlur_BuildClut; src/sys/gfxm_a.c ObjOutline_BuildClut;
    src/sys/gfxm_a.c StgGlare_BuildClut; src/sys/gfxm_a.c StgDepthTint_BuildClut; src/sys/gfxm_a.c ObjGlow_BuildClut;
    src/sys/gfxm_b.c GfxAlphaKey_BuildClut; src/sys/gfxm_b.c GfxDepthFog_BuildClut; src/battle/stg_c.c StgHaze_BuildClut;
    src/battle/stg_b.c StgFog_BuildClut; src/battle/eft_c.c EftSurf_RenderPalettes""".split(";")]
# Cópias de pacote para pacote (inventário 1.4): preservar os bytes.
COPIAS = [("src/sys/gfxm_a.c", "StgPanBlur_DrawView", "memcpy(p, &pkt.gifTag"),
          ("src/battle/eft_ad_b.c", "EftMesh_SendTri", "memcpy(dst, &pkt.gifTag"),
          ("src/sys/gfx_ot.c", "Ot_Send", "memcpy(dst, prim + 1, prim->qwc << 4)")]

FUNC = re.compile(r"^(?!typedef|struct|union|enum|extern|static\s+const|const|INCLUDE_)[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;]*$")
DEF = re.compile(r"^#\s*define\s+(\w+)\(")
COR = re.compile(r"(?i)rgb|col|shadow|ambient|diffuse|tint|clr|^c$|^al$")
VET = re.compile(r"^\s*(?:/\*.*?\*/\s*)?(?:u8|unsigned char)\s+(\w+)\s*\[\s*4\s*\]\s*;")
NOM = re.compile(r"^\s*(?:/\*.*?\*/\s*)?(?:u8|unsigned char)\s+(r\s*,\s*g\s*,\s*b\s*,\s*a|a\s*,\s*b\s*,\s*g\s*,\s*r)\s*;")
UM = re.compile(r"^\s*(?:/\*.*?\*/\s*)?(?:u8|unsigned char)\s+([rgba])\s*;")
G_IND = re.compile(r"(?:->|\.)(\w+)\[\s*([^]]+?)\s*\]\s*(?:[-+*/|&^]|<<|>>)?=(?!=)")
G_NOM = re.compile(r"(?:->|\.)([rgba])\s*(?:[-+*/|&^]|<<|>>)?=(?!=)")
BE = re.compile(r"#\s*(?:ifdef\s+PORT_BIG_ENDIAN\b|if\s+defined\s*\(?\s*PORT_BIG_ENDIAN\b)")


def ler(raiz):
    """[(arquivo, linhas, lados, funções, blocos)]: lado de cada linha (None fora de ramo big-endian, "be" no ramo
    big-endian, "orig" no #else); função ou macro em que a linha está; blocos [(linhas be, linhas orig)]."""
    fontes = sorted(list((raiz / "src").rglob("*.c")) + list((raiz / "include").rglob("*.h")))
    if not fontes:
        raise SystemExit(f"cores.py: nenhum fonte em {raiz}/src e {raiz}/include")
    saida = []
    for f in fontes:
        L = f.read_text(errors="replace").split("\n")
        lados, funcs, blocos, pilha, func = [], [], [], [], None
        for i, t in enumerate(L):
            s = t.strip()
            if re.match(r"#\s*if", s):
                pilha.append([bool(BE.match(s)), "be", [], []])
                if pilha[-1][0]:
                    blocos.append(pilha[-1])
            elif re.match(r"#\s*(else|elif)", s) and pilha:
                pilha[-1][1] = "orig" if pilha[-1][1] == "be" else "outro"
            elif re.match(r"#\s*endif", s) and pilha:
                pilha.pop()
            lado = next((p[1] for p in reversed(pilha) if p[0]), None)
            alvo = next((p for p in reversed(pilha) if p[0]), None)
            if alvo is not None and not re.match(r"#\s*(if|else|elif|endif)", s):
                (alvo[2] if alvo[1] == "be" else alvo[3]).append(i)
            m = DEF.match(t) or FUNC.match(t)
            if m:
                func = m.group(1)
            lados.append(lado)
            funcs.append(func)
        saida.append((str(f.relative_to(raiz)), L, lados, funcs, [(b[2], b[3]) for b in blocos]))
    return saida


def dono(L, i, funcs):
    d = 0
    for j in range(i, len(L)):
        t = re.sub(r"/\*.*?\*/|//.*", "", L[j])
        for k, ch in enumerate(t):
            if ch == "{":
                d += 1
            elif ch == "}":
                d -= 1
                if d < 0:
                    m = re.match(r"\}\s*(?:__attribute__\s*\(\(.*?\)\)\s*)?(\w+)\s*(?:\[[^]]*\])?\s*;", t[k:])
                    return m.group(1) if m else "local"
    return "?"


def comentario(t):
    """Linha só de comentário (um "/* 0x30 */" antes do código não conta: é o deslocamento do membro)."""
    s = re.sub(r"^\s*/\*.*?\*/", "", t).strip()
    return s == "" or s.startswith(("//", "*", "/*"))


def troca_indices(t):
    """A gravação de A no ramo big-endian: o índice constante i do campo de cor vira 3-i."""
    return G_IND.sub(lambda m: m.group(0).replace(f"[{m.group(2)}]", f"[{3 - int(m.group(2))}]", 1)
                     if COR.search(m.group(1)) and m.group(2) in "0123" else m.group(0), t)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    raiz = pathlib.Path(args[0]).resolve() if args else ROOT
    lista = "--lista" in sys.argv
    falhas = []
    fontes = ler(raiz)
    por_arquivo = {a: (L, lados, funcs, blocos) for a, L, lados, funcs, blocos in fontes}

    # 1. quartetos
    achados = collections.defaultdict(set)
    for a, L, lados, funcs, _ in fontes:
        for i, t in enumerate(L):
            if comentario(t):
                continue
            m = VET.match(t)
            if m and COR.search(m.group(1)):
                achados[(a, dono(L, i, funcs), m.group(1))].add((lados[i], "vetor", i + 1))
                continue
            m = NOM.match(t)
            if m:
                ordem = "abgr" if m.group(1).replace(" ", "").startswith("a") else "rgba"
                achados[(a, dono(L, i, funcs), "rgba")].add((lados[i], ordem, i + 1))
            elif UM.match(t) and UM.match(t).group(1) == "r" and i + 3 < len(L) and \
                    [UM.match(L[i + k]).group(1) if UM.match(L[i + k]) else None for k in (1, 2, 3)] == ["g", "b", "a"]:
                achados[(a, dono(L, i, funcs), "rgba")].add((lados[i], "rgba", i + 1))
    for k, v in sorted(achados.items()):
        cl = TIPOS.get(k)
        formas = {(lado, o) for lado, o, _ in v}
        linhas = ",".join(str(n) for _, _, n in sorted(v, key=lambda x: x[2]))
        if lista:
            print(f"tipo {cl or '?'} {k[0]}:{linhas} {k[1]}.{k[2]}")
        if cl is None:
            falhas.append(f"quarteto sem classe: {k[0]}:{linhas} {k[1]}.{k[2]} (TIPOS)")
        elif cl in ("B", "L") and formas != {("be", "abgr"), ("orig", "rgba")}:
            falhas.append(f"pacote {cl} sem a permutação do layout (u8 a, b, g, r no ramo PORT_BIG_ENDIAN e o original "
                          f"no #else): {k[0]}:{linhas} {k[1]}")
        elif cl in ("A", "CPU") and any(lado is not None for lado, _ in formas):
            falhas.append(f"{cl} com a declaração num ramo PORT_BIG_ENDIAN (A e CPU não se permutam no layout): "
                          f"{k[0]}:{linhas} {k[1]}.{k[2]}")
    for k in TIPOS:
        if k not in achados:
            falhas.append(f"quarteto da lista não encontrado: {k[0]} {k[1]}.{k[2]} (TIPOS desatualizado ou fonte ausente)")

    # 2. gravações
    contas = collections.Counter()
    vistas = collections.defaultdict(list)
    for a, L, lados, funcs, _ in fontes:
        for i, t in enumerate(L):
            if comentario(t):
                continue
            n_ind = sum(1 for m in G_IND.finditer(t) if COR.search(m.group(1)))
            n_nom = len(G_NOM.findall(t))
            for tipo, n in (("indice", n_ind), ("nome", n_nom)):
                if n:
                    vistas[(a, funcs[i], tipo)].append((i, lados[i], n))
    for k, ocorr in sorted(vistas.items(), key=lambda x: (x[0][0], x[0][1] or "", x[0][2])):
        orig = sum(n for _, lado, n in ocorr if lado != "be")
        be = sum(n for _, lado, n in ocorr if lado == "be")
        cl, esperado = ESCRITAS.get(k, (None, None))
        linhas = ",".join(str(i + 1) for i, lado, _ in ocorr if lado != "be")
        if lista:
            print(f"grava {cl or '?'} {k[0]} {k[1]} {k[2]} {orig}: {linhas}")
        if cl is None:
            falhas.append(f"gravação de cor sem classe: {k[0]}:{linhas} em {k[1]} ({k[2]}; ESCRITAS)")
            continue
        contas[cl] += orig
        if orig != esperado:
            falhas.append(f"{k[0]} {k[1]}: {orig} gravações {k[2]} no texto original, o inventário tem {esperado}")
        if cl == "A":
            sem = [i + 1 for i, lado, _ in ocorr if lado is None]
            if sem:
                falhas.append(f"cor de pacote sem a correção (gravação de A fora de #ifdef PORT_BIG_ENDIAN): "
                              f"{k[0]}:{','.join(map(str, sem))} em {k[1]}")
            if be != orig:
                falhas.append(f"{k[0]} {k[1]}: {be} gravações no ramo PORT_BIG_ENDIAN para {orig} no original")
        elif any(lado is not None for _, lado, _ in ocorr):
            falhas.append(f"{cl} com gravação num ramo PORT_BIG_ENDIAN (troca a mais): {k[0]}:"
                          f"{','.join(str(i + 1) for i, lado, _ in ocorr if lado is not None)} em {k[1]}")
    for k in ESCRITAS:
        if k not in vistas:
            falhas.append(f"gravações da lista não encontradas: {k[0]} {k[1]} {k[2]} (ESCRITAS ou fonte)")
    # cada bloco big-endian com gravação de A: o #else é o mesmo texto com o índice trocado
    for a, L, lados, funcs, blocos in fontes:
        for be, orig in blocos:
            if not any(ESCRITAS.get((a, funcs[i], "indice"), ("",))[0] == "A" for i in orig + be):
                continue
            tb = [L[i] for i in be]
            to = [troca_indices(L[i]) for i in orig]
            if tb != to:
                j = next((n for n in range(min(len(tb), len(to))) if tb[n] != to[n]), min(len(tb), len(to)))
                ln = be[j] + 1 if j < len(be) else (be[-1] + 1 if be else "?")
                falhas.append(f"ramo PORT_BIG_ENDIAN de A em {a}:{ln} não é o #else com o índice i trocado por 3-i")

    # 3. imagens
    for a, f in IMAGEM:
        if a not in por_arquivo:
            falhas.append(f"fonte ausente: {a}")
            continue
        L, lados, funcs, _ = por_arquivo[a]
        idx = [i for i in range(len(L)) if funcs[i] == f]
        if not idx:
            falhas.append(f"função de IMAGEM não encontrada: {a} {f}")
        elif any(lados[i] is not None for i in idx) or any(BE.match(L[i].strip()) for i in idx):
            falhas.append(f"IMAGEM (CLUT/texel) com ramo PORT_BIG_ENDIAN: {a} {f}")

    # 4. cópias
    for a, f, texto in COPIAS:
        if a not in por_arquivo:
            falhas.append(f"fonte ausente: {a}")
            continue
        L, lados, funcs, _ = por_arquivo[a]
        idx = [i for i in range(len(L)) if funcs[i] == f and texto in L[i]]
        if len(idx) != 1:
            falhas.append(f"cópia de pacote não encontrada uma vez: {a} {f} {texto!r} ({len(idx)})")
        elif lados[idx[0]] is not None:
            falhas.append(f"cópia de pacote num ramo PORT_BIG_ENDIAN (segunda troca): {a}:{idx[0] + 1} {f}")

    print(f"cores.py: gravações A {contas['A']}, B {contas['B']}, L {contas['L']}, CPU {contas['CPU']}; "
          f"quartetos {len(achados)}; imagens {len(IMAGEM)}; cópias {len(COPIAS)}")
    for x in falhas:
        print(f"  FALHA: {x}")
    if falhas or contas["A"] == 0 or contas["B"] == 0:
        print(f"cores.py: FALHA ({len(falhas)})")
        sys.exit(1)
    print("cores.py: ok")


if __name__ == "__main__":
    main()
