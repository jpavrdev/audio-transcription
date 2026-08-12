#pragma once
#include <drogon/HttpFilter.h>

// filtros de permissao por nivel minimo da sessao.
// sem sessao devolve 401, sessao com nivel abaixo do exigido devolve 403.

class RequerFuncionario : public drogon::HttpFilter<RequerFuncionario> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

class RequerGerente : public drogon::HttpFilter<RequerGerente> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

class RequerAdmin : public drogon::HttpFilter<RequerAdmin> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};
