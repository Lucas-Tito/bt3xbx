#!/usr/bin/env python3
"""F4, etapa 8: o save entre builds (x86 little-endian; PPC do qemu e 360 big-endian). As peças do save_entre.sh e do
save_console.sh; não lê nada do jogo, só os arquivos do cartão (dado do usuário, que fica no WSL e em build/, nunca no git).

    save_entre.py analisa ARQ...              tamanho, soma, as duas palavras da soma nas duas ordens, a ordem de quem
                                              gravou e o que cada build faz com o arquivo
    save_entre.py preve ARQ --build x86|ppc [--sem-regra]
                                              "carrega SOM" ou "danificado", com o porquê: o McCard_Checksum do jogo
                                              (src/battle/stgm_a_b.c) com as palavras lidas na ordem da build; na ppc, também
                                              a regra do cabeçalho do port (PORT_BIG_ENDIAN: palavra acima de 0xFF é de outra
                                              ordem); --sem-regra prevê a build de antes dela (d3fd6a6)
    save_entre.py estraga ORIG DEST CASO      um cartão estragado (a pasta inteira copiada, só o arquivo de dados muda):
                                              truncado (metade), zerado (16384 zeros), byte (o byte 0x2000 com os bits
                                              invertidos), bit (o bit 0 do byte 0x2001), soma0 (save de little-endian cuja
                                              soma tem o byte baixo 0: o último byte, que o jogo não lê, acertado para isso
                                              e as duas palavras regravadas como a x86 as grava; 2 em 256 saves de PC são
                                              assim, e a soma do jogo sozinha os aceita no big-endian)
    save_entre.py julga SAIDA --espera carrega|danificado --codigo N
                                              o rastro de uma rodada com um roteiro save_*.txt: o boot leu o save (McFlow 19)
                                              e foi a 20 (carregado) ou a 29 (dados danificados), como previsto; no carrega,
                                              o New Game (10) nunca abriu; no danificado, o 20 nunca veio e o New Game abriu;
                                              o roteiro chegou ao fim, sem FALHA, sem queda (o código de saída 0)
    save_entre.py campos X86 PPC              o save da x86 e o da PPC (a raiz do cartão ou a pasta com os três arquivos) campo a campo pela
                                              largura declarada (SaveData, include/sys/save.h; McCardIconSys,
                                              include/battle/stgm_a_b.h): os mesmos valores, cada um na ordem da sua build.
                                              Com --mesma-ordem, dois saves da mesma ordem (PPC e 360): bytes iguais, ou os
                                              campos que diferem
Sai com 1 em qualquer divergência.
"""
import pathlib, re, struct, sys

TAM = 0x4000
DADOS = "BASLUS-21678DBZT3"


def soma(b):
    """McCard_Checksum: a soma dos bytes depois dos 8 primeiros, os pares uma vez e os ímpares vezes 256, em 16 bits"""
    s = 0
    for i, x in enumerate(b[8:TAM]):
        s = (s + (x << 8 if i & 1 else x)) & 0xFFFF
    return s


def palavras(b, ordem):
    return struct.unpack(("<" if ordem == "le" else ">") + "II", b[:8])


def som(b, ordem):
    return struct.unpack_from(("<" if ordem == "le" else ">") + "i", b, 0x16A4)[0]


def preve(arq, build, regra=True):
    """(resultado, porquê): resultado 'carrega' ou 'danificado'. O caminho do boot (McFlow_UpdateBootLoad): os três
    arquivos abertos (McCard_CheckFiles; sem um deles, o diretório existe e é dado danificado), 16384 bytes lidos
    (senão MCCARD_F_DIFF), e a soma (McCard_Checksum: ruim só se as DUAS palavras diferem)"""
    arq = pathlib.Path(arq)
    for irmao in ("icon.sys", "dbzsm.ico"):
        if not (arq.parent / irmao).exists():
            return "danificado", f"falta {irmao} (McCard_CheckFiles)"
    b = arq.read_bytes()
    if len(b) < TAM:
        return "danificado", f"{len(b)} de {TAM} bytes (MCCARD_F_DIFF)"
    ordem = "le" if build == "x86" else "be"
    w0, w1 = palavras(b, ordem)
    s = soma(b)
    if build == "ppc" and regra and (w0 > 0xFF or w1 > 0xFF):
        return "danificado", f"palavras da soma {w0:#x} {w1:#x}: acima de 0xFF, de outra ordem (regra do port)"
    if w0 != s >> 8 and w1 != s & 0xFF:
        return "danificado", f"soma {s:#06x}, palavras {w0:#x} {w1:#x}: as duas diferem"
    if w0 == s >> 8 and w1 == s & 0xFF:
        porque = f"soma {s:#06x} confere"
    else:
        porque = f"soma {s:#06x}, palavras {w0:#x} {w1:#x}: só uma confere, e o jogo aceita (regra do PS2)"
    return "carrega", f"{porque}; som {som(b, ordem)}"


