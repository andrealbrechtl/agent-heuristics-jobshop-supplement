// ---------------------------------------------------------------------------
// Flexible Job Shop Scheduling Problem -- parallel tabu search / ruin-and-recreate
//
// Encoding      : machine assignment per operation + per-machine operation
//                 sequence (the standard two-vector representation).  Decoded
//                 into a schedule by longest-path (head) computation on the
//                 disjunctive graph.
// Neighbourhood : Mastrolilli & Gambardella (2000) -- take an operation on the
//                 critical path, move it to any eligible machine and any
//                 position in that machine's sequence.  Every candidate is
//                 scored in O(1) from precomputed heads/tails; only the move
//                 that is actually applied gets an exact O(n) re-evaluation.
// Metaheuristic : tabu search on (operation, machine) attributes with
//                 aspiration, plus ruin-and-recreate perturbation and restarts
//                 from a shared incumbent.
// Parallelism   : independent searchers on separate threads sharing a global
//                 incumbent; stagnating threads reseed from it.
//
// Build: g++ -O3 -march=native -pthread -o fjsp_solver fjsp_solver.cpp
// ---------------------------------------------------------------------------

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// ------------------------------- instance ---------------------------------

struct Inst {
    int nJobs = 0, nMach = 0, nOps = 0;
    std::vector<int> jobOf, opInJob, jpred, jsucc;
    std::vector<int> pt;                    // nOps * nMach, -1 when ineligible
    std::vector<std::vector<int>> elig;     // per op: eligible machine ids
    std::vector<int> minPt;                 // per op: min processing time
    bool oneIndexed = false;
    double headerAvgElig = -1.0;
    std::string name;

    inline int P(int o, int m) const { return pt[(size_t)o * nMach + m]; }
};

static Inst parseInstance(const std::string& path) {
    std::ifstream f(path);
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); exit(1); }

    std::string first;
    std::getline(f, first);
    std::istringstream hs(first);
    std::vector<std::string> htok;
    for (std::string t; hs >> t;) htok.push_back(t);
    if (htok.size() < 2) { fprintf(stderr, "bad header\n"); exit(1); }

    Inst in;
    in.nJobs = std::stoi(htok[0]);
    in.nMach = std::stoi(htok[1]);
    if (htok.size() >= 3) in.headerAvgElig = std::stod(htok[2]);

    // Read the body as raw integers, keeping machine ids unnormalised so the
    // indexing convention can be decided from the data itself.
    std::vector<long long> body;
    for (long long v; f >> v;) body.push_back(v);

    struct RawOp { std::vector<std::pair<int, int>> opts; };
    std::vector<std::vector<RawOp>> jobs;
    size_t p = 0;
    int rawMin = INT_MAX, rawMax = INT_MIN;
    for (int j = 0; j < in.nJobs; ++j) {
        if (p >= body.size()) { fprintf(stderr, "truncated file at job %d\n", j); exit(1); }
        int nops = (int)body[p++];
        if (nops < 1 || nops > 100000) { fprintf(stderr, "implausible op count %d\n", nops); exit(1); }
        std::vector<RawOp> job(nops);
        for (int k = 0; k < nops; ++k) {
            int ne = (int)body[p++];
            if (ne < 1 || ne > in.nMach) { fprintf(stderr, "implausible eligible count %d\n", ne); exit(1); }
            for (int e = 0; e < ne; ++e) {
                int m = (int)body[p++];
                int d = (int)body[p++];
                if (d <= 0) { fprintf(stderr, "non-positive duration %d\n", d); exit(1); }
                rawMin = std::min(rawMin, m);
                rawMax = std::max(rawMax, m);
                job[k].opts.push_back({m, d});
            }
        }
        jobs.push_back(std::move(job));
    }
    if (p != body.size()) fprintf(stderr, "warning: %zu trailing tokens\n", body.size() - p);

    // Indexing convention, decided from observed machine ids and reported below
    // so it can be audited against the independent validator.
    int off;
    if (rawMax >= in.nMach)      off = 1;
    else if (rawMin == 0)        off = 0;
    else                         off = 1;
    in.oneIndexed = (off == 1);
    if (rawMin - off < 0 || rawMax - off >= in.nMach) {
        fprintf(stderr, "machine ids [%d,%d] inconsistent with %d machines\n", rawMin, rawMax, in.nMach);
        exit(1);
    }

    for (auto& job : jobs) in.nOps += (int)job.size();
    in.jobOf.resize(in.nOps); in.opInJob.resize(in.nOps);
    in.jpred.resize(in.nOps); in.jsucc.resize(in.nOps);
    in.pt.assign((size_t)in.nOps * in.nMach, -1);
    in.elig.resize(in.nOps);
    in.minPt.resize(in.nOps);

    int o = 0;
    for (int j = 0; j < in.nJobs; ++j) {
        int nops = (int)jobs[j].size();
        for (int k = 0; k < nops; ++k, ++o) {
            in.jobOf[o] = j; in.opInJob[o] = k;
            in.jpred[o] = (k == 0)         ? -1 : o - 1;
            in.jsucc[o] = (k == nops - 1)  ? -1 : o + 1;
            int mn = INT_MAX;
            for (auto& pr : jobs[j][k].opts) {
                int m = pr.first - off;
                in.pt[(size_t)o * in.nMach + m] = pr.second;
                in.elig[o].push_back(m);
                mn = std::min(mn, pr.second);
            }
            in.minPt[o] = mn;
        }
    }

    size_t nameA = path.find_last_of('/');
    in.name = (nameA == std::string::npos) ? path : path.substr(nameA + 1);
    size_t dot = in.name.find_last_of('.');
    if (dot != std::string::npos) in.name = in.name.substr(0, dot);

    long long totElig = 0;
    for (int i = 0; i < in.nOps; ++i) totElig += (int)in.elig[i].size();
    double avgElig = (double)totElig / in.nOps;

    fprintf(stderr,
            "[parse] %s: %d jobs x %d machines, %d ops | machine ids %d..%d (%s) "
            "| avg eligible/op %.2f (header says %.2f)\n",
            in.name.c_str(), in.nJobs, in.nMach, in.nOps, rawMin, rawMax,
            in.oneIndexed ? "1-indexed -> normalised to 0" : "0-indexed",
            avgElig, in.headerAvgElig);
    if (in.headerAvgElig > 0 && std::abs(avgElig - in.headerAvgElig) > 0.02)
        fprintf(stderr, "[parse] WARNING: avg eligibility disagrees with header!\n");

    return in;
}

