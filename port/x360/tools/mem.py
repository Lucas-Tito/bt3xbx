#!/usr/bin/env python3
"""Escreve <programa>.mem (onde ficam as variáveis globais do jogo) para uma build sem janela da F1, com o
port/tools/make_state.py do upstream: assim o checksum e o dump de estado (state.c) cobrem também as
globais, não só o heap. Uso: mem.py port/build/<pasta>/bt3_<pasta>"""
import pathlib, sys, types

ROOT = pathlib.Path(__file__).resolve().parents[3]
exe = pathlib.Path(sys.argv[1]).resolve()
tc = types.ModuleType("toolchain")
tc.ROOT, tc.EXE, tc.OBJ, tc.DATA = ROOT, exe, exe.parent / "obj", exe.parent / "obj_data"
sys.modules["toolchain"] = tc
fonte = (ROOT / "port/tools/make_state.py").read_text().rsplit("\nmain()", 1)[0]
ns = {"__name__": "make_state", "__file__": str(ROOT / "port/tools/make_state.py")}
exec(compile(fonte, "make_state.py", "exec"), ns)
original = ns["is_game"]
# os arquivos da F1 (sem janela, rastreio, chamadas, inversão) e os stubs não são estado do jogo
ns["is_game"] = lambda obj: original(obj) and not pathlib.Path(obj.split("(")[0]).name.startswith(("x360_", "stubs"))
ns["main"]()
