#!/usr/bin/env bash
# checa a conexao com o supabase usando as chaves do worker/.env.
set -euo pipefail
raiz="$(cd "$(dirname "$0")/.." && pwd)"

if [ ! -f "$raiz/worker/.env" ]; then
    echo "crie o worker/.env a partir do worker/.env.example e preencha as chaves"
    exit 1
fi

set -a; . "$raiz/worker/.env"; set +a
exec python3 "$raiz/worker/testar.py"
