#!/bin/sh
set -eu

PREFIX=${PREFIX:-"${HOME:?HOME must be set}/.local"}
DATA_HOME=${XDG_DATA_HOME:-"$HOME/.local/share"}

case "$PREFIX" in
    ""|"/")
        printf '%s\n' "Refusing unsafe PREFIX: $PREFIX" >&2
        exit 1
        ;;
esac

rm -f -- "$PREFIX/bin/GnuChanBluetooth" "$PREFIX/bin/gnuchan-bluetooth"
rm -f -- "$DATA_HOME/applications/gnuchan-bluetooth.desktop"
rm -rf -- "$PREFIX/lib/gnuchan-bluetooth"
printf 'Uninstalled GnuChanBluetooth from %s (user configuration was preserved)\n' "$PREFIX"
