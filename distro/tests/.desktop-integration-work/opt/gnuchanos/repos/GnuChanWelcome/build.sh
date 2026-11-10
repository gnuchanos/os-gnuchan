#!/bin/sh
set -eu

if [ "$(uname -s)" != "Linux" ]; then
    echo "Build and tests must run in WSL/Linux." >&2
    exit 1
fi

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
python3 -m py_compile gnuchan_welcome.py tests/test_welcome.py
python3 -m unittest discover -s tests -v
sh tests/test_install.sh
python3 gnuchan_welcome.py --version
python3 gnuchan_welcome.py --check-config --config examples/welcome.conf
sh -n install.sh uninstall.sh tests/test_install.sh
echo "Build, configuration, and installer checks passed."
