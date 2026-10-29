#!/usr/bin/env python3
"""F7/03: entrada realmente lida, duas vistas e perfil informativo do backend falso.

entrada LOG PAD: cruza a gravação scePad2Read com o par preparado pelo roteiro, quadro a quadro.
vistas LOG: exige geometria nas duas tesouras, câmeras distintas e dividida/cheia/dividida.
imagens PPM: conteúdo e diferenças entre as metades do quadro de software.
perfil SIMPLES DIVIDIDO: quantidades e tiques da mesma janela marcada, sem estimar o Xenon.
negativos LOG PAD GPU SIMPLES PPM SAIDA: cada cópia adulterada deve ser recusada com saída 1.
"""
import argparse
import json
import pathlib
import re
import statistics
import subprocess
import sys


class Falha(ValueError):
    pass


def exige(cond, texto):
    if not cond:
        raise Falha(texto)


def ler(p):
    return pathlib.Path(p).read_text(errors="replace")


def inteiro_fim(s):
    m = re.findall(r"^roteiro: \S+ fim vblank \d+ quadro (\d+)$", s, re.M)
    exige(len(m) == 1, "falta um único fim do roteiro")
    exige("FALHA" not in s and "sem conversor em" not in s and "NÃO IMPLEMENTADO" not in s.upper(),
          "falha, guarda ou stub no log")
    return int(m[0])


PADS = re.compile(r"^roteiro: pads ([0-9a-f]{4}) ([0-9a-f]{4}) analogicos (\d+) (\d+) (\d+) (\d+) "
                  r"vblank \d+ quadro (\d+)$", re.M)
SELECAO = re.compile(r"^roteiro: selecao jogadores (\d+) estados (\d+) (\d+) personagens (\d+) (\d+) "
                     r"vblank \d+ quadro (\d+)$", re.M)
VISTAS = re.compile(r"^f7v: gs (\d+) quadro (\d+) seq (\d+) modo (\d+) cameras (\d+) ([0-9a-f]{8}) (\d+) (\d+) "
                    r"(\d+) ([0-9a-f]{8}) (\d+) (\d+) indices (\d+) (\d+) (\d+) desenhos (\d+) (\d+) (\d+) "
                    r"totais (\d+) (\d+) (\d+)$", re.M)


def entrada(log, pad):
    s = ler(log)
    fim = inteiro_fim(s)
    dados = pathlib.Path(pad).read_bytes()
    exige(len(dados) == fim * 36, f"relógio/leituras: {len(dados)} bytes, esperado {fim} pares de 36")
    eventos = [tuple(int(x, 16 if i < 2 else 10) for i, x in enumerate(m)) for m in PADS.findall(s)]
    exige(eventos and eventos[0][-1] == 0, "sem entrada inicial do par")
    exige(all(a[-1] < b[-1] for a, b in zip(eventos, eventos[1:])), "relógio de entrada repetido ou regressivo")
    k = 0
    usados = [set(), set()]
    juntos = False
    for q in range(fim):
        if k + 1 < len(eventos) and eventos[k + 1][-1] == q:
            k += 1
        b0, b1, x0, y0, x1, y1, _ = eventos[k]
        esperado = b"".join(bytes([(~b) & 255, (~(b >> 8)) & 255, 128, 128, x, y] + [0] * 12)
                            for b, x, y in ((b0, x0, y0), (b1, x1, y1)))
        exige(dados[q * 36:(q + 1) * 36] == esperado, f"entrada gravada difere do par no quadro {q}")
        usados[0].add(b0)
        usados[1].add(b1)
        juntos |= b0 == b1 == 0x4000
    exige(k == len(eventos) - 1 and eventos[-1][-1] < fim, "entrada fora do roteiro")
    exige(juntos and any(b != 0 for b in usados[1]) and usados[0] != usados[1],
          "sem confirmações simultâneas ou segundo controle distinto")
    sel = [tuple(map(int, m)) for m in SELECAO.findall(s)]
    exige(sel and all(v[0] == 1 for v in sel), "seletores não têm uma porta por lado")
    exige(any(a[1:3] == (0, 2) and b[1:3] == (0, 0) for a, b in zip(sel, sel[1:])),
          "cancelamento do jogador 2 não devolveu só seu seletor à grade")
    exige(any(v[1:5] == (7, 7, 0, 25) for v in sel), "os dois seletores não confirmaram Goku/Kuririn")
    exige("roteiro: dividido 1 controles 0/0 0/1 " in s, "controles não associados aos lados 0/1")
    for lado, faixa in ((0, "0xd-0x35"), (1, "0xd-0x35"), (0, "0x37-0x37"), (1, "0x37-0x37"),
                        (1, "0x38-0x38"), (0, "0x105-0x12f")):
        exige(f"ok acao {lado} {faixa} " in s, f"ação ausente no lado {lado}: {faixa}")
    exige(len(re.findall(r"ok pausa 1 vblank", s)) == 2 and len(re.findall(r"ok pausa 0 vblank", s)) == 2,
          "pausa/retomada dos dois controles ausente")
    exige(re.search(r"resultado estado 4 vencedor 0x1 razao 0x1 vida [1-9]\d* 0 ", s), "sem resultado por KO do lado 1")
    exige(re.search(r"derrubado lado 1 ", s), "resultado não identifica o dano de teste")
    exige(re.search(r"ok tela 4 vblank.*\n(?:.*\n)*roteiro: \S+ marca ponto retorno ", s), "sem retorno ao menu")
    return {"pares": fim, "mudancas": len(eventos), "seletores": len(sel), "resultado": "KO dado, lado 0 vence"}