// ------------------------------- solution ---------------------------------

struct Sol {
    std::vector<int> mach, pcur, pos;       // per operation
    std::vector<std::vector<int>> seq;      // per machine
    std::vector<int> r, q;                  // heads, tails
    int cmax = INT_MAX;
};

// Ablation switches, read from the environment.  All default to 0, in which
// case the full algorithm described at the top of the file runs.  They exist so
// the contribution of each component can be measured rather than asserted.
struct Config {
    bool noTabu     = false;  // pure descent + perturbation, no tabu memory
    bool noRR       = false;  // random kicks instead of ruin-and-recreate
    bool noShare    = false;  // independent multi-start, no shared incumbent
    bool exactEval  = false;  // exactly evaluate every neighbour (no MG estimate)
    bool gtOnly     = false;  // dispatching-rule construction only, no local search
    Config() {
        auto on = [](const char* k) { const char* v = getenv(k); return v && atoi(v) != 0; };
        noTabu    = on("FJSP_NO_TABU");
        noRR      = on("FJSP_NO_RR");
        noShare   = on("FJSP_NO_SHARE");
        exactEval = on("FJSP_EXACT_EVAL");
        gtOnly    = on("FJSP_GT_ONLY");
    }
};
static Config CFG;

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed ? seed : 0x9E3779B97F4A7C15ULL) {}
    inline uint64_t next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return s; }
    inline int operator()(int n) { return n <= 1 ? 0 : (int)(next() % (uint64_t)n); }
    inline double unit() { return (double)(next() >> 11) * (1.0 / 9007199254740992.0); }
};

// ------------------------------- searcher ---------------------------------

struct Searcher {
    const Inst& in;
    Rng rng;
    // scratch
    std::vector<int> indeg, topo, stackBuf;
    std::vector<int> tabu;                  // (op, machine) -> iteration until forbidden
    std::vector<int> critOps;

