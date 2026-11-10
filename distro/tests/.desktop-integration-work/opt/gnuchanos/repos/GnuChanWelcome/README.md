# GnuChanWelcome

GnuChanWelcome is a small Python/Tk desktop guide for first login. Its tabs
describe the GnuChan programs and show each program's configuration file and
the category to open in GnuChanSettings.

## Run and build

Run the guide with `python3 gnuchan_welcome.py`. The installable command is
`gnuchan-welcome`; it requires Python 3 and Tk (`python3-tk` on Debian).

On Debian/Ubuntu, install system-wide with:

```sh
sudo sh install.sh
```

The installer places the command in `/usr/local/bin/gnuchan-welcome`, the menu
entry in `/usr/local/share/applications/gnuchan-welcome.desktop`, and the
first-login autostart entry in `/etc/xdg/autostart/gnuchan-welcome.desktop`.
The autostart entry invokes `gnuchan-welcome --first-run`. The first time the
guide is dismissed, it records completion for the current user; later
autostart launches exit without opening a window. The application-menu entry
opens the guide whenever selected. Run `sudo sh uninstall.sh` to remove the
program and desktop entries.

The system-wide entry follows the XDG autostart convention. GnuChanWM starts
programs from its own `gcl_autostart.all` list rather than reading XDG entries;
the GnuChanWM session defaults used by a distro image must therefore also
include `gnuchan-welcome --first-run` in that list. The completion check makes
that command harmless on subsequent logins.

Both installer scripts honor `PREFIX`, `SYSCONFDIR`, and `DESTDIR`, for example
when packaging the application into an ISO or staging root:

```sh
DESTDIR=/path/to/stage PREFIX=/usr SYSCONFDIR=/etc sh install.sh
```

The distro's live-ISO build should install this repository using that
destination root; installing the application alone does not add it to a
separate distro build definition.

Run `sh build.sh` in Linux or WSL to compile-check, run unit tests, validate the
example configuration, check shell installer syntax, and exercise staged,
repeatable install/uninstall plus the completed first-run path.

## Configuration and first-run behavior

Welcome stores per-user state at
`~/.config/GnuChanWelcome/welcome.conf` (or
`$XDG_CONFIG_HOME/GnuChanWelcome/welcome.conf`). It uses the same simple
`Key = value` format used by other GnuChan applications:

```conf
FirstRunComplete = true
```

The file is created only after the user closes the welcome guide. `--first-run`
shows the guide until then; `--config PATH` overrides the state file for
testing or per-user administration. `--check-config` validates a config file
without opening a window.

GnuChanSettings provides a **GnuChanWelcome** category/entry for this guide's
`welcome.conf` file.

## Included programs

The guide describes the 24 supplied projects:
GnuChanBrowser, GnuChanDesktop, GnuChanDM, GnuChanDock, GnuChanFetch,
GnuChanFileManager, GnuChanGUI, GnuChanImageViewer, GnuChanLockScreen,
GnuChanNetworkManager, GnuChanNotification, GnuChanRunner, GnuChanScreenShoot,
GnuChanSettings, GnuChanSoftwareCenter, GnuChanSound, GnuChanSoundRecord,
GnuChanSS, GnuChanTerm, GnuChanTop, GnuChanWelcome, GnuChanWifi, GnuChanWM,
and GnuChanBluetooth.

GnuChanGUI is a reusable toolkit/library, not a launchable program, so it has
no standalone config or GnuChanSettings category. GnuChanBrowser's card points
to its source-tree `GnuChanBrowser.conf` template, which is not currently loaded
by the browser or exposed in GnuChanSettings. The GnuChanLockScreen project is
the screen locker installed as `GnuChanSL`; its card uses the project name and
points to `~/.config/GnuChanSL/GnuChanSL.conf`, configured in the GnuChanSL
category. It is not listed as a separate GnuChanSL project.

The GnuChanWelcome card lists its own
`~/.config/GnuChanWelcome/welcome.conf` and GnuChanWelcome Settings category.
GnuChanSound and GnuChanSettings share
`~/.config/GnuChanSettings/system.conf`.

GnuChanSettings also provides Autostart and GnuChan Themes categories; these
configure the session startup list and shared desktop theme rather than
separate programs.
