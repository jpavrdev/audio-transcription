#include "rbac.h"
#include <drogon/drogon.h>

using namespace drogon;

namespace {
// deixa passar se a sessao tem nivel suficiente, senao corta com o erro certo
void checar(const HttpRequestPtr& req, int minimo,
            FilterCallback& fcb, FilterChainCallback& fccb) {
    auto sessao = req->session();
    if (!sessao->find("usuario_id")) {
        Json::Value j;
        j["ok"] = false;
        j["erro"] = "nao autenticado";
        auto r = HttpResponse::newHttpJsonResponse(j);
        r->setStatusCode(k401Unauthorized);
        fcb(r);
        return;
    }
    if (sessao->get<int>("nivel") < minimo) {
        Json::Value j;
        j["ok"] = false;
        j["erro"] = "sem permissao";
        auto r = HttpResponse::newHttpJsonResponse(j);
        r->setStatusCode(k403Forbidden);
        fcb(r);
        return;
    }
    fccb();
}
}

void RequerFuncionario::doFilter(const HttpRequestPtr& req,
                                 FilterCallback&& fcb, FilterChainCallback&& fccb) {
    checar(req, 1, fcb, fccb);
}

void RequerGerente::doFilter(const HttpRequestPtr& req,
                             FilterCallback&& fcb, FilterChainCallback&& fccb) {
    checar(req, 2, fcb, fccb);
}

void RequerAdmin::doFilter(const HttpRequestPtr& req,
                           FilterCallback&& fcb, FilterChainCallback&& fccb) {
    checar(req, 3, fcb, fccb);
}
