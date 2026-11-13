/* Job-shop as a bounded-corridor CSP, with incremental overlap accounting.
 *
 * Same formulation as csp.c: fix a target makespan T, give op o of job j the hard
 * corridor  pref_j(o) <= st <= pref_j(o) + (T - L_j), keep the job chain feasible
 * at all times.  Makespan <= T then holds by construction and cannot inflate;
 * the only thing to search for is zero machine overlap.
 *
 * The difference here is cost per step.  csp.c re-sorted every touched machine
 * list (O(N) each, x4 candidate moves, x2 for apply/undo) and ran ~1.2k steps/s.
 * Here each machine list is kept permanently sorted and the total overlap TOT is
 * maintained incrementally: changing one op's start updates only the two or three
 * adjacent pairs around it, and since a typical shift (~300) is far smaller than
 * the mean spacing between ops on a machine (~5300), the op almost never crosses
 * a neighbour, so reordering is O(1) amortised.  A candidate move therefore costs
 * O(chain depth) instead of O(depth x N).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

static int N, M, NM;
static int *mach, *dur, *st, *opo;
static int *pref, *jlen, *slack;
static int *mlist, *mpos;
static int *bestst;
static long long bestmk = -1;
static int rawseed = 0;
static int DESCENT = 0;
static int ILS = 0, ILSRR = 30, ILSK = 4, ILSW = 1500, ILSACC = 0; static long long ILSDESC = 3000, RREVERY = 0;
static int LB, T;
static int verbose = 0, NOISE = 8;
static int MAXDEPTH = 60;

static long long TOT;        /* total machine overlap */
static long long *movl;      /* per-machine overlap */

static unsigned int rngs = 12345u;
static inline unsigned int rnd(void) {
    rngs ^= rngs << 13; rngs ^= rngs >> 17; rngs ^= rngs << 5; return rngs;
}
static inline int rndint(int n) { return (int)(rnd() % (unsigned)n); }
static double now(void) {
    struct timeval tv; gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}
static int *dlo = NULL, *dhi = NULL;   /* optional propagated per-op start domains */
static inline int LOv(int j, int o) { return dlo ? dlo[j * M + o] : pref[j * (M + 1) + o]; }
static inline int HIv(int j, int o) {
    int h = pref[j * (M + 1) + o] + slack[j];
    if (dhi && dhi[j * M + o] < h) h = dhi[j * M + o];
    return h;
}
/* clip the current state into the per-op domains keeping the job chains feasible */
static void clip_domains(void) {
    for (int j = 0; j < N; j++) {
        int base = j * M, lim = 1 << 30;
        for (int o = M - 1; o >= 0; o--) {
            int id = base + o, h = HIv(j, o);
            if (lim < h) h = lim;
            if (st[id] > h) st[id] = h;
            if (o) lim = st[id] - dur[id - 1];
        }
        int pe = 0;
        for (int o = 0; o < M; o++) {
            int id = base + o, l = LOv(j, o);
            if (pe > l) l = pe;
            if (st[id] < l) st[id] = l;
            pe = st[id] + dur[id];
        }
    }
}

/* ---- set of machines that currently have overlap ---- */
static int *cset, *cidx, cn;
static int *mw;
static inline void cadd(int k) { if (cidx[k] < 0) { cidx[k] = cn; cset[cn++] = k; } }
static inline void cdel(int k) {
    int p = cidx[k]; if (p < 0) return;
    int last = cset[--cn]; cset[p] = last; cidx[last] = p; cidx[k] = -1;
}

