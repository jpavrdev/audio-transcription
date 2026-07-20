#!/usr/bin/env python3
# normaliza a planilha de lotes (valor -> numero, nome do comprador, tipo)
# e gera uma pagina html navegavel com os horarios ligados ao video.
import csv, re, html, argparse, collections

UNID = {'zero':0,'um':1,'uma':1,'dois':2,'duas':2,'tres':3,'quatro':4,'cinco':5,
        'seis':6,'sete':7,'oito':8,'nove':9,'dez':10,'onze':11,'doze':12,'treze':13,
        'quatorze':14,'catorze':14,'quinze':15,'dezesseis':16,'dezessete':17,
        'dezoito':18,'dezenove':19}
DEZ = {'vinte':20,'trinta':30,'quarenta':40,'cinquenta':50,'sessenta':60,
       'setenta':70,'oitenta':80,'noventa':90}
CEM = {'cem':100,'cento':100,'duzentos':200,'trezentos':300,'quatrocentos':400,
       'quinhentos':500,'seiscentos':600,'setecentos':700,'oitocentos':800,'novecentos':900}

def tira_acento(s):
    return s.translate(str.maketrans('áâãàéêíóôõúüç','aaaaeeiooouuc'))

def palavras_para_num(txt):
    toks = re.findall(r'[a-zçãâáéêíóôõú]+', txt.lower())
    total = 0; atual = 0; achou = False
    for raw in toks:
        t = tira_acento(raw)
        if t in ('e','de','a','o','da','do'): continue
        if t == 'mil':
            total += (atual if atual else 1) * 1000; atual = 0; achou = True
        elif t in CEM: atual += CEM[t]; achou = True
        elif t in DEZ: atual += DEZ[t]; achou = True
        elif t in UNID: atual += UNID[t]; achou = True
    total += atual
    return total if achou and total > 0 else None

def parse_valor(v, lote=None):
    if not v: return None
    s = v.strip(); low = tira_acento(s.lower())
    m = re.search(r'\b(\d)\s*e\s*(\d{2,3})\b', low)
    if m: return int(m.group(1))*1000 + int(m.group(2))
    m = re.search(r'\b(\d{1,3}(?:\.\d{3})+)\b', s)
    if m: return int(m.group(1).replace('.',''))
    m = re.search(r'\b(\d)\.(\d)\b', s)
    if m: return int(m.group(1))*1000 + int(m.group(2))*100
    nums = [int(x) for x in re.findall(r'\b\d{3,4}\b', s) if int(x) != lote]
    if nums: return nums[-1]
    return palavras_para_num(low)

def norm_nome(c):
    if not c: return ''
    core = re.split(r'[(/]', c)[0]
    core = re.sub(r'\b(dr|dra|doutor|doutora|sr|sra|senhor|senhora|seu)\.?\s+', '', core, flags=re.I)
    return core.split(',')[0].strip().strip('-').strip()

def tipo_lote(trecho):
    return 'bateria' if 'bateria' in (trecho or '').lower() else 'lote'

def seg(ts):
    m = re.match(r'(\d+):(\d{2}):(\d{2})', ts or '')
    return int(m.group(1))*3600+int(m.group(2))*60+int(m.group(3)) if m else None

def g(r, k):
    return (r.get(k) or '').strip()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('csv')
    ap.add_argument('--out-csv', default='saidas/leilao_lotes_norm.csv')
    ap.add_argument('--out-html', default='saidas/leilao_lotes.html')
    ap.add_argument('--video-id', default=None)
    ap.add_argument('--titulo', default='Lotes do leilao')
    a = ap.parse_args()

    rows = list(csv.DictReader(open(a.csv, encoding='utf-8')))
    for r in rows:
        r['lote'] = int(r['lote'])
        r['valor_num'] = parse_valor(g(r,'valor'), r['lote'])
        r['comprador_norm'] = norm_nome(g(r,'comprador'))
        r['tipo'] = tipo_lote(g(r,'trecho'))
        r['uf'] = g(r,'estado')
    rows.sort(key=lambda r: r['lote'])

    cols = ['lote','tipo','valor_num','valor','comprador_norm','comprador','fazenda',
            'cidade','estado','com_quem','vendedor','timestamp','trecho']
    with open(a.out_csv,'w',newline='',encoding='utf-8') as fh:
        w = csv.writer(fh); w.writerow(cols)
        for r in rows:
            w.writerow([r['lote'], r['tipo'], r['valor_num'] if r['valor_num'] is not None else '',
                        g(r,'valor'), r['comprador_norm'], g(r,'comprador'), g(r,'fazenda'),
                        g(r,'cidade'), g(r,'estado'), g(r,'com_quem'), g(r,'vendedor'),
                        g(r,'timestamp'), g(r,'trecho')])

    com_valor = [r for r in rows if r['valor_num']]
    cnt = collections.Counter(r['comprador_norm'] for r in rows if r['comprador_norm'])
    top = cnt.most_common(8)

    def td(x, cls=''):
        c = f' class="{cls}"' if cls else ''
        return f'<td{c}>{html.escape(str(x or ""))}</td>'

    trs = []
    for r in rows:
        ts = g(r,'timestamp')
        s = seg(ts)
        if ts and a.video_id and s is not None:
            link = f'https://www.youtube.com/watch?v={a.video_id}&t={s}'
            tcell = f'<td data-v="{s}"><a href="{link}" target="_blank" rel="noopener">{html.escape(ts)}</a></td>'
        else:
            tcell = f'<td data-v="{s or 0}">{html.escape(ts)}</td>'
        vn = r['valor_num']
        trs.append('<tr>' +
            f'<td data-v="{r["lote"]}">{r["lote"]}</td>' +
            td(r['tipo']) +
            f'<td data-v="{vn if vn else 0}" class="num">{vn if vn else ""}</td>' +
            td(r['comprador_norm']) + td(g(r,'fazenda')) + td(g(r,'cidade')) +
            td(g(r,'estado')) + td(g(r,'com_quem')) + td(g(r,'vendedor')) +
            tcell + td(g(r,'trecho'),'trecho') + '</tr>')

    tops = ''.join(f'<li><b>{c}</b> lotes {html.escape(n)}</li>' for n,c in top)
    page = TEMPLATE.format(
        titulo=html.escape(a.titulo), n=len(rows),
        ncomp=sum(1 for r in rows if r['comprador_norm']), nval=len(com_valor),
        ncq=sum(1 for r in rows if g(r,'com_quem')), tops=tops, linhas=''.join(trs))
    open(a.out_html,'w',encoding='utf-8').write(page)

    print(f'lotes: {len(rows)} | valor num: {len(com_valor)} | comprador: {sum(1 for r in rows if r["comprador_norm"])}'
          f' | com_quem: {sum(1 for r in rows if g(r,"com_quem"))}')
    print(f'csv normalizado: {a.out_csv}')
    print(f'pagina html: {a.out_html}')

