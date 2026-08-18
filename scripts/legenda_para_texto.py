#!/usr/bin/env python3
# converte a legenda automatica do youtube (srt "rolante", que repete a linha
# anterior somando palavras) em texto limpo + srt, no formato que o extrair_ia.py
# consome. uso: legenda_para_texto.py <legenda.srt> <base_saida>
import re, sys

def main():
    entrada, base = sys.argv[1], sys.argv[2]
    linhas = open(entrada, encoding='utf-8').read().split('\n')
    n = len(linhas)

    def eh_indice(k):
        return (k < n and re.fullmatch(r'\d+', linhas[k].strip())
                and k + 1 < n and '-->' in linhas[k + 1])

    cues = []
    i = 0
    while i < n:
        if eh_indice(i):
            ts = linhas[i + 1].strip()
            k = i + 2
            texto = []
            while k < n and not eh_indice(k):
                s = linhas[k].strip()
                if s and '-->' not in s:
                    texto.append(s)
                k += 1
            if texto:
                cues.append((ts, texto[-1]))   # a linha mais nova do bloco
            i = k
        else:
            i += 1

    limpo = []
    prev = ''
    for ts, t in cues:
        if t != prev:
            limpo.append((ts, t)); prev = t

    with open(base + '.txt', 'w', encoding='utf-8') as ft:
        for _, t in limpo:
            ft.write(t + '\n')
    with open(base + '.srt', 'w', encoding='utf-8') as fs:
        for idx, (ts, t) in enumerate(limpo, 1):
            fs.write(f"{idx}\n{ts}\n{t}\n\n")
    print(f"{len(limpo)} linhas limpas -> {base}.txt e {base}.srt", file=sys.stderr)

if __name__ == '__main__':
    main()
