// FJSPT / FJSP-AGV solver, version 2: simulated annealing on explicit machine
// sequences + explicit vehicle routes, in-place moves with undo.
//
// Model (verified by replaying Meng et al. 2023 solutions):
//  * V identical unit-capacity vehicles start at L/U (location 0) at time 0.
//  * Every job starts at L/U; op k needs a transport from the machine of op k-1
//    (L/U for k = 0) unless both ops use the same machine.
//  * A vehicle drives empty from its last drop-off to the pickup, waits for the job
//    to finish its previous op, carries it (T[pick][drop]) and is then free.
//  * Machines: one op at a time, unlimited buffers, no return to L/U.
//  * Makespan = max completion time. Input times are pre-scaled by 2 (integral).
#include <bits/stdc++.h>
using namespace std;

static const int MAXN = 256, MAXM = 20, MAXV = 4;

struct Inst {
    string name;
    int J = 0, M = 0, N = 0, V = 2;
    vector<int> jobOf, idxOf, prevOp, nextOp, jobFirst, jobLen;
    vector<vector<pair<int, int>>> alts;
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
    if (I.N > MAXN || I.M > MAXM) { fprintf(stderr, "instance too large\n"); exit(1); }
    for (int o = 0; o < I.N; o++) {
        for (int m = 0; m <= I.M; m++) I.P[o][m] = -1;
        for (auto& pr : I.alts[o]) I.P[o][pr.first] = pr.second;
    }
}

struct Seqs {
    int mach[MAXN], veh[MAXN];
    int mlen[MAXM + 1], mseq[MAXM + 1][MAXN];
    int vlen[MAXV], vseq[MAXV][MAXN];
};
struct Timing {
    int S[MAXN], C[MAXN], DEP[MAXN], ARR[MAXN];
    int cmax; double f;
    int crit[2 * MAXN], ncrit;
};

static Seqs cur, best;
static Timing tA, tB, tBest;
static Timing *tc = &tA, *tn = &tB;

static double SEC_W = 0.001;
static int SEC_MODE = 0;

static inline bool needTr(const Seqs& s, int o) {
    int pr = I.prevOp[o];
    return pr < 0 || s.mach[pr] != s.mach[o];
}

static bool evaluate(const Seqs& s, Timing& t) {
    static int mpred[MAXN], mnext[MAXN], vpred[MAXN], vnext[MAXN];
    static int indeg[2 * MAXN], stk[2 * MAXN], bind[2 * MAXN];
    const int N = I.N;
    for (int o = 0; o < N; o++) mpred[o] = mnext[o] = vpred[o] = vnext[o] = -1;
    for (int m = 1; m <= I.M; m++) {
        const int* q = s.mseq[m]; int L = s.mlen[m];
        for (int i = 1; i < L; i++) { mpred[q[i]] = q[i - 1]; mnext[q[i - 1]] = q[i]; }
    }
    for (int v = 0; v < I.V; v++) {
        const int* q = s.vseq[v]; int L = s.vlen[v];
        for (int i = 1; i < L; i++) { vpred[q[i]] = q[i - 1]; vnext[q[i - 1]] = q[i]; }
    }
    int top = 0, total = 0;
    for (int o = 0; o < N; o++) {
        if (s.veh[o] >= 0 || s.veh[o] == -2) {
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
            int ready = pr >= 0 ? t.C[pr] : 0;
            int vp = vpred[o];
            int varr = vp >= 0 ? t.ARR[vp] + I.T[s.mach[vp]][pick] : (s.veh[o] == -2 ? 0 : I.T[0][pick]);
            if (ready >= varr) { t.DEP[o] = ready; bind[x] = pr >= 0 ? N + pr : -1; }
            else { t.DEP[o] = varr; bind[x] = vp >= 0 ? vp : -1; }
            t.ARR[o] = t.DEP[o] + I.T[pick][drop];
            if (--indeg[N + o] == 0) stk[top++] = N + o;
            if (vnext[o] >= 0 && --indeg[vnext[o]] == 0) stk[top++] = vnext[o];
        } else {
            int o = x - N, st, b;
            if (s.veh[o] != -1) { st = t.ARR[o]; b = o; }
            else { st = t.C[I.prevOp[o]]; b = N + I.prevOp[o]; }
            int mp = mpred[o];
            if (mp >= 0 && t.C[mp] > st) { st = t.C[mp]; b = N + mp; }
            t.S[o] = st; t.C[o] = st + I.P[o][s.mach[o]]; bind[x] = b;
            int nx = I.nextOp[o];
            if (nx >= 0) {
                if (s.veh[nx] != -1) { if (--indeg[nx] == 0) stk[top++] = nx; }
                else { if (--indeg[N + nx] == 0) stk[top++] = N + nx; }
            }
            if (mnext[o] >= 0 && --indeg[N + mnext[o]] == 0) stk[top++] = N + mnext[o];
        }
    }
    if (processed < total) return false;
    int cm = 0, arg = 0; double sec = 0;
    for (int j = 0; j < I.J; j++) {
        int o = I.jobFirst[j] + I.jobLen[j] - 1;
        sec += t.C[o];
        if (t.C[o] > cm) { cm = t.C[o]; arg = o; }
    }
    t.cmax = cm;
    if (SEC_MODE == 0) t.f = cm + SEC_W * sec / I.J;
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
        t.f = cm + SEC_W * tr;
    } else {
        double e = 0;
        for (int v = 0; v < I.V; v++) if (s.vlen[v]) e += t.ARR[s.vseq[v][s.vlen[v] - 1]];
        for (int m = 1; m <= I.M; m++) if (s.mlen[m]) e += t.C[s.mseq[m][s.mlen[m] - 1]];
        t.f = cm + SEC_W * e / (I.V + I.M);
    }
    t.ncrit = 0;
    int x = N + arg;
    while (x >= 0 && t.ncrit < 2 * MAXN) { t.crit[t.ncrit++] = x; x = bind[x]; }
    return true;
}

