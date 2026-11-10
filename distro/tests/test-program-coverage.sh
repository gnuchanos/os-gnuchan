#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DISTRO=$(CDPATH= cd -- "$HERE/.." && pwd)
PROGRAMS_FILE="$DISTRO/GnuChanPrograms.sh"
BUILD_FILE="$DISTRO/build.sh"
STAGE_FILE="$DISTRO/stage-program-checkouts.sh"
CLEANUP_HOOK="$DISTRO/config/hooks/normal/9990-cleanup.hook.chroot"

expected="
GnuChanGUI
GnuChanTerm
GnuChanFetch
GnuChanRunner
GnuChanWifi
GnuChanNetworkManager
GnuChanBluetooth
GnuChanDock
GnuChanBrowser
GnuChanSS
GnuChanLockScreen
GnuChanNotification
GnuChanScreenShoot
GnuChanSoftwareCenter
GnuChanImageViewer
GnuChanTop
GnuChanSettings
GnuChanWelcome
GnuChanSound
GnuChanSoundRecord
GnuChanDesktop
GnuChanFileManager
GCL
GnuChanWM
GnuChanDM
"

actual=$(sh "$PROGRAMS_FILE" --list)
expected=$(printf '%s\n' "$expected" | sed '/^$/d')
[ "$actual" = "$expected" ] || {
    printf 'canonical program list differs from expected:\n%s\n' "$actual" >&2
    exit 1
}

for program in $expected; do
    grep -Fq "$program" "$BUILD_FILE" || {
        printf 'build fail-early list omits %s\n' "$program" >&2
        exit 1
    }
    grep -Fq "$program" "$STAGE_FILE" || {
        printf 'local checkout staging list omits %s\n' "$program" >&2
        exit 1
    }
done

grep -Fq 'python3 setup.py install --single-version-externally-managed' \
    "$PROGRAMS_FILE"
grep -A4 -F 'GnuChanSoundRecord)' "$PROGRAMS_FILE" |
    grep -Fq 'sh ./install.sh install' || {
    printf 'GnuChanSoundRecord is not installed through its installer\n' >&2
    exit 1
}
update_whitelist=$(sed -n '/case "$requested" in/,/;;/p' "$PROGRAMS_FILE")
for program in GnuChanBluetooth GnuChanImageViewer GnuChanSoundRecord; do
    printf '%s\n' "$update_whitelist" | grep -Fq "$program" || {
        printf 'update whitelist omits %s\n' "$program" >&2
        exit 1
    }
done
for program in GnuChanBluetooth GnuChanImageViewer GnuChanSoundRecord; do
    grep -Fq "/usr/local/bin/$program" "$CLEANUP_HOOK" || {
        printf 'image verification does not check canonical command %s\n' "$program" >&2
        exit 1
    }
done
grep -Fq 'unsquashfs -f -d "$TARGET" "$SQUASHFS"' \
    "$DISTRO/installer/gnuchanos-install.sh"
printf 'all requested Gnuchan projects are covered by the ISO program list\n'
