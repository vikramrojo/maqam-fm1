#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
"""Write the original M-VAVE FM1 V15 factory DX7 bank as four 32-voice .syx files.

    python3 tools/dx7_factory.py [--out build/banks]

The fork's device image leaves the bank out to fit the firmware limit (an
unsaved library bank plays INIT VOICE there). These files restore the stock
V14 sounds: import bank-1 .. bank-4 into library banks 1-4 through the
workbench. The bank is decoded from app/fm_factory.inc (Apache-2.0, see
THIRD_PARTY.md) and checked against the VMEM SHA-256 in its header.
"""
import argparse, hashlib, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def decode(packed):
    # The format app/fm_factory.c expands: a byte below 128 is a literal; 128+n copies
    # n+3 bytes from `distance` back, with the distance's high bits in a third byte.
    out, at = bytearray(), 0
    while at < len(packed):
        token = packed[at]; at += 1
        if token < 128:
            out.append(token)
            continue
        low = packed[at]; at += 1
        distance = low & 127
        if low & 128:
            distance |= packed[at] << 7; at += 1
        for _ in range(token - 128 + 3):
            out.append(out[-distance])
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--out", default=os.path.join(ROOT, "build", "banks"))
    ap.add_argument("--inc", default=os.path.join(ROOT, "app", "fm_factory.inc"))
    a = ap.parse_args()
    text = open(a.inc, encoding="utf-8").read()
    want = re.search(r"VMEM SHA-256: ([0-9a-f]{64})", text)
    body = re.search(r"factory_packed\[\]\s*=\s*\{(.*?)\};", text, re.S)
    if not want or not body:
        sys.exit(f"error: {a.inc}: no SHA-256 header or factory_packed table")
    vmem = decode([int(x) for x in re.findall(r"\d+", body.group(1))])
    if len(vmem) != 16384 or hashlib.sha256(vmem).hexdigest() != want.group(1):
        sys.exit(f"error: decoded bank is {len(vmem)} bytes or does not match the header SHA-256")
    os.makedirs(a.out, exist_ok=True)
    for bank in range(4):
        data = vmem[bank * 4096:(bank + 1) * 4096]
        if any(b > 127 for b in data):
            sys.exit(f"error: bank {bank + 1} has bytes above 0x7f")
        path = os.path.join(a.out, f"fm1-v15-factory-bank-{bank + 1}.syx")
        with open(path, "wb") as f:
            f.write(bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) + data + bytes([(-sum(data)) & 0x7F, 0xF7]))
        names = [data[v * 128 + 118:v * 128 + 128].decode("ascii", "replace").rstrip() for v in (0, 31)]
        print(f"wrote {path} ({names[0]} .. {names[1]})")


if __name__ == "__main__":
    main()