/* ---- incremental overlap ---- */
static inline int ovp(int a, int b) { int v = st[a] + dur[a] - st[b]; return v > 0 ? v : 0; }
static inline long long padj(int k, int i) {     /* overlap of machine-k pair (i, i+1) */
    if (i < 0 || i + 1 >= N) return 0;
    int *L = mlist + (size_t)k * N;
    return ovp(L[i], L[i + 1]);
}
static inline void bump(int k, long long d) {
    if (!d) return;
    movl[k] += d; TOT += d;
    if (movl[k] > 0) cadd(k); else cdel(k);
}
/* set st[id] = ns, restoring the sorted invariant and updating TOT/movl */
static void setst(int id, int ns) {
    int k = mach[id], i = mpos[id];
    int *L = mlist + (size_t)k * N;
    bump(k, -(padj(k, i - 1) + padj(k, i)));
    st[id] = ns;
    bump(k, padj(k, i - 1) + padj(k, i));
    while (i > 0 && st[L[i - 1]] > ns) {
        bump(k, -(padj(k, i - 2) + padj(k, i - 1) + padj(k, i)));
        int t = L[i - 1]; L[i - 1] = L[i]; L[i] = t;
        mpos[L[i - 1]] = i - 1; mpos[L[i]] = i;
        bump(k, padj(k, i - 2) + padj(k, i - 1) + padj(k, i));
        i--;
    }
    while (i + 1 < N && st[L[i + 1]] < ns) {
        bump(k, -(padj(k, i - 1) + padj(k, i) + padj(k, i + 1)));
        int t = L[i + 1]; L[i + 1] = L[i]; L[i] = t;
        mpos[L[i]] = i; mpos[L[i + 1]] = i + 1;
        bump(k, padj(k, i - 1) + padj(k, i) + padj(k, i + 1));
        i++;
    }
}

/* ---- undo log ---- */
static int *ulog_id, *ulog_old, ulog_n;
static inline void rec(int id) { ulog_id[ulog_n] = id; ulog_old[ulog_n] = st[id]; ulog_n++; }
static void undo(void) {
    for (int i = ulog_n - 1; i >= 0; i--) setst(ulog_id[i], ulog_old[i]);
    ulog_n = 0;
}

static int move_right(int j, int o, int s) {
    int base = j * M;
    for (int i = o; i < M; i++) {
        int id = base + i;
        if (st[id] >= s) return 1;
        if (s > HIv(j, i)) return 0;
        if (i - o >= MAXDEPTH) return 0;
        rec(id); setst(id, s);
        s += dur[id];
    }
    return 1;
}
static int move_left(int j, int o, int s) {
    int base = j * M;
    for (int i = o; i >= 0; i--) {
        int id = base + i;
        if (st[id] <= s) return 1;
        if (s < LOv(j, i)) return 0;
        if (o - i >= MAXDEPTH) return 0;
        rec(id); setst(id, s);
        if (i) s -= dur[id - 1];
    }
    return 1;
}

static void rebuild(void) {
    TOT = 0; cn = 0;
    for (int k = 0; k < M; k++) { cidx[k] = -1; movl[k] = 0; }
    for (int k = 0; k < M; k++) {
        int *L = mlist + (size_t)k * N;
        for (int i = 1; i < N; i++) {          /* insertion sort by start */
            int v = L[i], s = st[v], p = i - 1;
            while (p >= 0 && st[L[p]] > s) { L[p + 1] = L[p]; p--; }
            L[p + 1] = v;
        }
        for (int i = 0; i < N; i++) mpos[L[i]] = i;
        long long t = 0;
        for (int i = 0; i + 1 < N; i++) t += ovp(L[i], L[i + 1]);
        movl[k] = t; TOT += t;
        if (t > 0) cadd(k);
    }
}

static void read_instance(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    if (fscanf(f, "%d %d", &N, &M) != 2) exit(1);
    NM = N * M;
    mach = malloc(sizeof(int) * NM); dur = malloc(sizeof(int) * NM);
    st = malloc(sizeof(int) * NM);   bestst = malloc(sizeof(int) * NM);
    opo = malloc(sizeof(int) * NM);  mpos = malloc(sizeof(int) * NM);
    pref = malloc(sizeof(int) * N * (M + 1));
    jlen = malloc(sizeof(int) * N);  slack = malloc(sizeof(int) * N);
    for (int j = 0; j < N; j++) {
        pref[j * (M + 1)] = 0;
        for (int o = 0; o < M; o++) {
            int id = j * M + o;
            if (fscanf(f, "%d %d", &mach[id], &dur[id]) != 2) exit(1);
            opo[id] = o;
            pref[j * (M + 1) + o + 1] = pref[j * (M + 1) + o] + dur[id];
        }
        jlen[j] = pref[j * (M + 1) + M];
    }
    fclose(f);
    mlist = malloc(sizeof(int) * M * N);
    int *cnt = calloc(M, sizeof(int));
    for (int id = 0; id < NM; id++) { int k = mach[id]; mlist[(size_t)k * N + cnt[k]++] = id; }
    free(cnt);
    cset = malloc(sizeof(int) * M); cidx = malloc(sizeof(int) * M);
    movl = malloc(sizeof(long long) * M);
    mw = malloc(sizeof(int) * M);
    for (int k = 0; k < M; k++) mw[k] = 1;
    LB = 0;
    for (int j = 0; j < N; j++) if (jlen[j] > LB) LB = jlen[j];
}

