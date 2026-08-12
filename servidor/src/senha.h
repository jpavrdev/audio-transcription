#pragma once
#include <string>

namespace senha {
// gera o hash argon2id (string no formato PHC) de uma senha
std::string hash(const std::string& senha);
// confere uma senha contra um hash PHC
bool confere(const std::string& hash, const std::string& senha);
}
