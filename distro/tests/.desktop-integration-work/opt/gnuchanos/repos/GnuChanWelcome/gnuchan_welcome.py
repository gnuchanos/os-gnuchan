#!/usr/bin/env python3
"""First-login guide to the GnuChan desktop."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tkinter as tk
from tkinter import messagebox, ttk

VERSION = "0.1.0"
CONFIG_RELATIVE_PATH = Path("GnuChanWelcome") / "welcome.conf"


class Program:
    def __init__(
        self,
        name: str,
        description: str,
        config_path: str,
        settings_category: str,
        group: str,
    ) -> None:
        self.name = name
        self.description = description
        self.config_path = config_path
        self.settings_category = settings_category
        self.group = group


PROGRAMS = (
    Program("GnuChanDM", "The graphical login screen.", "~/.config/GnuChanDM/GnuChanDM.conf", "GnuChanDM", "Desktop"),
    Program("GnuChanWM", "The window manager that arranges and manages your windows.", "~/.config/GnuChanWM/GnuChanWM.conf", "GnuChanWM", "Desktop"),
    Program("GnuChanDock", "The desktop dock for launching and switching between programs.", "~/.config/GnuChanDock/GnuChanDock.conf", "GnuChanDock", "Desktop"),
    Program("GnuChanDesktop", "Set your wallpaper, background colour, and display layout.", "~/.config/gnuchan-desktop/panel.conf", "GnuChanDesktop", "Desktop"),
    Program("GnuChanFileManager", "Browse files and folders, including hidden files.", "~/.config/GnuChanFileManager/config.conf", "GnuChanFileManager", "Desktop"),
    Program("GnuChanImageViewer", "View, organize, and make basic edits to images.", "No user configuration file", "No dedicated Settings category", "Apps"),
    Program("GnuChanLockScreen", "Lock the screen and require your password to unlock it (the installed program is GnuChanSL).", "~/.config/GnuChanSL/GnuChanSL.conf", "GnuChanSL", "Desktop"),
    Program("GnuChanBrowser", "A lightweight web browser for visiting websites.", "GnuChanBrowser/src/GnuChanBrowser_config/GnuChanBrowser.conf (source template; not currently loaded by the browser)", "Not currently available in GnuChanSettings", "Apps"),
    Program("GnuChanGUI", "A reusable GUI toolkit/library for building graphical applications; it is not a launchable desktop app.", "No standalone config (library)", "No standalone Settings category (library)", "Apps"),
    Program("GnuChanTerm", "A terminal for running shell commands.", "~/.config/GnuChanTerm/GnuChanTerm.conf", "GnuChanTerm", "Apps"),
    Program("GnuChanFetch", "Show system information in the terminal.", "~/.config/GnuChanFetch/GnuChanFetch.conf", "GnuChanFetch", "Apps"),
    Program("GnuChanRunner", "Find and launch installed programs from the keyboard.", "~/.config/GnuChanRunner/config.conf", "GnuChanRunner", "Apps"),
    Program("GnuChanSS", "The screen saver.", "~/.config/GnuChanSS/GnuChanSS.conf", "GnuChanSS", "Apps"),
    Program("GnuChanNotification", "Display desktop notification bubbles.", "~/.config/GnuChanNotification/GnuChanNotification.conf", "GnuChanNotification", "Apps"),
    Program("GnuChanScreenShoot", "Capture screenshots and choose where they are saved.", "~/.config/GnuChanScreenShoot/GnuChanScreenShoot.conf", "GnuChanScreenShoot", "Apps"),
    Program("GnuChanSoftwareCenter", "Browse packages and manage GnuChan updates.", "~/.config/GnuChanSoftwareCenter/GnuChanSoftwareCenter.conf", "GnuChanSoftwareCenter", "Apps"),
    Program("GnuChanSoundRecord", "Record audio from an available microphone.", "~/.config/GnuChanSoundRecord/config.conf", "No dedicated Settings category", "Apps"),
    Program("GnuChanTop", "Monitor system processes and resource use.", "~/.config/GnuChanTop/GnuChanTop.conf", "GnuChanTop", "Apps"),
    Program("GnuChanSettings", "The control panel for configuring GnuChan programs and system options.", "~/.config/GnuChanSettings/system.conf", "System", "System"),
    Program("GnuChanSound", "Control PipeWire audio; its language preference is shared with GnuChan apps.", "~/.config/GnuChanSettings/system.conf", "GnuChanSound", "System"),
    Program("GnuChanWifi", "Connect to and manage Wi-Fi networks.", "~/.config/GnuChanWifi/config.conf", "GnuChanWifi", "System"),
    Program("GnuChanNetworkManager", "Configure network interfaces, DNS, and the resolver.", "~/.config/GnuChanNetworkManager/config.conf", "GnuChanNetworkManager", "System"),
    Program("GnuChanBluetooth", "Discover and manage Bluetooth adapters and paired devices.", "~/.config/GnuChanBluetooth/config.json (optional)", "No dedicated Settings category", "System"),
    Program("GnuChanWelcome", "This first-login guide introduces GnuChan programs and settings.", "~/.config/GnuChanWelcome/welcome.conf", "GnuChanWelcome", "System"),
)

GROUPS = (
    ("Desktop", "Your desktop", "The parts of the desktop you see and use every day."),
    ("Apps", "Programs and tools", "Useful applications for commands, information, files, images, audio, and system activity."),
    ("System", "Settings and system tools", "Configure audio, networking, Bluetooth, and the GnuChan system."),
)

THEME = {
    "background": "#1a0b2e",
    "sidebar": "#140826",
    "selection": "#32143f",
    "panel": "#241033",
    "edge": "#7b2cbf",
    "text": "#e0c3fc",
    "muted": "#9d7bba",
    "accent": "#c77dff",
}


def wheel_scroll_units(delta: int = 0, button: int | None = None) -> int:
    if button == 4:
        return -1
    if button == 5:
        return 1
    if delta == 0:
        return 0
    units = max(1, abs(delta) // 120)
    return -units if delta > 0 else units


def get_config_path(override: str | None = None) -> Path:
    """Return the per-user Welcome settings file."""
    if override is not None:
        return Path(override).expanduser()
    config_home = os.environ.get("XDG_CONFIG_HOME")
    base = Path(config_home).expanduser() if config_home else Path.home() / ".config"
    return base / CONFIG_RELATIVE_PATH


def parse_config(text: str) -> dict[str, str]:
    """Parse Welcome's small, flat ``Key = value`` configuration format."""
    config: dict[str, str] = {}
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise ValueError(f"line {line_number}: expected Key = value")
        key, value = (part.strip() for part in line.split("=", 1))
        if not key:
            raise ValueError(f"line {line_number}: setting name is empty")
        config[key] = value.strip('"').strip("'")
    return config


