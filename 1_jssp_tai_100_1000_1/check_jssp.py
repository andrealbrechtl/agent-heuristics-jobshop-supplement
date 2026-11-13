#!/usr/bin/env python3
"""Independent JSSP feasibility checker. usage: check_jssp.py instance schedule [--quiet]
Instance: 'n m' then n lines of m (machine duration) pairs, 0-based machines; '#' comments ignored.
Schedule: n lines x m start times (job-major, routing order). Prints key=value result line."""
import sys, re, hashlib
import numpy as np
def toks(p):
    return re.sub(r'#[^\n]*','',open(p).read()).split()
def check(inst, sched):
    t=toks(inst); n,m=int(t[0]),int(t[1])
    b=np.array(t[2:],dtype=np.int64)
    if b.size!=n*m*2: return dict(ok=False,err='instance size %d'%b.size)
    b=b.reshape(n,m,2); MA,DU=b[:,:,0],b[:,:,1]
    if MA.min()<0 or MA.max()>=m: return dict(ok=False,err='machine id range')
    s=toks(sched)
    if len(s)!=n*m: return dict(ok=False,err='schedule has %d tokens, expected %d'%(len(s),n*m))
    S=np.array(s,dtype=np.int64).reshape(n,m); E=S+DU
    neg=int((S<0).sum()); chain=int((S[:,1:]<E[:,:-1]).sum())
    ov=0; badcnt=0
    for mu in range(m):
        j,o=np.where(MA==mu)
        if len(j)!=n: badcnt+=1
        i=np.argsort(S[j,o],kind='stable'); st=S[j,o][i]; en=E[j,o][i]
        ov+=int((st[1:]<en[:-1]).sum())
    mk=int(E.max()); jobend=E[:,-1]
    return dict(ok=neg==0 and chain==0 and ov==0 and badcnt==0, neg=neg, chain_viol=chain, machine_overlaps=ov,
                machines_wrong_count=badcnt, makespan=mk, jobs_at_makespan=int((jobend==mk).sum()),
                md5=hashlib.md5(open(sched,'rb').read()).hexdigest(),
                sha256=hashlib.sha256(open(sched,'rb').read()).hexdigest())
if __name__=='__main__':
    r=check(sys.argv[1],sys.argv[2]); print(sys.argv[2], ' '.join(f'{k}={v}' for k,v in r.items()))
    sys.exit(0 if r['ok'] else 1)
