#!/usr/bin/env bash
# Teste do coff.py: converte todos os port/data/*.s e um .s de formas próprio em objetos COFF do 360,
# confere cada objeto no dumpbin do XDK, liga tudo com o link.exe junto com um C mínimo e confere no .exe
# que os endereços e os números ficaram certos e big-endian. Não usa nenhum byte do jogo.
#   port/x360/tools/teste_coff.sh            (saída em build/x360/coff-teste)
set -euo pipefail
IMAGEM=${IMAGEM:-collectionui-xdk:light}
XDK_DIR=${XDK_DIR:-$HOME/Documentos/CollectionUI/sdk/XDK}
RAIZ=$(cd "$(dirname "$0")/../../.." && pwd)
DIR=build/x360/coff-teste
COFF="$RAIZ/port/x360/tools/coff.py"
cd "$RAIZ"
[ -f "$XDK_DIR/bin/win32/dumpbin.exe" ] \
    || { echo "XDK não encontrado em $XDK_DIR (defina XDK_DIR)"; exit 1; }
rm -rf "$DIR"; mkdir -p "$DIR/obj"

# 1. As formas que o esqueleto usa hoje e as que ele ainda não usa (números, texto, rodata, sbss, .L).
cat > "$DIR/formas.s" <<'EOF'
# formas para o teste do coff.py; nenhum byte do jogo
.section .rodata
.p2align 3
.globl teste_ro
teste_ro:
    .short 0x1234, -2
    .byte 1, 2, 0xff
    .p2align 2
.globl teste_num
teste_num:
    .long 0xCAFEBABE, 7
    .quad 0x0102030405060708
.globl teste_texto
teste_texto:
    .asciz "BT3\n"
    .ascii "ab"   # sem o zero
    .balign 4
.globl teste_real
teste_real:
    .float 1.5
.section .data
.p2align 4
.globl teste_ptr
teste_ptr:
    .long teste_num             # mesmo objeto, outra seção
    .long teste_num+4
    .long D_002EB380            # outro objeto de dados (cod_1EB380.rodata)
    .long gBattleReplay+12296   # .bss de outro objeto, com soma
    .long teste_funcao          # código do C
    .long rotulo_estatico       # rótulo sem .globl
    .long .Lsem_simbolo         # rótulo que não vai para a tabela
rotulo_estatico:
    .zero 4
.Lsem_simbolo:
    .long -1
.section .sbss
.p2align 5
.globl teste_bss
teste_bss:
    .space 64
EOF

cat > "$DIR/teste.c" <<'EOF'
/* Ligação dos objetos do coff.py com código do cl: lê rótulos de outros objetos e guarda o
   endereço de um deles (relocação do cl para um símbolo de um objeto nosso). */
extern unsigned char D_002EB380[];
extern int *teste_ptr[];
extern short teste_ro[];
extern char teste_bss[];
int teste_funcao(void) { return 42; }
unsigned char *teste_c_para_dados = D_002EB380;
int main(void)
{
    return (int)teste_ptr[0] + teste_ro[0] + teste_bss[0] + teste_c_para_dados[0];
}
EOF

