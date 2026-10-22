#!/usr/bin/env python3
"""F6, etapa 2: os verificadores do som no WSL (plano/F6/00-escopo.md, seção 2; plano/F6/02.md).

  som.py rastro A B [C ...] [--prefixo]
      rede 2: os rastros de som (BT3_SOM, port/x360/headless/som.c) iguais byte a byte. Com --prefixo, o último pode
      ser o começo dos outros (a PPC parou antes, na guarda), e isso é dito. Vazio, ou sem nenhum início de fluxo e
      nenhum comando de efeito, é falha (um rastro que não viu nada não prova nada).
  som.py mixer A B
      rede 4: duas gravações do dispositivo gravador (BT3_GRAVADOR, port/x360/headless/gravador.c): os mesmos blocos,
      fluxos, taxas, faltas e vozes de efeito ativas por bloco; o PCM dos fluxos ADX igual (pelo hash); a mistura dos
      efeitos igual dentro de 1 LSB; o ganho de cada fluxo igual dentro de 1 ulp do float. Sem nenhum bloco de ADX ou
      nenhum bloco de efeito com som é falha.
  som.py censo DADOS
      os cabeçalhos de todos os ADX da partição 2 (DADOS/pzs3us2) e os bancos da partição 1: tipos, versões, canais,
      taxas, laços (alinhados ao quadro de 32 amostras ou não) e os maiores arquivos.
  som.py amostras --dados DADOS --x86 PROG [--ppc PROG] [--tmp DIR] [--n 300] [--semente 25] [--estraga QUAL]
      rede 3: o PCM dos decodificadores do port (o teste_som da build, headless.py --teste-som) contra a referência
      independente (som_ref.py) byte a byte, nos 20 ADX maiores, em N sorteados com semente fixa (os estéreo, os mono
      mais longos e todos os com laço incluídos) e em todas as amostras de todos os bancos; e a x86 igual à PPC nos
      mesmos. Nas músicas com laço, a janela de 4 quadros de cada lado da volta (uma volta só num laço mais curto que
      ela). As variantes da referência (escala sem +1, coeficiente
      arredondado, laço que continua o histórico ou zera) medidas à parte, para dizer o que a comparação separa.
      A exceção da decisão 25.10: depois da volta do laço o port continua o histórico do filtro do fim (como o port
      de PC), e a referência restaura o estado do começo. A janela depois da volta passa se for igual à referência ou,
      só ela, igual byte a byte à variante "continua" e dentro de LACO_LIMITE LSB da referência; antes da volta, e em
      tudo o que não tem laço, a comparação continua exata.
      --estraga adx|vag|ppc|laco: o controle negativo (o c1 do port mais 1; um byte da parte das amostras trocado; um
      byte da saída da PPC trocado; um byte da saída da x86 trocado logo depois da volta do laço), num subconjunto
      pequeno: tem de sair com 1.
Saída 0 só com tudo certo.
"""
import argparse, array, collections, hashlib, os, pathlib, random, struct, subprocess, sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import som_ref  # noqa: E402

JANELA = 4 * 32  # amostras de cada lado da volta do laço
# LSB: a maior diferença aceita entre o port e a referência depois da volta do laço (decisão 25.10). A medida dos 72
# ADX com laço (todos entram na amostra): 3951 no 65120.bin e no 65143.bin, 860 no 65172.bin, 240 no 65119.bin; até 75
# nos outros (plano/F6/02.md, seção 2)
LACO_LIMITE = 3951


# ------------------------------------------------------------------------------------------------- rede 2: rastro

def resumo_rastro(linhas):
    c = collections.Counter()
    ini = fim = None
    for l in linhas:
        p = l.split()
        if not p:
            continue
        v = p[0].split("-")
        ini = int(v[0]) if ini is None else ini
        fim = int(v[-1])
        ev = p[1] if len(p) > 1 else "?"
        if ev == "se":
            c["se tipo " + p[3]] += 1
        elif ev == "rpc":
            c["rpc " + p[2]] += 1
        elif ev == "start":
            c["start " + p[3]] += 1
        else:
            c[ev] += 1
    return c, ini, fim