    explicit Searcher(const Inst& i, uint64_t seed) : in(i), rng(seed) {
        indeg.resize(in.nOps); topo.reserve(in.nOps); stackBuf.reserve(in.nOps);
        tabu.assign((size_t)in.nOps * in.nMach, -1);
        critOps.reserve(in.nOps);
    }

    void initSol(Sol& s) const {
        s.mach.assign(in.nOps, -1); s.pcur.assign(in.nOps, 0); s.pos.assign(in.nOps, -1);
        s.seq.assign(in.nMach, {});
        s.r.assign(in.nOps, 0); s.q.assign(in.nOps, 0);
        s.cmax = INT_MAX;
    }

    inline int mpred(const Sol& s, int o) const {
        int p = s.pos[o];
        return (p <= 0) ? -1 : s.seq[s.mach[o]][p - 1];
    }
    inline int msucc(const Sol& s, int o) const {
        int p = s.pos[o];
        if (p < 0) return -1;
        const std::vector<int>& S = s.seq[s.mach[o]];
        return (p + 1 >= (int)S.size()) ? -1 : S[p + 1];
    }

    // Exact decode: longest path over job arcs + machine arcs.
    // Returns false if the disjunctive graph contains a cycle (infeasible).
    bool evaluate(Sol& s) {
        const int n = in.nOps;
        for (int o = 0; o < n; ++o) {
            int d = 0;
            if (in.jpred[o] >= 0) ++d;
            if (s.pos[o] > 0) ++d;
            indeg[o] = d;
            s.r[o] = 0;
        }
        topo.clear();
        for (int o = 0; o < n; ++o) if (indeg[o] == 0) topo.push_back(o);

        for (size_t h = 0; h < topo.size(); ++h) {
            int o = topo[h];
            int fin = s.r[o] + s.pcur[o];
            int js = in.jsucc[o];
            if (js >= 0) {
                if (fin > s.r[js]) s.r[js] = fin;
                if (--indeg[js] == 0) topo.push_back(js);
            }
            int ms = msucc(s, o);
            if (ms >= 0) {
                if (fin > s.r[ms]) s.r[ms] = fin;
                if (--indeg[ms] == 0) topo.push_back(ms);
            }
        }
        if ((int)topo.size() != n) return false;   // cycle

        int cm = 0;
        for (int i = n - 1; i >= 0; --i) {
            int o = topo[i];
            int t = 0;
            int js = in.jsucc[o];
            if (js >= 0) t = std::max(t, s.q[js] + s.pcur[js]);
            int ms = msucc(s, o);
            if (ms >= 0) t = std::max(t, s.q[ms] + s.pcur[ms]);
            s.q[o] = t;
            cm = std::max(cm, s.r[o] + s.pcur[o]);
        }
        s.cmax = cm;
        return true;
    }

