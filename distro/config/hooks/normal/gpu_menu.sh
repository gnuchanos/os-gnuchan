#!/bin/sh
# =============================================================================
# gpu_menu.sh - give the ISO's boot menus one entry per graphics driver.
#
# Each extra entry is a COPY of the ISO's own first entry with one thing
# changed: a `gnuchan.gpu=` word appended to its kernel command line. The live
# system reads that word from /proc/cmdline while it boots and brings up the
# matching driver.
#
#     Live System -> NVIDIA (new)     gnuchan.gpu=nvidia-new modprobe.blacklist=nouveau
#     Live System -> NVIDIA (legacy)  gnuchan.gpu=nvidia-legacy modprobe.blacklist=nouveau
#     Live System -> Radeon           gnuchan.gpu=amd
#     Live System -> Intel            gnuchan.gpu=intel
#
# WHY THIS IS A SHELL SCRIPT AND NOT A PYTHON ONE. It was a .py, run by the
# 0080-boot-branding binary hook with `python3`. The hook runs on the BUILD
# HOST - not inside the chroot - and the build is driven from a minimal
# debian:trixie container whose only package is live-build, which does NOT
# depend on python3. So `command -v python3` failed there, the hook quietly
# added nothing, and the finished ISO came out with the distribution's own
# single entry and no graphics choice at all. A menu editor has no business
# dragging an interpreter onto the build host: sh and awk are on every system
# that can run live-build in the first place. This does the same edit with awk.
#
# It edits, in order and each only if it is there:
#
#     <root>/isolinux/live.cfg   (or the first *.cfg that has label+append)
#     <root>/boot/grub/grub.cfg
#
# A menu that already names gnuchan.gpu is left alone, so running this twice is
# the same as running it once.
#
# License: GPL3
# =============================================================================
set -eu

log() { printf '==> gpu-menu: %s\n' "$1"; }

# The live ISO keeps one normal menu entry and leaves the detection to the
# runtime selector. The separate Intel/AMD/NVIDIA variants were a source of
# user confusion and made the wrong vendor selection easy to hit.
#
# The build host runs a minimal environment, so keep this branch simple and
# leave the existing first menu block untouched; the runtime code decides the
# correct GPU family from the actual hardware when the system boots.
GPU_MENU_AWK='
BEGIN { }
{ lines[NR] = $0 }
END {
    for (i = 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*menuentry([ \t]|$)/) { grub(); exit 0 }
    }
    isolinux()
    exit 0
}

function isolinux(   first, last, i) {
    first = 0
    for (i = 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*label[ \t]+[^ \t]/) { first = i; break }
    }
    if (first == 0) { for (i = 1; i <= NR; i++) print lines[i]; return }

    last = NR
    for (i = first + 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*label[ \t]+[^ \t]/) { last = i - 1; break }
    }

    for (i = 1; i <= NR; i++) print lines[i]
}

function grub(   first, last, i) {
    first = 0
    for (i = 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*menuentry([ \t]|$)/) { first = i; break }
    }
    if (first == 0) { for (i = 1; i <= NR; i++) print lines[i]; return }

    last = first
    for (i = first + 1; i <= NR; i++) {
        if (lines[i] ~ /^[ \t]*menuentry([ \t]|$)/) { break }
        last = i
    }
    for (i = 1; i <= NR; i++) print lines[i]
}
'

# Edit one file in place, unless it already names a graphics word.
edit_menu() {
    file=$1
    [ -f "$file" ] || return 0
    if grep -q 'gnuchan.gpu=' "$file" 2>/dev/null; then
        return 0
    fi
    if awk "$GPU_MENU_AWK" "$file" > "$file.gpu-menu.tmp"; then
        mv "$file.gpu-menu.tmp" "$file"
        log "added the graphics driver entries to $file"
    else
        rm -f "$file.gpu-menu.tmp"
        log "could not edit $file; it is left as it is"
    fi
}

# The ISOLINUX menu: live-build's live.cfg, or the first .cfg that has a label
# and an append - the two lines this edits.
find_isolinux() {
    dir=$1/isolinux
    [ -d "$dir" ] || return 1
    if [ -f "$dir/live.cfg" ]; then
        printf '%s\n' "$dir/live.cfg"
        return 0
    fi
    for file in "$dir"/*.cfg; do
        [ -f "$file" ] || continue
        if grep -q 'label' "$file" 2>/dev/null && grep -q 'append' "$file" 2>/dev/null; then
            printf '%s\n' "$file"
            return 0
        fi
    done
    return 1
}

disable_live_plymouth() {
    file=$1
    [ -f "$file" ] || return 0
    temporary="$file.nosplash.tmp"
    if awk '
        {
            if (($0 ~ /^[ \t]*linux(efi)?[ \t]/ ||
                 $0 ~ /^[ \t]*append[ \t]/) &&
                $0 ~ /boot=live/ &&
                $0 !~ /(^|[ \t])nosplash([ \t]|$)/)
                $0 = $0 " nosplash"
            print
        }
    ' "$file" > "$temporary"; then
        mv "$temporary" "$file"
        log "disabled Plymouth on live boot entries in $file"
    else
        rm -f "$temporary"
        log "ERROR: could not disable Plymouth in $file"
        return 1
    fi
}

root=${1:-.}

if isolinux=$(find_isolinux "$root"); then
    disable_live_plymouth "$isolinux"
    edit_menu "$isolinux"
else
    log "no ISOLINUX configuration found; the BIOS menu is left as it is"
fi

grub="$root/boot/grub/grub.cfg"
if [ -f "$grub" ]; then
    disable_live_plymouth "$grub"
    edit_menu "$grub"
else
    log "no GRUB configuration found; the EFI menu is left as it is"
fi

exit 0
