# Rodar o worker no notebook Windows

Nesta arquitetura o notebook só roda o worker de extração, nativo no Windows, sem WSL2. O front (Vercel) e o banco (Supabase) já estão no ar. O worker é quem transforma um link em lotes.

## Instalar (uma vez)

1. **Python 3** (python.org). Na instalação, marque "Add python.exe to PATH".
2. **Claude Code para Windows**. Depois abra o terminal e rode `claude` uma vez pra logar na conta Max. A extração depende disso.
3. **ffmpeg**. No terminal: `winget install Gyan.FFmpeg` (ou baixe e ponha no PATH).
4. **yt-dlp**. No terminal: `pip install -U --pre yt-dlp`.
5. **O projeto**. Baixe o repositório (no GitHub, Code > Download ZIP, ou `git clone`) e extraia numa pasta.

## Configurar

Na pasta do projeto, copie `worker\.env.example` para `worker\.env` e preencha:

    SUPABASE_URL=https://xbhdkcspmetzcnulotqm.supabase.co
    SUPABASE_SECRET_KEY=sb_secret_...

A secret key é a que você regenerou no Supabase. Ela fica só nesse arquivo, no notebook.

## Testar e rodar

Abra o terminal (cmd ou PowerShell) na pasta do projeto:

    python worker\testar.py     (tem que dizer "chave secret valida")
    python worker\worker.py     (sobe o worker; ou dois cliques em worker\rodar.bat)

Com o worker rodando, cole um link no painel do Vercel: em segundos o leilão vira "pronto" com os lotes.

Se `python` não for encontrado, tente `py` no lugar: `py worker\testar.py`.

## Deixar rodando sozinho

No Agendador de Tarefas do Windows, crie uma tarefa "Ao iniciar o computador" com a ação:

    Programa: python
    Argumentos: C:\caminho\do\projeto\worker\worker.py

Ou aponte pro `worker\rodar.bat`.

## Vídeo sem legenda

O worker usa a legenda automática do YouTube, que cobre as lives de leilão. Vídeo sem legenda cai como "erro", porque o whisper não vem no modo Windows nativo. Se virar necessidade, dá pra adicionar o whisper depois.