def first_run_complete(config: dict[str, str]) -> bool:
    """Read the completion flag; missing or invalid values mean not complete."""
    return config.get("FirstRunComplete", "").strip().lower() in {
        "true",
        "yes",
        "1",
    }


def read_config(path: Path) -> dict[str, str]:
    if not path.exists():
        return {}
    return parse_config(path.read_text(encoding="utf-8"))


def should_show_first_run(path: Path) -> bool:
    try:
        return not first_run_complete(read_config(path))
    except (OSError, UnicodeError, ValueError):
        return True


def save_first_run_complete(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    staged_path = path.with_name(path.name + ".new")
    staged_path.write_text(
        "# GnuChan Welcome first-run state\nFirstRunComplete = true\n",
        encoding="utf-8",
    )
    staged_path.replace(path)


def validate_config(path: Path) -> None:
    if path.exists():
        parse_config(path.read_text(encoding="utf-8"))


class WelcomeWindow:
    def __init__(self, root: tk.Tk, config_path: Path) -> None:
        self.root = root
        self.config_path = config_path
        root.title("Welcome to GnuChan")
        root.geometry("920x680")
        root.minsize(720, 500)
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.configure(background=THEME["background"])

        style = ttk.Style(root)
        style.theme_use("clam")
        style.configure(".", background=THEME["background"], foreground=THEME["text"])
        style.configure("TFrame", background=THEME["background"])
        style.configure(
            "TLabel", background=THEME["background"], foreground=THEME["text"]
        )
        style.configure(
            "Title.TLabel",
            background=THEME["background"],
            foreground=THEME["text"],
            font=("TkDefaultFont", 20, "bold"),
        )
        style.configure(
            "Subtitle.TLabel",
            background=THEME["background"],
            foreground=THEME["muted"],
            font=("TkDefaultFont", 11),
        )
        style.configure(
            "Program.TLabel",
            background=THEME["panel"],
            foreground=THEME["text"],
            font=("TkDefaultFont", 12, "bold"),
        )
        style.configure(
            "Card.TLabel",
            background=THEME["panel"],
            foreground=THEME["text"],
        )
        style.configure(
            "Meta.TLabel",
            background=THEME["background"],
            foreground=THEME["muted"],
        )
        style.configure(
            "CardMeta.TLabel",
            background=THEME["panel"],
            foreground=THEME["muted"],
        )
        style.configure(
            "Card.TFrame",
            background=THEME["panel"],
            bordercolor=THEME["edge"],
            relief="solid",
            borderwidth=1,
        )
        style.configure(
            "TNotebook",
            background=THEME["background"],
            bordercolor=THEME["edge"],
            tabmargins=(2, 4, 2, 0),
        )
        style.configure(
            "TNotebook.Tab",
            background=THEME["sidebar"],
            foreground=THEME["text"],
            padding=(12, 7),
        )
        style.map(
            "TNotebook.Tab",
            background=[("selected", THEME["selection"])],
            foreground=[("selected", THEME["accent"])],
        )
        style.configure(
            "TButton",
            background=THEME["panel"],
            foreground=THEME["text"],
            bordercolor=THEME["edge"],
            padding=(10, 6),
        )
        style.map(
            "TButton",
            background=[("active", THEME["selection"]), ("pressed", THEME["edge"])],
            foreground=[("disabled", THEME["muted"])],
        )
        style.configure(
            "Vertical.TScrollbar",
            background=THEME["panel"],
            troughcolor=THEME["sidebar"],
            arrowcolor=THEME["text"],
            bordercolor=THEME["edge"],
        )

        shell = ttk.Frame(root, padding=18)
        shell.pack(fill="both", expand=True)
        self._scroll_canvases: set[tk.Canvas] = set()
        root.bind_all("<MouseWheel>", self._on_mousewheel)
        root.bind_all("<Button-4>", self._on_mousewheel)
        root.bind_all("<Button-5>", self._on_mousewheel)
        self.tabs = ttk.Notebook(shell)
        self.tabs.pack(fill="both", expand=True)

        self._add_welcome_tab()
        for group, title, description in GROUPS:
            self._add_program_tab(group, title, description)

        footer = ttk.Frame(shell)
        footer.pack(fill="x", pady=(14, 0))
        ttk.Label(
            footer,
            text="You can open this guide again any time from the Applications menu.",
            style="Meta.TLabel",
        ).pack(side="left")
        ttk.Button(footer, text="Open GnuChanSettings", command=self.open_settings).pack(
            side="right", padx=(8, 0)
        )
        ttk.Button(footer, text="Done", command=self.close).pack(side="right")

    def _add_welcome_tab(self) -> None:
        page = ttk.Frame(self.tabs, padding=28)
        self.tabs.add(page, text="Welcome")
        ttk.Label(page, text="Welcome to GnuChan", style="Title.TLabel").pack(
            anchor="w", pady=(0, 12)
        )
        ttk.Label(
            page,
            text=(
                "Your desktop is ready. This guide introduces the programs included "
                "with GnuChan and points you to the controls for each one."
            ),
            style="Subtitle.TLabel",
            wraplength=760,
            justify="left",
        ).pack(anchor="w", pady=(0, 24))

        quick = ttk.LabelFrame(page, text="Getting started", padding=18)
        quick.pack(fill="x", anchor="n")
        tips = (
            ("Open programs", "Use the dock or GnuChanRunner to find and start an app."),
            ("Change preferences", "Open GnuChanSettings and choose the matching category."),
            (
                "Find configuration files",
                "Each app's configuration path is shown on its tab. Most are under ~/.config.",
            ),
        )
        for title, detail in tips:
            row = ttk.Frame(quick)
            row.pack(fill="x", pady=6)
            ttk.Label(row, text=title, style="Program.TLabel", width=22).pack(
                side="left", anchor="n"
            )
            ttk.Label(row, text=detail, wraplength=620, justify="left").pack(
                side="left", fill="x", expand=True
            )

        ttk.Label(
            page,
            text=f"{len(PROGRAMS)} programs are described in the tabs. Select an app to see what it does and where to configure it.",
            style="Meta.TLabel",
            wraplength=760,
        ).pack(anchor="w", pady=(22, 0))

    def _add_program_tab(self, group: str, title: str, description: str) -> None:
        page = ttk.Frame(self.tabs)
        self.tabs.add(page, text=title)
        canvas = tk.Canvas(
            page,
            background=THEME["background"],
            highlightthickness=0,
            borderwidth=0,
        )
        self._scroll_canvases.add(canvas)
        scrollbar = ttk.Scrollbar(page, orient="vertical", command=canvas.yview)
        content = ttk.Frame(canvas, padding=18)
        content_window = canvas.create_window((0, 0), window=content, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        content.bind(
            "<Configure>",
            lambda _event: canvas.configure(scrollregion=canvas.bbox("all")),
        )
        canvas.bind(
            "<Configure>",
            lambda event: canvas.itemconfigure(content_window, width=event.width),
        )
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        ttk.Label(content, text=title, style="Title.TLabel").pack(anchor="w")
        ttk.Label(content, text=description, style="Subtitle.TLabel").pack(
            anchor="w", pady=(4, 16)
        )
        for program in (item for item in PROGRAMS if item.group == group):
            card = ttk.Frame(content, style="Card.TFrame", padding=14)
            card.pack(fill="x", pady=(0, 10))
            ttk.Label(card, text=program.name, style="Program.TLabel").pack(anchor="w")
            ttk.Label(
                card,
                text=program.description,
                style="Card.TLabel",
                wraplength=780,
                justify="left",
            ).pack(anchor="w", pady=(5, 9))
            ttk.Label(
                card,
                text=f"Configuration: {program.config_path}",
                style="CardMeta.TLabel",
                wraplength=780,
            ).pack(anchor="w")
            ttk.Label(
                card,
                text=f"Configure in GnuChanSettings: {program.settings_category}",
                style="CardMeta.TLabel",
                wraplength=780,
            ).pack(anchor="w", pady=(3, 0))

    def _on_mousewheel(self, event: tk.Event) -> str | None:
        widget = event.widget
        while widget is not None:
            if isinstance(widget, tk.Canvas) and widget in self._scroll_canvases:
                units = wheel_scroll_units(
                    getattr(event, "delta", 0),
                    getattr(event, "num", None),
                )
                if units:
                    widget.yview_scroll(units, "units")
                return "break"
            widget = getattr(widget, "master", None)
        return None

    def open_settings(self) -> None:
        try:
            subprocess.Popen(["GnuChanSettings"])
        except OSError as error:
            messagebox.showerror(
                "GnuChanSettings not available",
                f"Could not open GnuChanSettings:\n{error}",
                parent=self.root,
            )

    def close(self) -> None:
        try:
            save_first_run_complete(self.config_path)
        except OSError as error:
            messagebox.showerror(
                "Could not save Welcome state",
                f"The first-run state could not be saved:\n{error}",
                parent=self.root,
            )
        self.root.destroy()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Welcome guide for the GnuChan desktop.")
    parser.add_argument("--version", action="version", version=f"GnuChanWelcome {VERSION}")
    parser.add_argument(
        "--first-run",
        action="store_true",
        help="show only until the first-run guide has been dismissed",
    )
    parser.add_argument(
        "--config",
        help="use a different Welcome config file (default: ~/.config/GnuChanWelcome/welcome.conf)",
    )
    parser.add_argument(
        "--check-config",
        action="store_true",
        help="validate the Welcome config file and exit",
    )
    args = parser.parse_args(argv)
    config_file = get_config_path(args.config)
    if args.check_config:
        try:
            validate_config(config_file)
        except (OSError, UnicodeError, ValueError) as error:
            print(f"Invalid Welcome config: {error}", file=sys.stderr)
            return 1
        print(f"Welcome config is valid: {config_file}")
        return 0
    if args.first_run and not should_show_first_run(config_file):
        return 0
    try:
        root = tk.Tk()
    except tk.TclError as error:
        print(f"GnuChanWelcome needs a graphical desktop: {error}", file=sys.stderr)
        return 1
    WelcomeWindow(root, config_file)
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
