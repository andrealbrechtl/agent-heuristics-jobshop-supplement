"""Shared helpers for the CP Optimizer runs."""
import json, os, time, re
from docplex.cp.solver.solver import CpoSolver

CPO_EXE = os.environ.get("CPO_EXE", "/opt/ibm/ILOG/CPLEX_Studio_222_acad/cpoptimizer/bin/x86-64_linux/cpoptimizer")

def toks(path):
    return re.sub(r'#[^\n]*', '', open(path).read()).split()

def run_search(model, obj_expr, time_limit, workers, seed, trace_path):
    """Run CPO, record every improving solution as (wall s, makespan). Returns (last_solution, trace, bound, status)."""
    params = dict(TimeLimit=time_limit, Workers=workers, RandomSeed=seed,
                  RelativeOptimalityTolerance=0, OptimalityTolerance=0, LogVerbosity="Quiet")
    solver = CpoSolver(model, execfile=CPO_EXE, **params)
    t0 = time.time(); trace = []; best = None; last = None
    while True:
        sol = solver.search_next()
        if not sol or not sol.is_solution():
            last = sol; break
        v = sol.get_objective_values()[0]
        trace.append((round(time.time() - t0, 3), v)); best = sol
        if sol.is_solution_optimal():
            last = sol; break
    solver.end()
    bound = None; status = "NoSolution"
    ref = last if last is not None else best
    if ref is not None:
        try: bound = ref.get_objective_bounds()[0]
        except Exception: pass
        status = ref.get_solve_status()
    json.dump(dict(trace=trace, bound=bound, status=status, time_limit=time_limit,
                   workers=workers, seed=seed, wall=round(time.time() - t0, 3)),
              open(trace_path, "w"), indent=1)
    return best, trace, bound, status
