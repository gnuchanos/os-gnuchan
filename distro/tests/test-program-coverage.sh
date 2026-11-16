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
GnuChanDiskManager
GCL
GnuChanWM
GnuChanPlymouth
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
    grep -Fq 'python3 ./src/makefile.py install' || {
    printf 'GnuChanSoundRecord is not installed through src/makefile.py\n' >&2
    exit 1
}
update_whitelist=$(sed -n '/case "$requested" in/,/;;/p' "$PROGRAMS_FILE")
for program in GnuChanBluetooth GnuChanImageViewer GnuChanSoundRecord \
    GnuChanGUI GnuChanPlymouth; do
    printf '%s\n' "$update_whitelist" | grep -Fq "$program" || {
        printf 'update whitelist omits %s\n' "$program" >&2
        exit 1
    }
done
for program in $expected; do
    printf '%s\n' "$update_whitelist" | grep -Fq "$program" || {
        printf 'update whitelist omits canonical program %s\n' "$program" >&2
        exit 1
    }
done
grep -Fq '/usr/local/share/gnuchanos/versions' "$PROGRAMS_FILE" || {
    printf 'program updater does not persist installed versions\n' >&2
    exit 1
}
for program in GnuChanBluetooth GnuChanImageViewer GnuChanSoundRecord; do
    grep -Fq "/usr/local/bin/$program" "$CLEANUP_HOOK" || {
        printf 'image verification does not check canonical command %s\n' "$program" >&2
        exit 1
    }
done
grep -Fq 'unsquashfs -f -d "$TARGET" "$SQUASHFS"' \
    "$DISTRO/installer/gnuchanos-install.sh"
for live_only in \
    /usr/local/bin/gnuchanos-installer \
    /usr/local/bin/GnuChanInstall \
    /usr/local/sbin/gnuchanos-install \
    /usr/local/sbin/gnuchanos-gpu-kernel-args.sh \
    /usr/share/applications/gnuchanos-installer.desktop \
    /usr/share/pixmaps/gnuchanos/logo.png; do
    cleanup_path="\"\$TARGET$live_only\""
    grep -Fq "$cleanup_path" "$DISTRO/installer/gnuchanos-install.sh" || {
        printf 'installer cleanup does not remove live-only path %s\n' \
            "$live_only" >&2
        exit 1
    }
done
grep -Fq '"$TARGET/etc/xdg/autostart/gnuchanos-installer.desktop"' \
"$DISTRO/installer/gnuchanos-install.sh"
grep -Fq '[ -x /usr/local/bin/GnuChanPlymouth ]' "$CLEANUP_HOOK"
printf 'all requested Gnuchan projects are covered by the ISO program list\n'