TEMPLATE = '''<!doctype html>
<html lang="pt-br"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1"><title>{titulo}</title>
<style>
:root{{color-scheme:light dark}}
body{{font-family:system-ui,Segoe UI,Roboto,sans-serif;margin:0;padding:1.2rem;background:#f6f7f9;color:#1a1a1a}}
@media(prefers-color-scheme:dark){{body{{background:#16181d;color:#e8e8e8}}}}
h1{{font-size:1.35rem;margin:.2rem 0}}
.sub{{color:#777;font-size:.88rem;margin-bottom:1rem;max-width:70ch}}
.cards{{display:flex;flex-wrap:wrap;gap:.6rem;margin-bottom:1rem}}
.card{{background:#fff;border:1px solid #e2e2e2;border-radius:10px;padding:.5rem .9rem;min-width:110px}}
@media(prefers-color-scheme:dark){{.card{{background:#1f2229;border-color:#2c2f38}}}}
.card b{{font-size:1.4rem;display:block}}.card span{{color:#888;font-size:.78rem}}
.top{{font-size:.9rem;margin:0 0 1rem;padding-left:1.1rem;columns:2;max-width:560px}}
input{{padding:.5rem .7rem;width:min(420px,100%);border:1px solid #ccc;border-radius:8px;margin-bottom:.8rem;font-size:1rem}}
.wrap{{overflow-x:auto}}
table{{border-collapse:collapse;width:100%;font-size:.84rem;background:#fff}}
@media(prefers-color-scheme:dark){{table{{background:#1c1f26}}}}
th,td{{padding:.38rem .55rem;border-bottom:1px solid #ececec;text-align:left;vertical-align:top}}
@media(prefers-color-scheme:dark){{th,td{{border-color:#2a2d35}}}}
th{{position:sticky;top:0;background:#eef0f3;cursor:pointer;white-space:nowrap;user-select:none}}
@media(prefers-color-scheme:dark){{th{{background:#242832}}}}
th:hover{{color:#0a7}}
td.num{{text-align:right;font-variant-numeric:tabular-nums}}
tr:hover td{{background:rgba(0,150,120,.06)}}
a{{color:#0a7d62;text-decoration:none}}a:hover{{text-decoration:underline}}
.trecho{{color:#888;max-width:280px}}
</style></head><body>
<h1>{titulo}</h1>
<div class="sub">Extraido da transcricao do leilao. Valores sao como cantados pelo leiloeiro (em geral R$/cabeca/parcela) e podem ter erro de reconhecimento. "Com quem" e o assessor da equipe que fechou a compra. Clique no horario para abrir o video no ponto.</div>
<div class="cards">
<div class="card"><b>{n}</b><span>lotes</span></div>
<div class="card"><b>{ncomp}</b><span>com comprador</span></div>
<div class="card"><b>{nval}</b><span>com valor</span></div>
<div class="card"><b>{ncq}</b><span>com "com quem"</span></div>
</div>
<b>Quem mais levou:</b><ul class="top">{tops}</ul>
<input id="q" placeholder="filtrar por lote, comprador, cidade, assessor...">
<div class="wrap"><table id="t"><thead><tr>
<th data-t="n">Lote</th><th>Tipo</th><th data-t="n">Valor</th><th>Comprador</th>
<th>Fazenda</th><th>Cidade</th><th>UF</th><th>Com quem</th><th>Vendedor</th>
<th data-t="n">Horario</th><th>Trecho</th>
</tr></thead><tbody>{linhas}</tbody></table></div>
<script>
const t=document.getElementById('t'),q=document.getElementById('q');
q.addEventListener('input',()=>{{const s=q.value.toLowerCase();
 for(const tr of t.tBodies[0].rows) tr.style.display=tr.textContent.toLowerCase().includes(s)?'':'none';}});
t.querySelectorAll('th').forEach((th,i)=>{{let asc=true;th.addEventListener('click',()=>{{
 const num=th.dataset.t==='n';const rows=[...t.tBodies[0].rows];
 rows.sort((x,y)=>{{const a=num?+(x.cells[i].dataset.v||0):x.cells[i].textContent.toLowerCase();
  const b=num?+(y.cells[i].dataset.v||0):y.cells[i].textContent.toLowerCase();
  return (a<b?-1:a>b?1:0)*(asc?1:-1);}});
 asc=!asc;rows.forEach(r=>t.tBodies[0].appendChild(r));}});}});
</script></body></html>'''

if __name__ == '__main__':
    main()
