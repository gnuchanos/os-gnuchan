#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PREFIX=${PREFIX:-/usr/local}
SYSCONFDIR=${SYSCONFDIR:-/etc}
DESTDIR=${DESTDIR:-}
BIN_DIR="$DESTDIR$PREFIX/bin"
APP_DIR="$DESTDIR$PREFIX/share/applications"
AUTOSTART_DIR="$DESTDIR$SYSCONFDIR/xdg/autostart"

mkdir -p "$BIN_DIR" "$APP_DIR" "$AUTOSTART_DIR"
install -m 755 "$ROOT/gnuchan_welcome.py" "$BIN_DIR/gnuchan-welcome"
install -m 644 "$ROOT/gnuchan-welcome.desktop" "$APP_DIR/gnuchan-welcome.desktop"
install -m 644 "$ROOT/autostart/gnuchan-welcome.desktop" \
    "$AUTOSTART_DIR/gnuchan-welcome.desktop"

echo "Installed GnuChanWelcome:"
echo "  Program:   $BIN_DIR/gnuchan-welcome"
echo "  Menu:      $APP_DIR/gnuchan-welcome.desktop"
echo "  Autostart: $AUTOSTART_DIR/gnuchan-welcome.desktop"
echo "Runtime requirement: Python 3 with Tk (python3-tk)."
