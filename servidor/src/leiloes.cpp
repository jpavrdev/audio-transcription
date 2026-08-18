#include "leiloes.h"
#include "bd.h"
#include "importador.h"

#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <pqxx/pqxx>

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <unistd.h>

using namespace drogon;
namespace fs = std::filesystem;

namespace {

std::optional<int> para_int(const std::string& s) {
    if (s.empty()) return std::nullopt;
    try { return std::stoi(s); } catch (...) { return std::nullopt; }
}

HttpResponsePtr erro(const std::string& msg, HttpStatusCode codigo) {
    Json::Value j; j["ok"] = false; j["erro"] = msg;
    auto r = HttpResponse::newHttpJsonResponse(j);
    r->setStatusCode(codigo);
    return r;
}

// envolve em aspas simples pra passar pro shell com seguranca
std::string aspas(const std::string& s) {
    std::string out = "'";
    for (char c : s) { if (c == '\'') out += "'\\''"; else out.push_back(c); }
    out.push_back('\'');
    return out;
}

// dispara o pipeline (baixar, transcrever, extrair, importar) em segundo plano
void disparar_pipeline(long id, const std::string& link) {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    fs::path raiz = (n > 0)
        ? fs::path(std::string(buf, n)).parent_path().parent_path().parent_path()
        : fs::path(".");
    fs::path script = raiz / "servidor" / "scripts" / "processar_leilao.sh";
    fs::path trab = raiz / "servidor" / "trabalho";
    fs::path log = trab / (std::to_string(id) + ".log");
    std::string cmd = "mkdir -p " + aspas(trab.string()) + "; bash " + aspas(script.string()) +
        " " + std::to_string(id) + " " + aspas(link) + " " + aspas(raiz.string()) +
        " > " + aspas(log.string()) + " 2>&1 &";
    std::system(cmd.c_str());
}

}

