// transcritor: extrai o audio de um video com ffmpeg e transcreve com whisper.cpp
#include <whisper.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <thread>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

struct Opcoes {
    std::string entrada;
    std::string modelo = "models/ggml-small.bin";
    std::string idioma = "pt";
    std::string saida;            // base do nome de saida, sem extensao
    int threads = 0;              // 0 deixa o app escolher
    bool srt = true;
    bool txt = true;
    bool traduzir = false;
};

static void ajuda(const char* prog) {
    std::fprintf(stderr,
        "uso: %s <video> [opcoes]\n"
        "\n"
        "  -m <arquivo>   modelo ggml (padrao models/ggml-small.bin)\n"
        "  -l <idioma>    idioma da fala (padrao pt, use auto pra detectar)\n"
        "  -o <base>      nome base dos arquivos de saida\n"
        "  -t <n>         numero de threads\n"
        "  --no-srt       nao gerar legenda .srt\n"
        "  --no-txt       nao gerar arquivo .txt\n"
        "  --traduzir     traduzir a fala pra ingles\n"
        "  -h             mostra esta ajuda\n",
        prog);
}

// envolve um caminho em aspas simples pra passar pro shell com seguranca
static std::string aspas(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

static std::string aparar(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t a = s.find_first_not_of(ws);
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(ws);
    return s.substr(a, b - a + 1);
}

// os tempos do whisper vem em centesimos de segundo; a legenda quer 00:00:00,000
static std::string tempo_srt(int64_t cs) {
    int64_t ms = cs * 10;
    int64_t h = ms / 3600000; ms %= 3600000;
    int64_t m = ms / 60000;   ms %= 60000;
    int64_t s = ms / 1000;    ms %= 1000;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld,%03lld",
                  (long long)h, (long long)m, (long long)s, (long long)ms);
    return std::string(buf);
}

static bool extrair_audio(const std::string& entrada, const std::string& destino_pcm) {
    std::string cmd =
        "ffmpeg -y -loglevel error -i " + aspas(entrada) +
        " -vn -ac 1 -ar 16000 -f s16le -acodec pcm_s16le " + aspas(destino_pcm);
    int ret = std::system(cmd.c_str());
    return ret == 0;
}

static std::vector<float> ler_pcm(const std::string& caminho) {
    std::vector<float> amostras;
    std::ifstream f(caminho, std::ios::binary | std::ios::ate);
    if (!f) return amostras;
    std::streamsize bytes = f.tellg();
    f.seekg(0, std::ios::beg);
    size_t n = (size_t)(bytes / (std::streamsize)sizeof(int16_t));
    std::vector<int16_t> pcm(n);
    if (n > 0) f.read(reinterpret_cast<char*>(pcm.data()),
                      (std::streamsize)(n * sizeof(int16_t)));
    amostras.resize(n);
    for (size_t i = 0; i < n; ++i) amostras[i] = pcm[i] / 32768.0f;
    return amostras;
}

int main(int argc, char** argv) {
    Opcoes opts;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto proximo = [&](const char* nome) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "faltou o valor de %s\n", nome);
                std::exit(1);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--ajuda" || a == "--help") { ajuda(argv[0]); return 0; }
        else if (a == "-m") opts.modelo = proximo("-m");
        else if (a == "-l") opts.idioma = proximo("-l");
        else if (a == "-o") opts.saida = proximo("-o");
        else if (a == "-t") opts.threads = std::stoi(proximo("-t"));
        else if (a == "--no-srt") opts.srt = false;
        else if (a == "--no-txt") opts.txt = false;
        else if (a == "--traduzir") opts.traduzir = true;
        else if (!a.empty() && a[0] == '-') {
            std::fprintf(stderr, "opcao desconhecida: %s\n", a.c_str());
            return 1;
        }
        else opts.entrada = a;
    }

    if (opts.entrada.empty()) { ajuda(argv[0]); return 1; }

    if (!fs::exists(opts.entrada)) {
        std::fprintf(stderr, "video nao encontrado: %s\n", opts.entrada.c_str());
        return 1;
    }
    if (!fs::exists(opts.modelo)) {
        std::fprintf(stderr,
            "modelo nao encontrado: %s\n"
            "baixe com: ./scripts/baixar-modelo.sh small\n",
            opts.modelo.c_str());
        return 1;
    }

    if (opts.threads <= 0) {
        int hw = (int)std::thread::hardware_concurrency();
        if (hw <= 0) hw = 4;
        opts.threads = hw > 8 ? 8 : hw;
    }

    if (opts.saida.empty()) {
        fs::path p = opts.entrada;
        p.replace_extension();
        opts.saida = p.string();
    }

    fs::path tmp = fs::temp_directory_path() /
        ("transcritor_" + std::to_string((long)getpid()) + ".pcm");

    std::fprintf(stderr, "extraindo audio com ffmpeg...\n");
    if (!extrair_audio(opts.entrada, tmp.string())) {
        std::fprintf(stderr, "falha ao extrair o audio. o ffmpeg esta instalado?\n");
        return 1;
    }

    std::vector<float> audio = ler_pcm(tmp.string());
    std::error_code ec;
    fs::remove(tmp, ec);

    if (audio.empty()) {
        std::fprintf(stderr, "nao consegui ler o audio extraido\n");
        return 1;
    }
    std::fprintf(stderr, "audio: %.1f segundos, transcrevendo com %d threads...\n",
                 audio.size() / 16000.0, opts.threads);

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;
    whisper_context* ctx = whisper_init_from_file_with_params(opts.modelo.c_str(), cparams);
    if (!ctx) {
        std::fprintf(stderr, "falha ao carregar o modelo: %s\n", opts.modelo.c_str());
        return 1;
    }

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress   = true;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.print_special    = false;
    wparams.translate        = opts.traduzir;
    wparams.language         = opts.idioma.c_str();
    wparams.n_threads        = opts.threads;

    if (whisper_full(ctx, wparams, audio.data(), (int)audio.size()) != 0) {
        std::fprintf(stderr, "falha na transcricao\n");
        whisper_free(ctx);
        return 1;
    }

    int n = whisper_full_n_segments(ctx);
    std::string texto;
    std::string srt;
    for (int i = 0; i < n; ++i) {
        std::string t = aparar(whisper_full_get_segment_text(ctx, i));
        if (!t.empty()) {
            texto += t;
            texto += "\n";
        }
        if (opts.srt) {
            int64_t t0 = whisper_full_get_segment_t0(ctx, i);
            int64_t t1 = whisper_full_get_segment_t1(ctx, i);
            srt += std::to_string(i + 1) + "\n";
            srt += tempo_srt(t0) + " --> " + tempo_srt(t1) + "\n";
            srt += t + "\n\n";
        }
    }

    whisper_free(ctx);

    if (opts.txt) {
        std::ofstream(opts.saida + ".txt") << texto;
        std::fprintf(stderr, "gerado: %s.txt\n", opts.saida.c_str());
    }
    if (opts.srt) {
        std::ofstream(opts.saida + ".srt") << srt;
        std::fprintf(stderr, "gerado: %s.srt\n", opts.saida.c_str());
    }

    std::fputs(texto.c_str(), stdout);
    return 0;
}
