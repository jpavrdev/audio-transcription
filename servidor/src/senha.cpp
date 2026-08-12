#include "senha.h"

#include <argon2.h>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {
constexpr uint32_t T_COST = 3;          // iteracoes
constexpr uint32_t M_COST = 1u << 16;   // 65536 KiB, ou seja 64 MiB
constexpr uint32_t PARALELISMO = 1;
constexpr size_t   SALT_LEN = 16;
constexpr size_t   HASH_LEN = 32;

std::vector<uint8_t> salt_aleatorio(size_t n) {
    std::vector<uint8_t> s(n);
    std::ifstream f("/dev/urandom", std::ios::binary);
    if (!f.read(reinterpret_cast<char*>(s.data()), static_cast<std::streamsize>(n)))
        throw std::runtime_error("falha ao ler /dev/urandom");
    return s;
}
}

namespace senha {

std::string hash(const std::string& senha) {
    auto salt = salt_aleatorio(SALT_LEN);
    size_t enclen = argon2_encodedlen(T_COST, M_COST, PARALELISMO,
                                      SALT_LEN, HASH_LEN, Argon2_id);
    std::vector<char> enc(enclen);
    int rc = argon2id_hash_encoded(T_COST, M_COST, PARALELISMO,
                                   senha.data(), senha.size(),
                                   salt.data(), SALT_LEN, HASH_LEN,
                                   enc.data(), enc.size());
    if (rc != ARGON2_OK)
        throw std::runtime_error(std::string("argon2: ") + argon2_error_message(rc));
    return std::string(enc.data());
}

bool confere(const std::string& hash, const std::string& senha) {
    return argon2id_verify(hash.c_str(), senha.data(), senha.size()) == ARGON2_OK;
}

}