// ------------------------------------------------------------------ rng
static mt19937_64 rng;
static inline int rnd(int n) { return (int)(rng() % (uint64_t)n); }
static inline double urand() { return (rng() >> 11) * (1.0 / 9007199254740992.0); }

// ------------------------------------------------------------------ undo log
struct USeq { int kind, id, len; int seq[MAXN]; };
static USeq uSeq[12]; static int nUS = 0;
struct UOp { int o, mach, veh; };
static UOp uOp[2 * MAXN + 64]; static int nUO = 0;
static bool savedM[MAXM + 1], savedV[MAXV];

static void saveM(int m) {
    if (savedM[m]) return;
    savedM[m] = true; USeq& u = uSeq[nUS++];
    u.kind = 0; u.id = m; u.len = cur.mlen[m]; memcpy(u.seq, cur.mseq[m], sizeof(int) * u.len);
}
static void saveV(int v) {
    if (savedV[v]) return;
    savedV[v] = true; USeq& u = uSeq[nUS++];
    u.kind = 1; u.id = v; u.len = cur.vlen[v]; memcpy(u.seq, cur.vseq[v], sizeof(int) * u.len);
}
static void saveOp(int o) { uOp[nUO++] = {o, cur.mach[o], cur.veh[o]}; }
static void undoAll() {
    for (int i = nUS - 1; i >= 0; i--) {
        USeq& u = uSeq[i];
        if (u.kind == 0) { cur.mlen[u.id] = u.len; memcpy(cur.mseq[u.id], u.seq, sizeof(int) * u.len); savedM[u.id] = false; }
        else { cur.vlen[u.id] = u.len; memcpy(cur.vseq[u.id], u.seq, sizeof(int) * u.len); savedV[u.id] = false; }
    }
    for (int i = nUO - 1; i >= 0; i--) { cur.mach[uOp[i].o] = uOp[i].mach; cur.veh[uOp[i].o] = uOp[i].veh; }
    nUS = nUO = 0;
}
static void commitAll() {
    for (int i = 0; i < nUS; i++) { if (uSeq[i].kind == 0) savedM[uSeq[i].id] = false; else savedV[uSeq[i].id] = false; }
    nUS = nUO = 0;
}

// ------------------------------------------------------------------ sequence helpers
static inline int findPos(const int* a, int len, int x) {
    for (int i = 0; i < len; i++) if (a[i] == x) return i;
    return -1;
}
static inline void eraseAt(int* a, int& len, int pos) { memmove(a + pos, a + pos + 1, sizeof(int) * (len - pos - 1)); len--; }
static inline void insertAt(int* a, int& len, int pos, int x) { memmove(a + pos + 1, a + pos, sizeof(int) * (len - pos)); a[pos] = x; len++; }
static int posByTime(const int* a, int len, const int* key, int t) {
    int i = 0;
    while (i < len && key[a[i]] <= t) i++;
    return i;
}
static inline int clampi(int x, int lo, int hi) { return x < lo ? lo : (x > hi ? hi : x); }

