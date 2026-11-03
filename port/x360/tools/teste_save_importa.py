#!/usr/bin/env python3
"""Teste da importação e da exportação de saves (port/x360/conv/save_importa.c) no Linux, com saves SINTÉTICOS (bytes
aleatórios, soma válida; nenhum dado do jogo) e o sistema de arquivos da libc com falha injetável
(teste_save_importa_fs.c). A conversão esperada é calculada aqui, em Python, como em teste_save_conv.py.

    teste_save_importa.py                 a conferência e depois os mutantes (cada um tem de ser reprovado)
    teste_save_importa.py --mutante N     aplica o mutante N (numa cópia de conv/) e roda a conferência: sai com 1

Regra do projeto: todo verificador é visto falhando de propósito; os mutantes fazem isso a cada execução.
"""
import pathlib, random, re, shutil, subprocess, sys, tempfile

AQUI = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
import save_larguras as sl
import save_conv as sc
import teste_save_conv as tsc

RAIZ = AQUI.parents[2]
FERRAMENTA = AQUI / "teste_save_importa_fs.c"
CONV = RAIZ / "port/x360/conv"
SRC = RAIZ / "port/x360/src"
DADOS, ICONSYS, ICO = tsc.DADOS, tsc.ICONSYS, tsc.ICO
ORDEM = (ICONSYS, ICO, DADOS)
DATA_EXPORTA = "2007-11-13T12:00:00"


