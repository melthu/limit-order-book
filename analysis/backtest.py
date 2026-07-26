#!/usr/bin/env python3
# phase 4b — taker-only paper P&L on the held-out TEST block.
#
# model: the trained OLS combo forecasts the mid move over horizon h. when the
# forecast fires we cross the spread against the REAL book (lift the ask to go
# long / hit the bid to go short), hold h, then close by crossing back. every
# fill uses the real bid/ask, so the spread is paid directly; the taker fee is
# paid on both legs. reaction latency delta: we see the signal at t but only get
# filled at the book as it stands at t+delta.
#
# trades are non-overlapping (enter, wait out the horizon, then look again), so
# each is an independent bet — the clean read on "did the h-ahead forecast pay."
#
# the point is the gross-vs-net gap: the signal makes money before costs and the
# round-trip cost (~1 spread + 2 fees) eats it. we print both and sweep the fee
# to find the breakeven.
#
# usage: .venv/bin/python analysis/backtest.py [fee_bps] [delta_ms]
#   defaults: Binance USDT-M taker 5.0 bps (0.05%, VIP0), delta 50 ms (retail-ish)

import sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from common import (TRAIN, TEST, SIGNALS, day_files, load, fwd_return_bps)

HORIZONS_MS = [100, 200, 500, 1000, 2000, 5000]
COLS_TR = ["ts", "mid"] + SIGNALS
COLS_TE = ["ts", "mid", "bid", "ask"] + SIGNALS


def fit_betas(tr, mu, sd):
    X = np.column_stack([np.ones(len(tr["ts"]))] +
                        [(tr[s] - mu[i]) / sd[i] for i, s in enumerate(SIGNALS)])
    betas = {}
    for h in HORIZONS_MS:
        y = fwd_return_bps(tr["ts"], tr["mid"], tr["_slices"], h)
        m = ~np.isnan(y)
        betas[h], *_ = np.linalg.lstsq(X[m], y[m], rcond=None)
    return betas


def backtest_day(ts, bid, ask, yhat, h, delta, fee_bps, gate=0.0):
    # non-overlapping taker round-trips on a regular decision cadence of h ms.
    # at each decision time we read the signal known then, and if it fires we
    # fill at t+delta and close at t+delta+h against the real book.
    n = len(ts)
    grid = np.arange(ts[0], ts[-1] - h - delta, h)      # decision times
    if len(grid) == 0:
        return np.zeros((0, 2))
    kd = np.clip(np.searchsorted(ts, grid, side="right") - 1, 0, n - 1)  # signal as of grid
    yd = yhat[kd]
    fire = np.abs(yd) > gate
    grid, yd = grid[fire], yd[fire]

    ke = np.searchsorted(ts, grid + delta)               # entry fill (after latency)
    kx = np.searchsorted(ts, grid + delta + h)           # exit fill (one horizon later)
    ok = kx < n
    ke, kx, yd = ke[ok], kx[ok], yd[ok]

    side = np.sign(yd)
    entry = np.where(side > 0, ask[ke], bid[ke])         # buy ask / sell bid
    exit_ = np.where(side > 0, bid[kx], ask[kx])         # sell bid / buy ask
    gross = side * (exit_ - entry) / entry * 1e4
    return np.column_stack([gross, gross - 2 * fee_bps])


