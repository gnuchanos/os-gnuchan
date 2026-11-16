#!/bin/sh

# Translate only the ISO's known GPU choices into safe installed-kernel args.
gnuchanos_gpu_grub_cmdline() {
    gnuchanos_gpu_cmdline_rest=${1:-}
    gnuchanos_gpu_cmdline_selected=
    while [ -n "$gnuchanos_gpu_cmdline_rest" ]; do
        case "$gnuchanos_gpu_cmdline_rest" in
            *' '*)
                gnuchanos_gpu_cmdline_word=${gnuchanos_gpu_cmdline_rest%% *}
                gnuchanos_gpu_cmdline_rest=${gnuchanos_gpu_cmdline_rest#* }
                ;;
            *)
                gnuchanos_gpu_cmdline_word=$gnuchanos_gpu_cmdline_rest
                gnuchanos_gpu_cmdline_rest=
                ;;
        esac
        case "$gnuchanos_gpu_cmdline_word" in
            gnuchan.gpu=nvidia-new|gnuchan.gpu=nvidia-legacy|\
            gnuchan.gpu=amd|gnuchan.gpu=intel)
                gnuchanos_gpu_cmdline_selected=${gnuchanos_gpu_cmdline_word#gnuchan.gpu=}
                break
                ;;
        esac
    done

    case "$gnuchanos_gpu_cmdline_selected" in
        nvidia-new)
            printf 'quiet nosplash gnuchan.gpu=%s modprobe.blacklist=nouveau\n' \
                "$gnuchanos_gpu_cmdline_selected"
            ;;
        nvidia-legacy)
            printf 'quiet nosplash gnuchan.gpu=%s\n' \
                "$gnuchanos_gpu_cmdline_selected"
            ;;
        amd|intel)
            printf 'quiet nosplash gnuchan.gpu=%s\n' \
                "$gnuchanos_gpu_cmdline_selected"
            ;;
        *)
            printf 'quiet nosplash\n'
            ;;
    esac
}