    // ---- construction: Giffler-Thompson active schedule generation --------
    // rule: 0=EFT 1=MWR 2=SPT 3=LPT 4=random  (alpha adds randomised machine pick)
    void construct(Sol& s, int rule, double alpha) {
        initSol(s);
        std::vector<int> nextOp(in.nJobs), jobReady(in.nJobs, 0), machFree(in.nMach, 0);
        std::vector<int> workRem(in.nOps, 0);
        for (int o = in.nOps - 1; o >= 0; --o)
            workRem[o] = in.minPt[o] + (in.jsucc[o] >= 0 ? workRem[in.jsucc[o]] : 0);
        for (int j = 0, o = 0; j < in.nJobs; ++j) {
            nextOp[j] = o;
            while (o < in.nOps && in.jobOf[o] == j) ++o;
        }
        std::vector<int> cand;
        cand.reserve(in.nJobs);
        std::vector<std::pair<int, int>> pool;      // (op, machine) near-best finishers

        for (int done = 0; done < in.nOps; ++done) {
            cand.clear();
            for (int j = 0; j < in.nJobs; ++j)
                if (nextOp[j] >= 0) cand.push_back(nextOp[j]);

            int bestEct = INT_MAX;
            pool.clear();
            for (int o : cand)
                for (int m : in.elig[o]) {
                    int ect = std::max(jobReady[in.jobOf[o]], machFree[m]) + in.P(o, m);
                    if (ect < bestEct) bestEct = ect;
                }
            int thr = bestEct + (int)(alpha * bestEct);
            for (int o : cand)
                for (int m : in.elig[o]) {
                    int ect = std::max(jobReady[in.jobOf[o]], machFree[m]) + in.P(o, m);
                    if (ect <= thr) pool.push_back({o, m});
                }
            int mStar = pool[rng((int)pool.size())].second;
            int cStar = INT_MAX;
            for (int o : cand)
                if (in.P(o, mStar) >= 0)
                    cStar = std::min(cStar, std::max(jobReady[in.jobOf[o]], machFree[mStar]) + in.P(o, mStar));

            // conflict set: operations that could start on m* before c*
            int pick = -1;
            long long bestKey = LLONG_MIN;
            for (int o : cand) {
                if (in.P(o, mStar) < 0) continue;
                if (std::max(jobReady[in.jobOf[o]], machFree[mStar]) >= cStar) continue;
                long long key;
                switch (rule) {
                    case 0:  key = -(long long)(std::max(jobReady[in.jobOf[o]], machFree[mStar]) + in.P(o, mStar)); break;
                    case 1:  key = workRem[o]; break;
                    case 2:  key = -(long long)in.P(o, mStar); break;
                    case 3:  key = in.P(o, mStar); break;
                    default: key = rng(1 << 20); break;
                }
                key = key * 64 + rng(64);   // randomised tie-breaking
                if (key > bestKey) { bestKey = key; pick = o; }
            }
            int j = in.jobOf[pick];
            int st = std::max(jobReady[j], machFree[mStar]);
            int en = st + in.P(pick, mStar);
            s.mach[pick] = mStar; s.pcur[pick] = in.P(pick, mStar);
            s.pos[pick] = (int)s.seq[mStar].size();
            s.seq[mStar].push_back(pick);
            machFree[mStar] = en; jobReady[j] = en;
            nextOp[j] = in.jsucc[pick];
        }
        evaluate(s);
    }

    // ---- sequence surgery -------------------------------------------------
    void removeOp(Sol& s, int v) {
        std::vector<int>& S = s.seq[s.mach[v]];
        int p = s.pos[v];
        S.erase(S.begin() + p);
        for (int i = p; i < (int)S.size(); ++i) s.pos[S[i]] = i;
        s.pos[v] = -1;
    }
    void insertOp(Sol& s, int v, int k, int at) {
        std::vector<int>& S = s.seq[k];
        S.insert(S.begin() + at, v);
        for (int i = at; i < (int)S.size(); ++i) s.pos[S[i]] = i;
        s.mach[v] = k; s.pcur[v] = in.P(v, k);
    }

    struct Cand { int v, k, at, est; };

    // O(1)-per-position Mastrolilli-Gambardella estimate of the makespan that
    // results from placing v at position `at` of machine k, using the heads and
    // tails of the current solution as approximations.  This is the step that
    // makes a large neighbourhood affordable.
    template <class F>
    void scanInsertions(const Sol& s, int v, F&& emit) const {
        int rJP = (in.jpred[v] >= 0) ? s.r[in.jpred[v]] + s.pcur[in.jpred[v]] : 0;
        int qJS = (in.jsucc[v] >= 0) ? s.q[in.jsucc[v]] + s.pcur[in.jsucc[v]] : 0;
        int curM = s.mach[v];
        for (int k : in.elig[v]) {
            int pk = in.P(v, k);
            const std::vector<int>& S = s.seq[k];
            int L = (int)S.size();
            int skip = (k == curM) ? s.pos[v] : -1;
            int Le = (skip >= 0) ? L - 1 : L;
            for (int at = 0; at <= Le; ++at) {
                if (skip >= 0 && at == skip) continue;    // identity move
                int prev = -1, next = -1;
                if (at > 0)  { int i = at - 1; prev = S[(skip >= 0 && i >= skip) ? i + 1 : i]; }
                if (at < Le) { int i = at;     next = S[(skip >= 0 && i >= skip) ? i + 1 : i]; }
                int rIns = rJP, qIns = qJS;
                if (prev >= 0) rIns = std::max(rIns, s.r[prev] + s.pcur[prev]);
                if (next >= 0) qIns = std::max(qIns, s.q[next] + s.pcur[next]);
                emit(k, at, rIns + pk + qIns);
            }
        }
    }

