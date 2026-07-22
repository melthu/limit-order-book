#!/usr/bin/env bash
# run the live backend pipeline with auto-restart — for an always-on host (the
# VM behind the dashboard) or just locally. reconnects if the feed drops.
#
#   feed (Binance.US spot)  ->  C++ book+signals  ->  model + P&L bridge (ws)
#
# expose the ws publicly with a tunnel, e.g.:
#   cloudflared tunnel --url http://localhost:8765
# then point the page at the wss URL it prints (set BACKEND in dashboard.html).
set -u
cd "$(dirname "$0")/.."
PY="${PY:-.venv/bin/python}"
export LOB_HOST="${LOB_HOST:-localhost}"   # localhost is fine behind a tunnel
export LOB_PORT="${LOB_PORT:-8765}"
export LOB_TZ="${LOB_TZ:-America/New_York}"

while true; do
  echo "[run_backend] starting pipeline $(date -u)" >&2
  "$PY" scripts/crypto_feed.py | ./build/crypto emit | "$PY" scripts/dashboard_server.py
  echo "[run_backend] pipeline exited — restarting in 3s" >&2
  sleep 3
done
