import sys,numpy as np,time
from ortools.sat.python import cp_model
from common import *
# incremental exact core: feasibility at T inside propagated domains, K grows, each stage hinted by the last
k=int(sys.argv[1]); T=int(sys.argv[2]); doms=sys.argv[3]; Ks=[int(x) for x in sys.argv[4].split(',')]; out=sys.argv[5]; tl=float(sys.argv[6])
m,d=load(k); L=d.sum(1); N,M=d.shape
D=np.loadtxt(doms,dtype=np.int64); lo=D[:N]; hi=D[N:]
slack=T-L; order=sorted(range(N),key=lambda j:(slack[j],j))
S=lo.copy()   # hint: earliest
if len(sys.argv)>8: S=np.maximum(np.minimum(loadsol(sys.argv[8]),hi),lo)
import bisect
placed=set()
def greedy_insert(j,S,active):
    """left-justified insertion of job j into the gaps of the active jobs (exact for one job)"""
    occ={}
    for a in active:
        for o in range(M):
            occ.setdefault(int(m[a,o]),[]).append((int(S[a,o]),int(S[a,o]+d[a,o])))
    for v in occ.values(): v.sort()
    t=0
    for o in range(M):
        k=int(m[j,o]); p=int(d[j,o]); t=max(t,int(lo[j,o]))
        for (s0,e0) in occ.get(k,[]):
            if e0<=t: continue
            if s0>=t+p: break
            t=e0
        S[j,o]=min(t,int(hi[j,o])); t=S[j,o]+p
prevK=int(open(sys.argv[8]+'.k').read()) if len(sys.argv)>8 else 0
for K in Ks:
    J=order[:K]
    for j in order[prevK:K]: greedy_insert(j,S,order[:prevK])
    prevK=K
    md=cp_model.CpModel(); V={}; bym={}
    for j in J:
        prev=None
        for o in range(M):
            v=md.NewIntVar(int(lo[j,o]),int(hi[j,o]),''); V[j,o]=v; md.AddHint(v,int(min(max(S[j,o],lo[j,o]),hi[j,o])))
            bym.setdefault(int(m[j,o]),[]).append((md.NewFixedSizeIntervalVar(v,int(d[j,o]),''),int(lo[j,o]),int(hi[j,o]+d[j,o])))
            if prev is not None: md.Add(v>=prev+int(d[j,o-1]))
            prev=v
    for kk,l in bym.items():
        keep=set()
        for a in range(len(l)):
            for b in range(a+1,len(l)):
                if l[a][1]<l[b][2] and l[b][1]<l[a][2]: keep.add(a); keep.add(b)
        if len(keep)>1: md.AddNoOverlap([l[a][0] for a in keep])
    sv=cp_model.CpSolver(); sv.parameters.max_time_in_seconds=tl; sv.parameters.num_workers=int(sys.argv[7]) if len(sys.argv)>7 else 4
    t=time.time(); r=sv.Solve(md)
    print(f'K={K} {sv.StatusName(r)} {time.time()-t:.1f}s',flush=True)
    if r not in (cp_model.OPTIMAL,cp_model.FEASIBLE): break
    for (j,o),v in V.items(): S[j,o]=sv.Value(v)
    np.savetxt(out,S,fmt='%d'); open(out+'.k','w').write(str(K))
