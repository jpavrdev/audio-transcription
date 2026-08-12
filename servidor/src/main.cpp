// servidor do painel de leiloes: esqueleto com uma rota de saude que testa o banco
#include <drogon/drogon.h>

#include <cstdlib>
#include <string>

static std::string env(const char* chave, const char* padrao) {
    const char* v = std::getenv(chave);
    return v ? std::string(v) : std::string(padrao);
}

int main() {
    auto& app = drogon::app();

    app.createDbClient(
        "postgresql",
        env("DB_HOST", "127.0.0.1"),
        static_cast<unsigned short>(std::stoi(env("DB_PORT", "5433"))),
        env("DB_NAME", "leiloes"),
        env("DB_USER", "painel"),
        env("DB_PASSWORD", "painel_dev"),
        1);

    app.registerHandler(
        "/",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setBody("painel de leiloes: servidor no ar");
            cb(resp);
        });

    app.registerHandler(
        "/saude",
        [](const drogon::HttpRequestPtr&,
           std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
            auto db = drogon::app().getDbClient();
            db->execSqlAsync(
                "SELECT 1",
                [cb](const drogon::orm::Result&) {
                    Json::Value j;
                    j["ok"] = true;
                    j["banco"] = "conectado";
                    cb(drogon::HttpResponse::newHttpJsonResponse(j));
                },
                [cb](const drogon::orm::DrogonDbException& e) {
                    Json::Value j;
                    j["ok"] = false;
                    j["erro"] = e.base().what();
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(j);
                    resp->setStatusCode(drogon::k500InternalServerError);
                    cb(resp);
                });
        });

    LOG_INFO << "servidor em http://127.0.0.1:8080";
    app.addListener("0.0.0.0", 8080).run();
    return 0;
}
