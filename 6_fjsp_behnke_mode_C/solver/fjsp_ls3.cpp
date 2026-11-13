// ---------------------------------------------------------------------------
// fjsp_ls3 -- parallel tabu / large-neighbourhood search for the flexible job
// shop, specialised (automatically) for work-centre instances such as the
// Behnke & Geiger set.
//
//   * two-vector encoding (machine per op, sequence per machine), decoded by a
//     longest-path pass that yields heads, tails, makespan and the number of
//     near-critical operations (secondary objective; the makespan landscape is
//     extremely flat);
//   * tabu search over the Mastrolilli-Gambardella insertion neighbourhood:
//     candidates ranked by an O(1) estimate, the best K evaluated exactly;
//   * ruin-and-recreate LNS with several ruin operators (random, machine
//     window, whole jobs, time window, near-critical set), greedy best-position
//     reinsertion; a third of the workers run an annealed (Metropolis) LNS;
//   * exact-order re-optimisation of the first and last work centre: with the
//     rest fixed, each is a partition problem whose within-machine order is
//     known (Jackson order for the first stage, ERD for the last), solved by a
//     move/swap local search after every LNS step;
//   * shared incumbent + elite pool, parameter diversity across threads, warm
//     start from any schedule file, stop at target.
//
// Build: g++ -O3 -march=native -pthread -std=c++17 -o fjsp_ls3 fjsp_ls3.cpp
// Usage: fjsp_ls3 -i inst.fjs -o out.sched [-p progress.csv] [-t threads]
//        [-s seconds] [-r seed] [-w warm.sched ...] [-T target] [-k K]
// ---------------------------------------------------------------------------
#include <bits/stdc++.h>
#include <atomic>
#include <mutex>
#include <thread>
using namespace std;

// ------------------------------ instance -----------------------------------
static int nJobs, nMach, nOps;
static vector<int> opJob, opIdx, jobPred, jobSucc, remWork, minDur, jobFirst, jobLen;
static vector<vector<int>> elig;      // op -> eligible machines (0-based)
static vector<vector<int>> DUR;       // op x machine, -1 if ineligible
static string instName;
// end stages
static vector<int> stageAOps, stageCOps;   // first ops / last ops
static vector<int> stageAMach, stageCMach; // their machines
static bool haveStageA = false, haveStageC = false;

static void parseInstance(const string& path) {
    ifstream f(path);
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); exit(1); }
    string first; getline(f, first);
    istringstream hs(first);
    hs >> nJobs >> nMach;
    vector<long long> body; for (long long v; f >> v;) body.push_back(v);
    struct RawOp { vector<pair<int,int>> opts; };
    vector<vector<RawOp>> jobs; size_t p = 0; int rawMin = INT_MAX, rawMax = INT_MIN;
    for (int j = 0; j < nJobs; j++) {
        int no = (int)body[p++]; vector<RawOp> job(no);
        for (int k = 0; k < no; k++) {
            int ne = (int)body[p++];
            for (int e = 0; e < ne; e++) { int m = (int)body[p++], d = (int)body[p++]; rawMin = min(rawMin, m); rawMax = max(rawMax, m); job[k].opts.push_back({m, d}); }
        }
        jobs.push_back(job);
    }
    int off = (rawMax >= nMach) ? 1 : (rawMin == 0 ? 0 : 1);
    nOps = 0; for (auto& j : jobs) nOps += (int)j.size();
    opJob.assign(nOps, 0); opIdx.assign(nOps, 0); jobPred.assign(nOps, -1); jobSucc.assign(nOps, -1);
    elig.assign(nOps, {}); DUR.assign(nOps, vector<int>(nMach, -1)); minDur.assign(nOps, 0);
    jobFirst.assign(nJobs, -1); jobLen.assign(nJobs, 0);
    int o = 0;
    for (int j = 0; j < nJobs; j++) {
        jobFirst[j] = o; jobLen[j] = (int)jobs[j].size();
        for (int k = 0; k < (int)jobs[j].size(); k++, o++) {
            opJob[o] = j; opIdx[o] = k;
            jobPred[o] = k == 0 ? -1 : o - 1; jobSucc[o] = k + 1 == (int)jobs[j].size() ? -1 : o + 1;
            int mn = INT_MAX;
            for (auto& pr : jobs[j][k].opts) { int m = pr.first - off; if (DUR[o][m] < 0 || pr.second < DUR[o][m]) DUR[o][m] = pr.second; mn = min(mn, pr.second); }
            for (int m = 0; m < nMach; m++) if (DUR[o][m] >= 0) elig[o].push_back(m);
            minDur[o] = mn;
        }
    }
    remWork.assign(nOps, 0);
    for (int q = nOps - 1; q >= 0; q--) remWork[q] = minDur[q] + (jobSucc[q] >= 0 ? remWork[jobSucc[q]] : 0);
    size_t a = path.find_last_of('/'); instName = a == string::npos ? path : path.substr(a + 1);
    size_t dot = instName.find_last_of('.'); if (dot != string::npos) instName = instName.substr(0, dot);

    // end stages: machines used by first ops / last ops; valid only if no other op uses them
    vector<char> inA(nMach, 0), inC(nMach, 0);
    for (int q = 0; q < nOps; q++) {
        if (jobPred[q] < 0) { stageAOps.push_back(q); for (int m : elig[q]) inA[m] = 1; }
        if (jobSucc[q] < 0) { stageCOps.push_back(q); for (int m : elig[q]) inC[m] = 1; }
    }
    haveStageA = haveStageC = true;
    for (int q = 0; q < nOps; q++) {
        if (jobPred[q] >= 0) for (int m : elig[q]) if (inA[m]) haveStageA = false;
        if (jobSucc[q] >= 0) for (int m : elig[q]) if (inC[m]) haveStageC = false;
    }
    for (int m = 0; m < nMach; m++) { if (inA[m]) stageAMach.push_back(m); if (inC[m]) stageCMach.push_back(m); }
    if (!haveStageA) stageAMach.clear();
    if (!haveStageC) stageCMach.clear();
    fprintf(stderr, "[parse] %s: %d jobs x %d machines, %d ops, machine base %d; stage A %s (%zu machines), stage C %s (%zu machines)\n",
            instName.c_str(), nJobs, nMach, nOps, off, haveStageA ? "yes" : "no", stageAMach.size(), haveStageC ? "yes" : "no", stageCMach.size());
}