def linhas_vistas(s):
    out = []
    for m in VISTAS.findall(s):
        out.append([int(x, 16 if i in (5, 9) else 10) for i, x in enumerate(m)])
    exige(out, "sem dados do backend falso")
    exige(all(a[0] < b[0] and a[1] <= b[1] for a, b in zip(out, out[1:])), "quadros do backend fora de ordem")
    return out


def marca(s, nome):
    m = re.findall(r"^roteiro: \S+ marca ponto " + re.escape(nome) + r" vblank \d+ quadro (\d+)$", s, re.M)
    exige(len(m) == 1, f"marca {nome} ausente ou repetida")
    return int(m[0])


def vistas(log):
    s = ler(log)
    inteiro_fim(s)
    linhas = linhas_vistas(s)
    inicio, fim = marca(s, "dividido"), marca(s, "perfil_fim")
    janela = [v for v in linhas if inicio < v[1] <= fim]
    exige(len(janela) >= 180, "janela dividida menor que 180 quadros")
    def metades(v):
        return (v[3] == 1 and v[4] == v[8] == 1 and v[5] != v[9] and
                v[6:8] == [0, 254] and v[10:12] == [257, 511] and min(v[12:14]) > 0 and min(v[15:17]) > 0)
    exige(all(v[2] == 3 and metades(v) for v in janela), "metade sem geometria, câmera repetida ou tesoura errada")
    tecnica, volta = marca(s, "cheia"), marca(s, "dividido_volta")
    cheia = [v for v in linhas if fim < v[1] <= tecnica + 90 and v[3] == 1 and v[14] > 0 and
             ((v[4] == 0 and v[6:8] == [0, 511]) or (v[8] == 0 and v[10:12] == [0, 511]))]
    exige(cheia, "técnica não entregou vista cheia")
    retorno = [v for v in linhas if volta <= v[1] < volta + 10]
    exige(retorno and all(metades(v) for v in retorno), "técnica não voltou às duas metades")
    menu = marca(s, "retorno")
    ret = [tuple(map(int, m)) for m in re.findall(
        r"^f7m: gs (\d+) quadro (\d+) indices (\d+) (\d+) (\d+) desenhos (\d+) (\d+) (\d+)$", s, re.M)]
    ret = [v for v in ret if menu < v[1] <= menu + 20]
    exige(len(ret) == 20 and all(v[4] > 0 and v[7] > 0 for v in ret), "menu não voltou à tesoura cheia")
    return {"quadros_menu_cheio": len(ret), "quadros_duas_metades": len(janela), "quadros_cheios_tecnica": len(cheia), "retorno": len(retorno)}


