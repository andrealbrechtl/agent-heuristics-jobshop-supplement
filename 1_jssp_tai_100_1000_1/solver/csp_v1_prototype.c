/* Job-shop as a bounded-corridor CSP — tai_100_1000 (100 jobs x 1000 machines).
 *
 * Fix a target makespan T.  Then op o of job j must satisfy
 *     lo = pref_j(o)  <=  st  <=  pref_j(o) + slack_j,   slack_j = T - L_j
 * (it cannot start before its own prefix of work, and it must leave room for the
 * rest of the job before T).  Chain constraints st[o+1] >= st[o]+d[o] and these
 * corridors are kept satisfied at ALL times, so the makespan is <= T by
 * construction and can never inflate.  The only thing left to fix is machine
 * overlap, which we drive to zero by local search.  If it reaches zero we have a
 * feasible schedule at T; then lower T and repeat.
 *
 * This is the fix for the failure mode of the earlier repair solvers: there the
 * makespan was the thing being optimised, so cascading shifts could and did
 * inflate it (593k -> 660k).  Here inflation is structurally impossible.
 *
 * Why T ~ 535k is reachable: ~9.5k collisions exist between zero-idle job
 * profiles, each clearable by displacing one op by min(right, left) shift, which
 * averages E[(da+db)^2]/(4E[da+db]) = 291.  Priority by job length keeps the
 * displacement on jobs that have slack (the 522298-long job has only 12702 of
 * room; the shortest has 53522).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/time.h>

static int N, M, NM;
static int *mach, *dur, *st, *opo;
static int *pref, *jlen, *slack;
static int *mlist, *mpos;      /* mpos[id] = index of id within its machine list */
static int *bestst;
static long long bestmk = -1;
static int LB, T;
static int verbose = 0;

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

/* ---- conflicted-machine set ---- */
static int *cset, *cidx, cn;    /* cset: machines with >=1 overlap; cidx[k]=pos or -1 */
static inline void cadd(int k) { if (cidx[k] < 0) { cidx[k] = cn; cset[cn++] = k; } }
static inline void cdel(int k) {
    int p = cidx[k]; if (p < 0) return;
    int last = cset[--cn]; cset[p] = last; cidx[last] = p; cidx[k] = -1;
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
    LB = 0;
    for (int j = 0; j < N; j++) if (jlen[j] > LB) LB = jlen[j];
}

static void sort_machine(int k) {
    int *L = mlist + (size_t)k * N;
    for (int i = 1; i < N; i++) {
        int v = L[i], s = st[v], p = i - 1;
        while (p >= 0 && st[L[p]] > s) { L[p + 1] = L[p]; mpos[L[p + 1]] = p + 1; p--; }
        L[p + 1] = v; mpos[v] = p + 1;
    }
}
static long long machine_overlap(int k) {   /* assumes machine k is sorted */
    int *L = mlist + (size_t)k * N;
    long long t = 0;
    for (int i = 0; i + 1 < N; i++) {
        int ov = st[L[i]] + dur[L[i]] - st[L[i + 1]];
        if (ov > 0) t += ov;
    }
    return t;
}
static void refresh(int k) {                /* re-sort and update conflict membership */
    sort_machine(k);
    if (machine_overlap(k) > 0) cadd(k); else cdel(k);
}

/* ---- moves with undo log ---- */
static int *ulog_id, ulog_n;
static int *ulog_old;
static int *touch, ntouch;      /* machines whose lists need refreshing */
static char *touched;

static inline void record(int id) {
    ulog_id[ulog_n] = id; ulog_old[ulog_n] = st[id]; ulog_n++;
    int k = mach[id];
    if (!touched[k]) { touched[k] = 1; touch[ntouch++] = k; }
}
static void undo(void) {
    for (int i = ulog_n - 1; i >= 0; i--) st[ulog_id[i]] = ulog_old[i];
    ulog_n = 0;
}
static void clear_touch(void) {
    for (int i = 0; i < ntouch; i++) touched[touch[i]] = 0;
    ntouch = 0;
}

#define MAXDEPTH 60

/* move op (j,o) to start s (s > current), propagating forward.  Keeps corridors.
 * Returns 1 on success (changes recorded in the undo log), 0 on failure. */