python3 "$COFF" --saida "$DIR/obj" port/data/*.s
python3 "$COFF" -o "$DIR/formas.obj" "$DIR/formas.s"

# Os .long que apontam para o código do jogo (fora daqui) ganham um rótulo de 4 bytes cada.
{
    echo ".section .data"
    python3 "$COFF" --externos port/data/*.s "$DIR/formas.s" | grep -vx teste_funcao \
        | while read -r s; do echo ".globl $s"; echo "$s:"; echo "    .space 4"; done
} > "$DIR/stubs.s"
python3 "$COFF" -o "$DIR/stubs.obj" "$DIR/stubs.s"
echo "objetos: $(ls "$DIR"/obj/*.obj | wc -l) de port/data, formas.obj, stubs.obj ($(grep -c '^\.globl' "$DIR/stubs.s") rótulos)"

# 2. No XDK: compila o C, passa cada objeto nosso pelo dumpbin e liga tudo.
docker run --rm -v "$RAIZ":/app -v "$XDK_DIR/lib/xbox":/xdk/lib/xbox:ro -v "$XDK_DIR/bin/win32":/xdkbin:ro \
    -w /app/$DIR -e WINEDEBUG=-all "$IMAGEM" bash -c '
  wineboot -i >/dev/null 2>&1 || true
  mkdir -p dump
  wine /xdkbin/cl.exe -nologo -TC -D _XBOX -c -Ox teste.c -Foteste.obj > cl.txt 2>&1 || { cat cl.txt; exit 1; }
  for o in obj/*.obj formas.obj stubs.obj; do
    wine /xdkbin/dumpbin.exe /headers /symbols /relocations "$o" > dump/$(basename "$o" .obj).txt 2>&1 || echo "dumpbin falhou: $o"
  done
  LIB=/xdk/lib/xbox wine /xdkbin/link.exe -NOLOGO -RELEASE -XEX:NO -MAP:teste.map -OUT:teste.exe \
      teste.obj obj/*.obj formas.obj stubs.obj > link.txt 2>&1; echo $? > link.status
  chown -R '"$(id -u):$(id -g)"' .
'
cat "$DIR/link.txt"
[ "$(cat "$DIR/link.status")" = 0 ] || { echo "FALHOU: link.exe saiu com $(cat "$DIR/link.status")"; exit 1; }

# 3. Conferências.
python3 - "$DIR" "$COFF" port/data/*.s <<'PY'
import glob, importlib.util, os, re, struct, sys
d, coff_py, fontes = sys.argv[1], sys.argv[2], sys.argv[3:] + [sys.argv[1] + "/formas.s"]
spec = importlib.util.spec_from_file_location("coff", coff_py)
coff = importlib.util.module_from_spec(spec); spec.loader.exec_module(coff)
falhas = []
def falha(msg):
    falhas.append(msg)
    if len(falhas) <= 20:
        print("FALHA:", msg)

# 3a. dumpbin: sem erro, seções do tamanho e alinhamento do .s, cada rótulo no deslocamento do .s
montagens = {}
for f in fontes:
    m = coff.Montagem(f)
    nome = os.path.splitext(os.path.basename(f))[0]
    montagens[nome] = m
    txt = open(f"{d}/dump/{nome}.txt", errors="replace").read()
    if re.search(r"(error|warning) LNK|fatal", txt):
        falha(f"{nome}: dumpbin reclamou: " + "; ".join(re.findall(r".*(?:error|warning|fatal).*", txt))[:200])
    if "1F2 machine (PPCBE)" not in txt:
        falha(f"{nome}: máquina não é PPCBE")
    secs = re.findall(r"SECTION HEADER #(\d+)\n\s*(\S+) name.*?\n\s*([0-9A-F]+) size of raw data.*?"
                      r"\n\s*([0-9A-F]+) number of relocations.*?flags\n(.*?)\n\n", txt, re.S)
    lista = list(m.secoes.values())
    if len(secs) != len(lista):
        falha(f"{nome}: {len(secs)} seções no dumpbin, {len(lista)} no .s")
    for (num, sn, tam, nrel, flags), s in zip(secs, lista):
        if sn != s.nome or int(tam, 16) != s.tamanho or int(nrel, 16) != len(s.relocs):
            falha(f"{nome}: seção {num} é {sn} {tam} {nrel}, esperado {s.nome} {s.tamanho:X} {len(s.relocs):X}")
        if f"{s.alinhamento} byte align" not in flags:
            falha(f"{nome}: seção {sn} sem '{s.alinhamento} byte align'")
    vistos = {}
    for off, sec, classe, sim in re.findall(r"^[0-9A-F]{3,} ([0-9A-F]{8}) SECT(\d+)\s+notype\s+(External|Static)\s+\| (\S+)$", txt, re.M):
        vistos[sim] = (int(off, 16), lista[int(sec) - 1].nome, classe)
    for r, (s, off) in m.rotulos.items():
        if r.startswith(".L"):
            continue
        esperado = (off, s.nome, "External" if r in m.globais else "Static")
        if vistos.get(r) != esperado:
            falha(f"{nome}: {r} no dumpbin é {vistos.get(r)}, no .s {esperado}")
    nrel = len(re.findall(r"^\s*[0-9A-F]{8}\s+ADDR32\s", txt, re.M))
    if nrel != sum(len(s.relocs) for s in lista):
        falha(f"{nome}: {nrel} relocações ADDR32 no dumpbin")
print(f"dumpbin: {len(montagens)} objetos conferidos")

# 3b. mapa e .exe
mapa = {}
for l in open(f"{d}/teste.map", errors="replace"):
    c = l.split()
    if len(c) >= 4 and re.fullmatch(r"[0-9a-f]{4}:[0-9a-f]{8}", c[0]) and re.fullmatch(r"[0-9a-f]{8}", c[2]):
        mapa.setdefault(c[1], int(c[2], 16))
exe = open(f"{d}/teste.exe", "rb").read()
pe = struct.unpack_from("<I", exe, 0x3C)[0]
nsec, = struct.unpack_from("<H", exe, pe + 6)
opt, = struct.unpack_from("<H", exe, pe + 20)
base, = struct.unpack_from("<I", exe, pe + 24 + 28)
secs = [struct.unpack_from("<8sIIII", exe, pe + 24 + opt + 40 * i) for i in range(nsec)]
def le(va, n):
    rva = va - base
    for _, vsize, vaddr, rsize, rptr in secs:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            o = rva - vaddr
            return exe[rptr + o:rptr + o + n] if o + n <= rsize else bytes(n)
    raise KeyError(hex(va))

def endereco(m, rotulo):
    """endereço de um rótulo pelo mapa, ou pelo de um rótulo global da mesma seção."""
    if rotulo in mapa and rotulo in m.globais:
        return mapa[rotulo]
    s, off = m.rotulos[rotulo]
    for r, (s2, off2) in m.rotulos.items():
        if s2 is s and r in m.globais and r in mapa:
            return mapa[r] - off2 + off
    raise KeyError(rotulo)

definidos = {r: m for m in montagens.values() for r in m.globais if r in m.rotulos}
ponteiros = 0
for nome, m in montagens.items():
    for r in m.globais & set(m.rotulos):
        if r not in mapa:
            falha(f"{nome}: {r} não está no mapa")
        else:
            s, _ = m.rotulos[r]
            if endereco(m, r) % s.alinhamento and m.rotulos[r][1] == 0:
                falha(f"{nome}: {r} em {mapa[r]:#x} fora do alinhamento {s.alinhamento}")
    for s in m.secoes.values():
        for off, alvo in s.relocs:
            soma = struct.unpack_from(">i", s.dados, off)[0]
            if alvo in m.rotulos:
                esperado = endereco(m, alvo) + soma
            elif alvo in definidos:
                esperado = endereco(definidos[alvo], alvo) + soma
            else:
                esperado = mapa[alvo] + soma
            for r, (s2, o2) in m.rotulos.items():
                if s2 is s and r in m.globais:
                    lugar = mapa[r] - o2 + off
                    break
            v = struct.unpack(">I", le(lugar, 4))[0]
            ponteiros += 1
            if v != esperado & 0xFFFFFFFF:
                falha(f"{nome}: .long {alvo}{soma:+d} em {lugar:#x} vale {v:#x}, esperado {esperado:#x}")
print(f"ligação: {len(mapa)} símbolos no mapa, {ponteiros} ponteiros .long conferidos no .exe (big-endian)")

# formas: números big-endian, texto, alinhamento, ponteiros de cada tipo
f = montagens["formas"]
def confere(rotulo, esperado):
    v = le(mapa[rotulo], len(esperado))
    if v != esperado:
        falha(f"formas: {rotulo} = {v.hex()} no .exe, esperado {esperado.hex()}")
confere("teste_ro", bytes.fromhex("1234fffe0102ff00"))
confere("teste_num", bytes.fromhex("cafebabe000000070102030405060708"))
confere("teste_texto", b"BT3\n\0ab\0")
confere("teste_real", bytes.fromhex("3fc00000"))
for r, n in (("teste_bss", 32), ("teste_ptr", 16), ("teste_ro", 8)):
    if mapa[r] % n:
        falha(f"formas: {r} em {mapa[r]:#x} não está alinhado a {n}")
ptr = [struct.unpack(">I", le(mapa["teste_ptr"] + 4 * i, 4))[0] for i in range(9)]
esperado = [mapa["teste_num"], mapa["teste_num"] + 4, mapa["D_002EB380"], mapa["gBattleReplay"] + 12296,
            mapa["teste_funcao"], mapa["teste_ptr"] + 28, mapa["teste_ptr"] + 32, 0, 0xFFFFFFFF]
if ptr != esperado:
    falha("formas: teste_ptr = " + " ".join(f"{x:08x}" for x in ptr) + ", esperado " + " ".join(f"{x:08x}" for x in esperado))
c = struct.unpack(">I", le(mapa["teste_c_para_dados"], 4))[0]
if c != mapa["D_002EB380"]:
    falha(f"teste_c_para_dados (do cl) = {c:#x}, esperado {mapa['D_002EB380']:#x}")
print("formas: " + " ".join(f"{k}={mapa[k]:#x}" for k in ("teste_ro", "teste_ptr", "teste_bss", "teste_funcao", "D_002EB380", "gBattleReplay")))
if falhas:
    sys.exit(f"{len(falhas)} falhas")
print("OK")
PY
