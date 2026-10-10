#!/usr/bin/env bash
# Prepara o Ubuntu 24.04 do WSL (no Windows do Lucas) para a F1: o port de PC de referência
# (x86, 64 bits) e a build PowerPC big-endian rodando no qemu. O ISO e os dados do jogo ficam só lá.
# Sem gcc-multilib: ele conflita com o gcc-powerpc-linux-gnu no Ubuntu, e a referência é a build
# padrão de 64 bits do port.
#
# Roda como root dentro do WSL. Daqui do Linux de trabalho, pelo SSH:
#   ssh bt3win "wsl -d Ubuntu-24.04 -u root -- bash -s" < port/x360/tools/preparar-wsl.sh
# Idempotente: pode rodar de novo.
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

USUARIO=${USUARIO:-lucas}

# Usuário comum com sudo sem senha (o SSH não tem terminal para digitar senha).
if ! id "$USUARIO" >/dev/null 2>&1; then
    useradd -m -s /bin/bash -G sudo "$USUARIO"
fi
echo "$USUARIO ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/90-$USUARIO
chmod 440 /etc/sudoers.d/90-$USUARIO
printf '[user]\ndefault=%s\n[boot]\nsystemd=true\n' "$USUARIO" > /etc/wsl.conf

apt-get update -qq
apt-get install -y -qq --no-install-recommends \
    git ca-certificates openssh-client curl python3 python3-venv python3-pip \
    build-essential binutils pkg-config cmake ninja-build \
    p7zip-full glslc gnupg \
    gcc-powerpc-linux-gnu g++-powerpc-linux-gnu binutils-powerpc-linux-gnu \
    qemu-user \
    libx11-dev libxext-dev libwayland-dev libxkbcommon-dev libegl-dev libgl-dev \
    libasound2-dev libpulse-dev libudev-dev libdbus-1-dev libvulkan-dev \
    >/dev/null

# clang/LLVM 22: a versão para a qual o ptr32.py e o irfix.py do upstream foram escritos (é a do
# container de release, port/release/Dockerfile). Com o clang 18 do Ubuntu, 3 arquivos falham.
if ! command -v clang-22 >/dev/null; then
    curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg
    echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/noble/ llvm-toolchain-noble-22 main" \
        > /etc/apt/sources.list.d/llvm.list
    apt-get update -qq
    apt-get remove -y -qq python3-clang python3-clang-18 >/dev/null 2>&1 || true
    apt-get install -y -qq --no-install-recommends clang-22 llvm-22 libclang-22-dev python3-clang-22 >/dev/null
fi
ln -sf /usr/bin/clang-22 /usr/local/bin/clang
ln -sf /usr/bin/llc-22 /usr/local/bin/llc

# SDL3: o Ubuntu 24.04 não tem pacote. O port de PC exige (janela, som, controle); compila do
# fonte, numa versão fixa, para 64 bits.
SDL_VER=3.2.24
if ! pkg-config --exists sdl3 2>/dev/null; then
    tmp=$(mktemp -d)
    curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VER/SDL3-$SDL_VER.tar.gz" | tar -xz -C "$tmp"
    cmake -S "$tmp/SDL3-$SDL_VER" -B "$tmp/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF >/dev/null
    cmake --build "$tmp/build" >/dev/null
    cmake --install "$tmp/build" >/dev/null
    ldconfig
    rm -rf "$tmp"
fi

echo "pronto: usuário $USUARIO, $(gcc --version | head -1), $(clang --version | head -1), $(powerpc-linux-gnu-gcc --version | head -1), $(qemu-ppc --version | head -1), SDL3 $(pkg-config --modversion sdl3)"
