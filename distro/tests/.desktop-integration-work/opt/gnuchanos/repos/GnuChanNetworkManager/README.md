# GnuChanNetworkManager

GnuChanNetworkManager shows every network interface on the machine and does the
general work around them: it sets the DNS servers, brings an interface up or
down, and provides a DPI Settings panel for installing/removing zapret and
enabling/disabling the bypass. It opens the wifi manager for the
wireless side. It is written in C against Xlib and Xft, and is Debian-only, on
purpose.

## Building

    git clone <repository-url> GnuChanNetworkManager
    cd GnuChanNetworkManager/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanNetworkManager`; a desktop entry is written to
`/usr/share/applications/gnuchannetworkmanager.desktop`; the settings file goes
under `~/.config/GnuChanNetworkManager/config.conf`. Settings use plain
`Key = value` lines, with `#` comments. Existing `config.py` files remain
readable as legacy text and are never run as Python.
When the manager is launched through `sudo`, configurable executable paths are
replaced with the system defaults before any network action is run.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run`, `list`, `uninstall`, `zapret-install` and `zapret-remove`.
The two zapret actions are Debian-only and are also used by the DPI Settings
panel. Their operation log is `/var/log/gnuchan-zapret-manager.log`.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `build-essential` (gcc)
- `pkg-config`
- `libx11-dev` — `X11/Xlib.h`
- `libxft-dev` — `X11/Xft/Xft.h`

## What the install configures at run time

Besides the program itself, the install sets up the two things the manager
drives, so that one `python3 makefile.py` leaves a machine able to reach a site
its network blocks:

- **An encrypted resolver** (DNS-over-TLS) through systemd-resolved, already the
  resolver the manager drives: a drop-in at
  `/etc/systemd/resolved.conf.d/gnuchan-encrypted-dns.conf`, because the most
  common block is not DPI at all but plain DNS rewriting. `systemd-resolved` is
  installed when `resolvectl` is missing.
- **zapret** (nfqws), the DPI desynchroniser, under `/opt/zapret`, for the case
  where the block is the TLS name read inside a TCP packet. Its own `blockcheck`
  is run to pick the desync that works on the network at hand, and the result is
  written into its config; the default is used when no name looks blocked.

All of this is best-effort: a failure is reported and the install continues, so
the manager still opens.

## X11 only

This program is built directly against X11 (Xlib, Xft and the rest of the X
stack). It is designed for an X11 session and does not support Wayland: it was
never tested there, its window management assumes an X server, and no work has
been done to port it. On a Wayland session it may run through XWayland for basic
use, but that is not a supported configuration and behaviour there is undefined.
Run it on X11.

## License

GPL3.
