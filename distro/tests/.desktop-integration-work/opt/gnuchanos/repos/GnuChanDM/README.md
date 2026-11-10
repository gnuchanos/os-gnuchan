# GnuChanDM

GnuChanDM is the display manager (the login screen): it owns the console, starts
an X server on `:0`, and runs the session the user picks — by default GnuChanWM.
It is written in C against Xlib for the window and against PAM for the password
check. It is Debian-only, on purpose.

## Building

    git clone <repository-url> GnuChanDM
    cd GnuChanDM/src
    python3 makefile.py

The makefile installs what it needs with apt-get, compiles the sources, and
re-runs itself through sudo. It writes several files: the binary
(`/usr/local/bin/GnuChanDM`), a launcher `/usr/local/bin/gnuchandm-session` that
starts X and puts the greeter on it, an xgreeter entry, an xsession entry, and a
systemd unit `/etc/systemd/system/gnuchandm.service`. It then stops, disables and
masks any rival display manager and enables itself as `display-manager.service`,
so it starts at boot. The settings file `GnuChanDM.conf` goes under
`$XDG_CONFIG_HOME/GnuChanDM/` when that variable is absolute, or
`~/.config/GnuChanDM/` otherwise. It uses plain `Key = value` lines, like
GnuChanSettings' `system.conf`. Existing `GnuChanDM.py` settings are read by a
restricted static parser when no `.conf` exists; they are never executed.
The installed systemd service points `GCL_DM_CONFIG` at that same user file,
so the root-run greeter reads the settings GnuChanSettings edits.

The makefile takes an action as its first argument: `install` (the default),
`build` and `uninstall`.

The greeter enables Num Lock when it starts. In either text field, Left/Right
move the cursor, Ctrl+Left/Right move between words, Home/End jump to the ends,
Backspace/Delete edit around the cursor, and Ctrl+A selects the field contents.
Tab and Up/Down continue to move between controls. Input is restricted to
printable ASCII, so editing positions are always complete character boundaries;
non-ASCII characters are ignored.

Focused tests can be run with `sh tests/run-tests.sh` from WSL. The Num Lock
runtime test uses `xvfb-run`.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `gcc`
- `pkg-config`
- `libx11-dev`, `libpam0g-dev` — `X11/Xlib.h`, `security/pam_appl.h`
- `xauth`
- `xserver-xorg`, `xserver-xorg-legacy` — when `Xorg` is missing
- `x11-utils` — when `xdpyinfo` is missing
- `xterm` — when no terminal is installed

## X11 only

This program is built directly against X11 (Xlib and the rest of the X stack).
It is designed for an X11 session and does not support Wayland: a window manager
and a display manager own the X server itself, which has no Wayland equivalent,
and no work has been done to port them. Run this on an X11 session.

## License

GPL3.
