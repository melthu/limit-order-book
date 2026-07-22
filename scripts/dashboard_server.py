#!/usr/bin/env python3
# phase 5 — bridge between the C++ live feed and the browser dashboard.
#
# reads the `crypto emit` stream (depth ladder + 3 signals, one line per 100ms
# snapshot), applies the frozen phase-4 model to get position + running P&L, and
# broadcasts each tick as JSON over a websocket. the browser (dashboard.html)
# just draws it.
#
# the split is deliberate: C++ does the book + signals on the hot path, python
# does the model math + P&L accounting, the browser only renders.
#
# P&L runs perpetually server-side and RESETS at 7am each morning (crypto has no
# session, so we impose a daily "open"). state is persisted to disk so a restart
# resumes the same day, and the full curve is pushed to any newly-opened page —
# so the dashboard never "starts at zero" just because you opened it.
#
# trade rule = low-turnover hold-until-reversal (phase-4 target-position design):
# flip only when the forecast clears +/-ENTER_BPS, otherwise hold. this nets
# consecutive same-direction signals and keeps the fee tally sane.
#
# pipeline:
#   python3 scripts/crypto_feed.py | ./build/crypto emit | python3 scripts/dashboard_server.py
# then open scripts/dashboard.html in a browser.
#
# HONEST FRAMING: live feed is Binance.US SPOT, the model trained on Binance
# FUTURES — so the P&L is an illustrative demo on a real live stream, not an
# accuracy claim. the rigorous number is the phase-4 test result.

import asyncio, json, os, sys, threading, time
from datetime import datetime, timedelta
from zoneinfo import ZoneInfo
import numpy as np
import websockets

PORT = int(os.environ.get("LOB_PORT", "8765"))
HOST = os.environ.get("LOB_HOST", "localhost")   # 0.0.0.0 on a VM behind a tunnel
FEE_BPS = 5.0        # Binance USDT-M taker (VIP0), for the net-after-fees curve
CLIP = 1.0           # fixed 1-BTC clip, so P&L reads directly in dollars
ENTER_BPS = 0.05     # flip only when the forecast clears this (nets signals, cuts churn)
RESET_HOUR = 7       # daily "market open": P&L resets at 7am local
TZ = ZoneInfo(os.environ.get("LOB_TZ", "America/New_York"))
HIST_SEC = 5         # chart point cadence (the numbers still update every tick)
HIST_MAX = 20000
STATE_FILE = "data/pnl_state.json"


def load_model(path="analysis/model.json"):
    with open(path) as f:
        m = json.load(f)
    return np.array(m["mu"]), np.array(m["sd"]), np.array(m["beta"]), m["horizon_ms"]


MU, SD, BETA, H_MS = load_model()
clients = set()
cum = dict(day=None, gross=0.0, fees=0.0, flips=0, pos=0.0)   # perpetual, shared
hist = []                                                     # [[t, gross, net], ...]


def session_key(now):
    # which 7am-anchored day this instant belongs to
    local = datetime.fromtimestamp(now, TZ)
    if local.hour < RESET_HOUR:
        local -= timedelta(days=1)
    return local.strftime("%Y-%m-%d")


def load_state():
    try:
        with open(STATE_FILE) as f:
            s = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        s = None
    day = session_key(time.time())
    if not s or s.get("day") != day:          # stale / new day -> start fresh
        return dict(day=day, gross=0.0, fees=0.0, flips=0, pos=0.0), []
    return dict(day=s["day"], gross=s["gross"], fees=s["fees"],
                flips=s["flips"], pos=s["pos"]), s.get("history", [])


def save_state():
    os.makedirs("data", exist_ok=True)
    tmp = STATE_FILE + ".tmp"
    with open(tmp, "w") as f:
        json.dump(dict(**cum, history=hist), f)
    os.replace(tmp, STATE_FILE)               # atomic


