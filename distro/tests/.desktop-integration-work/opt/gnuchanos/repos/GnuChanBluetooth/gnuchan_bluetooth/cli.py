"""Command-line entry point."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from . import __version__
from .config import ConfigError, default_config_path, load_config


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="GnuChanBluetooth",
        description="Inspect and manage Bluetooth devices through BlueZ.",
        epilog="Bluetooth operations use the current user's BlueZ permissions; no root access is requested.",
    )
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")
    parser.add_argument(
        "--config",
        type=Path,
        metavar="FILE",
        help="read optional JSON settings from FILE (default: XDG config location)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = _parser()
    args = parser.parse_args(argv)
    config_path = args.config or default_config_path()
    try:
        config = load_config(config_path)
    except ConfigError as exc:
        parser.error(str(exc))

    try:
        import tkinter as tk

        from .gui import BluetoothWindow
    except ImportError as exc:
        print(f"Cannot start the desktop UI: {exc}", file=sys.stderr)
        return 1

    try:
        root = tk.Tk()
    except tk.TclError as exc:
        print(f"Cannot open the desktop UI: {exc}", file=sys.stderr)
        return 1
    BluetoothWindow(root, config)
    root.mainloop()
    return 0
