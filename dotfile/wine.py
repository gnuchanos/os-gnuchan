#!/usr/bin/env python3
"""
Debian 13 (Trixie) gaming odaklı Wine kurulum scripti.
Kullanım:
  python3 /home/archkubi/Github/os-gnuchan/dotfile/wine.py
"""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


APT_ENV = {**os.environ, "DEBIAN_FRONTEND": "noninteractive"}

APT_PACKAGES = [
    "wget",
    "gnupg",
    "apt-transport-https",
    "ca-certificates",
    "cabextract",
    "unzip",
    "p7zip-full",
    "curl",
    "mesa-utils",
    "libgl1",
    "libglx-mesa0",
    "x11-xserver-utils",
    "libasound2-plugins:i386",
    "libpulse0:i386",
    "libvulkan1",
    "libvulkan1:i386",
    "vulkan-tools",
    "fonts-liberation",
    "winbind",
]

WINE_PACKAGES = [
    "winehq-stable",
]


def run(cmd: list[str], check: bool = True, env: dict[str, str] | None = None) -> subprocess.CompletedProcess:
    print(f"\n>>> {' '.join(cmd)}")
    return subprocess.run(cmd, check=check, env=env)


def ensure_root_auto() -> None:
    if os.geteuid() == 0:
        return
    try:
        os.execvp("sudo", ["sudo", sys.executable, os.path.abspath(__file__), *sys.argv[1:]])
    except FileNotFoundError:
        print("Hata: sudo bulunamadı. Manuel: sudo python3", os.path.abspath(__file__))
        sys.exit(1)


def check_debian13() -> None:
    osr = Path("/etc/os-release")
    if not osr.exists():
        print("Uyarı: /etc/os-release yok, devam ediliyor...")
        return

    data: dict[str, str] = {}
    for line in osr.read_text(encoding="utf-8", errors="ignore").splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            data[k] = v.strip().strip('"')

    pretty = data.get("PRETTY_NAME", "Bilinmiyor")
    codename = (data.get("VERSION_CODENAME") or data.get("DEBIAN_CODENAME") or "").lower()
    print(f"Sistem: {pretty}")
    if codename and codename not in ("trixie", "13"):
        print(f"Uyarı: Trixie değil görünüyor (codename={codename}). Yine de devam ediliyor...")


def ensure_i386() -> None:
    print("\n[1/6] i386 ekleniyor...")
    res = subprocess.run(["dpkg", "--print-foreign-architectures"], capture_output=True, text=True, check=False)
    arches = {a.strip() for a in res.stdout.splitlines() if a.strip()}
    if "i386" in arches:
        print("i386 zaten var.")
    else:
        run(["dpkg", "--add-architecture", "i386"])


def apt_update() -> None:
    print("\n[2/6] apt update...")
    run(["apt-get", "update"], env=APT_ENV)


def install_base_packages() -> None:
    print("\n[3/6] Temel paketler...")
    run(["apt-get", "install", "-y", "--install-recommends"] + APT_PACKAGES, env=APT_ENV)


def setup_winehq_repo() -> None:
    print("\n[4/6] WineHQ repo...")
    keyrings_dir = Path("/etc/apt/keyrings")
    keyrings_dir.mkdir(parents=True, exist_ok=True)
    key_path = keyrings_dir / "winehq-archive.key"
    run(["wget", "-q", "-O", str(key_path), "https://dl.winehq.org/wine-builds/winehq.key"])
    sources_path = Path("/etc/apt/sources.list.d/winehq.sources")
    sources_content = """Types: deb
URIs: https://dl.winehq.org/wine-builds/debian/
Suites: trixie
Components: main
Architectures: amd64 i386
Signed-By: /etc/apt/keyrings/winehq-archive.key
"""
    sources_path.write_text(sources_content, encoding="utf-8")
    print(f"Yazıldı: {sources_path}")


def install_wine() -> None:
    print("\n[5/6] winehq-stable...")
    run(["apt-get", "update"], env=APT_ENV)
    run(["apt-get", "install", "-y", "--install-recommends"] + WINE_PACKAGES, env=APT_ENV)


def install_winetricks() -> None:
    print("\n[6/6] winetricks...")
    res = subprocess.run(["apt-get", "install", "-y", "winetricks"], check=False, env=APT_ENV)
    if res.returncode != 0:
        print("Uyarı: winetricks kurulamadı.")


def print_next_steps() -> None:
    user = os.environ.get("SUDO_USER") or os.environ.get("USER") or "kullanici"
    print("\n" + "=" * 60)
    print("Bitti.")
    print(f"Örnek: export WINEARCH=win64; export WINEPREFIX=$HOME/.wine-gaming; winecfg (user: {user})")
    print("winetricks -q vcrun2019 d3dx9 d3dcompiler_47 dxvk vkd3d")
    print("=" * 60)


def main() -> int:
    print("=== Wine Gaming Kurulum (Debian 13) ===")
    ensure_root_auto()
    check_debian13()
    ensure_i386()
    apt_update()
    install_base_packages()
    setup_winehq_repo()
    install_wine()
    install_winetricks()
    print_next_steps()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
