#!/usr/bin/env python3
# freeze the trained model for the live dashboard.
#
# refits the phase-4 OLS combo on the TRAIN block at one horizon and dumps the
# train-only scaler (mu/sd) + coefficients to analysis/model.json. the dashboard
# loads that tiny file, so it runs without the 3.5GB of signal csvs. the model is
# the same one the phase-4 study reports; this just pins it to disk.
#
# usage: .venv/bin/python analysis/export_model.py [horizon_ms]

import json, sys
import numpy as np
from common import TRAIN, SIGNALS, day_files, load, fwd_return_bps

H_MS = int(sys.argv[1]) if len(sys.argv) > 1 else 1000   # 1s: strong IC, on the plateau


def main():
    tr = load(day_files(TRAIN), ["ts", "mid"] + SIGNALS)
    mu = np.array([tr[s].mean() for s in SIGNALS])
    sd = np.array([tr[s].std() for s in SIGNALS])

    X = np.column_stack([np.ones(len(tr["ts"]))] +
                        [(tr[s] - mu[i]) / sd[i] for i, s in enumerate(SIGNALS)])
    y = fwd_return_bps(tr["ts"], tr["mid"], tr["_slices"], H_MS)
    m = ~np.isnan(y)
    beta, *_ = np.linalg.lstsq(X[m], y[m], rcond=None)     # [intercept, imb, micro, ofi]

    model = dict(horizon_ms=H_MS, signals=SIGNALS,
                 mu=mu.tolist(), sd=sd.tolist(), beta=beta.tolist())
    with open("analysis/model.json", "w") as f:
        json.dump(model, f, indent=2)
    print(f"wrote analysis/model.json  (h={H_MS}ms)")
    print("beta [intercept, imbalance, micro_dev, ofi] =",
          [f"{b:+.4f}" for b in beta])


if __name__ == "__main__":
    main()