def cmd_rastro(a):
    textos = [pathlib.Path(x).read_text(errors="replace").splitlines() for x in a.arquivos]
    falhas = []
    c, ini, fim = resumo_rastro(textos[0])
    print(f"rastro {a.arquivos[0]}: {len(textos[0])} linhas, vblanks {ini}..{fim}; " +
          ", ".join(f"{k} {v}" for k, v in sorted(c.items())))
    if not textos[0] or not (sum(v for k, v in c.items() if k.startswith("start")) or c.get("se tipo 0")):
        falhas.append("rastro sem nenhum início de fluxo e nenhum efeito tocado: não prova nada (BT3_SOM ligado?)")
    for nome, t in zip(a.arquivos[1:], textos[1:]):
        if t == textos[0]:
            print(f"   {nome}: igual ({len(t)} linhas)")
            continue
        k = next((i for i, (x, y) in enumerate(zip(textos[0], t)) if x != y), min(len(t), len(textos[0])))
        ultimo = nome == a.arquivos[-1]
        if a.prefixo and ultimo and len(t) <= len(textos[0]) and (k == len(t) or (k == len(t) - 1 and _mesmo_trecho(t[k], textos[0][k]))):
            print(f"   {nome}: o começo do primeiro ({len(t)} de {len(textos[0])} linhas; parou antes)")
            continue
        falhas.append(f"{nome} diferente na linha {k + 1}: {textos[0][k] if k < len(textos[0]) else '(fim)'!r} contra "
                      f"{t[k] if k < len(t) else '(fim)'!r}")
    for f in falhas:
        print("   FALHA: " + f)
    print("rastro: " + ("tudo certo" if not falhas else f"{len(falhas)} falhas"))
    return 1 if falhas else 0


def _mesmo_trecho(curta, longa):
    """A última linha de um rastro que parou antes pode ser um trecho só de estado cortado ("A-B stat ..." contra
    "A-C stat ..."): o mesmo começo e o mesmo texto."""
    p, q = curta.split(" ", 1), longa.split(" ", 1)
    return len(p) == 2 and len(q) == 2 and p[1] == q[1] and p[0].split("-")[0] == q[0].split("-")[0]


# --------------------------------------------------------------------------------------------- rede 4: gravador

CAB = struct.Struct("<cBHIIHHHHIQ")


def le_gravacao(nome):
    b = pathlib.Path(nome).read_bytes()
    o, regs = 0, []
    while o + 32 <= len(b):
        g, tipo, fluxo, vb, taxa, quadros, falta, vozes, band, ganho, h = CAB.unpack_from(b, o)
        if g != b"G":
            raise ValueError(f"{nome}: registro sem 'G' no byte {o}")
        o += 32
        pcm = None
        if band & 1:
            pcm = array.array("h", b[o:o + quadros * 4])
            if sys.byteorder != "little":
                pcm.byteswap()
            o += quadros * 4
        regs.append((tipo, fluxo, vb, taxa, quadros, falta, vozes, band, ganho, h, pcm))
    return regs


