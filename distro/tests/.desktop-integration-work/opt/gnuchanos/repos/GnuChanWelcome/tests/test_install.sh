#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
STAGE="$ROOT/.test-install-root"

if [ -e "$STAGE" ]; then
    echo "Refusing to overwrite existing test staging directory: $STAGE" >&2
    exit 1
fi

cleanup() {
    rm -rf "$STAGE"
}
trap cleanup EXIT HUP INT TERM

DESTDIR="$STAGE" PREFIX=/usr SYSCONFDIR=/etc sh "$ROOT/install.sh"
DESTDIR="$STAGE" PREFIX=/usr SYSCONFDIR=/etc sh "$ROOT/install.sh"

PROGRAM="$STAGE/usr/bin/gnuchan-welcome"
MENU_ENTRY="$STAGE/usr/share/applications/gnuchan-welcome.desktop"
AUTOSTART_ENTRY="$STAGE/etc/xdg/autostart/gnuchan-welcome.desktop"
test -x "$PROGRAM"
test -f "$MENU_ENTRY"
test -f "$AUTOSTART_ENTRY"
python3 "$PROGRAM" --version

STATE="$STAGE/welcome.conf"
printf 'FirstRunComplete = true\n' > "$STATE"
python3 "$PROGRAM" --first-run --config "$STATE"
python3 "$PROGRAM" --first-run --config "$STATE"

DESTDIR="$STAGE" PREFIX=/usr SYSCONFDIR=/etc sh "$ROOT/uninstall.sh"
DESTDIR="$STAGE" PREFIX=/usr SYSCONFDIR=/etc sh "$ROOT/uninstall.sh"
test ! -e "$PROGRAM"
test ! -e "$MENU_ENTRY"
test ! -e "$AUTOSTART_ENTRY"

echo "Staged install, uninstall, and completed first-run checks passed."
