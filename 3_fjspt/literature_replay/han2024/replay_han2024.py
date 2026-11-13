import sys, json, subprocess
from pathlib import Path
from fractions import Fraction as F
S = str(Path(__file__).resolve().parents[2])  # .../3_fjspt
sys.path.insert(0, S+'/literature_replay')
from replay_meng_iga import replay
out=Path(sys.argv[2]); out.mkdir(exist_ok=True)
dec=lambda s:' '.join(str(int(c,36)) for c in s)
for line in open(sys.argv[1]):
    name,rep,os_,ms,agv=line.strip().split(';')[:5]
    inst=name.replace('MFJS','MFJST') if name.startswith('MFJS') else (name.replace('MK','MKT') if name.startswith('MK') else name)
    idir=S+'/generalization/instances' if inst.startswith('MFJST') else S+'/instances'
    I=json.load(open(f'{idir}/{inst}.json'))
    try:
        mk,ops,trs=replay(I,dec(os_),dec(ms),dec(agv))
    except AssertionError as e:
        print(inst,'reported',rep,'REPLAY ERROR',e); continue
    g=lambda x:f'{float(x):g}'
    p=out/f'dcga2024_{inst}.sol'
    with open(p,'w') as f:
        f.write(f'instance {inst}\nmakespan {g(mk)}\n# replay of Han et al. 2024 (DCGA) Appendix A; reported {rep}\n')
        for t in trs: f.write('tr %d %d %d %d %d %s %s\n'%(*t[:5],g(t[5]),g(t[6])))
        for t in ops: f.write('op %d %d %d %s %s\n'%(*t[:3],g(t[3]),g(t[4])))
    r=subprocess.run([sys.executable,'-I',S+'/check_fjspt.py',f'{idir}/{inst}.json',str(p)],capture_output=True,text=True)
    print(inst,'reported',rep,'replayed',g(mk),'MATCH' if F(rep)==mk else 'DIFF','| checker:',(r.stdout+r.stderr).strip().splitlines()[-1][:120])
