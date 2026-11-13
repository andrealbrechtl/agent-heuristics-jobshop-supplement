// Coordinate-descent polisher for behnke16 schedules.
//
// behnke16 is a 3-stage flexible flow shop.  Two of the three stages decompose
// cleanly once the rest of the schedule is held fixed:
//
//   stage C (machines 17-20, op 4)  -- no job successors, so every operation
//       has a fixed release date r_i = completion of op 3.  Tails are zero.
//       On one machine, ERD order minimises the makespan, so the subproblem is
//       "partition 100 jobs over 4 unrelated machines".
//
//   stage A (machines 1-4, op 0)    -- no job predecessors, so every operation
//       has a fixed tail q_i = tail(op 1) + dur(op 1) taken from the current
//       disjunctive graph.  On one machine, non-increasing tail order minimises
//       max(C_i + q_i), so this is again a partitioning problem.
//
// Each partitioning problem is attacked with simulated annealing plus descent.
// Re-optimising one stage changes the heads/tails feeding the other, so the two
// are alternated until neither improves.
#include <bits/stdc++.h>
using namespace std;

static int nJobs, nMach, nOps;
static vector<int> opJob, opIdx, jobPred, jobSucc;
static vector<vector<int>> DUR, elig;

static void parseInstance(const string& path) {
    ifstream f(path);
    if (!f) { fprintf(stderr, "cannot open %s\n", path.c_str()); exit(1); }
    double avg; f >> nJobs >> nMach >> avg;
    for (int j = 0; j < nJobs; j++) {
        int no; f >> no;
        for (int o = 0; o < no; o++) {
            int k; f >> k; vector<int> row(nMach, -1), em;
            for (int t = 0; t < k; t++) { int m, d; f >> m >> d; m--; if (row[m] < 0 || d < row[m]) row[m] = d; em.push_back(m); }
            sort(em.begin(), em.end()); em.erase(unique(em.begin(), em.end()), em.end());
            DUR.push_back(row); elig.push_back(em);
            opJob.push_back(j); opIdx.push_back(o);
        }
    }
    nOps = (int)opJob.size();
    jobPred.assign(nOps, -1); jobSucc.assign(nOps, -1);
    for (int o = 0; o < nOps; o++) {
        if (o > 0 && opJob[o - 1] == opJob[o]) jobPred[o] = o - 1;
        if (o + 1 < nOps && opJob[o + 1] == opJob[o]) jobSucc[o] = o + 1;
    }
}

struct Graph {
    vector<int> mach, pos, head, tail, dur, topo, indeg, q;
    vector<vector<int>> seq;
    int Cmax = 0;
    Graph() { mach.assign(nOps, 0); pos.assign(nOps, 0); head.assign(nOps, 0); tail.assign(nOps, 0);
              dur.assign(nOps, 0); topo.assign(nOps, 0); indeg.assign(nOps, 0); q.assign(nOps, 0);
              seq.assign(nMach, {}); }
    bool evaluate() {
        for (int m = 0; m < nMach; m++) for (size_t i = 0; i < seq[m].size(); i++) pos[seq[m][i]] = (int)i;
        for (int o = 0; o < nOps; o++) {
            dur[o] = DUR[o][mach[o]];
            indeg[o] = (jobPred[o] >= 0) + (pos[o] > 0);
            head[o] = 0;
        }
        vector<int> qu; qu.reserve(nOps);
        for (int o = 0; o < nOps; o++) if (!indeg[o]) qu.push_back(o);
        int qh = 0, cnt = 0;
        while (qh < (int)qu.size()) {
            int o = qu[qh++]; topo[cnt++] = o; int fin = head[o] + dur[o];
            int js = jobSucc[o];
            if (js >= 0) { head[js] = max(head[js], fin); if (!--indeg[js]) qu.push_back(js); }
            auto& S = seq[mach[o]];
            if (pos[o] + 1 < (int)S.size()) { int ms = S[pos[o] + 1]; head[ms] = max(head[ms], fin); if (!--indeg[ms]) qu.push_back(ms); }
        }
        if (cnt != nOps) return false;
        Cmax = 0; for (int o = 0; o < nOps; o++) Cmax = max(Cmax, head[o] + dur[o]);
        for (int i = nOps - 1; i >= 0; i--) {
            int o = topo[i]; int t = 0;
            int js = jobSucc[o]; if (js >= 0) t = max(t, tail[js] + dur[js]);
            auto& S = seq[mach[o]];
            if (pos[o] + 1 < (int)S.size()) { int ms = S[pos[o] + 1]; t = max(t, tail[ms] + dur[ms]); }
            tail[o] = t;
        }
        return true;
    }
};