def _float(bits):
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def cmd_mixer(a):
    A, B = le_gravacao(a.a), le_gravacao(a.b)
    falhas = []
    adx = sum(1 for r in A if r[0] == 0)
    som = sum(1 for r in A if r[0] == 1 and r[7] & 1)
    fluxos = len({r[1] for r in A})
    vozes = max((r[6] for r in A), default=0)
    print(f"mixer {a.a}: {len(A)} blocos, {fluxos} fluxos, {adx} blocos de ADX, {som} blocos de efeito com som, "
          f"até {vozes} vozes de efeito ativas, {sum(r[5] for r in A if r[0] == 0)} quadros de ADX em falta")
    if adx == 0 or som == 0:
        falhas.append("sem bloco de ADX ou sem bloco de efeito com som: o gravador não ouviu nada (BT3_SOUND=1?)")
    if len(A) != len(B):
        falhas.append(f"{len(A)} blocos contra {len(B)}")
    maior, um, difs_hash = 0, 0, 0
    for i, (x, y) in enumerate(zip(A, B)):
        if x[:8] != y[:8]:
            falhas.append(f"bloco {i}: cabeçalho {x[:8]} contra {y[:8]} (tipo, fluxo, vblank, taxa, quadros, falta, vozes, dados)")
            if len(falhas) > 8:
                break
            continue
        if x[8] != y[8]:
            fx, fy = _float(x[8]), _float(y[8])
            if abs(x[8] - y[8]) > 1 or (fx < 0) != (fy < 0):
                falhas.append(f"bloco {i} (vblank {x[2]}, fluxo {x[1]}): ganho {fx!r} contra {fy!r}")
        if x[0] == 0:
            if x[9] != y[9]:
                difs_hash += 1
                if difs_hash <= 3:
                    falhas.append(f"bloco {i} (vblank {x[2]}, fluxo {x[1]}): PCM do ADX diferente (hash)")
        elif x[10] is not None:
            d = [abs(p - q) for p, q in zip(x[10], y[10])]
            m = max(d, default=0)
            um += sum(1 for v in d if v == 1)
            if m > maior:
                maior = m
            if m > 1:
                falhas.append(f"bloco {i} (vblank {x[2]}): mistura dos efeitos com diferença de {m} LSB")
        if len(falhas) > 8:
            break
    if difs_hash > 3:
        falhas.append(f"... {difs_hash} blocos de ADX diferentes no total")
    print(f"   {a.b}: maior diferença na mistura dos efeitos {maior} LSB ({um} amostras com 1)")
    for f in falhas:
        print("   FALHA: " + f)
    print("mixer: " + ("tudo certo" if not falhas else f"{len(falhas)} falhas"))
    return 1 if falhas else 0


# --------------------------------------------------------------------------------------------- rede 3: amostras

def adx_cabecalhos(dados):
    d = pathlib.Path(dados) / "pzs3us2"
    for f in sorted(d.iterdir()):
        with open(f, "rb") as fp:
            h = fp.read(0x40)
        if len(h) >= 0x40 and h[:2] == b"\x80\x00":
            try:
                yield f, som_ref.Adx(h + b"\0" * 0), f.stat().st_size
            except ValueError:
                pass


def bancos(dados):
    for f in sorted((pathlib.Path(dados) / "pzs3us1").iterdir()):
        with open(f, "rb") as fp:
            h = fp.read(20)
        if len(h) == 20 and struct.unpack_from("<I", h, 0)[0] == 3:
            b = f.read_bytes()
            if som_ref.banco_partes(b) is not None:
                yield f, b


def cmd_censo(a):
    c = collections.Counter()
    laco, desal_ini, desal_fim, tam = 0, [], [], []
    for f, h, t in adx_cabecalhos(a.dados):
        c[f"v{h.versao} tipo {h.tipo} {h.canais}c {h.taxa} Hz"] += 1
        tam.append((t, f.name))
        if h.laco:
            laco += 1
            if h.laco_ini % 32:
                desal_ini.append(f.name)
            if h.laco_fim % 32:
                desal_fim.append(f.name)
    print(f"ADX: {sum(c.values())} arquivos; " + "; ".join(f"{k}: {v}" for k, v in sorted(c.items())))
    print(f"ADX com laço: {laco}; começo do laço fora do quadro de 32: {len(desal_ini)} {desal_ini[:10]}; "
          f"fim fora do quadro: {len(desal_fim)} {desal_fim[:10]}")
    tam.sort(reverse=True)
    print("os 20 maiores: " + " ".join(f"{n}({t})" for t, n in tam[:20]))
    nb, na, conta = 0, 0, {}
    for f, b in bancos(a.dados):
        nb += 1
        inis = som_ref.banco_amostras(b)
        na += len(inis)
        _, _, (s0, s1) = som_ref.banco_partes(b)
        for ini, _ in inis:
            som_ref.vag_decodifica(b[s0:s1], ini, conta)
    print(f"bancos: {nb}, {na} amostras distintas; blocos fora da tabela do chip: {conta or 'nenhum'}")
    return 0


