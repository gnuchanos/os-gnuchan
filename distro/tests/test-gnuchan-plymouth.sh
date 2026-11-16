#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
DISTRO=$(CDPATH= cd -- "$HERE/.." && pwd)
OS_ROOT=$(CDPATH= cd -- "$DISTRO/.." && pwd)
CHECKOUT="$OS_ROOT/../GnuChanPlymouth"
if [ ! -f "$CHECKOUT/makefile.py" ]; then
    printf 'GnuChanPlymouth checkout not found: %s\n' "$CHECKOUT" >&2
    exit 1
fi

WORK=$(mktemp -d "$HERE/.gnuchan-plymouth-work.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM
ROOT="$WORK/root"
SYSFS="$WORK/sys"
mkdir -p "$SYSFS/bus/pci/devices"
: > "$WORK/cmdline"

DESTDIR="$ROOT" \
GNUCHAN_PLYMOUTH_BUILD="$WORK/build" \
GNUCHANOS_ASSETS="$OS_ROOT/assets" \
    python3 "$CHECKOUT/makefile.py" install

[ -x "$ROOT/usr/local/bin/GnuChanPlymouth" ]
[ -f "$ROOT/usr/share/gnuchan-plymouth/background.png" ]
[ -f "$ROOT/usr/share/gnuchan-plymouth/logo.png" ]
[ -f "$ROOT/usr/lib/systemd/system/gnuchan-plymouth.service" ]
[ -f "$ROOT/etc/systemd/system/gnuchandm.service.d/gnuchan-plymouth.conf" ]
for gpu in intel amd nvidia; do
    profile="$ROOT/etc/gnuchan-plymouth/$gpu.conf"
    [ -f "$profile" ]
    grep -Fxq 'drm_device=/dev/dri/card0' "$profile"
    grep -Fxq 'duration_ms=6000' "$profile"
done

add_device() {
    address=$1
    vendor=$2
    boot_vga=$3
    device="$SYSFS/bus/pci/devices/$address"
    mkdir -p "$device"
    printf '%s\n' "$vendor" > "$device/vendor"
    printf '0x030000\n' > "$device/class"
    printf '%s\n' "$boot_vga" > "$device/boot_vga"
}

for gpu in intel amd nvidia; do
    case "$gpu" in
        intel) vendor=0x8086 ;;
        amd) vendor=0x1002 ;;
        nvidia) vendor=0x10de ;;
    esac
    add_device 0000:01:00.0 "$vendor" 1
    detected=$("$ROOT/usr/local/bin/GnuChanPlymouth" \
        --detect-gpu --sysfs-root "$SYSFS" \
        --proc-cmdline "$WORK/cmdline")
    [ "$detected" = "$gpu" ]
done

add_device 0000:00:02.0 0x8086 0
add_device 0000:01:00.0 0x10de 1
[ "$("$ROOT/usr/local/bin/GnuChanPlymouth" \
    --detect-gpu --sysfs-root "$SYSFS" \
    --proc-cmdline "$WORK/cmdline")" = nvidia ]
printf 'BOOT_IMAGE=/vmlinuz gnuchan.gpu=amd\n' > "$WORK/cmdline"
[ "$("$ROOT/usr/local/bin/GnuChanPlymouth" \
    --detect-gpu --sysfs-root "$SYSFS" \
    --proc-cmdline "$WORK/cmdline")" = amd ]

grep -Fq 'After=systemd-modules-load.service gnuchan-gpu-select.service' \
    "$ROOT/usr/lib/systemd/system/gnuchan-plymouth.service"
grep -Fxq 'After=gnuchan-plymouth.service' \
    "$ROOT/etc/systemd/system/gnuchandm.service.d/gnuchan-plymouth.conf"
grep -Fxq 'Wants=gnuchan-plymouth.service' \
    "$ROOT/etc/systemd/system/gnuchandm.service.d/gnuchan-plymouth.conf"
grep -q 'quiet nosplash' "$DISTRO/installer/gnuchanos-gpu-kernel-args.sh"
grep -Fxq 'libdrm-dev' \
    "$DISTRO/config/package-lists/GnuchanOS.list.chroot"
for firmware in firmware-amd-graphics firmware-intel-graphics \
                firmware-nvidia-graphics; do
    grep -Fxq "$firmware" "$DISTRO/config/package-lists/GnuchanOS.list.chroot"
done
! grep -q '^plymouth\\(-themes\\)\\?$' \
    "$DISTRO/config/package-lists/GnuchanOS.list.chroot"
test "$(grep -c 'drmModeSetCrtc(' "$CHECKOUT/src/gnuchan_plymouth.c")" -eq 2
grep -Fq '__sync_synchronize();' "$CHECKOUT/src/gnuchan_plymouth.c"
printf 'GnuChanPlymouth build, GPU detection, and ISO install tests passed\n'
