# Front do painel (Vercel)

Página única que fala direto com o Supabase. Sem build, é só estático.

## Configuração

As chaves públicas já estão no topo do `index.html` (`SUPABASE_URL` e `SUPABASE_KEY`, a publishable). Se um dia trocar de projeto Supabase, é só editar essas duas linhas.

## Publicar no Vercel

Jeito mais simples, sem instalar nada:
1. Entre em vercel.com e crie a conta.
2. Add New, Project, importe o repositório do GitHub.
3. Em Root Directory escolha `web`.
4. Framework Preset: Other. Sem build command.
5. Deploy. Ele te dá uma URL tipo `painel-leiloes.vercel.app`.

Por linha de comando (precisa logar):

    npm i -g vercel
    cd web
    vercel

## Como funciona

- Login por nome de usuário. Por baixo vira `usuario@mamede.local` no Supabase Auth.
- Leilões e lotes vêm do Supabase, com o RLS controlando quem vê o quê.
- Novo leilão só cria a linha com status "processando". Quem processa é o worker no notebook (M3).
- A aba Usuários lista, ativa e desativa. Criar usuário novo depende da function do M4.
