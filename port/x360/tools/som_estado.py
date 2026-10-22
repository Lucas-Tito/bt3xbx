#!/usr/bin/env python3
"""F6, etapa 1 (docs/x360/plano/F6/01.md): as linhas "som: ..." do BT3_SOM_ESTADO (port/x360/headless/roteiro.c).

    som_estado.py compara LOG_CONSOLE LOG_X86   as linhas de mudança ("muda") das duas rodadas do mesmo roteiro,
                                                sem o " | ms N" do console: iguais, ou a primeira diferença (sai
                                                com 1) e a tela, o vblank e os campos de cada lado
    som_estado.py falas LOG                     cada fala do V0 (do 3 ao 5, ou à troca de arquivo): vblanks
                                                contados, o tempo real (só no log do console) e a razão, e quando a
                                                boca começou e acabou
    som_estado.py parada LOG                    a última linha antes do fim do log e a leitura dela: qual porta da
                                                tela de resultado da História (tela 8) não abriu

O bt3xbx.log do console pode ter outras linhas no meio; só as que começam com "som: " contam.
"""
import re
import sys

CANAIS = ["BGM", "MAP", "SE0", "SE1", "V0", "V1"]


def linhas(caminho):
    with open(caminho, encoding="utf-8", errors="replace") as f:
        return [l.rstrip("\n") for l in f if l.startswith("som: ")]


def sem_ms(l):
    return re.sub(r" \| ms -?\d+$", "", l)


def campos(l):
    """{vblank, leitura, tela, muda, canais {nome: (stat, pausado, left, arquivo, lido)}, boca (espera, ativa, t),
    menus, resultado {...} ou None, ms ou None}"""
    m = re.match(r"som: vblank (\d+) leitura (\d+) tela (-?\d+) (muda|igual) \|(.*)$", l)
    if m is None:
        raise ValueError(l)
    d = {"vblank": int(m.group(1)), "leitura": int(m.group(2)), "tela": int(m.group(3)), "muda": m.group(4) == "muda"}
    resto = m.group(5)
    d["canais"] = {}
    for c in re.finditer(r" (BGM|MAP|SE0|SE1|V0|V1) (\d+)(p?) l(-?\d+) a(-?\d+)(?: lido(-?\d+))?", resto):
        d["canais"][c.group(1)] = (int(c.group(2)), c.group(3) == "p", int(c.group(4)), int(c.group(5)),
                                   int(c.group(6)) if c.group(6) else None)
    b = re.search(r"boca espera (-?\d+) ativa (-?\d+) t (-?\d+)", resto)
    d["boca"] = tuple(int(x) for x in b.groups()) if b else None
    mm = re.search(r"menus (0x[0-9a-f]+|-1)", resto)
    d["menus"] = int(mm.group(1), 0) if mm else None
    r = re.search(r"resultado flags (0x[0-9a-f]+) state (-?\d+) se (-?\d+) premio (-?\d+)/(-?\d+) pad (\d) trig (\d)",
                  resto)
    d["resultado"] = None if r is None else {
        "flags": int(r.group(1), 16), "state": int(r.group(2)), "se": int(r.group(3)), "premio": int(r.group(4)),
        "premios": int(r.group(5)), "pad": int(r.group(6)), "trig": int(r.group(7))}
    t = re.search(r" \| ms (-?\d+)$", resto)
    d["ms"] = int(t.group(1)) if t else None
    return d


def compara(a, b):
    la = [sem_ms(l) for l in linhas(a) if " muda |" in l]
    lb = [sem_ms(l) for l in linhas(b) if " muda |" in l]
    if not la or not lb:
        print(f">> FALHA: sem linhas 'som: ... muda' ({len(la)} em {a}, {len(lb)} em {b})")
        return 1
    for i, (x, y) in enumerate(zip(la, lb)):
        if x != y:
            print(f">> FALHA: a linha {i + 1} das mudanças difere")
            print(f"   console: {x}")
            print(f"   x86:     {y}")
            return 1
    if len(la) != len(lb):
        curta, n = (a, len(la)) if len(la) < len(lb) else (b, len(lb))
        print(f">> FALHA: iguais nas {n} primeiras mudanças, e {curta} acaba antes ({len(la)} contra {len(lb)})")
        print(f"   a seguinte do outro lado: {(lb if len(la) < len(lb) else la)[n]}")
        return 1
    print(f"iguais: {len(la)} mudanças do som, vblank a vblank")
    return 0


