#!/usr/bin/env python3
"""Mutantes do teste do psu: cada cópia estragada de conv/psu.c tem de ser acusada por teste_psu.py
(saída 1). Regra do projeto: todo verificador é visto falhando de propósito.
Uso: <python com mymcplus> port/x360/tools/teste_psu_mutantes.py"""
import os, shutil, subprocess, sys, tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
FONTE = os.path.join(AQUI, "..", "conv", "psu.c")

MUTANTES = [
    ("sem completar até 1024 (escrita e leitura)",
     "return (n + (PSU_ALINHA - 1UL)) / PSU_ALINHA * PSU_ALINHA;", "return n;"),
    ("comprimento da pasta sem os dois do . e ..",
     "(unsigned long)s->nArquivos + 2UL", "(unsigned long)s->nArquivos"),
    ("nome sem conferir o zero final",
     "if (i == 0 || i == PSU_NOME) return PSU_E_NOME;", "if (i == 0) return PSU_E_NOME;"),
    # (a checagem "fl > tam - pos" é redundante em hosts de 64 bits com a de baixo, mas vale no console, de 32
    #  bits, onde arredonda(0xFFFFFFFF) dá a volta; por isso não tem mutante dela)
    ("falta de completamento aceita",
     "if (arredonda(fl) > tam - pos) return PSU_E_TRUNCADO; /* falta o completamento */", ""),
    ("sobra no fim aceita", "if (pos != tam) return PSU_E_SOBRA;", ""),
    ("BT3 sem conferir tamanhos", "if (s->arquivos[j].tamanho != tams[i]) return PSU_E_BT3_TAMANHO;", ""),
    ("segundo e minuto trocados na leitura", "d->seg = p[1];\n    d->min = p[2];", "d->seg = p[2];\n    d->min = p[1];"),
    ("inteiro de 32 bits em big-endian",
     "(unsigned long)p[0] | (unsigned long)p[1] << 8 | (unsigned long)p[2] << 16 | (unsigned long)p[3] << 24;",
     "(unsigned long)p[3] | (unsigned long)p[2] << 8 | (unsigned long)p[1] << 16 | (unsigned long)p[0] << 24;"),
    ("modo de pasta aceito como arquivo", "return (modo & (MODO_ARQUIVO | MODO_DIR | MODO_EXISTE)) == (MODO_ARQUIVO | MODO_EXISTE);",
     "return (modo & MODO_EXISTE) != 0;"),
]

def main():
    src = open(FONTE).read()
    tmp = tempfile.mkdtemp(prefix="mutantes_psu_")
    ruins = 0
    for i, (nome, velho, novo) in enumerate(MUTANTES):
        assert src.count(velho) == 1, "trecho do mutante %r não achado uma vez" % nome
        arq = os.path.join(tmp, "psu_m%d.c" % i)
        open(arq, "w").write(src.replace(velho, novo))
        r = subprocess.run([sys.executable, os.path.join(AQUI, "teste_psu.py"), "--fonte", arq],
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        acusou = r.returncode == 1
        print("%-50s %s" % (nome, "acusado (saída 1)" if acusou else "NÃO ACUSADO (saída %d)" % r.returncode))
        if not acusou:
            ruins += 1
    shutil.rmtree(tmp)
    sys.exit(1 if ruins else 0)

main()