// ------------------------------ shared state -------------------------------
static mutex bestMtx;
static atomic<int> bestCmax(INT_MAX);
static vector<int> bestMach, bestStart;
static string outPath, progPath;
static atomic<bool> stopFlag(false);
static int targetCmax = 0;
static chrono::steady_clock::time_point T0;
static double elapsed() { return chrono::duration<double>(chrono::steady_clock::now() - T0).count(); }

struct Elite { int cmax; vector<int> mach, start; };
static vector<Elite> elites;
static size_t ELITE_MAX = 24;

static void writeSchedule(int cmax, const vector<int>& mach, const vector<int>& st) {
    string tmp = outPath + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return;
    fprintf(f, "instance %s\njobs %d\nmachines %d\noperations %d\nmakespan %d\n# fjsp_ls3; machine ids 0-based\n# job op machine start end\n",
            instName.c_str(), nJobs, nMach, nOps, cmax);
    for (int o = 0; o < nOps; o++) fprintf(f, "%d %d %d %d %d\n", opJob[o], opIdx[o], mach[o], st[o], st[o] + DUR[o][mach[o]]);
    fclose(f);
    rename(tmp.c_str(), outPath.c_str());
}

// Load a schedule file (rows: job op machine start end; '#' comments; keyword lines skipped).
// Machine base is detected by validating eligibility/duration under base 1 then 0.
static bool loadScheduleFile(const string& path, vector<int>& mach, vector<int>& start, int& cmax) {
    ifstream f(path); if (!f) return false;
    vector<array<int,5>> rows; string line;
    while (getline(f, line)) {
        size_t h = line.find('#'); if (h != string::npos) line = line.substr(0, h);
        istringstream is(line); string w; if (!(is >> w)) continue;
        if (!isdigit((unsigned char)w[0]) && w[0] != '-') continue;   // keyword line
        array<int,5> r; r[0] = atoi(w.c_str());
        for (int i = 1; i < 5; i++) if (!(is >> r[i])) { r[0] = -1; break; }
        if (r[0] >= 0) rows.push_back(r);
    }
    if ((int)rows.size() != nOps) { fprintf(stderr, "[warm] %s: %zu rows, expected %d\n", path.c_str(), rows.size(), nOps); return false; }
    for (int base : {1, 0}) {
        vector<int> mm(nOps, -1), ss(nOps, -1); bool ok = true;
        for (auto& r : rows) {
            int j = r[0], o = r[1], m = r[2] - base, s = r[3], e = r[4];
            if (j < 0 || j >= nJobs || o < 0 || o >= jobLen[j] || m < 0 || m >= nMach) { ok = false; break; }
            int id = jobFirst[j] + o;
            if (DUR[id][m] < 0 || e - s != DUR[id][m] || mm[id] >= 0) { ok = false; break; }
            mm[id] = m; ss[id] = s;
        }
        if (!ok) continue;
        // feasibility: precedence + no overlap
        for (int o = 0; o < nOps; o++) if (jobPred[o] >= 0 && ss[o] < ss[jobPred[o]] + DUR[jobPred[o]][mm[jobPred[o]]]) ok = false;
        vector<vector<pair<int,int>>> per(nMach);
        for (int o = 0; o < nOps; o++) per[mm[o]].push_back({ss[o], o});
        for (int m = 0; m < nMach && ok; m++) { sort(per[m].begin(), per[m].end()); for (size_t i = 1; i < per[m].size(); i++) if (per[m][i].first < per[m][i-1].first + DUR[per[m][i-1].second][m]) ok = false; }
        if (!ok) continue;
        int c = 0; for (int o = 0; o < nOps; o++) c = max(c, ss[o] + DUR[o][mm[o]]);
        mach = mm; start = ss; cmax = c; return true;
    }
    fprintf(stderr, "[warm] %s: not a feasible schedule under either machine base\n", path.c_str());
    return false;
}

// -------------------------------- worker -----------------------------------
struct Worker {
    mt19937 rng;
    vector<int> mach, pos, head, tail, dur, topo, indeg, tmpq, curPos, curMach;
    vector<vector<int>> seq;
    vector<int> tabu;                    // op*nMach+machine -> iteration until forbidden
    int Cmax = INT_MAX, nearCrit = 0; long long score = LLONG_MAX;
    int SLACK_T = 4;
    long long iter = 0, totalIter = 0;
    int tid = 0, lnsEvery = 0, K = 24;
    int ruinLo = 6, ruinHi = 50, probe = 6;
    double saT0 = 160000.0, saT1 = 8000.0; long long saCycle = 4000;

    Worker(uint32_t seed) : rng(seed) {
        mach.assign(nOps, 0); pos.assign(nOps, 0); head.assign(nOps, 0); tail.assign(nOps, 0); dur.assign(nOps, 0);
        topo.assign(nOps, 0); indeg.assign(nOps, 0); tmpq.assign(nOps, 0); seq.assign(nMach, {});
        tabu.assign((size_t)nOps * nMach, 0);
    }
    inline int rnd(int n) { return n <= 1 ? 0 : (int)(rng() % (uint32_t)n); }
    inline double unit() { return (rng() & 0xFFFFFF) / 16777216.0; }

