// ---------------------------------------------------------------------------
// Flexible job-shop solver for behnke16 (100 jobs x 5 ops, 20 machines).
//
// Structure of the instance: it is a 3-stage flexible flow shop.
//   stage A = machines 1-4   : op 0 of every job          (100 ops)
//   stage B = machines 5-16  : ops 1,2,3 of every job     (300 ops)
//   stage C = machines 17-20 : op 4 of every job          (100 ops)
// The best achievable per-stage max machine load is ~346 / ~318 / ~350, and a
// stage-C machine cannot start before a job's first four operations are done,
// so the makespan is essentially "when can stage C start" + "how well is stage
// C packed".  The search therefore has to get the machine assignment close to
// load-optimal *and* keep the bottleneck machines idle-free.
//
// Method: tabu search over the disjunctive graph.  A move takes a critical
// operation and reinserts it at some position on some eligible machine; moves
// are ranked by an O(1) makespan estimate from the current heads/tails and the
// most promising ones are evaluated exactly by a longest-path pass.  Several
// independent workers run in parallel and share an elite pool that they restart
// from.
// ---------------------------------------------------------------------------
#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <atomic>
using namespace std;

// ---------------- problem data (read once, then read-only) ----------------
static int nJobs, nMach, nOps;
static vector<int> opJob, opIdx, jobPred, jobSucc, remWork;
static vector<vector<int>> elig;   // op -> eligible machines (0-indexed)
static vector<vector<int>> DUR;    // op x machine, -1 if not eligible
static vector<int> minDur;         // op -> cheapest duration

static void parseInstance(const string& path) {
    ifstream f(path);
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); exit(1); }
    double avg; f >> nJobs >> nMach >> avg;
    for (int j = 0; j < nJobs; j++) {
        int no; f >> no;
        for (int o = 0; o < no; o++) {
            int k; f >> k;
            vector<int> em; vector<int> row(nMach, -1);
            for (int t = 0; t < k; t++) {
                int m, d; f >> m >> d; m--;
                if (row[m] < 0 || d < row[m]) row[m] = d;
                em.push_back(m);
            }
            sort(em.begin(), em.end()); em.erase(unique(em.begin(), em.end()), em.end());
            elig.push_back(em); DUR.push_back(row);
            opJob.push_back(j); opIdx.push_back(o);
        }
    }
    nOps = (int)opJob.size();
    jobPred.assign(nOps, -1); jobSucc.assign(nOps, -1);
    for (int o = 0; o < nOps; o++) {
        if (o > 0 && opJob[o - 1] == opJob[o]) jobPred[o] = o - 1;
        if (o + 1 < nOps && opJob[o + 1] == opJob[o]) jobSucc[o] = o + 1;
    }
    minDur.assign(nOps, 0);
    for (int o = 0; o < nOps; o++) { int mn = INT_MAX; for (int m : elig[o]) mn = min(mn, DUR[o][m]); minDur[o] = mn; }
    remWork.assign(nOps, 0);           // cheapest work remaining in the job from o on
    for (int o = nOps - 1; o >= 0; o--) remWork[o] = minDur[o] + (jobSucc[o] >= 0 ? remWork[jobSucc[o]] : 0);
}

// ---------------- shared incumbent + elite pool ----------------
static mutex bestMtx;
static atomic<int> bestCmax(INT_MAX);
static vector<int> bestMach, bestStart;
static string outPath;
static atomic<bool> stopFlag(false);

struct Elite { int cmax; vector<int> mach, start; };
static vector<Elite> elites;
static const size_t ELITE_MAX = 20;

static void writeSchedule(int cmax, const vector<int>& mach, const vector<int>& st) {
    string tmp = outPath + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    fprintf(f, "makespan %d\n", cmax);
    for (int o = 0; o < nOps; o++)
        fprintf(f, "%d %d %d %d %d\n", opJob[o], opIdx[o], mach[o] + 1, st[o], st[o] + DUR[o][mach[o]]);
    fclose(f);
    rename(tmp.c_str(), outPath.c_str());
}

// ---------------- one tabu-search worker ----------------
struct Worker {
    mt19937 rng;
    vector<int> mach, pos, head, tail, dur, topo, indeg, tmpq, curPos, curMach;
    vector<vector<int>> seq;
    vector<vector<int>> tabu;   // [op][machine] -> iteration until forbidden
    int Cmax = INT_MAX;
    int nearCrit = 0;
    long long score = LLONG_MAX;
    static const int SLACK_T = 4;
    long long iter = 0, totalIter = 0;
    int tid = 0;
    int lnsEvery = 0;      // 0 = pure tabu worker
    bool verbose = false;

    Worker(uint32_t seed) : rng(seed) {
        mach.assign(nOps, 0); pos.assign(nOps, 0);
        head.assign(nOps, 0); tail.assign(nOps, 0); dur.assign(nOps, 0);
        topo.assign(nOps, 0); indeg.assign(nOps, 0); tmpq.assign(nOps, 0);
        seq.assign(nMach, {});
        tabu.assign(nOps, vector<int>(nMach, 0));
    }

