#!/usr/bin/env python3
"""Instrumenta uma cópia descartável do PC para repetir a entrada do roteiro local.

Uso: blur_pc.py PASTA_DO_GIT_ARCHIVE. Não usar na raiz de um checkout.
As alterações são só de diagnóstico, não são propostas de merge do upstream.
"""
import pathlib
import sys

RAIZ = pathlib.Path(__file__).resolve().parents[3]


def preparar(pc):
    if pc.resolve() == RAIZ or (pc / ".git").exists():
        raise SystemExit("use somente uma cópia descartável em port/build/blur")
    if (pc / "port/src/blur_teste.c").exists():
        raise SystemExit("cópia já instrumentada; extraia novamente o arquivo do upstream")
    grupos = {
        "src/battle/stg_b.c": [("StgFog_Draw", 0)],
        "src/battle/stg_c.c": [("StgHaze_Draw", 3), ("StgBlur_DrawAll", 2)],
        "src/sys/gfxm_a.c": [("StgPanBlur_Draw", 1), ("StgDepthTint_Draw", 4), ("GfxPost_DrawGlow", 5)],
    }
    import re
    for nome, funcoes in grupos.items():
        p = pc / nome
        s = p.read_text()
        for funcao, indice in funcoes:
            m = re.search(r"^void " + funcao + r"\([^\n]*\) \{", s, re.M)
            if m is None:
                raise ValueError("função ausente: " + funcao)
            extra = ""
            if indice == 1:
                extra = """        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            StgPanBlur_UpdateView(&gStgPanBlur->view[0]);
            if (Battle_IsSplitScreen()) StgPanBlur_UpdateView(&gStgPanBlur->view[1]);
        }
"""
            s = s[:m.end()] + "\n    if (Port_DesfoqueOff(%d)) {\n" % indice + extra + "        return;\n    }" + s[m.end():]
        p.write_text("extern int Port_DesfoqueOff(int);\n" + s)
    p = pc / "src/battle/eft_c.c"
    s = p.read_text()
    inicio = s.index("void EftWeatherPtcl_Draw(")
    antes, depois = s[:inicio], s[inicio:]
    depois = depois.replace("if (EftStage_IsDrawOn()) {", "if (!Port_DesfoqueOff(6) && EftStage_IsDrawOn()) {", 1)
    p.write_text("extern int Port_DesfoqueOff(int);\n" + antes + depois)
    (pc / "port/src/blur_teste.c").write_text((RAIZ / "port/x360/headless/desfoque.c").read_text())
    # O desbloqueio do roteiro não faz parte da gravação do controle. Repete só esse
    # passo no mesmo vblank, antes do menu principal, e limita a coleta após a pausa.
    p = pc / "port/src/plat_stub.c"
    s = p.read_text().replace("int scePad2Read(int socket, unsigned char *data) {", """int scePad2Read(int socket, unsigned char *data) {
    static int desbloqueou;
    extern uint32_t gSaveData;
    extern void Save_UnlockAll(void *);
    if (!desbloqueou && gPortVBlanks >= 2834 && gSaveData != 0) {
        Save_UnlockAll((void *)(uintptr_t)gSaveData);
        desbloqueou = 1;
        printf("blur_pc: desbloqueou no vblank %u\\n", gPortVBlanks);
    }
    if (gPortVBlanks > 7000) { printf("blur_pc: coleta encerrada\\n"); exit(0); }
""", 1)
    p.write_text(s)
    # Limita a coleta aos mesmos números do GS, lidos da rodada da x86.
    p = pc / "port/src/gs/gs_gl.c"
    s = p.read_text()
    marca = "static int blur_quadro(unsigned quadro) {\n"
    marca += '''    const char *p = getenv("BT3_SHOT_LISTA");
    while (p != NULL && *p) {
        char *fim;
        unsigned n = (unsigned)strtoul(p, &fim, 10);
        if (fim == p) { p++; continue; }
        if (n == quadro) return 1;
        p = fim;
    }
    return 0;
}
'''
    pos = s.index("static void frame_end(")
    s = s[:pos] + marca + s[pos:]
    s = s.replace("if (every > 0 && gGsFrame % (unsigned)every == 0 && best >= 0",
                  "if (blur_quadro(gGsFrame) && every > 0 && best >= 0", 1)
    p.write_text(s)
    print("blur_pc: cópia instrumentada; exige BT3_PAD_PLAY da x86 e cartão inicialmente vazio")


