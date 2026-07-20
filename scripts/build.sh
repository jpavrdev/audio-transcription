#!/usr/bin/env bash
# compila o transcritor. na primeira vez baixa e compila o whisper.cpp junto.
# se o nvcc estiver instalado, compila com suporte a gpu (cuda).
set -euo pipefail

raiz="$(cd "$(dirname "$0")/.." && pwd)"

flags=""
jobs=4
if command -v nvcc >/dev/null 2>&1; then
    flags="-DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=86"
    # o nvcc do cuda 12.0 nao aceita gcc 13, entao o codigo host vai no g++-12
    if command -v g++-12 >/dev/null 2>&1; then
        flags="$flags -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-12"
    fi
    jobs=2
    echo "cuda encontrado, compilando com gpu (arch 8.6)"
fi

cmake -S "$raiz" -B "$raiz/build" -DCMAKE_BUILD_TYPE=Release $flags
cmake --build "$raiz/build" -j "$jobs"
echo "binario: $raiz/build/transcritor"
