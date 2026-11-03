#!/usr/bin/env python3
"""Teste local da conversão do save (port/x360/conv/save_conv.c), com saves SINTÉTICOS: bytes aleatórios, a soma válida
calculada aqui, em Python, a partir da definição do jogo (McCard_Checksum), sem usar o C. Nenhum dado do jogo.

    teste_save_conv.py                 roda a conferência e depois os mutantes (cada um tem de ser reprovado)
    teste_save_conv.py --mutante N     aplica o mutante N ao save_conv.c (numa cópia) e roda só a conferência: sai com 1

A conferência (verifica):
  1. ida e volta: .psu do PS2 -> 360 -> .psu devolve os mesmos bytes (com as datas passadas), em saves diferentes;
  2. o 360 tem a soma válida na ordem do 360 e passa na regra da build big-endian (save_entre.py preve --build ppc);
  3. cada número do mapa de larguras ficou com a ordem invertida, os de largura 1 iguais, o .ico igual;
  4. a volta tem a soma válida em little-endian;
  5. erros: soma estragada no PS2, soma estragada no 360, save que não é do BT3, arquivo faltando (dos dois lados).
Regra do projeto: todo verificador é visto falhando de propósito; os mutantes abaixo fazem isso a cada execução.
"""
import importlib.util, pathlib, random, shutil, struct, subprocess, sys, tempfile
import save_larguras as sl
import save_conv as sc

AQUI = pathlib.Path(__file__).resolve().parent
DADOS, ICONSYS, ICO = "BASLUS-21678DBZT3", "icon.sys", "dbzsm.ico"
TAM = {DADOS: 0x4000, ICONSYS: 964, ICO: 40090}
DATA = (30, 20, 10, 5, 7, 2008)  # seg, min, hora, dia, mes, ano
DATA_TXT = "2008-07-05T10:20:30"


