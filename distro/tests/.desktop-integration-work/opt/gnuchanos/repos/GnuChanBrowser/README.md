# GnuChanBrowser

GnuChanBrowser is a light web browser: a navigation bar over a QWebEngineView
and nothing else. It is written in C++/Qt6 and is tuned for the 2007 laptop it
runs on — an Intel 965GM whose GLES tops out at 2.0, which Qt WebEngine cannot
use (it runs only on GLES 3.0). The engine is therefore pinned to software
rendering and WebGL is off. It is Debian-only, on purpose.

## Building

    git clone <repository-url> GnuChanBrowser
    cd GnuChanBrowser/src
    python3 makefile.py

The makefile installs the build dependencies with apt-get, compiles the sources,
and re-runs itself through sudo to write into `/usr/local/bin`. The binary goes
to `/usr/local/bin/GnuChanBrowser`; a desktop entry is written to
`/usr/share/applications/gnuchanbrowser.desktop`.

The makefile takes an action as its first argument: `install` (the default),
`build`, `run`, `software` and `uninstall`. `software` runs the browser with CPU
rendering forced (`--software`).

`src/GnuChanBrowser_config/GnuChanBrowser.conf` is a plain-text settings
template (`Key = value` lines). The browser currently does not load or install
this template; its window and home-page values remain compiled defaults.

## Dependencies

The makefile probes for these headers and installs the matching package when one
is missing:

- `build-essential` (g++)
- `pkg-config`
- `qt6-base-dev` — `QtWidgets/QApplication`
- `qt6-webengine-dev` — `QtWebEngineWidgets/QWebEngineView`

The linking is driven by pkg-config for the modules `Qt6Widgets` and
`Qt6WebEngineWidgets`. Qt6 requires C++17; the build uses `-std=c++17`.

Large web apps are not forced into a single renderer or a fixed JavaScript
memory/cache limit. Chromium keeps its network service and hang recovery
enabled; page loading progress is shown in the status bar.

## X11 only

This program uses Qt6, which can run on Wayland, but it is built and tested only
for X11 here: it pins software rendering for an old Intel GPU and was never run
on a Wayland session. Treat X11 as the supported session.

## License

GPL3.
