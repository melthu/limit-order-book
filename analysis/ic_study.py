#!/usr/bin/env python3
# phase 4a — do the three signals predict the next mid move?
#
# for a grid of horizons h (ms), line up each signal at time t against the
# forward mid return over the next h ms, and measure:
#   - IC        pearson corr(signal, fwd_return)   (streamed exactly over all rows)
#   - rank IC   spearman, on a capped random subsample (robust to fat tails)
#   - hit rate  how often sign(signal) == sign(fwd_return)   vs a 50% coin flip
#
# returns are computed within a single day only (each csv is one day), so a
# forward window never reaches across a day boundary — that's the leakage guard.
#
# usage: .venv/bin/python analysis/ic_study.py [signals_dir] [start_date] [end_date]
#   defaults to the training block: data/signals, 2024-02-15 .. 2024-03-15

import sys, glob, os
import numpy as np
import pandas as pd
from scipy import stats
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HORIZONS_MS = [100, 200, 500, 1000, 2000, 5000]
SIGNALS = ["imbalance", "micro_dev", "ofi"]
SUBSAMPLE = 1_000_000          # cap for the spearman subsample (pooled)


def fwd_return_bps(ts, mid, h):
    # next mid at or after t+h, within this day; NaN if the window runs off the end
    j = np.searchsorted(ts, ts + h, side="left")
    ok = j < len(ts)
    y = np.full(len(ts), np.nan)
    y[ok] = (mid[j[ok]] / mid[ok.nonzero()[0]] - 1.0) * 1e4
    return y


def run(files):
    # streaming sufficient stats for pearson + hit rate, keyed by (signal, h)
    keys = [(s, h) for s in SIGNALS for h in HORIZONS_MS]
    acc = {k: np.zeros(6) for k in keys}      # n, Sx, Sy, Sxy, Sxx, Syy
    hit = {k: np.zeros(2) for k in keys}      # agree, total
    sub = {k: [] for k in keys}               # (x, y) chunks for spearman
    sub_n = {k: 0 for k in keys}

    for f in files:
        df = pd.read_csv(f, usecols=["ts", "mid"] + SIGNALS)
        ts = df["ts"].to_numpy()
        mid = df["mid"].to_numpy()
        rng = np.random.default_rng(0)
        for h in HORIZONS_MS:
            y = fwd_return_bps(ts, mid, h)
            m = ~np.isnan(y)
            for s in SIGNALS:
                x = df[s].to_numpy()[m]
                yy = y[m]
                k = (s, h)
                a = acc[k]
                a[0] += len(x); a[1] += x.sum(); a[2] += yy.sum()
                a[3] += (x * yy).sum(); a[4] += (x * x).sum(); a[5] += (yy * yy).sum()
                nz = np.sign(yy) != 0          # only score rows that actually moved
                hit[k][0] += (np.sign(x[nz]) == np.sign(yy[nz])).sum()
                hit[k][1] += nz.sum()
                if sub_n[k] < SUBSAMPLE and len(x):        # reservoir-ish: take a slice
                    take = min(len(x), SUBSAMPLE // len(files) + 1)
                    idx = rng.choice(len(x), take, replace=False) if take < len(x) else slice(None)
                    sub[k].append((x[idx], yy[idx])); sub_n[k] += take
        print(f"  {os.path.basename(f)}", flush=True)

    rows = []
    for s in SIGNALS:
        for h in HORIZONS_MS:
            k = (s, h)
            n, Sx, Sy, Sxy, Sxx, Syy = acc[k]
            cov = n * Sxy - Sx * Sy
            den = np.sqrt((n * Sxx - Sx * Sx) * (n * Syy - Sy * Sy))
            ic = cov / den if den > 0 else np.nan
            xs = np.concatenate([c[0] for c in sub[k]])
            ys = np.concatenate([c[1] for c in sub[k]])
            ric = stats.spearmanr(xs, ys).statistic
            rows.append(dict(signal=s, h_ms=h, n=int(n), IC=ic, rank_IC=ric,
                             hit=hit[k][0] / hit[k][1]))
    return pd.DataFrame(rows)


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else "data/signals"
    lo = sys.argv[2] if len(sys.argv) > 2 else "2024-02-15"
    hi = sys.argv[3] if len(sys.argv) > 3 else "2024-03-15"
    files = sorted(f for f in glob.glob(f"{d}/*.csv")
                   if lo <= os.path.basename(f)[:10] <= hi)
    if not files:
        sys.exit(f"no signal csvs in {d} for {lo}..{hi}")
    print(f"{len(files)} days ({lo}..{hi})")
    res = run(files)

    pd.set_option("display.float_format", lambda v: f"{v:.4f}")
    print("\n" + res.to_string(index=False))

    # the money plot: IC vs horizon per signal
    fig, ax = plt.subplots(figsize=(7, 4.5))
    for s in SIGNALS:
        r = res[res.signal == s]
        ax.plot(r.h_ms, r.IC, marker="o", label=s)
    ax.axhline(0, color="#888", lw=.8)
    ax.set_xscale("log")
    ax.set_xlabel("horizon h (ms, log)"); ax.set_ylabel("IC  (pearson)")
    ax.set_title("signal IC vs forward horizon — train block")
    ax.legend(); fig.tight_layout()
    out = "analysis/ic_decay.png"
    fig.savefig(out, dpi=130)
    print(f"\nwrote {out}")


if __name__ == "__main__":
    main()
