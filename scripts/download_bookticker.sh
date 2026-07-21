#!/usr/bin/env bash
# grab N consecutive days of Binance USD-M futures BTCUSDT bookTicker zips.
# keeps the ~300MB zips (we stream them later with `unzip -p | ./bookticker`),
# never unzips to disk — a full day is ~2.5GB and we don't have the room.
# checksum-verified against the .CHECKSUM sibling, and resumable: a day that's
# already there and passes its checksum is skipped.
#
# usage: scripts/download_bookticker.sh [START_DATE] [DAYS]
# default is the 45-day block that ends on the last available day (2024-03-30).
# note: bookTicker daily was discontinued after 2024-03-30; range is 2023-05-16..2024-03-30.

set -euo pipefail

START="${1:-2024-02-15}"
DAYS="${2:-45}"
SYM="BTCUSDT"
BASE="https://data.binance.vision/data/futures/um/daily/bookTicker/$SYM"
OUT="data/bookticker"

mkdir -p "$OUT"

# macOS `date -j` walks dates; -v+${i}d adds days to START
for ((i = 0; i < DAYS; i++)); do
    day=$(date -j -v+"${i}"d -f "%Y-%m-%d" "$START" "+%Y-%m-%d")
    zip="$SYM-bookTicker-$day.zip"
    dst="$OUT/$zip"

    # already have a good copy? verify and skip
    if [[ -f "$dst" ]]; then
        if (cd "$OUT" && curl -s "$BASE/$zip.CHECKSUM" | shasum -a 256 -c --status); then
            echo "ok    $day (cached)"
            continue
        fi
        echo "stale $day, re-downloading"
    fi

    echo "get   $day"
    if ! curl -fs -o "$dst" "$BASE/$zip"; then
        echo "MISS  $day (no file at source — outside available range?)" >&2
        rm -f "$dst"
        continue
    fi

    # verify against the published sha256; drop the file if it doesn't match
    if ! (cd "$OUT" && curl -s "$BASE/$zip.CHECKSUM" | shasum -a 256 -c --status); then
        echo "BAD   $day checksum mismatch, discarding" >&2
        rm -f "$dst"
        exit 1
    fi
done

echo "---"
echo "have $(ls "$OUT"/*.zip 2>/dev/null | wc -l | tr -d ' ') zips, $(du -sh "$OUT" | cut -f1) total in $OUT"
