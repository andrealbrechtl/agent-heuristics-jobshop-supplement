import os
import numpy as np
INST = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'instances')
def load(k):
    a=np.loadtxt(os.path.join(INST, f'tai_100_1000_{k}.txt'),skiprows=1,dtype=np.int64)
    return a[:,0::2].copy(),a[:,1::2].copy()
def loadsol(path):
    return np.loadtxt(path,comments='#',dtype=np.int64)
def compact(m,d,S):
    """semi-active left shift keeping machine orders"""
    N,M=S.shape; order=np.argsort(S,axis=None,kind='stable')
    S2=np.zeros_like(S); jend=np.zeros(N,np.int64); mend=np.zeros(M,np.int64)
    jl=order//M; ol=order%M
    md=m.ravel(); dd=d.ravel()
    for idx,j,o in zip(order.tolist(),jl.tolist(),ol.tolist()):
        k=md[idx]; s=jend[j] if jend[j]>mend[k] else mend[k]
        S2[j,o]=s; e=s+dd[idx]; jend[j]=e; mend[k]=e
    return S2
def check(m,d,S):
    N,M=S.shape
    if (S<0).any(): return False,'neg'
    if (S[:,1:]<S[:,:-1]+d[:,:-1]).any(): return False,'chain'
    s=S.ravel(); e=s+d.ravel(); k=m.ravel()
    o=np.lexsort((s,k)); 
    same=k[o][1:]==k[o][:-1]
    if (e[o][:-1][same]>s[o][1:][same]).any(): return False,'overlap'
    return True,int((S[:,-1]+d[:,-1]).max())