def escolhe_adx(dados, n, semente):
    todos = list(adx_cabecalhos(dados))
    grandes = sorted(todos, key=lambda x: -x[2])[:20]
    resto = [x for x in todos if x not in grandes]
    estereo = [x for x in resto if x[1].canais == 2]
    mono = sorted((x for x in resto if x[1].canais == 1), key=lambda x: -x[1].amostras)
    longos = mono[:40]
    com_laco = [x for x in mono[40:] if x[1].laco]  # todos os com laço: a exceção da decisão 25.10 vê o pior caso
    rnd = random.Random(semente)
    sorteio = rnd.sample([x for x in mono[40:] if not x[1].laco], max(0, n - len(estereo) - len(longos) - len(com_laco)))
    return [x[0] for x in grandes], [x[0] for x in estereo + longos + com_laco + sorteio]


def roda(prog, qemu, linhas):
    cmd = ([qemu] if qemu else []) + [prog]
    r = subprocess.run(cmd, input="\n".join(linhas) + "\n", capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f"{prog} saiu com {r.returncode}: {r.stdout[-300:]} {r.stderr[-300:]}")
    return r.stdout.splitlines()


def compara_pcm(ref, got):
    """(igual, primeira diferença, maior diferença, comprimentos)"""
    if ref == got:
        return True, None, 0, len(ref)
    a, b = array.array("h", ref), array.array("h", got)
    k = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
    m = max((abs(x - y) for x, y in zip(a, b)), default=0)
    return False, k, m, (len(a), len(b))


