// Simulated annealing on an explicit disjunctive-graph representation of the
// flexible job shop with transport vehicles (FJSPT / FJSP-AGV).
//
// Model (verified by replaying Meng et al. 2023 solutions):
//  * V identical vehicles start at L/U (location 0) at time 0, unit capacity.
//  * Every job starts at L/U; op k of a job needs a transport from the machine of
//    op k-1 (or L/U for k = 0) unless both ops use the same machine.
//  * A vehicle travels empty from its last drop-off to the pickup, waits for the
//    job to finish its previous op, then carries it (loaded time T[pick][drop]).
//  * Machines process one op at a time, unlimited buffers, no return to L/U.
//  * Makespan = max completion time of all ops.
// All input times are pre-scaled by 2 so that the half-unit layouts are integral.
#include <bits/stdc++.h>
using namespace std;

static const int MAXN = 256, MAXM = 20, MAXV = 4;

struct Inst {
    string name;
    int J = 0, M = 0, N = 0, V = 2;
    vector<int> jobOf, idxOf, prevOp, nextOp, jobFirst, jobLen;
    vector<vector<pair<int, int>>> alts;  // (machine 1..M, ptime)
    int P[MAXN][MAXM + 1];
    int T[MAXM + 1][MAXM + 1];
};

static Inst I;

static void readInst(const string& fn) {
    ifstream in(fn);
    if (!in) { fprintf(stderr, "cannot open %s\n", fn.c_str()); exit(1); }
    in >> I.name >> I.J >> I.M;
    I.jobFirst.assign(I.J, 0); I.jobLen.assign(I.J, 0);
    for (int j = 0; j < I.J; j++) {
        int n; in >> n;
        I.jobFirst[j] = I.N; I.jobLen[j] = n;
        for (int k = 0; k < n; k++) {
            int a; in >> a;
            vector<pair<int, int>> v;
            for (int t = 0; t < a; t++) { int m, p; in >> m >> p; v.push_back({m, p}); }
            I.alts.push_back(v);
            I.jobOf.push_back(j); I.idxOf.push_back(k);
            I.prevOp.push_back(k ? I.N - 1 : -1);
            I.nextOp.push_back(-1);
            if (k) I.nextOp[I.N - 1] = I.N;
            I.N++;
        }
    }
    for (int i = 0; i <= I.M; i++) for (int j = 0; j <= I.M; j++) in >> I.T[i][j];
    for (int o = 0; o < I.N; o++) {
        for (int m = 0; m <= I.M; m++) I.P[o][m] = -1;
        for (auto& pr : I.alts[o]) I.P[o][pr.first] = pr.second;
    }
    if (I.N > MAXN || I.M > MAXM) { fprintf(stderr, "instance too large\n"); exit(1); }
}

struct Sol {
    int mach[MAXN];
    int veh[MAXN];
    int mlen[MAXM + 1];
    int mseq[MAXM + 1][MAXN];
    int vlen[MAXV];
    int vseq[MAXV][MAXN];
    int S[MAXN], C[MAXN], DEP[MAXN], ARR[MAXN];
    int cmax;
    double f;
    int crit[2 * MAXN]; int ncrit;  // critical path nodes (T-node: o, P-node: N+o)
};

static inline bool needTr(const Sol& s, int o) {
    int pr = I.prevOp[o];
    return pr < 0 || s.mach[pr] != s.mach[o];
}

static double SEC_W = 0.001;  // weight of the secondary objective
static int SEC_MODE = 0;       // 0: mean completion of jobs, 1: total vehicle travel, 2: sum of vehicle last drops + machine ends