def preparar_fase2(pc):
    """Abre cada tratamento na cópia de PC já instrumentada, para comparação."""
    if pc.resolve() == RAIZ or (pc / ".git").exists():
        raise SystemExit("use somente a cópia descartável do PC")
    if (pc / "BLUR_FASE2").exists():
        return
    for nome, prefixo, chave in (
        ("src/battle/stg_b.c", "PORT_LOWRES", "BT3_DISTANCIA_NATIVA"),
        ("src/sys/gfxm_a.c", "PORT_GLOW", "BT3_PC_BRILHO_NATIVO"),
    ):
        p = pc / nome
        s = p.read_text()
        for ponta in ("BEGIN", "END"):
            antes = "Port_GsMarker(%s_%s);" % (prefixo, ponta)
            if s.count(antes) != 1:
                raise RuntimeError("marcador ausente ou repetido: " + antes)
            s = s.replace(antes, 'if (Port_DesfoqueNative("%s")) %s' % (chave, antes))
        p.write_text("extern int Port_DesfoqueNative(const char *);\n" + s)
    p = pc / "port/src/gs/gs_draw.c"
    s = p.read_text()
    antes = "if (block && SCALE > 1 && ((gGs.tex1[ctx] >> 5) & 1) &&\n                (shrink || ("
    depois = """if (SCALE > 1 && ((gGs.tex1[ctx] >> 5) & 1) &&
                ((shrink && (block || (getenv("BT3_DISTANCIA_MEDIA") != NULL && atoi(getenv("BT3_DISTANCIA_MEDIA"))))) || (block && """
    if s.count(antes) != 1:
        raise RuntimeError("condição da redução do PC mudou")
    s = s.replace(antes, depois)
    p.write_text(s)
    (pc / "port/src/blur_teste.c").write_text(
        (RAIZ / "port/x360/headless/desfoque.c").read_text() + """
int Port_DesfoqueNative(const char *nome) {
    const char *v = getenv(nome);
    return v != NULL && atoi(v) != 0;
}
""")
    (pc / "BLUR_FASE2").write_text("Cópia de diagnóstico; nenhuma mudança no upstream de referência.\n")


def preparar_alinhamento(pc):
    """Registra o estado real nas capturas; números do GS sozinhos não o provam."""
    if (pc / "BLUR_ALINHAMENTO").exists():
        return
    p = pc / "port/src/gs/gs_gl.c"
    s = p.read_text()
    antes = "            GLsizei w = 512 * SCALE, h = 448 * SCALE;"
    if s.count(antes) != 1:
        raise RuntimeError("ponto de captura do PC mudou")
    s = s.replace(antes, "            extern void Port_BlurQuadro(unsigned);\n"
                  "            Port_BlurQuadro(gGsFrame);\n" + antes)
    p.write_text(s)
    p = pc / "port/src/blur_teste.c"
    with p.open("a") as f:
        f.write('''
#include <stdint.h>
/* Visões locais de diagnóstico: mode em MenuProgress+0x18 e state em BtlSeq+0. */
void Port_BlurQuadro(unsigned quadro) {
    extern uint32_t gProgress, gBtlSeq;
    extern unsigned gPortVBlanks;
    int modo = gProgress ? *(int *)((uintptr_t)gProgress + 0x18) : -1;
    int seq = gBtlSeq ? *(int *)(uintptr_t)gBtlSeq : -1;
    printf("blur_pc: captura gs %u vblank %u modo %d seq %d\\n", quadro, gPortVBlanks, modo, seq);
}
''')
    (pc / "BLUR_ALINHAMENTO").write_text("Estado registrado junto de cada captura.\n")