    bool evaluate() {
        for (int o = 0; o < nOps; o++) pos[o] = -1;
        for (int m = 0; m < nMach; m++) for (size_t i = 0; i < seq[m].size(); i++) pos[seq[m][i]] = (int)i;
        for (int o = 0; o < nOps; o++) {
            dur[o] = mach[o] >= 0 ? DUR[o][mach[o]] : minDur[o];
            indeg[o] = (jobPred[o] >= 0 ? 1 : 0) + (pos[o] > 0 ? 1 : 0);
            head[o] = 0;
        }
        int qh = 0, qt = 0;
        for (int o = 0; o < nOps; o++) if (indeg[o] == 0) tmpq[qt++] = o;
        int cnt = 0;
        while (qh < qt) {
            int o = tmpq[qh++]; topo[cnt++] = o;
            int fin = head[o] + dur[o];
            int js = jobSucc[o];
            if (js >= 0) { if (fin > head[js]) head[js] = fin; if (--indeg[js] == 0) tmpq[qt++] = js; }
            if (mach[o] >= 0) { const vector<int>& S = seq[mach[o]]; if (pos[o] + 1 < (int)S.size()) { int ms = S[pos[o] + 1]; if (fin > head[ms]) head[ms] = fin; if (--indeg[ms] == 0) tmpq[qt++] = ms; } }
        }
        if (cnt != nOps) return false;
        Cmax = 0;
        for (int o = 0; o < nOps; o++) Cmax = max(Cmax, head[o] + dur[o]);
        for (int i = nOps - 1; i >= 0; i--) {
            int o = topo[i]; int t = 0;
            int js = jobSucc[o]; if (js >= 0) t = max(t, tail[js] + dur[js]);
            if (mach[o] >= 0) { const vector<int>& S = seq[mach[o]]; if (pos[o] + 1 < (int)S.size()) { int ms = S[pos[o] + 1]; t = max(t, tail[ms] + dur[ms]); } }
            tail[o] = t;
        }
        nearCrit = 0;
        for (int o = 0; o < nOps; o++) if (Cmax - (head[o] + dur[o] + tail[o]) <= SLACK_T) nearCrit++;
        score = (long long)Cmax * 100000LL + nearCrit;
        return true;
    }

    // ---- constructions ----
    void balancedAssign(vector<int>& asg, long long iters) {
        vector<long long> load(nMach, 0);
        vector<int> ord(nOps); iota(ord.begin(), ord.end(), 0); shuffle(ord.begin(), ord.end(), rng);
        for (int o : ord) { int bm = -1; long long bv = LLONG_MAX; for (int m : elig[o]) { long long v = load[m] + DUR[o][m]; if (v < bv) { bv = v; bm = m; } } asg[o] = bm; load[bm] += DUR[o][bm]; }
        long long pot = 0; for (int m = 0; m < nMach; m++) pot += load[m] * load[m];
        vector<int> best = asg; long long bestMax = *max_element(load.begin(), load.end());
        double Ta = 3e4, Tb = 0.5;
        for (long long it = 0; it < iters; it++) {
            double T = Ta * pow(Tb / Ta, (double)it / iters);
            int o = rnd(nOps); int cm = asg[o]; int nm = elig[o][rnd((int)elig[o].size())]; if (nm == cm) continue;
            long long dc = DUR[o][cm], dn = DUR[o][nm];
            long long np = pot - load[cm] * load[cm] - load[nm] * load[nm] + (load[cm] - dc) * (load[cm] - dc) + (load[nm] + dn) * (load[nm] + dn);
            long long dl = np - pot;
            if (dl <= 0 || exp(-(double)dl / T) > unit()) { load[cm] -= dc; load[nm] += dn; asg[o] = nm; pot = np; long long c = *max_element(load.begin(), load.end()); if (c < bestMax) { bestMax = c; best = asg; } }
        }
        asg = best;
    }
    void scheduleFromAssign(const vector<int>& asg, int noise) {
        for (int m = 0; m < nMach; m++) seq[m].clear();
        mach = asg;
        vector<int> nextOp(nJobs, 0), jobReady(nJobs, 0), machFree(nMach, 0);
        vector<int> active(nJobs); iota(active.begin(), active.end(), 0);
        int done = 0;
        while (done < nOps) {
            int Cstar = INT_MAX, mStar = -1;
            for (int j : active) { int o = jobFirst[j] + nextOp[j]; int m = asg[o]; int c = max(jobReady[j], machFree[m]) + DUR[o][m]; if (c < Cstar) { Cstar = c; mStar = m; } }
            int pick = -1, bestPri = INT_MIN;
            for (int j : active) { int o = jobFirst[j] + nextOp[j]; if (asg[o] != mStar) continue; int st = max(jobReady[j], machFree[mStar]); if (st >= Cstar) continue; int pri = remWork[o] * 4 - st + (noise ? rnd(noise) : 0); if (pri > bestPri) { bestPri = pri; pick = o; } }
            int j = opJob[pick]; int st = max(jobReady[j], machFree[mStar]);
            seq[mStar].push_back(pick); machFree[mStar] = st + DUR[pick][mStar]; jobReady[j] = machFree[mStar];
            nextOp[j]++; done++;
            if (nextOp[j] == jobLen[j]) active.erase(find(active.begin(), active.end(), j));
        }
        evaluate();
    }
    void constructECT(double greed) {
        for (int m = 0; m < nMach; m++) seq[m].clear();
        vector<int> nextOp(nJobs, 0), jobReady(nJobs, 0), machFree(nMach, 0);
        vector<int> active(nJobs); iota(active.begin(), active.end(), 0);
        vector<pair<int, pair<int,int>>> cand; vector<int> pool; int done = 0;
        while (done < nOps) {
            cand.clear(); int bestC = INT_MAX;
            for (int j : active) { int o = jobFirst[j] + nextOp[j]; for (int m : elig[o]) { int c = max(jobReady[j], machFree[m]) + DUR[o][m]; cand.push_back({c, {j, m}}); bestC = min(bestC, c); } }
            int thr = bestC + (int)(greed * bestC / 100.0);
            pool.clear(); for (size_t i = 0; i < cand.size(); i++) if (cand[i].first <= thr) pool.push_back((int)i);
            int pick = pool[rnd((int)pool.size())];
            int j = cand[pick].second.first, m = cand[pick].second.second; int o = jobFirst[j] + nextOp[j];
            int st = max(jobReady[j], machFree[m]);
            mach[o] = m; seq[m].push_back(o); machFree[m] = st + DUR[o][m]; jobReady[j] = machFree[m];
            nextOp[j]++; done++;
            if (nextOp[j] == jobLen[j]) active.erase(find(active.begin(), active.end(), j));
        }
        evaluate();
    }
    void freshStart() {
        int r = rnd(10);
        if (r < 6) { vector<int> asg(nOps); balancedAssign(asg, 300000 + 3000LL * nOps); scheduleFromAssign(asg, r < 3 ? 0 : 40); }
        else constructECT(rnd(3) == 0 ? 0.0 : (double)rnd(12));
    }