static int check(int *S, long long *mkout) {
    long long mk = 0;
    for (int j = 0; j < N; j++) {
        for (int o = 0; o < M; o++) {
            int id = j * M + o;
            if (S[id] < 0) return 0;
            if (o && S[id] < S[id - 1] + dur[id - 1]) return 0;
        }
        int id = j * M + M - 1;
        if (S[id] + dur[id] > mk) mk = S[id] + dur[id];
    }
    int *tmp = malloc(sizeof(int) * N);
    for (int k = 0; k < M; k++) {
        for (int i = 0; i < N; i++) tmp[i] = mlist[(size_t)k * N + i];
        for (int i = 1; i < N; i++) {
            int v = tmp[i], s = S[v], p = i - 1;
            while (p >= 0 && S[tmp[p]] > s) { tmp[p + 1] = tmp[p]; p--; }
            tmp[p + 1] = v;
        }
        for (int i = 0; i + 1 < N; i++)
            if (S[tmp[i]] + dur[tmp[i]] > S[tmp[i + 1]]) { free(tmp); return 0; }
    }
    free(tmp);
    *mkout = mk;
    return 1;
}
static void write_solution(const char *path, int *S) {
    char tmp[512]; snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "w"); if (!f) return;
    char *buf = malloc(1 << 20);
    for (int j = 0; j < N; j++) {
        int p = 0;
        for (int o = 0; o < M; o++)
            p += sprintf(buf + p, "%d%c", S[j * M + o], o == M - 1 ? '\n' : ' ');
        fwrite(buf, 1, p, f);
    }
    free(buf); fclose(f); rename(tmp, path);
}

/* Escape mechanism.  When a conflict cannot be separated by any of the four
 * moves (every one violates a corridor), the search would otherwise spin on it
 * forever.  Displacing unrelated ops on the same machine changes which moves are
 * legal, so the blocked pair becomes solvable a few steps later. */
static void kick(int nops, int window) {
    if (cn == 0) return;
    int k = cset[rndint(cn)];
    int *L = mlist + (size_t)k * N;
    for (int t = 0; t < nops; t++) {
        int id = L[rndint(N)], j = id / M, o = opo[id];
        int lo = LOv(j, o), hi = HIv(j, o);
        if (hi <= lo) continue;
        int tgt = st[id] + (rndint(2 * window + 1) - window);
        if (tgt < lo) tgt = lo;
        if (tgt > hi) tgt = hi;
        ulog_n = 0;
        if (tgt > st[id]) { if (!move_right(j, o, tgt)) undo(); }
        else if (tgt < st[id]) { if (!move_left(j, o, tgt)) undo(); }
        ulog_n = 0;
    }
}

/* ---- whole-job exact reroute (negotiated congestion) ----
 * Remove nothing: job j's own ops are skipped when pricing.  DP over the delay
 * grid D in [0, slack_j] (step RR), cost = sum over ops of overlap with other
 * jobs' ops, each overlapped unit priced (1 + H[k][bucket]).  The DP returns the
 * cheapest monotone delay profile for the whole job, i.e. it moves delay budget
 * anywhere along the chain in one step. */
