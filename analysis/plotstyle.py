# shared look for the phase-4 figures so both plots match (same colors, same theme).
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

# one color + display name per signal, reused across every figure
NAME  = {"imbalance": "Imbalance", "micro_dev": "Microprice dev.", "ofi": "OFI"}
COLOR = {"imbalance": "#2563eb", "ofi": "#f59e0b", "micro_dev": "#9ca3af"}


def setup():
    plt.rcParams.update({
        "figure.facecolor": "white",
        "axes.facecolor":   "white",
        "axes.grid":        True,
        "grid.color":       "#e5e7eb",
        "grid.linewidth":   0.8,
        "font.size":        11,
        "axes.titlesize":   13,
        "axes.titleweight": "bold",
        "axes.labelsize":   11,
        "axes.labelcolor":  "#111827",
        "xtick.color":      "#374151",
        "ytick.color":      "#374151",
        "legend.frameon":   False,
        "legend.fontsize":  10,
    })


# draw one signal series with the shared color, markers, and a white marker edge
def line(ax, x, y, signal, **kw):
    ax.plot(x, y, marker="o", ms=6, lw=2, color=COLOR[signal],
            markeredgecolor="white", markeredgewidth=1, label=NAME[signal], zorder=3, **kw)


# log-x horizon axis with the round grid values labeled, top/right spines dropped
def finish(ax, horizons, xlabel, ylabel, title):
    ax.set_xscale("log")
    ax.set_xticks(horizons)
    ax.set_xticklabels([str(h) for h in horizons])
    ax.minorticks_off()
    ax.set_xlabel(xlabel); ax.set_ylabel(ylabel); ax.set_title(title, pad=12)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.tick_params(length=0)
    ax.legend(loc="best")
