"""Small, optional JSON configuration for GnuChanBluetooth."""

from __future__ import annotations

import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Any


class ConfigError(ValueError):
    """Raised when an application configuration file is invalid."""


@dataclass(frozen=True)
class AppConfig:
    command_timeout: int = 12
    scan_duration: int = 8


def default_config_path() -> Path:
    config_home = Path(os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config"))
    return config_home / "GnuChanBluetooth" / "config.json"


def _bounded_integer(data: dict[str, Any], key: str, default: int) -> int:
    value = data.get(key, default)
    if isinstance(value, bool) or not isinstance(value, int):
        raise ConfigError(f"{key} must be an integer")
    if not 1 <= value <= 60:
        raise ConfigError(f"{key} must be between 1 and 60")
    return value


def load_config(path: Path) -> AppConfig:
    """Load optional settings, keeping command and scan waits bounded."""
    try:
        if not path.exists():
            return AppConfig()
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ConfigError(f"Cannot read configuration {path}: {exc}") from exc

    if not isinstance(data, dict):
        raise ConfigError(f"Configuration {path} must contain a JSON object")
    return AppConfig(
        command_timeout=_bounded_integer(data, "command_timeout", 12),
        scan_duration=_bounded_integer(data, "scan_duration", 8),
    )
