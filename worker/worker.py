#!/usr/bin/env python3
# worker de extracao do painel (arquitetura supabase). roda no notebook, nativo no windows.
# observa os leiloes 'processando' no supabase, roda a extracao (legenda + claude,
# whisper opcional) e grava os lotes de volta. usa a chave secret (service_role).
import os, sys, csv, json, glob, time, subprocess, urllib.request, urllib.error

AQUI = os.path.dirname(os.path.abspath(__file__))


def carregar_env(caminho):
    # le worker/.env (KEY=VALUE) pro os.environ, pra funcionar sem 'source' no windows
    if not os.path.exists(caminho):
        return
    with open(caminho, encoding="utf-8") as fh:
        for linha in fh:
            linha = linha.strip()
            if not linha or linha.startswith("#") or "=" not in linha:
                continue
            k, v = linha.split("=", 1)
            os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))


carregar_env(os.path.join(AQUI, ".env"))

URL = os.environ.get("SUPABASE_URL", "").rstrip("/")
KEY = os.environ.get("SUPABASE_SECRET_KEY", "")
RAIZ = os.environ.get("RAIZ") or os.path.dirname(AQUI)
INTERVALO = int(os.environ.get("INTERVALO", "10"))
YTDLP = os.environ.get("YTDLP", "yt-dlp")
TRAB = os.path.join(AQUI, "trabalho")


def rest(method, path, body=None, prefer=None):
    data = json.dumps(body).encode() if body is not None else None
    r = urllib.request.Request(URL + "/rest/v1/" + path, data=data, method=method)
    r.add_header("apikey", KEY)
    r.add_header("Authorization", "Bearer " + KEY)
    r.add_header("Content-Type", "application/json")
    if prefer:
        r.add_header("Prefer", prefer)
    try:
        with urllib.request.urlopen(r) as resp:
            t = resp.read().decode()
            return json.loads(t) if t else []
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"supabase {method} {path} -> {e.code}: {e.read().decode()[:300]}")


def claim():
    # pega o proximo 'processando' e marca 'rodando' de forma atomica
    fila = rest("GET", "leiloes?status=eq.processando&select=id,fonte,titulo&order=id.asc&limit=1")
    if not fila:
        return None
    lid = fila[0]["id"]
    got = rest("PATCH", f"leiloes?id=eq.{lid}&status=eq.processando",
               {"status": "rodando"}, prefer="return=representation")
    return got[0] if got else None  # vazio = outro worker pegou antes


def transcritor_bin():
    for nome in ("transcritor", "transcritor.exe"):
        p = os.path.join(RAIZ, "build", nome)
        if os.path.exists(p):
            return p
    return None


def modelo_whisper():
    ms = sorted(glob.glob(os.path.join(RAIZ, "models", "ggml-*.bin")))
    return ms[0] if ms else None


def gerar_csv(link, trab, log):
    # produz trab/lotes.csv a partir do link. levanta em caso de erro.
    py = sys.executable
    base = os.path.join(trab, "leilao")

    # 1. legenda automatica do youtube (leve, sem baixar audio)
    subprocess.run([YTDLP, "--no-warnings", "--write-auto-subs",
                    "--sub-langs", "pt-orig,pt,pt-BR", "--convert-subs", "srt",
                    "--skip-download", "-o", os.path.join(trab, "legenda"), link],
                   stdout=log, stderr=subprocess.STDOUT)
    legs = glob.glob(os.path.join(trab, "legenda*.srt"))

    if legs:
        if subprocess.run([py, os.path.join(RAIZ, "scripts", "legenda_para_texto.py"), legs[0], base],
                          stdout=log, stderr=subprocess.STDOUT).returncode != 0:
            raise RuntimeError("conversao da legenda falhou")
    else:
        # sem legenda: precisa do whisper, que e opcional neste worker
        trans, modelo = transcritor_bin(), modelo_whisper()
        if not trans or not modelo:
            raise RuntimeError("video sem legenda automatica e o whisper nao esta configurado neste worker")
        if subprocess.run([YTDLP, "-f", "bestaudio", "--no-warnings",
                           "-o", os.path.join(trab, "audio.%(ext)s"), link],
                          stdout=log, stderr=subprocess.STDOUT).returncode != 0:
            raise RuntimeError("download do audio falhou")
        audios = glob.glob(os.path.join(trab, "audio.*"))
        if not audios:
            raise RuntimeError("audio nao encontrado apos o download")
        if subprocess.run([trans, audios[0], "-m", modelo, "-l", "pt", "-o", base],
                          stdout=log, stderr=subprocess.STDOUT).returncode != 0:
            raise RuntimeError("transcricao falhou")
        try:
            os.remove(audios[0])
        except OSError:
            pass

    # 2. extrai os lotes com o claude headless
    csv_path = os.path.join(trab, "lotes.csv")
    r = subprocess.run([py, os.path.join(RAIZ, "scripts", "extrair_ia.py"),
                        base + ".txt", base + ".srt", "-o", csv_path],
                       stdout=log, stderr=subprocess.STDOUT)
    if r.returncode != 0 or not os.path.exists(csv_path):
        raise RuntimeError("extracao falhou")
    return csv_path