    inline int rnd(int n) { return (int)(rng() % (uint32_t)n); }

    // Longest-path evaluation; false when the machine orders create a cycle.
    bool evaluate() {
        for (int o = 0; o < nOps; o++) pos[o] = -1;
        for (int m = 0; m < nMach; m++)
            for (size_t i = 0; i < seq[m].size(); i++) pos[seq[m][i]] = (int)i;
        for (int o = 0; o < nOps; o++) {
            // an operation removed by the LNS ruin step is not on any machine;
            // it keeps only its job-precedence edges and its cheapest duration
            dur[o] = (mach[o] >= 0) ? DUR[o][mach[o]] : minDur[o];
            indeg[o] = (jobPred[o] >= 0 ? 1 : 0) + (pos[o] > 0 ? 1 : 0);
            head[o] = 0;
        }
        int qh = 0, qt = 0;
        for (int o = 0; o < nOps; o++) if (indeg[o] == 0) tmpq[qt++] = o;
        int cnt = 0;
        while (qh < qt) {
            int o = tmpq[qh++];
            topo[cnt++] = o;
            int fin = head[o] + dur[o];
            int js = jobSucc[o];
            if (js >= 0) { if (fin > head[js]) head[js] = fin; if (--indeg[js] == 0) tmpq[qt++] = js; }
            if (mach[o] >= 0) {
                const vector<int>& S = seq[mach[o]];
                if (pos[o] + 1 < (int)S.size()) {
                    int ms = S[pos[o] + 1];
                    if (fin > head[ms]) head[ms] = fin;
                    if (--indeg[ms] == 0) tmpq[qt++] = ms;
                }
            }
        }
        if (cnt != nOps) return false;               // cycle
        Cmax = 0;
        for (int o = 0; o < nOps; o++) Cmax = max(Cmax, head[o] + dur[o]);
        for (int i = nOps - 1; i >= 0; i--) {
            int o = topo[i]; int t = 0;
            int js = jobSucc[o]; if (js >= 0) t = max(t, tail[js] + dur[js]);
            if (mach[o] >= 0) {
                const vector<int>& S = seq[mach[o]];
                if (pos[o] + 1 < (int)S.size()) { int ms = S[pos[o] + 1]; t = max(t, tail[ms] + dur[ms]); }
            }
            tail[o] = t;
        }
        // Secondary objective: how many operations sit on (or very near) the
        // critical path.  The makespan landscape is extremely degenerate, so
        // preferring solutions with a thinner critical region gives the tabu
        // search a gradient to follow across plateaus.
        nearCrit = 0;
        for (int o = 0; o < nOps; o++) {
            int slack = Cmax - (head[o] + dur[o] + tail[o]);
            if (slack <= SLACK_T) nearCrit++;
        }
        score = (long long)Cmax * 100000LL + nearCrit;
        return true;
    }

    // ---- a load-balanced machine assignment (simulated annealing on sum of
    //      squared machine loads, which drives the max load down) ----
    void balancedAssign(vector<int>& asg, long long iters) {
        vector<long long> load(nMach, 0);
        vector<int> ord(nOps); iota(ord.begin(), ord.end(), 0); shuffle(ord.begin(), ord.end(), rng);
        for (int o : ord) {
            int bm = -1; long long bv = LLONG_MAX;
            for (int m : elig[o]) { long long v = load[m] + DUR[o][m]; if (v < bv) { bv = v; bm = m; } }
            asg[o] = bm; load[bm] += DUR[o][bm];
        }
        long long pot = 0; for (int m = 0; m < nMach; m++) pot += load[m] * load[m];
        vector<int> best = asg; long long bestMax = *max_element(load.begin(), load.end());
        double T0 = 3e4, T1 = 0.5;
        for (long long it = 0; it < iters; it++) {
            double T = T0 * pow(T1 / T0, (double)it / iters);
            int o = rnd(nOps); int cm = asg[o];
            int nm = elig[o][rnd((int)elig[o].size())];
            if (nm == cm) continue;
            long long dc = DUR[o][cm], dn = DUR[o][nm];
            long long np = pot - load[cm] * load[cm] - load[nm] * load[nm]
                         + (load[cm] - dc) * (load[cm] - dc) + (load[nm] + dn) * (load[nm] + dn);
            long long dl = np - pot;
            if (dl <= 0 || exp(-(double)dl / T) > (double)rnd(1000000) / 1e6) {
                load[cm] -= dc; load[nm] += dn; asg[o] = nm; pot = np;
                long long c = *max_element(load.begin(), load.end());
                if (c < bestMax) { bestMax = c; best = asg; }
            }
        }
        asg = best;
    }

