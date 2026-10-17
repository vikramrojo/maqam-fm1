#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Fails if app/maqam/scales.h is not what tools/gen_scales.py generates from scales/*.txt,
# then runs the scale checks and error fixtures.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp "${TMPDIR:-/tmp}/scales.XXXXXX")
trap 'rm -f "$tmp"' EXIT HUP INT TERM
python3 "$root/tools/gen_scales.py" "$tmp"
if ! diff -u "$root/app/maqam/scales.h" "$tmp"; then
    echo "app/maqam/scales.h is out of date: run python3 tools/gen_scales.py app/maqam/scales.h" >&2
    exit 1
fi
python3 "$root/tests/scales_check.py"
