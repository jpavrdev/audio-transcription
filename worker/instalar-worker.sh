#!/usr/bin/env bash
# instalador do worker de extracao (arquitetura supabase).
# instala so o que o notebook precisa pra transcrever e extrair.
# sem drogon, sem postgres, sem docker. bem mais leve que o instalador antigo.
set -euo pipefail

raiz="$(cd "$(dirname "$0")/.." && pwd)"
titulo() { printf "\n\033[1;36m== %s ==\033[0m\n" "$1"; }
aviso()  { printf "\033[1;33m%s\033[0m\n" "$1"; }

if ! command -v apt-get >/dev/null 2>&1; then
    echo "este instalador e para ubuntu/debian, incluindo o wsl2 no windows."
    echo "no windows, instale o wsl2 com ubuntu primeiro. veja INSTALAR-WINDOWS.md"
    exit 1
fi

titulo "dependencias (pede a senha do sudo)"
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates curl ffmpeg python3

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

mem_kb=$(awk '/MemTotal/{print $2}' /proc/meminfo 2>/dev/null || echo 0)
jobs=2; [ "${mem_kb:-0}" -lt 6000000 ] && jobs=1

titulo "compilando o transcritor (cpu, reserva pra video sem legenda)"
cmake -S "$raiz" -B "$raiz/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$raiz/build" -j "$jobs"

titulo "modelo whisper small (leve, so entra quando o video nao tem legenda)"
"$raiz/scripts/baixar-modelo.sh" small || aviso "download do modelo falhou, refaz depois"

titulo "configuracao do worker"
if [ ! -f "$raiz/worker/.env" ]; then
    cp "$raiz/worker/.env.example" "$raiz/worker/.env"
    aviso "abra worker/.env e cole a SUPABASE_URL e a SUPABASE_SECRET_KEY (a secret)."
else
    echo "worker/.env ja existe, mantido"
fi

titulo "claude code"
command -v claude >/dev/null 2>&1 \
    && echo "claude ok. rode 'claude' uma vez e logue na conta Max se ainda nao logou." \
    || aviso "instale o claude code e rode 'claude' pra logar. a extracao depende dele."

titulo "pronto"
echo "1) preencha worker/.env com a URL e a SECRET key"
echo "2) teste a conexao:  ./worker/testar.sh"
echo "3) rode o worker:    ./worker/rodar.sh"
