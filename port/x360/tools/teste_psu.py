#!/usr/bin/env python3
"""Teste da biblioteca port/x360/conv/psu.c contra o mymcplus (referência).

Uso (com o Python que tem o mymcplus instalado, fora do repositório):
    python3 -m venv /caminho/venv && /caminho/venv/bin/pip install mymcplus
    /caminho/venv/bin/python port/x360/tools/teste_psu.py [--fonte OUTRO_psu.c]

--fonte compila outra cópia da biblioteca (usado para ver o teste falhar com cópias
estragadas; ver port/x360/tools/teste_psu_mutantes.py). Sai com 0 se tudo passa, 1 se
algo falha. Nenhum dado do jogo: os saves são sintéticos (bytes pseudoaleatórios com os
nomes e tamanhos do save do BT3 e de outros casos).
"""
import argparse, os, random, shutil, struct, subprocess, sys, tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
RAIZ = os.path.abspath(os.path.join(AQUI, "..", "..", ".."))
COD = {"ARGS": 1, "NAO_PSU": 2, "TRUNCADO": 3, "ENTRADA": 4, "NOME": 5, "SOBRA": 6, "MUITOS": 7,
       "ESPACO": 8, "BT3_PASTA": 9, "BT3_FALTA": 10, "BT3_TAMANHO": 11, "BT3_EXTRA": 12}
BT3 = "BASLUS-21678DBZT3"
BT3_ARQS = [("icon.sys", 964), ("dbzsm.ico", 40090), (BT3, 16384)]

falhas = []
def confere(cond, msg):
    if not cond:
        falhas.append(msg)
        print("FALHA:", msg)
    return cond

def rodar(*a, **k):
    return subprocess.run(list(a), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **k)

def mymc(cartao, *args):
    r = rodar(sys.executable, "-m", "mymcplus", "-i", cartao, *args)
    if r.returncode != 0:
        sys.exit("mymcplus falhou: %s\n%s" % (args, r.stdout))
    return r.stdout

def tod_str(t):  # t = (seg, min, hora, dia, mes, ano) do mymcplus
    return "%04d-%02d-%02dT%02d:%02d:%02d" % (t[5], t[4], t[3], t[2], t[1], t[0])

def parse_ref(dados):
    """Lê um .psu com o código do mymcplus (ps2mc_dir) e devolve (pasta, [arquivos])."""
    from mymcplus import ps2mc_dir as d
    e = [d.unpack_dirent(dados[i * 512:(i + 1) * 512]) for i in range(3)]
    pasta = e[0]
    n = pasta[2] - 2
    pos, arqs = 1536, []
    for _ in range(n):
        ent = d.unpack_dirent(dados[pos:pos + 512])
        pos += 512
        arqs.append((ent, dados[pos:pos + ent[2]]))
        pos += (ent[2] + 1023) // 1024 * 1024
    assert pos == len(dados)
    return pasta, arqs

def exporta_ref(tmp, nome, arquivos):
    """Cria um cartão, grava o save pelo mymcplus e devolve os bytes do .psu exportado."""
    cartao = os.path.join(tmp, nome + ".ps2")
    mymc(cartao, "format")
    mymc(cartao, "mkdir", nome_pasta(arquivos))
    for n, b in arquivos:
        orig = os.path.join(tmp, "orig")
        os.mkdir(orig)
        p = os.path.join(orig, n)
        open(p, "wb").write(b)
        mymc(cartao, "add", "-d", nome_pasta(arquivos), p)
        shutil.rmtree(orig)
    saida = os.path.join(tmp, nome + ".psu")
    mymc(cartao, "export", "-o", saida, nome_pasta(arquivos))
    return open(saida, "rb").read()

PASTA_ATUAL = {}
def nome_pasta(arquivos):
    return PASTA_ATUAL[id(arquivos)]

