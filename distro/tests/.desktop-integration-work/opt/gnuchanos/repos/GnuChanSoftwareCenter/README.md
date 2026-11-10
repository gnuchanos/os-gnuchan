# GnuChanSoftwareCenter

GnuChanSoftwareCenter is a Debian/X11 package browser and APT frontend. It
lists packages present in the local APT catalog, searches package names,
versions and descriptions, and filters by category, including **Fonts**.
The Fonts view is populated from Debian package section/tag metadata; it is
not a curated list, so it includes every font package available in the
configured APT catalog.

APT supplies dependency resolution and package downloads. The center supports
install, remove, update of an individual package, and a single **Check &
update** action that refreshes repository indexes before upgrading all
upgradable packages. APT operations use Polkit when authentication is enabled.
Installed package version, candidate version, description, maintainer,
homepage, and package sizes are shown when the local APT metadata provides
them. Install, remove, and update attempts are recorded in
`$XDG_STATE_HOME/GnuChanSoftwareCenter/history.log`, or
`~/.local/state/GnuChanSoftwareCenter/history.log` when `XDG_STATE_HOME` is
unset.

The package index is the configured local APT catalog; this application does
not invent package records or fetch a separate app-store catalog. A second
source selector can browse applications from configured Flatpak remotes when
the `flatpak` CLI is installed. APT and Flatpak actions run through a
single-worker queue, with status/progress text and a progress bar; APT update
actions include a one-button upgrade of all available system updates.

Flatpak remotes can be listed, added, or removed using the command-line
operations shown by `GnuChanSoftwareCenter --help`; remote management is not
yet available as an in-window editor. Debian APT metadata does not
consistently provide software licenses or screenshots, and this frontend has
no remote screenshot service. It does not provide a pause/cancel control for
an active package operation, an independent connectivity indicator, or
ratings/user reviews. Repository/source configuration for APT remains managed
by Debian's APT tools.

Keyboard shortcuts: `Ctrl+F` focuses search, `Ctrl+I` opens Installed, and
`Ctrl+U` opens Updates.

Build on Debian with `python3 src/makefile.py build`. Run the focused tests
from the repository root with:

```sh
mkdir -p _temp
gcc -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -pthread -Isrc \
    tests/catalog_test.c src/gnuchan_lang.c -lX11 \
    -o _temp/catalog-test
_temp/catalog-test
apt-cache dumpavail | _temp/catalog-test --system-catalog
```
