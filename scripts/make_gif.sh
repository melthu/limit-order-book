#!/usr/bin/env bash
# turn a screen recording into a small, looping gif for the README.
# record the dashboard with Cmd-Shift-5 (save a .mov), then:
#   scripts/make_gif.sh ~/Desktop/recording.mov docs/demo.gif
# two-pass palette keeps it sharp and reasonably small.
set -eu
IN="${1:?usage: make_gif.sh <input.mov> [out.gif]}"
OUT="${2:-docs/demo.gif}"
FPS="${FPS:-12}"      # 12fps reads fine and keeps the file small
W="${W:-1100}"        # output width in px
mkdir -p "$(dirname "$OUT")"
pal="$(mktemp -t lobpal).png"
ffmpeg -y -i "$IN" -vf "fps=$FPS,scale=$W:-1:flags=lanczos,palettegen=stats_mode=diff" "$pal"
ffmpeg -y -i "$IN" -i "$pal" \
  -lavfi "fps=$FPS,scale=$W:-1:flags=lanczos[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=3" "$OUT"
echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