    // Apply a move, exactly re-evaluate, and revert if it made a cycle.
    bool tryApply(Sol& s, int v, int k, int at, std::vector<int>& saveA, std::vector<int>& saveB,
                  int& oldM, int& oldPos) {
        oldM = s.mach[v]; oldPos = s.pos[v];
        saveA = s.seq[oldM];
        saveB = (k == oldM) ? std::vector<int>() : s.seq[k];
        removeOp(s, v);
        insertOp(s, v, k, at);
        if (evaluate(s)) return true;
        // revert
        s.seq[oldM] = saveA;
        if (k != oldM) s.seq[k] = saveB;
        for (int i = 0; i < (int)s.seq[oldM].size(); ++i) s.pos[s.seq[oldM][i]] = i;
        if (k != oldM) for (int i = 0; i < (int)s.seq[k].size(); ++i) s.pos[s.seq[k][i]] = i;
        s.mach[v] = oldM; s.pcur[v] = in.P(v, oldM); s.pos[v] = oldPos;
        evaluate(s);
        return false;
    }

    // Recombination for the two-vector encoding: take the machine assignment
    // from A and the operation ordering from B (B's start times give a global
    // topological order, so ordering each machine's queue by them can never
    // create a cycle -- the child is feasible by construction).
    void recombine(const Sol& A, const Sol& B, Sol& out) {
        initSol(out);
        for (int o = 0; o < in.nOps; ++o) {
            out.mach[o] = A.mach[o];
            out.pcur[o] = in.P(o, A.mach[o]);
        }
        std::vector<int> order(in.nOps);
        for (int o = 0; o < in.nOps; ++o) order[o] = o;
        std::sort(order.begin(), order.end(), [&](int x, int y) {
            if (B.r[x] != B.r[y]) return B.r[x] < B.r[y];
            return x < y;
        });
        for (int o : order) {
            int m = out.mach[o];
            out.pos[o] = (int)out.seq[m].size();
            out.seq[m].push_back(o);
        }
        if (!evaluate(out)) { out.cmax = INT_MAX; }
    }

    // Ablation alternative to ruin-and-recreate: purely random relocations.
    void randomKick(Sol& s, int nMoves) {
        for (int t = 0; t < nMoves; ++t) {
            int v = rng(in.nOps);
            int k = in.elig[v][rng((int)in.elig[v].size())];
            int oldM = s.mach[v], oldPos = s.pos[v];
            std::vector<int> a = s.seq[oldM], b = (k == oldM) ? std::vector<int>() : s.seq[k];
            removeOp(s, v);
            int at = rng((int)s.seq[k].size() + 1);
            insertOp(s, v, k, at);
            if (!evaluate(s)) {                       // undo an infeasible kick
                s.seq[oldM] = a;
                if (k != oldM) s.seq[k] = b;
                for (int i = 0; i < (int)s.seq[oldM].size(); ++i) s.pos[s.seq[oldM][i]] = i;
                if (k != oldM) for (int i = 0; i < (int)s.seq[k].size(); ++i) s.pos[s.seq[k][i]] = i;
                s.mach[v] = oldM; s.pcur[v] = in.P(v, oldM); s.pos[v] = oldPos;
                evaluate(s);
            }
        }
    }

    // ---- ruin & recreate --------------------------------------------------
    void ruinRecreate(Sol& s, int nJobsRuin, bool randomMachine) {
        std::vector<int> victims;
        for (int t = 0; t < nJobsRuin; ++t) {
            int j = rng(in.nJobs);
            for (int o = 0; o < in.nOps; ++o)
                if (in.jobOf[o] == j) victims.push_back(o);
        }
        std::sort(victims.begin(), victims.end());
        victims.erase(std::unique(victims.begin(), victims.end()), victims.end());
        for (size_t i = victims.size(); i > 1; --i) std::swap(victims[i - 1], victims[rng((int)i)]);

        for (int v : victims) {
            removeOp(s, v);
            evaluate(s);
            int bk = -1, ba = -1, be = INT_MAX;
            int forced = randomMachine ? in.elig[v][rng((int)in.elig[v].size())] : -1;
            scanInsertions(s, v, [&](int k, int at, int est) {
                if (forced >= 0 && k != forced) return;
                int e = est * 64 + rng(64);
                if (e < be) { be = e; bk = k; ba = at; }
            });
            if (bk < 0) { // fall back: append anywhere eligible
                bk = in.elig[v][rng((int)in.elig[v].size())]; ba = (int)s.seq[bk].size();
            }
            insertOp(s, v, bk, ba);
            if (!evaluate(s)) {   // extremely rare; repair by appending at the end
                removeOp(s, v);
                insertOp(s, v, bk, (int)s.seq[bk].size());
                evaluate(s);
            }
        }
    }
};