    // ---- Giffler-Thompson active-schedule construction with fixed machines ----
    void scheduleFromAssign(const vector<int>& asg, int noise) {
        for (int m = 0; m < nMach; m++) seq[m].clear();
        mach = asg;
        vector<int> nextOp(nJobs, 0), jobReady(nJobs, 0), machFree(nMach, 0), jobFirst(nJobs, -1), jobLen(nJobs, 0);
        for (int o = nOps - 1; o >= 0; o--) jobFirst[opJob[o]] = o;
        for (int o = 0; o < nOps; o++) jobLen[opJob[o]]++;
        vector<int> active(nJobs); iota(active.begin(), active.end(), 0);
        int done = 0;
        while (done < nOps) {
            int Cstar = INT_MAX, mStar = -1;
            for (int j : active) {
                int o = jobFirst[j] + nextOp[j]; int m = asg[o];
                int c = max(jobReady[j], machFree[m]) + DUR[o][m];
                if (c < Cstar) { Cstar = c; mStar = m; }
            }
            // conflict set: ops on mStar that can start before Cstar
            int pick = -1, bestPri = INT_MIN;
            for (int j : active) {
                int o = jobFirst[j] + nextOp[j];
                if (asg[o] != mStar) continue;
                int st = max(jobReady[j], machFree[mStar]);
                if (st >= Cstar) continue;
                int pri = remWork[o] * 4 - st + (noise ? rnd(noise) : 0);   // most-work-remaining
                if (pri > bestPri) { bestPri = pri; pick = o; }
            }
            int j = opJob[pick];
            int st = max(jobReady[j], machFree[mStar]);
            seq[mStar].push_back(pick);
            machFree[mStar] = st + DUR[pick][mStar];
            jobReady[j] = machFree[mStar];
            nextOp[j]++; done++;
            if (nextOp[j] == jobLen[j]) active.erase(find(active.begin(), active.end(), j));
        }
        evaluate();
    }

    // ---- construction: randomized earliest-completion-time dispatching ----
    void constructECT(double greed) {
        for (int m = 0; m < nMach; m++) seq[m].clear();
        vector<int> nextOp(nJobs, 0), jobReady(nJobs, 0), machFree(nMach, 0), jobFirst(nJobs, -1), jobLen(nJobs, 0);
        for (int o = nOps - 1; o >= 0; o--) jobFirst[opJob[o]] = o;
        for (int o = 0; o < nOps; o++) jobLen[opJob[o]]++;
        vector<int> active(nJobs); iota(active.begin(), active.end(), 0);
        vector<pair<int, pair<int, int>>> cand; vector<int> pool;
        int done = 0;
        while (done < nOps) {
            cand.clear(); int bestC = INT_MAX;
            for (int j : active) {
                int o = jobFirst[j] + nextOp[j];
                for (int m : elig[o]) {
                    int c = max(jobReady[j], machFree[m]) + DUR[o][m];
                    cand.push_back({c, {j, m}}); bestC = min(bestC, c);
                }
            }
            int thr = bestC + (int)(greed * bestC / 100.0);
            pool.clear();
            for (size_t i = 0; i < cand.size(); i++) if (cand[i].first <= thr) pool.push_back((int)i);
            int pick = pool[rnd((int)pool.size())];
            int j = cand[pick].second.first, m = cand[pick].second.second;
            int o = jobFirst[j] + nextOp[j];
            int st = max(jobReady[j], machFree[m]);
            mach[o] = m; seq[m].push_back(o);
            machFree[m] = st + DUR[o][m]; jobReady[j] = machFree[m];
            nextOp[j]++; done++;
            if (nextOp[j] == jobLen[j]) active.erase(find(active.begin(), active.end(), j));
        }
        evaluate();
    }

    struct Move { int v, k, p; int est; };

    // a/ap must be v's position in the *current* solution; pos[] is clobbered
    // by evaluate() while probing neighbours, so the candidate loop passes the
    // snapshot taken at the top of the iteration.
    void doMove(int v, int a, int ap, int k, int p) {
        seq[a].erase(seq[a].begin() + ap);
        seq[k].insert(seq[k].begin() + p, v);
        mach[v] = k;
    }
    void undoMove(int v, int a, int ap, int k, int p) {
        seq[k].erase(seq[k].begin() + p);
        seq[a].insert(seq[a].begin() + ap, v);
        mach[v] = a;
    }

    vector<Move> cands;
    vector<int> critical;

    void collectCritical() {
        critical.clear();
        for (int o = 0; o < nOps; o++)
            if (head[o] + dur[o] + tail[o] == Cmax) critical.push_back(o);
    }

