#!/usr/bin/env bash
# sobe o banco e o servidor do painel. use depois de rodar o instalar.sh.
# uso: ./scripts/iniciar.sh
set -euo pipefail

raiz="$(cd "$(dirname "$0")/.." && pwd)"

# o claude e o yt-dlp precisam estar no PATH mesmo se subir sem terminal (boot)
export PATH="$HOME/.local/bin:/usr/local/bin:$PATH"

# banco nativo
sudo service postgresql start || true

# carrega as variaveis do .env pro servidor achar o banco
if [ -f "$raiz/servidor/.env" ]; then
    set -a; . "$raiz/servidor/.env"; set +a
fi

echo "servidor em http://localhost:8080 (ctrl+c pra parar)"
exec "$raiz/servidor/build/servidor" servir
