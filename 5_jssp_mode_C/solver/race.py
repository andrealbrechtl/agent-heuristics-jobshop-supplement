import subprocess, time, os, sys, re, random
# racing driver: keep csp+domains runs that keep descending, replace the others
k=int(sys.argv[1]); T=int(sys.argv[2]); dom=sys.argv[3]; slots=int(sys.argv[4]); hours=float(sys.argv[5]); tag=sys.argv[6]
CHECK=600; WIN=300
env0=dict(os.environ, DOMS=dom, JSPV='1')
def amts(log):
    try: L=open(log).read().splitlines()
    except: return []
    out=[]
    for l in L:
        m=re.search(r'amt=(\d+).*\((\d+)s\)',l)
        if m and 'it=' in l: out.append((int(m.group(2)),int(m.group(1))))
    return out
def avg(a,t0,t1):
    v=[x for (t,x) in a if t0<=t<t1]; return sum(v)/len(v) if v else None
runs=[]; seedc=random.Random(k*7919+slots).randrange(10**6)
def start():
    global seedc
    seedc+=1; s=seedc; log=f'log_race_{tag}_{s}.txt'
    p=subprocess.Popen([os.environ.get('BIN','./nego2'),f'inst/tai_100_1000_{k}.txt',f'race_{tag}_{s}.txt',str(T),str(int(hours*3600)),str(s),'8','100000','none','1000','200000'],
        env=dict(env0,BESTDUMP=f'raceb_{tag}_{s}.txt'),stdout=subprocess.DEVNULL,stderr=open(log,'w'))
    return dict(p=p,s=s,log=log,t=time.time(),next=CHECK)
for _ in range(slots): runs.append(start())
end=time.time()+hours*3600; best_recent=None
while time.time()<end:
    time.sleep(30)
    for i,r in enumerate(runs):
        a=amts(r['log'])
        if 'FEASIBLE' in open(r['log']).read():
            print(f'FEASIBLE seed {r["s"]} file race_{tag}_{r["s"]}.txt',flush=True)
            for q in runs: q['p'].kill()
            sys.exit(0)
        age=time.time()-r['t']
        if r['p'].poll() is not None: runs[i]=start(); continue
        if age>=r['next']:
            now=avg(a,age-WIN,age+1); prev=avg(a,age-2*WIN,age-WIN)
            if now is None or prev is None: continue
            if best_recent is None or now<best_recent: best_recent=now
            r['own']=min(r.get('own',now),now)
            leader = r['own']<=min(q.get('own',1e18) for q in runs)
            keep = leader or now<0.92*prev or now<1.5*best_recent
            print(f'seed {r["s"]} age {age/60:.0f}m recent {now:.0f} prev {prev:.0f} best {best_recent:.0f} -> {"keep" if keep else "replace"}',flush=True)
            if keep: r['next']+=CHECK
            else: r['p'].kill(); runs[i]=start()
for q in runs: q['p'].kill()
