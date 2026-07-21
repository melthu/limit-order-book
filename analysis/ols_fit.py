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


if __name__ == "__main__":
    main()