    // ---- moves ----
    struct Move { int v, k, p, est; };
    vector<Move> cands; vector<int> critical;
    void doMove(int v, int a, int ap, int k, int p) { seq[a].erase(seq[a].begin() + ap); seq[k].insert(seq[k].begin() + p, v); mach[v] = k; }
    void undoMove(int v, int a, int ap, int k, int p) { seq[k].erase(seq[k].begin() + p); seq[a].insert(seq[a].begin() + ap, v); mach[v] = a; }
    void collectCritical() { critical.clear(); for (int o = 0; o < nOps; o++) if (head[o] + dur[o] + tail[o] == Cmax) critical.push_back(o); }
    void buildCandidates() {
        cands.clear();
        for (int v : critical) {
            int jp = jobPred[v], js = jobSucc[v];
            int rJ = jp >= 0 ? head[jp] + dur[jp] : 0, qJ = js >= 0 ? tail[js] + dur[js] : 0;
            int a = mach[v], ap = pos[v];
            for (int k : elig[v]) {
                const vector<int>& S = seq[k]; int len = (int)S.size() - (k == a ? 1 : 0); int dv = DUR[v][k];
                for (int p = 0; p <= len; p++) {
                    int prev = -1, next = -1;
                    if (k == a) { if (p == ap) continue; int ip = p < ap ? p - 1 : p; int in_ = p < ap ? p : p + 1; if (ip >= 0) prev = S[ip]; if (in_ < (int)S.size()) next = S[in_]; }
                    else { if (p - 1 >= 0) prev = S[p - 1]; if (p < (int)S.size()) next = S[p]; }
                    int st = rJ; if (prev >= 0) st = max(st, head[prev] + dur[prev]);
                    int q = qJ; if (next >= 0) q = max(q, tail[next] + dur[next]);
                    cands.push_back({v, k, p, st + dv + q});
                }
            }
        }
    }