static void addTransport(int o, int v, int key) {
    saveV(v); saveOp(o);
    int pos = clampi(posByTime(cur.vseq[v], cur.vlen[v], tc->DEP, key) + rnd(3) - 1, 0, cur.vlen[v]);
    insertAt(cur.vseq[v], cur.vlen[v], pos, o);
    cur.veh[o] = v;
}
static void removeTransport(int o) {
    int v = cur.veh[o];
    saveV(v); saveOp(o);
    eraseAt(cur.vseq[v], cur.vlen[v], findPos(cur.vseq[v], cur.vlen[v], o));
    cur.veh[o] = -1;
}
static void fixTransport(int o) {
    bool need = needTr(cur, o), has = cur.veh[o] >= 0;
    if (need && !has) {
        int pr = I.prevOp[o];
        addTransport(o, rnd(I.V), pr >= 0 ? tc->C[pr] : 0);
    } else if (!need && has) removeTransport(o);
}
static void moveOpToMachine(int o, int nm, int np) {
    int m = cur.mach[o];
    saveM(m); saveM(nm); saveOp(o);
    eraseAt(cur.mseq[m], cur.mlen[m], findPos(cur.mseq[m], cur.mlen[m], o));
    np = clampi(np, 0, cur.mlen[nm]);
    insertAt(cur.mseq[nm], cur.mlen[nm], np, o);
    cur.mach[o] = nm;
}

// ------------------------------------------------------------------ moves
static double PCRIT = 0.7;
static int pickOp(bool wantTransport, bool wantFlexible) {
    for (int tries = 0; tries < 30; tries++) {
        int o;
        if (urand() < PCRIT && tc->ncrit > 0) { int x = tc->crit[rnd(tc->ncrit)]; o = x >= I.N ? x - I.N : x; }
        else o = rnd(I.N);
        if (wantTransport && cur.veh[o] < 0) continue;
        if (wantFlexible && I.alts[o].size() < 2) continue;
        return o;
    }
    return -1;
}

