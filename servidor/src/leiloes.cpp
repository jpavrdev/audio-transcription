#include "leiloes.h"
#include "bd.h"

#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <pqxx/pqxx>

#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace drogon;

namespace {

// parser de csv que respeita aspas duplas, "" escapada e quebras CRLF
std::vector<std::vector<std::string>> parse_csv(const std::string& t) {
    std::vector<std::vector<std::string>> linhas;
    std::vector<std::string> campos;
    std::string campo;
    bool aspas = false;
    auto fim_campo = [&] { campos.push_back(campo); campo.clear(); };
    auto fim_linha = [&] { fim_campo(); linhas.push_back(campos); campos.clear(); };
    for (size_t i = 0; i < t.size();) {
        char c = t[i];
        if (aspas) {
            if (c == '"') {
                if (i + 1 < t.size() && t[i + 1] == '"') { campo.push_back('"'); i += 2; }
                else { aspas = false; i++; }
            } else { campo.push_back(c); i++; }
        } else {
            if (c == '"') { aspas = true; i++; }
            else if (c == ',') { fim_campo(); i++; }
            else if (c == '\r') { i++; }
            else if (c == '\n') { fim_linha(); i++; }
            else { campo.push_back(c); i++; }
        }
    }
    if (!campo.empty() || !campos.empty()) fim_linha();
    return linhas;
}

std::optional<int> para_int(const std::string& s) {
    if (s.empty()) return std::nullopt;
    try { return std::stoi(s); } catch (...) { return std::nullopt; }
}

HttpResponsePtr erro(const std::string& msg, HttpStatusCode codigo) {
    Json::Value j;
    j["ok"] = false;
    j["erro"] = msg;
    auto r = HttpResponse::newHttpJsonResponse(j);
    r->setStatusCode(codigo);
    return r;
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
                    "select id, titulo, coalesce(fonte,'') fonte, "
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
                    j["data_leilao"] = row["data_leilao"].is_null() ? "" : row["data_leilao"].as<std::string>();
                    j["total_lotes"] = row["total_lotes"].as<int>();
                    j["criado_em"] = row["criado_em"].as<std::string>();
                    arr.append(j);
                }
                Json::Value out;
                out["ok"] = true;
                out["leiloes"] = arr;
                cb(HttpResponse::newHttpJsonResponse(out));
            } catch (const std::exception& e) {
                cb(erro(e.what(), k500InternalServerError));
            }
        },
        {Get, "RequerFuncionario"});

    app.registerHandler("/leiloes/{id}",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb,
           std::string id) {
            auto oid = para_int(id);
            if (!oid) { cb(erro("id invalido", k400BadRequest)); return; }
            try {
                pqxx::connection c(bd::conn_string());
                pqxx::work w(c);
                auto rl = w.exec_params(
                    "select id, titulo, coalesce(fonte,'') fonte, total_lotes, "
                    "to_char(criado_em,'YYYY-MM-DD HH24:MI') criado_em "
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
                leilao["total_lotes"] = row0["total_lotes"].as<int>();
                leilao["criado_em"] = row0["criado_em"].as<std::string>();

                Json::Value lotes(Json::arrayValue);
                for (auto row : lr) {
                    Json::Value j;
                    j["lote"] = row["lote"].as<int>();
                    j["tipo"] = row["tipo"].as<std::string>();
                    j["valor"] = row["valor"].as<std::string>();
                    j["valor_num"] = row["valor_num"].is_null() ? Json::Value()
                                                                : Json::Value(row["valor_num"].as<int>());
                    j["comprador"] = row["comprador"].as<std::string>();
                    j["fazenda"] = row["fazenda"].as<std::string>();
                    j["cidade"] = row["cidade"].as<std::string>();
                    j["estado"] = row["estado"].as<std::string>();
                    j["com_quem"] = row["com_quem"].as<std::string>();
                    j["vendedor"] = row["vendedor"].as<std::string>();
                    j["timestamp"] = row["timestamp_video"].as<std::string>();
                    j["trecho"] = row["trecho"].as<std::string>();
                    lotes.append(j);
                }
                Json::Value out;
                out["ok"] = true;
                out["leilao"] = leilao;
                out["lotes"] = lotes;
                cb(HttpResponse::newHttpJsonResponse(out));
            } catch (const std::exception& e) {
                cb(erro(e.what(), k500InternalServerError));
            }
        },
        {Get, "RequerFuncionario"});

    app.registerHandler("/leiloes/importar",
        [](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& cb) {
            MultiPartParser mp;
            if (mp.parse(req) != 0 || mp.getFiles().empty()) {
                cb(erro("envie um arquivo csv no campo 'arquivo'", k400BadRequest));
                return;
            }
            const auto& params = mp.getParameters();
            auto param = [&](const std::string& k) {
                auto it = params.find(k);
                return it == params.end() ? std::string() : it->second;
            };
            std::string titulo = param("titulo");
            std::string fonte = param("fonte");
            if (titulo.empty()) { cb(erro("informe o titulo", k400BadRequest)); return; }

            std::string conteudo(mp.getFiles()[0].fileContent());
            long importador = req->session()->get<int64_t>("usuario_id");

            try {
                auto linhas = parse_csv(conteudo);
                if (linhas.size() < 2) { cb(erro("csv sem dados", k400BadRequest)); return; }

                std::map<std::string, int> col;
                for (size_t i = 0; i < linhas[0].size(); i++) col[linhas[0][i]] = static_cast<int>(i);
                auto val = [&](const std::vector<std::string>& row, const std::string& nome) -> std::string {
                    auto it = col.find(nome);
                    if (it == col.end() || it->second >= static_cast<int>(row.size())) return "";
                    return row[it->second];
                };

                pqxx::connection c(bd::conn_string());
                pqxx::work w(c);
                auto rl = w.exec_params(
                    "insert into leiloes(titulo, fonte, importado_por) values($1,$2,$3) returning id",
                    titulo, fonte, importador);
                long leilao_id = rl[0][0].as<long>();

                int total = 0;
                for (size_t i = 1; i < linhas.size(); i++) {
                    const auto& row = linhas[i];
                    auto olote = para_int(val(row, "lote"));
                    if (!olote) continue;
                    std::optional<int> vnum = para_int(val(row, "valor_num"));
                    w.exec_params(
                        "insert into lotes(leilao_id, lote, tipo, valor, valor_num, comprador, "
                        "fazenda, cidade, estado, com_quem, vendedor, timestamp_video, trecho) "
                        "values($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13)",
                        leilao_id, *olote, val(row, "tipo"), val(row, "valor"), vnum,
                        val(row, "comprador"), val(row, "fazenda"), val(row, "cidade"),
                        val(row, "estado"), val(row, "com_quem"), val(row, "vendedor"),
                        val(row, "timestamp"), val(row, "trecho"));
                    total++;
                }
                w.exec_params("update leiloes set total_lotes=$1 where id=$2", total, leilao_id);
                w.commit();

                Json::Value out;
                out["ok"] = true;
                out["leilao_id"] = static_cast<Json::Int64>(leilao_id);
                out["lotes"] = total;
                cb(HttpResponse::newHttpJsonResponse(out));
            } catch (const std::exception& e) {
                cb(erro(e.what(), k500InternalServerError));
            }
        },
        {Post, "RequerGerente"});
}
