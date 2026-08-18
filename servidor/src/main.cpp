// servidor do painel de leiloes.
// subcomandos:
//   servidor servir          sobe o servidor web (padrao)
//   servidor migrar          aplica as migrations de servidor/migracoes
//   servidor criar-admin U   cria ou atualiza o admin U, lendo a senha do stdin
#include <drogon/drogon.h>
#include <pqxx/pqxx>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <climits>
#include <unistd.h>

#include "bd.h"
#include "senha.h"
#include "auth.h"
#include "leiloes.h"
#include "importador.h"

namespace fs = std::filesystem;

static std::string env(const char* chave, const char* padrao) {
    const char* v = std::getenv(chave);
    return v ? std::string(v) : std::string(padrao);
}

// pasta das migrations relativa ao binario (servidor/build -> servidor/migracoes)
static fs::path dir_migracoes() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        fs::path cand = fs::path(std::string(buf, n)).parent_path().parent_path() / "migracoes";
        if (fs::exists(cand)) return cand;
    }
    return fs::path(env("MIGRACOES_DIR", "migracoes"));
}

static int migrar() {
    try {
        pqxx::connection c(bd::conn_string());
        {
            pqxx::work w(c);
            w.exec("create table if not exists migracoes ("
                   "nome text primary key, aplicada_em timestamptz not null default now())");
            w.commit();
        }
        fs::path dir = dir_migracoes();
        std::vector<fs::path> arquivos;
        for (auto& e : fs::directory_iterator(dir))
            if (e.path().extension() == ".sql") arquivos.push_back(e.path());
        std::sort(arquivos.begin(), arquivos.end());

        int novas = 0;
        for (auto& p : arquivos) {
            std::string nome = p.filename().string();
            {
                pqxx::work w(c);
                auto r = w.exec_params("select 1 from migracoes where nome=$1", nome);
                w.commit();
                if (!r.empty()) { std::cout << "ja aplicada: " << nome << "\n"; continue; }
            }
            std::ifstream f(p);
            std::string sql((std::istreambuf_iterator<char>(f)), {});
            pqxx::work w(c);
            w.exec(sql);
            w.exec_params("insert into migracoes(nome) values($1)", nome);
            w.commit();
            std::cout << "aplicada: " << nome << "\n";
            novas++;
        }
        std::cout << novas << " migracao(oes) nova(s). pasta: " << dir << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "erro na migracao: " << e.what() << "\n";
        return 1;
    }
}

static int criar_usuario(const std::string& usuario, const std::string& papel) {
    if (usuario.empty()) { std::cerr << "informe o nome de usuario\n"; return 1; }
    if (papel != "admin" && papel != "gerente" && papel != "funcionario") {
        std::cerr << "papel invalido (use admin, gerente ou funcionario)\n";
        return 1;
    }
    std::string s;
    std::getline(std::cin, s);
    if (s.size() < 8) { std::cerr << "senha muito curta (minimo 8 caracteres)\n"; return 1; }
    try {
        std::string h = senha::hash(s);
        pqxx::connection c(bd::conn_string());
        pqxx::work w(c);
        w.exec_params(
            "insert into usuarios(nome_usuario, senha_hash, papel) values($1,$2,$3) "
            "on conflict (nome_usuario) do update set senha_hash=excluded.senha_hash, "
            "papel=excluded.papel, ativo=true, atualizado_em=now()",
            usuario, h, papel);
        w.commit();
        std::cout << "usuario '" << usuario << "' (" << papel << ") criado ou atualizado\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "erro ao criar usuario: " << e.what() << "\n";
        return 1;
    }
}

static int criar_admin(const std::string& usuario) {
    return criar_usuario(usuario, "admin");
}

static int importar_csv(long id, const std::string& arquivo) {
    try {
        std::ifstream f(arquivo, std::ios::binary);
        if (!f) { std::cerr << "arquivo nao encontrado: " << arquivo << "\n"; return 1; }
        std::string csv((std::istreambuf_iterator<char>(f)), {});
        pqxx::connection c(bd::conn_string());
        pqxx::work w(c);
        int total = inserir_lotes(w, id, csv);
        w.exec_params("update leiloes set total_lotes=$1, status='pronto', erro=null where id=$2",
                      total, id);
        w.commit();
        std::cout << "importados " << total << " lotes no leilao " << id << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "erro ao importar: " << e.what() << "\n";
        return 1;
    }
}

static int marcar_erro(long id, const std::string& msg) {
    try {
        pqxx::connection c(bd::conn_string());
        pqxx::work w(c);
        w.exec_params("update leiloes set status='erro', erro=$1 where id=$2", msg, id);
        w.commit();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "erro: " << e.what() << "\n";
        return 1;
    }
}

static int servir() {
    auto& app = drogon::app();
    app.createDbClient(
        "postgresql",
        env("DB_HOST", "127.0.0.1"),
        static_cast<unsigned short>(std::stoi(env("DB_PORT", "55432"))),
        env("DB_NAME", "leiloes"),
        env("DB_USER", "painel"),
        env("DB_PASSWORD", "painel_dev"),
        1);

    app.setThreadNum(4);      // threads pra nao travar tudo no acesso sincrono ao banco
    app.enableSession(3600);  // sessao de 1 hora por cookie
    registrar_auth();
    registrar_leiloes();

    // serve as paginas web da pasta www (ao lado do binario)
    char exe[4096];
    ssize_t nexe = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    fs::path www = (nexe > 0)
        ? fs::path(std::string(exe, nexe)).parent_path().parent_path() / "www"
        : fs::path("www");
    app.setDocumentRoot(www.string());
    app.registerHandler("/",
        [www](const drogon::HttpRequestPtr&,
              std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            cb(drogon::HttpResponse::newFileResponse((www / "index.html").string()));
        });

    app.registerHandler("/saude",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto db = drogon::app().getDbClient();
            db->execSqlAsync("SELECT 1",
                [cb](const drogon::orm::Result&) {
                    Json::Value j; j["ok"] = true; j["banco"] = "conectado";
                    cb(drogon::HttpResponse::newHttpJsonResponse(j));
                },
                [cb](const drogon::orm::DrogonDbException& e) {
                    Json::Value j; j["ok"] = false; j["erro"] = e.base().what();
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
                    resp->setStatusCode(drogon::k500InternalServerError);
                    cb(resp);
                });
        });

    LOG_INFO << "servidor em http://127.0.0.1:8080";
    app.addListener("0.0.0.0", 8080).run();
    return 0;
}

int main(int argc, char** argv) {
    std::string cmd = argc > 1 ? argv[1] : "servir";
    if (cmd == "servir")        return servir();
    if (cmd == "migrar")        return migrar();
    if (cmd == "criar-admin")   return criar_admin(argc > 2 ? argv[2] : "");
    if (cmd == "criar-usuario") return criar_usuario(argc > 2 ? argv[2] : "", argc > 3 ? argv[3] : "");
    if (cmd == "importar-csv")  return importar_csv(argc > 2 ? std::stol(argv[2]) : 0, argc > 3 ? argv[3] : "");
    if (cmd == "marcar-erro")   return marcar_erro(argc > 2 ? std::stol(argv[2]) : 0, argc > 3 ? argv[3] : "");
    std::cerr << "uso: " << argv[0]
              << " [servir|migrar|criar-admin <u>|criar-usuario <u> <papel>"
              << "|importar-csv <id> <arquivo>|marcar-erro <id> <msg>]\n";
    return 1;
}