static bool mvMachineSeq() {
    int o = pickOp(false, false); if (o < 0) return false;
    int m = cur.mach[o]; if (cur.mlen[m] < 2) return false;
    int p = findPos(cur.mseq[m], cur.mlen[m], o);
    int w = urand() < 0.8 ? 2 : cur.mlen[m];
    int np = clampi(p + rnd(2 * w + 1) - w, 0, cur.mlen[m] - 1);
    if (np == p) return false;
    saveM(m);
    eraseAt(cur.mseq[m], cur.mlen[m], p); insertAt(cur.mseq[m], cur.mlen[m], np, o);
    return true;
}
static bool mvReassign() {
    int o = pickOp(false, true); if (o < 0) return false;
    int m = cur.mach[o], nm;
    do { nm = I.alts[o][rnd(I.alts[o].size())].first; } while (nm == m);
    moveOpToMachine(o, nm, posByTime(cur.mseq[nm], cur.mlen[nm], tc->S, tc->S[o]) + rnd(3) - 1);
    fixTransport(o);
    if (I.nextOp[o] >= 0) fixTransport(I.nextOp[o]);
    return true;
}
static bool mvChain() {
    int o = pickOp(false, false); if (o < 0) return false;
    int a = o, b = I.nextOp[o];
    if (b < 0) { b = o; a = I.prevOp[o]; if (a < 0) return false; }
    if (cur.mach[a] == cur.mach[b]) return false;
    int cands[MAXM], nc = 0;
    for (auto& pr : I.alts[a]) if (I.P[b][pr.first] >= 0) cands[nc++] = pr.first;
    if (!nc) return false;
    int m = cands[rnd(nc)];
    if (cur.mach[a] != m) {
        int np = posByTime(cur.mseq[m], cur.mlen[m], tc->S, tc->S[a]);
        int pb = cur.mach[b] == m ? findPos(cur.mseq[m], cur.mlen[m], b) : -1;
        if (pb >= 0 && np > pb) np = pb;
        moveOpToMachine(a, m, np);
    }
    if (cur.mach[b] != m) {
        int np = posByTime(cur.mseq[m], cur.mlen[m], tc->S, tc->S[b]);
        int pa = findPos(cur.mseq[m], cur.mlen[m], a);
        if (np <= pa) np = pa + 1;
        moveOpToMachine(b, m, np);
    }
    fixTransport(a); fixTransport(b);
    if (I.nextOp[b] >= 0) fixTransport(I.nextOp[b]);
    return true;
}
static bool mvVehicle() {
    int o = pickOp(true, false); if (o < 0) return false;
    int v = cur.veh[o];
    int p = findPos(cur.vseq[v], cur.vlen[v], o);
    if (I.V > 1 && urand() < 0.5) {
        int nv; do { nv = rnd(I.V); } while (nv == v);
        saveV(v); saveV(nv); saveOp(o);
        eraseAt(cur.vseq[v], cur.vlen[v], p);
        int np = clampi(posByTime(cur.vseq[nv], cur.vlen[nv], tc->DEP, tc->DEP[o]) + rnd(3) - 1, 0, cur.vlen[nv]);
        insertAt(cur.vseq[nv], cur.vlen[nv], np, o);
        cur.veh[o] = nv;
    } else {
        if (cur.vlen[v] < 2) return false;
        int w = urand() < 0.8 ? 2 : cur.vlen[v];
        int np = clampi(p + rnd(2 * w + 1) - w, 0, cur.vlen[v] - 1);
        if (np == p) return false;
        saveV(v);
        eraseAt(cur.vseq[v], cur.vlen[v], p); insertAt(cur.vseq[v], cur.vlen[v], np, o);
    }
    return true;
}
static bool mvTailSwap() {
    if (I.V < 2) return false;
    int a = rnd(I.V), b; do { b = rnd(I.V); } while (b == a);
    if (cur.vlen[a] < 1 || cur.vlen[b] < 1) return false;
    int i = rnd(cur.vlen[a] + 1);
    int t = i < cur.vlen[a] ? tc->DEP[cur.vseq[a][i]] : tc->DEP[cur.vseq[a][cur.vlen[a] - 1]] + 1;
    int j = clampi(posByTime(cur.vseq[b], cur.vlen[b], tc->DEP, t - 1) + rnd(3) - 1, 0, cur.vlen[b]);
    static int ta[MAXN], tb[MAXN];
    int na = cur.vlen[a] - i, nb = cur.vlen[b] - j;
    if (na == 0 && nb == 0) return false;
    saveV(a); saveV(b);
    memcpy(ta, cur.vseq[a] + i, sizeof(int) * na);
    memcpy(tb, cur.vseq[b] + j, sizeof(int) * nb);
    memcpy(cur.vseq[a] + i, tb, sizeof(int) * nb);
    memcpy(cur.vseq[b] + j, ta, sizeof(int) * na);
    cur.vlen[a] = i + nb; cur.vlen[b] = j + na;
    for (int k = 0; k < nb; k++) { saveOp(tb[k]); cur.veh[tb[k]] = a; }
    for (int k = 0; k < na; k++) { saveOp(ta[k]); cur.veh[ta[k]] = b; }
    return true;
}
static bool mvSwapBetween() {
    if (I.V < 2) return false;
    int o = pickOp(true, false); if (o < 0) return false;
    int a = cur.veh[o], b; do { b = rnd(I.V); } while (b == a);
    if (cur.vlen[b] < 1) return false;
    int pa = findPos(cur.vseq[a], cur.vlen[a], o);
    int pb = clampi(posByTime(cur.vseq[b], cur.vlen[b], tc->DEP, tc->DEP[o]) + rnd(3) - 2, 0, cur.vlen[b] - 1);
    int x = cur.vseq[b][pb];
    saveV(a); saveV(b); saveOp(o); saveOp(x);
    cur.vseq[a][pa] = x; cur.vseq[b][pb] = o; cur.veh[x] = a; cur.veh[o] = b;
    return true;
}
static bool mvBlock() {
    if (I.V < 2) return false;
    int o = pickOp(true, false); if (o < 0) return false;
    int a = cur.veh[o], b; do { b = rnd(I.V); } while (b == a);
    int pa = findPos(cur.vseq[a], cur.vlen[a], o);
    int len = min(1 + rnd(3), cur.vlen[a] - pa);
    int blk[4];
    memcpy(blk, cur.vseq[a] + pa, sizeof(int) * len);
    int pb = clampi(posByTime(cur.vseq[b], cur.vlen[b], tc->DEP, tc->DEP[o]) + rnd(3) - 1, 0, cur.vlen[b]);
    saveV(a); saveV(b);
    for (int k = 0; k < len; k++) eraseAt(cur.vseq[a], cur.vlen[a], pa);
    for (int k = 0; k < len; k++) { saveOp(blk[k]); insertAt(cur.vseq[b], cur.vlen[b], pb + k, blk[k]); cur.veh[blk[k]] = b; }
    return true;
}

