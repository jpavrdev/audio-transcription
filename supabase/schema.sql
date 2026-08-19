-- Painel de Leiloes no Supabase.
-- Cole tudo isto no SQL Editor do seu projeto Supabase e rode uma vez.
-- Cria as tabelas, liga o RLS e define o acesso por papel.

-- perfis: um por usuario do Auth. guarda o nome de usuario, o papel e se tem acesso.
create table if not exists public.perfis (
  id uuid primary key references auth.users(id) on delete cascade,
  usuario text unique not null,
  papel text not null default 'funcionario' check (papel in ('admin','gerente','funcionario')),
  nivel int generated always as (case papel when 'admin' then 3 when 'gerente' then 2 else 1 end) stored,
  ativo boolean not null default true,
  criado_em timestamptz not null default now()
);

create table if not exists public.leiloes (
  id bigint generated always as identity primary key,
  titulo text not null,
  fonte text,
  status text not null default 'processando' check (status in ('processando','rodando','pronto','erro')),
  erro text,
  total_lotes int not null default 0,
  data_leilao date,
  importado_por uuid references auth.users(id) on delete set null,
  criado_em timestamptz not null default now()
);

create table if not exists public.lotes (
  id bigint generated always as identity primary key,
  leilao_id bigint not null references public.leiloes(id) on delete cascade,
  lote int,
  tipo text,
  valor text,
  valor_num int,
  comprador text,
  fazenda text,
  cidade text,
  estado text,
  com_quem text,
  vendedor text,
  timestamp_video text,
  trecho text
);
create index if not exists idx_lotes_leilao on public.lotes(leilao_id);

-- funcoes auxiliares. security definer pra ler o perfil do chamador sem cair em
-- recursao de RLS (a policy de perfis nao pode consultar perfis diretamente).
create or replace function public.meu_nivel() returns int
  language sql stable security definer set search_path = public as $$
  select coalesce((select nivel from public.perfis where id = auth.uid() and ativo), 0)
$$;

create or replace function public.sou_admin() returns boolean
  language sql stable security definer set search_path = public as $$
  select exists(select 1 from public.perfis where id = auth.uid() and ativo and papel = 'admin')
$$;

-- quando um usuario e criado no Auth (pelo painel ou pelo dashboard), cria o perfil.
-- o papel pode vir nos metadados; senao entra como funcionario.
create or replace function public.ao_criar_usuario() returns trigger
  language plpgsql security definer set search_path = public as $$
begin
  insert into public.perfis(id, usuario, papel)
  values (new.id,
          split_part(new.email, '@', 1),
          coalesce(new.raw_user_meta_data->>'papel', 'funcionario'))
  on conflict (id) do nothing;
  return new;
end $$;

drop trigger if exists trg_ao_criar_usuario on auth.users;
create trigger trg_ao_criar_usuario after insert on auth.users
  for each row execute function public.ao_criar_usuario();

-- liga o RLS em tudo
alter table public.perfis  enable row level security;
alter table public.leiloes enable row level security;
alter table public.lotes   enable row level security;

-- perfis: cada um ve o seu, admin ve todos e edita. admin nao pode se desativar.
drop policy if exists perfis_select on public.perfis;
create policy perfis_select on public.perfis for select
  using (id = auth.uid() or public.sou_admin());

drop policy if exists perfis_update on public.perfis;
create policy perfis_update on public.perfis for update
  using (public.sou_admin())
  with check (public.sou_admin() and not (id = auth.uid() and ativo = false));

-- leiloes: qualquer usuario ativo ve. gerente ou admin cria (marcando a si mesmo).
drop policy if exists leiloes_select on public.leiloes;
create policy leiloes_select on public.leiloes for select
  using (public.meu_nivel() >= 1);

drop policy if exists leiloes_insert on public.leiloes;
create policy leiloes_insert on public.leiloes for insert
  with check (public.meu_nivel() >= 2 and importado_por = auth.uid());

-- gerente+ pode retentar um leilao que deu erro: so a transicao erro -> processando
drop policy if exists leiloes_retentar on public.leiloes;
create policy leiloes_retentar on public.leiloes for update
  using (public.meu_nivel() >= 2 and status = 'erro')
  with check (public.meu_nivel() >= 2 and status = 'processando');

-- lotes: qualquer usuario ativo ve. quem grava e o worker, que usa a chave de
-- servico e passa por cima do RLS, entao nao precisa de policy de escrita aqui.
drop policy if exists lotes_select on public.lotes;
create policy lotes_select on public.lotes for select
  using (public.meu_nivel() >= 1);