// Longest-path evaluation. Returns false if the graph has a cycle.
static bool evaluate(Sol& s) {
    static int mpred[MAXN], mnext[MAXN], vpred[MAXN], vnext[MAXN];
    static int indeg[2 * MAXN], stk[2 * MAXN], bind[2 * MAXN];
    const int N = I.N;
    for (int o = 0; o < N; o++) mpred[o] = mnext[o] = vpred[o] = vnext[o] = -1;
    for (int m = 1; m <= I.M; m++)
        for (int i = 0; i < s.mlen[m]; i++) {
            int o = s.mseq[m][i];
            if (i) mpred[o] = s.mseq[m][i - 1];
            if (i + 1 < s.mlen[m]) mnext[o] = s.mseq[m][i + 1];
        }
    for (int v = 0; v < I.V; v++)
        for (int i = 0; i < s.vlen[v]; i++) {
            int o = s.vseq[v][i];
            if (i) vpred[o] = s.vseq[v][i - 1];
            if (i + 1 < s.vlen[v]) vnext[o] = s.vseq[v][i + 1];
        }
    int top = 0, total = 0;
    for (int o = 0; o < N; o++) {
        if (s.veh[o] >= 0) {
            indeg[o] = (I.idxOf[o] > 0) + (vpred[o] >= 0);
            total++;
            if (!indeg[o]) stk[top++] = o;
        }
        indeg[N + o] = 1 + (mpred[o] >= 0);
        total++;
    }
    int processed = 0;
    while (top) {
        int x = stk[--top];
        processed++;
        if (x < N) {
            int o = x, pr = I.prevOp[o];
            int pick = pr >= 0 ? s.mach[pr] : 0, drop = s.mach[o];
            int ready = pr >= 0 ? s.C[pr] : 0;
            int vp = vpred[o];
            int varr = vp >= 0 ? s.ARR[vp] + I.T[s.mach[vp]][pick] : I.T[0][pick];
            if (ready >= varr) { s.DEP[o] = ready; bind[x] = pr >= 0 ? N + pr : -1; }
            else { s.DEP[o] = varr; bind[x] = vp >= 0 ? vp : -1; }
            s.ARR[o] = s.DEP[o] + I.T[pick][drop];
            if (--indeg[N + o] == 0) stk[top++] = N + o;
            if (vnext[o] >= 0 && --indeg[vnext[o]] == 0) stk[top++] = vnext[o];
        } else {
            int o = x - N;
            int st, b;
            if (s.veh[o] >= 0) { st = s.ARR[o]; b = o; }
            else { st = s.C[I.prevOp[o]]; b = N + I.prevOp[o]; }
            int mp = mpred[o];
            if (mp >= 0 && s.C[mp] > st) { st = s.C[mp]; b = N + mp; }
            s.S[o] = st; s.C[o] = st + I.P[o][s.mach[o]]; bind[x] = b;
            int nx = I.nextOp[o];
            if (nx >= 0) {
                if (s.veh[nx] >= 0) { if (--indeg[nx] == 0) stk[top++] = nx; }
                else { if (--indeg[N + nx] == 0) stk[top++] = N + nx; }
            }
            if (mnext[o] >= 0 && --indeg[N + mnext[o]] == 0) stk[top++] = N + mnext[o];
        }
    }
    if (processed < total) return false;
    int cm = 0, arg = 0; double sec = 0;
    for (int j = 0; j < I.J; j++) {
        int o = I.jobFirst[j] + I.jobLen[j] - 1;
        sec += s.C[o];
        if (s.C[o] > cm) { cm = s.C[o]; arg = o; }
    }
    s.cmax = cm;
    if (SEC_MODE == 0) s.f = cm + SEC_W * sec / I.J;
    else if (SEC_MODE == 1) {
        double tr = 0;
        for (int v = 0; v < I.V; v++) {
            int loc = 0;
            for (int i = 0; i < s.vlen[v]; i++) {
                int o = s.vseq[v][i], pr = I.prevOp[o];
                int pick = pr >= 0 ? s.mach[pr] : 0;
                tr += I.T[loc][pick] + I.T[pick][s.mach[o]];
                loc = s.mach[o];
            }
        }
        s.f = cm + SEC_W * tr;
    } else {
        double e = 0;
        for (int v = 0; v < I.V; v++) if (s.vlen[v]) e += s.ARR[s.vseq[v][s.vlen[v] - 1]];
        for (int m = 1; m <= I.M; m++) if (s.mlen[m]) e += s.C[s.mseq[m][s.mlen[m] - 1]];
        s.f = cm + SEC_W * e / (I.V + I.M);
    }
    // critical path
    s.ncrit = 0;
    int x = N + arg;
    while (x >= 0 && s.ncrit < 2 * MAXN) { s.crit[s.ncrit++] = x; x = bind[x]; }
    return true;
}

