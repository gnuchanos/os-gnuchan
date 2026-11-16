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
    "$POOL/common" "$POOL/nvidia-detect" \
    "$POOL/nvidia-tesla-535-driver" \
    "$POOL/nvidia-legacy-470xx-driver" \
    "$POOL/nvidia-legacy-390xx-driver" \
    "$POOL/nvidia-legacy-340xx-driver"

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
case " ${GPU_TEST_FAIL_MODULES:-} " in
    *" $1 "*) exit 1 ;;
esac
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
        if [ -n "${GPU_TEST_BOUND_DRIVER:-}" ]; then
            printf '/sys/bus/pci/drivers/%s\n' "$GPU_TEST_BOUND_DRIVER"
            exit 0
        fi
        if [ "${GPU_TEST_BOUND:-0}" = 1 ]; then
            printf '/sys/bus/pci/drivers/nvidia\n'
            exit 0
        fi
        ;;
esac
exit 1
MOCK
cat > "$BIN/nvidia-detect" <<'MOCK'
#!/bin/sh
printf '%s\n' "${GPU_TEST_NVIDIA_RECOMMENDATION:-nvidia-legacy-470xx-driver}"
MOCK
chmod +x "$BIN/"*

export GPU_TEST_WORK=$WORK
export GNUCHAN_GPU_SYSROOT=$ROOT
export GNUCHAN_GPU_POOL=$POOL
export GPU_TEST_NVIDIA_RECOMMENDATION='nvidia-legacy-470xx-driver'
export PATH=$BIN:$PATH

SELECTOR=$HERE/../config/hooks/normal/gnuchan-gpu-select.sh
. "$HERE/../installer/gnuchanos-gpu-kernel-args.sh"

[ "$(gnuchanos_gpu_grub_cmdline 'gnuchan.gpu=nvidia-legacy quiet')" = \
    'quiet nosplash gnuchan.gpu=nvidia-legacy' ]
[ "$(gnuchanos_gpu_grub_cmdline 'gnuchan.gpu=nvidia-new quiet')" = \
    'quiet nosplash gnuchan.gpu=nvidia-new modprobe.blacklist=nouveau' ]
[ "$(gnuchanos_gpu_grub_cmdline 'quiet gnuchan.gpu=amd')" = \
    'quiet nosplash gnuchan.gpu=amd' ]
[ "$(gnuchanos_gpu_grub_cmdline 'quiet gnuchan.gpu=intel')" = \
    'quiet nosplash gnuchan.gpu=intel' ]
[ "$(gnuchanos_gpu_grub_cmdline 'quiet gnuchan.gpu=unknown')" = \
    'quiet nosplash' ]
printf 'installed GPU kernel-argument tests passed\n'

# Failed NVIDIA activation must report failure, attempt the open driver, and
# avoid persisting a blacklist. The selected legacy package name is exact.
printf 'BOOT_IMAGE=/vmlinuz gnuchan.gpu=nvidia-legacy modprobe.blacklist=nouveau\n' \
    > "$ROOT/proc/cmdline"
touch "$WORK/fail-nvidia"
export GPU_TEST_FAIL_APT=1
if sh "$SELECTOR" > "$WORK/failure.log" 2>&1; then
    :
else
    cat "$WORK/failure.log" >&2
    exit 1
fi
grep -q 'nvidia-legacy-470xx-driver)' "$WORK/failure.log"
grep -q 'continuing with nouveau' \
    "$WORK/failure.log"
[ ! -e "$ROOT/etc/modprobe.d/gnuchan-nouveau-off.conf" ]

# With repositories available, the fallback uses the actual legacy package
# name rather than the nonexistent generic nvidia-legacy-driver name.
unset GPU_TEST_FAIL_APT
: > "$WORK/apt.log"
if sh "$SELECTOR" > "$WORK/module-failure.log" 2>&1; then
    :
else
    cat "$WORK/module-failure.log" >&2
    exit 1
fi
grep -q 'install -y --no-install-recommends nvidia-legacy-470xx-driver' \
    "$WORK/apt.log"
grep -q 'continuing with nouveau' \
    "$WORK/module-failure.log"
grep -q '^nouveau$' "$WORK/modprobe.log"
[ ! -e "$ROOT/etc/modprobe.d/gnuchan-nouveau-off.conf" ]

# Successful activation only reports active after the generic NVIDIA PCI
# device is actually bound to the nvidia driver.
rm -f "$WORK/fail-nvidia"
touch "$POOL/nvidia-legacy-470xx-driver/nvidia-legacy-470xx-driver.deb"
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

# The legacy menu delegates older generations to Debian's recommendation.
for package in nvidia-tesla-535-driver nvidia-legacy-390xx-driver \
               nvidia-legacy-340xx-driver; do
    mkdir -p "$POOL/$package"
    touch "$POOL/$package/$package.deb"
    export GPU_TEST_NVIDIA_RECOMMENDATION=$package
    if ! sh "$SELECTOR" > "$WORK/$package.log" 2>&1; then
        cat "$WORK/$package.log" >&2
        exit 1
    fi
    grep -q "selected NVIDIA driver family: $package ($package)" \
        "$WORK/$package.log"
    rm -f "$ROOT/etc/modprobe.d/gnuchan-nouveau-off.conf"
done
export GPU_TEST_NVIDIA_RECOMMENDATION='nvidia-legacy-470xx-driver'

