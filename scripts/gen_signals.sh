#!/usr/bin/env bash
# turn every downloaded bookTicker zip into a 100ms signals CSV, by streaming it
# through the bookticker driver (never unzips a 2.5GB day to disk). resumable:
# skips a day whose output already exists and is non-empty.
#
# usage: scripts/gen_signals.sh
# needs build/bookticker (clang++ -std=c++20 -O2 -Isrc src/bookticker_main.cpp src/signals.cpp -o build/bookticker)

set -euo pipefail

IN="data/bookticker"
OUT="data/signals"
BIN="build/bookticker"

[[ -x "$BIN" ]] || { echo "build $BIN first" >&2; exit 1; }
mkdir -p "$OUT"

for zip in "$IN"/*.zip; do
    day=$(basename "$zip" .zip | sed 's/.*bookTicker-//')   # YYYY-MM-DD
    dst="$OUT/$day.csv"

    if [[ -s "$dst" ]]; then
        echo "skip $day ($(wc -l < "$dst" | tr -d ' ') rows)"
        continue
    fi

    echo "gen  $day"
    unzip -p "$zip" | "$BIN" > "$dst"
done

echo "---"
echo "$(ls "$OUT"/*.csv 2>/dev/null | wc -l | tr -d ' ') signal files, $(du -sh "$OUT" 2>/dev/null | cut -f1) in $OUT"