def main():
    fee_bps = float(sys.argv[1]) if len(sys.argv) > 1 else 5.0
    delta = float(sys.argv[2]) if len(sys.argv) > 2 else 50.0

    tr = load(day_files(TRAIN), COLS_TR)
    mu = np.array([tr[s].mean() for s in SIGNALS])
    sd = np.array([tr[s].std() for s in SIGNALS])
    betas = fit_betas(tr, mu, sd)

    te = load(day_files(TEST), COLS_TE)
    Xte = np.column_stack([np.ones(len(te["ts"]))] +
                          [(te[s] - mu[i]) / sd[i] for i, s in enumerate(SIGNALS)])

    cost = 2 * fee_bps        # round-trip taker cost (spread on BTC is ~0.02 bps, negligible)
    print(f"fee {fee_bps:.2f} bps/side · latency {delta:.0f} ms · test = {len(day_files(TEST))} days")
    print(f"round-trip cost ~ 2 fees + 1 spread ≈ {cost:.1f} bps\n")
    print(f"{'h_ms':>6} {'trades':>10} {'gross/t':>9} {'t-stat':>8} {'net/t':>9} "
          f"{'breakeven_fee':>14} {'costgate_trades':>16}")

    plot_rows = []          # (h, gross_edge) for the edge-vs-cost figure
    for h in HORIZONS_MS:
        yhat = Xte @ betas[h]
        allt, gate_hits = [], 0
        for a, b in te["_slices"]:
            r = backtest_day(te["ts"][a:b], te["bid"][a:b], te["ask"][a:b],
                             yhat[a:b], h, delta, fee_bps)
            if len(r):
                allt.append(r)
            gate_hits += int((np.abs(yhat[a:b]) > cost).sum())   # decided rule: predict > cost
        allt = np.concatenate(allt) if allt else np.zeros((0, 2))
        if not len(allt):
            print(f"{h:>6} {0:>10}"); continue
        g = allt[:, 0]
        gross_t = g.mean()
        tstat = gross_t / g.std() * np.sqrt(len(g))    # is the gross edge real?
        net_t = gross_t - cost
        breakeven = gross_t / 2                         # fee/side that would zero it out
        print(f"{h:>6} {len(allt):>10,} {gross_t:>9.4f} {tstat:>8.1f} {net_t:>9.3f} "
              f"{breakeven:>13.3f}  {gate_hits:>16,}")
        plot_rows.append((h, gross_t))

    # the phase-4b money plot: the raw signal edge per trade vs the cost to trade it.
    # both in bps on a log axis so the ~2-orders-of-magnitude gap is legible — the
    # edge grows with horizon but never reaches the cost line, so it's never tradeable.
    import plotstyle as ps
    ps.setup()
    hs = [r[0] for r in plot_rows]
    gross = [r[1] for r in plot_rows]
    fig, ax = plt.subplots(figsize=(7.4, 4.6))
    ax.axhspan(cost, cost * 3, color="#dcfce7", zorder=0)     # the tradeable band
    ax.text(hs[0], cost * 1.35, "tradeable (edge > cost)", fontsize=9, color="#16a34a")
    ax.plot(hs, gross, marker="o", ms=7, lw=2.2, color="#2563eb",
            markeredgecolor="white", markeredgewidth=1, label="gross edge per trade", zorder=3)
    ax.axhline(cost, color="#dc2626", lw=2, ls=(0, (5, 3)), zorder=2,
               label=f"cost to trade (1 spread + 2 taker fees ≈ {cost:.0f} bps)")
    ax.set_xscale("log"); ax.set_yscale("log")
    ax.set_xticks(hs); ax.set_xticklabels([str(h) for h in hs]); ax.minorticks_off()
    ax.set_ylim(min(gross) * 0.6, cost * 3)
    worst, best = cost / max(gross), cost / min(gross)
    ax.annotate(f"{worst:.0f}–{best:.0f}× short of the cost line",
                xy=(hs[-1], gross[-1]), xytext=(hs[1], gross[-1] * 2.3),
                fontsize=9, color="#374151",
                arrowprops=dict(arrowstyle="->", color="#9ca3af", lw=1))
    ax.set_xlabel("Forward horizon  (ms, log scale)")
    ax.set_ylabel("Per-trade P&L  (bps, log scale)")
    ax.set_title("Per-trade edge vs trading cost, by horizon")
    ax.spines["top"].set_visible(False); ax.spines["right"].set_visible(False)
    ax.tick_params(length=0); ax.legend(loc="lower right")
    out = "analysis/edge_vs_cost.png"
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"\nwrote {out}")

    print("\ngross/t = per-trade bps before fees (raw signal edge) · t-stat over all trades ·")
    print("net/t = gross − round-trip cost · breakeven_fee = the per-side fee that zeroes net ·")
    print("costgate_trades = decisions where predicted move > cost (the decided 'only trade if")
    print("it beats cost' rule) — how often the edge actually clears the bar.")


if __name__ == "__main__":
    main()