    // ---- end-stage re-optimisation (partition with known within-machine order) ----
    vector<int> stKey, stAsg, stBest; vector<vector<int>> stBucket; vector<int> mval;
    // stage 0 = first ops (tails, Jackson order), stage 1 = last ops (releases, ERD order)
    bool optEndStage(int stage, int descent) {
        const vector<int>& ops = stage == 0 ? stageAOps : stageCOps;
        const vector<int>& M = stage == 0 ? stageAMach : stageCMach;
        if (M.empty()) return false;
        const bool rel = (stage == 1); const int Mn = (int)M.size();
        vector<int> midx(nMach, -1); for (int i = 0; i < Mn; i++) midx[M[i]] = i;
        int n = (int)ops.size();
        stKey.assign(n, 0);
        for (int i = 0; i < n; i++) { int o = ops[i]; if (rel) { int jp = jobPred[o]; stKey[i] = jp >= 0 ? head[jp] + dur[jp] : 0; } else { int js = jobSucc[o]; stKey[i] = js >= 0 ? tail[js] + dur[js] : 0; } }
        auto before = [&](int a, int b) { return rel ? stKey[a] < stKey[b] : stKey[a] > stKey[b]; };
        auto machVal = [&](int m) { int t = 0, v = 0; for (int i : stBucket[m]) { int o = ops[i]; int d = DUR[o][M[m]]; if (rel) { t = max(t, stKey[i]) + d; v = t; } else { t += d; v = max(v, t + stKey[i]); } } return v; };
        auto insertSorted = [&](int m, int i) { auto& B = stBucket[m]; size_t p = 0; while (p < B.size() && !before(i, B[p])) p++; B.insert(B.begin() + p, i); };
        // start from the current assignment when every op sits in the stage, otherwise greedy
        stBucket.assign(Mn, {}); stAsg.assign(n, -1);
        bool cur_ok = true;
        for (int i = 0; i < n && cur_ok; i++) { int m = mach[ops[i]]; if (m < 0 || midx[m] < 0) cur_ok = false; else stAsg[i] = midx[m]; }
        if (cur_ok) { for (int i = 0; i < n; i++) insertSorted(stAsg[i], i); }
        else {
            vector<int> ord(n); iota(ord.begin(), ord.end(), 0); sort(ord.begin(), ord.end(), before);
            vector<int> t(Mn, 0), v(Mn, 0);
            for (int i : ord) {
                int o = ops[i]; int bm = -1, bv = INT_MAX, bt = 0;
                for (int m : elig[o]) { int mm = midx[m]; if (mm < 0) continue; int nt, nv; if (rel) { nt = max(t[mm], stKey[i]) + DUR[o][m]; nv = nt; } else { nt = t[mm] + DUR[o][m]; nv = max(v[mm], nt + stKey[i]); } if (nv < bv || (nv == bv && nt < bt)) { bv = nv; bm = mm; bt = nt; } }
                if (bm < 0) return false;
                stAsg[i] = bm; t[bm] = bt; v[bm] = bv; stBucket[bm].push_back(i);
            }
        }
        mval.assign(Mn, 0); for (int m = 0; m < Mn; m++) mval[m] = machVal(m);
        auto objOf = [&]() { long long mx = 0, ss = 0; for (int m = 0; m < Mn; m++) { mx = max(mx, (long long)mval[m]); ss += (long long)mval[m] * mval[m]; } return mx * (1LL << 40) + ss; };
        long long cur = objOf(), best = cur; stBest = stAsg;
        for (int it = 0; it < descent; it++) {
            if (rnd(3) == 0 && Mn > 1) {
                // swap two ops of different machines
                int i = rnd(n), j = rnd(n); int mi = stAsg[i], mj = stAsg[j]; if (mi == mj) continue;
                if (DUR[ops[i]][M[mj]] < 0 || DUR[ops[j]][M[mi]] < 0) continue;
                auto& Bi = stBucket[mi]; auto& Bj = stBucket[mj];
                Bi.erase(find(Bi.begin(), Bi.end(), i)); Bj.erase(find(Bj.begin(), Bj.end(), j));
                insertSorted(mj, i); insertSorted(mi, j); stAsg[i] = mj; stAsg[j] = mi;
                int si = mval[mi], sj = mval[mj]; mval[mi] = machVal(mi); mval[mj] = machVal(mj);
                long long v = objOf();
                if (v <= cur) { cur = v; if (v < best) { best = v; stBest = stAsg; } }
                else { mval[mi] = si; mval[mj] = sj; Bi.erase(find(Bi.begin(), Bi.end(), j)); Bj.erase(find(Bj.begin(), Bj.end(), i)); insertSorted(mi, i); insertSorted(mj, j); stAsg[i] = mi; stAsg[j] = mj; }
            } else {
                int i = rnd(n); int o = ops[i]; int nmm = elig[o][rnd((int)elig[o].size())]; int nm = midx[nmm];
                if (nm < 0 || nm == stAsg[i]) continue;
                int om = stAsg[i];
                auto& B = stBucket[om]; B.erase(find(B.begin(), B.end(), i));
                insertSorted(nm, i); stAsg[i] = nm;
                int so = mval[om], sn = mval[nm]; mval[om] = machVal(om); mval[nm] = machVal(nm);
                long long v = objOf();
                if (v <= cur) { cur = v; if (v < best) { best = v; stBest = stAsg; } }
                else { mval[om] = so; mval[nm] = sn; auto& C = stBucket[nm]; C.erase(find(C.begin(), C.end(), i)); insertSorted(om, i); stAsg[i] = om; }
            }
        }
        vector<int> saveMach = mach; vector<vector<int>> saveSeq = seq; long long saveScore = score;
        vector<vector<int>> nb(Mn); for (int i = 0; i < n; i++) nb[stBest[i]].push_back(i);
        for (int m = 0; m < Mn; m++) { sort(nb[m].begin(), nb[m].end(), before); seq[M[m]].clear(); for (int i : nb[m]) { seq[M[m]].push_back(ops[i]); mach[ops[i]] = M[m]; } }
        if (!evaluate() || score > saveScore) { mach = saveMach; seq = saveSeq; evaluate(); return false; }
        return score < saveScore;
    }
    bool polishEnds(int descent) {
        bool any = false;
        if (haveStageC && optEndStage(1, descent)) any = true;
        if (haveStageA && optEndStage(0, descent)) any = true;
        if (haveStageC && optEndStage(1, descent)) any = true;
        return any;
    }