// ---------------------------------------------------------------- utilities
static mt19937_64 rng;
static inline int rnd(int n) { return (int)(rng() % (uint64_t)n); }
static inline double urand() { return (rng() >> 11) * (1.0 / 9007199254740992.0); }

static inline int findPos(const int* a, int len, int x) {
    for (int i = 0; i < len; i++) if (a[i] == x) return i;
    return -1;
}
static inline void eraseAt(int* a, int& len, int pos) {
    memmove(a + pos, a + pos + 1, sizeof(int) * (len - pos - 1)); len--;
}
static inline void insertAt(int* a, int& len, int pos, int x) {
    memmove(a + pos + 1, a + pos, sizeof(int) * (len - pos)); a[pos] = x; len++;
}

// position in a machine sequence consistent with a start time
static int posByTime(const int* a, int len, const int* key, int t) {
    int i = 0;
    while (i < len && key[a[i]] <= t) i++;
    return i;
}

static void addTransport(Sol& s, int o, int v, int key) {
    int pos = posByTime(s.vseq[v], s.vlen[v], s.DEP, key);
    pos += rnd(3) - 1;
    pos = max(0, min(pos, s.vlen[v]));
    insertAt(s.vseq[v], s.vlen[v], pos, o);
    s.veh[o] = v;
}
static void removeTransport(Sol& s, int o) {
    int v = s.veh[o];
    int p = findPos(s.vseq[v], s.vlen[v], o);
    eraseAt(s.vseq[v], s.vlen[v], p);
    s.veh[o] = -1;
}
static void fixTransport(Sol& s, int o) {
    bool need = needTr(s, o), has = s.veh[o] >= 0;
    if (need && !has) {
        int pr = I.prevOp[o];
        int key = pr >= 0 ? s.C[pr] : 0;
        addTransport(s, o, rnd(I.V), key);
    } else if (!need && has) {
        removeTransport(s, o);
    }
}

// Build a solution by decoding a job sequence (IGA-style: append, greedy earliest vehicle).
static void buildFromOS(Sol& s, const vector<int>& os, const vector<int>& machOf) {
    for (int m = 0; m <= I.M; m++) s.mlen[m] = 0;
    for (int v = 0; v < I.V; v++) s.vlen[v] = 0;
    vector<int> cnt(I.J, 0);
    vector<double> tv(I.V, 0); vector<int> lv(I.V, 0);
    vector<int> cm(I.M + 1, 0), cj(I.J, 0), locj(I.J, 0);
    for (int g : os) {
        int o = I.jobFirst[g] + cnt[g]++;
        int m = machOf[o];
        s.mach[o] = m;
        s.mseq[m][s.mlen[m]++] = o;
        int prev = locj[g];
        int arrive;
        if (I.idxOf[o] > 0 && prev == m) {
            s.veh[o] = -1; arrive = cj[g];
        } else {
            int best = 0; double bt = 1e18;
            for (int v = 0; v < I.V; v++) {
                double t = tv[v] + I.T[lv[v]][prev] + I.T[prev][m];
                if (t < bt) { bt = t; best = v; }
            }
            int v = best;
            double d = max(tv[v] + I.T[lv[v]][prev], (double)cj[g]);
            tv[v] = d + I.T[prev][m]; lv[v] = m;
            s.veh[o] = v; s.vseq[v][s.vlen[v]++] = o;
            arrive = (int)tv[v];
        }
        int c = max(cm[m], arrive) + I.P[o][m];
        cm[m] = c; cj[g] = c; locj[g] = m;
    }
}

static void randomInit(Sol& s) {
    vector<int> os;
    for (int j = 0; j < I.J; j++) for (int k = 0; k < I.jobLen[j]; k++) os.push_back(j);
    shuffle(os.begin(), os.end(), rng);
    vector<int> machOf(I.N);
    // machine choice: keep the previous op's machine when eligible (w.p. 0.5), else random
    for (int o = 0; o < I.N; o++) {
        int pr = I.prevOp[o];
        if (pr >= 0 && I.P[o][machOf[pr]] >= 0 && rnd(2)) machOf[o] = machOf[pr];
        else machOf[o] = I.alts[o][rnd(I.alts[o].size())].first;
    }
    buildFromOS(s, os, machOf);
}

