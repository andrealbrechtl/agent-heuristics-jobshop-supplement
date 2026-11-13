/* Domain propagation + shaving for the job shop at a fixed makespan T.
 *
 * Start-time domains [lo, hi] for every op.  Rules, iterated to a fixpoint:
 *   chain    lo[o+1] >= lo[o] + d[o],  hi[o] <= hi[o+1] - d[o]
 *   pairwise two ops a, b on one machine: if a cannot precede b
 *            (lo[a] + d[a] > hi[b]) then b precedes a:
 *            lo[a] >= lo[b] + d[b]  and  hi[b] <= hi[a] - d[b]
 *            (and fail if neither order fits)
 * Shaving: for op x test "start(x) <= lo[x] + delta" on a copy; if propagation
 * fails the hypothesis is refuted and lo[x] := lo[x] + delta + 1 (likewise on the
 * upper side).  Every bound found is implied by the model, so any schedule with
 * makespan <= T satisfies the output domains.  If the root propagation fails,
 * T is infeasible: a lower bound proof.
 *
 * usage: shave inst T out.txt [seconds] [topjobs] [init_domains.txt]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define NN 100
#define MM 1000
static int N, M, NM, T;
static int mach[NN * MM], dur[NN * MM], pref[NN * MM], L[NN];
static int mops[MM][NN];          /* op ids per machine */
static int *lo, *hi;
static int *q, qh, qt, *inq;
static double now(void) { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec + tv.tv_usec * 1e-6; }

static inline void push(int id) { if (!inq[id]) { inq[id] = 1; q[qt] = id; qt = (qt + 1) % (NM + 1); } }
static int trailing = 0, *tr_id, *tr_lo, *tr_hi, trn = 0;
static inline void trail(int id) { if (trailing) { tr_id[trn] = id; tr_lo[trn] = lo[id]; tr_hi[trn] = hi[id]; trn++; } }
static inline int setlo(int id, int v) { if (v > lo[id]) { trail(id); lo[id] = v; push(id); } return lo[id] <= hi[id]; }
static inline int sethi(int id, int v) { if (v < hi[id]) { trail(id); hi[id] = v; push(id); } return lo[id] <= hi[id]; }

/* propagate until fixpoint; returns 0 on failure */
static int propagate(void) {
    while (qh != qt) {
        int x = q[qh]; qh = (qh + 1) % (NM + 1); inq[x] = 0;
        int j = x / M, o = x % M;
        if (o + 1 < M && !setlo(x + 1, lo[x] + dur[x])) return 0;
        if (o > 0 && !sethi(x - 1, hi[x] - dur[x - 1])) return 0;
        int k = mach[x];
        for (int i = 0; i < N; i++) {
            int y = mops[k][i];
            if (y == x) continue;
            /* windows that cannot meet impose nothing */
            if (lo[y] >= hi[x] + dur[x] || lo[x] >= hi[y] + dur[y]) continue;
            int xy = lo[x] + dur[x] <= hi[y];   /* x before y possible */
            int yx = lo[y] + dur[y] <= hi[x];   /* y before x possible */
            if (!xy && !yx) return 0;
            if (!xy) {   /* y before x */
                if (!setlo(x, lo[y] + dur[y])) return 0;
                if (!sethi(y, hi[x] - dur[y])) return 0;
            } else if (!yx) {
                if (!setlo(y, lo[x] + dur[x])) return 0;
                if (!sethi(x, hi[y] - dur[x])) return 0;
            }
        }
        (void)j;
    }
    return 1;
}

static int *slo, *shi;
static long long nprop = 0;
/* test domain change on a copy; returns 1 if consistent */
static int probe(int id, int newlo, int newhi) {
    trailing = 1; trn = 0; qh = qt = 0;
    int ok = 1;
    if (newlo > lo[id]) ok = setlo(id, newlo);
    if (ok && newhi < hi[id]) ok = sethi(id, newhi);
    if (ok) ok = propagate();
    nprop++; if (getenv("DBG") && (id % 1000) >= 950 && (id / 1000) == 59) fprintf(stderr, "probe id=%d [%d,%d] -> ok=%d trn=%d\n", id, newlo, newhi, ok, trn);
    while (qh != qt) { inq[q[qh]] = 0; qh = (qh + 1) % (NM + 1); }
    for (int i = trn - 1; i >= 0; i--) { lo[tr_id[i]] = tr_lo[i]; hi[tr_id[i]] = tr_hi[i]; }
    trailing = 0; trn = 0; qh = qt = 0;
    return ok;
}

