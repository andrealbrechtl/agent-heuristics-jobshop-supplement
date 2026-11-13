"""Shared helpers for the Hexaly runs. Same design as ../cpo/common.py."""
import json, re, sys, time
sys.path.insert(0, "/opt/hexaly_15_0/bin/python")
import hexaly.optimizer as hx

def toks(path):
    return re.sub(r'#[^\n]*', '', open(path).read()).split()

def run_search(opt, obj, time_limit, threads, seed, trace_path):
    """Solve with Hexaly; record every improvement of the incumbent as (wall s, makespan) once per second.
    Returns (trace, bound, status). The model must be closed."""
    opt.param.time_limit = int(time_limit)
    opt.param.nb_threads = threads
    opt.param.seed = seed
    opt.param.verbosity = 0
    opt.param.time_between_ticks = 1
    trace = []; t0 = time.time()
    def cb(o, kind):
        st = o.solution.status
        if st in (hx.HxSolutionStatus.FEASIBLE, hx.HxSolutionStatus.OPTIMAL):
            v = obj.value
            if not trace or v < trace[-1][1]:
                trace.append((round(time.time() - t0, 3), v))
    opt.add_callback(hx.HxCallbackType.TIME_TICKED, cb)
    opt.solve()
    st = opt.solution.status
    if st in (hx.HxSolutionStatus.FEASIBLE, hx.HxSolutionStatus.OPTIMAL):
        v = obj.value
        if not trace or v < trace[-1][1]:
            trace.append((round(time.time() - t0, 3), v))
    try: bound = opt.solution.get_objective_bound(0)
    except Exception: bound = None
    info = dict(trace=trace, bound=bound, status=str(st).split(".")[-1], time_limit=time_limit, threads=threads,
                seed=seed, wall=round(time.time() - t0, 3), version=hx.HxVersion.get_version())
    json.dump(info, open(trace_path, "w"), indent=1)
    return trace, bound, info["status"]
