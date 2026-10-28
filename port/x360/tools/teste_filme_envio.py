#!/usr/bin/env python3
"""Compila o DMA real e o corpo do SyncPath do 360 em C89; mutantes precisam falhar por asserção."""
from pathlib import Path
import re
import resource
import subprocess
import tempfile

raiz = Path(__file__).resolve().parents[3]
stub = (raiz / "port/src/plat_stub.c").read_text()
trecho = stub.split("#ifdef PORT_X360\n/* O DMA da GIF", 1)[1].split("#else", 1)[0]
funcao = re.search(r"int sceGsSyncPath\(\).*?\}", trecho, re.S).group()
header = (raiz / "port/x360/src/gs/captura_pasta.h").read_text()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp)
    for nome, corpo, h in (
        ("atual", funcao, header),
        ("sem_consumir", funcao.replace("Port_DmaChcr(2);", ""), header),
        ("ordem_invertida", funcao.replace("Port_DmaChcr(2); Port_GsEspera();", "Port_GsEspera(); Port_DmaChcr(2);"), header),
        ("prefixo_duplicado", funcao, header.replace("e += 6;", "e += 0;")),
        ("pasta_invalida", funcao, header.replace("n > 30", "n > 31")),
    ):
        (p / "sync.c").write_text("#include <stdint.h>\nextern volatile uint32_t *Port_DmaChcr(int);\n"
                                  "extern void Port_GsEspera(void);\n" + corpo)
        (p / "captura_pasta.h").write_text(h)
        subprocess.run(["gcc", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-O2", "-I", str(p),
                        str(raiz / "port/src/plat_gs.c"), str(p / "sync.c"),
                        str(raiz / "port/x360/headless/teste_filme_envio.c"), "-o", str(p / "teste")], check=True)
        r = subprocess.run([str(p / "teste")], capture_output=True, text=True)
        if nome == "atual":
            assert r.returncode == 0, r.stderr
            print(r.stdout.strip())
        else:
            assert r.returncode == -6 and "Assertion" in r.stderr, (nome, r.returncode, r.stderr)
            print(f"controle {nome}: compilou e foi recusado por asserção")
print("tudo certo: DMA e pastas em C89, quatro controles recusados")
