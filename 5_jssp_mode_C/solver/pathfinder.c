/* Negotiated-congestion (PathFinder-style) router for the large job shop.
 *
 * Target makespan T is fixed; job j may accumulate total delay D in [0, s_j],
 * s_j = T - L_j, monotone along its route.  Each job is ripped up and rerouted
 * by an exact DP over (op, delay) on a delay grid of resolution R, minimising
 *     pf * overlap-with-other-jobs  +  hw * history(machine, time bucket)
 * History grows on every bucket that is still overused, so jobs negotiate who
 * yields where.  The whole-job DP moves delay budget along the entire chain in
 * one step, which local chain moves cannot do.
 *
 * usage: pf inst out T iters seed R [init.txt]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#define NN 100
#define MM 1000
static int N, M, LB, T, R = 2;
static int mach[NN][MM], dur[NN][MM], pref[NN][MM + 1], L[NN], slack[NN];
static int st[NN][MM];
/* per-machine op lists (job,o) sorted by start */
static int mj[MM][NN], mo[MM][NN], mc[MM];
#define BK 64
static int NB;
static float *hist;      /* [M][NB] */
static double *hcum;     /* [M][NB+1] */
static unsigned rs = 1;
static inline unsigned rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static double now(void) { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec + tv.tv_usec * 1e-6; }

static void mrem(int k, int j) {
    int p = 0;
    for (int i = 0; i < mc[k]; i++) if (mj[k][i] != j) { mj[k][p] = mj[k][i]; mo[k][p] = mo[k][i]; p++; }
    mc[k] = p;
}
static void mins(int k, int j, int o) {
    int s = st[j][o], i = mc[k];
    while (i > 0 && st[mj[k][i - 1]][mo[k][i - 1]] > s) { mj[k][i] = mj[k][i - 1]; mo[k][i] = mo[k][i - 1]; i--; }
    mj[k][i] = j; mo[k][i] = o; mc[k]++;
}
static void place_job(int j) { for (int o = 0; o < M; o++) mins(mach[j][o], j, o); }
static void remove_job(int j) { for (int o = 0; o < M; o++) mrem(mach[j][o], j); }

static long long overlap_job(int j) {   /* overlap of job j with others (j placed) */
    long long t = 0;
    for (int o = 0; o < M; o++) {
        int k = mach[j][o], a = st[j][o], b = a + dur[j][o];
        for (int i = 0; i < mc[k]; i++) {
            int jj = mj[k][i]; if (jj == j) continue;
            int s = st[jj][mo[k][i]], e = s + dur[jj][mo[k][i]];
            if (s >= b) break;
            int lo = a > s ? a : s, hi = b < e ? b : e;
            if (hi > lo) t += hi - lo;
        }
    }
    return t;
}
static long long total_overlap(int *nconf) {
    long long t = 0; int nc = 0;
    for (int k = 0; k < M; k++) {
        int bad = 0;
        for (int i = 0; i < mc[k]; i++) {
            int a = st[mj[k][i]][mo[k][i]], b = a + dur[mj[k][i]][mo[k][i]];
            for (int q = i + 1; q < mc[k]; q++) {
                int s = st[mj[k][q]][mo[k][q]];
                if (s >= b) break;
                int e = s + dur[mj[k][q]][mo[k][q]];
                t += (b < e ? b : e) - s; bad = 1;
            }
        }
        nc += bad;
    }
    if (nconf) *nconf = nc;
    return t;
}
static void hist_update(double hf) {
    for (int k = 0; k < M; k++) {
        for (int i = 0; i < mc[k]; i++) {
            int a = st[mj[k][i]][mo[k][i]], b = a + dur[mj[k][i]][mo[k][i]];
            for (int q = i + 1; q < mc[k]; q++) {
                int s = st[mj[k][q]][mo[k][q]];
                if (s >= b) break;
                int e = s + dur[mj[k][q]][mo[k][q]];
                int hi = b < e ? b : e;
                for (int bb = s / BK; bb <= (hi - 1) / BK; bb++) hist[(size_t)k * NB + bb] += hf;
            }
        }
    }
    for (int k = 0; k < M; k++) {
        double c = 0; hcum[(size_t)k * (NB + 1)] = 0;
        for (int b = 0; b < NB; b++) { c += hist[(size_t)k * NB + b]; hcum[(size_t)k * (NB + 1) + b + 1] = c; }
    }
}

static float *fbuf, *cbuf, *gbuf;
static unsigned short *arg;   /* [M][nd] */
static int maxnd;

/* reroute job j (already removed from machine lists).
 * BAND > 0 restricts op o's delay index to [cur_o - BAND, cur_o + BAND]; the
 * current path is always inside, so a banded reroute never gets worse. */
