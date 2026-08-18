#!/usr/bin/env bash
# pipeline de um leilao a partir de um link.
# tenta a legenda automatica do youtube (bem leve, sem baixar audio).
# se o video nao tiver legenda em pt, cai no audio + whisper.
# uso: processar_leilao.sh <leilao_id> <link> <raiz_do_projeto>
# o servidor chama este script em segundo plano. o status vai pro banco.
set -uo pipefail

ID="$1"; LINK="$2"; RAIZ="$3"
SERV="$RAIZ/servidor/build/servidor"
TRANS="$RAIZ/build/transcritor"
MODELO="$RAIZ/models/ggml-large-v3-turbo.bin"
YTDLP="$HOME/.local/bin/yt-dlp"
TRAB="$RAIZ/servidor/trabalho/$ID"
mkdir -p "$TRAB"

falhar() { "$SERV" marcar-erro "$ID" "$1" >/dev/null 2>&1; exit 1; }

# 1. tenta a legenda automatica em portugues (KB, sem baixar audio)
"$YTDLP" --no-warnings --write-auto-subs --sub-langs "pt-orig,pt,pt-BR" \
    --convert-subs srt --skip-download -o "$TRAB/legenda" "$LINK" >/dev/null 2>&1 || true
LEG=$(ls "$TRAB"/legenda*.srt 2>/dev/null | head -1)

if [ -n "$LEG" ]; then
    # caminho leve: legenda -> texto limpo
    python3 "$RAIZ/scripts/legenda_para_texto.py" "$LEG" "$TRAB/leilao" || falhar "conversao da legenda falhou"
else
    # sem legenda: baixa o audio e transcreve na gpu
    "$YTDLP" -f bestaudio --no-warnings -o "$TRAB/audio.%(ext)s" "$LINK" || falhar "download do audio falhou"
    AUDIO=$(ls "$TRAB"/audio.* 2>/dev/null | head -1)
    [ -n "$AUDIO" ] || falhar "audio nao encontrado apos o download"
    "$TRANS" "$AUDIO" -m "$MODELO" -l pt -o "$TRAB/leilao" || falhar "transcricao falhou"
    rm -f "$AUDIO"
fi

# 2. extrai os lotes com o claude headless (plano), separando os campos
python3 "$RAIZ/scripts/extrair_ia.py" "$TRAB/leilao.txt" "$TRAB/leilao.srt" -o "$TRAB/lotes.csv" || falhar "extracao falhou"

# 3. importa no banco e marca como pronto
"$SERV" importar-csv "$ID" "$TRAB/lotes.csv" || falhar "import falhou"
