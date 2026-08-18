#include "usuarios.h"
#include "bd.h"
#include "senha.h"

#include <drogon/drogon.h>
#include <pqxx/pqxx>

#include <algorithm>
#include <cctype>
#include <string>

using namespace drogon;

namespace {

HttpResponsePtr erro(const std::string& msg, HttpStatusCode codigo) {
    Json::Value j; j["ok"] = false; j["erro"] = msg;
    auto r = HttpResponse::newHttpJsonResponse(j);
    r->setStatusCode(codigo);
    return r;
}

bool papel_valido(const std::string& p) {
    return p == "admin" || p == "gerente" || p == "funcionario";
}

std::string trim(std::string s) {
    auto nao_espaco = [](unsigned char c){ return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), nao_espaco));
    s.erase(std::find_if(s.rbegin(), s.rend(), nao_espaco).base(), s.end());
    return s;
}

void listar(std::function<void(const HttpResponsePtr&)>& cb) {
    try {
        pqxx::connection c(bd::conn_string());
        pqxx::work w(c);
        auto r = w.exec(
            "select id, nome_usuario, papel, ativo, "
            "to_char(criado_em,'YYYY-MM-DD HH24:MI') criado_em "
            "from usuarios order by nivel desc, nome_usuario");
        w.commit();
        Json::Value arr(Json::arrayValue);
        for (auto row : r) {
            Json::Value j;
            j["id"] = static_cast<Json::Int64>(row["id"].as<long>());
            j["usuario"] = row["nome_usuario"].as<std::string>();
            j["papel"] = row["papel"].as<std::string>();
            j["ativo"] = row["ativo"].as<bool>();
            j["criado_em"] = row["criado_em"].as<std::string>();
            arr.append(j);
        }
        Json::Value out; out["ok"] = true; out["usuarios"] = arr;
        cb(HttpResponse::newHttpJsonResponse(out));
    } catch (const std::exception& e) {
        cb(erro(e.what(), k500InternalServerError));
    }
}

void criar(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>& cb) {
    auto j = req->getJsonObject();
    std::string usuario = trim(j ? (*j)["usuario"].asString() : "");
    std::string senha_txt = j ? (*j)["senha"].asString() : "";
    std::string papel = j ? (*j)["papel"].asString() : "";
    if (usuario.empty()) { cb(erro("informe o nome de usuario", k400BadRequest)); return; }
    if (senha_txt.size() < 8) { cb(erro("senha muito curta (minimo 8 caracteres)", k400BadRequest)); return; }
    if (!papel_valido(papel)) { cb(erro("papel invalido", k400BadRequest)); return; }
    try {
        std::string h = senha::hash(senha_txt);
        pqxx::connection c(bd::conn_string());
        pqxx::work w(c);
        auto ex = w.exec_params("select 1 from usuarios where nome_usuario=$1", usuario);
        if (!ex.empty()) { w.commit(); cb(erro("ja existe um usuario com esse nome", k409Conflict)); return; }
        auto r = w.exec_params(
            "insert into usuarios(nome_usuario, senha_hash, papel) values($1,$2,$3) returning id",
            usuario, h, papel);
        long id = r[0][0].as<long>();
        w.commit();
        Json::Value out; out["ok"] = true; out["id"] = static_cast<Json::Int64>(id);
        cb(HttpResponse::newHttpJsonResponse(out));
    } catch (const std::exception& e) {
        cb(erro(e.what(), k500InternalServerError));
    }
}

void alterar_ativo(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>& cb, long uid) {
    auto j = req->getJsonObject();
    if (!j || !j->isMember("ativo")) { cb(erro("informe o campo ativo", k400BadRequest)); return; }
    bool ativo = (*j)["ativo"].asBool();
    long eu = req->session()->get<int64_t>("usuario_id");
    if (uid == eu && !ativo) { cb(erro("voce nao pode desativar o proprio acesso", k400BadRequest)); return; }
    try {
        pqxx::connection c(bd::conn_string());
        pqxx::work w(c);
        auto r = w.exec_params(
            "update usuarios set ativo=$1, atualizado_em=now() where id=$2 "
            "returning nome_usuario, ativo", ativo, uid);
        w.commit();
        if (r.empty()) { cb(erro("usuario nao encontrado", k404NotFound)); return; }
        Json::Value out; out["ok"] = true;
        out["usuario"] = r[0]["nome_usuario"].as<std::string>();
        out["ativo"] = r[0]["ativo"].as<bool>();
        cb(HttpResponse::newHttpJsonResponse(out));
    } catch (const std::exception& e) {
        cb(erro(e.what(), k500InternalServerError));
    }
}

}

void registrar_usuarios() {
    auto& app = drogon::app();

    app.registerHandler("/usuarios",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
            if (req->method() == Post) criar(req, cb);
            else listar(cb);
        },
        {Get, Post, "RequerAdmin"});

    app.registerHandler("/usuarios/{id}",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb, std::string id) {
            long uid = 0;
            try { uid = std::stol(id); } catch (...) { cb(erro("id invalido", k400BadRequest)); return; }
            alterar_ativo(req, cb, uid);
        },
        {Patch, "RequerAdmin"});
}
