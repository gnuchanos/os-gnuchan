#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    printf 'usage: %s OS_ROOT REPOSITORIES_DIR\n' "$0" >&2
    exit 2
fi

OS_ROOT=$1
REPOSITORIES_DIR=$2

for PROGRAM in GnuChanSound GnuChanDesktop GnuChanFileManager GnuChanWelcome; do
    CHECKOUT="$OS_ROOT/../$PROGRAM"
    if [ -d "$CHECKOUT" ] && [ ! -d "$REPOSITORIES_DIR/$PROGRAM" ]; then
        mkdir -p "$REPOSITORIES_DIR"
        cp -a "$CHECKOUT" "$REPOSITORIES_DIR/$PROGRAM"
        rm -rf "$REPOSITORIES_DIR/$PROGRAM/.git"
        printf '==> staged local %s source from %s\n' "$PROGRAM" "$CHECKOUT"
    fi
done