def falas(caminho):
    ls = [campos(l) for l in linhas(caminho)]
    aberta = None
    print("arquivo  início  boca(+vb)  boca dura  fim(vb)  contados  vblanks  real ms  ms/vblank")
    for d in ls:
        v0 = d["canais"].get("V0")
        if v0 is None:
            continue
        stat, _, left, arq, _ = v0
        if aberta is not None and (arq != aberta["arq"] or stat != 3):
            fim = d
            vb = fim["vblank"] - aberta["vblank"]
            real = (fim["ms"] - aberta["ms"]) if fim["ms"] is not None and aberta["ms"] is not None else None
            print(f"{aberta['arq']:7} {aberta['vblank']:7} {str(aberta.get('boca_ini', '-')):>9} "
                  f"{str(aberta.get('boca_dur', '-')):>10} {fim['vblank']:8} {aberta['left']:9} {vb:8} "
                  f"{str(real) if real is not None else '-':>8} {(real / vb if real is not None and vb else 0):10.2f}")
            aberta = None
        if aberta is None and stat == 3 and left > 0:
            aberta = {"arq": arq, "vblank": d["vblank"], "ms": d["ms"], "left": left}
        if aberta is not None and d["boca"] is not None:
            espera, ativa, _ = d["boca"]
            if espera == 0 and ativa == 1 and "boca_ini" not in aberta:
                aberta["boca_ini"] = d["vblank"] - aberta["vblank"]
                aberta["boca_vb"] = d["vblank"]
            if ativa == 0 and "boca_vb" in aberta and "boca_dur" not in aberta:
                aberta["boca_dur"] = d["vblank"] - aberta["boca_vb"]
    return 0


PORTAS = [
    (lambda r, se0, m: m is not None and m & 0x100, "MPROG_FREEZE (0x100) ligado: a tela inteira parada; quem o liga "
     "é o achado (não o som)"),
    (lambda r, se0, m: r["se"] == 2 and se0 is not None and se0[0] == 3 and se0[2] == -1,
     "seState 2 espera o SE0 == 5 e o SE0 está em laço (l-1): o jingle não começou (o arquivo é o anterior) ou o "
     "cabeçalho dele foi lido como laço; olhe o 'a' do SE0 (65151 ou 65153 é o jingle) e o Port_FilePath/adx_blanks"),
    (lambda r, se0, m: r["se"] == 2 and se0 is not None and se0[1],
     "seState 2 espera o SE0 == 5 e o SE0 está pausado: o StreamSe_Resume da entrada (fade) não veio"),
    (lambda r, se0, m: r["se"] == 2 and se0 is not None and se0[0] == 0,
     "seState 2 espera o SE0 == 5 e o SE0 está parado (0): o arquivo não abriu no adx_blanks (view_start)"),
    (lambda r, se0, m: r["se"] == 2, "seState 2: o SE0 ainda contando (veja o l)"),
    (lambda r, se0, m: r["se"] == 0 and r["state"] == 0 and not (r["flags"] & 4) and not r["pad"],
     "a animação não chegou ao 'pad true' (MFLASH_PAD): o Flash da tela parou antes"),
    (lambda r, se0, m: r["flags"] & 0x40 and not r["trig"], "fl_out pedido e o trig do fim nunca veio (Flash)"),
    (lambda r, se0, m: r["state"] in (53, 56), "o GetWin não terminou a animação (prêmios)"),
    (lambda r, se0, m: True, "nenhuma porta conhecida fechada: leia os campos"),
]


def parada(caminho):
    ls = linhas(caminho)
    if not ls:
        print(f">> sem linhas 'som:' em {caminho} (BT3_SOM_ESTADO estava no ambiente.txt?)")
        return 1
    d = campos(ls[-1])
    print("última linha: " + ls[-1])
    r = d["resultado"]
    if d["tela"] != 8 or r is None:
        print(f"a última linha é da tela {d['tela']}, não da tela de resultado da História (8)")
        return 0
    se0 = d["canais"].get("SE0")
    for teste, texto in PORTAS:
        if teste(r, se0, d["menus"]):
            print("leitura: " + texto)
            break
    return 0


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "compara":
        return compara(sys.argv[2], sys.argv[3])
    if len(sys.argv) == 3 and sys.argv[1] == "falas":
        return falas(sys.argv[2])
    if len(sys.argv) == 3 and sys.argv[1] == "parada":
        return parada(sys.argv[2])
    sys.exit(__doc__)


if __name__ == "__main__":
    sys.exit(main())
