#!/data/data/com.termux/files/usr/bin/bash
# termux-sigsys-shield installer — run this inside Termux.
set -eu

SRC="$(cd "$(dirname "$0")" && pwd)"

echo "==> 1/4 clang"
command -v clang >/dev/null 2>&1 || pkg install -y clang

echo "==> 2/4 build sigsys-shield"
clang -O2 -Wall -o "$PREFIX/bin/sigsys-shield" "$SRC/sigsys-shield.c"

echo "==> 3/4 self test"
OUT="$("$PREFIX/bin/sigsys-shield" "$PREFIX/bin/sh" -c 'echo SHIELD-OK')"
[ "$OUT" = "SHIELD-OK" ] && echo "    $OUT" || { echo "!! self test failed"; exit 1; }

echo "==> 4/4 opencode wrapper (if opencode is installed)"
if command -v opencode >/dev/null 2>&1; then
    OC="$(command -v opencode)"
    printf '#!/data/data/com.termux/files/usr/bin/bash\nexec sigsys-shield %s "$@"\n' "$OC" > "$PREFIX/bin/oc"
    chmod +x "$PREFIX/bin/oc"
    echo "    created \$PREFIX/bin/oc -> sigsys-shield opencode"
    "$PREFIX/bin/oc" --version || true
else
    echo "    opencode not found, skipped"
fi

echo
echo "done. usage:"
echo "  sigsys-shield <command> [args...]   # run any program under the shield"
echo "  oc                                  # opencode shortcut (if created above)"
