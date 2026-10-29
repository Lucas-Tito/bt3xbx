#!/usr/bin/env python3
"""F7, etapa 2 (docs/x360/plano/F7/02.md): receitas de cartões avançados nativos e o verificador de save por marco.

As receitas (port/x360/receitas/NOME.txt) e a tabela de campos identificados (port/x360/receitas/campos.txt) são as
mesmas que port/x360/headless/receitas.c aplica no jogo; o formato está no começo daquele arquivo. Não lê nada do jogo:
só as receitas, os cartões gerados (dado de teste, fora do git) e os logs das rodadas.

    f7_receitas.py valida [--pasta DIR]
        a tabela de campos (dentro do save, alinhada, sem sobreposição) e cada receita: campos identificados, índices e
        valores na faixa, todo elemento escrito com um "exige" do New Game, e ao menos um "depois" que a receita não
        escreveu (o que o jogo tem de produzir)
    f7_receitas.py confere --receita NOME --build x86|ppc --cartao PASTA --evento LOG --reabre LOG
                           [--antes PASTA --reabre-antes LOG] [--json SAIDA]
        o cartão depois do marco: carrega na build (save_entre.preve), cada "depois" e cada escrita da receita na ordem
        da build; o log do evento com a receita aplicada (assinatura do texto versionado), a conferência em memória e
        a gravação pelo McFlow depois dela; o log da reabertura pela carga normal (McFlow 19 e 20, sem New Game) com a
        conferência em memória e sem receita aplicada. Com --antes: o cartão de antes do marco tem a receita e NÃO
        cumpre o marco. Sai com 1 em qualquer falha
    f7_receitas.py negativos (os mesmos argumentos de confere) --saida DIR
        estraga cópias, uma coisa por vez, e exige que confere recuse cada uma
"""
import argparse, contextlib, hashlib, io, json, pathlib, re, shutil, struct, sys, tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import save_entre  # noqa: E402

RAIZ = pathlib.Path(__file__).resolve().parents[3]
PASTA = RAIZ / "port/x360/receitas"
TAM = save_entre.TAM
TIPOS = {"u8": 1, "u16": 2, "s32": 4, "b32": 4, "b64": 8}
ESCRITAS = ("escreve", "ou", "liga", "item")


class Falha(Exception):
    pass


def exigir(cond, msg):
    if not cond:
        raise Falha(msg)


