#!/usr/bin/env python3
"""Recompila somente a cópia descartável do PC já preparada nesta coleta."""
import os
import pathlib
import subprocess
import sys

from blur_pc import preparar_fase2, preparar_alinhamento, RAIZ


def construir(pc):
    pc = pc.resolve()
    if pc == RAIZ or (pc / ".git").exists() or not (pc / "port/src/blur_teste.c").exists():
        raise RuntimeError("exige a cópia isolada e instrumentada do PC")
    preparar_fase2(pc)
    preparar_alinhamento(pc)
    os.environ["BT3_CC"] = "clang64"
    os.chdir(pc)
    sys.path.insert(0, str(pc / "port/tools"))
    import undefined as u
    for nome in ("src/battle/stg_b.c", "src/battle/stg_c.c", "src/battle/eft_c.c", "src/sys/gfxm_a.c"):
        objeto, _, erro, _ = u.cc(pc / nome)
        if objeto is None:
            raise RuntimeError(nome + ": " + str(erro))
    for nome in ("plat_stub", "blur_teste"):
        fonte = pc / ("port/src/" + nome + ".c")
        cmd = ["gcc", "-m32", "-std=gnu99", "-c", "-O2", "-g", "-fno-pic", "-malign-double",
               "-fno-strict-aliasing", "-ffp-contract=off", "-fno-builtin", "-w", "-Iinclude", "-Iport/include",
               "-Iport/src", "-msoft-float", "-mno-sse", "-mno-mmx", "-DSF_FAST_CALLS",
               "-include", "math.h", "-include", "stdlib.h", "-include", "port_libm.h"]
        r = u.compile_ee(cmd, fonte, u.OBJ / ("pc_" + nome + ".o"))
        if r.returncode:
            raise RuntimeError(r.stderr)
    # SDL local anterior à API PNG do PC atual. Só a cópia de diagnóstico:
    # substituições e arte PNG da janela ficam indisponíveis, sem tocar nos alvos.
    png = pc / "port/build/blur_png.h"
    png.write_text("""#include <SDL3/SDL.h>
static SDL_Surface *SDL_LoadPNG(const char *nome) { (void)nome; return NULL; }
""")
    for nome in ("gs_draw", "gs_gl"):
        r = u.run(["gcc", "-m32", "-std=gnu99", "-c", "-O2", "-g", "-fno-pic", "-msse2", "-mfpmath=sse",
                   "-fno-strict-aliasing", "-w", "-Iport/build/gen/gs", "-include", str(png),
                   "port/src/gs/" + nome + ".c", "-o", str(u.OBJ / ("gs_" + nome + ".o"))],
                  capture_output=True, text=True)
        if r.returncode:
            raise RuntimeError(r.stderr)
    subprocess.run([sys.executable, "port/tools/link.py"], check=True)
    (pc / "port/build/shots").mkdir(parents=True, exist_ok=True)


if __name__ == "__main__":
    construir(pathlib.Path(sys.argv[1]))