# The generic entry does no package work and preserves the kernel's choice.
: > "$WORK/apt.log"
rm -f "$ROOT/sys/bus/pci/devices/0000:01:00.0/vendor"
rmdir "$ROOT/sys/bus/pci/devices/0000:01:00.0"
printf 'BOOT_IMAGE=/vmlinuz quiet\n' > "$ROOT/proc/cmdline"
sh "$SELECTOR" > "$WORK/generic.log" 2>&1
grep -q "no explicit driver selected" "$WORK/generic.log"
[ ! -s "$WORK/apt.log" ]

# Ignore non-display PCI devices and prefer the active boot VGA when selecting
# between hybrid graphics. A failed amdgpu load must fall back to radeon for
# older Radeon generations and AMD integrated GPUs.
INTEL_BRIDGE=$ROOT/sys/bus/pci/devices/0000:00:00.0
AMD_GPU=$ROOT/sys/bus/pci/devices/0000:01:00.0
mkdir -p "$INTEL_BRIDGE" "$AMD_GPU"
printf '0x8086\n' > "$INTEL_BRIDGE/vendor"
printf '0x060000\n' > "$INTEL_BRIDGE/class"
printf '0x1002\n' > "$AMD_GPU/vendor"
printf '0x030000\n' > "$AMD_GPU/class"
printf '1\n' > "$AMD_GPU/boot_vga"
printf 'BOOT_IMAGE=/vmlinuz quiet\n' > "$ROOT/proc/cmdline"
export GPU_TEST_BOUND_DRIVER=radeon
: > "$WORK/modprobe.log"
sh "$SELECTOR" > "$WORK/amd-fallback.log" 2>&1
grep -q 'auto-detected an AMD GPU' "$WORK/amd-fallback.log"
grep -q '^amdgpu$' "$WORK/modprobe.log"
grep -q '^radeon$' "$WORK/modprobe.log"
unset GPU_TEST_BOUND_DRIVER

export GPU_TEST_BOUND_DRIVER=amdgpu
: > "$WORK/modprobe.log"
sh "$SELECTOR" > "$WORK/amd-modern.log" 2>&1
grep -q '^amdgpu$' "$WORK/modprobe.log"
! grep -q '^radeon$' "$WORK/modprobe.log"
unset GPU_TEST_BOUND_DRIVER

# Modern Intel GPUs can use xe when i915 cannot bind; older Intel HD graphics
# continue to use i915 first.
rm -rf "$ROOT/sys/bus/pci/devices"
mkdir -p "$ROOT/sys/bus/pci/devices/0000:00:02.0"
printf '0x8086\n' > "$ROOT/sys/bus/pci/devices/0000:00:02.0/vendor"
printf '0x030000\n' > "$ROOT/sys/bus/pci/devices/0000:00:02.0/class"
printf '1\n' > "$ROOT/sys/bus/pci/devices/0000:00:02.0/boot_vga"
export GPU_TEST_FAIL_MODULES=i915
: > "$WORK/modprobe.log"
sh "$SELECTOR" > "$WORK/intel-fallback.log" 2>&1
grep -q '^i915$' "$WORK/modprobe.log"
grep -q '^xe$' "$WORK/modprobe.log"
unset GPU_TEST_FAIL_MODULES

# An unsupported old NVIDIA branch remains bootable with nouveau instead of
# making the GPU selector fail the live system.
rm -rf "$ROOT/sys/bus/pci/devices"
mkdir -p "$ROOT/sys/bus/pci/devices/0000:01:00.0"
printf '0x10de\n' > "$ROOT/sys/bus/pci/devices/0000:01:00.0/vendor"
printf '0x030000\n' > "$ROOT/sys/bus/pci/devices/0000:01:00.0/class"
printf '1\n' > "$ROOT/sys/bus/pci/devices/0000:01:00.0/boot_vga"
export GPU_TEST_NVIDIA_RECOMMENDATION='no supported package'
: > "$WORK/modprobe.log"
sh "$SELECTOR" > "$WORK/nvidia-open-fallback.log" 2>&1
grep -q 'continuing with nouveau' "$WORK/nvidia-open-fallback.log"
grep -q '^nouveau$' "$WORK/modprobe.log"
export GPU_TEST_NVIDIA_RECOMMENDATION='nvidia-legacy-470xx-driver'

# An invalid selection is an explicit error, not a successful no-op.
printf 'gnuchan.gpu=made-up\n' > "$ROOT/proc/cmdline"
if sh "$SELECTOR" > "$WORK/unknown.log" 2>&1; then
    printf 'unknown GPU selection unexpectedly succeeded\n' >&2
    exit 1
fi
grep -q "ERROR: unknown gnuchan.gpu selection 'made-up'" "$WORK/unknown.log"

# Both live boot menus suppress Plymouth and remain idempotent.
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
grep -q '^  append .*boot=live.*nosplash$' "$WORK/iso/isolinux/live.cfg"
grep -q '^    linux /live/vmlinuz boot=live nosplash$' \
    "$WORK/iso/boot/grub/grub.cfg"
! grep -q 'gnuchan.gpu=' "$WORK/iso/isolinux/live.cfg"
! grep -q 'gnuchan.gpu=' "$WORK/iso/boot/grub/grub.cfg"
before=$(wc -l < "$WORK/iso/isolinux/live.cfg")
grub_before=$(wc -l < "$WORK/iso/boot/grub/grub.cfg")
sh "$HERE/../config/hooks/normal/gpu_menu.sh" "$WORK/iso" \
    > "$WORK/menu-second.log"
after=$(wc -l < "$WORK/iso/isolinux/live.cfg")
grub_after=$(wc -l < "$WORK/iso/boot/grub/grub.cfg")
[ "$before" -eq "$after" ]
[ "$grub_before" -eq "$grub_after" ]

printf 'GPU selector and boot-menu tests passed\n'