def fnv(b):
    h = 0xcbf29ce484222325
    for x in b:
        h = ((h ^ x) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def palavras(linha):
    return linha.split("#", 1)[0].split()


def numero(p):
    try:
        return int(p, 0)
    except (ValueError, TypeError):
        raise Falha(f"número inválido: {p}")


def ler_campos(pasta):
    campos = {}
    for k, l in enumerate((pasta / "campos.txt").read_text().splitlines(), 1):
        w = palavras(l)
        if not w:
            continue
        exigir(len(w) >= 6 and w[1] in TIPOS, f"campos.txt:{k}: nome tipo desl qtd passo fonte")
        nome, tipo, desl, qtd, passo = w[0], w[1], numero(w[2]), numero(w[3]), numero(w[4])
        t = TIPOS[tipo]
        exigir(nome not in campos, f"campos.txt:{k}: {nome} repetido")
        exigir(qtd >= 1 and desl >= 0 and desl % t == 0, f"campos.txt:{k}: {nome}: alinhamento ou quantidade")
        if tipo in ("b32", "b64"):
            fim = desl + -(-qtd // (t * 8)) * t
            faixas = [(desl, fim)]
        else:
            exigir(qtd == 1 or passo >= t, f"campos.txt:{k}: {nome}: passo menor que o tipo")
            faixas = [(desl + i * passo, desl + i * passo + t) for i in range(qtd)]
        exigir(faixas[-1][1] <= TAM, f"campos.txt:{k}: {nome}: fora do save")
        campos[nome] = dict(tipo=tipo, desl=desl, qtd=qtd, passo=passo, faixas=faixas, fonte=" ".join(w[5:]))
    # nenhum byte em dois campos (nem a soma, 0..7)
    dono = {}
    for nome, c in campos.items():
        for a, b in c["faixas"]:
            exigir(a >= 8, f"campos.txt: {nome} cobre a soma do cartão (bytes 0..7)")
            for x in range(a, b):
                exigir(x not in dono, f"campos.txt: {nome} e {dono.get(x)} no mesmo byte {x:#x}")
                dono[x] = nome
    return campos


def alvo(texto, campos):
    m = re.fullmatch(r"([a-z0-9_.]+)(?:\[([^\]]+)\])?", texto)
    exigir(m and m.group(1) in campos, f"campo não identificado: {texto}")
    c = campos[m.group(1)]
    if m.group(2) is None:
        exigir(c["qtd"] == 1, f"{texto}: campo com {c['qtd']} elementos pede o índice")
        i0 = i1 = 0
    elif ".." in m.group(2):
        a, b = m.group(2).split("..", 1)
        i0, i1 = numero(a), numero(b)
    else:
        i0 = i1 = numero(m.group(2))
    exigir(0 <= i0 <= i1 < c["qtd"], f"{texto}: índice fora de 0..{c['qtd'] - 1}")
    return m.group(1), i0, i1


def cabe(c, v):
    return {"u8": 0 <= v <= 0xFF, "u16": 0 <= v <= 0xFFFF, "s32": -2**31 <= v <= 0xFFFFFFFF}.get(c["tipo"], v in (0, 1))


def ler_receita(nome, pasta=PASTA, campos=None):
    """A receita como o receitas.c a lê: {nome, fonte (FNV do texto), ops: [(op, campo, i0, i1, valor, linha)]}."""
    campos = campos or ler_campos(pasta)
    arq = pasta / f"{nome}.txt"
    texto = arq.read_bytes()
    ops, marco, arquivos = [], False, []
    for k, l in enumerate(texto.decode().split("\n"), 1):
        w = palavras(l)
        if not w:
            continue
        onde = f"{arq.name}:{k}"
        if w[0] == "marco":
            exigir(len(w) == 2 and w[1] == nome, f"{onde}: marco NOME (o nome do arquivo)")
            marco = True
        elif w[0] == "evento":
            exigir(len(w) >= 2, f"{onde}: evento TEXTO")
        elif w[0] == "arquivo":
            exigir(len(w) == 3 and "/" not in w[1] and numero(w[2]) > 0, f"{onde}: arquivo NOME TAMANHO")
            arquivos.append((w[1], numero(w[2])))
        elif w[0] == "chama":
            exigir(len(w) == 3 and w[1] in ("Save_AddItem", "Save_AddMoney"), f"{onde}: chama Save_AddItem|Save_AddMoney N")
            v = numero(w[2])
            if w[1] == "Save_AddItem":
                exigir("item" in campos and 0 <= v < campos["item"]["qtd"], f"{onde}: item fora da tabela")
                ops.append(("item", "item", v, v, v, k))
            else:
                exigir("dinheiro" in campos and v > 0, f"{onde}: Save_AddMoney com valor positivo")
                ops.append(("dinheiro", "dinheiro", 0, 0, v, k))
        elif w[0] == "liga":
            exigir(len(w) == 2, f"{onde}: liga CAMPO[I..J]")
            c, i0, i1 = alvo(w[1], campos)
            exigir(campos[c]["tipo"] in ("b32", "b64"), f"{onde}: liga só em campo de bits")
            ops.append(("liga", c, i0, i1, 1, k))
        elif w[0] in ("exige", "escreve", "ou", "depois"):
            exigir(len(w) == 4, f"{onde}: {w[0]} CAMPO[I..J] OP V")
            c, i0, i1 = alvo(w[1], campos)
            v = numero(w[3])
            op = w[0]
            if op == "depois":
                op = {"=": "igual", "tem": "tem", "sem": "sem", ">=": "maior", "<=": "menor"}.get(w[2])
                exigir(op, f"{onde}: depois CAMPO = | tem | sem | >= | <= V")
            else:
                exigir(w[2] == "=", f"{onde}: {w[0]} CAMPO = V")
            if op in ("ou", "tem", "sem"):
                exigir(campos[c]["tipo"] not in ("b32", "b64") and cabe(campos[c], v), f"{onde}: máscara fora do tipo")
            elif op not in ("maior", "menor"):
                exigir(cabe(campos[c], v), f"{onde}: valor não cabe no tipo")
            ops.append((op, c, i0, i1, v, k))
        else:
            raise Falha(f"{onde}: palavra desconhecida {w[0]}")
    exigir(marco, f"{arq.name}: sem a linha marco")
    exigir(any(o[0] in ("igual", "tem", "sem", "maior", "menor") for o in ops) or arquivos, f"{arq.name}: nenhum depois")
    return dict(nome=nome, fonte=fnv(texto), ops=ops, arquivos=arquivos, campos=campos)


def predicado(op, x, v, tipo="s32"):
    u, m = x & 0xFFFFFFFF, v & 0xFFFFFFFF
    return {"escreve": x == v or (tipo == "s32" and u == m), "liga": x == v,
            "igual": x == v or (tipo == "s32" and u == m),
            "ou": u & m == m, "tem": u & m == m, "item": x & 1 == 1, "sem": u & m == 0,
            "maior": x >= v, "menor": x <= v}[op]


def valida_receita(r):
    """Regras do contrato, além da forma: cada elemento escrito tem um "exige" (o estado do New Game, que o jogo confere
    antes de escrever), e ao menos um "depois" é falso no estado que a receita deixa (calculado dos "exige" e das
    escritas): sobra algo para o jogo produzir. O cartão de antes, gerado, tem de mostrar o mesmo (confere --antes)."""
    pre = {(c, i): v for op, c, i0, i1, v, k in r["ops"] if op == "exige" for i in range(i0, i1 + 1)}
    for op, c, i0, i1, v, k in r["ops"]:
        if op in ESCRITAS or op == "dinheiro":
            for i in range(i0, i1 + 1):
                exigir((c, i) in pre, f"{r['nome']}:{k}: {c}[{i}] escrito sem um exige do New Game antes")
                x = pre[(c, i)]
                pre[(c, i)] = {"escreve": v, "liga": 1, "ou": x | v, "item": x | 3, "dinheiro": x + v}[op]
    falsos = 0
    for op, c, i0, i1, v, k in r["ops"]:
        if op in ("igual", "tem", "sem", "maior", "menor"):
            for i in range(i0, i1 + 1):
                if (c, i) in pre and not predicado(op, pre[(c, i)], v, r["campos"][c]["tipo"]):
                    falsos += 1
    exigir(falsos or r["arquivos"], f"{r['nome']}: nenhum depois é falso no estado da receita: nada para o jogo produzir")


def valida(pasta):
    campos = ler_campos(pasta)
    nomes = sorted(p.stem for p in pasta.glob("*.txt") if p.name != "campos.txt")
    exigir(nomes, "nenhuma receita")
    for n in nomes:
        r = ler_receita(n, pasta, campos)
        valida_receita(r)
        esc = sum(i1 - i0 + 1 for op, c, i0, i1, v, k in r["ops"] if op in ESCRITAS or op == "dinheiro")
        dep = sum(i1 - i0 + 1 for op, c, i0, i1, v, k in r["ops"] if op in ("igual", "tem", "sem", "maior", "menor"))
        print(f"{n}: fonte {r['fonte']:016x}; {esc} elementos escritos, {dep} conferidos depois"
              + (f", {len(r['arquivos'])} arquivo(s)" if r["arquivos"] else ""))
    print(f"campos: {len(campos)} identificados; receitas: {len(nomes)}; tudo certo")


# ---------------------------------------------------------------- o cartão

def valor(b, c, i, ordem):
    t = c["tipo"]
    if t in ("b32", "b64"):
        w = TIPOS[t]
        bits = w * 8
        pal = int.from_bytes(b[c["desl"] + (i // bits) * w:][:w], "little" if ordem == "<" else "big")
        return (pal >> (i % bits)) & 1
    pos = c["desl"] + i * c["passo"]
    return struct.unpack_from(ordem + {"u8": "B", "u16": "H", "s32": "i"}[t], b, pos)[0]


def confere_bloco(r, b, ordem, so_antes):
    """O que o receitas.c confere, sobre os bytes do cartão. Devolve quantos elementos."""
    explicitos = {(c, i) for op, c, i0, i1, v, k in r["ops"] if op in ("igual", "tem", "sem", "maior", "menor")
                  for i in range(i0, i1 + 1)}
    n = 0
    for op, c, i0, i1, v, k in r["ops"]:
        if op in ("exige", "dinheiro") or (so_antes and op not in ESCRITAS):
            continue
        cc = r["campos"][c]
        for i in range(i0, i1 + 1):
            if op in ESCRITAS and not so_antes and (c, i) in explicitos:
                continue
            x = valor(b, cc, i, ordem)
            exigir(predicado(op, x, v, cc["tipo"]),
                   f"{r['nome']}:{k}: {c}[{i}] = {x} ({x & 0xFFFFFFFF:#x}), fora do esperado ({op} {v:#x})")
            n += 1
    return n


def arquivo_dados(pasta):
    pasta = pathlib.Path(pasta)
    achados = [p for p in pasta.rglob(save_entre.DADOS) if p.is_file() and p.parent.name == save_entre.DADOS]
    exigir(len(achados) == 1, f"{pasta}: {len(achados)} arquivos de dados do save")
    return achados[0]


def confere_cartao(r, pasta, build, so_antes):
    arq = arquivo_dados(pasta)
    prev = save_entre.preve(arq, build)
    exigir(prev[0] == "carrega", f"{arq}: na {build}, {prev[0]} ({prev[1]})")
    b = arq.read_bytes()
    exigir(len(b) == TAM, f"{arq}: {len(b)} bytes")
    n = confere_bloco(r, b, "<" if build == "x86" else ">", so_antes)
    if not so_antes:
        for nome, tam in r["arquivos"]:
            achados = [p for p in pathlib.Path(pasta).rglob(nome) if p.is_file()]
            exigir(achados and all(p.stat().st_size == tam for p in achados), f"{pasta}: falta {nome} de {tam} bytes")
            n += 1
    return n


# ---------------------------------------------------------------- os logs

def ler_log(caminho):
    texto = pathlib.Path(caminho).read_text(errors="replace")
    cod = pathlib.Path(str(caminho) + ".codigo")
    codigo = int(cod.read_text().strip()) if cod.exists() else None
    return texto, codigo


def confere_log_evento(r, caminho, antes=False):
    texto, codigo = ler_log(caminho)
    nome = pathlib.Path(caminho).name
    exigir(codigo == 0, f"{nome}: saiu com {codigo}")
    exigir(re.search(r"^roteiro: \S+ fim ", texto, re.M), f"{nome}: o roteiro não chegou ao fim")
    exigir(not re.search(r"FALHA|abort|Segmentation|queda do port|não implementado", texto), f"{nome}: falha ou queda")
    ap = re.findall(rf"^receita: {re.escape(r['nome'])} aplicada fonte ([0-9a-f]{{16}}) .*semente (\S+)$", texto, re.M)
    exigir(len(ap) == 1, f"{nome}: {len(ap)} linhas de receita aplicada (esperada uma)")
    exigir(int(ap[0][0], 16) == r["fonte"], f"{nome}: fonte {ap[0][0]}, a receita versionada é {r['fonte']:016x}")
    resto = texto.split(f"receita: {r['nome']} aplicada ", 1)[1]
    quando = "antes" if antes else "depois"
    exigir(re.search(rf"^receita: {re.escape(r['nome'])} confere {quando} ok \d+ elementos fonte {r['fonte']:016x} ",
                     resto, re.M), f"{nome}: sem a conferência {quando} em memória depois de aplicar")
    # a gravação pelo McFlow depois da receita: os estados 15 e 16 (gravando, boot_novo.txt) e a volta ao 0; a build sem
    # janela da x86 não escreve as linhas "mc: gravou" do caminho atômico
    exigir(re.search(r"^mcflow 15 .*\n(?:.*\n)*?mcflow 16 .*\n(?:.*\n)*?mcflow 0 ", resto, re.M),
           f"{nome}: o McFlow não gravou o save depois da receita (estados 15, 16 e 0)")
    origem = re.search(r"^passos: origem (\S+ diff:\S+)", texto, re.M)
    return dict(semente=ap[0][1], origem=origem.group(1) if origem else None)


def confere_log_reabre(r, caminho, antes=False):
    texto, codigo = ler_log(caminho)
    nome = pathlib.Path(caminho).name
    with contextlib.redirect_stdout(io.StringIO()) as juiz:
        r_julga = save_entre.julga(caminho, "carrega", codigo if codigo is not None else 999)
    exigir(r_julga == 0, f"{nome}: a carga normal não cumpriu 19 -> 20 sem New Game ({juiz.getvalue().strip()})")
    exigir(" aplicada fonte " not in texto, f"{nome}: receita aplicada na reabertura")
    quando = "antes" if antes else "depois"
    exigir(re.search(rf"^receita: {re.escape(r['nome'])} confere {quando} ok \d+ elementos fonte {r['fonte']:016x} ",
                     texto, re.M), f"{nome}: sem a conferência {quando} em memória depois da carga")
    exigir(not re.search(r"^(?:mc: gravou |mcflow 1[56] )", texto, re.M), f"{nome}: a reabertura gravou no cartão")


def confere(a, r=None):
    r = r or ler_receita(a.receita)
    valida_receita(r)
    n = confere_cartao(r, a.cartao, a.build, False)
    ev = confere_log_evento(r, a.evento)
    confere_log_reabre(r, a.reabre)
    res = dict(receita=r["nome"], fonte=f"{r['fonte']:016x}", build=a.build, elementos=n, **ev,
               cartao={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                       for p in sorted(pathlib.Path(a.cartao).rglob("*")) if p.is_file()})
    if a.antes:
        na = confere_cartao(r, a.antes, a.build, True)
        confere_log_evento(r, pathlib.Path(str(a.antes) + ".txt"), antes=True)
        confere_log_reabre(r, a.reabre_antes, antes=True)
        try:
            confere_cartao(r, a.antes, a.build, False)
        except Falha:
            pass
        else:
            raise Falha("o cartão de antes do marco já cumpre o marco: a receita não deixou nada para o jogo")
        res["antes"] = dict(elementos=na, cartao=hashlib.sha256(arquivo_dados(a.antes).read_bytes()).hexdigest())
    return res


# ---------------------------------------------------------------- negativos

def refaz_soma(b, ordem):
    s = save_entre.soma(bytes(b))
    struct.pack_into(ordem + "II", b, 0, s >> 8, s & 0xFF)


def negativos(a, saida):
    r = ler_receita(a.receita)
    confere(a, r)  # o intacto passa
    saida = pathlib.Path(saida)
    exigir(not saida.exists(), f"{saida} já existe")
    saida.mkdir(parents=True)
    ordem = "<" if a.build == "x86" else ">"
    outra = "ppc" if a.build == "x86" else "x86"
    explicitos = [o for o in r["ops"] if o[0] in ("igual", "tem", "sem", "maior", "menor")]
    escrita = next((o for o in r["ops"] if o[0] in ("escreve", "ou", "liga")), None)

    def mexe_campo(op_alvo, quebra):
        def f(d):
            arq = arquivo_dados(d.cartao)
            b = bytearray(arq.read_bytes())
            op, c, i0, i1, v, k = op_alvo
            cc = r["campos"][c]
            if cc["tipo"] in ("b32", "b64"):
                w = TIPOS[cc["tipo"]]
                pos = cc["desl"] + (i0 // (w * 8)) * w
                pal = int.from_bytes(b[pos:pos + w], "little" if ordem == "<" else "big") ^ (1 << (i0 % (w * 8)))
                b[pos:pos + w] = pal.to_bytes(w, "little" if ordem == "<" else "big")
            else:
                fmt = ordem + {"u8": "B", "u16": "H", "s32": "I"}[cc["tipo"]]
                pos = cc["desl"] + i0 * cc["passo"]
                x = struct.unpack_from(fmt, b, pos)[0]
                struct.pack_into(fmt, b, pos, quebra(op, x, v) & {"u8": 0xFF, "u16": 0xFFFF, "s32": 0xFFFFFFFF}[cc["tipo"]])
            refaz_soma(b, ordem)
            arq.write_bytes(bytes(b))
        return f

    def quebra(op, x, v):
        return {"igual": v + 1, "escreve": v + 1, "maior": v - 1, "menor": v + 1, "tem": x & ~v, "ou": x & ~v,
                "sem": x | v}[op]

    def troca_log(campo, velho, novo):
        def f(d):
            p = pathlib.Path(getattr(d, campo))
            t = p.read_text(errors="replace")
            exigir(velho in t, f"negativo sem alvo: {velho!r} em {p.name}")
            p.write_text(t.replace(velho, novo))
        return f

    def trunca(d):
        arq = arquivo_dados(d.cartao)
        arq.write_bytes(arq.read_bytes()[:TAM // 2])

    def soma_errada(d):
        # um byte que a soma do jogo recusa (as DUAS palavras diferem; mexer num byte só às vezes deixa uma conferir,
        # e o jogo aceita: regra do PS2, save_entre.preve)
        arq = arquivo_dados(d.cartao)
        orig = arq.read_bytes()
        for pos in range(0x3FFE, 0x3F00, -1):
            for bit in (0xFF, 0x80, 0x01):
                b = bytearray(orig)
                b[pos] ^= bit
                arq.write_bytes(bytes(b))
                if save_entre.preve(arq, d.build)[0] == "danificado":
                    return
        raise Falha("nenhum byte de soma errada encontrado")

    casos = [
        ("truncado", trunca),
        ("soma-errada", soma_errada),
        ("outra-build", lambda d: setattr(d, "build", outra)),
        ("sem-aplicada", troca_log("evento", f"receita: {r['nome']} aplicada ", "receita: apagada ")),
        ("fonte-errada", troca_log("evento", f"aplicada fonte {r['fonte']:016x}", f"aplicada fonte {r['fonte'] ^ 1:016x}")),
        ("evento-sem-gravar", troca_log("evento", "\nmcflow 16 ", "\nmcflow 61 ")),
        ("reabre-sem-confere", troca_log("reabre", f"receita: {r['nome']} confere depois ok", "receita: x")),
        ("reabre-new-game", troca_log("reabre", "mcflow 20 ", "mcflow 10 ")),
    ]
    # cada "depois" estragado no seu primeiro elemento, com a soma refeita na ordem da build (o jogo carregaria)
    casos += [(f"depois-{o[1]}-{o[2]}-l{o[5]}", mexe_campo(o, quebra)) for o in explicitos]
    if escrita is not None and not any((o[1], o[2]) == (escrita[1], escrita[2]) for o in r["ops"]
                                       if o[0] in ("igual", "tem", "sem", "maior", "menor")):
        casos.append((f"escrita-{escrita[1]}", mexe_campo(escrita, quebra)))
    if a.antes:
        casos.append(("antes-como-depois", lambda d: (shutil.rmtree(d.cartao), shutil.copytree(d.antes, d.cartao))))
    for nome, estrago in casos:
        with tempfile.TemporaryDirectory(dir=saida) as td:
            td = pathlib.Path(td)
            d = argparse.Namespace(**vars(a))
            d.cartao = td / "cartao"
            shutil.copytree(a.cartao, d.cartao)
            for campo in ("evento", "reabre"):
                p = td / f"{campo}.txt"
                shutil.copy2(getattr(a, campo), p)
                c = pathlib.Path(str(getattr(a, campo)) + ".codigo")
                if c.exists():
                    shutil.copy2(c, str(p) + ".codigo")
                setattr(d, campo, p)
            if a.antes:
                d.antes = td / "antes"
                shutil.copytree(a.antes, d.antes)
                shutil.copy2(str(a.antes) + ".txt", str(d.antes) + ".txt")
                shutil.copy2(str(a.antes) + ".txt.codigo", str(d.antes) + ".txt.codigo")
            estrago(d)
            try:
                confere(d, r)
            except (Falha, OSError, StopIteration, IndexError, struct.error) as e:
                (saida / f"{nome}.txt").write_text(f"{e}\n")
                print(f"negativo {nome}: recusado ({e})")
            else:
                raise Falha(f"negativo {nome} ACEITO")
    print(f"negativos: {len(casos)} recusados")


def fora_do_repo(p):
    p = pathlib.Path(p).resolve()
    exigir(RAIZ not in p.parents and p != RAIZ, f"{p}: saída dentro do repositório recusada")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    v = sub.add_parser("valida")
    v.add_argument("--pasta", type=pathlib.Path, default=PASTA)
    for nome in ("confere", "negativos"):
        c = sub.add_parser(nome)
        c.add_argument("--receita", required=True)
        c.add_argument("--build", choices=["x86", "ppc"], required=True)
        c.add_argument("--cartao", type=pathlib.Path, required=True)
        c.add_argument("--evento", type=pathlib.Path, required=True)
        c.add_argument("--reabre", type=pathlib.Path, required=True)
        c.add_argument("--antes", type=pathlib.Path)
        c.add_argument("--reabre-antes", type=pathlib.Path)
        c.add_argument("--json", type=pathlib.Path)
        if nome == "negativos":
            c.add_argument("--saida", type=pathlib.Path, required=True)
    a = ap.parse_args()
    try:
        if a.cmd == "valida":
            valida(a.pasta)
        elif a.cmd == "confere":
            exigir(not a.antes or a.reabre_antes, "--antes pede --reabre-antes")
            res = confere(a)
            if a.json:
                fora_do_repo(a.json)
                a.json.write_text(json.dumps(res, ensure_ascii=False, indent=2) + "\n")
            print(f"{a.receita} na {a.build}: {res['elementos']} elementos conferidos no cartão; tudo certo")
        else:
            fora_do_repo(a.saida)
            negativos(a, a.saida)
        return 0
    except (Falha, OSError, StopIteration, IndexError, struct.error) as e:
        print(f"FALHA: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
