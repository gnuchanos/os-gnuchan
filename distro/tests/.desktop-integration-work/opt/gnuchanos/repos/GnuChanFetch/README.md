# GnuChanFetch

GnuChanFetch prints facts about the machine with the GnuchanOS logo drawn
beside them, in the shape of neofetch. It is written in C and its one library is
Imlib2, which reads and scales the logo — the same library the window manager
draws its wallpaper with. It is Debian-only, on purpose.

GnuChanFetch is a terminal program, not a resizable desktop window. When its
output is a terminal, it reads the terminal's current column count and scales
the logo down to leave room for the facts; redirected output keeps the
configured image size.

## Building

    git clone <repository-url> GnuChanFetch
    cd GnuChanFetch/src
    python3 makefile.py

The makefile installs the build dependency with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanFetch`; `GnuChanFetch.conf` and the logo go under
`~/.config/GnuChanFetch/`. The settings file uses plain `Key = value` lines,
like GnuChanSettings' `system.conf`. Existing `GnuChanFetch.py` settings are
read by a restricted static parser when no `.conf` exists; they are never
executed.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run` and `uninstall`.

## Dependencies

The makefile probes for the Imlib2 header and installs the matching package when
it is missing:

- `build-essential` (gcc)
- `pkg-config`
- `libimlib2-dev` — `Imlib2.h`

The linking is driven by pkg-config for `imlib2`. Nothing else is named: every
number this program prints comes from a file or an integer count.

## Terminal

This is a terminal program: it needs no X server and no Wayland compositor, only
a terminal. Imlib2 is used to load the logo, not to open a window, so nothing
here connects to a display. It can run over ssh.

## License

GPL3.
