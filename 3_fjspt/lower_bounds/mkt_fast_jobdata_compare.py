#!/usr/bin/env python3
"""MKT04 puzzle, step 0: compare MK01-05 job data of the FAST distribution of Homayouni & Fontes
(brandimarte1993-2.pdf, sha256 25e10959...c469, pdftotext copy) with ../instances/MKT0x.json.
(SOURCE.md compared only MK03, 06-10 with this PDF.)  Usage: mkt_fast_jobdata_compare.py [PDF_TEXT]"""
import json, re, sys
if len(sys.argv) < 2:
    sys.exit("usage: mkt_fast_jobdata_compare.py <brandimarte1993-2.txt from the FAST distribution of Homayouni and Fontes (2021)>")
src = sys.argv[1]
txt = open(src).read()
def block(name):
    rest = txt[txt.index(name + ':') + len(name) + 1:]
    j = re.search(r'Mk\d\d:', rest)
    return (rest[:j.start()] if j else rest).split()
for nm in ['Mk01', 'Mk02', 'Mk03', 'Mk04', 'Mk05']:
    tok = block(nm); n = int(tok[0]); p = 3; jobs = []
    for _ in range(n):
        k = int(tok[p]); p += 1; J = []
        for _ in range(k):
            a = int(tok[p]); p += 1
            J.append(sorted([int(tok[p + 2 * q]), float(tok[p + 2 * q + 1])] for q in range(a))); p += 2 * a
        jobs.append(J)
    ours = [[sorted([m, float(pp)] for m, pp in a) for a in J] for J in json.load(open(f'../instances/MKT{nm[2:]}.json'))['jobs']]
    print(nm, 'unparsed tokens:', len(tok) - p, '->', 'IDENTICAL to our file' if ours == jobs else 'DIFFERENT')
