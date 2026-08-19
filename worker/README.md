# Worker de extração (notebook Windows)

Peça que roda no notebook e transforma um link em lotes. O front (Vercel) e o banco (Supabase) já rodam sozinhos; o worker é quem processa.

## O que faz

Em loop:
1. procura no Supabase um leilão com status "processando"
2. marca "rodando" e pega a legenda automática do YouTube
3. extrai os lotes com o Claude (`claude -p`, plano Max)
4. grava os lotes no Supabase e marca "pronto" (ou "erro" com a mensagem)

Só faz chamadas de saída pro Supabase, então o notebook não precisa abrir porta nenhuma nem ficar exposto.

## Instalar e rodar

Passo a passo completo no `INSTALAR-WINDOWS.md` (na raiz do projeto). Resumo:

    copie worker\.env.example para worker\.env e preencha URL + SECRET
    python worker\testar.py     confirma a conexao
    python worker\worker.py     sobe o worker

## Arquivos

- `worker.py` o loop principal (nativo, roda no Windows e no Linux)
- `testar.py` checagem de conexao
- `rodar.bat` / `testar.bat` atalhos pro Windows
- `.env` suas chaves, fora do git

O whisper (vídeo sem legenda) é opcional e não vem no modo Windows nativo; a legenda cobre as lives do YouTube. Se um dia precisar, é só ter o binário do transcritor em `build/` e um modelo em `models/`, que o worker usa automaticamente.