// Chain-link move: create a zero-empty link. Either put transport t right after a
// task s that drops at t's pickup machine ("swap at the machine"), or put a task u
// whose pickup is t's drop machine right after t.
static int CL_WIN = 60;  // time window (scaled units) for link candidates
static bool mvChainLink() {
    int t = pickOp(true, false); if (t < 0) return false;
    static int cand[MAXN]; int nc = 0;
    int pr = I.prevOp[t];
    if (urand() < 0.5) {
        if (pr < 0) return false;                 // pickup at L/U: no task drops there
        int loc = cur.mach[pr], ready = tc->C[pr];
        for (int v = 0; v < I.V; v++) for (int i = 0; i < cur.vlen[v]; i++) {
            int x = cur.vseq[v][i];
            if (x != t && cur.mach[x] == loc && abs(tc->ARR[x] - ready) <= CL_WIN && I.jobOf[x] != I.jobOf[t]) cand[nc++] = x;
        }
        if (!nc) return false;
        int x = cand[rnd(nc)], v = cur.veh[x], vt = cur.veh[t];
        saveV(vt); saveV(v); saveOp(t);
        eraseAt(cur.vseq[vt], cur.vlen[vt], findPos(cur.vseq[vt], cur.vlen[vt], t));
        insertAt(cur.vseq[v], cur.vlen[v], findPos(cur.vseq[v], cur.vlen[v], x) + 1, t);
        cur.veh[t] = v;
    } else {
        int loc = cur.mach[t], arr = tc->ARR[t];
        for (int v = 0; v < I.V; v++) for (int i = 0; i < cur.vlen[v]; i++) {
            int u = cur.vseq[v][i], pu = I.prevOp[u];
            if (u != t && pu >= 0 && cur.mach[pu] == loc && abs(tc->C[pu] - arr) <= CL_WIN && I.jobOf[u] != I.jobOf[t]) cand[nc++] = u;
        }
        if (!nc) return false;
        int u = cand[rnd(nc)], vu = cur.veh[u], v = cur.veh[t];
        saveV(vu); saveV(v); saveOp(u);
        eraseAt(cur.vseq[vu], cur.vlen[vu], findPos(cur.vseq[vu], cur.vlen[vu], u));
        insertAt(cur.vseq[v], cur.vlen[v], findPos(cur.vseq[v], cur.vlen[v], t) + 1, u);
        cur.veh[u] = v;
    }
    return true;
}

// Ruin & recreate: unroute k transports that are close in time (seeded on the
// critical path), then reinsert them one by one at the best evaluated position.
static int RR_WIN = 6;
static bool mvRuinRecreate() {
    int o = pickOp(true, false); if (o < 0) return false;
    int k = 2 + rnd(5);
    // collect the k transports with departure closest to o's
    static int cand[MAXN]; int nc = 0;
    for (int v = 0; v < I.V; v++) for (int i = 0; i < cur.vlen[v]; i++) cand[nc++] = cur.vseq[v][i];
    int t0 = tc->DEP[o];
    partial_sort(cand, cand + min(k, nc), cand + nc, [&](int a, int b) { return abs(tc->DEP[a] - t0) < abs(tc->DEP[b] - t0); });
    k = min(k, nc);
    static int rem[8]; static int remDep[8];
    for (int i = 0; i < k; i++) { rem[i] = cand[i]; remDep[i] = tc->DEP[cand[i]]; }
    for (int i = 0; i < k; i++) {
        int x = rem[i], v = cur.veh[x];
        saveV(v); saveOp(x);
        eraseAt(cur.vseq[v], cur.vlen[v], findPos(cur.vseq[v], cur.vlen[v], x));
        cur.veh[x] = -2;
    }
    // reinsert in random order
    for (int i = k - 1; i > 0; i--) { int j = rnd(i + 1); swap(rem[i], rem[j]); swap(remDep[i], remDep[j]); }
    static Timing tt;
    for (int i = 0; i < k; i++) {
        int x = rem[i];
        double bestF = 1e18; int bv = -1, bp = -1;
        for (int v = 0; v < I.V; v++) {
            saveV(v);
            int c = posByTime(cur.vseq[v], cur.vlen[v], tc->DEP, remDep[i]);
            int lo = max(0, c - RR_WIN), hi = min(cur.vlen[v], c + RR_WIN);
            for (int p = lo; p <= hi; p++) {
                insertAt(cur.vseq[v], cur.vlen[v], p, x); cur.veh[x] = v;
                if (evaluate(cur, tt) && tt.f < bestF) { bestF = tt.f; bv = v; bp = p; }
                eraseAt(cur.vseq[v], cur.vlen[v], p); cur.veh[x] = -2;
            }
        }
        if (bv < 0) return true;  // leaves an unrouted task -> caller's evaluate rejects via check below
        insertAt(cur.vseq[bv], cur.vlen[bv], bp, x); cur.veh[x] = bv;
    }
    return true;
}

static double PM[8] = {0.05, 0.27, 0.23, 0.10, 0.20, 0.08, 0.07, 0.0};  // tail, mseq, reassign, chain, veh, swapB, block, ruin&recreate
static double PCL = 0.0;
static bool randomMove() {
    double r = urand();
    if (r < PCL) return mvChainLink();
    r = urand();
    if ((r -= PM[7]) < 0) {
        bool ok = mvRuinRecreate();
        for (int o = 0; o < I.N; o++) if (cur.veh[o] == -2) return false;  // not fully reinserted
        return ok;
    }
    if ((r -= PM[0]) < 0) return mvTailSwap();
    if ((r -= PM[1]) < 0) return mvMachineSeq();
    if ((r -= PM[2]) < 0) return mvReassign();
    if ((r -= PM[3]) < 0) return mvChain();
    if ((r -= PM[4]) < 0) return mvVehicle();
    if ((r -= PM[5]) < 0) return mvSwapBetween();
    return mvBlock();
}

