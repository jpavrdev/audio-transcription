# Worker de extração (notebook)

O worker é a peça que roda no notebook e faz o trabalho pesado. O front (Vercel) e o banco (Supabase) já estão no ar; o worker é quem transforma um link em lotes.

## O que ele faz

Num laço, ele:
1. procura no Supabase um leilão com status "processando"
2. marca como "rodando", pega a legenda do YouTube (ou baixa o áudio e usa o whisper se não tiver)
3. extrai os lotes com o Claude (`claude -p`, plano Max)
4. grava os lotes no Supabase e marca "pronto" (ou "erro" com a mensagem)

Ele só faz chamadas de saída pro Supabase, então o notebook não precisa ficar exposto na internet nem abrir porta nenhuma.

## Requisitos

- WSL2 com Ubuntu no notebook (passos 1 a 3 do `INSTALAR-WINDOWS.md`: instalar o WSL2, clonar o repo, instalar e logar o Claude Code)
- a **secret key** do Supabase (a `sb_secret_...`, aquela que você regenerou)

Nesta arquitetura o notebook **não** roda mais o servidor C++ (Drogon) nem o Postgres nem o Docker. Só isto aqui.

## Instalar

Dentro do Ubuntu do WSL, na pasta do projeto:

    ./worker/instalar-worker.sh

Ele instala as dependências, compila o transcritor (whisper em CPU), baixa o modelo leve e cria o `worker/.env`.

## Configurar

Abra `worker/.env` e preencha:

    SUPABASE_URL=https://xbhdkcspmetzcnulotqm.supabase.co
    SUPABASE_SECRET_KEY=sb_secret_...     # a secret, só aqui no notebook

Esse arquivo fica fora do git de propósito.

## Testar e rodar

    ./worker/testar.sh     # confirma que a chave secret conecta
    ./worker/rodar.sh      # sobe o worker (ctrl+c pra parar)

Com o worker rodando, quando alguém colar um link no painel, em alguns instantes o leilão sai de "processando" pra "pronto" com os lotes na tabela.

## Deixar rodando sozinho

O jeito simples no Windows é uma tarefa agendada que sobe o worker quando o notebook liga, chamando:

    wsl -d Ubuntu -u SEU_USUARIO_LINUX -- /home/SEU_USUARIO_LINUX/audio-transcription/worker/rodar.sh

Ajuste o usuário e o caminho.