// ---------------------------------------------------------------------------
// Partition subproblem shared by both end stages.
//   items i = 0..n-1, machines 0..M-1 (offset by `off` in the real instance)
//   key[i]  : release date (stage C) or tail (stage A)
//   value of a machine = simulate its sorted item list
// ---------------------------------------------------------------------------
struct Part {
    int n, M, off;
    vector<int> item;            // global op ids
    vector<int> key;             // release (C) or tail (A)
    vector<vector<int>> d;       // d[i][m]
    bool releaseMode;            // true: release dates + ERD; false: tails + LTT
    vector<int> asg;
    vector<vector<int>> bucket;
    vector<int> mval;

    void sortBucket(int m) {
        auto& B = bucket[m];
        if (releaseMode) sort(B.begin(), B.end(), [&](int a, int b) { return key[a] < key[b]; });
        else             sort(B.begin(), B.end(), [&](int a, int b) { return key[a] > key[b]; });
    }
    int evalMach(int m) {
        auto& B = bucket[m];
        int t = 0, v = 0;
        if (releaseMode) { for (int i : B) { t = max(t, key[i]) + d[i][m]; } v = t; }
        else { for (int i : B) { t += d[i][m]; v = max(v, t + key[i]); } }
        return v;
    }
    void rebuild() {
        bucket.assign(M, {});
        for (int i = 0; i < n; i++) bucket[asg[i]].push_back(i);
        mval.assign(M, 0);
        for (int m = 0; m < M; m++) { sortBucket(m); mval[m] = evalMach(m); }
    }
    int value() { return *max_element(mval.begin(), mval.end()); }

    // move item i to machine nm, keeping buckets sorted; returns new objective
    int tryMove(int i, int nm, int& om) {
        om = asg[i];
        auto& Bo = bucket[om];
        Bo.erase(find(Bo.begin(), Bo.end(), i));
        bucket[nm].push_back(i); asg[i] = nm;
        sortBucket(nm);
        int a = evalMach(om), b = evalMach(nm);
        int sv1 = mval[om], sv2 = mval[nm];
        mval[om] = a; mval[nm] = b;
        int v = value();
        mval[om] = sv1; mval[nm] = sv2;
        return v;
    }
    void commit(int i, int om, int nm) {
        mval[om] = evalMach(om); mval[nm] = evalMach(nm);
    }
    void revert(int i, int om, int nm) {
        auto& Bn = bucket[nm];
        Bn.erase(find(Bn.begin(), Bn.end(), i));
        bucket[om].push_back(i); asg[i] = om; sortBucket(om);
    }

    int solve(mt19937& rng, long long iters, double T0, double T1) {
        rebuild();
        vector<int> best = asg; int bestV = value(); int curV = bestV;
        for (long long it = 0; it < iters; it++) {
            double T = T0 * pow(T1 / T0, (double)it / iters);
            int i = rng() % n;
            // only machines the operation is actually eligible on
            int gm = item[i];
            const vector<int>& E = elig[gm];
            int nm = E[rng() % E.size()] - off;
            if (nm == asg[i] || nm < 0 || nm >= M) continue;
            int om;
            int v = tryMove(i, nm, om);
            int dl = v - curV;
            if (dl <= 0 || exp(-dl / T) > (double)(rng() % 1000000) / 1e6) {
                commit(i, om, nm); curV = v;
                if (v < bestV) { bestV = v; best = asg; }
            } else revert(i, om, nm);
        }
        asg = best; rebuild();
        return bestV;
    }
};

