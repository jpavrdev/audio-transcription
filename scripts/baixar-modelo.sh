#!/usr/bin/env bash
# baixa um modelo ggml do whisper.cpp pra pasta models/
# uso: scripts/baixar-modelo.sh [nome]
# nomes comuns: base, small, medium, large-v3
set -euo pipefail

modelo="${1:-small}"
raiz="$(cd "$(dirname "$0")/.." && pwd)"
destino="$raiz/models/ggml-$modelo.bin"
url="https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-$modelo.bin"

if [ -f "$destino" ]; then
    echo "ja existe: $destino"
    exit 0
fi

echo "baixando $modelo"
mkdir -p "$raiz/models"
curl -L --fail --retry 3 -o "$destino.part" "$url"
mv "$destino.part" "$destino"
echo "pronto: $destino"