static int RR = 1, NBK = 0;
#define HBK 64
static float *Hh;            /* history price per (machine, bucket) */
static float *rf, *rc;
static unsigned short *rarg;
static int rmaxnd = 0;
static int *rns;
static long long nreroute = 0, nreroute_gain = 0;
static double HF = 0.05;
static inline int lbound_start(int k, int t) {   /* first index on machine k with st >= t */
    int *Lk = mlist + (size_t)k * N; int lo = 0, hi = N;
    while (lo < hi) { int md = (lo + hi) >> 1; if (st[Lk[md]] < t) lo = md + 1; else hi = md; }
    return lo;
}
static void reroute_alloc(void) {
    rmaxnd = 0;
    for (int j = 0; j < N; j++) if (slack[j] / RR + 1 > rmaxnd) rmaxnd = slack[j] / RR + 1;
    if (rmaxnd > 65535) { RR = (rmaxnd * RR) / 65535 + 1; rmaxnd = 0; for (int j = 0; j < N; j++) if (slack[j] / RR + 1 > rmaxnd) rmaxnd = slack[j] / RR + 1; }
    free(rf); free(rc); free(rarg);
    rf = malloc(sizeof(float) * rmaxnd); rc = malloc(sizeof(float) * rmaxnd);
    rarg = malloc(sizeof(unsigned short) * (size_t)M * rmaxnd);
    if (!rns) rns = malloc(sizeof(int) * M);
    if (!Hh) { NBK = (T + 4000) / HBK + 2; Hh = calloc((size_t)M * NBK, sizeof(float)); }
}
static void hist_bump(void) {   /* raise prices where machines overlap now */
    for (int c = 0; c < cn; c++) {
        int k = cset[c]; int *Lk = mlist + (size_t)k * N;
        for (int i = 0; i + 1 < N; i++) {
            int a = Lk[i], b = Lk[i + 1];
            int e = st[a] + dur[a];
            if (e > st[b]) {
                int hi = e < st[b] + dur[b] ? e : st[b] + dur[b];
                for (int q = st[b] / HBK; q <= (hi - 1) / HBK && q < NBK; q++) Hh[(size_t)k * NBK + q] += (float)HF;
            }
        }
    }
}
static long long job_overlap(int j) {
    long long t = 0;
    for (int o = 0; o < M; o++) {
        int id = j * M + o, k = mach[id], i = mpos[id];
        t += padj(k, i - 1) + padj(k, i);
    }
    return t;
}
static void reroute(int j) {
    int nd = slack[j] / RR + 1;
    float *f = rf, *c = rc;
    for (int o = 0; o < M; o++) {
        int id = j * M + o, k = mach[id], d = dur[id], t0 = pref[j * (M + 1) + o];
        int ilo0 = (LOv(j, o) - t0 + RR - 1) / RR, ihi0 = (HIv(j, o) - t0) / RR;
        for (int i = 0; i < nd; i++) c[i] = (i < ilo0 || i > ihi0) ? 1e30f : 0.f;
        int tmax = t0 + (nd - 1) * RR + d;
        int *Lk = mlist + (size_t)k * N;
        float *Hk = Hh + (size_t)k * NBK;
        for (int q = lbound_start(k, t0 - 1000); q < N; q++) {
            int x = Lk[q]; int s = st[x];
            if (s >= tmax) break;
            if (x == id) continue;
            int e = s + dur[x];
            if (e <= t0) continue;
            int ilo = s - d - t0 < 0 ? 0 : (s - d - t0) / RR;
            int ihi = (e - 1 - t0) / RR; if (ihi >= nd) ihi = nd - 1;
            for (int i = ilo; i <= ihi; i++) {
                int t = t0 + i * RR;
                int lo = t > s ? t : s, hi = t + d < e ? t + d : e;
                if (hi > lo) c[i] += (float)(hi - lo) * (1.f + Hk[lo / HBK]);
            }
        }
        unsigned short *ar = rarg + (size_t)o * rmaxnd;
        if (o == 0) { for (int i = 0; i < nd; i++) { f[i] = c[i]; ar[i] = i; } }
        else {
            float best = 3e38f; int bi = 0;
            for (int i = 0; i < nd; i++) {
                if (f[i] < best || (f[i] == best && (rnd() & 7) == 0)) { best = f[i]; bi = i; }
                ar[i] = (unsigned short)bi; f[i] = best + c[i];
            }
        }
    }
    float best = 3e38f; int bi = 0;
    for (int i = 0; i < nd; i++) if (f[i] < best || (f[i] == best && (rnd() & 3) == 0)) { best = f[i]; bi = i; }
    int D = bi;
    if (best >= 1e29f) return;   /* no domain-consistent path on this grid */
    for (int o = M - 1; o >= 0; o--) { rns[o] = pref[j * (M + 1) + o] + D * RR; if (o > 0) D = rarg[(size_t)o * rmaxnd + D]; }
    long long before = TOT;
    for (int o = 0; o < M; o++) { int id = j * M + o; if (st[id] != rns[o]) setst(id, rns[o]); }
    nreroute++; if (TOT < before) nreroute_gain++;
}
/* pick a job involved in a random conflict */
static int conflict_job(void) {
    if (cn == 0) return -1;
    int k = cset[rndint(cn)]; int *L = mlist + (size_t)k * N;
    int cand[128], nc = 0;
    for (int i = 0; i + 1 < N && nc < 128; i++) if (st[L[i]] + dur[L[i]] > st[L[i + 1]]) cand[nc++] = i;
    if (!nc) return -1;
    int i0 = cand[rndint(nc)];
    int id = (rnd() & 1) ? L[i0] : L[i0 + 1];
    return id / M;
}

