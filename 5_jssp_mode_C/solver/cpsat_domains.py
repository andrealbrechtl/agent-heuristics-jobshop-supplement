import sys,time,numpy as np
from ortools.sat.python import cp_model
from common import *
k=int(sys.argv[1]); T=int(sys.argv[2])
m,d=load(k); N,M=d.shape; L=d.sum(1)
pref=np.concatenate([np.zeros((N,1),np.int64),np.cumsum(d,1)[:,:-1]],1)
md=cp_model.CpModel(); V={}; bym={}
for j in range(N):
    s=int(T-L[j]); prev=None
    for o in range(M):
        lo=int(pref[j,o]); v=md.NewIntVar(lo,lo+s,f'x{j}_{o}'); V[j,o]=v
        bym.setdefault(int(m[j,o]),[]).append((md.NewFixedSizeIntervalVar(v,int(d[j,o]),''),lo,lo+s+int(d[j,o])))
        if prev is not None: md.Add(v>=prev+int(d[j,o-1]))
        prev=v
for kk,l in bym.items():
    keep=set()
    l.sort(key=lambda x:x[1])
    for a in range(len(l)):
        for b in range(a+1,len(l)):
            if l[b][1]>=l[a][2]: break
            keep.add(a); keep.add(b)
    if len(keep)>1: md.AddNoOverlap([l[a][0] for a in keep])
sv=cp_model.CpSolver(); sv.parameters.max_time_in_seconds=float(sys.argv[3]); sv.parameters.num_workers=int(sys.argv[4]) if len(sys.argv)>4 else 1
sv.parameters.fill_tightened_domains_in_response=True
t=time.time(); r=sv.Solve(md); print(sv.StatusName(r),f'{time.time()-t:.1f}s')
resp=sv.ResponseProto()
td=resp.tightened_variables
print('tightened vars',len(td))
if len(td):
    lo=np.zeros((N,M),np.int64); hi=np.zeros((N,M),np.int64)
    for j in range(N):
        for o in range(M):
            dom=td[V[j,o].Index()].domain
            lo[j,o]=dom[0]; hi[j,o]=dom[len(dom)-1]
    w0=(T-L)[:,None]*np.ones((1,M)); w=hi-lo
    print('mean corridor width before',w0.mean(),'after',w.mean(),'ops narrowed',(w<w0).sum())
    sl=T-L; o2=np.argsort(sl)
    for j in o2[:6]: print(' job',j,'slack',sl[j],'min width',w[j].min(),'mean',w[j].mean())
    np.save(sys.argv[5] if len(sys.argv)>5 else f"dom{k}_{T}.npy",np.stack([lo,hi]))