def parse(line):
    # D <mid> <imb> <micro> <ofi>  B <p> <q>...  A <p> <q>...
    t = line.split()
    if not t or t[0] != "D":
        return None
    mid, imb, micro, ofi = map(float, t[1:5])
    ai = t.index("A", 5)
    bl = [float(x) for x in t[6:ai]]
    al = [float(x) for x in t[ai + 1:]]
    bids = [[bl[i], bl[i + 1]] for i in range(0, len(bl), 2)]
    asks = [[al[i], al[i + 1]] for i in range(0, len(al), 2)]
    return mid, imb, micro, ofi, bids, asks


def feed(loop, queue):
    # thread: read stdin, apply the model + P&L accounting, push json ticks out.
    st, h0 = load_state()
    cum.update(st)
    hist[:] = h0
    prev_mid, last_hist, last_save = None, 0.0, 0.0

    for line in sys.stdin:
        rec = parse(line)
        if rec is None:
            continue
        mid, imb, micro, ofi, bids, asks = rec
        now = time.time()

        # 7am daily reset (the crypto "market open")
        day = session_key(now)
        if day != cum["day"]:
            cum.update(day=day, gross=0.0, fees=0.0, flips=0, pos=0.0)
            hist.clear(); prev_mid = None

        # 1) realize the last interval's dollar P&L on the clip we held (no
        #    lookahead: decided last tick, earn the move into this one).
        if prev_mid is not None:
            cum["gross"] += cum["pos"] * (mid - prev_mid) * CLIP

        # 2) model decision, kept transparent: z-score each signal with the train
        #    scaler, times its coefficient = its contribution (bps) to the forecast.
        z = (np.array([imb, micro, ofi]) - MU) / SD
        contrib = BETA[1:] * z
        f = BETA[0] + contrib.sum()             # predicted mid move over h, bps

        # 3) low-turnover rule: flip only when the forecast clears the band, else hold
        newpos = cum["pos"]
        if f > ENTER_BPS:
            newpos = 1.0
        elif f < -ENTER_BPS:
            newpos = -1.0
        if newpos != cum["pos"]:                # crossing the spread costs a taker fee
            cum["fees"] += FEE_BPS / 1e4 * mid * abs(newpos - cum["pos"]) * CLIP
            cum["flips"] += 1
        cum["pos"], prev_mid = newpos, mid
        net = cum["gross"] - cum["fees"]

        msg = json.dumps(dict(
            type="tick", mid=mid, imb=imb, micro=micro, ofi=ofi, h=H_MS, pos=cum["pos"],
            c_imb=contrib[0], c_micro=contrib[1], c_ofi=contrib[2], forecast=f,
            gross=cum["gross"], net=net, fees=cum["fees"], flips=cum["flips"], day=cum["day"],
            bids=bids, asks=asks))

        if now - last_hist >= HIST_SEC:         # coarse curve for the chart
            hist.append([now, round(cum["gross"], 4), round(net, 4)])
            if len(hist) > HIST_MAX:
                del hist[:len(hist) - HIST_MAX]
            last_hist = now
        if now - last_save >= 10:               # survive restarts
            save_state(); last_save = now

        loop.call_soon_threadsafe(queue.put_nowait, msg)


async def handler(ws):
    clients.add(ws)
    try:
        # hand the newcomer the running total + curve so it doesn't start at zero
        await ws.send(json.dumps(dict(
            type="history", h=H_MS, day=cum["day"], reset_hour=RESET_HOUR, enter=ENTER_BPS,
            gross=cum["gross"], net=cum["gross"] - cum["fees"],
            fees=cum["fees"], flips=cum["flips"], history=list(hist))))
        await ws.wait_closed()
    finally:
        clients.discard(ws)


async def main():
    loop = asyncio.get_running_loop()
    queue = asyncio.Queue()
    threading.Thread(target=feed, args=(loop, queue), daemon=True).start()
    async with websockets.serve(handler, HOST, PORT):
        print(f"dashboard ws on ws://{HOST}:{PORT}  ·  open scripts/dashboard.html",
              file=sys.stderr)
        while True:
            msg = await queue.get()
            websockets.broadcast(clients, msg)     # fan out to every open page


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
