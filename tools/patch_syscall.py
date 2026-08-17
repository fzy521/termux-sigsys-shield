#!/usr/bin/env python3
"""Patch inline aarch64 syscall sites in a binary to return -ENOSYS.

Replaces every 8-byte sequence

    movz w8, #NR ; svc #0        (the pattern Go emits for inline syscalls)

with

    movn w0, #38  ; nop          (x0 = -ENOSYS, syscall skipped)

so callers take their old-kernel fallback path. Useful when a binary
statically embeds a syscall number your vendor seccomp filter kills, and
you want it fixed without a wrapper process.

Usage:
    python3 patch_syscall.py BINARY NR [NR ...]

Backs up to BINARY.bak before writing.
"""
import shutil
import struct
import sys


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)

    path = sys.argv[1]
    nrs = [int(a, 0) for a in sys.argv[2:]]

    data = bytearray(open(path, "rb").read())
    total = 0

    for nr in nrs:
        movz_w8 = 0x52800000 | (nr << 5) | 8
        target = struct.pack("<I", movz_w8) + bytes.fromhex("010000d4")
        # movn w0,#37 -> w0 = -38 (-ENOSYS); then a nop
        rep = struct.pack("<I", 0x12800000 | (37 << 5)) + bytes.fromhex("1f2003d5")

        sites = []
        i = 0
        while True:
            j = data.find(target, i)
            if j < 0:
                break
            sites.append(j)
            data[j:j + 8] = rep
            i = j + 8

        suffix = f" at {[hex(s) for s in sites]}" if sites else ""
        print(f"nr={nr}: patched {len(sites)} site(s){suffix}")
        total += len(sites)

    if total:
        shutil.copy2(path, path + ".bak")
        open(path, "wb").write(data)
        print(f"done: {total} site(s), backup: {path}.bak")
    else:
        print("nothing to patch")


if __name__ == "__main__":
    main()