static int BAND = 0;
static double HM = 0.0;
static int frozen[NN];
static double HWADD = 1.0;
static int *dlo = NULL, *dhi = NULL;   /* multiplicative history on present overlap */
static int *blo, *bhi;
static double route(int j, double pf, double hw, double tie) {
    int nd = slack[j] / R + 1;
    float *f = fbuf, *c = cbuf, *g = gbuf;
    int plo = 0, phi = nd - 1;
    for (int o = 0; o < M; o++) {
        int lo = 0, hi = nd - 1;
        if (BAND) {
            int cur = (st[j][o] - pref[j][o]) / R;
            lo = cur - BAND; hi = cur + BAND;
            if (lo < 0) lo = 0;
            if (hi > nd - 1) hi = nd - 1;
            if (lo < plo) lo = plo;   /* monotone: D_o >= some feasible D_{o-1} */
        }
        if (dlo) {   /* propagated domain of this op, on the delay grid */
            int a = (dlo[j * M + o] - pref[j][o] + R - 1) / R, b = (dhi[j * M + o] - pref[j][o]) / R;
            if (b > nd - 1) b = nd - 1;
            if (a > lo) lo = a;
            if (b < hi) hi = b;
            if (lo < plo) lo = plo;
        }
        if (hi < lo) hi = lo;
        blo[o] = lo; bhi[o] = hi;
        int k = mach[j][o], d = dur[j][o], t0 = pref[j][o];
        double *hc = hcum + (size_t)k * (NB + 1);
        for (int i = lo; i <= hi; i++) {
            int t = t0 + i * R;
            int b0 = t / BK, b1 = (t + d - 1) / BK + 1;
            if (b1 > NB) b1 = NB;
            c[i] = (float)(hw * BK * (hc[b1] - hc[b0]) + tie * i);
        }
        int tmin = t0 + lo * R, tmax = t0 + hi * R + d;
        for (int q = 0; q < mc[k]; q++) {
            int jj = mj[k][q], s = st[jj][mo[k][q]];
            if (s >= tmax) break;
            int e = s + dur[jj][mo[k][q]];
            if (e <= tmin) continue;
            int ilo = s - d - t0 < 0 ? 0 : (s - d - t0) / R;
            if (ilo < lo) ilo = lo;
            int ihi = (e - 1 - t0) / R; if (ihi > hi) ihi = hi;
            for (int i = ilo; i <= ihi; i++) {
                int t = t0 + i * R;
                int a = t > s ? t : s, b = t + d < e ? t + d : e;
                if (b > a) c[i] += (float)(pf * (b - a) * (1.0 + HM * hist[(size_t)k * NB + a / BK]));
            }
        }
        unsigned short *ar = arg + (size_t)o * maxnd;
        if (o == 0) { for (int i = lo; i <= hi; i++) { g[i] = c[i]; ar[i] = i; } }
        else {
            /* prefix min of f (= g_{o-1} on [plo,phi]) over D' <= D */
            float best = 3e38f; int bi = plo;
            int i = plo;
            for (; i < lo && i <= phi; i++) if (f[i] < best || (f[i] == best && (rnd() & 7) == 0)) { best = f[i]; bi = i; }
            for (i = lo; i <= hi; i++) {
                if (i <= phi && (f[i] < best || (f[i] == best && (rnd() & 7) == 0))) { best = f[i]; bi = i; }
                ar[i] = (unsigned short)bi;
                g[i] = best + c[i];
            }
        }
        float *tmp = f; f = g; g = tmp;
        plo = lo; phi = hi;
    }
    float best = 3e38f; int bi = plo;
    for (int i = plo; i <= phi; i++) if (f[i] < best) { best = f[i]; bi = i; }
    int D = bi;
    for (int o = M - 1; o >= 0; o--) {
        st[j][o] = pref[j][o] + D * R;
        if (o > 0) D = arg[(size_t)o * maxnd + D];
    }
    return best;
}

static void read_instance(const char *p) {
    FILE *f = fopen(p, "r"); if (!f) { perror(p); exit(1); }
    if (fscanf(f, "%d %d", &N, &M) != 2) exit(1);
    for (int j = 0; j < N; j++) {
        pref[j][0] = 0;
        for (int o = 0; o < M; o++) { if (fscanf(f, "%d %d", &mach[j][o], &dur[j][o]) != 2) exit(1); pref[j][o + 1] = pref[j][o] + dur[j][o]; }
        L[j] = pref[j][M]; if (L[j] > LB) LB = L[j];
    }
    fclose(f);
}
static void write_sol(const char *p) {
    FILE *f = fopen(p, "w");
    for (int j = 0; j < N; j++) for (int o = 0; o < M; o++) fprintf(f, "%d%c", st[j][o], o == M - 1 ? '\n' : ' ');
    fclose(f);
}

