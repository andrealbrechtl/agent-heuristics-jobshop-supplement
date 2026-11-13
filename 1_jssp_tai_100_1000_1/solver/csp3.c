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
static inline int LOv(int j, int o) { return pref[j * (M + 1) + o]; }
static inline int HIv(int j, int o) { return pref[j * (M + 1) + o] + slack[j]; }

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
    if (bi < 0) { if (mw[k] < 1000000) mw[k]++; kick(4, 2000); return; }
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

    read_instance(inst);
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
            } else fprintf(stderr, "seed unusable\n");
        }
    }
    double t0 = now(), deadline = t0 + maxsec;
    fprintf(stderr, "N=%d M=%d LB=%d T0=%d noise=%d stallit=%lld step=%d\n",
            N, M, LB, T0, NOISE, stallit, step);
    T = T0;
    for (;;) {
        for (int j = 0; j < N; j++) { slack[j] = T - jlen[j]; if (slack[j] < 0) slack[j] = 0; }
        if (bestmk > 0) warm_from(bestst); else init_stretch();
        rebuild();
        long long amt0 = TOT; int c0 = cn;
        double rt0 = now();
        long long it = 0, sincebest = 0, bestamt = TOT;
        long long nkick = 0;
        while (cn > 0 && now() < deadline) {
            ls_step(); it++; sincebest++;
            if (TOT < bestamt) { bestamt = TOT; sincebest = 0; }
            if (sincebest && (sincebest % shakeit) == 0) {
                for (int q = 0; q < 20; q++) kick(8, 3000);
                for (int q = 0; q < M; q++) mw[q] = 1;
                nkick++;
            }
            if ((it & 0xFFFFF) == 0) {
                if (verbose)
                    fprintf(stderr, "  T=%d it=%lldM conf=%d amt=%lld shk=%lld (%.0fs)\n",
                            T, it >> 20, cn, TOT, nkick, now() - t0);
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
        if (now() >= deadline) break;
        if (T < LB) T = LB;
    }
    fprintf(stderr, "FINAL best=%lld\n", bestmk);
    return 0;
}
