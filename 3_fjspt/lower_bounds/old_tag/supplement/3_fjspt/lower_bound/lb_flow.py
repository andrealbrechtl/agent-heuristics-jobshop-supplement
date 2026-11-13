"""Relaxation lower bound for FJSPT makespan (CP-SAT).

Relaxation: choose a machine per op; loaded transports follow from the choice;
vehicles are modelled only by flow conservation of loaded + empty moves over the
locations (routes start at L/U, may end anywhere), so the empty travel needed to
chain the transports is bounded from below. Then
    Cmax >= (loaded + empty) / V + (min processing after the last drop-off)
    Cmax >= load of every machine
    Cmax >= every job's path length (transports + processing)
Sequencing interactions are ignored, so the optimum of this model is a valid LB.
All times are scaled by 2 internally.
"""
import json, sys
from ortools.sat.python import cp_model


def lb(inst_path, V=2, tlim=60, workers=8, verbose=False):
    I = json.load(open(inst_path))
    T = [[int(round(2 * x)) for x in r] for r in I["travel"]]
    jobs = I["jobs"]; M = I["n_machines"]
    L = range(M + 1)
    mdl = cp_model.CpModel()
    x = {}
    ops = []
    for j, J in enumerate(jobs):
        for k, alts in enumerate(J):
            ops.append((j, k))
            for m, p in alts:
                x[j, k, m] = mdl.NewBoolVar("")
            mdl.AddExactlyOne(x[j, k, m] for m, p in alts)
    # loaded transports: arc counts between locations
    loaded_terms = []
    inflow = {v: [] for v in L}; outflow = {v: [] for v in L}
    job_len = {}
    for j, J in enumerate(jobs):
        path_terms = []
        for k, alts in enumerate(J):
            for m, p in alts:
                path_terms.append(2 * p * x[j, k, m])
            if k == 0:
                for m, p in alts:
                    loaded_terms.append(T[0][m] * x[j, k, m])
                    path_terms.append(T[0][m] * x[j, k, m])
                    outflow[0].append(x[j, k, m]); inflow[m].append(x[j, k, m])
            else:
                for a, _ in J[k - 1]:
                    for b, _ in alts:
                        if a == b:
                            continue
                        y = mdl.NewBoolVar("")
                        mdl.AddBoolOr([x[j, k - 1, a].Not(), x[j, k, b].Not(), y])
                        mdl.AddImplication(y, x[j, k - 1, a]); mdl.AddImplication(y, x[j, k, b])
                        loaded_terms.append(T[a][b] * y)
                        path_terms.append(T[a][b] * y)
                        outflow[a].append(y); inflow[b].append(y)
        job_len[j] = sum(path_terms)
    ntr_max = sum(len(J) for J in jobs)
    # Direct pairing of every drop-off with the vehicle's next pickup (no multi-hop
    # empty paths): e[b, a] = number of (drop at b -> next pickup at a) pairs,
    # st[a] = routes whose first pickup is at a (vehicles start at L/U),
    # z[b] = routes ending with a drop at b.
    e = {(b, a): mdl.NewIntVar(0, ntr_max, "") for b in L for a in L if b != 0}
    st = {a: mdl.NewIntVar(0, V, "") for a in L}
    z = {b: mdl.NewIntVar(0, V, "") for b in L if b != 0}
    mdl.Add(sum(st.values()) == sum(z.values()))
    mdl.Add(sum(st.values()) <= V)
    for b in L:
        if b == 0:
            continue
        mdl.Add(sum(e[b, a] for a in L) + z[b] == sum(inflow[b]))
    for a in L:
        mdl.Add(sum(e[b, a] for b in L if b != 0) + st[a] == sum(outflow[a]))
    empty = sum(T[b][a] * e[b, a] for (b, a) in e) + sum(T[0][a] * st[a] for a in L)
    horizon = 4 * sum(max(p for m, p in a) for J in jobs for a in J) + 4 * ntr_max * max(map(max, T))
    cmax = mdl.NewIntVar(0, horizon, "cmax")
    pmin_last = min(2 * min(p for m, p in a) for J in jobs for a in J)  # tail after a vehicle's last drop-off
    # vehicle workload (V * Cmax >= total vehicle time + V * minimal tail processing)
    mdl.Add(V * cmax >= sum(loaded_terms) + empty + V * pmin_last)
    for m in range(1, M + 1):
        mdl.Add(cmax >= sum(2 * p * x[j, k, mm] for (j, k) in ops for mm, p in jobs[j][k] if mm == m))
    for j in job_len:
        mdl.Add(cmax >= job_len[j])
    mdl.Minimize(cmax)
    s = cp_model.CpSolver()
    s.parameters.max_time_in_seconds = tlim
    s.parameters.num_workers = workers
    st = s.Solve(mdl)
    return s.BestObjectiveBound() / 2, s.ObjectiveValue() / 2, s.StatusName(st)


if __name__ == "__main__":
    for p in sys.argv[1:]:
        b, v, st = lb(p)
        print(f"{p}: LB={b:g}  (relaxation incumbent {v:g}, {st})", flush=True)
