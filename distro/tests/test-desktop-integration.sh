#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DISTRO=$(CDPATH= cd -- "$HERE/.." && pwd)
OS_ROOT=$(CDPATH= cd -- "$DISTRO/.." && pwd)
CHECKOUT="$OS_ROOT/../GnuChanDesktop"
WORK="$HERE/.desktop-integration-work"

if [ -e "$WORK" ]; then
    printf 'test work path already exists; refusing to overwrite: %s\n' "$WORK" >&2
    exit 1
fi
if [ ! -d "$CHECKOUT" ]; then
    printf 'GnuChanDesktop sibling checkout not found: %s\n' "$CHECKOUT" >&2
    exit 1
fi

mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

STAGE="$WORK/opt/gnuchanos"
DESTDIR="$WORK/root"
sh "$DISTRO/stage-program-checkouts.sh" "$OS_ROOT" "$STAGE/repos"

[ -f "$STAGE/repos/GnuChanDesktop/install.sh" ]
[ -f "$STAGE/repos/GnuChanDesktop/gnuchan_desktop_panel.py" ]
[ ! -e "$STAGE/repos/GnuChanDesktop/.git" ]

(
    cd "$STAGE/repos/GnuChanDesktop"
    DESTDIR="$DESTDIR" sh ./install.sh
)

LAUNCHER="$DESTDIR/usr/local/bin/gnuchan-desktop-panel"
LEGACY_COMMAND="$DESTDIR/usr/local/bin/GnuChanDesktop"
DESKTOP_ENTRY="$DESTDIR/usr/share/applications/gnuchan-desktop-panel.desktop"
[ -x "$LAUNCHER" ]
[ -x "$LEGACY_COMMAND" ]
[ "$(readlink "$LEGACY_COMMAND")" = "gnuchan-desktop-panel" ]
[ -f "$DESKTOP_ENTRY" ]
grep -Fxq 'Type=Application' "$DESKTOP_ENTRY"
grep -Fxq 'Name=GnuChan Desktop' "$DESKTOP_ENTRY"
grep -Fxq 'Exec=/usr/local/bin/GnuChanDesktop' "$DESKTOP_ENTRY"
grep -Fxq 'TryExec=/usr/local/bin/GnuChanDesktop' "$DESKTOP_ENTRY"
grep -Fxq 'Categories=Settings;DesktopSettings;' "$DESKTOP_ENTRY"
grep -Fxq 'Terminal=false' "$DESKTOP_ENTRY"

PATH="$DESTDIR/usr/local/bin:$PATH" gnuchan-desktop-panel --version
PATH="$DESTDIR/usr/local/bin:$PATH" GnuChanDesktop --version
grep -Fq 'python3-tk' "$DISTRO/config/package-lists/GnuchanOS.list.chroot"
grep -Fq 'feh' "$DISTRO/config/package-lists/GnuchanOS.list.chroot"
grep -Fq 'x11-xserver-utils' "$DISTRO/config/package-lists/GnuchanOS.list.chroot"
grep -Fq 'unsquashfs -f -d "$TARGET" "$SQUASHFS"' \
    "$DISTRO/installer/gnuchanos-install.sh"
grep -Fq '[ -x /usr/local/bin/GnuChanDesktop ]' \
    "$DISTRO/config/hooks/normal/9990-cleanup.hook.chroot"
grep -Fq '[ -f /usr/share/applications/gnuchan-desktop-panel.desktop ]' \
    "$DISTRO/config/hooks/normal/9990-cleanup.hook.chroot"
grep -Fq 'GnuChanWelcome' "$DISTRO/GnuChanPrograms.sh"
grep -Fq 'GnuChanWelcome' "$DISTRO/stage-program-checkouts.sh"
printf 'GnuChanDesktop staging and desktop-entry integration tests passed\n'