    // ---- ruin & recreate ----
    vector<int> ruined;
    void removeOp(int v) { auto& S = seq[mach[v]]; S.erase(find(S.begin(), S.end(), v)); mach[v] = -1; }
    void ruin(int k) {
        ruined.clear();
        int mode = rnd(5);
        if (mode == 0) {                                  // random ops
            vector<int> perm(nOps); iota(perm.begin(), perm.end(), 0);
            for (int i = 0; i < k && i < nOps; i++) { swap(perm[i], perm[i + rnd(nOps - i)]); ruined.push_back(perm[i]); }
        } else if (mode == 1) {                           // window on a busy machine
            int m = -1, bestLoad = -1;
            for (int t = 0; t < 3; t++) { int c = rnd(nMach); int load = 0; for (int o : seq[c]) load += DUR[o][c]; if (load > bestLoad) { bestLoad = load; m = c; } }
            if (seq[m].empty()) { ruin(k); return; }
            int start = rnd((int)seq[m].size());
            for (int i = start; i < (int)seq[m].size() && (int)ruined.size() < k; i++) ruined.push_back(seq[m][i]);
            for (int i = start - 1; i >= 0 && (int)ruined.size() < k; i--) ruined.push_back(seq[m][i]);
        } else if (mode == 2) {                           // whole jobs
            while ((int)ruined.size() < k) { int j = rnd(nJobs); for (int o = jobFirst[j]; o < jobFirst[j] + jobLen[j]; o++) if (find(ruined.begin(), ruined.end(), o) == ruined.end()) ruined.push_back(o); }
        } else if (mode == 3) {                           // time window across all machines
            int t0 = rnd(max(1, Cmax)); vector<pair<int,int>> byStart;
            for (int o = 0; o < nOps; o++) byStart.push_back({abs(head[o] - t0), o});
            nth_element(byStart.begin(), byStart.begin() + min(k, nOps) - 1, byStart.end());
            for (int i = 0; i < k && i < nOps; i++) ruined.push_back(byStart[i].second);
        } else {                                          // near-critical operations
            vector<int> nc; for (int o = 0; o < nOps; o++) if (Cmax - (head[o] + dur[o] + tail[o]) <= SLACK_T + 2) nc.push_back(o);
            shuffle(nc.begin(), nc.end(), rng);
            for (int i = 0; i < (int)nc.size() && (int)ruined.size() < k; i++) ruined.push_back(nc[i]);
            if ((int)ruined.size() < k) { vector<int> perm(nOps); iota(perm.begin(), perm.end(), 0); shuffle(perm.begin(), perm.end(), rng); for (int o : perm) { if ((int)ruined.size() >= k) break; if (find(ruined.begin(), ruined.end(), o) == ruined.end()) ruined.push_back(o); } }
        }
        for (int v : ruined) removeOp(v);
    }
    bool recreate(int probeN) {
        shuffle(ruined.begin(), ruined.end(), rng);
        // reinsert in job order within the shuffled list so predecessors go first: stable partition by opIdx
        stable_sort(ruined.begin(), ruined.end(), [&](int a, int b) { return opIdx[a] < opIdx[b]; });
        for (int v : ruined) {
            if (!evaluate()) return false;
            int jp = jobPred[v], js = jobSucc[v];
            int rJ = jp >= 0 ? head[jp] + dur[jp] : 0, qJ = js >= 0 ? tail[js] + dur[js] : 0;
            cands.clear();
            for (int k : elig[v]) { const vector<int>& S = seq[k]; int dv = DUR[v][k]; for (int p = 0; p <= (int)S.size(); p++) { int st = rJ, q = qJ; if (p > 0) st = max(st, head[S[p-1]] + dur[S[p-1]]); if (p < (int)S.size()) q = max(q, tail[S[p]] + dur[S[p]]); cands.push_back({v, k, p, st + dv + q}); } }
            int kk = min((int)cands.size(), probeN);
            partial_sort(cands.begin(), cands.begin() + kk, cands.end(), [&](const Move& x, const Move& y) { return x.est < y.est; });
            long long bestVal = LLONG_MAX; int bk = -1, bp = -1;
            for (int i = 0; i < kk; i++) { Move& mv = cands[i]; seq[mv.k].insert(seq[mv.k].begin() + mv.p, v); mach[v] = mv.k; bool ok = evaluate(); long long val = ok ? score * 4 + rnd(4) : LLONG_MAX; seq[mv.k].erase(seq[mv.k].begin() + mv.p); mach[v] = -1; if (ok && val < bestVal) { bestVal = val; bk = mv.k; bp = mv.p; } }
            if (bk < 0) { for (int k : elig[v]) { for (int p = 0; p <= (int)seq[k].size(); p++) { seq[k].insert(seq[k].begin() + p, v); mach[v] = k; bool ok = evaluate(); long long val = ok ? score : LLONG_MAX; seq[k].erase(seq[k].begin() + p); mach[v] = -1; if (ok && val < bestVal) { bestVal = val; bk = k; bp = p; } } if (bk >= 0) break; } }
            if (bk < 0) return false;
            seq[bk].insert(seq[bk].begin() + bp, v); mach[v] = bk;
        }
        ruined.clear();
        return evaluate();
    }
    bool lnsPass(int k, int probeN, long long tol = 0) {
        vector<int> saveMach = mach; vector<vector<int>> saveSeq = seq; long long saveScore = score; int saveC = Cmax;
        ruin(k);
        if (!recreate(probeN)) { mach = saveMach; seq = saveSeq; evaluate(); return false; }
        polishEnds(300);
        if (score > saveScore + tol) { mach = saveMach; seq = saveSeq; evaluate(); return false; }
        return Cmax < saveC || score < saveScore;
    }
    void lnsBurst(int n, long long tol) { for (int i = 0; i < n; i++) lnsPass(ruinLo + rnd(ruinHi - ruinLo + 1), probe, tol); }
    void perturb(int strength) {
        for (int t = 0; t < strength; t++) {
            int v = rnd(nOps); int k = elig[v][rnd((int)elig[v].size())]; int a = mach[v], ap = pos[v];
            int len = (int)seq[k].size() - (k == a ? 1 : 0); int p = rnd(len + 1);
            if (k == a && p == ap) continue;
            doMove(v, a, ap, k, p);
            if (!evaluate()) { undoMove(v, a, ap, k, p); evaluate(); }
        }
    }

