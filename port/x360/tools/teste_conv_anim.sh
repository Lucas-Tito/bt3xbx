#!/bin/bash
# Teste do host do conversor de animação, só com dados sintéticos (nenhum byte do jogo).
# Cobre o vão entre os eventos e a primeira trilha (a 0x9A do personagem 82 tem 1.860 bytes não nulos ali, que o
# jogo nunca lê) e a sabotagem que tem de continuar reprovando: byte não nulo depois das trilhas.
# Uso: teste_conv_anim.sh   (da raiz do repo; sai 0 se tudo se comportou como esperado)
set -u
cd "$(dirname "$0")/../../.."
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
gcc -std=c89 -O2 -w -o "$T/conv_tool" $(ls port/x360/conv/*.c | grep -v conv_jogo) || exit 1
python3 -I - "$T" <<'P'
import struct, sys
t = sys.argv[1]
def anim(eventos, vao, trilha_ofs, cauda=b""):
    b = bytearray(trilha_ofs)
    b[1] = eventos
    struct.pack_into("<H", b, 6, trilha_ofs // 4)          # trilha 0
    for i in range(0x94, 0x94 + eventos * 0x10):            # eventos sem volume (atributo 0)
        b[i] = 0
    for i in range(0x94 + eventos * 0x10, trilha_ofs):
        b[i] = vao(i)
    b += struct.pack("<HH", 0, 1) + bytes(0x18)             # trilha completa, 1 chave
    return bytes(b) + cauda
open(t + "/vao.bin", "wb").write(anim(0, lambda i: (i * 7 + 1) & 0xFF or 1, 0x7D8))
open(t + "/zeros.bin", "wb").write(anim(0, lambda i: 0, 0x7D8))
open(t + "/depois.bin", "wb").write(anim(0, lambda i: (i * 7 + 1) & 0xFF or 1, 0x7D8, b"\x00\x00\x00\x05"))
open(t + "/evento_vao.bin", "wb").write(anim(2, lambda i: 3, 0x7D8))
P
ruim=0
espera() { # nome esperado(0=conforme, 1=reprova)
    "$T/conv_tool" --formato animacao "$T" "$1" > "$T/saida.txt" 2>&1; r=$?
    [ "$2" -eq 1 ] && ! grep -q "byte 0x7F7 não reconhecido" "$T/saida.txt" && { echo "FALHA $1: motivo errado"; cat "$T/saida.txt"; ruim=1; }
    if [ "$r" -eq "$2" ] || { [ "$2" -eq 1 ] && [ "$r" -ne 0 ]; }; then echo "ok   $1 (saída $r, esperado $2)"
    else echo "FALHA $1 (saída $r, esperado $2)"; cat "$T/saida.txt"; ruim=1; fi
}
espera zeros.bin 0
espera vao.bin 0
espera evento_vao.bin 0
espera depois.bin 1
[ $ruim -eq 0 ] && echo "teste_conv_anim: ok" || { echo "teste_conv_anim: FALHOU"; exit 1; }
