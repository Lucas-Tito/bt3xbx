#!/usr/bin/env python3
"""Teste local do mapa de larguras do save (sem dados do jogo). Sai com 0 só se tudo passa.

    teste_save_larguras.py            confere o mapa e roda os mutantes (cada um tem de ser reprovado)
    teste_save_larguras.py --mutante N   aplica o mutante N ao save_larguras.c (numa cópia) e roda só a conferência: sai com 1

A conferência (verifica):
  1. `save_larguras_tool confere` (C): cobertura exata dos três arquivos, larguras 1/2/4/8, alinhamento natural, troca
     de ordem (duas vezes devolve o original; uma vez inverte cada número na largura do mapa);
  2. as faixas tipadas batem com campos_save() e campos_iconsys() de save_entre.py, número a número: o mesmo
     conjunto de (deslocamento, largura) no que o save.h tipa, e nada tipado dentro dos opacos dele;
  3. o dbzsm.ico é uma faixa só, de bytes, do tamanho que o jogo grava (0x9C9A).
Regra do projeto: todo verificador é visto falhando de propósito; os mutantes abaixo fazem isso a cada execução.
"""
import importlib.util, pathlib, shutil, subprocess, sys, tempfile
import save_larguras as sl

AQUI = pathlib.Path(__file__).resolve().parent
DADOS, ICONSYS, ICO = "BASLUS-21678DBZT3", "icon.sys", "dbzsm.ico"


def carrega_save_entre():
    spec = importlib.util.spec_from_file_location("save_entre", AQUI / "save_entre.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def compara_tipado(rotulo, campos, nums):
    """campos: (desl, largura>0) tipados, ou (desl, -bytes) opacos. nums: [(desl, larg, cat)] do mapa."""
    erros = []
    tipados = {(o, w) for o, w in campos if w > 0}
    meus_t = {(o, w) for o, w, c in nums if c == sl.CATEGORIAS.index("tipado")}
    for o, w in sorted(tipados - meus_t):
        erros.append(f"{rotulo}: o save.h tipa {w} byte(s) em {o:#x} e o mapa não tem esse número tipado")
    for o, w in sorted(meus_t - tipados):
        erros.append(f"{rotulo}: o mapa tipa {w} byte(s) em {o:#x} e o save.h não")
    for o, n in ((o, -w) for o, w in campos if w < 0):  # nada tipado dentro de um opaco, nem atravessando a borda
        for p, w, c in nums:
            if p < o + n and p + w > o and c == sl.CATEGORIAS.index("tipado"):
                erros.append(f"{rotulo}: número tipado em {p:#x} dentro do opaco {o:#x}+{n:#x}")
    return erros


def verifica(conv, tmp):
    """lista de defeitos do mapa de `conv` (vazia = tudo certo)"""
    erros = []
    try:
        exe = sl.compila(tmp, conv)
    except RuntimeError as e:
        return [str(e)]
    r = subprocess.run([str(exe), "confere"], capture_output=True, text=True)
    if r.returncode:
        erros.append("save_larguras_tool confere saiu com %d: %s" % (r.returncode, r.stdout.strip().splitlines()[-1:] or r.stderr))
    mapa = sl.le_faixas(exe)
    se = carrega_save_entre()
    erros += compara_tipado("dados", se.campos_save(), sl.numeros(mapa[DADOS]))
    erros += compara_tipado("icon.sys", se.campos_iconsys(), sl.numeros(mapa[ICONSYS]))
    ico = mapa[ICO]
    if len(ico) != 1 or ico[0].desl != 0 or ico[0].larg != 1 or ico[0].qtd != 0x9C9A:
        erros.append("dbzsm.ico: esperava uma faixa de 0x9C9A bytes de largura 1")
    return erros


# mutantes: (descrição, texto que existe uma vez no .c, troca)
MUTANTES = [
    ("faixa sobreposta (o slot passa a ter 38 palavras)", '{0x0010, 36, 4, T, 1, 0,', '{0x0010, 38, 4, T, 1, 0,'),
    ("buraco (o bloco sem acesso de 0xA0 perde 4 bytes)", '{0x00A0, 0x168, 1, S, 1, 0,', '{0x00A0, 0x164, 1, S, 1, 0,'),
    ("largura errada num campo tipado (money em 2 bytes)", '{0x3028, 1, 4, T, 1, 0,', '{0x3028, 2, 2, T, 1, 0,'),
    ("largura errada num campo tipado (stageBits em 4 bytes)", '{0x0C10, 4, 8, T, 1, 0,', '{0x0C10, 8, 4, T, 1, 0,'),
    ("largura 3", '{0x0210, 2, 4, R, 10, 12,', '{0x0210, 2, 3, R, 10, 12,'),
    ("número sem alinhamento (level com u16 em desl ímpar)", '{0x183C, 2, 2, T, 97, 0x38,', '{0x183D, 2, 2, T, 97, 0x38,'),
    ("opaco resolvido marcado como tipado", '{0x0E0C, 3, 4, R, 1, 0,', '{0x0E0C, 3, 4, T, 1, 0,'),
    ("o ico some um byte", '{0x0000, 40090, 1, D', '{0x0000, 40089, 1, D'),
    ("o icon.sys perde o titleBreak (u16 vira dois bytes)", '{0x004, 2, 2, T, 1, 0,', '{0x004, 4, 1, T, 1, 0,'),
]


def aplica(n, destino):
    desc, velho, novo = MUTANTES[n]
    fonte = (sl.CONV / "save_larguras.c").read_text()
    if fonte.count(velho) != 1:
        raise SystemExit(f"mutante {n}: o texto '{velho}' aparece {fonte.count(velho)} vezes (esperava 1)")
    shutil.copy(sl.CONV / "save_larguras.h", destino)
    (destino / "save_larguras.c").write_text(fonte.replace(velho, novo))
    return desc


def main(a):
    if a[:1] == ["--mutante"]:
        n = int(a[1])
        with tempfile.TemporaryDirectory() as d:
            conv = pathlib.Path(d) / "conv"
            conv.mkdir()
            desc = aplica(n, conv)
            erros = verifica(conv, d)
            print(f"mutante {n} ({desc}): {len(erros)} defeito(s)")
            for e in erros[:3]:
                print("  " + e)
            return 1 if erros else 0
    with tempfile.TemporaryDirectory() as d:
        erros = verifica(sl.CONV, d)
    for e in erros[:20]:
        print("ERRO:", e)
    if erros:
        return 1
    print("mapa: ok (cobertura, larguras, troca de ordem, igual ao save.h tipado)")
    falhas = 0
    for n in range(len(MUTANTES)):
        r = subprocess.run([sys.executable, str(AQUI / "teste_save_larguras.py"), "--mutante", str(n)], capture_output=True,
                           text=True, cwd=AQUI)
        sai = r.returncode
        print(f"  mutante {n}: saída {sai} ({MUTANTES[n][0]})")
        if sai != 1:
            falhas += 1
            print(r.stdout, r.stderr)
    if falhas:
        print(f"{falhas} mutante(s) não foram reprovados: o teste não vale")
        return 1
    print("mutantes: todos reprovados com saída 1")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
