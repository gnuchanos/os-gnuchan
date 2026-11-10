# GnuChanWifi

GnuChanWifi is a wireless-network manager: it lists the networks in range,
joins one, and manages the saved connections. It talks to NetworkManager through
`nmcli`, which is already installed on any machine that uses NetworkManager, so
there is no runtime library to install beyond what the build needs. It is written
in C against Xlib and Xft, and is Debian-only, on purpose.

## Building

    git clone <repository-url> GnuChanWifi
    cd GnuChanWifi/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanWifi`; a desktop entry is written to
`/usr/share/applications/gnuchanwifi.desktop`, which is what makes the manager
appear in the launcher; the settings file is
`~/.config/GnuChanWifi/config.conf`. It uses plain `Key = value` lines.
Existing `config.py` files remain readable as legacy text and are never run as
Python.
When launched through `sudo`, `NmcliPath` and `RestartModule` are replaced
with their system defaults before network operations run.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run`, `list` and `uninstall`. `list` compiles and prints the networks
in range without opening a window.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `build-essential` (gcc)
- `pkg-config`
- `libx11-dev` — `X11/Xlib.h`
- `libxft-dev` — `X11/Xft/Xft.h`

At run time it drives `nmcli`, which comes with NetworkManager. Xft's own header
includes freetype's, which comes with `libxft-dev`. The linking is driven by
pkg-config for the modules `x11` and `xft`.

## X11 only

This program is built directly against X11 (Xlib, Xft and the rest of the X
stack). It is designed for an X11 session and does not support Wayland: it was
never tested there, its window management assumes an X server, and no work has
been done to port it. On a Wayland session it may run through XWayland for basic
use, but that is not a supported configuration and behaviour there is undefined.
Run it on X11.

## License

GPL3.