def num(v, lote=None):
    try:
        n = int(str(v).strip())
        return None if lote is not None and n == lote else n
    except (TypeError, ValueError):
        return None


def processar(leilao):
    lid = leilao["id"]
    link = leilao.get("fonte") or ""
    trab = os.path.join(TRAB, str(lid))
    os.makedirs(trab, exist_ok=True)
    with open(os.path.join(trab, "log.txt"), "w", encoding="utf-8") as log:
        csv_path = gerar_csv(link, trab, log)

    linhas = []
    with open(csv_path, encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            lote = num(row.get("lote"))
            linhas.append({
                "leilao_id": lid,
                "lote": lote,
                "valor": (row.get("valor") or None),
                "valor_num": num(row.get("valor_num"), lote),
                "comprador": (row.get("comprador") or None),
                "fazenda": (row.get("fazenda") or None),
                "cidade": (row.get("cidade") or None),
                "estado": (row.get("estado") or None),
                "com_quem": (row.get("com_quem") or None),
                "vendedor": (row.get("vendedor") or None),
                "timestamp_video": (row.get("timestamp") or None),
                "trecho": (row.get("trecho") or None),
            })

    rest("DELETE", f"lotes?leilao_id=eq.{lid}")  # idempotente se reprocessar
    for i in range(0, len(linhas), 500):          # em lotes pra nao estourar o payload
        rest("POST", "lotes", linhas[i:i + 500], prefer="return=minimal")
    return len(linhas)


def main():
    if not URL or not KEY:
        print("faltou SUPABASE_URL ou SUPABASE_SECRET_KEY (veja worker/.env)", file=sys.stderr)
        sys.exit(1)
    os.makedirs(TRAB, exist_ok=True)
    # recupera jobs que ficaram 'rodando' de uma execucao anterior que caiu
    try:
        rest("PATCH", "leiloes?status=eq.rodando", {"status": "processando"})
    except Exception as e:
        print("aviso: nao consegui recuperar jobs pendentes:", e, flush=True)

    print(f"worker ligado, olhando {URL} a cada {INTERVALO}s", flush=True)
    while True:
        try:
            leilao = claim()
        except Exception as e:
            print("erro ao buscar a fila:", e, flush=True)
            time.sleep(INTERVALO)
            continue
        if not leilao:
            time.sleep(INTERVALO)
            continue
        lid = leilao["id"]
        print(f"processando leilao {lid}: {leilao.get('titulo')}", flush=True)
        try:
            total = processar(leilao)
            rest("PATCH", f"leiloes?id=eq.{lid}", {"status": "pronto", "total_lotes": total, "erro": None})
            print(f"  ok, {total} lotes gravados", flush=True)
        except Exception as e:
            msg = str(e)[:500]
            try:
                rest("PATCH", f"leiloes?id=eq.{lid}", {"status": "erro", "erro": msg})
            except Exception as e2:
                print("  falha ao marcar erro:", e2, flush=True)
            print("  erro:", msg, flush=True)


if __name__ == "__main__":
    main()
