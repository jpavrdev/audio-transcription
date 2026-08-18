#pragma once
#include <pqxx/pqxx>
#include <string>

// insere os lotes de um csv num leilao ja existente. devolve quantos lotes
// foram inseridos. nao faz commit (quem chama controla a transacao).
int inserir_lotes(pqxx::work& w, long leilao_id, const std::string& csv_texto);
