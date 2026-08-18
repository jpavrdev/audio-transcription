# Base no Supabase (M1)

Passo a passo pra criar o banco e o Auth do painel no Supabase. Feito uma vez.

## 1. Criar o projeto

1. Entre em supabase.com e crie uma conta (grátis).
2. New project. Dê um nome (ex: painel-leiloes), escolha a região mais perto (São Paulo se aparecer) e defina a senha do banco. Guarde essa senha.
3. Espere o projeto subir (uns 2 minutos).

## 2. Desligar a confirmação de e-mail

Como a gente usa nome de usuário (com um e-mail sintético por baixo, tipo `joao@mamede.local`), não dá pra confirmar e-mail de verdade.

- Authentication > Sign In / Providers (ou Email) > desligue "Confirm email".

## 3. Criar as tabelas e as regras

- Abra o SQL Editor, cole todo o conteúdo de `schema.sql` e rode.
- Ele cria as tabelas perfis, leiloes e lotes, liga o RLS e define o acesso por papel.

## 4. Criar o primeiro admin

1. Authentication > Users > Add user. Use um e-mail como `admin@mamede.local` e uma senha. (Marque como já confirmado se pedir.)
2. Isso dispara um gatilho que cria o perfil automaticamente como funcionário. Promova pra admin no SQL Editor:

        update public.perfis set papel = 'admin' where usuario = 'admin';

## 5. Pegar as chaves

Em Project Settings > API:

- **Project URL** e **anon key**: são públicas, vão no front (Vercel). Pode me passar essas duas.
- **service_role key**: é secreta, passa por cima de todas as regras. Ela vai só no worker do notebook, num arquivo local fora do git. Nunca ponha no front, no repositório, nem cole no chat.

Depois de rodar isso, me mande a Project URL e a anon key que eu ligo o front no próximo passo.
