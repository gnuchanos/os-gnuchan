# GnuChanSL

GnuChanSL is the lock screen: it covers the screen and asks for the user's
password before it lets go. It is written in C against X11 and Xft for the
window and the text, XRandR for screen geometry changes, and PAM for the
password check. PAM is not optional: a lock screen that could not check a
password would open for anyone. It is Debian-only, on purpose.

The lock screen is intentionally a full-screen overlay, not a maximizable
window. It resizes its window and drawing buffer when the screen mode changes;
the password panel stays centered in the available screen area.

## Building

    git clone <repository-url> GnuChanLockScreen
    cd GnuChanLockScreen/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanSL`; a desktop entry is written to
`/usr/share/applications/gnuchansl.desktop`; `GnuChanSL.conf` goes under
`~/.config/GnuChanSL/`. The file uses plain `Key = value` lines, like
GnuChanSettings' `system.conf`. Existing `GnuChanSL.py` settings are read by a
restricted static parser when no `.conf` exists; they are never executed. It
also writes a PAM policy for the service `gnuchansl`
when none is there, pointing at the common include `login` uses, so an existing
policy written by an administrator is kept.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run` and `uninstall`.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `build-essential` (gcc)
- `pkg-config`
- `libx11-dev` — `X11/Xlib.h`
- `libxft-dev` — `X11/Xft/Xft.h`
- `libfreetype-dev` — `ft2build.h`
- `libxrandr-dev` — `X11/extensions/Xrandr.h`
- `libpam0g-dev` — `security/pam_appl.h`

The linking is driven by pkg-config for the modules `x11`, `xft` and `xrandr`;
PAM has no `.pc` entry and is named directly with `-lpam`.

## X11 only

This program is built directly against X11 (Xlib, Xft and the rest of the X
stack). It is designed for an X11 session and does not support Wayland: it was
never tested there, its window management assumes an X server, and no work has
been done to port it. On a Wayland session it may run through XWayland for basic
use, but that is not a supported configuration and behaviour there is undefined.
Run it on X11.

## License

GPL3.
