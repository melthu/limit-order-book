#!/usr/bin/env bash
# one command to run the live dashboard locally: start the pipeline and open
# the page. this is what you screen-record for the README gif.
#   feed (Binance.US spot) -> C++ book+signals -> model+P&L bridge -> browser
set -u
cd "$(dirname "$0")/.."
PY="${PY:-.venv/bin/python}"

"$PY" scripts/crypto_feed.py | ./build/crypto emit | "$PY" scripts/dashboard_server.py &
trap 'kill 0' INT TERM          # Ctrl-C stops the whole pipeline
sleep 2
open scripts/dashboard.html 2>/dev/null || echo "open scripts/dashboard.html in your browser"
echo "dashboard live — press Ctrl-C to stop."
wait
