#!/bin/sh
set -eu

PREFIX=${PREFIX:-/usr/local}
SYSCONFDIR=${SYSCONFDIR:-/etc}
DESTDIR=${DESTDIR:-}
BIN_DIR="$DESTDIR$PREFIX/bin"
APP_DIR="$DESTDIR$PREFIX/share/applications"
AUTOSTART_DIR="$DESTDIR$SYSCONFDIR/xdg/autostart"

rm -f "$BIN_DIR/gnuchan-welcome" \
    "$APP_DIR/gnuchan-welcome.desktop" \
    "$AUTOSTART_DIR/gnuchan-welcome.desktop"
echo "Removed GnuChanWelcome program and desktop entries."