void registrar_leiloes() {
    auto& app = drogon::app();

    app.registerHandler("/leiloes",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb) {
            try {
                pqxx::connection c(bd::conn_string());
                pqxx::work w(c);
                auto r = w.exec(
                    "select id, titulo, coalesce(fonte,'') fonte, status, coalesce(erro,'') erro, "
                    "to_char(data_leilao,'YYYY-MM-DD') data_leilao, total_lotes, "
                    "to_char(criado_em,'YYYY-MM-DD HH24:MI') criado_em "
                    "from leiloes order by id desc");
                w.commit();
                Json::Value arr(Json::arrayValue);
                for (auto row : r) {
                    Json::Value j;
                    j["id"] = static_cast<Json::Int64>(row["id"].as<long>());
                    j["titulo"] = row["titulo"].as<std::string>();
                    j["fonte"] = row["fonte"].as<std::string>();
                    j["status"] = row["status"].as<std::string>();
                    j["erro"] = row["erro"].as<std::string>();
                    j["data_leilao"] = row["data_leilao"].is_null() ? "" : row["data_leilao"].as<std::string>();
                    j["total_lotes"] = row["total_lotes"].as<int>();
                    j["criado_em"] = row["criado_em"].as<std::string>();
                    arr.append(j);
                }
                Json::Value out; out["ok"] = true; out["leiloes"] = arr;
                cb(HttpResponse::newHttpJsonResponse(out));
            } catch (const std::exception& e) {
                cb(erro(e.what(), k500InternalServerError));
            }
        },
        {Get, "RequerFuncionario"});

    app.registerHandler("/leiloes/novo",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
            auto j = req->getJsonObject();
            std::string titulo = j ? (*j)["titulo"].asString() : "";
            std::string link = j ? (*j)["link"].asString() : "";
            if (titulo.empty() || link.empty()) { cb(erro("informe titulo e link", k400BadRequest)); return; }
            if (link.rfind("http", 0) != 0) { cb(erro("link invalido", k400BadRequest)); return; }
            long importador = req->session()->get<int64_t>("usuario_id");
            try {
                pqxx::connection c(bd::conn_string());
                pqxx::work w(c);
                auto r = w.exec_params(
                    "insert into leiloes(titulo, fonte, status, importado_por) "
                    "values($1,$2,'processando',$3) returning id",
                    titulo, link, importador);
                long id = r[0][0].as<long>();
                w.commit();
                disparar_pipeline(id, link);
                Json::Value out; out["ok"] = true; out["leilao_id"] = static_cast<Json::Int64>(id);
                cb(HttpResponse::newHttpJsonResponse(out));
            } catch (const std::exception& e) {
                cb(erro(e.what(), k500InternalServerError));
            }
        },
        {Post, "RequerGerente"});

    app.registerHandler("/leiloes/importar",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
            MultiPartParser mp;
            if (mp.parse(req) != 0 || mp.getFiles().empty()) {
                cb(erro("envie um arquivo csv no campo 'arquivo'", k400BadRequest)); return;
            }
            const auto& params = mp.getParameters();
            auto param = [&](const std::string& k) {
                auto it = params.find(k); return it == params.end() ? std::string() : it->second;
            };
            std::string titulo = param("titulo");
            std::string fonte = param("fonte");
            if (titulo.empty()) { cb(erro("informe o titulo", k400BadRequest)); return; }
            std::string conteudo(mp.getFiles()[0].fileContent());
            long importador = req->session()->get<int64_t>("usuario_id");
            try {
                pqxx::connection c(bd::conn_string());
                pqxx::work w(c);
                auto rl = w.exec_params(
                    "insert into leiloes(titulo, fonte, status, importado_por) "
                    "values($1,$2,'pronto',$3) returning id",
                    titulo, fonte, importador);
                long id = rl[0][0].as<long>();
                int total = inserir_lotes(w, id, conteudo);
                w.exec_params("update leiloes set total_lotes=$1 where id=$2", total, id);
                w.commit();
                Json::Value out; out["ok"] = true;
                out["leilao_id"] = static_cast<Json::Int64>(id);
                out["lotes"] = total;
                cb(HttpResponse::newHttpJsonResponse(out));
            } catch (const std::exception& e) {
                cb(erro(e.what(), k500InternalServerError));
            }
        },
        {Post, "RequerGerente"});

    app.registerHandler("/leiloes/{id}",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb, std::string id) {
            auto oid = para_int(id);
            if (!oid) { cb(erro("id invalido", k400BadRequest)); return; }
            try {
                pqxx::connection c(bd::conn_string());
                pqxx::work w(c);
                auto rl = w.exec_params(
                    "select id, titulo, coalesce(fonte,'') fonte, status, coalesce(erro,'') erro, "
                    "total_lotes, to_char(criado_em,'YYYY-MM-DD HH24:MI') criado_em "
                    "from leiloes where id=$1", *oid);
                if (rl.empty()) { w.commit(); cb(erro("leilao nao encontrado", k404NotFound)); return; }
                auto lr = w.exec_params(
                    "select lote, coalesce(tipo,'') tipo, coalesce(valor,'') valor, valor_num, "
                    "coalesce(comprador,'') comprador, coalesce(fazenda,'') fazenda, "
                    "coalesce(cidade,'') cidade, coalesce(estado,'') estado, "
                    "coalesce(com_quem,'') com_quem, coalesce(vendedor,'') vendedor, "
                    "coalesce(timestamp_video,'') timestamp_video, coalesce(trecho,'') trecho "
                    "from lotes where leilao_id=$1 order by lote", *oid);
                w.commit();
                auto row0 = rl[0];
                Json::Value leilao;
                leilao["id"] = static_cast<Json::Int64>(row0["id"].as<long>());
                leilao["titulo"] = row0["titulo"].as<std::string>();
                leilao["fonte"] = row0["fonte"].as<std::string>();
                leilao["status"] = row0["status"].as<std::string>();
                leilao["erro"] = row0["erro"].as<std::string>();
                leilao["total_lotes"] = row0["total_lotes"].as<int>();
                leilao["criado_em"] = row0["criado_em"].as<std::string>();
                Json::Value lotes(Json::arrayValue);
                for (auto row : lr) {
                    Json::Value x;
                    x["lote"] = row["lote"].as<int>();
                    x["tipo"] = row["tipo"].as<std::string>();
                    x["valor"] = row["valor"].as<std::string>();
                    x["valor_num"] = row["valor_num"].is_null() ? Json::Value() : Json::Value(row["valor_num"].as<int>());
                    x["comprador"] = row["comprador"].as<std::string>();
                    x["fazenda"] = row["fazenda"].as<std::string>();
                    x["cidade"] = row["cidade"].as<std::string>();
                    x["estado"] = row["estado"].as<std::string>();
                    x["com_quem"] = row["com_quem"].as<std::string>();
                    x["vendedor"] = row["vendedor"].as<std::string>();
                    x["timestamp"] = row["timestamp_video"].as<std::string>();
                    x["trecho"] = row["trecho"].as<std::string>();
                    lotes.append(x);
                }
                Json::Value out; out["ok"] = true; out["leilao"] = leilao; out["lotes"] = lotes;
                cb(HttpResponse::newHttpJsonResponse(out));
            } catch (const std::exception& e) {
                cb(erro(e.what(), k500InternalServerError));
            }
        },
        {Get, "RequerFuncionario"});
}