def imagens(caminho):
    import roteiros
    w, h, px = roteiros.ler_ppm(caminho)
    exige((w, h) == (640, 448), "imagem sem a geometria esperada do GS")
    metades = []
    for x0, x1 in ((0, 255), (257, 512)):
        parte = b"".join(px[(y * w + x0) * 3:(y * w + x1) * 3] for y in range(32, 416))
        cores = set(zip(parte[::3], parte[1::3], parte[2::3]))
        claros = sum(max(c) > 24 for c in zip(parte[::3], parte[1::3], parte[2::3]))
        exige(len(cores) >= 64 and claros > len(parte) // 3 // 20, "metade sem conteúdo visível suficiente")
        metades.append(parte)
    exige(metades[0] != metades[1], "metades da imagem repetidas")
    return {"metades_com_conteudo": 2, "pixels_por_metade": len(metades[0]) // 3}


def resumo(valores):
    v = sorted(valores)
    exige(v, "amostra vazia")
    return {"media": round(statistics.mean(v), 2), "p50": v[len(v) // 2],
            "p95": v[min(len(v) - 1, int(len(v) * .95))], "maximo": max(v)}


def perfil(log):
    s = ler(log)
    inteiro_fim(s)
    a, b = marca(s, "dividido"), marca(s, "perfil_fim")
    janela = [v for v in linhas_vistas(s) if a < v[1] <= b]
    exige(len(janela) >= 180, "perfil menor que 180 quadros")
    tempos = {int(gs): tuple(map(int, (c, t, g))) for gs, c, t, g in re.findall(
        r"^f7p: gs (\d+) chamadas (\d+) tiques (\d+) gravacao (\d+)$", s, re.M)}
    exige(all(v[0] in tempos for v in janela), "sem contadores do perfil por quadro")
    exige(all(tempos[v[0]][0] > 0 and tempos[v[0]][1] > 0 for v in janela), "perfil sem chamadas/tiques x86")
    return {"quadros": len(janela), **{nome: resumo([v[i] for v in janela]) for nome, i in
            (("desenhos", 18), ("indices", 19), ("constantes", 20))},
            **{nome: resumo([tempos[v[0]][i] for v in janela]) for nome, i in
               (("chamadas_vu1", 0), ("tiques_vu1", 1), ("tiques_gravacao", 2))}}


def negativos(log, pad, gpu, simples, ppm, saida):
    # Invoca a mesma CLI sobre cópias, exigindo exatamente saída 1, não só uma exceção interna.
    destino = pathlib.Path(saida)
    destino.mkdir(parents=True, exist_ok=False)
    entrada(log, pad)
    vistas(gpu)
    perfil(simples)
    perfil(gpu)
    imagens(ppm)
    casos = []
    def recusa(nome, modo, texto=None, binario=None):
        pasta = destino / nome
        pasta.mkdir()
        l = pasta / "log.txt"
        l.write_text(ler(log if modo == "entrada" else gpu) if texto is None else texto)
        args = [sys.executable, __file__, modo]
        if modo == "perfil": args.append(str(simples))
        args.append(str(l))
        if modo == "entrada":
            p = pasta / "pad.bin"
            p.write_bytes(pathlib.Path(pad).read_bytes() if binario is None else binario)
            args.append(str(p))
        r = subprocess.run(args, capture_output=True, text=True)
        (pasta / "resultado.txt").write_text(r.stdout + r.stderr)
        exige(r.returncode == 1, f"negativo {nome} não recusado com 1: {r.returncode}")
        casos.append(nome)
    dados = bytearray(pathlib.Path(pad).read_bytes())
    for q in range(len(dados) // 36):
        dados[q * 36 + 18:q * 36 + 20] = b"\xff\xff"
    recusa("porta2_neutra", "entrada", binario=dados)
    recusa("par_truncado", "entrada", binario=pathlib.Path(pad).read_bytes()[:-18])
    recusa("sem_cancelamento", "entrada", texto=SELECAO.sub("", ler(log)))
    recusa("sem_resultado", "entrada", texto=re.sub(r"^roteiro: resultado.*$", "", ler(log), flags=re.M))
    recusa("sem_fim", "entrada", texto=re.sub(r"^roteiro: \S+ fim .*$", "", ler(log), flags=re.M))
    def muda_vistas(qual):
        limite = marca(ler(gpu), "dividido_volta")
        def troca(m):
            v = list(m.groups())
            if qual == "metade": v[13] = "0"
            if qual == "camera": v[9] = v[5]
            if qual == "cheia": v[14] = "0"
            if qual == "volta" and int(v[1]) >= limite: v[8] = "0"
            return ("f7v: gs %s quadro %s seq %s modo %s cameras %s %s %s %s %s %s %s %s "
                    "indices %s %s %s desenhos %s %s %s totais %s %s %s") % tuple(v)
        return VISTAS.sub(troca, ler(gpu))
    for nome in ("metade", "camera", "cheia", "volta"):
        recusa(nome, "vistas", texto=muda_vistas(nome))
    recusa("menu_cortado", "vistas", texto=re.sub(r"^f7m:.*$", "", ler(gpu), flags=re.M))
    recusa("sem_perfil", "perfil", texto=re.sub(r"^f7p:.*$", "", ler(gpu), flags=re.M))
    import roteiros
    w, h, px = roteiros.ler_ppm(ppm)
    for nome in ("imagem_sem_metade", "imagem_repetida"):
        p = destino / (nome + ".ppm")
        dados = bytearray(px)
        for y in range(h):
            ini, fim = (y * w + 257) * 3, (y * w + 512) * 3
            dados[ini:fim] = b"\0" * (fim - ini) if nome == "imagem_sem_metade" else px[y * w * 3:(y * w + 255) * 3]
        p.write_bytes(f"P6\n{w} {h}\n255\n".encode() + dados)
        r = subprocess.run([sys.executable, __file__, "imagens", str(p)], capture_output=True, text=True)
        (destino / (nome + ".txt")).write_text(r.stdout + r.stderr)
        exige(r.returncode == 1, f"negativo {nome} não recusado com 1: {r.returncode}")
        casos.append(nome)
    return {"negativos_recusados": casos}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("modo", choices=("entrada", "vistas", "imagens", "perfil", "negativos"))
    ap.add_argument("arquivos", nargs="+")
    a = ap.parse_args()
    try:
        if a.modo == "entrada": r = entrada(*a.arquivos)
        elif a.modo == "vistas": r = vistas(*a.arquivos)
        elif a.modo == "imagens": r = imagens(*a.arquivos)
        elif a.modo == "perfil":
            r = {"simples": perfil(a.arquivos[0]), "dividido": perfil(a.arquivos[1]),
                 "limite": "x86, tiques do host; COM no simples e humano no dividido; não prova 30 fps no console"}
        else: r = negativos(*a.arquivos)
        print(json.dumps(r, ensure_ascii=False, indent=2))
        print("tudo certo")
    except (Falha, OSError, ValueError, TypeError) as e:
        print(f"FALHA: {e}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
