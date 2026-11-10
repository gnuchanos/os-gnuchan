# GnuChanWM

GnuChanWM is the window manager — the session itself. It owns the windows, the
frame, the bar, the workspaces and the keyboard bindings; application windows
keep their native X11 rendering and input path. It also starts the desktop's
other programs from its own autostart list. It is written in C against Xlib, Xft,
freetype, Xcursor, Imlib2 and the compositor's Xcomposite, Xdamage and Xrender,
plus Xrandr and Xss. It is Debian-only, on purpose.

## Building

    git clone <repository-url> GnuChanWM
    cd GnuChanWM/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, builds a terminal if
the machine has none, compiles the sources, and re-runs itself through sudo. The
binary goes to `/usr/local/bin/GnuChanWM` with a session entry at
`/usr/share/xsessions/gnuchanwm.desktop`, so the display manager offers
"GnuChanWM". The settings script goes under `~/.config/GnuChanWM/`.

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
- `libxcursor-dev` — `X11/Xcursor/Xcursor.h`
- `libimlib2-dev` — `Imlib2.h`
- `libxcomposite-dev` — `X11/extensions/Xcomposite.h`
- `libxdamage-dev` — `X11/extensions/Xdamage.h`
- `libxrender-dev` — `X11/extensions/Xrender.h`
- `libxrandr-dev` — `X11/extensions/Xrandr.h`
- `libxss-dev` — `X11/extensions/scrnsaver.h`

The linking is driven by pkg-config for the modules `x11`, `xft`, `xcursor`,
`imlib2`, `xcomposite`, `xdamage`, `xrender`, `xrandr`, `xscrnsaver` and `xext`.

## X11 only

This program is built directly against X11 (Xlib and the rest of the X stack).
It is designed for an X11 session and does not support Wayland: a window manager
and a display manager own the X server itself, which has no Wayland equivalent,
and no work has been done to port them. Run this on an X11 session.

## License

GPL3.
