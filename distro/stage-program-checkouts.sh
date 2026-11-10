#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    printf 'usage: %s OS_ROOT REPOSITORIES_DIR\n' "$0" >&2
    exit 2
fi

OS_ROOT=$1
REPOSITORIES_DIR=$2

is_buildable_checkout() {
    checkout=$1
    [ -f "$checkout/src/makefile.py" ] ||
        [ -f "$checkout/makefile.py" ] ||
        [ -f "$checkout/install.sh" ] ||
        [ -f "$checkout/scripts/install.sh" ] ||
        [ -f "$checkout/setup.py" ] ||
        [ -f "$checkout/Makefile" ] ||
        [ -f "$checkout/CMakeLists.txt" ]
}

for PROGRAM in \
    GnuChanGUI GnuChanBrowser GnuChanDesktop GnuChanDM GnuChanDock \
    GnuChanFetch GnuChanFileManager GnuChanImageViewer GnuChanLockScreen \
    GnuChanBluetooth GnuChanNetworkManager GnuChanNotification \
    GnuChanRunner GnuChanScreenShoot GnuChanSettings \
    GnuChanSoftwareCenter GnuChanSound GnuChanSoundRecord GnuChanSS \
    GnuChanTerm GnuChanTop GnuChanWelcome GnuChanWifi GnuChanWM GCL; do
    CHECKOUT_NAME=$PROGRAM
    [ "$PROGRAM" != GCL ] || CHECKOUT_NAME=gcl
    CHECKOUT="$OS_ROOT/../$CHECKOUT_NAME"
    if [ -d "$CHECKOUT" ] && is_buildable_checkout "$CHECKOUT" &&
       [ ! -d "$REPOSITORIES_DIR/$PROGRAM" ]; then
        mkdir -p "$REPOSITORIES_DIR"
        cp -a "$CHECKOUT" "$REPOSITORIES_DIR/$PROGRAM"
        rm -rf "$REPOSITORIES_DIR/$PROGRAM/.git"
        printf '==> staged local %s source from %s\n' "$PROGRAM" "$CHECKOUT"
    fi
done
