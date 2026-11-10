#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_binary="$root/tests/test_notif_dbus_image"
trap 'rm -f "$test_binary"' EXIT

cc -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter \
   -D_DEFAULT_SOURCE $(pkg-config --cflags dbus-1) \
   "$root/tests/test_notif_dbus_image.c" \
   -o "$test_binary" $(pkg-config --libs dbus-1)
"$test_binary"