def ordem_de(b):
    """quem gravou: as duas palavras da soma são o byte alto e o baixo dela, abaixo de 0x100 na ordem de quem gravou"""
    le, be = palavras(b, "le"), palavras(b, "be")
    s = soma(b)
    cands = [o for o, (w0, w1) in (("little-endian (x86, PS2)", le), ("big-endian (PPC, 360)", be))
             if w0 == s >> 8 and w1 == s & 0xFF]
    if len(cands) == 2:
        return "qualquer (soma 0)"
    return cands[0] if cands else "nenhuma: a soma não confere"


def analisa(arqs):
    for a in arqs:
        b = pathlib.Path(a).read_bytes()
        print(f"{a}: {len(b)} bytes")
        if len(b) < 8:
            continue
        s = soma(b)
        print(f"  soma {s:#06x} (alto {s >> 8:#04x}, baixo {s & 0xFF:#04x}); cabeçalho {b[:8].hex()}")
        print(f"  palavras: lidas em little-endian {palavras(b, 'le')[0]:#x} {palavras(b, 'le')[1]:#x}, "
              f"em big-endian {palavras(b, 'be')[0]:#x} {palavras(b, 'be')[1]:#x}")
        print(f"  gravado por: {ordem_de(b)}")
        for build in ("x86", "ppc"):
            r, p = preve(a, build)
            print(f"  na {build}: {r} ({p})")
        r, p = preve(a, "ppc", regra=False)
        print(f"  na ppc sem a regra do cabeçalho: {r} ({p})")