// ------------------------------------------------------------------ construction / io
static void buildFromOS(const vector<int>& os, const vector<int>& machOf) {
    for (int m = 0; m <= I.M; m++) cur.mlen[m] = 0;
    for (int v = 0; v < I.V; v++) cur.vlen[v] = 0;
    vector<int> cnt(I.J, 0), cm(I.M + 1, 0), cj(I.J, 0), locj(I.J, 0), lv(I.V, 0);
    vector<long long> tv(I.V, 0);
    for (int g : os) {
        int o = I.jobFirst[g] + cnt[g]++, m = machOf[o];
        cur.mach[o] = m; cur.mseq[m][cur.mlen[m]++] = o;
        int prev = locj[g], arrive;
        if (I.idxOf[o] > 0 && prev == m) { cur.veh[o] = -1; arrive = cj[g]; }
        else {
            int bv = 0; long long bt = LLONG_MAX;
            for (int v = 0; v < I.V; v++) { long long t = tv[v] + I.T[lv[v]][prev] + I.T[prev][m]; if (t < bt) { bt = t; bv = v; } }
            long long d = max(tv[bv] + I.T[lv[bv]][prev], (long long)cj[g]);
            tv[bv] = d + I.T[prev][m]; lv[bv] = m;
            cur.veh[o] = bv; cur.vseq[bv][cur.vlen[bv]++] = o; arrive = (int)tv[bv];
        }
        int c = max(cm[m], arrive) + I.P[o][m];
        cm[m] = c; cj[g] = c; locj[g] = m;
    }
}
static void randomInit() {
    vector<int> os;
    for (int j = 0; j < I.J; j++) for (int k = 0; k < I.jobLen[j]; k++) os.push_back(j);
    shuffle(os.begin(), os.end(), rng);
    vector<int> machOf(I.N);
    for (int o = 0; o < I.N; o++) {
        int pr = I.prevOp[o];
        if (pr >= 0 && I.P[o][machOf[pr]] >= 0 && rnd(2)) machOf[o] = machOf[pr];
        else machOf[o] = I.alts[o][rnd(I.alts[o].size())].first;
    }
    buildFromOS(os, machOf);
}
// load a schedule written by writeSolution (or any file in that format)
static bool loadSolution(const string& fn) {
    ifstream in(fn);
    if (!in) return false;
    string tag; vector<array<double, 3>> mo(I.N, {0, 0, 0});  // machine, start
    vector<array<double, 2>> tr(I.N, {-1, 0});             // vehicle, depart
    string line;
    while (getline(in, line)) {
        istringstream ss(line); ss >> tag;
        if (tag == "op") { int j, k, m; double s, e; ss >> j >> k >> m >> s >> e; int o = I.jobFirst[j - 1] + k - 1; mo[o] = {(double)m, s, 0}; }
        else if (tag == "tr") { int j, k, v, a, b; double d, r; ss >> j >> k >> v >> a >> b >> d >> r; int o = I.jobFirst[j - 1] + k - 1; tr[o] = {(double)(v - 1), d}; }
    }
    for (int m = 0; m <= I.M; m++) cur.mlen[m] = 0;
    for (int v = 0; v < I.V; v++) cur.vlen[v] = 0;
    vector<pair<double, int>> order;
    for (int o = 0; o < I.N; o++) { cur.mach[o] = (int)mo[o][0]; order.push_back({mo[o][1] + 1e-6 * I.idxOf[o], o}); }
    sort(order.begin(), order.end());
    for (auto& pr : order) { int o = pr.second; cur.mseq[cur.mach[o]][cur.mlen[cur.mach[o]]++] = o; }
    vector<pair<double, int>> tord;
    for (int o = 0; o < I.N; o++) { cur.veh[o] = (int)tr[o][0]; if (cur.veh[o] >= 0) tord.push_back({tr[o][1], o}); }
    sort(tord.begin(), tord.end());
    for (auto& pr : tord) { int o = pr.second; cur.vseq[cur.veh[o]][cur.vlen[cur.veh[o]]++] = o; }
    for (int o = 0; o < I.N; o++) if (needTr(cur, o) != (cur.veh[o] >= 0)) { fprintf(stderr, "inconsistent transports in %s\n", fn.c_str()); return false; }
    return true;
}
static void writeSolution(const Seqs& s, const Timing& t, const string& fn) {
    string tmp = fn + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    fprintf(f, "instance %s\nmakespan %.1f\n", I.name.c_str(), t.cmax / 2.0);
    fprintf(f, "# ops: job op machine start end   (job/op 1-based, times in original units)\n");
    for (int o = 0; o < I.N; o++)
        fprintf(f, "op %d %d %d %.1f %.1f\n", I.jobOf[o] + 1, I.idxOf[o] + 1, s.mach[o], t.S[o] / 2.0, t.C[o] / 2.0);
    fprintf(f, "# transports: job op vehicle from to depart arrive (location 0 = L/U)\n");
    for (int v = 0; v < I.V; v++)
        for (int i = 0; i < s.vlen[v]; i++) {
            int o = s.vseq[v][i], pr = I.prevOp[o];
            fprintf(f, "tr %d %d %d %d %d %.1f %.1f\n", I.jobOf[o] + 1, I.idxOf[o] + 1, v + 1,
                    pr >= 0 ? s.mach[pr] : 0, s.mach[o], t.DEP[o] / 2.0, t.ARR[o] / 2.0);
        }
    fclose(f);
    rename(tmp.c_str(), fn.c_str());
}

