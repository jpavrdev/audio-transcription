#!/usr/bin/env python3
# extrator LOCAL por regras: le a transcricao (.txt e opcionalmente .srt)
# e monta um csv de lotes sem usar IA. e offline e de graca, porem mais cru
# que a extracao com llm, principalmente no nome do comprador e no valor.
import csv, re, argparse

# palavras que sao patrocinio/assessoria, nunca comprador
BLOCK = {'chevrolet','silverado','matsuda','ivomec','vomec','ourofino','safra','banco',
 'programa','leiloes','leilao','leiloe','involmec','prime','remate','lance','rural',
 'belgo','arames','natus','pharma','nutricao','animal','premier','jhj','sap','dueto',
 'assessoria','opcao','porteira','mesa','tela','martelo','lote','bateria','obrigado',
 'informa','comprador','fechou','vendido','vendida','parcela','parcelas','reais',
 'foi','eu','para','pra','vamos','agora','aqui','entao','composta','compostas',
 'formada','formado','whatsapp','bom','negocio','sonhos','levou','vendi','numero',
 'apenas','ficando','preferencia','individual','individuais','bateu'}

def sa(s):
    return s.translate(str.maketrans('áâãàéêíóôõúüç','aaaaeeiooouuc'))

def carrega_srt(path):
    idx={}; b=[]
    def flush(x):
        if len(x)>=2:
            try:
                i=int(x[0]); m=re.match(r'(\d\d:\d\d:\d\d),',x[1])
                if m: idx[i]=m.group(1)
            except Exception: pass
    for line in open(path,encoding='utf-8'):
        line=line.rstrip('\n')
        if line=='': flush(b); b=[]
        else: b.append(line)
    flush(b); return idx

def nome(texto):
    # sequencia de palavras capitalizadas, parando/pulando patrocinadores
    ws = re.findall(r"[A-ZÁÉÍÓÚÂÊÔÃÕ][a-zá-úâ-ûA-Z']+", texto)
    out=[]
    for w in ws:
        if sa(w.lower()) in BLOCK:
            if out: break
            continue
        out.append(w)
        if len(out)>=4: break
    return ' '.join(out) if len(out)>=2 else ''

def valor(texto, lote):
    t=sa(texto.lower()); v=None
    m=re.search(r'(\d)\s*e\s*(\d{2,3})\b', t)
    if m: v=int(m.group(1))*1000+int(m.group(2))
    if v is None:
        m=re.search(r'\b(\d{1,3}\.\d{3})\b', texto)
        if m: v=int(m.group(1).replace('.',''))
    if v is None:
        cand=[int(x) for x in re.findall(r'\b\d{3,4}\b', texto) if int(x)!=lote and 150<=int(x)<=2000]
        v=cand[-1] if cand else None
    return v if v is not None and 150<=v<=2000 else None  # trava de faixa contra ruido

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('txt'); ap.add_argument('srt', nargs='?')
    ap.add_argument('-o','--out', default='saidas/lotes_auto.csv')
    ap.add_argument('--vendedor', default='')
    a=ap.parse_args()

    L=[l.rstrip('\n') for l in open(a.txt,encoding='utf-8')]
    ts=carrega_srt(a.srt) if a.srt else {}
    lotes={}

    for i,l in enumerate(L, start=1):
        low=sa(l.lower())
        m=re.search(r'comprador d[oa]s? (?:lote|bateria)s?\s*(?:numero\s*)?(\d{1,4})', low)
        if not m: continue
        n=int(m.group(1))
        e=lotes.setdefault(n, {'lote':n,'valor':None,'comprador':None,
                               'vendedor':a.vendedor,'linha':i,'trecho':l[:120]})
        janela=' '.join(L[i-1:i+4])
        depois=janela.split(str(n),1)[-1]
        nm=nome(depois)
        if nm and not e['comprador']: e['comprador']=nm
        v=valor(janela, n)
        if v and not e['valor']: e['valor']=v

    rows=sorted(lotes.values(), key=lambda r:r['lote'])
    with open(a.out,'w',newline='',encoding='utf-8') as fh:
        w=csv.writer(fh); w.writerow(['lote','valor','comprador','vendedor','timestamp','trecho'])
        for r in rows:
            w.writerow([r['lote'], r['valor'] or '', r['comprador'] or '',
                        r['vendedor'], ts.get(r['linha'],''), r['trecho']])

    print(f'lotes: {len(rows)} | com comprador: {sum(1 for r in rows if r["comprador"])}'
          f' | com valor: {sum(1 for r in rows if r["valor"])}')
    print(f'csv: {a.out}')

if __name__=='__main__':
    main()