    // Candidate moves for the critical operations, ranked by an O(1) makespan
    // estimate built from the current heads and tails.
    void buildCandidates() {
        cands.clear();
        for (int v : critical) {
            int jp = jobPred[v], js = jobSucc[v];
            int rJ = jp >= 0 ? head[jp] + dur[jp] : 0;
            int qJ = js >= 0 ? tail[js] + dur[js] : 0;
            int a = mach[v], ap = pos[v];
            for (int k : elig[v]) {
                const vector<int>& S = seq[k];
                int len = (int)S.size() - (k == a ? 1 : 0);
                int dv = DUR[v][k];
                for (int p = 0; p <= len; p++) {
                    int prev = -1, next = -1;
                    if (k == a) {
                        if (p == ap) continue;                       // no-op
                        int ip = p < ap ? p - 1 : p;
                        int in_ = p < ap ? p : p + 1;
                        if (ip >= 0) prev = S[ip];
                        if (in_ < (int)S.size()) next = S[in_];
                    } else {
                        if (p - 1 >= 0) prev = S[p - 1];
                        if (p < (int)S.size()) next = S[p];
                    }
                    int st = rJ;
                    if (prev >= 0) st = max(st, head[prev] + dur[prev]);
                    int q = qJ;
                    if (next >= 0) q = max(q, tail[next] + dur[next]);
                    cands.push_back({v, k, p, st + dv + q});
                }
            }
        }
    }



    // ---------------------------------------------------------------------
    //  Exact re-optimisation of the two end stages.
    //
    //  Every source of the disjunctive graph is a stage-A operation and every
    //  sink is a stage-C operation, so with the rest of the schedule fixed:
    //      makespan = max over stage-A ops of (C_i + q_i)      (q = job tail)
    //      makespan = max over stage-C machines of completion  (r = job head)
    //  On one machine, non-increasing tail order is optimal for the first and
    //  ERD order is optimal for the second, so each reduces to partitioning
    //  100 operations over 4 machines.  Re-solving stage C after an LNS move
    //  means the move is judged against the *best* response of the bottleneck
    //  stage rather than against a stale sequence.
    // ---------------------------------------------------------------------
    vector<int> stOps[2], stKey, stAsg, stBest;
    vector<vector<int>> stBucket;

    // stage 0 -> machines 0..3 (op 0, tails);  stage 1 -> machines 16..19 (op 4, releases)
    bool optEndStage(int stage, int descent) {
        const int off = stage == 0 ? 0 : 16, M = 4;
        const bool releaseMode = (stage == 1);
        vector<int>& ops = stOps[stage];
        if (ops.empty()) {
            int want = stage == 0 ? 0 : 4;
            for (int o = 0; o < nOps; o++) if (opIdx[o] == want) ops.push_back(o);
        }
        int n = (int)ops.size();
        stKey.assign(n, 0);
        for (int i = 0; i < n; i++) {
            int o = ops[i];
            if (releaseMode) { int jp = jobPred[o]; stKey[i] = jp >= 0 ? head[jp] + dur[jp] : 0; }
            else             { int js = jobSucc[o]; stKey[i] = js >= 0 ? tail[js] + dur[js] : 0; }
        }
        vector<int> ord(n); iota(ord.begin(), ord.end(), 0);
        if (releaseMode) sort(ord.begin(), ord.end(), [&](int a, int b) { return stKey[a] < stKey[b]; });
        else             sort(ord.begin(), ord.end(), [&](int a, int b) { return stKey[a] > stKey[b]; });

        stBucket.assign(M, {});
        stAsg.assign(n, 0);
        // greedy: walk the optimal order, put each op where it hurts least
        {
            vector<int> t(M, 0), v(M, 0);
            for (int i : ord) {
                int o = ops[i]; int bm = -1, bv = INT_MAX, bt = 0, bval = 0;
                for (int m : elig[o]) {
                    int mm = m - off; if (mm < 0 || mm >= M) continue;
                    int nt, nv;
                    if (releaseMode) { nt = max(t[mm], stKey[i]) + DUR[o][m]; nv = nt; }
                    else             { nt = t[mm] + DUR[o][m]; nv = max(v[mm], nt + stKey[i]); }
                    if (nv < bv || (nv == bv && nt < bt)) { bv = nv; bm = mm; bt = nt; bval = nv; }
                }
                if (bm < 0) return false;
                stAsg[i] = bm; t[bm] = bt; v[bm] = bval;
                stBucket[bm].push_back(i);
            }
        }
        auto machVal = [&](int m) {
            int t = 0, v = 0;
            for (int i : stBucket[m]) {
                int o = ops[i]; int d = DUR[o][off + m];
                if (releaseMode) { t = max(t, stKey[i]) + d; v = t; }
                else             { t += d; v = max(v, t + stKey[i]); }
            }
            return v;
        };
        vector<int> mval(M);
        for (int m = 0; m < M; m++) mval[m] = machVal(m);
        auto obj = [&]() { return *max_element(mval.begin(), mval.end()); };
        int cur = obj(); stBest = stAsg; int best = cur;
        // first-improvement descent with occasional sideways moves
        for (int it = 0; it < descent; it++) {
            int i = rnd(n); int o = ops[i];
            const vector<int>& E = elig[o];
            int nm = E[rnd((int)E.size())] - off;
            if (nm < 0 || nm >= M || nm == stAsg[i]) continue;
            int om = stAsg[i];
            auto& B = stBucket[om];
            B.erase(find(B.begin(), B.end(), i));
            // keep buckets in the optimal order: insert by key
            auto& C = stBucket[nm];
            size_t p = 0;
            while (p < C.size() && (releaseMode ? stKey[C[p]] <= stKey[i] : stKey[C[p]] >= stKey[i])) p++;
            C.insert(C.begin() + p, i);
            stAsg[i] = nm;
            int a = machVal(om), b = machVal(nm), sa = mval[om], sb = mval[nm];
            mval[om] = a; mval[nm] = b;
            int v = obj();
            if (v <= cur) {
                cur = v;
                if (v < best) { best = v; stBest = stAsg; }
            } else {
                mval[om] = sa; mval[nm] = sb;
                C.erase(find(C.begin(), C.end(), i));
                auto& B2 = stBucket[om];
                size_t q = 0;
                while (q < B2.size() && (releaseMode ? stKey[B2[q]] <= stKey[i] : stKey[B2[q]] >= stKey[i])) q++;
                B2.insert(B2.begin() + q, i);
                stAsg[i] = om;
            }
        }
        // install the best partition, in the provably optimal within-machine order
        vector<int> saveMach = mach; vector<vector<int>> saveSeq = seq;
        long long saveScore = score;
        vector<vector<int>> nb(M);
        for (int i = 0; i < n; i++) nb[stBest[i]].push_back(i);
        for (int m = 0; m < M; m++) {
            if (releaseMode) sort(nb[m].begin(), nb[m].end(), [&](int a, int b) { return stKey[a] < stKey[b]; });
            else             sort(nb[m].begin(), nb[m].end(), [&](int a, int b) { return stKey[a] > stKey[b]; });
            seq[off + m].clear();
            for (int i : nb[m]) { seq[off + m].push_back(ops[i]); mach[ops[i]] = off + m; }
        }
        if (!evaluate() || score > saveScore) {
            mach = saveMach; seq = saveSeq; evaluate();
            return false;
        }
        return true;
    }