int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "r");
    if (!f || fscanf(f, "%d %d", &N, &M) != 2) return 1;
    NM = N * M;
    int cnt[MM] = {0};
    for (int j = 0; j < N; j++) {
        int p = 0;
        for (int o = 0; o < M; o++) {
            int id = j * M + o;
            if (fscanf(f, "%d %d", &mach[id], &dur[id]) != 2) return 1;
            pref[id] = p; p += dur[id];
            mops[mach[id]][cnt[mach[id]]++] = id;
        }
        L[j] = p;
    }
    fclose(f);
    T = atoi(argv[2]);
    const char *out = argv[3];
    double secs = argc > 4 ? atof(argv[4]) : 60;
    int top = argc > 5 ? atoi(argv[5]) : 20;
    lo = malloc(sizeof(int) * NM); hi = malloc(sizeof(int) * NM);
    slo = malloc(sizeof(int) * NM); shi = malloc(sizeof(int) * NM);
    q = malloc(sizeof(int) * (NM + 1)); inq = calloc(NM, sizeof(int));
    tr_id = malloc(sizeof(int) * 64 * NM); tr_lo = malloc(sizeof(int) * 64 * NM); tr_hi = malloc(sizeof(int) * 64 * NM);
    for (int id = 0; id < NM; id++) { int j = id / M; lo[id] = pref[id]; hi[id] = pref[id] + T - L[j]; }
    if (argc > 6) {
        FILE *g = fopen(argv[6], "r"); int ok = g != NULL;
        for (int i = 0; i < NM && ok; i++) { int v; ok = fscanf(g, "%d", &v) == 1; if (ok && v > lo[i]) lo[i] = v; }
        for (int i = 0; i < NM && ok; i++) { int v; ok = fscanf(g, "%d", &v) == 1; if (ok && v < hi[i]) hi[i] = v; }
        if (g) fclose(g);
    }
    double t0 = now();
    qh = qt = 0;
    for (int id = 0; id < NM; id++) push(id);
    if (!propagate()) { printf("ROOT FAIL: makespan %d is infeasible\n", T); return 3; }
    long long w = 0; for (int id = 0; id < NM; id++) w += hi[id] - lo[id];
    printf("root propagation %.1fs mean width %.1f\n", now() - t0, (double)w / NM);
    /* jobs by slack */
    int ord[NN]; for (int j = 0; j < N; j++) ord[j] = j;
    for (int a = 0; a < N; a++) for (int b = a + 1; b < N; b++) if (L[ord[b]] > L[ord[a]]) { int t = ord[a]; ord[a] = ord[b]; ord[b] = t; }
    /* shaving rounds over the critical jobs, coarse to fine */
    int round = 0, changed = 1;
    while (changed && now() - t0 < secs) {
        changed = 0; round++;
        int stride = round == 1 ? 25 : (round == 2 ? 10 : 3);
        for (int r = 0; r < top && now() - t0 < secs; r++) {
            int j = ord[r];
            for (int o = 0; o < M && now() - t0 < secs; o += stride) {
                int id = j * M + o;
                /* lower side: refute start <= lo + delta, largest delta first via halving */
                for (int side = 0; side < 2; side++) {
                    int width = hi[id] - lo[id];
                    if (width <= 0) break;
                    int delta = width / 2;
                    while (delta >= 1) {
                        int ok = side == 0 ? probe(id, lo[id], lo[id] + delta) : probe(id, hi[id] - delta, hi[id]);
                        if (!ok) {
                            qh = qt = 0;
                            if (side == 0) setlo(id, lo[id] + delta + 1); else sethi(id, hi[id] - delta - 1);
                            if (!propagate()) { printf("SHAVE FAIL: makespan %d is infeasible\n", T); return 3; }
                            changed = 1;
                            break;
                        }
                        delta /= 4;
                    }
                }
            }
        }
        w = 0; for (int id = 0; id < NM; id++) w += hi[id] - lo[id];
        printf("round %d stride %d mean width %.1f probes %lld (%.0fs)\n", round, stride, (double)w / NM, nprop, now() - t0);
        for (int r = 0; r < 6; r++) {
            int j = ord[r]; long long s = 0; int mn = 1 << 30;
            for (int o = 0; o < M; o++) { int ww = hi[j * M + o] - lo[j * M + o]; s += ww; if (ww < mn) mn = ww; }
            printf("   job %d slack %d mean width %.0f min %d\n", j, T - L[j], (double)s / M, mn);
        }
        fflush(stdout);
    }
    FILE *g = fopen(out, "w");
    for (int id = 0; id < NM; id++) fprintf(g, "%d%c", lo[id], (id % M) == M - 1 ? '\n' : ' ');
    for (int id = 0; id < NM; id++) fprintf(g, "%d%c", hi[id], (id % M) == M - 1 ? '\n' : ' ');
    fclose(g);
    return 0;
}