static int move_right(int j, int o, int s) {
    int base = j * M;
    for (int i = o; i < M; i++) {
        int id = base + i;
        if (st[id] >= s) return 1;
        if (s > HIv(j, i)) return 0;
        if (i - o >= MAXDEPTH) return 0;
        record(id); st[id] = s;
        s = s + dur[id];
    }
    return 1;
}
/* move op (j,o) to start s (s < current), propagating backward. */
static int move_left(int j, int o, int s) {
    int base = j * M;
    for (int i = o; i >= 0; i--) {
        int id = base + i;
        if (st[id] <= s) return 1;
        if (s < LOv(j, i)) return 0;
        if (o - i >= MAXDEPTH) return 0;
        record(id); st[id] = s;
        if (i) s = s - dur[id - 1];
    }
    return 1;
}

/* sum of overlap over the touched machines (they must be sorted first) */
static long long touched_overlap(void) {
    long long t = 0;
    for (int i = 0; i < ntouch; i++) { sort_machine(touch[i]); t += machine_overlap(touch[i]); }
    return t;
}

static long long total_overlap(int *cntout) {
    long long t = 0; int c = 0;
    for (int k = 0; k < M; k++) {
        sort_machine(k);
        long long o = machine_overlap(k);
        if (o > 0) { t += o; c++; }
        if (o > 0) cadd(k); else cdel(k);
    }
    if (cntout) *cntout = c;
    return t;
}

