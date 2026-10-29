#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Fails if app/maqam/scales.h or scales.c is not what tools/gen_scales.py generates from scales/*.txt,
# then runs the scale checks, error fixtures and the vendored-hash check (scales/UPSTREAM).
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/scales.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
python3 "$root/tools/gen_scales.py" "$tmp/scales.h"
for f in scales.h scales.c; do
    if ! diff -u "$root/app/maqam/$f" "$tmp/$f"; then
        echo "app/maqam/$f is out of date: run python3 tools/gen_scales.py app/maqam/scales.h" >&2
        exit 1
    fi
done
python3 "$root/tests/scales_check.py"