int main(int argc, char **argv) {
    read_instance(argv[1]);
    const char *out = argv[2];
    T = atoi(argv[3]); if (T < LB) T = LB;
    int iters = atoi(argv[4]);
    rs = (unsigned)atoi(argv[5]) * 2654435761u + 7;
    R = atoi(argv[6]);
    double pf0 = getenv("PF0") ? atof(getenv("PF0")) : 0.5, pfm = getenv("PFM") ? atof(getenv("PFM")) : 1.3;
    double pfmax = getenv("PFMAX") ? atof(getenv("PFMAX")) : 1e30;
    double hf = getenv("HF") ? atof(getenv("HF")) : 0.2, tie = getenv("TIE") ? atof(getenv("TIE")) : 0.0;
    NB = (T + 2000) / BK + 2;
    hist = calloc((size_t)M * NB, sizeof(float)); hcum = calloc((size_t)M * (NB + 1), sizeof(double));
    maxnd = 0;
    long long switchr = getenv("SWITCHR") ? atoll(getenv("SWITCHR")) : 0;
    int Ralloc = switchr ? 1 : R;
    for (int j = 0; j < N; j++) { slack[j] = T - L[j]; if (slack[j] / Ralloc + 1 > maxnd) maxnd = slack[j] / Ralloc + 1; }
    fbuf = malloc(sizeof(float) * maxnd); cbuf = malloc(sizeof(float) * maxnd); gbuf = malloc(sizeof(float) * maxnd);
    blo = malloc(sizeof(int) * M); bhi = malloc(sizeof(int) * M);
    if (getenv("BAND")) BAND = atoi(getenv("BAND"));
    if (getenv("HM")) HM = atof(getenv("HM"));
    if (getenv("HW")) HWADD = atof(getenv("HW"));
    if (getenv("FREEZE")) { FILE *ff = fopen(getenv("FREEZE"), "r"); int x; while (ff && fscanf(ff, "%d", &x) == 1) frozen[x] = 1; if (ff) fclose(ff); }
    arg = malloc(sizeof(unsigned short) * (size_t)M * maxnd);
    if (maxnd > 65535) { fprintf(stderr, "R too small\n"); return 1; }
    int init = 0;
    if (argc > 7) {
        FILE *f = fopen(argv[7], "r");
        if (f) { init = 1; for (int j = 0; j < N; j++) for (int o = 0; o < M; o++) if (fscanf(f, "%d", &st[j][o]) != 1) init = 0; fclose(f); }
        if (init) for (int j = 0; j < N; j++) {   /* clip into corridor */
            int lim = T - dur[j][M - 1];
            for (int o = M - 1; o >= 0; o--) { if (st[j][o] > lim) st[j][o] = lim; if (st[j][o] < pref[j][o]) st[j][o] = pref[j][o]; if (o) lim = st[j][o] - dur[j][o - 1]; }
        }
    }
    if (getenv("DOMS")) {
        FILE *df = fopen(getenv("DOMS"), "r"); int NMt = N * M, ok = df != NULL;
        dlo = malloc(sizeof(int) * NMt); dhi = malloc(sizeof(int) * NMt);
        for (int q = 0; q < NMt && ok; q++) ok = fscanf(df, "%d", &dlo[q]) == 1;
        for (int q = 0; q < NMt && ok; q++) ok = fscanf(df, "%d", &dhi[q]) == 1;
        if (df) fclose(df);
        if (!ok) { fprintf(stderr, "bad DOMS\n"); return 1; }
    }
    if (!init) for (int j = 0; j < N; j++) for (int o = 0; o < M; o++) st[j][o] = pref[j][o] + (int)((long long)slack[j] * o / M) / R * R;
    if (dlo) for (int j = 0; j < N; j++) {   /* clip into domains, keep chains */
        int lim = 1 << 30;
        for (int o = M - 1; o >= 0; o--) { int h = dhi[j * M + o]; if (lim < h) h = lim; if (st[j][o] > h) st[j][o] = h; if (o) lim = st[j][o] - dur[j][o - 1]; }
        int pe = 0;
        for (int o = 0; o < M; o++) { int l = dlo[j * M + o]; if (pe > l) l = pe; if (st[j][o] < l) st[j][o] = l; pe = st[j][o] + dur[j][o]; }
    }
    for (int j = 0; j < N; j++) place_job(j);
    double t0 = now(), pf = pf0;
    int nc; long long ov = total_overlap(&nc), bestov = ov;
    fprintf(stderr, "LB=%d T=%d R=%d init overlap %lld conf machines %d\n", LB, T, R, ov, nc);
    int ord[NN];
    for (int it = 0; it < iters && ov > 0; it++) {
        for (int j = 0; j < N; j++) ord[j] = j;
        for (int j = N - 1; j > 0; j--) { int q = rnd() % (j + 1); int t = ord[j]; ord[j] = ord[q]; ord[q] = t; }
        for (int q = 0; q < N; q++) {
            int j = ord[q];
            if (frozen[j]) continue;
            if (overlap_job(j) == 0 && it > 0 && (rnd() % 4)) continue;   /* mostly leave legal jobs alone */
            remove_job(j); route(j, pf, HWADD, tie); place_job(j);
        }
        ov = total_overlap(&nc);
        hist_update(hf);
        pf *= pfm; if (pf > pfmax) pf = pfmax;
        if (ov < bestov) { bestov = ov; write_sol(out); }
        if (switchr && R > 1 && ov < switchr) {
            BAND *= R; R = 1;
            fprintf(stderr, "switch to exact grid R=1 band=%d\n", BAND);
        }
        fprintf(stderr, "it %d overlap %lld confm %d pf %.3g (%.1fs)\n", it, ov, nc, pf, now() - t0);
    }
    if (ov == 0) { write_sol(out); fprintf(stderr, "LEGAL at T=%d\n", T); }
    return 0;
}