static void init_stretch(void) {
    for (int j = 0; j < N; j++)
        for (int o = 0; o < M; o++)
            st[j * M + o] = LOv(j, o) + (int)((long long)slack[j] * o / M);
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

/* ---------------- local search ---------------- */
/* One step: take a conflicting pair, try the four ways of separating it
 * (either op, either direction), keep whichever reduces total overlap most. */
static long long ls_step(void) {
    if (cn == 0) return 0;
    int k = cset[rndint(cn)];
    sort_machine(k);
    int *L = mlist + (size_t)k * N;
    /* collect conflicting adjacent pairs, pick one at random */
    int cand[128], nc = 0;
    for (int i = 0; i + 1 < N && nc < 128; i++)
        if (st[L[i]] + dur[L[i]] > st[L[i + 1]]) cand[nc++] = i;
    if (!nc) { cdel(k); return 0; }
    int i0 = cand[rndint(nc)];
    int A = L[i0], B = L[i0 + 1];

    struct { int op, dir, s; } opt[4];
    int no = 0;
    /* A right past B, A left before B, B right past A, B left before A */
    opt[no].op = A; opt[no].dir = 1; opt[no].s = st[B] + dur[B]; no++;
    opt[no].op = A; opt[no].dir = -1; opt[no].s = st[B] - dur[A]; no++;
    opt[no].op = B; opt[no].dir = 1; opt[no].s = st[A] + dur[A]; no++;
    opt[no].op = B; opt[no].dir = -1; opt[no].s = st[A] - dur[B]; no++;

    long long bestd = LLONG_MAX; int bi = -1;
    long long base = 0;
    /* baseline overlap of every machine any candidate could touch is handled by
     * measuring before/after over the touched set for each candidate */
    for (int c = 0; c < no; c++) {
        int id = opt[c].op, j = id / M, o = opo[id];
        if (opt[c].dir > 0 && opt[c].s <= st[id]) continue;
        if (opt[c].dir < 0 && opt[c].s >= st[id]) continue;
        ulog_n = 0; clear_touch();
        /* baseline: overlap of the machines this move will touch.  Do a trial
         * apply first to discover them, measure after, then undo and measure
         * before over the same set. */
        int ok = (opt[c].dir > 0) ? move_right(j, o, opt[c].s) : move_left(j, o, opt[c].s);
        if (!ok) { undo(); clear_touch(); continue; }
        long long after = touched_overlap();
        int nt = ntouch;
        int save[512]; if (nt > 512) nt = 512;
        for (int x = 0; x < nt; x++) save[x] = touch[x];
        undo();
        base = 0;
        for (int x = 0; x < nt; x++) { sort_machine(save[x]); base += machine_overlap(save[x]); }
        clear_touch();
        long long d = after - base;
        if (d < bestd || (d == bestd && (rnd() & 1))) { bestd = d; bi = c; }
    }
    if (bi < 0) return 0;
    /* noise: occasionally take a random feasible option instead of the best */
    if ((rnd() % 100) < 8) {
        int tries = 0;
        while (tries++ < 6) {
            int c = rndint(no);
            int id = opt[c].op, j = id / M, o = opo[id];
            if (opt[c].dir > 0 && opt[c].s <= st[id]) continue;
            if (opt[c].dir < 0 && opt[c].s >= st[id]) continue;
            ulog_n = 0; clear_touch();
            int ok = (opt[c].dir > 0) ? move_right(j, o, opt[c].s) : move_left(j, o, opt[c].s);
            if (!ok) { undo(); clear_touch(); continue; }
            bi = c; goto applied;
        }
    }
    {
        int id = opt[bi].op, j = id / M, o = opo[id];
        ulog_n = 0; clear_touch();
        int ok = (opt[bi].dir > 0) ? move_right(j, o, opt[bi].s) : move_left(j, o, opt[bi].s);
        if (!ok) { undo(); clear_touch(); return 0; }
    }
applied:
    for (int x = 0; x < ntouch; x++) refresh(touch[x]);
    clear_touch(); ulog_n = 0;
    refresh(k);
    return 1;
}

int main(int argc, char **argv) {
    const char *inst = argc > 1 ? argv[1] : "/app/tai_100_1000_1.txt";
    const char *out = argc > 2 ? argv[2] : "/app/solution.txt";
    int T0 = argc > 3 ? atoi(argv[3]) : 545000;
    double maxsec = argc > 4 ? atof(argv[4]) : 600.0;
    if (argc > 5) rngs = (unsigned)atoi(argv[5]) * 2654435761u + 12345u;
    if (getenv("JSPV")) verbose = 1;

    read_instance(inst);
    ulog_id = malloc(sizeof(int) * (MAXDEPTH + 8) * 2);
    ulog_old = malloc(sizeof(int) * (MAXDEPTH + 8) * 2);
    touch = malloc(sizeof(int) * (MAXDEPTH + 8) * 2);
    touched = calloc(M, 1);
    double t0 = now(), deadline = t0 + maxsec;
    fprintf(stderr, "N=%d M=%d LB=%d T0=%d\n", N, M, LB, T0);

    T = T0;
    for (;;) {
        for (int j = 0; j < N; j++) { slack[j] = T - jlen[j]; if (slack[j] < 0) slack[j] = 0; }
        if (bestmk > 0) {
            /* warm start from the best solution, clipped into the new corridors */
            memcpy(st, bestst, sizeof(int) * NM);
            for (int j = 0; j < N; j++) {
                int base = j * M, lim = T - dur[base + M - 1];
                for (int o = M - 1; o >= 0; o--) {
                    int id = base + o;
                    if (st[id] > lim) st[id] = lim;
                    if (st[id] < LOv(j, o)) st[id] = LOv(j, o);
                    if (o) lim = st[id] - dur[id - 1];
                }
            }
        } else init_stretch();
        cn = 0; for (int k = 0; k < M; k++) cidx[k] = -1;
        int c0; long long amt0 = total_overlap(&c0);
        double rt0 = now();
        long long it = 0, stall = 0;
        long long bestamt = amt0;
        while (cn > 0 && now() < deadline) {
            ls_step(); it++;
            if ((it & 0xFFFF) == 0) {
                int cc; long long amt = total_overlap(&cc);
                if (amt < bestamt) { bestamt = amt; stall = 0; } else stall++;
                if (verbose)
                    fprintf(stderr, "  T=%d it=%lldk conf=%d amt=%lld (%.0fs)\n",
                            T, it / 1000, cc, amt, now() - t0);
                if (stall > 60) break;
            }
        }
        int cc; long long amt = total_overlap(&cc);
        fprintf(stderr, "T=%d: start conf=%d amt=%lld -> conf=%d amt=%lld  it=%lld (%.0fs)\n",
                T, c0, amt0, cc, amt, it, now() - rt0);
        if (cc == 0) {
            long long mk;
            if (check(st, &mk)) {
                if (bestmk < 0 || mk < bestmk) {
                    memcpy(bestst, st, sizeof(int) * NM); bestmk = mk;
                    write_solution(out, bestst);
                    fprintf(stderr, "  *** FEASIBLE %lld\n", mk);
                }
                T = (int)bestmk - 1000;
            } else { fprintf(stderr, "  check failed?!\n"); break; }
        } else {
            if (bestmk < 0) T += 5000;          /* nothing yet: loosen */
            else T = (int)bestmk - 200;         /* try a smaller step */
        }
        if (now() >= deadline) break;
        if (T < LB) T = LB;
    }
    fprintf(stderr, "FINAL best=%lld\n", bestmk);
    return 0;
}