def estraga(orig, dest, caso):
    import shutil
    orig, dest = pathlib.Path(orig), pathlib.Path(dest)
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(orig, dest)
    arq = next(dest.rglob(f"card1/{DADOS}/{DADOS}"))
    b = bytearray(arq.read_bytes())
    if caso == "truncado":
        b = b[:TAM // 2]
    elif caso == "zerado":
        b = bytearray(TAM)
    elif caso == "byte":
        b[0x2000] ^= 0xFF
    elif caso == "bit":
        b[0x2001] ^= 0x01
    elif caso == "soma0":
        if ordem_de(bytes(b)) != "little-endian (x86, PS2)":
            sys.exit("soma0 pede um save da x86")
        # o último byte (0x3FFF, ímpar a partir do 8: conta vezes 256) mexe só no byte alto; o penúltimo (0x3FFE, par)
        # no baixo: acerta o baixo para 0
        s = soma(bytes(b))
        b[0x3FFE] = (b[0x3FFE] - (s & 0xFF)) & 0xFF
        s = soma(bytes(b))
        assert s & 0xFF == 0, hex(s)
        struct.pack_into("<II", b, 0, s >> 8, s & 0xFF)
    else:
        sys.exit(f"caso desconhecido: {caso}")
    arq.write_bytes(bytes(b))
    print(f"{caso}: {arq} com {len(b)} bytes, soma {soma(bytes(b)):#06x}")


def julga(saida, espera, codigo):
    linhas = pathlib.Path(saida).read_text(errors="replace").splitlines()
    rastro = [l for l in linhas if l.startswith(("tela ", "mcflow ", "roteiro: "))]
    mc = [int(m.group(1)) for l in rastro for m in [re.match(r"mcflow (-?\d+) ", l)] if m]
    erros = []
    if codigo != 0:
        erros.append(f"o programa saiu com {codigo}")
    for l in linhas:
        if re.search(r"FALHA|abort|Segmentation|queda do port|sem conversor em|não implementado", l):
            erros.append(f"linha: {l.strip()[:160]}")
            break
    fim = [l for l in rastro if re.match(r"roteiro: \S+ fim ", l)]
    if not fim:
        erros.append("o roteiro não chegou ao fim")
    depois = None
    if 19 not in mc:
        erros.append("o boot não leu o save (sem o McFlow 19)")
    else:
        i = mc.index(19)
        depois = mc[i + 1] if i + 1 < len(mc) else None
        quer = 20 if espera == "carrega" else 29
        if depois != quer:
            erros.append(f"depois da leitura (19) veio o McFlow {depois}, esperado {quer}")
    if espera == "carrega":
        if 10 in mc:
            erros.append("o New Game abriu (McFlow 10): o título não ofereceu o Continue")
    else:
        if 20 in mc:
            erros.append("o save foi carregado (McFlow 20)")
        if 10 not in mc[mc.index(29) + 1:] if 29 in mc else True:
            erros.append("o New Game não abriu depois da mensagem de danificado")
    leu = [l for l in linhas if l.startswith(f"mc: leu {DADOS}/{DADOS} ")]
    gravou = [l for l in linhas if l.startswith("mc: gravou ")]
    sons = re.findall(r"ok som (\d+)", "\n".join(rastro))
    resumo = (f"19 -> {depois}; som {'/'.join(sons) or '-'}; " + (f"{fim[0].split(' fim ')[1]}" if fim else "sem fim")
              + (f"; {leu[0][len('mc: leu '):]}" if leu else "") + (f"; {len(gravou)} gravações" if gravou else ""))
    if erros:
        print(f"FALHA ({espera}): " + "; ".join(erros) + f"  [{resumo}]")
        return 1
    print(f"ok ({espera}): {resumo}")
    return 0


# Larguras declaradas. SaveData (include/sys/save.h): o resto é s32; "o" = vetor de bytes de conteúdo desconhecido
# (opaco: julgado palavra a palavra pela largura que o explica); "8" = u8 conhecido.
def campos_save():
    c = []
    def s32(a, b):
        c.extend((o, 4) for o in range(a, b, 4))
    def opaco(a, b):
        c.append((a, -(b - a)))
    s32(0, 0xA0); opaco(0xA0, 0x208); s32(0x208, 0x210); opaco(0x210, 0x77C); s32(0x77C, 0x780); opaco(0x780, 0xA08)
    s32(0xA08, 0xA14); opaco(0xA14, 0xC10)
    c.extend((o, 8) for o in range(0xC10, 0xC30, 8))
    s32(0xC30, 0xC4C); opaco(0xC4C, 0x1008); s32(0x1008, 0x1010); opaco(0x1010, 0x1208); s32(0x1208, 0x120C)
    opaco(0x120C, 0x1608); s32(0x1608, 0x16B0); opaco(0x16B0, 0x1808)
    for k in range(97):  # SaveCustom: u16 item[3][8], s32 unk30, u16 level, u16 unk36
        o = 0x1808 + k * 0x38
        c.extend((o + j, 2) for j in range(0, 0x30, 2))
        c.append((o + 0x30, 4)); c.append((o + 0x34, 2)); c.append((o + 0x36, 2))
    for k in range(14):  # SaveRec: u8 unk0[0x14], u16 level, u16 unk16, s32 chara
        o = 0x2D40 + k * 0x1C
        opaco(o, o + 0x14); c.append((o + 0x14, 2)); c.append((o + 0x16, 2)); c.append((o + 0x18, 4))
    c.extend((o, 1) for o in range(0x2EC8, 0x3026))  # item[]
    opaco(0x3026, 0x3028); c.append((0x3028, 4)); opaco(0x302C, TAM)
    return c


def campos_iconsys():
    c = [(o, 1) for o in range(4)] + [(4, 2), (6, 2), (8, 4), (0xC, 4)]
    c.extend((o, 4) for o in range(0x10, 0xC0, 4))  # bgColor s32, lightDir/lightColor/ambient f32 (pelos bits)
    c.extend((o, 1) for o in range(0xC0, 0x3C4))    # textos e unk1C4
    return c


NOMES = {0x0: "soma (alto)", 0x4: "soma (baixo)", 0x16A4: "soundMode", 0x16A8: "bgmVolume", 0x16AC: "seVolume",
         0xC10: "charaBits[0]", 0xC18: "charaBits[1]", 0xC20: "charaBits[2]", 0xC28: "stageBits", 0x3028: "money"}


def compara_campos(a, b, campos, ordem_b, rotulo):
    """a: little-endian (x86); b: na ordem_b. Campos tipados: o mesmo valor. Opacos, palavra a palavra: iguais, ou
    iguais como s32 ou 2 x u16 invertidos (o jogo grava ali números de uma largura que a decomp não deu nome).
    Devolve (contagens, divergências)."""
    cont, div = {}, []
    def conta(k):
        cont[k] = cont.get(k, 0) + 1
    for o, w in campos:
        if w > 0:
            x, y = a[o:o + w], b[o:o + w]
            va = int.from_bytes(x, "little")
            vb = int.from_bytes(y, "little" if ordem_b == "le" else "big")
            if va == vb:
                conta("igual" if x == y else f"igual invertido ({w * 8} bits)")
            elif o in (0, 4) and rotulo == DADOS:
                conta("soma (sobre os bytes, que mudam com a ordem)")
            else:
                div.append(f"{rotulo} {o:#06x} ({NOMES.get(o, f'{w * 8} bits')}): {va:#x} contra {vb:#x}")
        else:
            for p in range(o, o - w, 4):
                n = min(4, o - w - p)
                x, y = a[p:p + n], b[p:p + n]
                if x == y:
                    conta("opaco igual")
                elif ordem_b == "le":
                    div.append(f"{rotulo} {p:#06x} (opaco): {x.hex()} contra {y.hex()}")
                elif n == 4 and x == y[::-1]:
                    conta("opaco igual como s32 invertido")
                elif n >= 2 and all(x[i:i + 2] == y[i:i + 2][::-1] for i in range(0, n, 2)):
                    conta("opaco igual como u16 invertidos")
                else:
                    div.append(f"{rotulo} {p:#06x} (opaco): {x.hex()} contra {y.hex()}, sem largura que explique")
    return cont, div


def campos(da, db, mesma):
    def pasta(d):  # a pasta com os três arquivos (a do save_console.sh), ou a raiz de um cartão (card1/...)
        d = pathlib.Path(d)
        return d if (d / DADOS).is_file() else next(d.rglob(f"card1/{DADOS}"))
    pa, pb = pasta(da), pasta(db)
    erros = 0
    for nome, cs in ((DADOS, campos_save()), ("icon.sys", campos_iconsys()), ("dbzsm.ico", None)):
        a, b = (pa / nome).read_bytes(), (pb / nome).read_bytes()
        if len(a) != len(b):
            print(f"{nome}: tamanhos {len(a)} e {len(b)}"); erros += 1; continue
        ndif = sum(1 for x, y in zip(a, b) if x != y)
        if mesma or cs is None:
            # mesma ordem (PPC e 360), ou o ícone (bytes do disco, iguais em qualquer build)
            if a == b:
                print(f"{nome}: {len(a)} bytes, idênticos")
                continue
            print(f"{nome}: {len(a)} bytes, {ndif} diferentes")
            if cs is None:
                erros += 1; continue
            div = []
            for o, w in cs:
                n = abs(w)
                if a[o:o + n] != b[o:o + n]:
                    div.append(f"{nome} {o:#06x} ({NOMES.get(o, 'opaco' if w < 0 else f'{w * 8} bits')}): "
                               f"{a[o:o + n][:16].hex()} contra {b[o:o + n][:16].hex()}")
            for d in div[:40]:
                print(f"  DIFERENTE: {d}")
            erros += 1
            continue
        cont, div = compara_campos(a, b, cs, "be", nome)
        print(f"{nome}: {len(a)} bytes, {ndif} diferentes; " + ", ".join(f"{k} {v}" for k, v in sorted(cont.items())
                                                                           if k != "igual" and k != "opaco igual")
              + f" (iguais byte a byte: {cont.get('igual', 0)} campos, {cont.get('opaco igual', 0)} palavras opacas)")
        for d in div[:40]:
            print(f"  DIFERENTE: {d}")
        erros += len(div)
    print("campos: " + ("os mesmos valores" if not erros else f"{erros} divergência(s)"))
    return 1 if erros else 0


def main(a):
    if not a:
        print(__doc__); return 1
    cmd = a[0]
    if cmd == "analisa":
        analisa(a[1:]); return 0
    if cmd == "preve":
        build = a[a.index("--build") + 1]
        r, p = preve(a[1], build, regra="--sem-regra" not in a)
        print(f"{r} {p}")
        return 0
    if cmd == "estraga":
        estraga(a[1], a[2], a[3]); return 0
    if cmd == "julga":
        return julga(a[1], a[a.index("--espera") + 1], int(a[a.index("--codigo") + 1]))
    if cmd == "campos":
        return campos(a[1], a[2], "--mesma-ordem" in a)
    print(__doc__); return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
