-- status do processamento de um leilao (pronto, processando, erro) e mensagem de erro
alter table leiloes add column status text not null default 'pronto';
alter table leiloes add column erro   text;