// ---------------------------------------------------------------- moves
static double PCRIT = 0.7;

static int pickOp(const Sol& s, bool wantTransport, bool wantFlexible) {
    for (int tries = 0; tries < 20; tries++) {
        int o;
        if (urand() < PCRIT && s.ncrit > 0) {
            int x = s.crit[rnd(s.ncrit)];
            o = x >= I.N ? x - I.N : x;
            if (wantTransport && x >= I.N) {
                // prefer the transport node of this op if it exists
            }
        } else o = rnd(I.N);
        if (wantTransport && s.veh[o] < 0) continue;
        if (wantFlexible && I.alts[o].size() < 2) continue;
        return o;
    }
    return -1;
}

static bool moveMachineSeq(Sol& s) {
    int o = pickOp(s, false, false);
    if (o < 0) return false;
    int m = s.mach[o];
    if (s.mlen[m] < 2) return false;
    int p = findPos(s.mseq[m], s.mlen[m], o);
    int w = urand() < 0.8 ? 2 : s.mlen[m];
    int np = p + (rnd(2 * w + 1) - w);
    np = max(0, min(np, s.mlen[m] - 1));
    if (np == p) return false;
    eraseAt(s.mseq[m], s.mlen[m], p);
    insertAt(s.mseq[m], s.mlen[m], np, o);
    return true;
}

static bool moveReassign(Sol& s) {
    int o = pickOp(s, false, true);
    if (o < 0) return false;
    int m = s.mach[o];
    int nm;
    do { nm = I.alts[o][rnd(I.alts[o].size())].first; } while (nm == m);
    int p = findPos(s.mseq[m], s.mlen[m], o);
    eraseAt(s.mseq[m], s.mlen[m], p);
    int np = posByTime(s.mseq[nm], s.mlen[nm], s.S, s.S[o]) + rnd(3) - 1;
    np = max(0, min(np, s.mlen[nm]));
    insertAt(s.mseq[nm], s.mlen[nm], np, o);
    s.mach[o] = nm;
    fixTransport(s, o);
    if (I.nextOp[o] >= 0) fixTransport(s, I.nextOp[o]);
    return true;
}

// put two consecutive ops of a job on a common machine (removes a transport)
static bool moveChain(Sol& s) {
    int o = pickOp(s, false, false);
    if (o < 0) return false;
    int a = o, b = I.nextOp[o];
    if (b < 0) { b = o; a = I.prevOp[o]; if (a < 0) return false; }
    if (s.mach[a] == s.mach[b]) return false;
    int cands[MAXM], nc = 0;
    for (auto& pr : I.alts[a]) if (I.P[b][pr.first] >= 0) cands[nc++] = pr.first;
    if (!nc) return false;
    int m = cands[rnd(nc)];
    for (int x : {a, b}) {
        if (s.mach[x] == m) continue;
        int om = s.mach[x];
        int p = findPos(s.mseq[om], s.mlen[om], x);
        eraseAt(s.mseq[om], s.mlen[om], p);
        int np = posByTime(s.mseq[m], s.mlen[m], s.S, s.S[x]);
        // keep a before b on the shared machine
        if (x == b) {
            int pa = findPos(s.mseq[m], s.mlen[m], a);
            if (np <= pa) np = pa + 1;
        } else {
            int pb = findPos(s.mseq[m], s.mlen[m], b);
            if (pb >= 0 && np > pb) np = pb;
        }
        insertAt(s.mseq[m], s.mlen[m], np, x);
        s.mach[x] = m;
    }
    fixTransport(s, a);
    fixTransport(s, b);
    if (I.nextOp[b] >= 0) fixTransport(s, I.nextOp[b]);
    return true;
}

static bool moveVehicle(Sol& s) {
    int o = pickOp(s, true, false);
    if (o < 0) return false;
    int v = s.veh[o];
    int p = findPos(s.vseq[v], s.vlen[v], o);
    if (I.V > 1 && urand() < 0.5) {
        int nv; do { nv = rnd(I.V); } while (nv == v);
        eraseAt(s.vseq[v], s.vlen[v], p);
        int np = posByTime(s.vseq[nv], s.vlen[nv], s.DEP, s.DEP[o]) + rnd(3) - 1;
        np = max(0, min(np, s.vlen[nv]));
        insertAt(s.vseq[nv], s.vlen[nv], np, o);
        s.veh[o] = nv;
    } else {
        if (s.vlen[v] < 2) return false;
        int w = urand() < 0.8 ? 2 : s.vlen[v];
        int np = p + (rnd(2 * w + 1) - w);
        np = max(0, min(np, s.vlen[v] - 1));
        if (np == p) return false;
        eraseAt(s.vseq[v], s.vlen[v], p);
        insertAt(s.vseq[v], s.vlen[v], np, o);
    }
    return true;
}

