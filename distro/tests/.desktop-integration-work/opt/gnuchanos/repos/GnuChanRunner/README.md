# GnuChanRunner

GnuChanRunner is an application launcher in the shape of rofi or dmenu: it
scans the freedesktop `.desktop` files, matches what you type against them, and
starts the one you pick. It is written in C against Xlib and Xft for the window
and the text. It is Debian-only, on purpose.

## Building

    git clone <repository-url> GnuChanRunner
    cd GnuChanRunner/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanRunner`; the settings file goes to
`~/.config/GnuChanRunner/config.conf`. It uses plain `Key = value` lines;
repeat `DesktopDir`, `TerminalProgram`, or `Program` for multiple values.
Installed applications are discovered automatically from the user's
`XDG_DATA_HOME` and system `XDG_DATA_DIRS` application folders, including
`/usr/local/share/applications` and `/usr/share/applications`; custom
`DesktopDir` folders are added to that catalog. Programs with desktop entries
need no manual `Program` setting. Use `Program = "name|command"` only for
personal commands that do not have a desktop entry (add `|terminal` to run it
in GnuChanTerm). Binary-only executables are not enumerated from `/usr/local/bin`,
which would add unrelated utilities and services to the launcher. The list
shows a few results at a time; use the arrow keys to scroll through matches or
type part of a program name to filter them.
Existing `config.py` files are parsed by a restricted legacy reader, never
executed. It is a separate program from the window manager and installs on its
own.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run`, `list` and `uninstall`. `list` compiles and prints what the scan
finds without opening a window.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `build-essential` (gcc)
- `pkg-config`
- `libx11-dev` — `X11/Xlib.h`
- `libxft-dev` — `X11/Xft/Xft.h`

Xft's own header includes freetype's, which comes with `libxft-dev`. The linking
is driven by pkg-config for the modules `x11` and `xft`.

## Starting it

The window manager starts the launcher from its own settings script, bound to
Alt+R:

    gcl_key.MultiKey(keys=["Mod1", "r"],
                     action=gcl_spawn.RunProgram(command="GnuChanRunner"))

If the window manager's settings script does not bind it, run `GnuChanRunner`
from a shell.
Keys held when the launcher opens, including the Alt+R shortcut, are ignored
until released; key repeat from the launch chord therefore does not enter the
query.

Programs whose desktop entries set `Terminal=true` run inside `GnuChanTerm`
when it is installed. Other terminal emulators are only used as fallbacks.
Add desktop-entry names or executable names with repeated
`TerminalProgram = "name"` lines in the GnuChanRunner settings to force any
other program to start in GnuChanTerm.

## X11 only

This program is built directly against X11 (Xlib, Xft and the rest of the X
stack). It is designed for an X11 session and does not support Wayland: it was
never tested there, its window management assumes an X server, and no work has
been done to port it. On a Wayland session it may run through XWayland for basic
use, but that is not a supported configuration and behaviour there is undefined.
Run it on X11.

## License

GPL3.