// ------------------------------ shared state ------------------------------

struct Shared {
    std::mutex mtx;
    std::atomic<int> best{INT_MAX};
    Sol bestSol;
    bool haveBest = false;
    std::vector<Sol> elite;            // diverse pool of good solutions (under mtx)
    static const int ELITE_MAX = 12;
    std::chrono::steady_clock::time_point t0;
    std::string schedPath, progPath;
    std::vector<std::pair<double, int>> trace;
    double bestTime = 0;
    std::atomic<bool> stop{false};
    std::atomic<long long> iters{0};        // total tabu iterations, all threads
    std::atomic<long long> moveEvals{0};    // total neighbours scored
};

static void writeSchedule(const Inst& in, const Sol& s, const std::string& path) {
    std::ofstream f(path);
    f << "instance " << in.name << "\n";
    f << "jobs " << in.nJobs << "\n";
    f << "machines " << in.nMach << "\n";
    f << "operations " << in.nOps << "\n";
    f << "makespan " << s.cmax << "\n";
    f << "# machine ids are 0-based; start/end are integers\n";
    f << "# job op machine start end\n";
    for (int o = 0; o < in.nOps; ++o)
        f << in.jobOf[o] << ' ' << in.opInJob[o] << ' ' << s.mach[o] << ' '
          << s.r[o] << ' ' << (s.r[o] + s.pcur[o]) << "\n";
}

// Keep a pool of good but structurally distinct solutions.  Restarting from a
// random elite rather than always from the single incumbent is what stops all
// 32 threads from collapsing onto the same basin.
static void addElite(Shared& sh, const Sol& s) {
    std::lock_guard<std::mutex> lk(sh.mtx);
    uint64_t h = 1469598103934665603ULL;
    for (int m : s.mach) { h ^= (uint64_t)m; h *= 1099511628211ULL; }
    for (auto& e : sh.elite) {
        uint64_t he = 1469598103934665603ULL;
        for (int m : e.mach) { he ^= (uint64_t)m; he *= 1099511628211ULL; }
        if (he == h) return;                       // already have this assignment
    }
    if ((int)sh.elite.size() < Shared::ELITE_MAX) { sh.elite.push_back(s); return; }
    int worst = 0;
    for (int i = 1; i < (int)sh.elite.size(); ++i)
        if (sh.elite[i].cmax > sh.elite[worst].cmax) worst = i;
    if (s.cmax < sh.elite[worst].cmax) sh.elite[worst] = s;
}

static bool getElite(Shared& sh, Rng& rng, Sol& out) {
    std::lock_guard<std::mutex> lk(sh.mtx);
    if (sh.elite.empty()) return false;
    out = sh.elite[rng((int)sh.elite.size())];
    return true;
}

static void report(Shared& sh, const Inst& in, const Sol& s) {
    if (s.cmax >= sh.best.load(std::memory_order_relaxed)) return;
    std::lock_guard<std::mutex> lk(sh.mtx);
    if (s.cmax >= sh.best.load()) return;
    sh.best.store(s.cmax);
    sh.bestSol = s;
    sh.haveBest = true;
    double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - sh.t0).count();
    sh.bestTime = el;
    sh.trace.push_back({el, s.cmax});
    writeSchedule(in, s, sh.schedPath);                 // continuous checkpoint
    std::ofstream pf(sh.progPath, std::ios::app);
    pf << el << ',' << s.cmax << "\n";
    fprintf(stderr, "[%7.2fs] makespan %d\n", el, s.cmax);
}

// --------------------------------- worker ---------------------------------