    bool polishEnds(int descent) {
        bool any = false;
        if (optEndStage(1, descent)) any = true;
        if (optEndStage(0, descent)) any = true;
        if (optEndStage(1, descent)) any = true;
        return any;
    }

    // ---------------------------------------------------------------------
    //  Ruin & recreate (large neighbourhood search)
    //
    //  Single reinsertion moves cannot rebalance a stage whose machines all
    //  finish at exactly the makespan: any one move that shifts work onto a
    //  sibling machine immediately makes things worse.  Tearing out a set of
    //  operations and greedily rebuilding lets the search perform that kind of
    //  compound rebalancing in one step.
    // ---------------------------------------------------------------------
    vector<int> ruined;

    void removeOp(int v) {
        int a = mach[v];
        auto& S = seq[a];
        S.erase(find(S.begin(), S.end(), v));
        mach[v] = -1;
    }

    // Choose the operations to tear out.  Three flavours: purely random,
    // "same machine + nearby in time" (fixes a congested machine), and
    // "whole jobs" (lets a job be re-routed end to end).
    void ruin(int k) {
        ruined.clear();
        int mode = rnd(3);
        if (mode == 0) {
            vector<int> perm(nOps); iota(perm.begin(), perm.end(), 0);
            for (int i = 0; i < k && i < nOps; i++) { swap(perm[i], perm[i + rnd(nOps - i)]); ruined.push_back(perm[i]); }
        } else if (mode == 1) {
            // pick a machine biased towards the busiest, take a time window
            int m = -1; int bestLoad = -1;
            for (int t = 0; t < 3; t++) {
                int c = rnd(nMach); int load = 0;
                for (int o : seq[c]) load += DUR[o][c];
                if (load > bestLoad) { bestLoad = load; m = c; }
            }
            if (seq[m].empty()) { ruin(k); return; }
            int start = rnd((int)seq[m].size());
            for (int i = start; i < (int)seq[m].size() && (int)ruined.size() < k; i++) ruined.push_back(seq[m][i]);
            for (int i = start - 1; i >= 0 && (int)ruined.size() < k; i--) ruined.push_back(seq[m][i]);
        } else {
            vector<int> jobFirst(nJobs, -1);
            for (int o = nOps - 1; o >= 0; o--) jobFirst[opJob[o]] = o;
            while ((int)ruined.size() < k) {
                int j = rnd(nJobs);
                for (int o = jobFirst[j]; o < nOps && opJob[o] == j; o++)
                    if (find(ruined.begin(), ruined.end(), o) == ruined.end()) ruined.push_back(o);
            }
        }
        for (int v : ruined) removeOp(v);
    }

