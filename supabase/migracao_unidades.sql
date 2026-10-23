-- Migracao: isolamento por unidade (campinas, jaguariuna, londrina).
-- Rode uma vez no SQL Editor do Supabase. Idempotente (pode rodar de novo sem quebrar).

-- 1. coluna unidade nos perfis + backfill
alter table public.perfis add column if not exists unidade text;
update public.perfis set unidade = 'jaguariuna' where usuario = 'jaguariuna';
update public.perfis set unidade = 'londrina'   where usuario = 'londrina';
update public.perfis set unidade = 'campinas'   where unidade is null;
alter table public.perfis alter column unidade set not null;
alter table public.perfis drop constraint if exists perfis_unidade_chk;
alter table public.perfis add constraint perfis_unidade_chk
  check (unidade in ('campinas','jaguariuna','londrina'));

-- 2. funcao: unidade do usuario logado (security definer, sem recursao de RLS)
create or replace function public.minha_unidade() returns text
  language sql stable security definer set search_path = public as $$
  select unidade from public.perfis where id = auth.uid() and ativo
$$;

-- 3. coluna unidade nos leiloes. default = unidade de quem cria (os antigos viram campinas)
alter table public.leiloes add column if not exists unidade text;
update public.leiloes set unidade = 'campinas' where unidade is null;
alter table public.leiloes alter column unidade set default public.minha_unidade();
alter table public.leiloes alter column unidade set not null;
alter table public.leiloes drop constraint if exists leiloes_unidade_chk;
alter table public.leiloes add constraint leiloes_unidade_chk
  check (unidade in ('campinas','jaguariuna','londrina'));

-- 4. gatilho: ao criar usuario, grava papel e unidade dos metadados (default campinas)
create or replace function public.ao_criar_usuario() returns trigger
  language plpgsql security definer set search_path = public as $$
begin
  insert into public.perfis(id, usuario, papel, unidade)
  values (new.id, split_part(new.email, '@', 1),
          coalesce(new.raw_user_meta_data->>'papel', 'funcionario'),
          coalesce(new.raw_user_meta_data->>'unidade', 'campinas'))
  on conflict (id) do nothing;
  return new;
end $$;

-- 5. RLS reescrito, agora isolado por unidade
drop policy if exists perfis_select on public.perfis;
create policy perfis_select on public.perfis for select
  using (id = auth.uid() or (public.sou_admin() and unidade = public.minha_unidade()));

drop policy if exists perfis_update on public.perfis;
create policy perfis_update on public.perfis for update
  using (public.sou_admin() and unidade = public.minha_unidade())
  with check (public.sou_admin() and unidade = public.minha_unidade()
              and not (id = auth.uid() and ativo = false));

drop policy if exists leiloes_select on public.leiloes;
create policy leiloes_select on public.leiloes for select
  using (public.meu_nivel() >= 1 and unidade = public.minha_unidade());

drop policy if exists leiloes_insert on public.leiloes;
create policy leiloes_insert on public.leiloes for insert
  with check (public.meu_nivel() >= 2 and importado_por = auth.uid()
              and unidade = public.minha_unidade());

drop policy if exists leiloes_retentar on public.leiloes;
create policy leiloes_retentar on public.leiloes for update
  using (public.meu_nivel() >= 2 and status = 'erro' and unidade = public.minha_unidade())
  with check (public.meu_nivel() >= 2 and status = 'processando' and unidade = public.minha_unidade());

drop policy if exists lotes_select on public.lotes;
create policy lotes_select on public.lotes for select
  using (public.meu_nivel() >= 1
         and exists (select 1 from public.leiloes l
                     where l.id = lotes.leilao_id and l.unidade = public.minha_unidade()));
