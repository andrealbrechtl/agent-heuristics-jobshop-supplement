"""Left-compaction of a job-shop schedule (semi-active schedule with the same machine orders).

Every operation is moved as early as its job predecessor and its machine predecessor (in the
original schedule's machine order) allow. The result is feasible whenever the input is, and its
makespan is never larger.

usage: python compact.py instance.txt schedule_in.txt schedule_out.txt
"""
import sys
import numpy as np


def load_instance(path):
    a = np.loadtxt(path, skiprows=1, dtype=np.int64)
    return a[:, 0::2].copy(), a[:, 1::2].copy()


def load_schedule(path):
    return np.loadtxt(path, comments='#', dtype=np.int64)


def compact(mach, dur, S):
    N, M = S.shape
    order = np.argsort(S, axis=None, kind='stable')
    out = np.zeros_like(S)
    jend = np.zeros(N, np.int64)
    mend = np.zeros(mach.max() + 1, np.int64)
    md, dd = mach.ravel(), dur.ravel()
    for idx in order.tolist():
        j, o = divmod(idx, M)
        k = md[idx]
        s = jend[j] if jend[j] > mend[k] else mend[k]
        out[j, o] = s
        e = s + dd[idx]
        jend[j] = e
        mend[k] = e
    return out


def makespan(dur, S):
    return int((S[:, -1] + dur[:, -1]).max())


if __name__ == '__main__':
    mach, dur = load_instance(sys.argv[1])
    S = load_schedule(sys.argv[2])
    C = compact(mach, dur, S)
    np.savetxt(sys.argv[3], C, fmt='%d')
    print(f'makespan {makespan(dur, S)} -> {makespan(dur, C)}')
