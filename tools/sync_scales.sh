#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Copy the scale files from a Sinfon checkout (github vikramrojo/maqam-dev) and record their commit and
# hashes in scales/UPSTREAM, then regenerate app/maqam/scales.{h,c}. The files are edited in Sinfon only.
#   tools/sync_scales.sh ~/Git/maqam-dev
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
src=${1:?usage: tools/sync_scales.sh <sinfon checkout>}
dir="$src/SketchKit/Sources/SketchCore/Scales"
[ -f "$dir/west.txt" ] || { echo "no $dir/west.txt" >&2; exit 1; }
if command -v sha256sum >/dev/null 2>&1; then sha() { sha256sum "$1" | cut -d' ' -f1; }
else sha() { shasum -a 256 "$1" | cut -d' ' -f1; }; fi
for f in west.txt arab.txt SOURCE.md; do cp "$dir/$f" "$root/scales/$f"; done
{
    echo "# Vendored from Sinfon (github vikramrojo/maqam-dev), SketchKit/Sources/SketchCore/Scales, by tools/sync_scales.sh."
    echo "# Do not edit these files here: change them in Sinfon and re-sync. tests/scales_check.py verifies the hashes."
    echo "commit $(git -C "$src" rev-parse HEAD)"
    for f in west.txt arab.txt SOURCE.md; do echo "sha256 $f $(sha "$root/scales/$f")"; done
} > "$root/scales/UPSTREAM"
python3 "$root/tools/gen_scales.py" "$root/app/maqam/scales.h"
echo "synced scales from $src ($(git -C "$src" rev-parse --short HEAD)); review the diff and run tools/check_scales.sh"