    // ---- shared pool ----
    void reportIfBest() {
        if (Cmax < bestCmax.load()) {
            lock_guard<mutex> lk(bestMtx);
            if (Cmax < bestCmax.load()) {
                bestCmax.store(Cmax); bestMach = mach; bestStart = head;
                writeSchedule(Cmax, mach, head);
                double el = elapsed();
                if (!progPath.empty()) { FILE* pf = fopen(progPath.c_str(), "a"); if (pf) { fprintf(pf, "%.3f,%d\n", el, Cmax); fclose(pf); } }
                fprintf(stderr, "[%8.2fs] makespan %d (worker %d)\n", el, Cmax, tid);
                if (targetCmax > 0 && Cmax <= targetCmax) stopFlag.store(true);
            }
        }
    }
    void pushElite() {
        lock_guard<mutex> lk(bestMtx);
        for (auto& e : elites) if (e.cmax == Cmax && e.mach == mach) return;
        if (elites.size() >= ELITE_MAX && Cmax >= elites.back().cmax) return;
        elites.push_back({Cmax, mach, head});
        sort(elites.begin(), elites.end(), [](const Elite& a, const Elite& b) { return a.cmax < b.cmax; });
        if (elites.size() > ELITE_MAX) elites.resize(ELITE_MAX);
    }
    void loadFrom(const vector<int>& mm, const vector<int>& ss) {
        mach = mm; vector<vector<pair<int,int>>> tmp(nMach);
        for (int o = 0; o < nOps; o++) tmp[mach[o]].push_back({ss[o], o});
        for (int m = 0; m < nMach; m++) { sort(tmp[m].begin(), tmp[m].end()); seq[m].clear(); for (auto& pr : tmp[m]) seq[m].push_back(pr.second); }
        evaluate();
    }
    bool loadRandomElite() {
        vector<int> mm, ss;
        { lock_guard<mutex> lk(bestMtx); if (elites.empty()) return false; const Elite& e = elites[rnd((int)elites.size())]; mm = e.mach; ss = e.start; }
        loadFrom(mm, ss); return true;
    }
    void startFromEliteOrFresh() {
        // odd workers start fresh (diversity), even workers from a warm start / elite
        if (tid % 2 == 1 || !loadRandomElite()) { freshStart(); }
    }

    // ---- annealed LNS worker ----
    void runSaLns(double seconds) {
        startFromEliteOrFresh();
        polishEnds(3000); reportIfBest(); pushElite();
        long long curScore = score;
        vector<int> bMach = mach; vector<vector<int>> bSeq = seq; long long bScore = score;
        long long cycle = 0, sinceImp = 0;
        while (!stopFlag.load()) {
            if ((cycle & 15) == 0 && elapsed() > seconds) break;
            cycle++;
            double frac = (double)(cycle % saCycle) / (double)saCycle;
            double T = saT0 * pow(saT1 / saT0, frac);
            vector<int> sMach = mach; vector<vector<int>> sSeq = seq;
            ruin(ruinLo + rnd(ruinHi - ruinLo + 1));
            if (!recreate(probe)) { mach = sMach; seq = sSeq; evaluate(); continue; }
            polishEnds(400);
            long long d = score - curScore;
            bool accept = (d <= 0) || (exp(-(double)d / T) > unit());
            if (!accept) { mach = sMach; seq = sSeq; evaluate(); }
            else {
                curScore = score;
                if (score < bScore) {
                    bScore = score; bMach = mach; bSeq = seq; sinceImp = 0;
                    polishEnds(4000);
                    if (score < bScore) { bScore = score; bMach = mach; bSeq = seq; }
                    curScore = score;
                    reportIfBest(); pushElite();
                }
            }
            if (++sinceImp > 6000) {
                sinceImp = 0;
                if (rnd(3) == 0) { freshStart(); polishEnds(2000); }
                else if (!loadRandomElite()) { mach = bMach; seq = bSeq; evaluate(); }
                curScore = score; bScore = min(bScore, score);
                if (score < bScore || bMach.empty()) { bScore = score; bMach = mach; bSeq = seq; }
            }
        }
    }

    // ---- tabu worker ----
    void run(double seconds) {
        startFromEliteOrFresh();
        polishEnds(2000);
        int localBest = Cmax; vector<int> lbMach = mach; vector<vector<int>> lbSeq = seq;
        reportIfBest(); pushElite();
        long long sinceImp = 0, sinceRestart = 0; int tenureBase = 8 + rnd(12);
        const long long stagLimit = 2000 + 4LL * nOps + rnd(2000);
        while (!stopFlag.load()) {
            if ((totalIter & 31) == 0 && elapsed() > seconds) break;
            iter++; totalIter++; sinceRestart++;
            if (lnsEvery && (totalIter % lnsEvery == 0)) {
                if (lnsPass(ruinLo + rnd(ruinHi - ruinLo + 1), probe) && Cmax < localBest) { localBest = Cmax; lbMach = mach; lbSeq = seq; sinceImp = 0; reportIfBest(); pushElite(); }
            }
            collectCritical(); buildCandidates();
            if (cands.empty()) { perturb(10); continue; }
            int kk = min((int)cands.size(), K);
            partial_sort(cands.begin(), cands.begin() + kk, cands.end(), [&](const Move& x, const Move& y) { return x.est < y.est; });
            long long bestVal = LLONG_MAX, bestTabuVal = LLONG_MAX;
            int bv = -1, bk = -1, bp = -1, ba = -1, bap = -1, tv = -1, tk = -1, tp = -1, ta = -1, tap = -1;
            curPos = pos; curMach = mach;
            int gbest = bestCmax.load();
            for (int i = 0; i < kk; i++) {
                Move& mv = cands[i]; int a = curMach[mv.v], ap = curPos[mv.v];
                doMove(mv.v, a, ap, mv.k, mv.p);
                bool ok = evaluate(); long long val = ok ? score * 8 + rnd(8) : LLONG_MAX; int valC = ok ? Cmax : INT_MAX;
                undoMove(mv.v, a, ap, mv.k, mv.p);
                if (!ok) continue;
                bool isTabu = tabu[(size_t)mv.v * nMach + a] > (int)iter;
                if (isTabu && valC >= gbest) { if (val < bestTabuVal) { bestTabuVal = val; tv = mv.v; tk = mv.k; tp = mv.p; ta = a; tap = ap; } continue; }
                if (val < bestVal) { bestVal = val; bv = mv.v; bk = mv.k; bp = mv.p; ba = a; bap = ap; }
            }
            if (bv < 0) { if (tv < 0) { evaluate(); perturb(8); continue; } bv = tv; bk = tk; bp = tp; ba = ta; bap = tap; }
            doMove(bv, ba, bap, bk, bp);
            if (!evaluate()) { undoMove(bv, ba, bap, bk, bp); evaluate(); perturb(5); continue; }
            tabu[(size_t)bv * nMach + ba] = (int)iter + tenureBase + rnd(8);
            if (Cmax < localBest) {
                polishEnds(1500);
                localBest = Cmax; lbMach = mach; lbSeq = seq; sinceImp = 0;
                reportIfBest(); if (Cmax <= bestCmax.load() + 6) pushElite();
            } else if (++sinceImp > stagLimit) {
                sinceImp = 0; tenureBase = 8 + rnd(14);
                int r = rnd(10);
                if (sinceRestart > 120000 && r < 2) { sinceRestart = 0; freshStart(); polishEnds(1000); localBest = Cmax; lbMach = mach; lbSeq = seq; }
                else if (r < 5) { loadRandomElite(); lnsBurst(3 + rnd(6), 400000); }
                else { mach = lbMach; seq = lbSeq; evaluate(); if (rnd(2)) lnsBurst(2 + rnd(5), 300000); else perturb(15 + rnd(45)); }
                fill(tabu.begin(), tabu.end(), 0); iter = 0;
                if (Cmax < localBest) { localBest = Cmax; lbMach = mach; lbSeq = seq; }
                reportIfBest();
            }
        }
    }
};

