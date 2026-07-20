# Transcritor de Vídeo

App de linha de comando que pega o áudio de um vídeo e gera a transcrição em texto e legenda `.srt`. Usa o ffmpeg pra extrair o áudio e o whisper.cpp pra transcrever, tudo local, sem nuvem.

## Dependências

- ffmpeg
- g++ e cmake
- Um modelo ggml do Whisper na pasta `models/`

No Ubuntu:

    sudo apt install ffmpeg g++ cmake git

## Compilar

    ./scripts/build.sh

Na primeira vez isso baixa e compila o whisper.cpp junto. O binário sai em `build/transcritor`.

## Baixar um modelo

    ./scripts/baixar-modelo.sh small

Opções: `base` (leve), `small` (equilibrado), `medium` (mais preciso e mais pesado).

## Usar

    ./build/transcritor video.mp4

Gera `video.txt` e `video.srt` na mesma pasta do vídeo. O texto também aparece na tela.

### Opções

    -m <arquivo>   modelo a usar (padrão models/ggml-small.bin)
    -l <idioma>    idioma da fala (padrão pt, use auto pra detectar)
    -o <base>      nome base dos arquivos de saída
    -t <n>         número de threads
    --no-srt       não gerar legenda .srt
    --no-txt       não gerar arquivo .txt
    --traduzir     traduzir a fala pra inglês
    -h             ajuda

### Exemplos

    ./build/transcritor aula.mkv
    ./build/transcritor entrevista.mp4 -o entrevista_final
    ./build/transcritor palestra.mp4 -l auto

## Como funciona

    vídeo -> ffmpeg -> WAV 16kHz mono -> whisper.cpp -> texto e .srt

Rodando em CPU por enquanto. Dá pra ligar a GPU (CUDA) depois pra ganhar velocidade.
