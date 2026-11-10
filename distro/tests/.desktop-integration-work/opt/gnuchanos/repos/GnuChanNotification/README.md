# GnuChanNotification

GnuChanNotification is a notification daemon in the shape of dunst: it holds the
name `org.freedesktop.Notifications` on the session bus and draws the messages
other programs send it. It is built against the X stack (the window, the font,
the shape) and libdbus, which every desktop that has a session bus already has.
It is written in C and is Debian-only, on purpose.

## Building

    git clone <repository-url> GnuChanNotification
    cd GnuChanNotification/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanNotification`; a desktop entry is written to
`/usr/share/applications/gnuchannotification.desktop` and an autostart entry to
`/etc/xdg/autostart/gnuchannotification.desktop`, so the daemon starts with the
session. `GnuChanNotification.conf` and the logo are installed into
`~/.config/GnuChanNotification/`. The settings file uses plain `Key = value`
lines, like GnuChanSettings' `system.conf`. Existing
`GnuChanNotification.py` settings are read by a restricted static parser when
no `.conf` exists; they are never executed.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run`, `test` and `uninstall`. `test` compiles and shows one test
notification.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `build-essential` (gcc)
- `pkg-config`
- `libx11-dev` — `X11/Xlib.h`
- `libxft-dev` — `X11/Xft/Xft.h`
- `libfreetype-dev` — `ft2build.h`
- `libxext-dev` — `X11/extensions/shape.h`
- `libimlib2-dev` — `Imlib2.h`
- `libdbus-1-dev` — `dbus/dbus.h`

The linking is driven by pkg-config for the modules `x11`, `xft`, `xext`,
`fontconfig`, `freetype2`, `imlib2` and `dbus-1`.

## X11 only

This program is built directly against X11 (Xlib, Xft and the rest of the X
stack). It is designed for an X11 session and does not support Wayland: it was
never tested there, its window management assumes an X server, and no work has
been done to port it. On a Wayland session it may run through XWayland for basic
use, but that is not a supported configuration and behaviour there is undefined.
Run it on X11.

## License

GPL3.