static void buildStage(Graph& G, int stage, Part& P) {
    // stage 0 -> machines 0..3, op 0 (tails);  stage 2 -> machines 16..19, op 4 (releases)
    P.off = (stage == 0 ? 0 : 16); P.M = 4;
    P.item.clear(); P.key.clear(); P.d.clear();
    int want = (stage == 0 ? 0 : 4);
    for (int o = 0; o < nOps; o++) if (opIdx[o] == want) P.item.push_back(o);
    P.n = (int)P.item.size();
    P.releaseMode = (stage != 0);
    P.asg.assign(P.n, 0);
    for (int i = 0; i < P.n; i++) {
        int o = P.item[i];
        vector<int> dd(P.M, 1 << 28);
        for (int m : elig[o]) dd[m - P.off] = DUR[o][m];
        P.d.push_back(dd);
        if (stage == 0) {
            int js = jobSucc[o];
            P.key.push_back(js >= 0 ? G.tail[js] + G.dur[js] : 0);   // fixed job tail
        } else {
            int jp = jobPred[o];
            P.key.push_back(jp >= 0 ? G.head[jp] + G.dur[jp] : 0);   // fixed release date
        }
        P.asg[i] = G.mach[o] - P.off;
    }
}

static void applyStage(Graph& G, int stage, Part& P) {
    for (int m = 0; m < P.M; m++) G.seq[P.off + m].clear();
    for (int m = 0; m < P.M; m++) {
        P.sortBucket(m);
        for (int i : P.bucket[m]) {
            int o = P.item[i];
            G.mach[o] = P.off + m;
            G.seq[P.off + m].push_back(o);
        }
    }
}

int main(int argc, char** argv) {
    string inst = "/app/instances/behnke16.fjs", sch = "/app/solution/schedule.txt", out = "";
    long long iters = 400000; int rounds = 60; uint32_t seed = 1;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-i") inst = argv[++i]; else if (a == "-f") sch = argv[++i];
        else if (a == "-o") out = argv[++i]; else if (a == "-n") iters = atoll(argv[++i]);
        else if (a == "-R") rounds = atoi(argv[++i]); else if (a == "-r") seed = atoi(argv[++i]);
    }
    if (out.empty()) out = sch;
    parseInstance(inst);
    ifstream f(sch); string w; int c;
    if (!(f >> w >> c) || w != "makespan") { fprintf(stderr, "bad schedule file\n"); return 1; }
    vector<array<int, 5>> rows; int j, o, m, s, e;
    while (f >> j >> o >> m >> s >> e) rows.push_back({j, o, m, s, e});
    if ((int)rows.size() != nOps) { fprintf(stderr, "bad row count\n"); return 1; }

    Graph G;
    {
        vector<vector<int>> byJob(nJobs);
        for (int i = 0; i < nOps; i++) byJob[opJob[i]].push_back(i);
        vector<vector<pair<int, int>>> tmp(nMach);
        for (auto& r : rows) { int id = byJob[r[0]][r[1]]; G.mach[id] = r[2] - 1; tmp[r[2] - 1].push_back({r[3], id}); }
        for (int mm = 0; mm < nMach; mm++) { sort(tmp[mm].begin(), tmp[mm].end()); for (auto& pr : tmp[mm]) G.seq[mm].push_back(pr.second); }
    }
    if (!G.evaluate()) { fprintf(stderr, "input schedule has a cycle\n"); return 1; }
    fprintf(stderr, "loaded makespan %d\n", G.Cmax);
    int start = G.Cmax;

    mt19937 rng(seed * 2654435761u + 7u);
    Graph bestG = G; int bestC = G.Cmax;
    for (int r = 0; r < rounds; r++) {
        bool improvedAny = false;
        for (int stage : {2, 0}) {
            Part P; buildStage(G, stage, P);
            Graph save = G;
            P.solve(rng, iters, stage == 0 ? 20.0 : 20.0, 0.05);
            applyStage(G, stage, P);
            if (!G.evaluate() || G.Cmax > save.Cmax) { G = save; G.evaluate(); }
            else if (G.Cmax < save.Cmax) improvedAny = true;
        }
        if (G.Cmax < bestC) { bestC = G.Cmax; bestG = G; }
        if (!improvedAny && r > 3) break;
    }
    G = bestG; G.evaluate();
    fprintf(stderr, "polished %d -> %d\n", start, G.Cmax);
    if (G.Cmax < start || out != sch) {
        string tmp = out + ".tmp";
        FILE* g = fopen(tmp.c_str(), "w");
        fprintf(g, "makespan %d\n", G.Cmax);
        for (int i = 0; i < nOps; i++)
            fprintf(g, "%d %d %d %d %d\n", opJob[i], opIdx[i], G.mach[i] + 1, G.head[i], G.head[i] + G.dur[i]);
        fclose(g); rename(tmp.c_str(), out.c_str());
    }
    printf("%d\n", G.Cmax);
    return 0;
}
