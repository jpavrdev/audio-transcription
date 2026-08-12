#include "bd.h"
#include <cstdlib>

static std::string env(const char* chave, const char* padrao) {
    const char* v = std::getenv(chave);
    return v ? std::string(v) : std::string(padrao);
}

namespace bd {
std::string conn_string() {
    return "host=" + env("DB_HOST", "127.0.0.1") +
           " port=" + env("DB_PORT", "55432") +
           " dbname=" + env("DB_NAME", "leiloes") +
           " user=" + env("DB_USER", "painel") +
           " password=" + env("DB_PASSWORD", "painel_dev");
}
}
