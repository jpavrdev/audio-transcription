#!/usr/bin/env bash
# compila o transcritor. na primeira vez baixa e compila o whisper.cpp junto.
set -euo pipefail

raiz="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$raiz" -B "$raiz/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$raiz/build" -j 4
echo "binario: $raiz/build/transcritor"
