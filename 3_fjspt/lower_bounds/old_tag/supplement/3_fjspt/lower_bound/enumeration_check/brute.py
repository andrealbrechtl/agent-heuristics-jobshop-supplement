"""Exhaustive optimum of tiny FJSPT instances (same semantics as the checker check_fjspt.py).
Enumerates machine assignment, machine sequences, vehicle assignment and per-vehicle transport
sequences; each candidate is evaluated as an earliest-start schedule by longest path
(a cycle in the precedence graph means infeasible)."""
import itertools
from fractions import Fraction as F


def evaluate(jobs, T, assign, mseq, vseq):
    ops = [(j, k) for j, J in enumerate(jobs) for k in range(len(J))]
    p = {o: jobs[o[0]][o[1]][assign[o]] for o in ops}
    cons = []; base = {}; trans = {}
    for (j, k) in ops:
        m = assign[(j, k)]
        frm = 0 if k == 0 else assign[(j, k - 1)]
        if frm != m:
            trans[(j, k)] = (frm, m)
            cons.append((('D', j, k), ('S', j, k), T[frm][m]))
            if k == 0: base[('D', j, k)] = 0
            else: cons.append((('S', j, k - 1), ('D', j, k), p[(j, k - 1)]))
        else:
            cons.append((('S', j, k - 1), ('S', j, k), p[(j, k - 1)]))
    for m, seq in mseq.items():
        for a, b in zip(seq, seq[1:]): cons.append((('S',) + a, ('S',) + b, p[a]))
    for seq in vseq:
        prev = None
        for t in seq:
            a, b = trans[t]
            if prev is None: base[('D',) + t] = max(base.get(('D',) + t, 0), T[0][a])
            else:
                pa, pb = trans[prev]
                cons.append((('D',) + prev, ('D',) + t, T[pa][pb] + T[pb][a]))
            prev = t
    nodes = set(base) | {u for u, v, w in cons} | {v for u, v, w in cons} | {('S',) + o for o in ops}
    val = {n: base.get(n, 0) for n in nodes}
    for it in range(len(nodes) + 2):
        ch = False
        for u, v, w in cons:
            if val[v] < val[u] + w: val[v] = val[u] + w; ch = True
        if not ch: break
    else: return None
    return max(val[('S',) + o] + p[o] for o in ops)


def brute(jobs, T, V=2):
    ops = [(j, k) for j, J in enumerate(jobs) for k in range(len(J))]
    best = None
    for choice in itertools.product(*[list(jobs[j][k].keys()) for (j, k) in ops]):
        assign = dict(zip(ops, choice))
        bym = {}
        for o in ops: bym.setdefault(assign[o], []).append(o)
        ms = list(bym)
        trs = [o for o in ops if (0 if o[1] == 0 else assign[(o[0], o[1] - 1)]) != assign[o]]
        for mperm in itertools.product(*[list(itertools.permutations(bym[m])) for m in ms]):
            mseq = dict(zip(ms, mperm))
            for vv in itertools.product(range(V), repeat=len(trs)):
                groups = [[t for t, v in zip(trs, vv) if v == x] for x in range(V)]
                for vperm in itertools.product(*[itertools.permutations(g) for g in groups]):
                    r = evaluate(jobs, T, assign, mseq, [list(s) for s in vperm])
                    if r is not None and (best is None or r < best): best = r
    return best


def gen(rng, nj, nops, nm, half=True, tri=False, zero=False):
    q = lambda lo, hi: F(rng.randint(2 * lo, 2 * hi), 2) if half else F(rng.randint(lo, hi))
    jobs = []
    for j in range(nj):
        J = []
        for k in range(nops[j]):
            ms = rng.sample(range(1, nm + 1), rng.randint(1, min(2, nm)))
            J.append({m: F(rng.randint(1, 6)) for m in ms})
        jobs.append(J)
    n = nm + 1
    T = [[F(0)] * n for _ in range(n)]
    for a in range(n):
        for b in range(n):
            if a != b: T[a][b] = q(0 if zero else 1, 8)
    if not tri:  # metric: shortest-path closure
        for k in range(n):
            for a in range(n):
                for b in range(n):
                    T[a][b] = min(T[a][b], T[a][k] + T[k][b]) if a != b else T[a][b]
    return jobs, T


def to_json(jobs, T, name):
    num = lambda x: int(x) if x == int(x) else float(x)
    return dict(name=name, n_jobs=len(jobs), n_machines=len(T) - 1,
                jobs=[[[[m, num(p)] for m, p in a.items()] for a in J] for J in jobs],
                travel=[[num(x) for x in r] for r in T])
