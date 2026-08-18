#!/usr/bin/env python3
# extrai os lotes de um leilao chamando o claude code headless (claude -p) por pedaco.
# uso: extrair_ia.py <txt> [srt] -o <csv> [--chunk N]
# roda no plano do claude instalado na maquina. sem chave de api.
import subprocess, json, re, csv, sys, argparse, os

PROMPT = ('Este texto e um trecho de transcricao automatica de um leilao de gado Nelore '
    '(tem erros de reconhecimento de voz). Cada linha comeca com o numero original no '
    'formato "NUMERO:". Extraia os lotes vendidos como um ARRAY JSON e responda SO com o '
    'JSON, sem texto antes ou depois e sem cercas de codigo. Cada item: {"lote": int, '
    '"valor": string|null, "comprador": string|null (so o nome da pessoa ou haras), '
    '"fazenda": string|null, "cidade": string|null, "estado": string|null (UF de 2 letras), '
    '"com_quem": string|null (o assessor da equipe do leilao que fechou, padrao "foi com X"; '
    'pode ser pessoa ou firma de assessoria; NAO e patrocinador de marca), '
    '"vendedor": string|null, "linha": int, "trecho": string}. Distinga comprador de assessor. '
    'Ignore patrocinio repetido (Chevrolet, Matsuda, Ivomec, Banco Safra, Programa Leiloes, Belgo). '
    'So inclua lote com numero plausivel.')

CAMPOS = ['valor', 'comprador', 'fazenda', 'cidade', 'estado', 'com_quem', 'vendedor']

def parse_valor(v, lote):
    if not v: return None
    s = str(v).strip()
    m = re.search(r'\b(\d{1,3}(?:\.\d{3})+)\b', s)          # 1.510 -> 1510
    if m: return int(m.group(1).replace('.', ''))
    nums = [int(x) for x in re.findall(r'\b\d{3,4}\b', s) if int(x) != lote]
    if nums: return nums[-1]                                # ignora o numero do lote
    return None

def chamar_claude(texto):
    try:
        r = subprocess.run(['claude', '-p', PROMPT], input=texto,
                           capture_output=True, text=True, timeout=900)
    except Exception as e:
        print(f"  claude falhou: {e}", file=sys.stderr); return []
    out = r.stdout.strip()
    out = re.sub(r'^```(json)?', '', out).strip()
    out = re.sub(r'```$', '', out).strip()
    a, b = out.find('['), out.rfind(']')
    if a < 0 or b < 0:
        return []
    try:
        return json.loads(out[a:b + 1])
    except Exception:
        return []

def carrega_srt(path):
    idx, bloco = {}, []
    def flush(x):
        if len(x) >= 2:
            try:
                i = int(x[0]); m = re.match(r'(\d\d:\d\d:\d\d),', x[1])
                if m: idx[i] = m.group(1)
            except Exception: pass
    for line in open(path, encoding='utf-8'):
        line = line.rstrip('\n')
        if line == '': flush(bloco); bloco = []
        else: bloco.append(line)
    flush(bloco); return idx

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('txt'); ap.add_argument('srt', nargs='?')
    ap.add_argument('-o', '--out', default='lotes.csv')
    ap.add_argument('--chunk', type=int, default=4000)
    a = ap.parse_args()

    linhas = [l.rstrip('\n') for l in open(a.txt, encoding='utf-8')]
    numeradas = [f"{i + 1}:{t}" for i, t in enumerate(linhas)]
    ts = carrega_srt(a.srt) if (a.srt and os.path.exists(a.srt)) else {}

    by = {}
    n = (len(numeradas) + a.chunk - 1) // a.chunk
    for c in range(n):
        pedaco = "\n".join(numeradas[c * a.chunk:(c + 1) * a.chunk])
        print(f"pedaco {c + 1}/{n}...", file=sys.stderr)
        for r in chamar_claude(pedaco):
            try:
                lote = int(r.get('lote'))
            except (TypeError, ValueError):
                continue
            e = by.setdefault(lote, {'lote': lote, 'linha': None, 'trecho': None,
                                     **{k: None for k in CAMPOS}})
            for k in CAMPOS:
                if not e[k] and r.get(k): e[k] = str(r[k]).strip()
            ln = r.get('linha')
            if isinstance(ln, int) and (e['linha'] is None or ln < e['linha']):
                e['linha'] = ln; e['trecho'] = r.get('trecho')

    for e in by.values():
        e['timestamp'] = ts.get(e['linha'], '') if e['linha'] else ''

    rows = sorted(by.values(), key=lambda x: x['lote'])
    cols = ['lote', 'valor', 'valor_num', 'comprador', 'fazenda', 'cidade', 'estado',
            'com_quem', 'vendedor', 'timestamp', 'trecho']
    with open(a.out, 'w', newline='', encoding='utf-8') as fh:
        w = csv.writer(fh); w.writerow(cols)
        for e in rows:
            e['valor_num'] = parse_valor(e.get('valor'), e['lote'])
            w.writerow([e.get(k) if e.get(k) is not None else '' for k in cols])
    print(f"{len(rows)} lotes -> {a.out}", file=sys.stderr)

if __name__ == '__main__':
    main()
