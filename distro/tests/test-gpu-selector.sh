#!/bin/sh
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK=$HERE/.gpu-selector-work
if [ -e "$WORK" ]; then
    printf 'test work path already exists; refusing to overwrite: %s\n' "$WORK" >&2
    exit 1
fi
mkdir -p "$WORK"
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

ROOT=$WORK/root
BIN=$WORK/bin
POOL=$WORK/pool
mkdir -p "$ROOT/proc" "$ROOT/sys/bus/pci/devices" "$BIN" \
    "$POOL/common" "$POOL/nvidia-legacy"

cat > "$BIN/apt-get" <<'MOCK'
#!/bin/sh
printf '%s\n' "$*" >> "$GPU_TEST_WORK/apt.log"
[ "${GPU_TEST_FAIL_APT:-0}" = 1 ] && exit 1
exit 0
MOCK
cat > "$BIN/modprobe" <<'MOCK'
#!/bin/sh
printf '%s\n' "$*" >> "$GPU_TEST_WORK/modprobe.log"
if [ "$1" = nvidia ] && [ -f "$GPU_TEST_WORK/fail-nvidia" ]; then
    exit 1
fi
exit 0
MOCK
cat > "$BIN/depmod" <<'MOCK'
#!/bin/sh
exit 0
MOCK
cat > "$BIN/readlink" <<'MOCK'
#!/bin/sh
case "$1" in
    */sys/bus/pci/devices/*/driver)
        if [ "${GPU_TEST_BOUND:-0}" = 1 ]; then
            printf '/sys/bus/pci/drivers/nvidia\n'
            exit 0
        fi
        ;;
esac
exit 1
MOCK
chmod +x "$BIN/"*

export GPU_TEST_WORK=$WORK
export GNUCHAN_GPU_SYSROOT=$ROOT
export GNUCHAN_GPU_POOL=$POOL
export PATH=$BIN:$PATH

SELECTOR=$HERE/../config/hooks/normal/gnuchan-gpu-select.sh

# Failed NVIDIA activation must report failure, attempt the open driver, and
# avoid persisting a blacklist. The selected legacy package name is exact.
printf 'BOOT_IMAGE=/vmlinuz gnuchan.gpu=nvidia-legacy modprobe.blacklist=nouveau\n' \
    > "$ROOT/proc/cmdline"
touch "$WORK/fail-nvidia"
export GPU_TEST_FAIL_APT=1
if sh "$SELECTOR" > "$WORK/failure.log" 2>&1; then
    printf 'failed NVIDIA module activation unexpectedly succeeded\n' >&2
    exit 1
fi
grep -q 'nvidia-legacy-470xx-driver)' "$WORK/failure.log"
grep -q 'ERROR: nvidia-legacy-470xx-driver installation failed' \
    "$WORK/failure.log"
[ ! -e "$ROOT/etc/modprobe.d/gnuchan-nouveau-off.conf" ]

# With repositories available, the fallback uses the actual legacy package
# name rather than the nonexistent generic nvidia-legacy-driver name.
unset GPU_TEST_FAIL_APT
: > "$WORK/apt.log"
if sh "$SELECTOR" > "$WORK/module-failure.log" 2>&1; then
    printf 'failed NVIDIA module activation unexpectedly succeeded\n' >&2
    exit 1
fi
grep -q 'install -y --no-install-recommends nvidia-legacy-470xx-driver' \
    "$WORK/apt.log"
grep -q 'ERROR: the selected package is present but its nvidia module did not load' \
    "$WORK/module-failure.log"
grep -q '^nouveau$' "$WORK/modprobe.log"
[ ! -e "$ROOT/etc/modprobe.d/gnuchan-nouveau-off.conf" ]

# Successful activation only reports active after the generic NVIDIA PCI
# device is actually bound to the nvidia driver.
rm -f "$WORK/fail-nvidia"
touch "$POOL/nvidia-legacy/nvidia-legacy-470xx-driver.deb"
DEVICE=$ROOT/sys/bus/pci/devices/0000:01:00.0
mkdir -p "$DEVICE" "$ROOT/sys/bus/pci/drivers/nvidia"
printf '0x10de\n' > "$DEVICE/vendor"
export GPU_TEST_BOUND=1
if ! sh "$SELECTOR" > "$WORK/success.log" 2>&1; then
    cat "$WORK/success.log" >&2
    exit 1
fi
grep -q 'NVIDIA driver is active for this boot' "$WORK/success.log"
grep -q '^blacklist nouveau$' "$ROOT/etc/modprobe.d/gnuchan-nouveau-off.conf"

# The generic entry does no package work and preserves the kernel's choice.
: > "$WORK/apt.log"
printf 'BOOT_IMAGE=/vmlinuz quiet\n' > "$ROOT/proc/cmdline"
sh "$SELECTOR" > "$WORK/generic.log" 2>&1
grep -q "no explicit driver selected" "$WORK/generic.log"
[ ! -s "$WORK/apt.log" ]

# An invalid selection is an explicit error, not a successful no-op.
printf 'gnuchan.gpu=made-up\n' > "$ROOT/proc/cmdline"
if sh "$SELECTOR" > "$WORK/unknown.log" 2>&1; then
    printf 'unknown GPU selection unexpectedly succeeded\n' >&2
    exit 1
fi
grep -q "ERROR: unknown gnuchan.gpu selection 'made-up'" "$WORK/unknown.log"

# Both boot menus receive the NVIDIA no-bind argument and remain idempotent.
mkdir -p "$WORK/iso/isolinux" "$WORK/iso/boot/grub"
cat > "$WORK/iso/isolinux/live.cfg" <<'MENU'
label live
  menu label Live System
  kernel /live/vmlinuz
  append initrd=/live/initrd.img boot=live
MENU
cat > "$WORK/iso/boot/grub/grub.cfg" <<'MENU'
menuentry "Live System" {
    linux /live/vmlinuz boot=live
    initrd /live/initrd.img
}
MENU
sh "$HERE/../config/hooks/normal/gpu_menu.sh" "$WORK/iso" \
    > "$WORK/menu.log"
grep -q 'gnuchan.gpu=nvidia-legacy modprobe.blacklist=nouveau' \
    "$WORK/iso/isolinux/live.cfg"
grep -q 'gnuchan.gpu=nvidia-new modprobe.blacklist=nouveau' \
    "$WORK/iso/boot/grub/grub.cfg"
before=$(wc -l < "$WORK/iso/isolinux/live.cfg")
grub_before=$(wc -l < "$WORK/iso/boot/grub/grub.cfg")
sh "$HERE/../config/hooks/normal/gpu_menu.sh" "$WORK/iso" \
    > "$WORK/menu-second.log"
after=$(wc -l < "$WORK/iso/isolinux/live.cfg")
grub_after=$(wc -l < "$WORK/iso/boot/grub/grub.cfg")
[ "$before" -eq "$after" ]
[ "$grub_before" -eq "$grub_after" ]

printf 'GPU selector and boot-menu tests passed\n'
