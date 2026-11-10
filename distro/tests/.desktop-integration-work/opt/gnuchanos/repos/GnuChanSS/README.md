# GnuChanSS

GnuChanSS is the screen saver: it draws one of its effects over the whole screen
once the desk has been idle for the configured time. It is written in C against
X11, XRandR and the X ScreenSaver extension (the idle clock), and against D-Bus, which
carries the inhibit name a video player uses to hold the saver off. It is
Debian-only, on purpose.

The saver is intentionally a full-screen overlay, not a maximizable window. If
the screen mode changes while it is showing, it resizes its window, drawing
buffer and active effect to the new screen dimensions.

The 3D wall casts one ray for each pair of screen columns and targets 20 frames
per second to reduce CPU use. The pipe effect keeps its existing 30 FPS target.

The saver also recognizes common browser window classes automatically. Any
open browser window prevents it from appearing, including windowed video in
browsers that do not implement D-Bus inhibition; users do not need to configure
each browser separately. Fullscreen windows and applications using the
freedesktop inhibition protocol are handled as well.

## Building

    git clone <repository-url> GnuChanSS
    cd GnuChanSS/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanSS`; a desktop entry is written to
`/usr/share/applications/gnuchanss.desktop`; `GnuChanSS.conf` goes under
`~/.config/GnuChanSS/`. The file uses plain `Key = value` lines, like
GnuChanSettings' `system.conf`. Existing `GnuChanSS.py` settings are read by a
restricted static parser when no `.conf` exists; they are never executed.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run` and `uninstall`.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `build-essential` (gcc)
- `pkg-config`
- `libx11-dev` — `X11/Xlib.h`
- `libxss-dev` — `X11/extensions/scrnsaver.h`
- `libxrandr-dev` — `X11/extensions/Xrandr.h`
- `libdbus-1-dev` — `dbus/dbus.h`

The linking is driven by pkg-config for the modules `x11`, `xscrnsaver`,
`xrandr` and `dbus-1`. D-Bus is a dependency and not an optional extra because a saver built
without it drops over a film once the idle clock passes; the C code is still
written to compile without it, in which case the build says so.

## X11 only

This program is built directly against X11 (Xlib, Xft and the rest of the X
stack). It is designed for an X11 session and does not support Wayland: it was
never tested there, its window management assumes an X server, and no work has
been done to port it. On a Wayland session it may run through XWayland for basic
use, but that is not a supported configuration and behaviour there is undefined.
Run it on X11.

## License

GPL3.
