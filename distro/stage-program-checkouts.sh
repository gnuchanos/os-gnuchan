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

checkout_names() {
    case "$1" in
        GCL) printf '%s\n' gcl ;;
        GnuChanBluetooth) printf '%s\n' gnuchanbluetooth GnuChanBluetooth ;;
        GnuChanRunner) printf '%s\n' gnuchanrunner GnuChanRunner ;;
        GnuChanSettings) printf '%s\n' gnuchansettings GnuChanSettings ;;
        GnuChanSound) printf '%s\n' gnuchansound Gnuchansound GnuChanSound ;;
        GnuChanTop) printf '%s\n' gnuchantop Gnuchantop GnuChanTop ;;
        *) printf '%s\n' "$1" ;;
    esac
}

for PROGRAM in \
    GnuChanGUI GnuChanTerm GnuChanFetch GnuChanRunner GnuChanWifi \
    GnuChanNetworkManager GnuChanBluetooth GnuChanDock GnuChanBrowser \
    GnuChanSS GnuChanLockScreen GnuChanNotification GnuChanScreenShoot \
    GnuChanSoftwareCenter GnuChanImageViewer GnuChanTop GnuChanSettings \
    GnuChanWelcome GnuChanSound GnuChanSoundRecord GnuChanDesktop \
    GnuChanFileManager GnuChanDiskManager GCL GnuChanWM GnuChanPlymouth \
    GnuChanDM; do
    [ -d "$REPOSITORIES_DIR/$PROGRAM" ] && continue
    for CHECKOUT_NAME in $(checkout_names "$PROGRAM"); do
        CHECKOUT="$OS_ROOT/../$CHECKOUT_NAME"
        if [ -d "$CHECKOUT" ] && is_buildable_checkout "$CHECKOUT"; then
            mkdir -p "$REPOSITORIES_DIR"
            cp -a "$CHECKOUT" "$REPOSITORIES_DIR/$PROGRAM"
            rm -rf "$REPOSITORIES_DIR/$PROGRAM/.git"
            printf '==> staged local %s source from %s\n' "$PROGRAM" "$CHECKOUT"
            break
        fi
    done
done
