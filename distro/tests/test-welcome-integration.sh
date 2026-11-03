#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DISTRO=$(CDPATH= cd -- "$HERE/.." && pwd)
OS_ROOT=$(CDPATH= cd -- "$DISTRO/.." && pwd)
CHECKOUT="$OS_ROOT/../GnuChanWelcome"
WORK="$HERE/.welcome-integration-work"

if [ -e "$WORK" ]; then
    printf 'test work path already exists; refusing to overwrite: %s\n' "$WORK" >&2
    exit 1
fi
[ -f "$CHECKOUT/install.sh" ] || {
    printf 'GnuChanWelcome sibling checkout not found: %s\n' "$CHECKOUT" >&2
    exit 1
}

mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

STAGE="$WORK/opt/gnuchanos"
DESTDIR="$WORK/root"
sh "$DISTRO/stage-program-checkouts.sh" "$OS_ROOT" "$STAGE/repos"
REPO="$STAGE/repos/GnuChanWelcome"
[ -f "$REPO/gnuchan_welcome.py" ]
[ ! -e "$REPO/.git" ]

(
    cd "$REPO"
    DESTDIR="$DESTDIR" sh ./install.sh
)

LAUNCHER="$DESTDIR/usr/local/bin/gnuchan-welcome"
DESKTOP="$DESTDIR/usr/local/share/applications/gnuchan-welcome.desktop"
AUTOSTART="$DESTDIR/etc/xdg/autostart/gnuchan-welcome.desktop"
[ -x "$LAUNCHER" ]
[ -f "$DESKTOP" ]
[ -f "$AUTOSTART" ]
grep -Fxq 'Exec=gnuchan-welcome' "$DESKTOP"
grep -Fxq 'Exec=gnuchan-welcome --first-run' "$AUTOSTART"
python3 "$LAUNCHER" --version
python3 "$LAUNCHER" --check-config --config "$REPO/examples/welcome.conf"

grep -Fq 'GnuChanWelcome' "$DISTRO/GnuChanPrograms.sh"
grep -Fq 'GnuChanWelcome' "$DISTRO/stage-program-checkouts.sh"
grep -Fq 'gnuchan-welcome --first-run' \
    "$DISTRO/config/hooks/normal/0300-gnuchan-programs.hook.chroot"
grep -Fq 'FirstRunComplete = false' \
    "$DISTRO/config/hooks/normal/0300-gnuchan-programs.hook.chroot"
grep -Fq 'unsquashfs -f -d "$TARGET" "$SQUASHFS"' \
    "$DISTRO/installer/gnuchanos-install.sh"
printf 'GnuChanWelcome ISO and installer integration tests passed\n'
