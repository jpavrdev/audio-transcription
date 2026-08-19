#!/usr/bin/env python3
# checagem rapida de conexao com o supabase antes de rodar o worker.
# confirma que a chave e a SECRET (nao a publishable) e que da pra ler as tabelas.
import os, sys, json, shutil, urllib.request, urllib.error

AQUI = os.path.dirname(os.path.abspath(__file__))


def carregar_env(caminho):
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

if not URL or not KEY:
    print("faltou SUPABASE_URL ou SUPABASE_SECRET_KEY no worker/.env")
    sys.exit(1)


def get(path):
    r = urllib.request.Request(URL + "/rest/v1/" + path)
    r.add_header("apikey", KEY)
    r.add_header("Authorization", "Bearer " + KEY)
    with urllib.request.urlopen(r) as resp:
        t = resp.read().decode()
        return json.loads(t) if t else []


try:
    perfis = get("perfis?select=usuario,papel")
    leiloes = get("leiloes?select=id,status")
except urllib.error.HTTPError as e:
    print(f"erro {e.code}: {e.read().decode()[:200]}")
    if e.code == 401:
        print("a chave esta errada. confira a SUPABASE_SECRET_KEY no worker/.env.")
    sys.exit(1)
except Exception as e:
    print("nao consegui conectar:", e)
    sys.exit(1)

if not perfis:
    print("conectou, mas nao veio nenhum perfil.")
    print("isso quase sempre e a chave PUBLISHABLE no lugar da SECRET.")
    print("use a secret (sb_secret_...) no worker/.env, so aqui no notebook.")
    sys.exit(1)

print(f"ok! chave secret valida. {len(perfis)} usuario(s), {len(leiloes)} leilao(oes).")

faltando = [t for t in ("yt-dlp", "ffmpeg", "claude") if not shutil.which(t)]
if faltando:
    print("atencao: nao encontrei no PATH:", ", ".join(faltando))
    print("instale eles (veja INSTALAR-WINDOWS.md), senao a extracao vai falhar.")
else:
    print("ferramentas ok: yt-dlp, ffmpeg e claude no PATH.")

print("pode rodar o worker com  python worker\\worker.py  (ou dois cliques no rodar.bat)")
