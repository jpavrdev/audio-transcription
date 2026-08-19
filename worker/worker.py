#!/usr/bin/env python3
# worker de extracao do painel (arquitetura supabase).
# fica de olho nos leiloes com status 'processando' no supabase, roda a extracao
# (legenda ou whisper + claude) e grava os lotes de volta, marcando 'pronto' ou 'erro'.
# usa a chave secret (service_role) pra passar por cima do RLS. roda no notebook.
import os, sys, csv, json, time, subprocess, urllib.request, urllib.error

URL = os.environ.get("SUPABASE_URL", "").rstrip("/")
KEY = os.environ.get("SUPABASE_SECRET_KEY", "")
RAIZ = os.environ.get("RAIZ") or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INTERVALO = int(os.environ.get("INTERVALO", "10"))
TRAB = os.path.join(RAIZ, "worker", "trabalho")

CAMPOS = ["lote", "valor", "valor_num", "comprador", "fazenda", "cidade",
          "estado", "com_quem", "vendedor", "timestamp", "trecho"]


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
    # pega o proximo 'processando' e marca 'rodando' de forma atomica.
    fila = rest("GET", "leiloes?status=eq.processando&select=id,fonte,titulo&order=id.asc&limit=1")
    if not fila:
        return None
    lid = fila[0]["id"]
    got = rest("PATCH", f"leiloes?id=eq.{lid}&status=eq.processando",
               {"status": "rodando"}, prefer="return=representation")
    return got[0] if got else None  # vazio = outro worker pegou antes


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
    with open(os.path.join(trab, "log.txt"), "w") as log:
        rc = subprocess.run(["bash", os.path.join(RAIZ, "worker", "processar.sh"), link, RAIZ, trab],
                            stdout=log, stderr=subprocess.STDOUT)
    csv_path = os.path.join(trab, "lotes.csv")
    if rc.returncode != 0 or not os.path.exists(csv_path):
        raise RuntimeError(f"pipeline falhou (rc={rc.returncode}), veja worker/trabalho/{lid}/log.txt")

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
