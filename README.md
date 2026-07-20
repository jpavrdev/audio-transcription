# Transcritor de Vídeo

App de linha de comando que pega o áudio de um vídeo e gera a transcrição em texto e legenda `.srt`. Usa o ffmpeg pra extrair o áudio e o whisper.cpp pra transcrever, tudo local, sem nuvem.

## Dependências

- ffmpeg
- g++ e cmake
- Um modelo ggml do Whisper na pasta `models/`

No Ubuntu:

    sudo apt install ffmpeg g++ cmake git

Opcional, pra rodar na GPU NVIDIA (bem mais rápido, permite usar modelos maiores):

    sudo apt install nvidia-cuda-toolkit g++-12

## Compilar

    ./scripts/build.sh

Na primeira vez isso baixa e compila o whisper.cpp junto. Se encontrar o `nvcc`, compila com suporte a GPU automaticamente. O binário sai em `build/transcritor`.

## Baixar um modelo

    ./scripts/baixar-modelo.sh large-v3-turbo

Opções: `base` e `small` (leves, pra CPU), `medium` e `large-v3-turbo` (mais precisos, melhor com GPU), `large-v3` (máxima precisão).

## Usar

    ./build/transcritor video.mp4

Gera `video.txt` e `video.srt` na mesma pasta do vídeo. O texto também aparece na tela. Se tiver GPU compilada, ele usa sozinho.

### Opções

    -m <arquivo>   modelo a usar (padrão models/ggml-large-v3-turbo.bin)
    -l <idioma>    idioma da fala (padrão pt, use auto pra detectar)
    -o <base>      nome base dos arquivos de saída
    -t <n>         número de threads
    -b <n>         beam search (padrão 5, use 1 pra modo guloso mais rápido)
    --cpu          forçar CPU mesmo com GPU disponível
    --no-srt       não gerar legenda .srt
    --no-txt       não gerar arquivo .txt
    --traduzir     traduzir a fala pra inglês
    -h             ajuda

### Exemplos

    ./build/transcritor aula.mkv
    ./build/transcritor entrevista.mp4 -o entrevista_final
    ./build/transcritor palestra.mp4 --cpu -b 1

## Como funciona

    vídeo -> ffmpeg -> WAV 16kHz mono -> whisper.cpp -> texto e .srt

A precisão vem do tamanho do modelo. A GPU não muda o texto, ela deixa rápido o suficiente pra usar um modelo grande sem sofrer.
