#include "importador.h"

#include <map>
#include <optional>
#include <vector>

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

}

int inserir_lotes(pqxx::work& w, long leilao_id, const std::string& csv_texto) {
    auto linhas = parse_csv(csv_texto);
    if (linhas.size() < 2) return 0;

    std::map<std::string, int> col;
    for (size_t i = 0; i < linhas[0].size(); i++) col[linhas[0][i]] = static_cast<int>(i);
    auto val = [&](const std::vector<std::string>& row, const std::string& nome) -> std::string {
        auto it = col.find(nome);
        if (it == col.end() || it->second >= static_cast<int>(row.size())) return "";
        return row[it->second];
    };

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
    return total;
}
