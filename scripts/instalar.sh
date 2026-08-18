#!/usr/bin/env bash
# instalador do painel de leiloes para ubuntu ou wsl2 (windows).
# instala as dependencias, compila o servidor e o transcritor, prepara o
# postgres nativo, aplica as migrations e cria o admin. sem docker e sem gpu.
# uso: ./scripts/instalar.sh
set -euo pipefail

raiz="$(cd "$(dirname "$0")/.." && pwd)"
titulo() { printf "\n\033[1;36m== %s ==\033[0m\n" "$1"; }
aviso()  { printf "\033[1;33m%s\033[0m\n" "$1"; }

if ! command -v apt-get >/dev/null 2>&1; then
    echo "este instalador e para ubuntu/debian, incluindo o wsl2 no windows."
    echo "no windows instale o wsl2 com ubuntu primeiro. veja INSTALAR-WINDOWS.md"
    exit 1
fi

titulo "dependencias do sistema (vai pedir a senha do sudo)"
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config ca-certificates curl \
    libjsoncpp-dev uuid-dev zlib1g-dev libssl-dev \
    libpq-dev libpqxx-dev libargon2-dev \
    ffmpeg python3 postgresql

titulo "yt-dlp (canal nightly, o do apt e velho e toma 403 do youtube)"
mkdir -p "$HOME/.local/bin"
curl -L --fail --retry 3 \
    https://github.com/yt-dlp/yt-dlp-nightly-builds/releases/latest/download/yt-dlp \
    -o "$HOME/.local/bin/yt-dlp"
chmod +x "$HOME/.local/bin/yt-dlp"
case ":$PATH:" in
    *":$HOME/.local/bin:"*) ;;
    *) echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc" ;;
esac

titulo "configuracao (.env)"
if [ ! -f "$raiz/servidor/.env" ]; then
    cp "$raiz/servidor/.env.example" "$raiz/servidor/.env"
    sed -i 's/^DB_PORT=.*/DB_PORT=5432/' "$raiz/servidor/.env"   # postgres nativo
    echo "criado servidor/.env (postgres nativo na porta 5432)"
else
    echo "servidor/.env ja existe, mantido como esta"
fi
set -a; . "$raiz/servidor/.env"; set +a

titulo "banco postgres (papel e base)"
sudo service postgresql start || true
sudo -u postgres psql -tc "SELECT 1 FROM pg_roles WHERE rolname='$DB_USER'" | grep -q 1 \
    || sudo -u postgres psql -c "CREATE ROLE \"$DB_USER\" LOGIN PASSWORD '$DB_PASSWORD'"
sudo -u postgres psql -tc "SELECT 1 FROM pg_database WHERE datname='$DB_NAME'" | grep -q 1 \
    || sudo -u postgres createdb -O "$DB_USER" "$DB_NAME"
echo "banco '$DB_NAME' pronto para o usuario '$DB_USER'"

# poucos jobs de compilacao se a maquina tem pouca ram, pra nao estourar
mem_kb=$(awk '/MemTotal/{print $2}' /proc/meminfo 2>/dev/null || echo 0)
jobs=2; [ "${mem_kb:-0}" -lt 6000000 ] && jobs=1
echo "compilando com -j$jobs (ram total ${mem_kb} kb)"

titulo "compilando o servidor (a primeira vez baixa e compila o drogon, demora)"
cmake -S "$raiz/servidor" -B "$raiz/servidor/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$raiz/servidor/build" -j "$jobs"

titulo "compilando o transcritor (cpu, reserva pra videos sem legenda)"
cmake -S "$raiz" -B "$raiz/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$raiz/build" -j "$jobs"

titulo "modelo whisper small (leve, so entra quando o video nao tem legenda)"
"$raiz/scripts/baixar-modelo.sh" small || aviso "download do modelo falhou, da pra refazer depois"

titulo "migrations"
"$raiz/servidor/build/servidor" migrar

titulo "usuario admin"
if [ -t 0 ]; then
    while :; do
        read -rsp "senha do admin (min 8 caracteres): " s1; echo
        read -rsp "repita a senha: " s2; echo
        [ "$s1" = "$s2" ] || { aviso "as senhas nao batem, tente de novo"; continue; }
        [ "${#s1}" -ge 8 ] || { aviso "muito curta"; continue; }
        printf '%s\n' "$s1" | "$raiz/servidor/build/servidor" criar-admin admin && break
    done
else
    aviso "sem terminal interativo, crie o admin depois com:"
    aviso "  printf 'SUA_SENHA\\n' | $raiz/servidor/build/servidor criar-admin admin"
fi

titulo "claude code (extracao)"
if command -v claude >/dev/null 2>&1; then
    echo "claude encontrado. rode 'claude' uma vez e logue na conta Max se ainda nao logou."
else
    aviso "o claude code NAO esta instalado. a extracao depende dele."
    aviso "instale o claude code no ubuntu e rode 'claude' pra logar na conta Max."
fi

titulo "pronto"
echo "suba o painel com:  ./scripts/iniciar.sh"
echo "depois abra http://localhost:8080 e entre com o usuario admin"
