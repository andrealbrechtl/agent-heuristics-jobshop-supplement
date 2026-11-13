// Exact-ish re-optimisation of the last stage of a behnke16 schedule.
//
// The 5th operation of every job runs on machines 17-20 and has no successor,
// so once stages A and B are fixed each of these operations has a fixed release
// date r_i (the completion of its job's 4th operation).  What is left is
// "partition 100 jobs over 4 unrelated machines, minimise the maximum
// completion", and on a single machine with release dates ERD order is optimal
// for the makespan.  So the subproblem is a pure partitioning problem, solved
// here by simulated annealing + steepest descent.
//
// The same routine also re-optimises the *first* stage (machines 1-4) in the
// reversed problem: those operations have no predecessor, and each carries a
// tail q_i (the longest path from its completion to the end) that is fixed as
// long as the rest of the schedule is untouched.  Minimising max(C_i + q_i)
// over a single machine is solved by processing in non-increasing tail order.
#include <bits/stdc++.h>
using namespace std;

static int nJobs, nMach, nOps;
static vector<int> opJob, opIdx;
static vector<vector<int>> DUR, elig;

static void parseInstance(const string& path) {
    ifstream f(path);
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
}

struct Row { int j, o, m, s, e; };

int main(int argc, char** argv) {
    string inst = "/app/instances/behnke16.fjs", sch = "/app/solution/schedule.txt", out = sch;
    long long iters = 20000000; int restarts = 30; uint32_t seed = 1;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "-i") inst = argv[++i]; else if (a == "-f") sch = argv[++i];
        else if (a == "-o") out = argv[++i]; else if (a == "-n") iters = atoll(argv[++i]);
        else if (a == "-R") restarts = atoi(argv[++i]); else if (a == "-r") seed = atoi(argv[++i]);
    }
    parseInstance(inst);
    // ---- read schedule ----
    ifstream f(sch); string w; int c; f >> w >> c;
    vector<Row> rows; Row r;
    while (f >> r.j >> r.o >> r.m >> r.s >> r.e) rows.push_back(r);
    if ((int)rows.size() != nOps) { fprintf(stderr, "bad schedule (%zu rows)\n", rows.size()); return 1; }
    int oldMk = 0; for (auto& x : rows) oldMk = max(oldMk, x.e);

    // index rows by (job, op)
    vector<vector<int>> idx(nJobs, vector<int>(5, -1));
    for (int i = 0; i < (int)rows.size(); i++) idx[rows[i].j][rows[i].o] = i;

    // ---- last-stage subproblem ----
    // release date of job j's op4 = completion of its op3
    vector<int> rel(nJobs), dur4(nJobs * 4);
    for (int j = 0; j < nJobs; j++) {
        rel[j] = rows[idx[j][3]].e;
        int op = idx[j][4] >= 0 ? 0 : 0;
        (void)op;
    }
    // duration table for op4 of each job on machines 17..20 (0-indexed 16..19)
    // find the global operation id of (j,4)
    vector<int> gid4(nJobs, -1);
    { int cur = 0; for (int o = 0; o < nOps; o++) if (opIdx[o] == 4) gid4[opJob[o]] = o; (void)cur; }
    for (int j = 0; j < nJobs; j++)
        for (int m = 0; m < 4; m++) dur4[j * 4 + m] = DUR[gid4[j]][16 + m];

    // makespan of one machine given the jobs assigned to it (ERD order optimal)
    vector<vector<int>> bucket(4);
    auto machMk = [&](int m, vector<int>& jobsOn) {
        sort(jobsOn.begin(), jobsOn.end(), [&](int a, int b) { return rel[a] < rel[b]; });
        int t = 0;
        for (int j : jobsOn) { t = max(t, rel[j]) + dur4[j * 4 + m]; }
        return t;
    };
    auto totalMk = [&](vector<int>& asg) {
        for (int m = 0; m < 4; m++) bucket[m].clear();
        for (int j = 0; j < nJobs; j++) bucket[asg[j]].push_back(j);
        int mk = 0; for (int m = 0; m < 4; m++) mk = max(mk, machMk(m, bucket[m]));
        return mk;
    };

    mt19937 rng(seed * 7919u + 13u);
    vector<int> best(nJobs), cur(nJobs);
    // start from the current schedule's assignment
    for (int j = 0; j < nJobs; j++) cur[j] = rows[idx[j][4]].m - 17;
    best = cur; int bestMk = totalMk(cur);
    int startMk = bestMk;

    for (int rs = 0; rs < restarts; rs++) {
        if (rs == 0) cur = best;
        else { for (int j = 0; j < nJobs; j++) cur[j] = rng() % 4; }
        int curMk = totalMk(cur);
        double T0 = 25.0, T1 = 0.05;
        for (long long it = 0; it < iters / restarts; it++) {
            double T = T0 * pow(T1 / T0, (double)it * restarts / iters);
            int j = rng() % nJobs; int om = cur[j]; int nm = rng() % 4;
            if (nm == om) continue;
            cur[j] = nm;
            int v = totalMk(cur);
            int d = v - curMk;
            if (d <= 0 || exp(-d / T) > (double)(rng() % 1000000) / 1e6) {
                curMk = v;
                if (v < bestMk) { bestMk = v; best = cur; }
            } else cur[j] = om;
        }
    }
    fprintf(stderr, "last stage: %d -> %d\n", startMk, bestMk);

    // ---- rewrite the stage-C rows ----
    for (int m = 0; m < 4; m++) bucket[m].clear();
    for (int j = 0; j < nJobs; j++) bucket[best[j]].push_back(j);
    for (int m = 0; m < 4; m++) {
        sort(bucket[m].begin(), bucket[m].end(), [&](int a, int b) { return rel[a] < rel[b]; });
        int t = 0;
        for (int j : bucket[m]) {
            int st = max(t, rel[j]);
            int d = dur4[j * 4 + m];
            rows[idx[j][4]].m = 17 + m; rows[idx[j][4]].s = st; rows[idx[j][4]].e = st + d;
            t = st + d;
        }
    }
    int newMk = 0; for (auto& x : rows) newMk = max(newMk, x.e);
    fprintf(stderr, "schedule makespan %d -> %d\n", oldMk, newMk);
    if (newMk < oldMk || out != sch) {
        string tmp = out + ".tmp";
        FILE* g = fopen(tmp.c_str(), "w");
        fprintf(g, "makespan %d\n", newMk);
        for (auto& x : rows) fprintf(g, "%d %d %d %d %d\n", x.j, x.o, x.m, x.s, x.e);
        fclose(g); rename(tmp.c_str(), out.c_str());
    }
    printf("%d\n", newMk);
    return 0;
}