    // Greedy best-position reinsertion, guided by the O(1) estimate and
    // confirmed by exact longest-path evaluation of the few best candidates.
    bool recreate(int probe) {
        shuffle(ruined.begin(), ruined.end(), rng);
        for (int v : ruined) {
            if (!evaluate()) return false;
            int jp = jobPred[v], js = jobSucc[v];
            int rJ = jp >= 0 ? head[jp] + dur[jp] : 0;
            int qJ = js >= 0 ? tail[js] + dur[js] : 0;
            cands.clear();
            for (int k : elig[v]) {
                const vector<int>& S = seq[k];
                int dv = DUR[v][k];
                for (int p = 0; p <= (int)S.size(); p++) {
                    int st = rJ, q = qJ;
                    if (p > 0) st = max(st, head[S[p - 1]] + dur[S[p - 1]]);
                    if (p < (int)S.size()) q = max(q, tail[S[p]] + dur[S[p]]);
                    cands.push_back({v, k, p, st + dv + q});
                }
            }
            int kk = min((int)cands.size(), probe);
            partial_sort(cands.begin(), cands.begin() + kk, cands.end(),
                         [](const Move& x, const Move& y) { return x.est < y.est; });
            long long bestVal = LLONG_MAX; int bk = -1, bp = -1;
            for (int i = 0; i < kk; i++) {
                Move& mv = cands[i];
                seq[mv.k].insert(seq[mv.k].begin() + mv.p, v); mach[v] = mv.k;
                bool ok = evaluate();
                long long val = ok ? score : LLONG_MAX;
                seq[mv.k].erase(seq[mv.k].begin() + mv.p); mach[v] = -1;
                if (ok && val < bestVal) { bestVal = val; bk = mv.k; bp = mv.p; }
            }
            if (bk < 0) {            // every probed slot cycles: fall back to a scan
                for (int k : elig[v]) {
                    for (int p = 0; p <= (int)seq[k].size(); p++) {
                        seq[k].insert(seq[k].begin() + p, v); mach[v] = k;
                        bool ok = evaluate();
                        long long val = ok ? score : LLONG_MAX;
                        seq[k].erase(seq[k].begin() + p); mach[v] = -1;
                        if (ok && val < bestVal) { bestVal = val; bk = k; bp = p; }
                    }
                    if (bk >= 0) break;
                }
            }
            if (bk < 0) return false;
            seq[bk].insert(seq[bk].begin() + bp, v); mach[v] = bk;
        }
        ruined.clear();
        return evaluate();
    }

    // One LNS pass on the current solution; restores it when the result is worse.
    bool lnsPass(int k, int probe, long long tol = 0) {
        vector<int> saveMach = mach; vector<vector<int>> saveSeq = seq;
        long long saveScore = score; int saveC = Cmax;
        ruin(k);
        if (!recreate(probe)) { mach = saveMach; seq = saveSeq; evaluate(); return false; }
        polishEnds(300);
        if (score > saveScore + tol) {
            mach = saveMach; seq = saveSeq; evaluate();
            return false;
        }
        return Cmax < saveC || score < saveScore;
    }

    // A burst of LNS passes used in place of the random perturbation: it
    // escapes the basin while still producing sensible schedules.
    void lnsBurst(int n, long long tol) {
        for (int i = 0; i < n; i++) lnsPass(6 + rnd(45), 6, tol);
    }

    void perturb(int strength) {
        for (int t = 0; t < strength; t++) {
            int v = rnd(nOps);
            int k = elig[v][rnd((int)elig[v].size())];
            int a = mach[v], ap = pos[v];
            int len = (int)seq[k].size() - (k == a ? 1 : 0);
            int p = rnd(len + 1);
            if (k == a && p == ap) continue;
            doMove(v, a, ap, k, p);
            if (!evaluate()) { undoMove(v, a, ap, k, p); evaluate(); }
        }
    }

    void reportIfBest() {
        if (Cmax < bestCmax.load()) {
            lock_guard<mutex> lk(bestMtx);
            if (Cmax < bestCmax.load()) {
                bestCmax.store(Cmax);
                bestMach = mach; bestStart = head;
                writeSchedule(Cmax, mach, head);
                time_t tt = time(nullptr); char buf[32]; strftime(buf, 32, "%H:%M:%S", localtime(&tt));
                fprintf(stderr, "[%s] new best makespan %d (worker %d)\n", buf, Cmax, tid);
            }
        }
    }

    void pushElite() {
        lock_guard<mutex> lk(bestMtx);
        for (auto& e : elites) if (e.cmax == Cmax && e.mach == mach) return;
        elites.push_back({Cmax, mach, head});
        sort(elites.begin(), elites.end(), [](const Elite& a, const Elite& b) { return a.cmax < b.cmax; });
        if (elites.size() > ELITE_MAX) elites.resize(ELITE_MAX);
    }