static void worker(const Inst& in, Shared& sh, uint64_t seed, double limitSec, int target, int tid) {
    Searcher S(in, seed);
    // Parameter diversity across threads: identical settings on 32 cores just
    // buys 32 correlated searches.  Each thread gets its own tenure regime,
    // stagnation patience and perturbation strength.
    const int tenureLo   = 6 + (tid % 5) * 4;                  // 6,10,14,18,22
    const int tenureSpan = 6 + (tid / 5 % 4) * 6;              // 6,12,18,24
    const double stagMul = 0.5 + 0.5 * (tid % 4);              // 0.5,1.0,1.5,2.0
    const int shareProb  = 25 + (tid % 4) * 20;                // 25,45,65,85
    const int ruinDiv    = 4 + (tid % 5) * 3;                  // 4,7,10,13,16
    Sol s, localBest, cur;
    S.initSol(s);

    // best of a few randomised dispatching-rule constructions
    {
        Sol tmp; int bc = INT_MAX;
        for (int t = 0; t < 12; ++t) {
            S.construct(tmp, t % 5, (t % 3) * 0.05);
            if (tmp.cmax < bc) { bc = tmp.cmax; s = tmp; }
        }
    }
    localBest = s;
    report(sh, in, s);

    auto elapsedNow = [&] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - sh.t0).count();
    };
    if (CFG.gtOnly) {                       // ablation: construction only
        Sol tmp;
        while (!sh.stop.load() && elapsedNow() < limitSec) {
            for (int t = 0; t < 20; ++t) {
                S.construct(tmp, S.rng(5), (S.rng(3)) * 0.05);
                if (tmp.cmax < localBest.cmax) { localBest = tmp; report(sh, in, tmp); }
            }
        }
        return;
    }

    std::vector<int> saveA, saveB;
    std::vector<Searcher::Cand> cands;
    long long iter = 0;
    long long sinceImp = 0;
    const long long stagLimit = (long long)((2000 + (long long)in.nOps * 12) * stagMul);
    int tenureBase = tenureLo + S.rng(tenureSpan);

    auto elapsed = [&] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - sh.t0).count();
    };

    while (!sh.stop.load(std::memory_order_relaxed)) {
        if ((iter & 255) == 0) {
            if (elapsed() > limitSec) break;
            if (target > 0 && sh.best.load() <= target) { /* keep going: record curve */ }
        }
        ++iter;

        // ---- critical operations
        S.critOps.clear();
        for (int o = 0; o < in.nOps; ++o)
            if (s.r[o] + s.pcur[o] + s.q[o] == s.cmax) S.critOps.push_back(o);
        if (S.critOps.empty()) break;

        // ---- scan the MG neighbourhood, keep the best few candidates
        cands.clear();
        int gbest = sh.best.load(std::memory_order_relaxed);
        int bestEst = INT_MAX;
        for (int v : S.critOps) {
            if (in.elig[v].size() == 1 && s.seq[s.mach[v]].size() == 1) continue;
            S.scanInsertions(s, v, [&](int k, int at, int est) {
                bool isTabu = !CFG.noTabu && S.tabu[(size_t)v * in.nMach + s.mach[v]] > iter;
                if (isTabu && est >= gbest) return;              // aspiration
                if (CFG.exactEval) { cands.push_back({v, k, at, 0}); return; }
                if ((int)cands.size() < 12 || est < bestEst + 4) {
                    cands.push_back({v, k, at, est});
                    bestEst = std::min(bestEst, est);
                }
            });
        }
        if (CFG.exactEval) {
            // Ablation: score every neighbour with a full O(n) re-decode instead
            // of the O(1) Mastrolilli-Gambardella estimate.
            for (auto& c : cands) {
                int oldM, oldPos;
                if (S.tryApply(s, c.v, c.k, c.at, saveA, saveB, oldM, oldPos)) {
                    c.est = s.cmax;
                    int backAt = oldPos;
                    S.removeOp(s, c.v);
                    S.insertOp(s, c.v, oldM, std::min(backAt, (int)s.seq[oldM].size()));
                    S.evaluate(s);
                } else c.est = INT_MAX;
            }
        }
        if (cands.empty()) {  // everything tabu -> take a random move
            int v = S.critOps[S.rng((int)S.critOps.size())];
            S.scanInsertions(s, v, [&](int k, int at, int est) { cands.push_back({v, k, at, est}); });
            if (cands.empty()) break;
        }
        std::sort(cands.begin(), cands.end(),
                  [](const Searcher::Cand& a, const Searcher::Cand& b) { return a.est < b.est; });

        // ---- apply the best feasible candidate
        bool applied = false;
        int tries = std::min<size_t>(cands.size(), 8);
        for (int t = 0; t < tries; ++t) {
            const Searcher::Cand& c = cands[t];
            int oldM, oldPos;
            if (c.est == INT_MAX) continue;
            if (S.tryApply(s, c.v, c.k, c.at, saveA, saveB, oldM, oldPos)) {
                if (!CFG.noTabu)
                    S.tabu[(size_t)c.v * in.nMach + oldM] = (int)(iter + tenureBase + S.rng(tenureBase + 4));
                applied = true;
                break;
            }
        }
        if (!applied) { ++sinceImp; continue; }

        if (s.cmax < localBest.cmax) {
            localBest = s; sinceImp = 0; report(sh, in, s);
            if (!CFG.noShare) addElite(sh, s);
        } else ++sinceImp;

        // ---- diversification
        if (sinceImp > stagLimit) {
            int roll = S.rng(100);
            Sol tmpA, tmpB;
            if (!CFG.noShare && roll < 18 && getElite(sh, S.rng, tmpA) && getElite(sh, S.rng, tmpB)) {
                Sol child;                            // recombine two elites
                S.recombine(tmpA, tmpB, child);
                s = (child.cmax != INT_MAX) ? child : localBest;
            } else if (!CFG.noShare && roll < 18 + shareProb && getElite(sh, S.rng, tmpA)) {
                s = tmpA;                             // restart from a random elite
            } else {
                s = localBest;
            }
            int strength = 1 + S.rng(std::max(2, in.nJobs / ruinDiv));
            if (CFG.noRR) S.randomKick(s, strength * 4);
            else          S.ruinRecreate(s, strength, S.rng(100) < 35);
            std::fill(S.tabu.begin(), S.tabu.end(), -1);
            tenureBase = tenureLo + S.rng(tenureSpan);
            sinceImp = 0;
            if (s.cmax < localBest.cmax) { localBest = s; report(sh, in, s); if (!CFG.noShare) addElite(sh, s); }
            // occasionally accept a full fresh restart to keep diversity
            if (S.rng(100) < 6) {
                Sol tmp; int bc = INT_MAX;
                for (int t = 0; t < 6; ++t) { S.construct(tmp, S.rng(5), 0.05); if (tmp.cmax < bc) { bc = tmp.cmax; s = tmp; } }
                localBest = s;
            }
        }
        sh.moveEvals.fetch_add((long long)cands.size(), std::memory_order_relaxed);
    }
    sh.iters.fetch_add(iter, std::memory_order_relaxed);
}

