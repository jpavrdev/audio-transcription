-- schema inicial do painel de leiloes: usuarios, leiloes, lotes

create table usuarios (
    id            bigint generated always as identity primary key,
    nome_usuario  text not null unique,
    senha_hash    text not null,
    papel         text not null check (papel in ('admin', 'gerente', 'funcionario')),
    nivel         smallint generated always as
                    (case papel when 'admin' then 3 when 'gerente' then 2 else 1 end) stored,
    ativo         boolean not null default true,
    criado_em     timestamptz not null default now(),
    atualizado_em timestamptz not null default now()
);

create table leiloes (
    id            bigint generated always as identity primary key,
    titulo        text not null,
    fonte         text,
    data_leilao   date,
    data_extracao timestamptz,
    total_lotes   integer not null default 0,
    importado_por bigint references usuarios(id),
    criado_em     timestamptz not null default now()
);

create table lotes (
    id              bigint generated always as identity primary key,
    leilao_id       bigint not null references leiloes(id) on delete cascade,
    lote            integer not null,
    tipo            text,
    valor           text,
    valor_num       integer,
    comprador       text,
    fazenda         text,
    cidade          text,
    estado          text,
    com_quem        text,
    vendedor        text,
    timestamp_video text,
    trecho          text
);

create index idx_lotes_leilao on lotes(leilao_id);