    // Rebuild machine sequences from a solution given as (machine, start time).
    void loadFrom(const vector<int>& mm, const vector<int>& ss) {
        mach = mm;
        vector<vector<pair<int, int>>> tmp(nMach);
        for (int o = 0; o < nOps; o++) tmp[mach[o]].push_back({ss[o], o});
        for (int m = 0; m < nMach; m++) {
            sort(tmp[m].begin(), tmp[m].end());
            seq[m].clear();
            for (auto& pr : tmp[m]) seq[m].push_back(pr.second);
        }
        evaluate();
    }

    bool loadRandomElite() {
        vector<int> mm, ss;
        {
            lock_guard<mutex> lk(bestMtx);
            if (elites.empty()) return false;
            const Elite& e = elites[rnd((int)elites.size())];
            mm = e.mach; ss = e.start;
        }
        loadFrom(mm, ss);
        return true;
    }

    void freshStart() {
        int r = rnd(10);
        if (r < 6) {                            // load-balanced assignment + GT
            vector<int> asg(nOps);
            balancedAssign(asg, 1500000);
            scheduleFromAssign(asg, r < 3 ? 0 : 40);
        } else {
            constructECT(rnd(3) == 0 ? 0.0 : (double)rnd(12));
        }
    }


    // ---------------------------------------------------------------------
    //  Iterated greedy with Metropolis acceptance.
    //
    //  Pure "accept only non-worsening" LNS locks onto a basin very hard (408
    //  was reproducible from many starts and never escaped).  Letting the ruin
    //  step occasionally keep a schedule that is one unit worse, on a slow
    //  cooling schedule with reheats, is what actually explores.
    //  score = Cmax * 100000 + nearCrit, so one unit of makespan costs 1e5.
    // ---------------------------------------------------------------------
    void runSaLns(double seconds) {
        auto t0 = chrono::steady_clock::now();
        freshStart();
        polishEnds(3000);
        reportIfBest(); pushElite();
        vector<int> curMachS = mach; vector<vector<int>> curSeqS = seq;
        long long curScore = score;
        vector<int> bMach = mach; vector<vector<int>> bSeq = seq;
        long long bScore = score; int bC = Cmax;
        double T0 = 160000.0, T1 = 8000.0;
        long long cycle = 0, sinceImp = 0;
        while (!stopFlag.load()) {
            double el = chrono::duration<double>(chrono::steady_clock::now() - t0).count();
            if (el > seconds) break;
            cycle++;
            double frac = (double)(cycle % 4000) / 4000.0;      // sawtooth with reheats
            double T = T0 * pow(T1 / T0, frac);
            vector<int> sMach = mach; vector<vector<int>> sSeq = seq;
            long long sScore = score;
            ruin(5 + rnd(55));
            if (!recreate(6)) { mach = sMach; seq = sSeq; evaluate(); continue; }
            polishEnds(400);
            long long d = score - curScore;
            bool accept = (d <= 0) || (exp(-(double)d / T) > (double)rnd(1000000) / 1e6);
            if (!accept) { mach = sMach; seq = sSeq; evaluate(); }
            else {
                curScore = score;
                if (score < bScore) {
                    bScore = score; bC = Cmax; bMach = mach; bSeq = seq; sinceImp = 0;
                    polishEnds(4000);
                    if (score < bScore) { bScore = score; bC = Cmax; bMach = mach; bSeq = seq; }
                    reportIfBest(); pushElite();
                }
            }
            if (++sinceImp > 6000) {                 // stuck: jump to another elite
                sinceImp = 0;
                if (rnd(3) == 0) { freshStart(); polishEnds(2000); }
                else if (!loadRandomElite()) { mach = bMach; seq = bSeq; evaluate(); }
                curScore = score;
            }
        }
        (void)bC;
    }