// ---------------------------------- main ----------------------------------

int main(int argc, char** argv) {
    if (argc < 6) {
        fprintf(stderr,
                "usage: %s <instance> <seconds> <threads> <out_schedule> <out_progress_csv> [target] [seed]\n",
                argv[0]);
        return 1;
    }
    std::string ipath = argv[1];
    double limit = atof(argv[2]);
    int nthreads = atoi(argv[3]);
    std::string schedPath = argv[4], progPath = argv[5];
    int target = (argc > 6) ? atoi(argv[6]) : 0;
    uint64_t seed = (argc > 7) ? strtoull(argv[7], nullptr, 10) : 12345;

    Inst in = parseInstance(ipath);

    Shared sh;
    sh.t0 = std::chrono::steady_clock::now();
    sh.schedPath = schedPath; sh.progPath = progPath;
    { std::ofstream pf(progPath); pf << "seconds,makespan\n"; }

    std::vector<std::thread> th;
    for (int t = 0; t < nthreads; ++t)
        th.emplace_back(worker, std::cref(in), std::ref(sh), seed + 0x9E3779B9ULL * (t + 1), limit, target, t);
    for (auto& x : th) x.join();

    double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - sh.t0).count();
    printf("instance %s\nbest_makespan %d\ntime_to_best %.2f\nthreads %d\nlimit %.0f\n"
           "total_iterations %lld\niterations_per_sec_total %.0f\nkept_candidates %lld\n",
           in.name.c_str(), sh.best.load(), sh.bestTime, nthreads, limit,
           sh.iters.load(), sh.iters.load() / std::max(1e-9, wall), sh.moveEvals.load());
    fflush(stdout);
    return 0;
}