static bool moveTailSwap(Sol& s) {
    if (I.V < 2) return false;
    int a = rnd(I.V), b; do { b = rnd(I.V); } while (b == a);
    if (s.vlen[a] < 1 || s.vlen[b] < 1) return false;
    int i = rnd(s.vlen[a] + 1);
    int t = i < s.vlen[a] ? s.DEP[s.vseq[a][i]] : (s.vlen[a] ? s.DEP[s.vseq[a][s.vlen[a] - 1]] + 1 : 0);
    int j = posByTime(s.vseq[b], s.vlen[b], s.DEP, t - 1) + rnd(3) - 1;
    j = max(0, min(j, s.vlen[b]));
    static int ta[MAXN], tb[MAXN];
    int na = s.vlen[a] - i, nb = s.vlen[b] - j;
    if (na == 0 && nb == 0) return false;
    memcpy(ta, s.vseq[a] + i, sizeof(int) * na);
    memcpy(tb, s.vseq[b] + j, sizeof(int) * nb);
    memcpy(s.vseq[a] + i, tb, sizeof(int) * nb);
    memcpy(s.vseq[b] + j, ta, sizeof(int) * na);
    s.vlen[a] = i + nb; s.vlen[b] = j + na;
    for (int k = 0; k < nb; k++) s.veh[tb[k]] = a;
    for (int k = 0; k < na; k++) s.veh[ta[k]] = b;
    return true;
}

// swap two time-aligned tasks between two vehicles
static bool moveSwapBetween(Sol& s) {
    if (I.V < 2) return false;
    int o = pickOp(s, true, false);
    if (o < 0) return false;
    int a = s.veh[o], b; do { b = rnd(I.V); } while (b == a);
    if (s.vlen[b] < 1) return false;
    int pa = findPos(s.vseq[a], s.vlen[a], o);
    int pb = posByTime(s.vseq[b], s.vlen[b], s.DEP, s.DEP[o]) + rnd(3) - 2;
    pb = max(0, min(pb, s.vlen[b] - 1));
    int x = s.vseq[b][pb];
    s.vseq[a][pa] = x; s.vseq[b][pb] = o; s.veh[x] = a; s.veh[o] = b;
    return true;
}

// move a block of 1-3 consecutive tasks to the other vehicle (time-aligned)
static bool moveBlock(Sol& s) {
    if (I.V < 2) return false;
    int o = pickOp(s, true, false);
    if (o < 0) return false;
    int a = s.veh[o], b; do { b = rnd(I.V); } while (b == a);
    int pa = findPos(s.vseq[a], s.vlen[a], o);
    int len = 1 + rnd(3);
    len = min(len, s.vlen[a] - pa);
    static int blk[4];
    memcpy(blk, s.vseq[a] + pa, sizeof(int) * len);
    int pb = posByTime(s.vseq[b], s.vlen[b], s.DEP, s.DEP[o]) + rnd(3) - 1;
    pb = max(0, min(pb, s.vlen[b]));
    for (int k = 0; k < len; k++) eraseAt(s.vseq[a], s.vlen[a], pa);
    for (int k = 0; k < len; k++) { insertAt(s.vseq[b], s.vlen[b], pb + k, blk[k]); s.veh[blk[k]] = b; }
    return true;
}