def sintetico(rng, pasta, arqs):
    lst = [(n, bytes(rng.getrandbits(8) for _ in range(t))) for n, t in arqs]
    PASTA_ATUAL[id(lst)] = pasta
    return lst

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fonte", default=os.path.join(RAIZ, "port/x360/conv/psu.c"))
    args = ap.parse_args()
    tmp = tempfile.mkdtemp(prefix="teste_psu_")
    exe = os.path.join(tmp, "teste_psu")
    cc = rodar("gcc", "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-I", os.path.dirname(args.fonte),
               "-I", os.path.join(RAIZ, "port/x360/conv"), os.path.join(AQUI, "teste_psu.c"), args.fonte, "-o", exe)
    if cc.returncode:
        print(cc.stdout); sys.exit("compilação falhou")
    rng = random.Random(360)
    casos = {
        "bt3": (BT3, BT3_ARQS),
        "vazio": ("PASTA-VAZIA", []),
        "pequenos": ("OUTRO-JOGO", [("a", 1), ("b.bin", 1023), ("c.bin", 1024), ("d.bin", 1025), ("zero", 0)]),
        "nome31": ("X" * 31, [("N" * 31, 2048)]),
        "ultimo_1000": ("ULTIMO", [("p", 5000), ("q", 1000)]),
    }
    refs = {}
    for nome, (pasta, arqs) in casos.items():
        lst = sintetico(rng, pasta, arqs)
        refs[nome] = (lst, exporta_ref(tmp, nome, lst))

    # 1. leitura: mesmos nomes, tamanhos, bytes, modos e datas que o mymcplus
    for nome, (lst, psu) in refs.items():
        p = os.path.join(tmp, nome + ".psu")
        out = os.path.join(tmp, "out_" + nome)
        os.mkdir(out)
        r = rodar(exe, "ler", p, out)
        confere(r.returncode == 0, "%s: leitura falhou: %s" % (nome, r.stdout.strip()))
        if r.returncode:
            continue
        pasta, arqs = parse_ref(psu)
        linhas = r.stdout.splitlines()
        esperado = ["pasta %s modo=%d c=%s m=%s n=%d" % (pasta[8].decode(), pasta[0], tod_str(pasta[3]),
                                                           tod_str(pasta[6]), len(arqs))]
        for ent, dados in arqs:
            esperado.append("arq %s modo=%d tam=%d c=%s m=%s" % (ent[8].decode(), ent[0], ent[2], tod_str(ent[3]), tod_str(ent[6])))
        obtido = linhas
        confere(obtido == esperado, "%s: leitura difere do mymcplus\n  obtido:   %s\n  esperado: %s" % (nome, obtido, esperado))
        for (n, b) in lst:
            confere(open(os.path.join(out, n), "rb").read() == b, "%s: bytes de %s diferem" % (nome, n))
        r = rodar(exe, "bt3", p)
        esp = 0 if nome == "bt3" else COD["BT3_PASTA"]
        confere(r.returncode == (0 if esp == 0 else 2) and r.stdout.startswith("codigo %d " % esp),
                "%s: conferência do BT3 devia dar %d: %s" % (nome, esp, r.stdout.strip()))

    # 2. escrita: mesmos dados e datas -> .psu que o mymcplus importa de volta, e comparação byte a byte
    # Campos que o cartão preenche e a nossa escrita zera (não fazem parte do conteúdo do save):
    #   0x02-0x03 (reservado), 0x10-0x13 (primeiro cluster FAT), 0x14-0x17 (entrada pai), 0x20-0x23 (atributo).
    def mascara(entrada):
        b = bytearray(entrada)
        for a, z in ((2, 4), (16, 24), (32, 36)):
            b[a:z] = b"\0" * (z - a)
        return bytes(b)
    for nome, (lst, psu) in refs.items():
        pasta, arqs = parse_ref(psu)
        l = os.path.join(tmp, nome + ".lista")
        with open(l, "w") as f:
            f.write("pasta %s %d %s %s\n" % (pasta[8].decode(), pasta[0], tod_str(pasta[3]), tod_str(pasta[6])))
            for i, (ent, dados) in enumerate(arqs):
                cam = os.path.join(tmp, "d_%s_%d" % (nome, i))
                open(cam, "wb").write(dados)
                f.write("arq %s %d %s %s %s\n" % (ent[8].decode(), ent[0], tod_str(ent[3]), tod_str(ent[6]), cam))
        nosso = os.path.join(tmp, nome + "_nosso.psu")
        r = rodar(exe, "escrever", l, nosso)
        confere(r.returncode == 0, "%s: escrita falhou: %s" % (nome, r.stdout.strip()))
        if r.returncode:
            continue
        b = open(nosso, "rb").read()
        confere(len(b) == len(psu), "%s: tamanho %d, mymcplus %d" % (nome, len(b), len(psu)))
        # byte a byte, exceto os campos do cartão
        igual = len(b) == len(psu)
        if igual:
            pos = 0
            for i in range(3):
                igual &= mascara(b[i * 512:(i + 1) * 512]) == mascara(psu[i * 512:(i + 1) * 512])
            pos = 1536
            for ent, dados in arqs:
                igual &= mascara(b[pos:pos + 512]) == mascara(psu[pos:pos + 512])
                pos += 512
                m = (ent[2] + 1023) // 1024 * 1024
                igual &= b[pos:pos + m] == psu[pos:pos + m]
                pos += m
        confere(igual, "%s: escrita difere do .psu do mymcplus (fora dos campos do cartão)" % nome)
        # importação pelo mymcplus: mesmos bytes e datas
        cartao = os.path.join(tmp, nome + "_imp.ps2")
        mymc(cartao, "format")
        mymc(cartao, "import", nosso)
        reexp = os.path.join(tmp, nome + "_reexp.psu")
        mymc(cartao, "export", "-o", reexp, pasta[8].decode())
        p2, a2 = parse_ref(open(reexp, "rb").read())
        confere(p2[8] == pasta[8] and p2[2] == pasta[2] and p2[3] == pasta[3] and p2[6] == pasta[6],
                "%s: pasta reimportada difere" % nome)
        confere(len(a2) == len(arqs), "%s: número de arquivos reimportados" % nome)
        for (e1, d1), (e2, d2) in zip(arqs, a2):
            confere(d1 == d2 and e1[8] == e2[8] and e1[0] == e2[0] and e1[3] == e2[3] and e1[6] == e2[6],
                    "%s: arquivo %s reimportado difere" % (nome, e1[8]))

    # 3. datas escolhidas (fixas) passam pela escrita, pelo mymcplus e voltam
    lst, psu = refs["bt3"]
    pasta, arqs = parse_ref(psu)
    l = os.path.join(tmp, "datas.lista")
    with open(l, "w") as f:
        f.write("pasta %s 33831 2005-12-31T23:59:58 2008-02-29T01:02:03\n" % BT3)
        for i, (ent, dados) in enumerate(arqs):
            cam = os.path.join(tmp, "d_bt3_%d" % i)
            f.write("arq %s 33943 2001-01-02T03:04:05 2030-12-31T23:59:59 %s\n" % (ent[8].decode(), cam))
    rodar(exe, "escrever", l, os.path.join(tmp, "datas.psu"))
    cartao = os.path.join(tmp, "datas.ps2")
    mymc(cartao, "format"); mymc(cartao, "import", os.path.join(tmp, "datas.psu"))
    mymc(cartao, "export", "-o", os.path.join(tmp, "datas2.psu"), BT3)
    p2, a2 = parse_ref(open(os.path.join(tmp, "datas2.psu"), "rb").read())
    confere(tod_str(p2[3]) == "2005-12-31T23:59:58" and tod_str(p2[6]) == "2008-02-29T01:02:03", "datas fixas da pasta")
    confere(all(tod_str(e[3]) == "2001-01-02T03:04:05" and tod_str(e[6]) == "2030-12-31T23:59:59" for e, _ in a2), "datas fixas dos arquivos")

    # 4. casos de erro
    bom = refs["bt3"][1]
    pequenos = refs["pequenos"][1]
    def estraga(nome, dados, cod, comando="ler"):
        p = os.path.join(tmp, "erro_" + nome + ".psu")
        open(p, "wb").write(dados)
        r = rodar(exe, comando, p, os.path.join(tmp, "out_" + "bt3")) if comando == "ler" else rodar(exe, comando, p)
        if comando == "ler":
            ok = r.returncode == 2 and r.stdout.startswith("erro %d " % COD[cod])
        else:
            ok = r.returncode == 2 and r.stdout.startswith("codigo %d " % COD[cod])
        confere(ok, "erro %s: esperado %s, veio: %r (saída %d)" % (nome, cod, r.stdout.strip(), r.returncode))
    def poe32(b, off, v):
        b = bytearray(b); b[off:off + 4] = struct.pack("<I", v); return bytes(b)
    def poe16(b, off, v):
        b = bytearray(b); b[off:off + 2] = struct.pack("<H", v); return bytes(b)
    estraga("vazio", b"", "NAO_PSU")
    estraga("lixo", bytes(rng.getrandbits(8) for _ in range(5000)), "NAO_PSU")
    estraga("zeros", bytes(5000), "NAO_PSU")
    estraga("so_cabecalho", bom[:1000], "NAO_PSU")
    estraga("truncado_dados", bom[:len(bom) - 1], "TRUNCADO")
    estraga("truncado_meio", bom[:20000], "TRUNCADO")
    estraga("truncado_entrada", bom[:1536 + 300], "TRUNCADO")
    estraga("sem_completar_ultimo", refs["ultimo_1000"][1][:-24], "TRUNCADO")
    estraga("tamanho_passa_do_fim", poe32(bom, 1536 + 4, 10 * 1024 * 1024), "TRUNCADO")
    estraga("tamanho_4gb", poe32(bom, 1536 + 4, 0xFFFFFFFF), "TRUNCADO")
    estraga("pasta_maior", poe32(bom, 4, 6), "TRUNCADO")
    estraga("pasta_menor", poe32(bom, 4, 4), "SOBRA")
    estraga("sobra_zeros", bom + bytes(1024), "SOBRA")
    estraga("sem_ponto", bom[:512] + bom[1024:], "NAO_PSU")
    estraga("modo_arquivo_na_pasta", poe16(bom, 0, 0x8497), "NAO_PSU")
    estraga("subpasta", poe16(bom, 1536, 0x8427), "ENTRADA")
    estraga("modo_apagado", poe16(bom, 1536, 0x0497), "ENTRADA")
    estraga("nome_sem_zero", bom[:1536 + 0x40] + b"A" * 32 + bom[1536 + 0x40 + 32:], "NOME")
    estraga("nome_vazio", bom[:1536 + 0x40] + bytes(32) + bom[1536 + 0x40 + 32:], "NOME")
    estraga("nome_pasta_sem_zero", bom[:0x40] + b"A" * 32 + bom[0x40 + 32:], "NOME")
    # BT3: a leitura passa, a conferência acusa
    estraga("bt3_outro_jogo", refs["pequenos"][1], "BT3_PASTA", "bt3")
    estraga("bt3_pasta_outra_regiao", bom[:0x40] + b"BESLES-12345DBZT3".ljust(32, b"\0") + bom[0x40 + 32:], "BT3_PASTA", "bt3")
    # arquivo faltando: reescrever com dois arquivos
    def reescreve(nome, arqs_novos):
        l = os.path.join(tmp, nome + ".lista")
        with open(l, "w") as f:
            f.write("pasta %s 33831 2005-01-01T00:00:00 2005-01-01T00:00:00\n" % BT3)
            for i, (n, b) in enumerate(arqs_novos):
                cam = os.path.join(tmp, "n_%s_%d" % (nome, i))
                open(cam, "wb").write(b)
                f.write("arq %s 33943 2005-01-01T00:00:00 2005-01-01T00:00:00 %s\n" % (n, cam))
        p = os.path.join(tmp, nome + ".psu")
        r = rodar(exe, "escrever", l, p)
        assert r.returncode == 0, r.stdout
        return open(p, "rb").read()
    dados = dict(refs["bt3"][0])
    estraga("bt3_falta_um", reescreve("falta", [(n, dados[n]) for n in ("icon.sys", "dbzsm.ico")]), "BT3_FALTA", "bt3")
    estraga("bt3_nome_trocado", reescreve("trocado", [("icon.sys", dados["icon.sys"]), ("dbzsm.ico", dados["dbzsm.ico"]), ("OUTRO", dados[BT3])]), "BT3_FALTA", "bt3")
    estraga("bt3_tamanho_errado", reescreve("tam", [("icon.sys", dados["icon.sys"]), ("dbzsm.ico", dados["dbzsm.ico"][:-1]), (BT3, dados[BT3])]), "BT3_TAMANHO", "bt3")
    estraga("bt3_tamanho_maior", reescreve("tam2", [("icon.sys", dados["icon.sys"]), ("dbzsm.ico", dados["dbzsm.ico"]), (BT3, dados[BT3] + b"\0")]), "BT3_TAMANHO", "bt3")
    estraga("bt3_arquivo_extra", reescreve("extra", [(n, dados[n]) for n in dados] + [("extra", b"x")]), "BT3_EXTRA", "bt3")
    estraga("bt3_vazio", reescreve("vazio3", []), "BT3_FALTA", "bt3")

    shutil.rmtree(tmp)
    if falhas:
        print("%d falha(s)" % len(falhas))
        sys.exit(1)
    print("teste_psu: tudo certo")

main()
