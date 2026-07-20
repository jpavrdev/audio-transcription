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
#include <algorithm>
#include <filesystem>
#include <unistd.h>

namespace fs = std::filesystem;

struct Opcoes {
    std::string entrada;
    std::string modelo = "models/ggml-large-v3-turbo.bin";
    std::string idioma = "pt";
    std::string saida;            // base do nome de saida, sem extensao
    int threads = 0;              // 0 deixa o app escolher
    int beam = 5;                 // <= 1 usa modo guloso
    int bloco = 1800;             // tamanho do bloco em segundos pra arquivos longos
    bool usar_gpu = true;
    bool srt = true;
    bool txt = true;
    bool traduzir = false;
};

static void ajuda(const char* prog) {
    std::fprintf(stderr,
        "uso: %s <video> [opcoes]\n"
        "\n"
        "  -m <arquivo>   modelo ggml (padrao models/ggml-large-v3-turbo.bin)\n"
        "  -l <idioma>    idioma da fala (padrao pt, use auto pra detectar)\n"
        "  -o <base>      nome base dos arquivos de saida\n"
        "  -t <n>         numero de threads\n"
        "  -b <n>         beam search (padrao 5, use 1 pra modo guloso mais rapido)\n"
        "  --bloco <s>    tamanho do bloco em segundos pra arquivos longos (padrao 1800)\n"
        "  --cpu          forcar cpu mesmo com gpu disponivel\n"
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
        else if (a == "-b") opts.beam = std::stoi(proximo("-b"));
        else if (a == "--bloco") opts.bloco = std::stoi(proximo("--bloco"));
        else if (a == "--cpu") opts.usar_gpu = false;
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
            "baixe com: ./scripts/baixar-modelo.sh large-v3-turbo\n",
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

    // o audio bruto vai pro lado da saida (mesmo disco), nao pro /tmp,
    // porque arquivos longos viram varios GB e /tmp pode ser tmpfs
    std::string pcm_path = opts.saida + "." + std::to_string((long)getpid()) + ".tmp.pcm";

    std::fprintf(stderr, "extraindo audio com ffmpeg...\n");
    if (!extrair_audio(opts.entrada, pcm_path)) {
        std::fprintf(stderr, "falha ao extrair o audio. o ffmpeg esta instalado?\n");
        return 1;
    }

    std::ifstream pcm(pcm_path, std::ios::binary | std::ios::ate);
    if (!pcm) {
        std::fprintf(stderr, "nao consegui abrir o audio extraido\n");
        return 1;
    }
    std::streamsize bytes = pcm.tellg();
    pcm.seekg(0, std::ios::beg);
    size_t total_amostras = (size_t)(bytes / (std::streamsize)sizeof(int16_t));
    if (total_amostras == 0) {
        std::fprintf(stderr, "audio vazio\n");
        return 1;
    }

    double dur_total = total_amostras / 16000.0;
    size_t janela = opts.bloco > 0 ? (size_t)opts.bloco * 16000 : total_amostras;
    size_t n_blocos = (total_amostras + janela - 1) / janela;
    bool multi = n_blocos > 1;

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = opts.usar_gpu;
    whisper_context* ctx = whisper_init_from_file_with_params(opts.modelo.c_str(), cparams);
    if (!ctx) {
        std::fprintf(stderr, "falha ao carregar o modelo: %s\n", opts.modelo.c_str());
        return 1;
    }

    whisper_sampling_strategy estrategia =
        opts.beam > 1 ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY;
    whisper_full_params wparams = whisper_full_default_params(estrategia);
    if (opts.beam > 1) wparams.beam_search.beam_size = opts.beam;
    wparams.print_progress   = true;
    wparams.print_realtime   = false;
    wparams.print_timestamps = false;
    wparams.print_special    = false;
    wparams.translate        = opts.traduzir;
    wparams.language         = opts.idioma.c_str();
    wparams.n_threads        = opts.threads;

    std::fprintf(stderr, "audio: %.1f min, %zu bloco(s), transcrevendo (beam %d, %d threads)...\n",
                 dur_total / 60.0, n_blocos, opts.beam, opts.threads);

    std::ofstream ftxt, fsrt;
    if (opts.txt) ftxt.open(opts.saida + ".txt");
    if (opts.srt) fsrt.open(opts.saida + ".srt");

    std::vector<int16_t> buf(std::min(janela, total_amostras));
    std::vector<float> audio;
    std::string saida_stdout;
    int idx = 0;
    bool erro = false;

    for (size_t b = 0; b < n_blocos; ++b) {
        size_t inicio = b * janela;
        size_t n = std::min(janela, total_amostras - inicio);
        pcm.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)(n * sizeof(int16_t)));
        audio.resize(n);
        for (size_t i = 0; i < n; ++i) audio[i] = buf[i] / 32768.0f;

        if (multi) {
            std::fprintf(stderr, "bloco %zu/%zu (a partir de %.0f min)...\n",
                         b + 1, n_blocos, (inicio / 16000.0) / 60.0);
        }

        if (whisper_full(ctx, wparams, audio.data(), (int)audio.size()) != 0) {
            std::fprintf(stderr, "falha na transcricao do bloco %zu\n", b + 1);
            erro = true;
            break;
        }

        int64_t offset_cs = (int64_t)(inicio / 16000) * 100;
        int ns = whisper_full_n_segments(ctx);
        for (int i = 0; i < ns; ++i) {
            std::string t = aparar(whisper_full_get_segment_text(ctx, i));
            if (t.empty()) continue;
            if (opts.txt) ftxt << t << "\n";
            if (!multi) { saida_stdout += t; saida_stdout += "\n"; }
            if (opts.srt) {
                int64_t t0 = whisper_full_get_segment_t0(ctx, i) + offset_cs;
                int64_t t1 = whisper_full_get_segment_t1(ctx, i) + offset_cs;
                fsrt << (++idx) << "\n"
                     << tempo_srt(t0) << " --> " << tempo_srt(t1) << "\n"
                     << t << "\n\n";
            }
        }
        if (opts.txt) ftxt.flush();
        if (opts.srt) fsrt.flush();
    }

    whisper_free(ctx);
    pcm.close();
    std::error_code ec;
    fs::remove(pcm_path, ec);

    if (erro) return 1;

    if (opts.txt) std::fprintf(stderr, "gerado: %s.txt\n", opts.saida.c_str());
    if (opts.srt) std::fprintf(stderr, "gerado: %s.srt\n", opts.saida.c_str());
    if (!multi) std::fputs(saida_stdout.c_str(), stdout);
    return 0;
}