/* one min-conflicts step: separate a random overlapping pair the cheapest way */
static void ls_step(void) {
    if (cn == 0) return;
    int k = cset[rndint(cn)];
    for (int s2 = 0; s2 < 2; s2++) { int k2 = cset[rndint(cn)]; if (mw[k2] > mw[k]) k = k2; }
    int *L = mlist + (size_t)k * N;
    int cand[128], nc = 0;
    for (int i = 0; i + 1 < N && nc < 128; i++)
        if (st[L[i]] + dur[L[i]] > st[L[i + 1]]) cand[nc++] = i;
    if (!nc) { cdel(k); return; }
    int i0 = cand[rndint(nc)];
    int A = L[i0], B = L[i0 + 1];

    int oid[4], odir[4], os[4], no = 0;
    oid[no] = A; odir[no] = 1;  os[no] = st[B] + dur[B]; no++;
    oid[no] = A; odir[no] = -1; os[no] = st[B] - dur[A]; no++;
    oid[no] = B; odir[no] = 1;  os[no] = st[A] + dur[A]; no++;
    oid[no] = B; odir[no] = -1; os[no] = st[A] - dur[B]; no++;

    long long t0 = TOT, bestd = LLONG_MAX;
    int bi = -1;
    for (int c = 0; c < no; c++) {
        int id = oid[c], j = id / M, o = opo[id];
        if (odir[c] > 0 ? os[c] <= st[id] : os[c] >= st[id]) continue;
        ulog_n = 0;
        int ok = (odir[c] > 0) ? move_right(j, o, os[c]) : move_left(j, o, os[c]);
        long long d = ok ? TOT - t0 : LLONG_MAX;
        undo();
        if (!ok) continue;
        if (d < bestd || (d == bestd && (rnd() & 1))) { bestd = d; bi = c; }
    }
    if (bi < 0) { if (mw[k] < 1000000) mw[k]++; if (!DESCENT) kick(4, 2000); return; }
    if (DESCENT && bestd >= 0 && !(bestd == 0 && (rnd() & 1))) { if (mw[k] < 1000000) mw[k]++; return; }
    if ((rnd() % 100) < (unsigned)NOISE) {           /* noise: random legal option */
        for (int tries = 0; tries < 6; tries++) {
            int c = rndint(no), id = oid[c], j = id / M, o = opo[id];
            if (odir[c] > 0 ? os[c] <= st[id] : os[c] >= st[id]) continue;
            ulog_n = 0;
            if ((odir[c] > 0) ? move_right(j, o, os[c]) : move_left(j, o, os[c])) { bi = c; goto done; }
            undo();
        }
    }
    {
        int id = oid[bi], j = id / M, o = opo[id];
        ulog_n = 0;
        if (!((odir[bi] > 0) ? move_right(j, o, os[bi]) : move_left(j, o, os[bi]))) { undo(); return; }
    }
done:
    ulog_n = 0;
    if (cidx[k] >= 0) { if (mw[k] < 1000000) mw[k]++; } else mw[k] = 1;
}