def carrega_save_entre():
    spec = importlib.util.spec_from_file_location("save_entre", AQUI / "save_entre.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def soma(d):
    """McCard_Checksum, da definição: bytes depois dos 8 primeiros; par (relativo) uma vez, ímpar vezes 256; 16 bits"""
    s = 0
    for i, x in enumerate(d[8:]):
        s = (s + (x << 8 if i & 1 else x)) & 0xFFFF
    return s


def com_soma(d, ordem):
    d = bytearray(d)
    s = soma(d)
    d[:8] = struct.pack(ordem + "II", s >> 8, s & 0xFF)
    return bytes(d)


def sintetico(rng):
    """os três arquivos na ordem do PS2, soma válida (little-endian)"""
    return {ICONSYS: bytes(rng.randrange(256) for _ in range(TAM[ICONSYS])),
            ICO: bytes(rng.randrange(256) for _ in range(TAM[ICO])),
            DADOS: com_soma(bytes(rng.randrange(256) for _ in range(TAM[DADOS])), "<")}


def entrada(modo, tam, nome):
    seg, mi, h, d, me, a = DATA
    tod = struct.pack("<BBBBBBH", 0, seg, mi, h, d, me, a)
    return struct.pack("<HHI", modo, 0, tam) + tod + bytes(8) + tod + bytes(0x40 - 0x20) + nome.encode().ljust(0x1C0, b"\0")


def faz_psu(arqs, pasta=DADOS, ordem=(ICONSYS, ICO, DADOS)):
    out = entrada(0x8427, len(ordem) + 2, pasta) + entrada(0x8427, 0, ".") + entrada(0x8427, 0, "..")
    for n in ordem:
        out += entrada(0x8497, len(arqs[n]), n) + arqs[n]
        out += bytes((-len(arqs[n])) % 1024)
    return out


def grava_pasta(d, arqs):
    d = pathlib.Path(d)
    d.mkdir(parents=True, exist_ok=True)
    for n, b in arqs.items():
        (d / n).write_bytes(b)
    return d


def le_pasta(d):
    return {n: (pathlib.Path(d) / n).read_bytes() for n in TAM}


def esperado_360(arqs, mapa):
    """cada número do mapa invertido, o resto igual; a soma refeita em big-endian"""
    out = {}
    for n, k in ((DADOS, "dados"), (ICONSYS, "iconsys")):
        b = bytearray(arqs[n])
        for f in mapa[n]:
            for q in range(f.qtd):
                p = f.desl + q * f.larg
                b[p:p + f.larg] = b[p:p + f.larg][::-1]
        out[n] = bytes(b)
    out[DADOS] = com_soma(out[DADOS], ">")
    out[ICO] = arqs[ICO]
    return out


def verifica(conv, tmp):
    erros = []
    tmp = pathlib.Path(tmp)
    try:
        exe = sc.compila(tmp, conv)
    except RuntimeError as e:
        return [str(e)]
    mapa = sl.carrega()
    se = carrega_save_entre()
    rng = random.Random(360)

    def e(cond, msg):
        if not cond:
            erros.append(msg)
        return cond

    for caso in range(4):
        arqs = sintetico(rng)
        if caso == 3:  # soma com o byte baixo 0 e alto 0: palavras zero nas duas ordens
            d = bytearray(arqs[DADOS]); d[0x3FFE] = (d[0x3FFE] - soma(d)) & 0xFF
            d[0x3FFF] = (d[0x3FFF] - (soma(d) >> 8)) & 0xFF
            assert soma(d) == 0
            arqs[DADOS] = com_soma(d, "<")
        psu = tmp / f"s{caso}.psu"
        psu.write_bytes(faz_psu(arqs))
        p360 = tmp / f"p{caso}"
        p360.mkdir()
        r = sc.roda(exe, "psu-para-360", psu, p360)
        if not e(r.returncode == 0, f"caso {caso}: psu-para-360 saiu com {r.returncode}: {r.stderr.strip()}"):
            continue
        got = le_pasta(p360)
        exp = esperado_360(arqs, mapa)
        e(soma(got[DADOS]) == (struct.unpack(">I", got[DADOS][:4])[0] << 8 | struct.unpack(">I", got[DADOS][4:8])[0]),
          f"caso {caso}: a soma do 360 não confere na ordem do 360")
        for n in TAM:
            e(got[n] == exp[n], f"caso {caso}: {n} no 360 difere do esperado")
        r2, porque = se.preve(p360 / DADOS, "ppc")
        e(r2 == "carrega", f"caso {caso}: save_entre preve ppc: {r2} ({porque})")
        e(max(struct.unpack(">II", got[DADOS][:8])) <= 0xFF, f"caso {caso}: palavras da soma acima de 0xFF")
        # cada número invertido, cada byte de largura 1 igual (fora das 8 primeiras posições, da soma)
        for n in (DADOS, ICONSYS):
            for f in mapa[n]:
                for q in range(f.qtd):
                    p = f.desl + q * f.larg
                    if n == DADOS and p < 8:
                        continue
                    a, b = arqs[n][p:p + f.larg], got[n][p:p + f.larg]
                    if not e(b == a[::-1], f"caso {caso}: {n} +{p:#x} (largura {f.larg}) não ficou invertido"):
                        break
                else:
                    continue
                break
        e(got[ICO] == arqs[ICO], f"caso {caso}: o .ico mudou")
        r = sc.roda(exe, "confere", p360)
        e(r.returncode == 0 and r.stdout.strip() == "360 ok", f"caso {caso}: confere da pasta 360: {r.stdout!r}")
        r = sc.roda(exe, "confere", psu)
        e(r.returncode == 0 and r.stdout.strip() == "ps2 ok", f"caso {caso}: confere do psu: {r.stdout!r}")
        # volta
        volta = tmp / f"v{caso}.psu"
        r = sc.roda(exe, "360-para-psu", p360, volta, "--data", DATA_TXT)
        if e(r.returncode == 0, f"caso {caso}: 360-para-psu saiu com {r.returncode}: {r.stderr.strip()}"):
            e(volta.read_bytes() == psu.read_bytes(), f"caso {caso}: a volta não devolve os mesmos bytes do .psu")
            r = sc.roda(exe, "confere", volta)
            e(r.stdout.strip() == "ps2 ok", f"caso {caso}: confere da volta: {r.stdout!r}")

    # erros
    arqs = sintetico(rng)

    def falha_com(rotulo, args, texto, saida=None):
        r = sc.roda(exe, *args)
        e(r.returncode == 1, f"{rotulo}: saiu com {r.returncode}, esperado 1")
        e(texto in r.stderr, f"{rotulo}: erro {r.stderr.strip()!r} sem {texto!r}")
        if saida is not None:
            e(not pathlib.Path(saida).exists() or not any(pathlib.Path(saida).iterdir()), f"{rotulo}: gravou saída")

    ruim = dict(arqs); d = bytearray(ruim[DADOS]); d[0x1000] ^= 0x01; ruim[DADOS] = bytes(d)
    (tmp / "ruim.psu").write_bytes(faz_psu(ruim))
    (tmp / "o1").mkdir()
    falha_com("soma ruim no PS2", ["psu-para-360", tmp / "ruim.psu", tmp / "o1"], "PS2 save is damaged (checksum)", tmp / "o1")
    r = sc.roda(exe, "confere", tmp / "ruim.psu")
    e(r.returncode == 1 and r.stdout.strip() == "danificado", f"confere de soma ruim: {r.stdout!r}")

    boa360 = tmp / "boa360"
    (tmp / "boa.psu").write_bytes(faz_psu(arqs))
    boa360.mkdir()
    r = sc.roda(exe, "psu-para-360", tmp / "boa.psu", boa360)
    if not e(r.returncode == 0, f"o save bom não converteu: {r.stderr.strip()}"):
        return erros
    ruim360 = tmp / "ruim360"
    shutil.copytree(boa360, ruim360)
    d = bytearray((ruim360 / DADOS).read_bytes()); d[0x2000] ^= 0x10; (ruim360 / DADOS).write_bytes(bytes(d))
    falha_com("soma ruim no 360", ["360-para-psu", ruim360, tmp / "x.psu"], "save is damaged (checksum)")
    e(not (tmp / "x.psu").exists(), "soma ruim no 360: gravou o .psu")
    # a pasta 360 com a soma na ordem do PS2 (não convertida) também é recusada
    ps2pasta = grava_pasta(tmp / "ps2pasta", arqs)
    falha_com("pasta na ordem do PS2", ["360-para-psu", ps2pasta, tmp / "y.psu"], "save is damaged (checksum)")

    (tmp / "outro.psu").write_bytes(faz_psu(arqs, pasta="BASLUS-21677DBZT3"))
    falha_com("não é do BT3", ["psu-para-360", tmp / "outro.psu", tmp / "o1"], "not a Budokai Tenkaichi 3 USA save", tmp / "o1")
    (tmp / "falta.psu").write_bytes(faz_psu(arqs, ordem=(ICONSYS, DADOS)))
    falha_com("arquivo faltando no psu", ["psu-para-360", tmp / "falta.psu", tmp / "o1"], "missing a file", tmp / "o1")
    sem = tmp / "sem"
    shutil.copytree(boa360, sem)
    (sem / ICONSYS).unlink()
    falha_com("arquivo faltando no 360", ["360-para-psu", sem, tmp / "z.psu"], "cannot read")
    curto = tmp / "curto"
    shutil.copytree(boa360, curto)
    (curto / ICO).write_bytes(b"x" * 100)
    falha_com("arquivo curto no 360", ["360-para-psu", curto, tmp / "w.psu"], "wrong size")
    return erros


# Mutantes: (descrição, [(trecho, troca)]) sobre save_conv.c; cada trecho tem de existir e a troca vale em todas as ocorrências.
MUTANTES = [
    ("PS2 para 360 sem recalcular a soma", [("    SaveConv_GravaSoma(saida->dados, SAVECONV_360);\n", "")]),
    ("PS2 para 360 grava as palavras da soma em little-endian", [("SaveConv_GravaSoma(saida->dados, SAVECONV_360)", "SaveConv_GravaSoma(saida->dados, SAVECONV_PS2)")]),
    ("360 para PS2 grava as palavras da soma em big-endian", [("SaveConv_GravaSoma(tmp->dados, SAVECONV_PS2)", "SaveConv_GravaSoma(tmp->dados, SAVECONV_360)")]),
    ("360 para PS2 sem recalcular a soma", [("    SaveConv_GravaSoma(tmp->dados, SAVECONV_PS2);\n", "")]),
    ("icon.sys sem trocar a ordem (os dois sentidos)", [("    if (SaveLarg_Inverte(saida->iconsys, SAVELARG_TAM_ICONSYS, SAVELARG_ARQ_ICONSYS) != 0) return SAVECONV_E_TAMANHO;\n", ""),
                                                      ("    if (SaveLarg_Inverte(tmp->iconsys, SAVELARG_TAM_ICONSYS, SAVELARG_ARQ_ICONSYS) != 0) return SAVECONV_E_TAMANHO;\n", "")]),
    ("PS2 para 360 sem conferir a soma de entrada", [("    if (!SaveConv_ConferirSoma(da->dados, SAVECONV_PS2)) return SAVECONV_E_SOMA_PS2;\n", "")]),
    ("360 para PS2 sem conferir a soma de entrada", [("    if (!SaveConv_ConferirSoma(entrada->dados, SAVECONV_360)) return SAVECONV_E_SOMA;\n", "")]),
    ("dados sem trocar a ordem (PS2 para 360)", [("    if (SaveLarg_Inverte(saida->dados, SAVELARG_TAM_DADOS, SAVELARG_ARQ_DADOS) != 0) return SAVECONV_E_TAMANHO;\n", "")]),
    ("a soma conta o byte 8 como ímpar", [("(i - 8) & 1", "(i - 7) & 1")]),
    ("a conferência aceita se só uma palavra confere", [("return w0 == (unsigned long)(s >> 8) && w1 == (unsigned long)(s & 0xFFU);", "return w0 == (unsigned long)(s >> 8) || w1 == (unsigned long)(s & 0xFFU);")]),
    (".ico com um byte alterado (PS2 para 360)", [("    memcpy(saida->ico, co->dados, SAVELARG_TAM_ICO);\n", "    memcpy(saida->ico, co->dados, SAVELARG_TAM_ICO);\n    saida->ico[0] ^= 1;\n")]),
]


def aplica(n, destino):
    shutil.copytree(sc.CONV, destino)
    f = pathlib.Path(destino) / "save_conv.c"
    t = f.read_text()
    for velho, novo in MUTANTES[n][1]:
        if velho not in t:
            raise SystemExit(f"mutante {n}: trecho não encontrado: {velho!r}")
        t = t.replace(velho, novo)
    f.write_text(t)


def main(a):
    if a[:1] == ["--mutante"]:
        n = int(a[1])
        with tempfile.TemporaryDirectory() as d:
            aplica(n, pathlib.Path(d) / "conv")
            erros = verifica(pathlib.Path(d) / "conv", d)
        for x in erros[:5]:
            print("FALHA:", x)
        print(f"mutante {n} ({MUTANTES[n][0]}): " + (f"reprovado ({len(erros)} defeitos)" if erros else "NÃO REPROVADO"))
        return 1 if erros else 0
    with tempfile.TemporaryDirectory() as d:
        erros = verifica(sc.CONV, d)
    for x in erros:
        print("FALHA:", x)
    if erros:
        return 1
    print("conferência: ok")
    vivos = []
    for n, (desc, _) in enumerate(MUTANTES):
        r = subprocess.run([sys.executable, str(pathlib.Path(__file__).resolve()), "--mutante", str(n)], capture_output=True, text=True)
        print(f"  mutante {n} ({desc}): saída {r.returncode}")
        if r.returncode != 1:
            vivos.append(n)
            print(r.stdout, r.stderr)
    if vivos:
        print("FALHA: mutantes que passaram:", vivos)
        return 1
    print(f"ok: conferência e {len(MUTANTES)} mutantes reprovados")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
