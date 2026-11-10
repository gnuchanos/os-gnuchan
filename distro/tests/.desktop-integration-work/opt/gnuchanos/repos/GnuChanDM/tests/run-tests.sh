#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build="$root/tests/.build"
mkdir -p "$build"
trap 'rm -rf "$build"' EXIT HUP INT TERM

cc -std=c11 -Wall -Wextra -Werror -I"$root/src" \
    "$root/tests/test_dm_form.c" "$root/src/dm_form.c" \
    -o "$build/test_dm_form"
"$build/test_dm_form"

cc -std=c11 -Wall -Wextra -Werror -I"$root/src" \
    "$root/tests/test_numlock.c" "$root/src/dm_numlock.c" -lX11 \
    -o "$build/test_numlock"
if command -v xvfb-run >/dev/null 2>&1; then
    xvfb-run -a "$build/test_numlock"
    cc -std=c11 -Wall -Wextra -Werror -I"$root/src" \
        "$root/tests/test_input_keys.c" "$root/src/dm_input.c" \
        "$root/src/dm_form.c" -lX11 -o "$build/test_input_keys"
    xvfb-run -a "$build/test_input_keys"
else
    echo "SKIP: X11 key and Num Lock tests require xvfb-run" >&2
fi