def cmd_amostras(a):
    tmp = pathlib.Path(a.tmp)
    for d in ("x86", "ppc"):
        (tmp / d).mkdir(parents=True, exist_ok=True)
    falhas = []
    grandes, sorteados = escolhe_adx(a.dados, a.n, a.semente)
    arquivos = grandes + sorteados
    if a.estraga:  # o controle negativo: poucos arquivos bastam para ver a falha
        arquivos = grandes[:1] + sorteados[:3]
    print(f"ADX: {len(grandes)} maiores e {len(sorteados)} sorteados (semente {a.semente}); "
          f"PPC: {'sim' if a.ppc else 'não'}{'; ESTRAGO DE PROPÓSITO: ' + a.estraga if a.estraga else ''}")
    lacos, janelas_iguais, variantes = 0, 0, collections.Counter()
    excecoes, laco_max = [], (0, "")  # a exceção do laço: os arquivos e a maior diferença (com o arquivo)
    var_dif = collections.Counter()
    for f in arquivos:
        b = f.read_bytes()
        ref = som_ref.Adx(b)
        sai = {d: tmp / d / (f.stem + ".pcm") for d in ("x86", "ppc")}
        est = "1" if a.estraga == "adx" else "0"
        lx = roda(a.x86, None, [f"adx {f} {sai['x86']} {JANELA} {est}"])
        lp = roda(a.ppc, a.qemu, [f"adx {f} {sai['ppc']} {JANELA} 0"]) if a.ppc else None
        if "recusado" in lx[0]:
            falhas.append(f"{f.name}: o port recusou o arquivo")
            continue
        pcm, n = ref.toca(JANELA if ref.laco else 0)
        if sys.byteorder != "little":
            pcm.byteswap()
        rb = pcm.tobytes()
        gx = sai["x86"].read_bytes()
        if a.estraga == "laco" and ref.laco and len(gx) > 2 * 2 * n + 2:
            gx = bytearray(gx)
            gx[2 * 2 * n + 2] ^= 0x40  # a terceira palavra depois da volta: 64 LSB
            gx = bytes(gx)
        # antes da volta do laço (ou o arquivo inteiro, sem laço), exato; depois dela, a exceção abaixo
        corte = 2 * 2 * n if ref.laco else len(rb)
        ok, k, m, ln = compara_pcm(rb[:corte], gx[:corte])
        if not ok or len(gx) != len(rb):
            falhas.append(f"{f.name}: x86 diferente da referência na amostra {(k or 0) // 2} (canal {(k or 0) % 2}), "
                          f"maior diferença {m}, comprimentos {(len(rb) // 2, len(gx) // 2)}; port: {lx[0]}")
        if a.ppc:
            gp = bytearray(sai["ppc"].read_bytes())
            if a.estraga == "ppc" and len(gp) > 100:
                gp[len(gp) // 2] ^= 0x5A
            if gp != gx:
                ok2, k2, m2, ln2 = compara_pcm(gx, bytes(gp))
                falhas.append(f"{f.name}: PPC diferente da x86 na amostra {k2 // 2}, maior diferença {m2}, comprimentos {ln2}")
        if ref.laco:
            lacos += 1
            volta = 2 * 2 * n  # bytes: a volta do laço (antes dela, a janela de 4 quadros já conferida acima)
            if gx[volta:] == rb[volta:]:
                janelas_iguais += 1
            for v in ("continua", "zera"):
                pv, _ = ref.toca(JANELA, laco=v)
                if sys.byteorder != "little":
                    pv.byteswap()
                pv = pv.tobytes()
                if pv[volta:] == gx[volta:]:
                    variantes["laço " + v + " igual ao port"] += 1
                if pv[volta:] != rb[volta:]:
                    var_dif["laço " + v] += 1
                if v == "continua":
                    continua = pv
            if gx[volta:] != rb[volta:]:
                # a exceção da decisão 25.10: depois da volta, igual à variante "continua" e perto da referência
                _, kd, md, _ = compara_pcm(rb[volta:], gx[volta:])
                if gx[volta:] != continua[volta:]:
                    falhas.append(f"{f.name}: depois da volta do laço, x86 diferente da referência e da variante "
                                  f"\"continua\" (decisão 25.10) na amostra {n + (kd or 0) // 2}, maior diferença {md}")
                elif md > LACO_LIMITE:
                    falhas.append(f"{f.name}: depois da volta do laço, {md} LSB da referência, acima do limite "
                                  f"{LACO_LIMITE} da exceção (decisão 25.10)")
                else:
                    excecoes.append(f.name)
                laco_max = max(laco_max, (md, f.name))
        if f in sorteados[:60]:  # as variantes do formato (escala e coeficiente), num subconjunto
            for nome, kw in (("escala sem +1", {"escala_mais_um": False}), ("coeficiente arredondado", {"arredonda": True})):
                pv, _ = ref.toca(JANELA if ref.laco else 0, **kw)
                if sys.byteorder != "little":
                    pv.byteswap()
                if pv.tobytes() != rb:
                    var_dif[nome] += 1
        for d in sai.values():
            d.unlink(missing_ok=True)
    print(f"ADX: {len(arquivos)} arquivos comparados; com laço {lacos}, janela da volta igual à referência em {janelas_iguais}")
    print(f"   laço: a exceção da decisão 25.10 (depois da volta igual à variante \"continua\") em {len(excecoes)} de "
          f"{lacos}; maior diferença da referência {laco_max[0]} LSB ({laco_max[1] or 'nenhum'}), limite {LACO_LIMITE}")
    print("   variantes da referência que dão outro PCM (o que a comparação separa): " +
          (", ".join(f"{k} em {v}" for k, v in sorted(var_dif.items())) or "nenhuma"))
    print("   variantes do laço iguais ao port: " + (", ".join(f"{k} em {v}" for k, v in sorted(variantes.items())) or "nenhuma"))
    # os bancos
    nb = na = 0
    lista = list(bancos(a.dados))
    if a.estraga:
        lista = lista[:3]
    for f, b in lista:
        inis = som_ref.banco_amostras(b)
        _, _, (s0, s1) = som_ref.banco_partes(b)
        parte = b[s0:s1]
        est = -1
        if a.estraga == "vag" and inis:
            est = inis[0][0] + 16 + 4  # um byte de dados do segundo bloco da primeira amostra
        refs = [som_ref.vag_decodifica(parte, i) for i, _ in inis]
        rb = b"".join((r.byteswap() or r).tobytes() if sys.byteorder != "little" else r.tobytes() for r in refs)
        sai = {d: tmp / d / (f.stem + ".vag") for d in ("x86", "ppc")}
        cab = f"{f} {s0} {s1 - s0}"
        lx = roda(a.x86, None, [f"vag {cab} {sai['x86']} {est} " + " ".join(str(i) for i, _ in inis)])
        gx = sai["x86"].read_bytes()
        nb += 1
        na += len(inis)
        if gx != rb:
            cont = [int(x) for x in lx[0].split()[2:]]
            k = next((j for j, (x, y) in enumerate(zip(cont, (len(r) for r in refs))) if x != y), None)
            ok, kk, m, ln = compara_pcm(rb, gx)
            falhas.append(f"{f.name}: amostras do banco diferentes da referência (byte {kk * 2}, maior diferença {m}, "
                          f"comprimentos {ln}{'; contagem da amostra ' + str(k) if k is not None else ''})")
        if a.ppc:
            roda(a.ppc, a.qemu, [f"vag {cab} {sai['ppc']} -1 " + " ".join(str(i) for i, _ in inis)])
            gp = bytearray(sai["ppc"].read_bytes())
            if a.estraga == "ppc" and len(gp) > 100:
                gp[len(gp) // 2] ^= 0x5A
            if bytes(gp) != gx:
                falhas.append(f"{f.name}: amostras do banco na PPC diferentes da x86")
        for d in sai.values():
            d.unlink(missing_ok=True)
    print(f"bancos: {nb} bancos, {na} amostras comparadas")
    if not a.estraga and (nb < 300 or len(arquivos) < 300):
        falhas.append(f"poucos arquivos: {len(arquivos)} ADX e {nb} bancos (os dados estão em {a.dados}?)")
    for f in falhas[:30]:
        print("   FALHA: " + f)
    if len(falhas) > 30:
        print(f"   ... {len(falhas) - 30} falhas a mais")
    print("amostras: " + ("tudo certo" if not falhas else f"{len(falhas)} falhas"))
    return 1 if falhas else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("rastro")
    p.add_argument("arquivos", nargs="+")
    p.add_argument("--prefixo", action="store_true")
    p = sub.add_parser("mixer")
    p.add_argument("a")
    p.add_argument("b")
    p = sub.add_parser("censo")
    p.add_argument("dados")
    p = sub.add_parser("amostras")
    p.add_argument("--dados", required=True)
    p.add_argument("--x86", required=True)
    p.add_argument("--ppc")
    p.add_argument("--qemu", default="qemu-ppc")
    p.add_argument("--tmp", default="/tmp/som_amostras")
    p.add_argument("--n", type=int, default=300)
    p.add_argument("--semente", type=int, default=25)
    p.add_argument("--estraga", choices=["adx", "vag", "ppc", "laco"])
    a = ap.parse_args()
    if a.cmd == "rastro" and len(a.arquivos) < 2:
        sys.exit("rastro: pelo menos dois arquivos")
    sys.exit({"rastro": cmd_rastro, "mixer": cmd_mixer, "censo": cmd_censo, "amostras": cmd_amostras}[a.cmd](a))


if __name__ == "__main__":
    main()
