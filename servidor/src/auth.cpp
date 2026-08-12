#include "auth.h"
#include "senha.h"

#include <drogon/drogon.h>

using namespace drogon;

namespace {
// hash descartavel usado pra gastar um tempo parecido quando o usuario nao
// existe, evitando que o tempo de resposta revele se o login existe ou nao
const std::string& dummy_hash() {
    static const std::string h = senha::hash("timing-dummy-nao-usar");
    return h;
}
}

void registrar_auth() {
    auto& app = drogon::app();

    app.registerHandler("/login",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& cb) {
            std::string usuario, senha_txt;
            if (auto j = req->getJsonObject()) {
                usuario = (*j)["usuario"].asString();
                senha_txt = (*j)["senha"].asString();
            } else {
                usuario = req->getParameter("usuario");
                senha_txt = req->getParameter("senha");
            }

            auto erro = [cb]() {
                Json::Value j;
                j["ok"] = false;
                j["erro"] = "usuario ou senha invalidos";
                auto r = HttpResponse::newHttpJsonResponse(j);
                r->setStatusCode(k401Unauthorized);
                cb(r);
            };

            if (usuario.empty() || senha_txt.empty()) { erro(); return; }

            auto sessao = req->session();
            auto db = drogon::app().getDbClient();
            db->execSqlAsync(
                "select id, nome_usuario, senha_hash, papel, nivel, ativo "
                "from usuarios where nome_usuario=$1",
                [cb, senha_txt, sessao, erro](const orm::Result& r) {
                    if (r.size() != 1) {
                        senha::confere(dummy_hash(), senha_txt);  // gasta tempo igual
                        erro();
                        return;
                    }
                    const auto& row = r[0];
                    bool ativo = row["ativo"].as<bool>();
                    std::string hash = row["senha_hash"].as<std::string>();
                    if (!ativo || !senha::confere(hash, senha_txt)) { erro(); return; }

                    sessao->insert("usuario_id", row["id"].as<int64_t>());
                    sessao->insert("nome_usuario", row["nome_usuario"].as<std::string>());
                    sessao->insert("papel", row["papel"].as<std::string>());
                    sessao->insert("nivel", static_cast<int>(row["nivel"].as<int>()));

                    Json::Value j;
                    j["ok"] = true;
                    j["usuario"] = row["nome_usuario"].as<std::string>();
                    j["papel"] = row["papel"].as<std::string>();
                    cb(HttpResponse::newHttpJsonResponse(j));
                },
                [erro, senha_txt](const orm::DrogonDbException&) {
                    senha::confere(dummy_hash(), senha_txt);
                    erro();
                },
                usuario);
        },
        {Post});

    app.registerHandler("/logout",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& cb) {
            req->session()->clear();
            Json::Value j;
            j["ok"] = true;
            cb(HttpResponse::newHttpJsonResponse(j));
        },
        {Post});

    app.registerHandler("/eu",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& cb) {
            auto sessao = req->session();
            if (!sessao->find("usuario_id")) {
                Json::Value j;
                j["ok"] = false;
                j["erro"] = "nao autenticado";
                auto r = HttpResponse::newHttpJsonResponse(j);
                r->setStatusCode(k401Unauthorized);
                cb(r);
                return;
            }
            Json::Value j;
            j["ok"] = true;
            j["usuario"] = sessao->get<std::string>("nome_usuario");
            j["papel"] = sessao->get<std::string>("papel");
            j["nivel"] = sessao->get<int>("nivel");
            cb(HttpResponse::newHttpJsonResponse(j));
        },
        {Get});
}