// ---------------------------------------------------------------- output
static void writeSolution(const Sol& s, const string& fn) {
    FILE* f = fopen(fn.c_str(), "w");
    fprintf(f, "instance %s\nmakespan %.1f\n", I.name.c_str(), s.cmax / 2.0);
    fprintf(f, "# ops: job op machine start end   (job/op 1-based, times in original units)\n");
    for (int o = 0; o < I.N; o++)
        fprintf(f, "op %d %d %d %.1f %.1f\n", I.jobOf[o] + 1, I.idxOf[o] + 1, s.mach[o], s.S[o] / 2.0, s.C[o] / 2.0);
    fprintf(f, "# transports: job op vehicle from to depart arrive (location 0 = L/U)\n");
    for (int v = 0; v < I.V; v++)
        for (int i = 0; i < s.vlen[v]; i++) {
            int o = s.vseq[v][i], pr = I.prevOp[o];
            fprintf(f, "tr %d %d %d %d %d %.1f %.1f\n", I.jobOf[o] + 1, I.idxOf[o] + 1, v + 1,
                    pr >= 0 ? s.mach[pr] : 0, s.mach[o], s.DEP[o] / 2.0, s.ARR[o] / 2.0);
        }
    fclose(f);
}

// ---------------------------------------------------------------- main
int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s inst.txt [seed] [seconds] [out] [T0] [T1] [iters/run]\n", argv[0]); return 1; }
    readInst(argv[1]);
    uint64_t seed = argc > 2 ? atoll(argv[2]) : 1;
    double tlim = argc > 3 ? atof(argv[3]) : 60;
    string out = argc > 4 ? argv[4] : "best.sol";
    double T0 = argc > 5 ? atof(argv[5]) : 6.0, T1 = argc > 6 ? atof(argv[6]) : 0.3;
    long long itersPerRun = argc > 7 ? atoll(argv[7]) : 3000000;
    if (argc > 8) SEC_MODE = atoi(argv[8]);
    if (argc > 9) SEC_W = atof(argv[9]);
    double pTail = argc > 10 ? atof(argv[10]) : 0.05;
    rng.seed(seed * 7919 + 17);

    auto t0 = chrono::steady_clock::now();
    auto elapsed = [&]() { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); };

    static Sol cur, cand, best, runBest;
    do { randomInit(cur); } while (!evaluate(cur));
    best = cur;
    long long totalIt = 0, feasibleIt = 0;
    int run = 0;
    while (elapsed() < tlim) {
        if (run > 0) {
            // restart: from the global best (intensify) or a fresh random solution (diversify)
            if (urand() < 0.7) cur = best; else do { randomInit(cur); } while (!evaluate(cur));
        }
        runBest = cur;
        double temp = T0, alpha = pow(T1 / T0, 1.0 / itersPerRun);
        for (long long it = 0; it < itersPerRun; it++, totalIt++) {
            temp *= alpha;
            memcpy(&cand, &cur, sizeof(Sol));
            double r = urand(); bool ok;
            if (r < pTail) ok = moveTailSwap(cand);
            else {
                r = urand();
                if (r < 0.30) ok = moveMachineSeq(cand);
                else if (r < 0.55) ok = moveReassign(cand);
                else if (r < 0.65) ok = moveChain(cand);
                else if (r < 0.85) ok = moveVehicle(cand);
                else if (r < 0.93) ok = moveSwapBetween(cand);
                else ok = moveBlock(cand);
            }
            if (!ok || !evaluate(cand)) continue;
            feasibleIt++;
            double d = (cand.f - cur.f) * 2.0;  // in scaled units -> *2 keeps T in original units
            if (d <= 0 || urand() < exp(-d / (2.0 * temp))) {
                memcpy(&cur, &cand, sizeof(Sol));
                if (cur.f < runBest.f) runBest = cur;
                if (cur.f < best.f - 1e-9) {
                    best = cur;
                    if (best.cmax < runBest.cmax + 1) {}
                }
            }
            if ((it & 0xFFFF) == 0 && elapsed() > tlim) break;
        }
        run++;
        fprintf(stderr, "[%s seed %llu] run %d  t=%.0fs  runBest=%.1f  best=%.1f  it=%lld feas=%.2f\n",
                I.name.c_str(), (unsigned long long)seed, run, elapsed(), runBest.cmax / 2.0, best.cmax / 2.0,
                totalIt, totalIt ? (double)feasibleIt / totalIt : 0.0);
        writeSolution(best, out);
    }
    writeSolution(best, out);
    printf("%s %llu %.1f\n", I.name.c_str(), (unsigned long long)seed, best.cmax / 2.0);
    return 0;
}
