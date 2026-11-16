#!/usr/bin/env python3
"""Install the extra packages a GnuchanOS machine carries on top of base."""

import os
import shutil
import subprocess

# Packages that provide a discoverable binary.
BINARIES = {
    "rfkill": "rfkill",
    "ntpsec": "ntpd",
    "deluge-gtk": "deluge-gtk",
    "deluge": "deluge",
    "deluged": "deluged",
    "openssh-server": "sshd",
    "irqbalance": "irqbalance",
    "kdenlive": "kdenlive",
}

# Packages that are libraries (no binary to check for).
LIBRARIES = (
    "python3-libtorrent",
    "python3-setuptools",
)

ALL_PACKAGES = list(BINARIES.keys()) + list(LIBRARIES)

POST_COMMANDS = [
    "sudo systemctl enable --now ssh",
    "sudo apt clean",
    "sudo systemctl enable --now irqbalance",
    "sudo apt install --reinstall breeze-icon-theme breeze kdenlive-data -y"
]


def is_package_installed(name: str) -> bool:
    """Return True if a Debian package is installed."""
    binary = BINARIES.get(name)
    if binary is not None:
        return shutil.which(binary) is not None
    # For library packages, ask dpkg.
    result = subprocess.run(
        ["dpkg", "-s", name],
        capture_output=True,
        text=True,
        check=False,
    )
    return result.returncode == 0


if __name__ == "__main__":
    missing = [pkg for pkg in ALL_PACKAGES if not is_package_installed(pkg)]

    if missing:
        print(f"Installing {len(missing)} missing packages: {', '.join(missing)}")
        os.system("sudo apt update && sudo apt upgrade -y")
        os.system(f"sudo apt install {' '.join(missing)} -y")

        if "ntpsec" in missing:
            os.system("sudo systemctl enable ntpsec")
            os.system("sudo systemctl start ntpsec")
            answer = input(
                "Do you want to set the timezone to Europe/Istanbul? (y/n): "
            )
            if answer == "y":
                os.system("sudo timedatectl set-timezone Europe/Istanbul")
            os.system("sudo timedatectl set-ntp true")
    else:
        print("All packages already installed.")

    for command in POST_COMMANDS:
        os.system(command)
