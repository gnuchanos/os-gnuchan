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

auto_detect_driver() {
    first=
    boot=
    for device in "$(root_path /sys/bus/pci/devices)"/*; do
        [ -f "$device/vendor" ] || continue
        vendor=$(cat "$device/vendor" 2>/dev/null || :)
        pci_class=$(cat "$device/class" 2>/dev/null || :)
        case "$vendor" in
            0x10de) detected=nvidia ;;
            0x1002) detected=amd ;;
            0x8086) detected=intel ;;
            *) continue ;;
        esac
        case "$pci_class" in
            0x03*) ;;
            *) continue ;;
        esac

        [ -n "$first" ] || first=$detected
        boot_vga=$(cat "$device/boot_vga" 2>/dev/null || :)
        if [ "$boot_vga" = 1 ]; then
            boot=$detected
            break
        fi
    done
    if [ -n "$boot" ]; then
        printf '%s\n' "$boot"
    else
        printf '%s\n' "$first"
    fi
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

has_amd_device_bound() {
    expected_driver=$1
    for device in "$(root_path /sys/bus/pci/devices)"/*; do
        [ -f "$device/vendor" ] || continue
        vendor=$(cat "$device/vendor" 2>/dev/null || :)
        pci_class=$(cat "$device/class" 2>/dev/null || :)
        [ "$vendor" = "0x1002" ] || continue
        case "$pci_class" in
            0x03*) ;;
            *) continue ;;
        esac
        driver=$(readlink "$device/driver" 2>/dev/null || :)
        case "$driver" in
            */"$expected_driver") return 0 ;;
        esac
    done
    return 1
}

restore_open_driver() {
    log "trying to restore the kernel's open NVIDIA driver"
    if modprobe nouveau; then
        log "nouveau loaded as a fallback"
        return 0
    else
        log "ERROR: nouveau could not be loaded as a fallback"
        return 1
    fi
}

fallback_to_nouveau() {
    reason=$1
    if restore_open_driver; then
        log "continuing with nouveau: $reason"
        return 0
    fi
    fail "$reason; nouveau fallback also failed"
}

install_from_pool() {
    package_dir=$1
    set --
    for package_file in "$POOL/common/"*.deb "$POOL/$package_dir/"*.deb; do
        [ -f "$package_file" ] || continue
        set -- "$@" "$package_file"
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
            fallback_to_nouveau "$package installation failed; no proprietary driver was activated"
            return 0
        fi
    fi

    # Install the NVIDIA Vulkan ICD so Vulkan-based apps (DXVK, games) use the
    # GPU instead of falling back to lavapipe / software rendering.
    # Failure here is non-fatal: the driver works without it, only Vulkan is missing.
    log "installing nvidia-vulkan-icd for Vulkan support"
    DEBIAN_FRONTEND=noninteractive \
        apt-get install -y --no-install-recommends nvidia-vulkan-icd 2>/dev/null \
        || log "WARNING: nvidia-vulkan-icd could not be installed; Vulkan will fall back to software rendering"

    log "refreshing kernel module dependencies"
    if command -v depmod >/dev/null 2>&1 && ! depmod -a; then
        fallback_to_nouveau "depmod failed; the selected driver cannot be loaded"
        return 0
    fi

    if [ -d "$(root_path /sys/module/nouveau)" ]; then
        log "nouveau is already loaded; unloading it before binding NVIDIA"
        if ! modprobe -r nouveau; then
            fallback_to_nouveau "nouveau is still active; NVIDIA was not loaded"
            return 0
        fi
    fi

    if ! load_module nvidia; then
        fallback_to_nouveau "the selected package is present but its nvidia module did not load"
        return 0
    fi
    if ! has_nvidia_device_bound; then
        fallback_to_nouveau "the nvidia module loaded but is not bound to an NVIDIA PCI device"
        return 0
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

bring_up_legacy_nvidia() {
    if ! command -v nvidia-detect >/dev/null 2>&1; then
        log "installing nvidia-detect to identify the card's supported driver"
        if ! install_from_pool nvidia-detect; then
            if ! install_from_apt nvidia-detect; then
                fallback_to_nouveau "could not install nvidia-detect for the selected NVIDIA card"
                return 0
            fi
        fi
    fi

    if ! command -v nvidia-detect >/dev/null 2>&1; then
        fallback_to_nouveau "nvidia-detect is still unavailable"
        return 0
    fi
    recommendation=$(nvidia-detect 2>&1 || :)
    package=
    for candidate in nvidia-driver nvidia-tesla-535-driver \
                     nvidia-legacy-470xx-driver \
                     nvidia-legacy-390xx-driver nvidia-legacy-340xx-driver; do
        case "$recommendation" in
            *"$candidate"*) package=$candidate; break ;;
        esac
    done
    if [ -z "$package" ]; then
        fallback_to_nouveau "nvidia-detect found no installable branch: $recommendation"
        return 0
    fi

    case "$package" in
        nvidia-driver) family=nvidia ;;
        *) family=$package ;;
    esac
    bring_up_nvidia "$family" "$package"
}

case "$(selected_driver)" in
    nvidia-new)
        bring_up_nvidia nvidia nvidia-driver
        ;;
    nvidia-legacy)
        bring_up_legacy_nvidia
        ;;
    amd)
        if load_module amdgpu && has_amd_device_bound amdgpu; then
            :
        elif load_module radeon && has_amd_device_bound radeon; then
            :
        else
            fail "neither amdgpu nor radeon bound to an AMD display device"
        fi
        ;;
    intel)
        load_module i915 || load_module xe ||
            fail "could not load either supported Intel module"
        ;;
    "")
        detected=$(auto_detect_driver)
        case "$detected" in
            nvidia)
                log "no explicit driver selected; auto-detected an NVIDIA GPU"
                bring_up_legacy_nvidia
                ;;
            amd)
                log "no explicit driver selected; auto-detected an AMD GPU"
                if load_module amdgpu && has_amd_device_bound amdgpu; then
                    :
                elif load_module radeon && has_amd_device_bound radeon; then
                    :
                else
                    fail "neither amdgpu nor radeon bound to an AMD display device"
                fi
                ;;
            intel)
                log "no explicit driver selected; auto-detected an Intel GPU"
                load_module i915 || load_module xe ||
                    fail "could not load either supported Intel module"
                ;;
            *)
                log "no explicit driver selected; keeping the kernel's default"
                ;;
        esac
        ;;
    *)
        fail "unknown gnuchan.gpu selection '$(selected_driver)'"
        ;;
esac
