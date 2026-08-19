#!/usr/bin/env bash
# sobe o worker de extracao. carrega o worker/.env e roda o loop.
set -euo pipefail
raiz="$(cd "$(dirname "$0")/.." && pwd)"

if [ ! -f "$raiz/worker/.env" ]; then
    echo "crie o worker/.env a partir do worker/.env.example e preencha as chaves"
    exit 1
fi

# o claude e o yt-dlp precisam estar no PATH mesmo sem terminal (boot)
export PATH="$HOME/.local/bin:/usr/local/bin:$PATH"
set -a; . "$raiz/worker/.env"; set +a
export RAIZ="$raiz"

exec python3 "$raiz/worker/worker.py"
