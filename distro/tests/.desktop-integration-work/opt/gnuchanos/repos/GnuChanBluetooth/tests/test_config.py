import json
import unittest
from pathlib import Path
from unittest.mock import patch

from gnuchan_bluetooth.config import AppConfig, ConfigError, load_config


class ConfigTests(unittest.TestCase):
    @patch.object(Path, "exists", return_value=False)
    def test_missing_config_uses_defaults(self, exists):
        self.assertEqual(load_config(Path("unused.json")), AppConfig())
        exists.assert_called_once()

    @patch.object(Path, "exists", return_value=True)
    @patch.object(Path, "read_text", return_value='{"command_timeout": 20, "scan_duration": 5}')
    def test_reads_supported_settings(self, read_text, _exists):
        self.assertEqual(
            load_config(Path("settings.json")),
            AppConfig(command_timeout=20, scan_duration=5),
        )
        read_text.assert_called_once_with(encoding="utf-8")

    @patch.object(Path, "exists", return_value=True)
    @patch.object(Path, "read_text", return_value='{"scan_duration": 90}')
    def test_rejects_unbounded_values(self, _read_text, _exists):
        with self.assertRaisesRegex(ConfigError, "between 1 and 60"):
            load_config(Path("settings.json"))

    @patch.object(Path, "exists", return_value=True)
    @patch.object(Path, "read_text", return_value=json.dumps(["not", "an", "object"]))
    def test_rejects_non_object_config(self, _read_text, _exists):
        with self.assertRaisesRegex(ConfigError, "JSON object"):
            load_config(Path("settings.json"))


if __name__ == "__main__":
    unittest.main()