def compila(destino, conv, src, mutante=False):
    exe = pathlib.Path(destino) / "fs"
    fontes = [str(pathlib.Path(src) / "save_importa.c")] + [str(pathlib.Path(conv) / n) for n in ("save_conv.c", "psu.c", "save_larguras.c")]
    r = subprocess.run(["gcc", "-std=gnu99", "-Wall", "-Wextra", "-Werror"] + (["-Wno-error"] if mutante else []) + [f"-I{conv}", f"-I{src}", "-o", str(exe), str(FERRAMENTA)] + fontes,
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError("compilação falhou:\n" + r.stderr)
    return exe


def roda(exe, cmd, raiz, *args, falha=None):
    import os
    env = dict(os.environ)
    env.pop("FALHA", None)
    if falha:
        env["FALHA"] = falha
    r = subprocess.run([str(exe), cmd, str(raiz)] + [str(a) for a in args], capture_output=True, text=True, env=env)
    saida = {"_rc": r.returncode, "_err": r.stderr, "_itens": []}
    for l in r.stdout.splitlines():
        k, _, v = l.partition("=")
        if k == "item":
            saida["_itens"].append(v.split("|"))
        else:
            saida[k] = v
    return saida


def psu_de(arqs, **kw):
    return tsc.faz_psu(arqs, **kw)


def save_360_de(arqs, mapa):
    return tsc.esperado_360(arqs, mapa)


def leb(p):
    """os bytes do arquivo, ou None se não existe (um mutante que não grava reprova pela conferência, não por exceção)"""
    p = pathlib.Path(p)
    return p.read_bytes() if p.is_file() else None


def lp(d):
    """os três arquivos de uma pasta; {} se algum falta"""
    try:
        return tsc.le_pasta(d)
    except OSError:
        return {}


def lista_arquivos(pasta):
    p = pathlib.Path(pasta)
    return sorted(str(f.relative_to(p)) for f in p.rglob("*") if f.is_file()) if p.exists() else []


def verifica(conv, src, tmp, mutante=False):
    erros = []
    tmp = pathlib.Path(tmp)
    try:
        exe = compila(tmp, conv, src, mutante)
        tool = sc.compila(tmp, conv)
    except RuntimeError as e:
        if mutante:
            raise
        return [str(e)]
    mapa = sl.carrega()
    rng = random.Random(1113)
    contador = [0]

    def e(cond, msg):
        if not cond:
            erros.append(msg)
        return cond

    def novo():
        contador[0] += 1
        r = tmp / f"c{contador[0]}"
        r.mkdir()
        return r

    def sav(raiz):
        return pathlib.Path(raiz) / "saves/card1/BASLUS-21678DBZT3"

    def poe_save(raiz, arqs360):
        tsc.grava_pasta(sav(raiz), arqs360)

    def le_save(raiz):
        return lp(sav(raiz))

    def poe_psu(raiz, nome, dados):
        d = pathlib.Path(raiz) / "import"
        d.mkdir(exist_ok=True)
        (d / nome).write_bytes(dados)

    def soltos(raiz):
        d = pathlib.Path(raiz) / "import"
        return sorted(f.name for f in d.iterdir() if f.is_file() and f.suffix.lower() == ".psu") if d.exists() else []

    def sem_residuos(raiz, rotulo):
        ruim = [f for f in lista_arquivos(raiz) if f.endswith(".tmp") or f.endswith(".new")]
        e(not ruim, f"{rotulo}: sobraram temporários {ruim}")

    ps2 = [tsc.sintetico(rng) for _ in range(6)]
    psus = [psu_de(a) for a in ps2]
    antigo = tsc.sintetico(rng)
    antigo360 = save_360_de(antigo, mapa)

    # 1. sem import: nada aparece, nada é criado
    r = novo()
    o = roda(exe, "procurar", r)
    e(o["n"] == "0" and o["ha_save"] == "0", f"sem a pasta import: {o}")
    (r / "import").mkdir()
    o = roda(exe, "procurar", r)
    e(o["n"] == "0", f"pasta import vazia: {o}")
    (r / "import/leiame.txt").write_text("x")
    (r / "import/sub").mkdir()
    (r / "import/sub/escondido.psu").write_bytes(psus[0])
    o = roda(exe, "procurar", r)
    e(o["n"] == "0", f".txt e subpasta contaram: {o}")
    e(lista_arquivos(r) == ["import/leiame.txt", "import/sub/escondido.psu"], "a procura mexeu em arquivos")

    # 2. um válido, sem save atual
    r = novo()
    poe_psu(r, "meu_save.psu", psus[0])
    o = roda(exe, "procurar", r)
    e(o["n"] == "1" and o["ha_save"] == "0" and o["_itens"] == [["meu_save.psu", "0", "OK"]], f"procura do válido: {o}")
    o = roda(exe, "importar", r, 0)
    e(o["rc"] == "0" and o["backup"] == "0", f"importar sem save atual: {o} {o['_err']}")
    esp = save_360_de(ps2[0], mapa)
    got = le_save(r)
    for n in esp:
        e(got[n] == esp[n], f"sem save atual: {n} diferente do esperado")
    e(not (r / "backup").exists(), "sem save atual: criou backup")
    e(soltos(r) == [] and leb(r / "import/done/meu_save.psu") == psus[0], "sem save atual: o .psu não foi para done")
    sem_residuos(r, "sem save atual")

    # 3. com save atual: o backup, e o segundo import vai para 002
    r = novo()
    poe_save(r, antigo360)
    poe_psu(r, "a.psu", psus[1])
    o = roda(exe, "procurar", r)
    e(o["ha_save"] == "1", f"ha_save: {o}")
    o = roda(exe, "importar", r, 0)
    e(o["rc"] == "0" and o["backup"] == "1", f"importar com save atual: {o} {o['_err']}")
    b1 = lp(r / "backup/001") if (r / "backup/001").exists() else {}
    e(b1 == antigo360, "backup 001 não tem os três arquivos do save antigo")
    esp = save_360_de(ps2[1], mapa)
    e(le_save(r) == esp, "com save atual: o save novo não é o convertido")
    e(soltos(r) == [] and (r / "import/done/a.psu").exists(), "com save atual: o .psu não foi para done")
    sem_residuos(r, "com save atual")
    poe_psu(r, "b.psu", psus[2])
    o = roda(exe, "importar", r, 0)
    e(o["rc"] == "0" and o["backup"] == "2", f"segundo import: {o}")
    e(lp(r / "backup/002") == esp, "backup 002 não tem o save do primeiro import")
    e(lp(r / "backup/001") == antigo360, "backup 001 foi alterado pelo segundo import")

    # 4. SKIP: válidos para skipped, recusados para failed, o save intacto, sem backup
    r = novo()
    poe_save(r, antigo360)
    poe_psu(r, "a.psu", psus[0])
    poe_psu(r, "b.psu", psus[1])
    poe_psu(r, "lixo.psu", bytes(rng.randrange(256) for _ in range(3000)))
    o = roda(exe, "pular", r)
    e(o["rc"] == "0", f"pular: {o}")
    e(soltos(r) == [], "SKIP deixou .psu solto")
    e(sorted(lista_arquivos(r / "import")) == ["failed/lixo.psu", "skipped/a.psu", "skipped/b.psu"],
      f"SKIP: movimentos errados {lista_arquivos(r / 'import')}")
    e(le_save(r) == antigo360 and not (r / "backup").exists(), "SKIP mexeu no save ou criou backup")
    # depois do SKIP a procura não acha mais nada
    e(roda(exe, "procurar", r)["n"] == "0", "a tela abriria de novo depois do SKIP")

    # 5. inválidos: cada um com o seu motivo; IMPORT recusado não grava nem move
    r = novo()
    poe_save(r, antigo360)
    ruim = dict(ps2[3]); d = bytearray(ruim[DADOS]); d[0x1000] ^= 1; ruim[DADOS] = bytes(d)
    poe_psu(r, "1-soma.psu", psu_de(ruim))
    poe_psu(r, "2-outro.psu", psu_de(ps2[3], pasta="BASLUS-21677DBZT3"))
    poe_psu(r, "3-falta.psu", psu_de(ps2[3], ordem=(ICONSYS, DADOS)))
    poe_psu(r, "4-lixo.psu", bytes(rng.randrange(256) for _ in range(5000)))
    poe_psu(r, "5-grande.psu", bytes(300000))
    poe_psu(r, "6-vazio.psu", b"")
    poe_psu(r, "7-bom.PSU", psus[3])
    o = roda(exe, "procurar", r)
    tx = {i[0]: (int(i[1]), i[2]) for i in o["_itens"]}
    e(o["n"] == "7", f"inválidos: n={o.get('n')}")
    e(tx.get("1-soma.psu", (0, ""))[1] == "PS2 save is damaged (checksum)", f"texto da soma: {tx.get('1-soma.psu')}")
    e(tx.get("2-outro.psu", (0, ""))[1] == "not a BT3 USA save", f"texto do outro jogo: {tx.get('2-outro.psu')}")
    e("missing a file" in tx.get("3-falta.psu", (0, ""))[1], f"texto do arquivo faltando: {tx.get('3-falta.psu')}")
    e(tx.get("4-lixo.psu", (0, ""))[0] != 0, "lixo foi aceito")
    e(tx.get("5-grande.psu", (0, ""))[1] == "file is too big", f"texto do arquivo grande: {tx.get('5-grande.psu')}")
    e(tx.get("6-vazio.psu", (0, ""))[0] != 0, "arquivo vazio foi aceito")
    e(tx.get("7-bom.PSU") == (0, "OK"), f".PSU em maiúsculas não foi achado ou foi recusado: {tx.get('7-bom.PSU')}")
    antes = lista_arquivos(r)
    for i in range(6):
        o = roda(exe, "importar", r, i)
        e(o["rc"] != "0", f"importar o inválido {i} deu certo")
    e(lista_arquivos(r) == antes and le_save(r) == antigo360, "importar inválido mexeu em arquivos")
    o = roda(exe, "importar", r, 99)
    e(o["rc"] != "0", "importar um índice fora da lista deu certo")
    # importar o bom (o último): os recusados vão para failed
    o = roda(exe, "importar", r, 6)
    e(o["rc"] == "0", f"importar o bom entre inválidos: {o}")
    e(sorted(lista_arquivos(r / "import")) == sorted(["done/7-bom.PSU"] + [f"failed/{n}" for n in
      ("1-soma.psu", "2-outro.psu", "3-falta.psu", "4-lixo.psu", "5-grande.psu", "6-vazio.psu")]),
      f"movimentos com vários arquivos: {lista_arquivos(r / 'import')}")

    # 6. vários válidos: o escolhido para done, os outros para skipped
    r = novo()
    for i, n in enumerate(("a.psu", "b.psu", "c.psu")):
        poe_psu(r, n, psus[i])
    o = roda(exe, "importar", r, 1)
    e(o["rc"] == "0", f"vários: {o}")
    e(sorted(lista_arquivos(r / "import")) == ["done/b.psu", "skipped/a.psu", "skipped/c.psu"],
      f"vários: {lista_arquivos(r / 'import')}")
    e(le_save(r) == save_360_de(ps2[1], mapa), "vários: importou o arquivo errado")

    # 7. colisão de nomes: _2, _3, e o que já estava lá não muda
    r = novo()
    for sub in ("done", "skipped"):
        (r / f"import/{sub}").mkdir(parents=True)
        (r / f"import/{sub}/a.psu").write_bytes(b"antigo")
        (r / f"import/{sub}/a_2.psu").write_bytes(b"antigo2")
    poe_psu(r, "a.psu", psus[0])
    poe_psu(r, "b.psu", psus[1])
    o = roda(exe, "importar", r, 0)
    e(o["rc"] == "0", f"colisão: {o}")
    e(leb(r / "import/done/a.psu") == b"antigo" and leb(r / "import/done/a_2.psu") == b"antigo2",
      "colisão: sobrescreveu um arquivo de done")
    e(leb(r / "import/done/a_3.psu") == psus[0], "colisão: o novo não ficou em a_3.psu")
    e(leb(r / "import/skipped/b.psu") == psus[1], "colisão: b.psu não foi para skipped")
    poe_psu(r, "a.psu", psus[2])
    roda(exe, "pular", r)
    e(leb(r / "import/skipped/a_3.psu") == psus[2] and leb(r / "import/skipped/a.psu") == b"antigo",
      "colisão no SKIP")

    # 8. falhas no meio: o save atual continua intacto (ou é recolocado)
    def cenario():
        r = novo()
        poe_save(r, antigo360)
        poe_psu(r, "a.psu", psus[0])
        poe_psu(r, "ruim.psu", b"x" * 40)
        return r

    r = cenario()
    o = roda(exe, "importar", r, 0, falha="gravar:1:-:backup")
    e(o["rc"] == "204" and o["passo"] == "2" and o["sistema"] != "0", f"falha no backup: {o}")
    e(le_save(r) == antigo360 and soltos(r) == ["a.psu", "ruim.psu"], "falha no backup: mexeu no save ou moveu")
    sem_residuos(r, "falha no backup")
    r = cenario()
    o = roda(exe, "importar", r, 0, falha="gravar:1:-:dbzsm.ico.tmp")
    e(o["rc"] == "205" and o["passo"] == "3", f"falha ao gravar o temporário: {o}")
    e(le_save(r) == antigo360, "falha de escrita no meio: o save atual mudou")
    e(soltos(r) == ["a.psu", "ruim.psu"], "falha de escrita: moveu os .psu")
    sem_residuos(r, "falha de escrita")
    e(lp(r / "backup/001") == antigo360, "falha de escrita: o backup não tem o save")
    for alvo, rot in (("icon.sys", "icon.sys"), ("dbzsm.ico", "ico"), ("BASLUS-21678DBZT3/BASLUS-21678DBZT3", "dados")):
        for k in (1, 2):  # 1 = tmp para new, 2 = new para o nome (depois de apagar o antigo)
            r = cenario()
            o = roda(exe, "importar", r, 0, falha=f"mover:{k}:-:{alvo}.new")
            e(o["rc"] == "206" and o["passo"] == "4" and o["restaurado"] == "1", f"falha na troca de {rot} ({k}): {o}")
            e(le_save(r) == antigo360, f"falha na troca de {rot} ({k}): o save antigo não foi recolocado")
            e(soltos(r) == ["a.psu", "ruim.psu"], f"falha na troca de {rot} ({k}): moveu os .psu")
            sem_residuos(r, f"falha na troca de {rot} ({k})")
    r = cenario()  # a falha que não passa: a restauração também falha; o backup tem a cópia
    o = roda(exe, "importar", r, 0, falha="mover:2:*:BASLUS-21678DBZT3/BASLUS-21678DBZT3.new")
    e(o["rc"] == "206" and o["restaurado"] == "0", f"falha permanente na troca: {o}")
    e(lp(r / "backup/001") == antigo360, "falha permanente: o backup não tem o save")
    r = cenario()  # falha ao mover o .psu depois: o save novo já está lá
    o = roda(exe, "importar", r, 0, falha="mover:1:-:done")
    e(o["rc"] == "207" and o["importado"] == "1" and o["passo"] == "5", f"falha ao mover: {o}")
    e(le_save(r) == save_360_de(ps2[0], mapa), "falha ao mover: o save novo não está lá")
    r = novo()  # sem save atual e a troca falha: não sobra nada do save novo
    poe_psu(r, "a.psu", psus[0])
    o = roda(exe, "importar", r, 0, falha="mover:2:-:BASLUS-21678DBZT3/BASLUS-21678DBZT3.new")
    e(o["rc"] == "206" and o["restaurado"] == "1", f"falha sem save atual: {o}")
    e(lista_arquivos(sav(r)) == [], f"falha sem save atual: sobrou {lista_arquivos(sav(r))}")

    # 9. exportação
    caminho_dados = "/jogo/saves/card1/BASLUS-21678DBZT3/BASLUS-21678DBZT3"
    r = novo()
    poe_save(r, antigo360)
    o = roda(exe, "gravado", r, caminho_dados)
    e(o["rc"] == "0" and not (r / "export").exists(), f"sem a pasta export: criou algo ({lista_arquivos(r)})")
    e(roda(exe, "exportar", r)["rc"] == "1", "exportar sem a pasta export não devolveu 1")
    (r / "export").mkdir()
    o = roda(exe, "gravado", r, "/jogo/saves/card1/BASLUS-21678DBZT3/icon.sys")
    e(lista_arquivos(r / "export") == [], "um arquivo que não é o de dados disparou a exportação")
    o = roda(exe, "gravado", r, "/jogo/saves/card1/OUTRO/BASLUS-21678DBZT3")
    e(lista_arquivos(r / "export") == [], "o arquivo de outra pasta disparou a exportação")
    o = roda(exe, "gravado", r, caminho_dados)
    e(o["rc"] == "0", f"exportação: {o}")
    exportado = r / "export/BASLUS-21678DBZT3.psu"
    e(lista_arquivos(r / "export") == ["BASLUS-21678DBZT3.psu"], f"exportação: arquivos {lista_arquivos(r / 'export')}")
    if exportado.exists():
        ref = tmp / "ref.psu"
        t = subprocess.run([str(tool), "360-para-psu", str(sav(r)), str(ref), "--data", DATA_EXPORTA], capture_output=True, text=True)
        e(t.returncode == 0 and leb(exportado) == leb(ref), "o .psu exportado difere do esperado (data 2007-11-13 12:00:00)")
        e(subprocess.run([str(tool), "confere", str(exportado)], capture_output=True, text=True).stdout.strip() == "ps2 ok",
          "o .psu exportado não confere")
        # a ida e a volta: o .psu exportado volta ao save de antes
        volta = tmp / "volta"
        volta.mkdir(exist_ok=True)
        subprocess.run([str(tool), "psu-para-360", str(exportado), str(volta)], check=True)
        e(lp(volta) == antigo360, "importar o .psu exportado não devolve o save")
    # novo save, nova exportação: substitui
    poe_save(r, save_360_de(ps2[2], mapa))
    roda(exe, "gravado", r, caminho_dados)
    e(exportado.exists() and leb(exportado) != leb(ref), "a segunda exportação não substituiu a primeira")
    sem_residuos(r, "exportação")
    # exportação que falha: nada afeta o jogo (rc 0 no gancho), o export anterior fica, sem temporários
    antes = leb(exportado)
    poe_save(r, antigo360)
    for falha in ("gravar:1", "mover:1", "mover:2", "apagar:1", "ler:2"):
        o = roda(exe, "gravado", r, caminho_dados, falha=falha)
        e(o["rc"] == "0", f"exportação com falha {falha} afetou o retorno do gancho: {o}")
        o = roda(exe, "exportar", r, falha=falha)
        e(o["rc"] not in ("0", "1"), f"exportação com falha {falha} não devolveu erro: {o}")
        sem_residuos(r, f"exportação com falha {falha}")
        e(lista_arquivos(r / "export") == ["BASLUS-21678DBZT3.psu"], f"falha {falha}: sobrou algo em export")
    # save estragado (a soma): a exportação recusa e o gancho segue com 0; o export anterior não muda
    ruim360 = dict(antigo360); d = bytearray(ruim360[DADOS]); d[0x2000] ^= 0x10; ruim360[DADOS] = bytes(d)
    poe_save(r, ruim360)
    antes = leb(exportado)
    o = roda(exe, "gravado", r, caminho_dados)
    e(o["rc"] == "0" and leb(exportado) == antes, "save estragado: o gancho falhou ou o export mudou")
    e(roda(exe, "exportar", r)["rc"] not in ("0", "1"), "save estragado: a exportação não recusou")
    # sem o save
    r2 = novo()
    (r2 / "export").mkdir()
    o = roda(exe, "gravado", r2, caminho_dados)
    e(o["rc"] == "0" and lista_arquivos(r2 / "export") == [], "sem save: o gancho falhou ou criou arquivo")

    # 10. o gancho do jogo (plat_mc.c) fica atrás de PORT_X360 e não usa o retorno
    mc = (RAIZ / "port/src/plat_mc.c").read_text()
    m = re.search(r"#ifdef PORT_X360\n\s*Port_X360SaveGravado\(path\);[^\n]*\n#endif", mc)
    e(m is not None, "plat_mc.c: a chamada do gancho não está atrás de #ifdef PORT_X360 sem usar o retorno")
    return erros


MUTANTES = [
    ("sem backup", "save_importa.c", [("        if (tinha) r = faz_backup(fs, pastas, &falha->backup, &sis);\n", "")]),
    ("não move depois de importar", "save_importa.c", [("    mal = move_todos(fs, pastas, lista, indice, falha);\n", "    mal = 0;\n")]),
    ("SKIP não move", "save_importa.c", [("    mal = move_todos(fs, pastas, lista, -1, falha);\n", "    mal = 0;\n")]),
    ("importa arquivo inválido", "save_importa.c", [("    if (lista->item[indice].erro != 0) {", "    if (0) {"),
        ("    r = le_converte(fs, cam);\n    if (r != 0) {\n        falha->codigo = r;", "    r = le_converte(fs, cam);\n    if (0) {\n        falha->codigo = r;")]),
    ("exportação que derruba o save", "save_importa.c", [("    return 0;\n}\n\n/* ---------------------------------------------------------------- textos", "    return r ? -5 : 0;\n}\n\n/* ---------------------------------------------------------------- textos")]),
    ("exportação sem conferir a pasta", "save_importa.c", [("    if (fs->tipo(fs->ctx, pastas->exportar) != 2) return 1; /* sem a pasta: nada */\n", "")]),
    ("colisão sobrescreve (sem sufixo)", "save_importa.c", [("for (n = 2; fs->tipo(fs->ctx, para) != 0; n++) {", "for (n = 2; 0; n++) {")]),
    ("o backup sempre usa 001", "save_importa.c", [("        if (fs->tipo(fs->ctx, pasta) == 0) break;\n", "        break;\n")]),
    ("troca que falha não recoloca o save", "save_importa.c", [("falha->restaurado = restaura(fs, pastas, i);", "falha->restaurado = 0;")]),
    ("falha de escrita deixa os temporários", "save_importa.c", [("if (junta(fs, cam, pastas->save, kNomes[j]) == 0) limpa_temporarios(fs, cam);", ";")]),
    ("recusados vão para skipped", "save_importa.c", [('(lista->item[i].erro ? "failed" : "skipped")', '"skipped"')]),
    (".PSU em maiúsculas não conta", "save_importa.c", [('return n > 4 && igual_sem_caixa(nome + n - 4, ".psu") == 0;', 'return n > 4 && strcmp(nome + n - 4, ".psu") == 0;')]),
    ("data da exportação errada", "save_importa.h", [("#define SI_DATA_EXPORTA_ANO 2007", "#define SI_DATA_EXPORTA_ANO 2008")]),
    ("qualquer arquivo gravado dispara a exportação", "save_importa.c", [("if (!gravado || !eh_arquivo_de_dados(gravado)) return 0;", "if (!gravado) return 0;")]),
    ("a troca grava direto no nome (sem temporário)", "save_importa.c", [("    r = fs->mover(fs->ctx, tmp, nova);\n    if (r != 0) return r;\n    if (fs->tipo(fs->ctx, cam) != 0) {\n        r = fs->apagar(fs->ctx, cam);\n        if (r != 0) return r;\n    }\n    return fs->mover(fs->ctx, nova, cam);",
        "    (void)nova;\n    if (fs->tipo(fs->ctx, cam) != 0) {\n        r = fs->apagar(fs->ctx, cam);\n        if (r != 0) return r;\n    }\n    return fs->mover(fs->ctx, tmp, cam);")]),
]


def aplica(n, destino):
    """copia conv/ e os dois arquivos de src/ em destino/conv e destino/src; o mutante vale numa cópia"""
    destino = pathlib.Path(destino)
    shutil.copytree(CONV, destino / "conv")
    (destino / "src").mkdir()
    for nome in ("save_importa.c", "save_importa.h"):
        shutil.copy(SRC / nome, destino / "src" / nome)
    _, arq, trocas = MUTANTES[n]
    f = destino / "src" / arq
    t = f.read_text()
    for velho, novo in trocas:
        if velho not in t:
            raise SystemExit(f"mutante {n}: trecho não encontrado: {velho!r}")
        t = t.replace(velho, novo)
    f.write_text(t)


def main(a):
    if a[:1] == ["--mutante"]:
        n = int(a[1])
        with tempfile.TemporaryDirectory() as d:
            aplica(n, d)
            try:
                erros = verifica(pathlib.Path(d) / "conv", pathlib.Path(d) / "src", d, True)
            except Exception as ex:  # defeito do teste, não do mutante: não vale como reprovação
                print(f"ERRO DO TESTE no mutante {n}: {ex!r}")
                return 3
        for x in erros[:5]:
            print("FALHA:", x)
        print(f"mutante {n} ({MUTANTES[n][0]}): " + (f"reprovado ({len(erros)} defeitos)" if erros else "NÃO REPROVADO"))
        return 1 if erros else 0
    with tempfile.TemporaryDirectory() as d:
        erros = verifica(CONV, SRC, d)
    for x in erros:
        print("FALHA:", x)
    if erros:
        return 1
    print("conferência: ok")
    vivos = []
    for n, (desc, _, _) in enumerate(MUTANTES):
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