// ---------------------------------- main -----------------------------------
int main(int argc, char** argv) {
    string inst; vector<string> warm; double seconds = 300; int threads = 8, K = 24; uint32_t seed0 = 1; int slack = 4;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-i") inst = argv[++i];
        else if (a == "-o") outPath = argv[++i];
        else if (a == "-p") progPath = argv[++i];
        else if (a == "-t") threads = atoi(argv[++i]);
        else if (a == "-s") seconds = atof(argv[++i]);
        else if (a == "-k") K = atoi(argv[++i]);
        else if (a == "-r") seed0 = (uint32_t)atoi(argv[++i]);
        else if (a == "-w") warm.push_back(argv[++i]);
        else if (a == "-T") targetCmax = atoi(argv[++i]);
        else if (a == "-S") slack = atoi(argv[++i]);
        else if (a == "-E") ELITE_MAX = (size_t)atoi(argv[++i]);
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 1; }
    }
    if (inst.empty() || outPath.empty()) { fprintf(stderr, "usage: %s -i inst -o out.sched [-p prog.csv] [-t threads] [-s seconds] [-r seed] [-w warm.sched]* [-T target] [-k K] [-S slack]\n", argv[0]); return 1; }
    parseInstance(inst);
    T0 = chrono::steady_clock::now();
    if (!progPath.empty()) { FILE* pf = fopen(progPath.c_str(), "w"); if (pf) { fprintf(pf, "seconds,makespan\n"); fclose(pf); } }
    // warm starts: the output file itself (if present) and any -w files
    vector<string> ws = warm; ws.insert(ws.begin(), outPath);
    for (auto& w : ws) {
        vector<int> mm, ss; int c;
        if (loadScheduleFile(w, mm, ss, c)) {
            elites.push_back({c, mm, ss});
            fprintf(stderr, "[warm] %s: makespan %d\n", w.c_str(), c);
            if (c < bestCmax.load()) { bestCmax.store(c); bestMach = mm; bestStart = ss; if (w != outPath) writeSchedule(c, mm, ss); }
        }
    }
    sort(elites.begin(), elites.end(), [](const Elite& a, const Elite& b) { return a.cmax < b.cmax; });
    if (targetCmax > 0 && bestCmax.load() <= targetCmax) { fprintf(stderr, "target already reached (%d)\n", bestCmax.load()); printf("instance %s\nbest_makespan %d\ntime_to_best 0\n", instName.c_str(), bestCmax.load()); return 0; }

    vector<thread> th;
    for (int t = 0; t < threads; t++)
        th.emplace_back([&, t]() {
            Worker w(seed0 * 7919u + (uint32_t)t * 104729u + 17u);
            w.tid = t; w.K = K; w.SLACK_T = slack;
            // parameter diversity
            w.lnsEvery = (t % 3 == 2) ? 0 : (t % 3 == 1 ? 60 : 400);
            w.ruinLo = 4 + (t % 4) * 2;                    // 4,6,8,10
            w.ruinHi = min(nOps / 2, 20 + (t % 5) * 10);   // 20..60
            w.probe = 4 + (t % 3) * 2;                     // 4,6,8
            w.saT0 = 120000.0 + (t % 4) * 30000.0; w.saT1 = 5000.0 + (t % 3) * 3000.0; w.saCycle = 2500 + (t % 4) * 1000;
            if (t % 3 == 2) w.runSaLns(seconds); else w.run(seconds);
        });
    for (auto& x : th) x.join();
    fprintf(stderr, "final best %d\n", bestCmax.load());
    printf("instance %s\nbest_makespan %d\nthreads %d\nlimit %.0f\n", instName.c_str(), bestCmax.load(), threads, seconds);
    return 0;
}
