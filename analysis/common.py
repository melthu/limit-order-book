# shared bits for the phase-4 crypto study: the train/val/test split, loading
# signal csvs into arrays, and the within-day forward return.

import glob, os
import numpy as np
import pandas as pd

# contiguous day-blocks in time order (never shuffled). see notes.md.
TRAIN = ("2024-02-15", "2024-03-15")   # 30 days — fit coefficients + scaler
VAL   = ("2024-03-16", "2024-03-20")   #  5 days — pick horizon + thresholds
TEST  = ("2024-03-21", "2024-03-30")   # 10 days — touched once, at the end

SIGNALS = ["imbalance", "micro_dev", "ofi"]


def day_files(block, d="data/signals"):
    lo, hi = block
    return sorted(f for f in glob.glob(f"{d}/*.csv")
                  if lo <= os.path.basename(f)[:10] <= hi)


def load(files, cols):
    # concatenate the requested columns across days, and remember where each day
    # starts/ends so forward returns never cross a day boundary.
    chunks, slices, off = [], [], 0
    for f in files:
        df = pd.read_csv(f, usecols=cols)
        chunks.append(df)
        slices.append((off, off + len(df)))
        off += len(df)
    big = pd.concat(chunks, ignore_index=True)
    arrs = {c: big[c].to_numpy() for c in cols}
    arrs["_slices"] = slices
    return arrs


def fwd_return_bps(ts, mid, slices, h):
    # next mid at or after t+h, computed within each day; NaN where the window
    # runs off the end of that day.
    y = np.full(len(ts), np.nan)
    for a, b in slices:
        t, m = ts[a:b], mid[a:b]
        j = np.searchsorted(t, t + h, side="left")
        ok = j < len(t)
        y[a:b][ok] = (m[j[ok]] / m[ok.nonzero()[0]] - 1.0) * 1e4
    return y


def ic(x, y):
    m = ~np.isnan(y)
    if m.sum() < 2:
        return np.nan
    return np.corrcoef(x[m], y[m])[0, 1]
