#!/usr/bin/env bash
# produz o csv de lotes de um leilao a partir do link. nao fala com banco nenhum.
# tenta a legenda automatica do youtube (leve); se nao tiver, baixa o audio e usa o whisper.
# uso: processar.sh <link> <raiz_do_projeto> <pasta_trabalho>
# escreve <pasta_trabalho>/lotes.csv. sai 0 no sucesso, !=0 no erro.
set -uo pipefail

LINK="$1"; RAIZ="$2"; TRAB="$3"
TRANS="$RAIZ/build/transcritor"
MODELO="${WHISPER_MODELO:-$(ls "$RAIZ"/models/ggml-*.bin 2>/dev/null | head -1)}"
YTDLP="$HOME/.local/bin/yt-dlp"
mkdir -p "$TRAB"

# 1. legenda automatica em portugues (KB, sem baixar audio)
"$YTDLP" --no-warnings --write-auto-subs --sub-langs "pt-orig,pt,pt-BR" \
    --convert-subs srt --skip-download -o "$TRAB/legenda" "$LINK" >/dev/null 2>&1 || true
LEG=$(ls "$TRAB"/legenda*.srt 2>/dev/null | head -1)

if [ -n "$LEG" ]; then
    python3 "$RAIZ/scripts/legenda_para_texto.py" "$LEG" "$TRAB/leilao" || exit 2
else
    # sem legenda: baixa o audio e transcreve localmente
    [ -x "$TRANS" ] || { echo "sem legenda e o transcritor nao esta compilado" >&2; exit 3; }
    [ -n "$MODELO" ] || { echo "sem legenda e nenhum modelo whisper em models/" >&2; exit 4; }
    "$YTDLP" -f bestaudio --no-warnings -o "$TRAB/audio.%(ext)s" "$LINK" || exit 5
    AUDIO=$(ls "$TRAB"/audio.* 2>/dev/null | head -1)
    [ -n "$AUDIO" ] || exit 6
    "$TRANS" "$AUDIO" -m "$MODELO" -l pt -o "$TRAB/leilao" || exit 7
    rm -f "$AUDIO"
fi

# 2. extrai os lotes com o claude headless (plano Max), separando os campos
python3 "$RAIZ/scripts/extrair_ia.py" "$TRAB/leilao.txt" "$TRAB/leilao.srt" -o "$TRAB/lotes.csv" || exit 8
