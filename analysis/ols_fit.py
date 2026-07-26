#!/usr/bin/env python3
# phase 4a (part 2) — combine the three signals with a linear model.
#
# fit  yhat = b0 + b1*imbalance + b2*micro_dev + b3*ofi  by least squares on the
# TRAIN block, predicting the forward mid return (bps) at each horizon. features
# are z-scored with TRAIN mean/std (reused on val/test — no scaler leak), so each
# coefficient reads as "bps of forward move per 1 std of that signal."
#
# the honest test isn't in-sample R² (with ~24M rows every t-stat is enormous and
# meaningless) — it's whether the combined forecast's IC holds up out-of-sample.
# so we report the combo IC on train vs val vs TEST, next to the best single signal.
#
# usage: .venv/bin/python analysis/ols_fit.py

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from common import (TRAIN, VAL, TEST, SIGNALS, day_files, load,
                    fwd_return_bps, ic)

HORIZONS_MS = [100, 200, 500, 1000, 2000, 5000]
COLS = ["ts", "mid"] + SIGNALS


def design(arrs, mu, sd):
    X = np.column_stack([(arrs[s] - mu[i]) / sd[i] for i, s in enumerate(SIGNALS)])
    return np.column_stack([np.ones(len(X)), X])   # intercept + 3 standardized signals


def main():
    tr = load(day_files(TRAIN), COLS)
    va = load(day_files(VAL), COLS)
    te = load(day_files(TEST), COLS)
    print(f"train {len(tr['ts']):,} rows · val {len(va['ts']):,} · test {len(te['ts']):,}")

    # scaler from TRAIN only
    mu = np.array([tr[s].mean() for s in SIGNALS])
    sd = np.array([tr[s].std() for s in SIGNALS])
    Xtr, Xva, Xte = design(tr, mu, sd), design(va, mu, sd), design(te, mu, sd)

    rows = []
    for h in HORIZONS_MS:
        ytr = fwd_return_bps(tr["ts"], tr["mid"], tr["_slices"], h)
        yva = fwd_return_bps(va["ts"], va["mid"], va["_slices"], h)
        yte = fwd_return_bps(te["ts"], te["mid"], te["_slices"], h)

        m = ~np.isnan(ytr)
        beta, *_ = np.linalg.lstsq(Xtr[m], ytr[m], rcond=None)
        yhat_tr, yhat_va, yhat_te = Xtr @ beta, Xva @ beta, Xte @ beta

        # R2 on train
        r = ytr[m] - yhat_tr[m]
        r2 = 1 - (r @ r) / (((ytr[m] - ytr[m].mean()) ** 2).sum())

        # best single-signal IC on test, for comparison
        best_single = max(abs(ic(te[s], yte)) for s in SIGNALS)

        rows.append(dict(h_ms=h,
                         b_imb=beta[1], b_micro=beta[2], b_ofi=beta[3],
                         R2_tr=r2,
                         IC_tr=ic(yhat_tr, ytr), IC_va=ic(yhat_va, yva),
                         IC_te=ic(yhat_te, yte), best1_te=best_single))

    res = pd.DataFrame(rows)
    pd.set_option("display.float_format", lambda v: f"{v:.4f}")
    print("\ncoefficients are bps per 1 std; IC_* = corr of combined forecast vs realized return")
    print(res.to_string(index=False))

    # generalization check: does test IC track train IC?
    print("\ncombo IC train->test retention (test/train):")
    for _, r in res.iterrows():
        print(f"  h={int(r.h_ms):>5}ms  {r.IC_te / r.IC_tr:5.2f}")

    # companion to ic_decay.png: how the joint model weights each signal as h grows.
    # imbalance carries the largest weight at every horizon; ofi adds a smaller flow
    # component; micro_dev sits at ~0 (collinear with imbalance, drops out).
    import plotstyle as ps
    ps.setup()
    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    ax.axhline(0, color="#9ca3af", lw=.8, zorder=1)
    ps.line(ax, res.h_ms, res.b_imb,   "imbalance")
    ps.line(ax, res.h_ms, res.b_ofi,   "ofi")
    ps.line(ax, res.h_ms, res.b_micro, "micro_dev")
    ax.set_ylim(bottom=-0.07)               # headroom for the note below the micro line
    ax.annotate("microprice ≈ 0  (collinear, drops out)",
                xy=(2000, res.b_micro.iloc[-2]), xytext=(230, -0.055),
                fontsize=9, color="#6b7280",
                arrowprops=dict(arrowstyle="->", color="#9ca3af", lw=1))
    ps.finish(ax, HORIZONS_MS, "Forward horizon  (ms, log scale)",
              "OLS weight  (bps per 1σ)",
              "OLS coefficient by signal vs horizon")
    out = "analysis/beta_vs_horizon.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"\nwrote {out}")

    # horizon selection: fit on train, pick on val. val IC is strong and broad
    # across 100ms-1s (peaks ~200ms); the live demo freezes 1s — a hair below the
    # val peak but safely above reaction latency (a 200ms horizon gets eaten by a
    # 50-200ms retail delay). so it's a robustness pick, not the raw val max.
    fig, ax = plt.subplots(figsize=(7.2, 4.6))
    ax.axvspan(90, 200, color="#fca5a5", alpha=0.18, zorder=0)     # retail latency zone
    ax.text(133, 0.128, "≲ reaction\nlatency", fontsize=8.5, color="#b91c1c",
            ha="center", va="bottom")
    ax.plot(res.h_ms, res.IC_tr, marker="o", ms=6, lw=2, color="#9ca3af",
            markeredgecolor="white", markeredgewidth=1, label="train", zorder=3)
    ax.plot(res.h_ms, res.IC_va, marker="o", ms=6, lw=2, color="#2563eb",
            markeredgecolor="white", markeredgewidth=1, label="validation", zorder=3)
    ax.axvline(1000, color="#10b981", lw=1.5, ls="--", zorder=2)
    va1000 = res.loc[res.h_ms == 1000, "IC_va"].iloc[0]
    ax.annotate("frozen for demo (1 s):\nstrong IC, clears latency",
                xy=(1000, va1000), xytext=(1050, 0.255), fontsize=9, color="#065f46",
                arrowprops=dict(arrowstyle="->", color="#10b981", lw=1))
    ps.finish(ax, HORIZONS_MS, "Forward horizon  (ms, log scale)",
              "Combined-model IC", "Combined-model IC vs horizon: train vs validation")
    out = "analysis/horizon_selection.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