// ------------------------------------------------------------------ main
int main(int argc, char** argv) {
    map<string, string> a;
    for (int i = 1; i + 1 < argc; i += 2) a[argv[i]] = argv[i + 1];
    auto get = [&](const string& k, const string& d) { return a.count(k) ? a[k] : d; };
    readInst(get("--inst", ""));
    uint64_t seed = stoull(get("--seed", "1"));
    double tlim = stod(get("--time", "60"));
    string out = get("--out", "best.sol");
    double T0 = stod(get("--T0", "6")), T1 = stod(get("--T1", "0.3"));
    long long iters = stoll(get("--iters", "3000000"));
    SEC_MODE = stoi(get("--sec", "0")); SEC_W = stod(get("--secw", "0.001"));
    PCRIT = stod(get("--pcrit", "0.7"));
    double pRestart = stod(get("--prestart", "0.3"));
    int kick = stoi(get("--kick", "0"));
    double target = stod(get("--target", "0"));  // stop when makespan <= target (original units)
    if (a.count("--pm")) { stringstream ss(a["--pm"]); for (int i = 0; i < 7; i++) { ss >> PM[i]; if (ss.peek() == ',') ss.ignore(); } }
    PM[7] = stod(get("--prr", "0"));
    PCL = stod(get("--pcl", "0"));
    CL_WIN = stoi(get("--clwin", "60"));
    RR_WIN = stoi(get("--rrwin", "6"));
    string init = get("--init", "");
    string share = get("--share", "");       // shared best-solution file (island model)
    double pShare = stod(get("--pshare", "0.5"));
    rng.seed(seed * 1000003ULL + 12345);

    auto t0 = chrono::steady_clock::now();
    auto elapsed = [&]() { return chrono::duration<double>(chrono::steady_clock::now() - t0).count(); };

    if (!init.empty()) {
        if (!loadSolution(init) || !evaluate(cur, *tc)) { fprintf(stderr, "bad init solution\n"); return 1; }
        fprintf(stderr, "[%s] init from %s: %.1f\n", I.name.c_str(), init.c_str(), tc->cmax / 2.0);
    } else {
        do { randomInit(); } while (!evaluate(cur, *tc));
    }
    auto sharedMakespan = [&](const string& fn) -> double {
        ifstream in(fn); string tag, nm; double mk = 1e18;
        if (!(in >> tag >> nm >> tag >> mk)) return 1e18;
        return mk;
    };
    best = cur; tBest = *tc;
    // ---------------------------------------------------------------- parallel tempering mode
    int K = stoi(get("--pt", "0"));
    if (K > 0) {
        double Tmin = stod(get("--tmin", "0.3")), Tmax = stod(get("--tmax", "4"));
        long long batch = stoll(get("--batch", "20000"));
        vector<Seqs> RS(K); vector<Timing> RT(K); vector<int> at(K);  // at[r] = state index at temperature r
        vector<double> temps(K);
        for (int r = 0; r < K; r++) temps[r] = Tmin * pow(Tmax / Tmin, K > 1 ? (double)r / (K - 1) : 0.0);
        for (int r = 0; r < K; r++) {
            if (r > 0 && init.empty()) { do { randomInit(); } while (!evaluate(cur, *tc)); }
            RS[r] = cur; RT[r] = *tc; at[r] = r;
            if (tc->f < tBest.f) { best = cur; tBest = *tc; }
        }
        vector<long long> tries(K, 0), acc(K, 0);
        long long sweeps = 0; double lastLog = 0;
        while (elapsed() < tlim) {
            for (int r = 0; r < K; r++) {
                int si = at[r];
                cur = RS[si]; *tc = RT[si];
                double temp = temps[r];
                for (long long it = 0; it < batch; it++) {
                    if (!randomMove()) { undoAll(); continue; }
                    if (!evaluate(cur, *tn)) { undoAll(); continue; }
                    double d = (tn->f - tc->f) * 0.5;
                    if (d <= 0 || urand() < exp(-d / temp)) {
                        swap(tc, tn); commitAll();
                        if (tc->f < tBest.f - 1e-12) { best = cur; tBest = *tc; }
                    } else undoAll();
                }
                RS[si] = cur; RT[si] = *tc;
            }
            for (int r = K - 2; r >= 0; r--) {
                double fr = RT[at[r]].f * 0.5, fs = RT[at[r + 1]].f * 0.5;
                double x = (fr - fs) * (1.0 / temps[r] - 1.0 / temps[r + 1]);
                tries[r]++;
                if (x >= 0 || urand() < exp(x)) { swap(at[r], at[r + 1]); acc[r]++; }
            }
            sweeps++;
            if (elapsed() - lastLog > 60) {
                lastLog = elapsed();
                fprintf(stderr, "[%s s%llu PT] t=%.0fs sweeps=%lld best=%.1f | cur:", I.name.c_str(), (unsigned long long)seed, elapsed(), sweeps, tBest.cmax / 2.0);
                for (int r = 0; r < K; r++) fprintf(stderr, " %.1f", RT[at[r]].cmax / 2.0);
                fprintf(stderr, " | swap acc:");
                for (int r = 0; r + 1 < K; r++) fprintf(stderr, " %.2f", tries[r] ? (double)acc[r] / tries[r] : 0.0);
                fprintf(stderr, "\n");
                writeSolution(best, tBest, out);
                if (!share.empty() && tBest.cmax / 2.0 < sharedMakespan(share) - 1e-9) writeSolution(best, tBest, share);
            }
        }
        writeSolution(best, tBest, out);
        printf("%s %llu %.1f\n", I.name.c_str(), (unsigned long long)seed, tBest.cmax / 2.0);
        return 0;
    }
    long long totalIt = 0, feas = 0;
    int run = 0;
    while (elapsed() < tlim) {
        if (run > 0 && !share.empty() && urand() < pShare) {
            double sm = sharedMakespan(share);
            if (sm < tBest.cmax / 2.0 - 1e-9) {
                Seqs keep = cur;
                if (loadSolution(share) && evaluate(cur, *tc)) {
                    best = cur; tBest = *tc;
                    fprintf(stderr, "[%s s%llu] adopted shared best %.1f\n", I.name.c_str(), (unsigned long long)seed, tBest.cmax / 2.0);
                } else cur = keep;
            }
        }
        if (run > 0) {
            if (urand() < pRestart) { do { randomInit(); } while (!evaluate(cur, *tc)); }
            else {
                cur = best; *tc = tBest;
                for (int k = 0; k < kick; k++) {  // random kick
                    if (randomMove() && evaluate(cur, *tn)) { swap(tc, tn); commitAll(); } else undoAll();
                }
            }
        }
        double runBest = tc->cmax;
        double temp = T0, alpha = pow(T1 / T0, 1.0 / iters);
        for (long long it = 0; it < iters; it++, totalIt++) {
            temp *= alpha;
            if (!randomMove()) { undoAll(); continue; }
            if (!evaluate(cur, *tn)) { undoAll(); continue; }
            feas++;
            double d = (tn->f - tc->f) * 0.5;  // original time units
            if (d <= 0 || urand() < exp(-d / temp)) {
                swap(tc, tn); commitAll();
                if (tc->cmax < runBest) runBest = tc->cmax;
                if (tc->f < tBest.f - 1e-12) { best = cur; tBest = *tc; }
            } else undoAll();
            if ((it & 0xFFFF) == 0 && elapsed() > tlim) break;
        }
        run++;
        fprintf(stderr, "[%s s%llu] run %d t=%.0fs runBest=%.1f best=%.1f it=%lld feas=%.2f\n", I.name.c_str(),
                (unsigned long long)seed, run, elapsed(), runBest / 2.0, tBest.cmax / 2.0, totalIt,
                totalIt ? (double)feas / totalIt : 0.0);
        writeSolution(best, tBest, out);
        if (!share.empty() && tBest.cmax / 2.0 < sharedMakespan(share) - 1e-9) writeSolution(best, tBest, share);
        if (target > 0 && tBest.cmax / 2.0 <= target) break;
    }
    writeSolution(best, tBest, out);
    printf("%s %llu %.1f\n", I.name.c_str(), (unsigned long long)seed, tBest.cmax / 2.0);
    return 0;
}
