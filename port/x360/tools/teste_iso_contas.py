#!/usr/bin/env python3
"""Contas da tela de conversão (categorias, peso dos menus, batidas, progresso) com inventário sintético, sem a ISO.
Compila conv/iso_contas.c real com src/teste_instalar/teste_contas.c e vê cópias estragadas falharem com saída 1."""
import os
from pathlib import Path
import subprocess
import tempfile

RAIZ = Path(__file__).resolve().parents[3]
CONV = RAIZ / "port/x360/conv"
TESTE = RAIZ / "port/x360/src/teste_instalar/teste_contas.c"


def compilar(pasta, contas, cabecalho):
    # A cópia do iso_conv.h fica na pasta do mutante e vem antes do original no caminho de inclusão.
    exe = pasta / "teste_contas"
    subprocess.run([os.environ.get("CC", "gcc"), "-std=gnu89", "-O1", "-Wall", "-Wextra", "-Werror",
                    "-Wdeclaration-after-statement", "-Wno-unused-parameter", "-I" + str(pasta), "-I" + str(CONV), str(TESTE), str(contas),
                    "-o", str(exe)], check=True)
    return exe


def main():
    contas = (CONV / "iso_contas.c").read_text()
    cab = (CONV / "iso_conv.h").read_text()
    with tempfile.TemporaryDirectory(prefix="iso-contas-") as tmp:
        pasta = Path(tmp)
        (pasta / "iso_conv.h").write_text(cab)  # o original, só para a pasta ter o mesmo layout
        r = subprocess.run([str(compilar(pasta, CONV / "iso_contas.c", cab))], capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(f"FALHA: o código real foi recusado: {r.stderr}")
        print(r.stdout.strip(), flush=True)
        mutantes = [
            ("sem o peso de menu", "iso_conv.h", "#define IC_PESO_MENU 12", "#define IC_PESO_MENU 0", "peso de menu ausente"),
            ("total da barra sem o extra", "iso_contas.c", "return 3 * iso + c->extra_total;", "return 3 * iso;", "não inclui o extra"),
            ("contagem que não fecha", "iso_contas.c", "    c->feito[IsoContas_Categoria(formato)]++;\n",
             "    if (IsoContas_Categoria(formato) != IC_CAT_CENARIOS)\n        c->feito[IsoContas_Categoria(formato)]++;\n",
             "a contagem não fecha"),
            ("batida que não avança", "iso_contas.c", "    c->batidas++;\n", "", "batidas não avançaram"),
            ("sons contados como dados de luta", "iso_contas.c", 'strcmp(formato, "som")', 'strcmp(formato, "soms")', "formato som"),
            ("menus fora da categoria", "iso_contas.c", 'strncmp(formato, "menu_", 5)', 'strncmp(formato, "menu", 0)', None),
            ("verificação que retrocede", "iso_contas.c", "return 2 * iso + c->extra_total + pos_kib;", "return 2 * iso + pos_kib;",
             "retrocedeu"),
        ]
        for i, (nome, arq, de, para, acusacao) in enumerate(mutantes):
            fonte = contas if arq == "iso_contas.c" else cab
            if de not in fonte:
                raise SystemExit(f"FALHA: o mutante '{nome}' não achou o alvo")
            d = pasta / f"m{i}"
            d.mkdir()
            novo = fonte.replace(de, para)
            (d / "iso_conv.h").write_text(novo if arq == "iso_conv.h" else cab)
            (d / "iso_contas.c").write_text(novo if arq == "iso_contas.c" else contas)
            r = subprocess.run([str(compilar(d, d / "iso_contas.c", cab))], capture_output=True, text=True)
            if r.returncode != 1 or "FALHA:" not in r.stderr or (acusacao and acusacao not in r.stderr):
                raise SystemExit(f"FALHA: mutante '{nome}' não foi recusado (código {r.returncode}): {r.stderr}")
            print(f"ok: mutante '{nome}' recusado com saída 1: {r.stderr.strip()}", flush=True)
    print("teste_iso_contas: tudo certo", flush=True)


if __name__ == "__main__":
    main()