    void run(double seconds, int K) {
        auto t0 = chrono::steady_clock::now();
        freshStart();
        int localBest = Cmax;
        vector<int> lbMach = mach; vector<vector<int>> lbSeq = seq;
        reportIfBest(); pushElite();
        long long sinceImp = 0;
        int tenureBase = 8 + rnd(12);
        long long sinceRestart = 0;
        while (!stopFlag.load()) {
            if (chrono::duration<double>(chrono::steady_clock::now() - t0).count() > seconds) break;
            iter++; totalIter++; sinceRestart++;
            if (lnsEvery && (totalIter % lnsEvery == 0)) {
                if (lnsPass(8 + rnd(40), 6) && Cmax < localBest) {
                    localBest = Cmax; lbMach = mach; lbSeq = seq; sinceImp = 0;
                    reportIfBest(); pushElite();
                }
            }
            collectCritical();
            buildCandidates();
            if (cands.empty()) { perturb(10); continue; }
            int kk = min((int)cands.size(), K);
            partial_sort(cands.begin(), cands.begin() + kk, cands.end(),
                         [](const Move& x, const Move& y) { return x.est < y.est; });
            long long bestVal = LLONG_MAX, bestTabuVal = LLONG_MAX;
            int bv = -1, bk = -1, bp = -1, ba = -1, bap = -1;
            int tv = -1, tk = -1, tp = -1, ta = -1, tap = -1;
            curPos = pos; curMach = mach;
            for (int i = 0; i < kk; i++) {
                Move& mv = cands[i];
                int a = curMach[mv.v], ap = curPos[mv.v];
                doMove(mv.v, a, ap, mv.k, mv.p);
                bool ok = evaluate();
                long long val = ok ? score : LLONG_MAX;
                int valC = ok ? Cmax : INT_MAX;
                undoMove(mv.v, a, ap, mv.k, mv.p);
                if (!ok) continue;
                bool isTabu = tabu[mv.v][a] > (int)iter;
                if (isTabu && valC >= bestCmax.load()) {
                    if (val < bestTabuVal) { bestTabuVal = val; tv = mv.v; tk = mv.k; tp = mv.p; ta = a; tap = ap; }
                    continue;
                }
                if (val < bestVal) { bestVal = val; bv = mv.v; bk = mv.k; bp = mv.p; ba = a; bap = ap; }
            }
            if (bv < 0) {
                if (tv < 0) { evaluate(); perturb(8); continue; }
                bv = tv; bk = tk; bp = tp; ba = ta; bap = tap;
            }
            doMove(bv, ba, bap, bk, bp);
            if (!evaluate()) { undoMove(bv, ba, bap, bk, bp); evaluate(); perturb(5); continue; }
            tabu[bv][ba] = (int)iter + tenureBase + rnd(8);
            if (Cmax < localBest) {
                polishEnds(1500);
                localBest = Cmax; lbMach = mach; lbSeq = seq; sinceImp = 0;
                reportIfBest();
                if (Cmax <= bestCmax.load() + 6) pushElite();
            } else if (++sinceImp > 3000) {
                sinceImp = 0;
                tenureBase = 8 + rnd(14);
                int r = rnd(10);
                if (sinceRestart > 120000 && r < 2) {      // occasional full restart
                    sinceRestart = 0; freshStart();
                    localBest = Cmax; lbMach = mach; lbSeq = seq;
                } else if (r < 5) {
                    loadRandomElite();
                    lnsBurst(3 + rnd(6), 400000);
                } else {
                    mach = lbMach; seq = lbSeq; evaluate();
                    if (rnd(2)) lnsBurst(2 + rnd(5), 300000); else perturb(15 + rnd(45));
                }
                for (auto& row : tabu) fill(row.begin(), row.end(), 0);
                iter = 0;
                if (Cmax < localBest) { localBest = Cmax; lbMach = mach; lbSeq = seq; }
                reportIfBest();
            }
        }
    }
};

int main(int argc, char** argv) {
    string inst = "/app/instances/behnke16.fjs";
    outPath = "/app/solution/schedule.txt";
    double seconds = 300; int threads = 8; int K = 24; uint32_t seed0 = 1;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-i") inst = argv[++i];
        else if (a == "-o") outPath = argv[++i];
        else if (a == "-t") threads = atoi(argv[++i]);
        else if (a == "-s") seconds = atof(argv[++i]);
        else if (a == "-k") K = atoi(argv[++i]);
        else if (a == "-r") seed0 = (uint32_t)atoi(argv[++i]);
    }
    parseInstance(inst);

    // Warm start: adopt an existing schedule file as the incumbent if valid.
    {
        ifstream f(outPath);
        string w; int c;
        if (f >> w >> c && w == "makespan") {
            vector<vector<int>> byJob(nJobs);
            for (int o = 0; o < nOps; o++) byJob[opJob[o]].push_back(o);
            vector<int> mm(nOps, -1), ss(nOps, -1);
            int j, o, m, s, e; bool ok = true; int cnt = 0;
            while (f >> j >> o >> m >> s >> e) {
                if (j < 0 || j >= nJobs || o < 0 || o >= (int)byJob[j].size()) { ok = false; break; }
                int id = byJob[j][o];
                mm[id] = m - 1; ss[id] = s; cnt++;
            }
            if (ok && cnt == nOps) {
                bestCmax.store(c); bestMach = mm; bestStart = ss;
                elites.push_back({c, mm, ss});
                fprintf(stderr, "[warm] loaded incumbent %d\n", c);
            }
        }
    }

    vector<thread> th;
    for (int t = 0; t < threads; t++)
        th.emplace_back([&, t]() {
            Worker w(seed0 * 7919u + (uint32_t)t * 104729u + 17u);
            w.tid = t; w.verbose = (t == 0);
            w.lnsEvery = (t % 3 == 2) ? 60 : (t % 3 == 1 ? 400 : 0);
            if (t % 3 == 2) w.runSaLns(seconds); else w.run(seconds, K);
        });
    for (auto& x : th) x.join();
    fprintf(stderr, "final best %d\n", bestCmax.load());
    return 0;
}