static void init_stretch(void) {
    for (int j = 0; j < N; j++)
        for (int o = 0; o < M; o++)
            st[j * M + o] = LOv(j, o) + (int)((long long)slack[j] * o / M);
}
/* clip a saved schedule into the corridors of the current (smaller) T */
static void warm_from(int *src) {
    memcpy(st, src, sizeof(int) * NM);
    for (int j = 0; j < N; j++) {
        int base = j * M, lim = T - dur[base + M - 1];
        for (int o = M - 1; o >= 0; o--) {
            int id = base + o;
            if (st[id] > lim) st[id] = lim;
            if (st[id] < LOv(j, o)) st[id] = LOv(j, o);
            if (o) lim = st[id] - dur[id - 1];
        }
    }
}

int main(int argc, char **argv) {
    const char *inst = argc > 1 ? argv[1] : "/app/tai_100_1000_1.txt";
    const char *out = argc > 2 ? argv[2] : "/app/solution.txt";
    int T0 = argc > 3 ? atoi(argv[3]) : 545000;
    double maxsec = argc > 4 ? atof(argv[4]) : 600.0;
    if (argc > 5) rngs = (unsigned)atoi(argv[5]) * 2654435761u + 12345u;
    NOISE = argc > 6 ? atoi(argv[6]) : 8;
    long long stallit = argc > 7 ? atoll(argv[7]) * 1000000LL : 40000000LL;
    const char *seedfile = argc > 8 ? argv[8] : NULL;
    int step = argc > 9 ? atoi(argv[9]) : 1000;
    long long shakeit = argc > 10 ? atoll(argv[10]) : 200000LL;
    if (getenv("JSPV")) verbose = 1;
    if (getenv("DESCENT")) DESCENT = atoi(getenv("DESCENT"));
    long long rrevery = getenv("RREVERY") ? atoll(getenv("RREVERY")) : 0;
    if (getenv("ILS")) { ILS = 1; RREVERY = rrevery; rrevery = 0; }
    if (getenv("ILSRR")) ILSRR = atoi(getenv("ILSRR"));
    if (getenv("ILSK")) ILSK = atoi(getenv("ILSK"));
    if (getenv("ILSW")) ILSW = atoi(getenv("ILSW"));
    if (getenv("ILSACC")) ILSACC = atoi(getenv("ILSACC"));
    if (getenv("ILSDESC")) ILSDESC = atoll(getenv("ILSDESC"));
    if (getenv("RR")) RR = atoi(getenv("RR"));
    if (getenv("HF")) HF = atof(getenv("HF"));

    read_instance(inst);
    if (getenv("DOMS")) {
        FILE *df = fopen(getenv("DOMS"), "r");
        dlo = malloc(sizeof(int) * NM); dhi = malloc(sizeof(int) * NM);
        int ok = df != NULL;
        for (int q = 0; q < NM && ok; q++) ok = fscanf(df, "%d", &dlo[q]) == 1;
        for (int q = 0; q < NM && ok; q++) ok = fscanf(df, "%d", &dhi[q]) == 1;
        if (df) fclose(df);
        if (!ok) { fprintf(stderr, "bad DOMS file\n"); return 1; }
        fprintf(stderr, "loaded propagated domains\n");
    }
    ulog_id = malloc(sizeof(int) * (MAXDEPTH + 8));
    ulog_old = malloc(sizeof(int) * (MAXDEPTH + 8));

    if (seedfile) {
        FILE *sf = fopen(seedfile, "r");
        if (sf) {
            int okread = 1;
            for (int id = 0; id < NM && okread; id++)
                if (fscanf(sf, "%d", &st[id]) != 1) okread = 0;
            fclose(sf);
            long long mk;
            if (okread && check(st, &mk)) {
                memcpy(bestst, st, sizeof(int) * NM); bestmk = mk;
                if ((int)mk - step < T0) T0 = (int)mk - step;
                fprintf(stderr, "seeded mk=%lld\n", mk);
            } else if (okread && getenv("RAWSEED")) {
                /* infeasible seed: use it as the starting state at T0 (clipped into corridors) */
                memcpy(bestst, st, sizeof(int) * NM); rawseed = 1;
                fprintf(stderr, "raw infeasible seed used as start\n");
            } else fprintf(stderr, "seed unusable\n");
        }
    }
    double t0 = now(), deadline = t0 + maxsec;
    fprintf(stderr, "N=%d M=%d LB=%d T0=%d noise=%d stallit=%lld step=%d\n",
            N, M, LB, T0, NOISE, stallit, step);
    T = T0;
    for (;;) {
        for (int j = 0; j < N; j++) { slack[j] = T - jlen[j]; if (slack[j] < 0) slack[j] = 0; }
        if (bestmk > 0 || rawseed) warm_from(bestst); else init_stretch();
        if (dlo) clip_domains();
        rawseed = 0;
        rebuild();
        if (rrevery || ILS) reroute_alloc();
        long long amt0 = TOT; int c0 = cn;
        double rt0 = now();
        long long it = 0, sincebest = 0, bestamt = TOT;
        long long nkick = 0;
        if (ILS) {
            /* iterated local search: perturb near a conflict, strict descent, accept if not worse */
            int *snap = malloc(sizeof(int) * NM);
            long long acc = 0, ilsit = 0, bestTOT = TOT;
            DESCENT = 1;
            /* initial descent */
            for (long long q = 0; q < ILSDESC * 20 && cn > 0; q++) ls_step();
            while (cn > 0 && now() < deadline) {
                memcpy(snap, st, sizeof(int) * NM); long long t0v = TOT;
                int r = rndint(100);
                if (r < ILSRR) { int j = conflict_job(); if (j >= 0) reroute(j); }
                else kick(ILSK, ILSW);
                for (long long q = 0; q < ILSDESC && cn > 0; q++) {
                    ls_step();
                    if (RREVERY && (q % RREVERY) == RREVERY - 1) { int j = conflict_job(); if (j >= 0) reroute(j); }
                }
                ilsit++;
                if (TOT <= t0v || (rndint(1000) < ILSACC)) { acc++; if (TOT < bestTOT) bestTOT = TOT; }
                else { memcpy(st, snap, sizeof(int) * NM); rebuild(); }
                if ((ilsit & 255) == 0 && verbose)
                    fprintf(stderr, "  ILS T=%d iter=%lld acc=%lld conf=%d amt=%lld best=%lld rr=%lld/%lld (%.0fs)\n",
                            T, ilsit, acc, cn, TOT, bestTOT, nreroute_gain, nreroute, now() - t0);
            }
            free(snap);
        }
        while (cn > 0 && now() < deadline) {
            ls_step(); it++; sincebest++;
            if (rrevery && (it % rrevery) == 0) {
                hist_bump();
                int j = conflict_job(); if (j >= 0) reroute(j);
            }
            if (TOT < bestamt) { bestamt = TOT; sincebest = 0; }
            if (sincebest && (sincebest % shakeit) == 0) {
                for (int q = 0; q < 20; q++) kick(8, 3000);
                for (int q = 0; q < M; q++) mw[q] = 1;
                nkick++;
            }
            if ((it & 0xFFFFF) == 0) {
                if (verbose)
                    fprintf(stderr, "  T=%d it=%lldM conf=%d amt=%lld shk=%lld rr=%lld/%lld (%.0fs)\n",
                            T, it >> 20, cn, TOT, nkick, nreroute_gain, nreroute, now() - t0);
                if (sincebest > stallit) break;
            }
        }
        fprintf(stderr, "T=%d: conf %d->%d amt %lld->%lld it=%lld (%.1fs)\n",
                T, c0, cn, amt0, TOT, it, now() - rt0);
        if (cn == 0) {
            long long mk;
            if (check(st, &mk)) {
                if (bestmk < 0 || mk < bestmk) {
                    memcpy(bestst, st, sizeof(int) * NM); bestmk = mk;
                    write_solution(out, bestst);
                    fprintf(stderr, "  *** FEASIBLE %lld\n", mk);
                }
                T = (int)bestmk - step;
            } else { fprintf(stderr, "  CHECK FAILED\n"); return 2; }
        } else {
            if (bestmk < 0) T += 5000;
            else T = (int)bestmk - (1 + rndint(step));
        }
        if (now() >= deadline) {
            if (cn > 0 && getenv("DUMP")) { write_solution(getenv("DUMP"), st); fprintf(stderr, "dumped T=%d conf=%d\n", T, cn); }
            break;
        }
        if (T < LB) T = LB;
    }
    fprintf(stderr, "FINAL best=%lld\n", bestmk);
    return 0;
}
