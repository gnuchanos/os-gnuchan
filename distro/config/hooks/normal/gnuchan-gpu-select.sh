#!/bin/sh
# Apply the graphics-driver choice made in the ISO boot menu.
set -u

SYSROOT=${GNUCHAN_GPU_SYSROOT:-}
POOL=${GNUCHAN_GPU_POOL:-"$SYSROOT/usr/share/gnuchanos/gpu-pool"}

root_path() {
    printf '%s%s' "$SYSROOT" "$1"
}

log() {
    printf 'gnuchan-gpu-select: %s\n' "$1"
}

fail() {
    log "ERROR: $1"
    exit 1
}

selected_driver() {
    cmdline=$(cat "$(root_path /proc/cmdline)" 2>/dev/null || :)
    set -f
    set -- $cmdline
    set +f
    for token do
        case "$token" in
            gnuchan.gpu=*) printf '%s\n' "${token#gnuchan.gpu=}"; return ;;
        esac
    done
    printf '\n'
}

load_module() {
    module=$1
    log "loading kernel module $module"
    if modprobe "$module"; then
        log "loaded kernel module $module"
        return 0
    fi
    log "ERROR: could not load kernel module $module"
    return 1
}

has_nvidia_device_bound() {
    for device in "$(root_path /sys/bus/pci/devices)"/*; do
        [ -f "$device/vendor" ] || continue
        vendor=$(cat "$device/vendor" 2>/dev/null || :)
        [ "$vendor" = "0x10de" ] || continue
        driver=$(readlink "$device/driver" 2>/dev/null || :)
        case "$driver" in
            */nvidia) return 0 ;;
        esac
    done
    return 1
}

restore_open_driver() {
    log "trying to restore the kernel's open NVIDIA driver"
    if modprobe nouveau; then
        log "nouveau loaded as a fallback"
    else
        log "ERROR: nouveau could not be loaded as a fallback"
    fi
}

install_from_pool() {
    package_dir=$1
    set --
    for package in "$POOL/common/"*.deb "$POOL/$package_dir/"*.deb; do
        [ -f "$package" ] || continue
        set -- "$@" "$package"
    done
    [ "$#" -gt 0 ] || return 1

    log "installing the selected driver from the ISO package pool"
    DEBIAN_FRONTEND=noninteractive \
        apt-get install -y --no-install-recommends "$@"
}

install_from_apt() {
    package=$1
    log "refreshing package lists to fetch $package"
    if ! DEBIAN_FRONTEND=noninteractive apt-get update; then
        log "ERROR: package-list refresh failed"
        return 1
    fi
    log "installing $package from configured repositories"
    DEBIAN_FRONTEND=noninteractive \
        apt-get install -y --no-install-recommends "$package"
}

bring_up_nvidia() {
    family=$1
    package=$2
    log "selected NVIDIA driver family: $family ($package)"

    if ! install_from_pool "$family"; then
        log "the ISO pool did not install the driver; trying repositories"
        if ! install_from_apt "$package"; then
            restore_open_driver
            fail "$package installation failed; no proprietary driver was activated"
        fi
    fi

    log "refreshing kernel module dependencies"
    if command -v depmod >/dev/null 2>&1 && ! depmod -a; then
        restore_open_driver
        fail "depmod failed; the selected driver cannot be loaded"
    fi

    if [ -d "$(root_path /sys/module/nouveau)" ]; then
        log "nouveau is already loaded; unloading it before binding NVIDIA"
        if ! modprobe -r nouveau; then
            restore_open_driver
            fail "nouveau is still active; NVIDIA was not loaded"
        fi
    fi

    if ! load_module nvidia; then
        restore_open_driver
        fail "the selected package is present but its nvidia module did not load"
    fi
    if ! has_nvidia_device_bound; then
        restore_open_driver
        fail "the nvidia module loaded but is not bound to an NVIDIA PCI device"
    fi

    modprobe_dir=$(root_path /etc/modprobe.d)
    if install -d "$modprobe_dir" &&
        printf 'blacklist nouveau\noptions nouveau modeset=0\n' \
            > "$modprobe_dir/gnuchan-nouveau-off.conf"; then
        log "saved the nouveau blacklist for future boots"
    else
        log "WARNING: could not save the nouveau blacklist; this boot is active"
    fi
    log "NVIDIA driver is active for this boot"
    log "a reboot is required to make the package and selection permanent"
}

case "$(selected_driver)" in
    nvidia-new)
        bring_up_nvidia nvidia nvidia-driver
        ;;
    nvidia-legacy)
        bring_up_nvidia nvidia-legacy nvidia-legacy-470xx-driver
        ;;
    amd)
        load_module amdgpu || fail "could not load the requested AMD module"
        ;;
    intel)
        load_module i915 || load_module xe ||
            fail "could not load either supported Intel module"
        ;;
    "")
        log "no explicit driver selected; keeping the kernel's default"
        ;;
    *)
        fail "unknown gnuchan.gpu selection '$(selected_driver)'"
        ;;
esac
