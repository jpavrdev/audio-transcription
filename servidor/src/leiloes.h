#pragma once
// registra as rotas de leiloes:
//   GET  /leiloes            lista (funcionario+)
//   GET  /leiloes/{id}       detalhe com os lotes (funcionario+)
//   POST /leiloes/importar   sobe um csv e grava o leilao (gerente+)
void registrar_leiloes();