def preparar_cadeias(pc):
    """Conserva os marcadores de Sharp mesmo com os alvos nativos desligados."""
    if pc.resolve() == RAIZ or (pc / ".git").exists():
        raise SystemExit("use somente a cópia descartável do PC")
    if (pc / "BLUR_CADEIAS").exists():
        preparar_despacho(pc)
        return
    for nome, prefixo, chave, codigos in (
        ("src/battle/stg_b.c", "PORT_LOWRES", "BT3_DISTANCIA_NATIVA", ("0x22", "0x23")),
        ("src/sys/gfxm_a.c", "PORT_GLOW", "BT3_PC_BRILHO_NATIVO", ("0x20", "0x21")),
    ):
        p = pc / nome
        s = p.read_text()
        for ponta, codigo in zip(("BEGIN", "END"), codigos):
            antes = 'if (Port_DesfoqueNative("%s")) Port_GsMarker(%s_%s);' % (chave, prefixo, ponta)
            if s.count(antes) != 1:
                raise RuntimeError("marcador condicionado ausente ou repetido: " + antes)
            s = s.replace(antes, "Port_GsMarker(%s);" % codigo)
        p.write_text(s)
    p = pc / "port/src/gs/gs_draw.c"
    s = p.read_text()
    trocas = {
        "return sInGlowChain && t >= 0 && target_lowres(gsTargets[t].fbp);":
        '''return ((sInGlowChain == 1 && Port_DesfoqueNative("BT3_PC_BRILHO_NATIVO")) ||
            (sInGlowChain == 2 && Port_DesfoqueNative("BT3_DISTANCIA_NATIVA"))) &&
           t >= 0 && target_lowres(gsTargets[t].fbp);''',
        "if (effect == 0x20 || effect == 0x21)":
        "if (effect == 0x20 || effect == 0x21 || effect == 0x22 || effect == 0x23)",
        "sInGlowChain = effect == 0x20;":
        "sInGlowChain = effect == 0x20 ? 1 : effect == 0x22 ? 2 : 0;",
    }
    for antes, depois in trocas.items():
        if s.count(antes) != 1:
            raise RuntimeError("política da cadeia mudou: " + antes)
        s = s.replace(antes, depois)
    p.write_text("extern int Port_DesfoqueNative(const char *);\n" + s)
    (pc / "BLUR_CADEIAS").write_text("Sharp reconhece as cadeias independentemente dos alvos nativos.\n")
    preparar_despacho(pc)


def preparar_despacho(pc):
    """Migra a distância para códigos livres no PC, fora dos menus 16:9."""
    if (pc / "BLUR_DESPACHO").exists():
        return
    trocas = {
        "src/battle/stg_b.c": {
            "Port_GsMarker(0x22);": "Port_GsMarker(0x80);",
            "Port_GsMarker(0x23);": "Port_GsMarker(0x81);",
        },
        "port/src/gs/gs_draw.c": {
            "if (effect == 0x20 || effect == 0x21 || effect == 0x22 || effect == 0x23)":
            "if (effect == 0x20 || effect == 0x21 || effect == 0x80 || effect == 0x81)",
            "sInGlowChain = effect == 0x20 ? 1 : effect == 0x22 ? 2 : 0;":
            "sInGlowChain = effect == 0x20 ? 1 : effect == 0x80 ? 2 : 0;",
        },
    }
    for nome, mudancas in trocas.items():
        p = pc / nome
        s = p.read_text()
        for antes, depois in mudancas.items():
            if s.count(antes) != 1:
                raise RuntimeError("despacho da distância mudou: " + antes)
            s = s.replace(antes, depois)
        p.write_text(s)
    (pc / "BLUR_DESPACHO").write_text("Distância usa 0x80/0x81; menus conservam 0x22/0x23.\n")


if __name__ == "__main__":
    if len(sys.argv) > 2 and sys.argv[2] == "--fase2":
        preparar_fase2(pathlib.Path(sys.argv[1]))
    else:
        preparar(pathlib.Path(sys.argv[1]))
