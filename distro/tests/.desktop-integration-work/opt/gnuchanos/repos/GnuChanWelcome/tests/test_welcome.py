import unittest
from pathlib import Path
from unittest.mock import patch

import gnuchan_welcome as welcome


class WelcomeConfigTests(unittest.TestCase):
    def test_default_is_not_complete_and_true_values_are_recognized(self):
        self.assertFalse(welcome.first_run_complete({}))
        for value in ("true", "TRUE", "yes", "1"):
            with self.subTest(value=value):
                self.assertTrue(welcome.first_run_complete({"FirstRunComplete": value}))
        for value in ("false", "no", "0", "invalid"):
            with self.subTest(value=value):
                self.assertFalse(welcome.first_run_complete({"FirstRunComplete": value}))

    def test_flat_config_parser_ignores_comments_and_keeps_values(self):
        parsed = welcome.parse_config(
            "# comment\nFirstRunComplete = false\nCustom = \"a=b\"\n"
        )
        self.assertEqual(
            parsed,
            {"FirstRunComplete": "false", "Custom": "a=b"},
        )

    def test_flat_config_parser_reports_malformed_lines(self):
        with self.assertRaisesRegex(ValueError, "line 2"):
            welcome.parse_config("# ok\nnot a setting\n")

    def test_missing_or_unreadable_config_does_not_suppress_first_run(self):
        with patch.object(welcome, "read_config", side_effect=ValueError("broken")):
            self.assertTrue(welcome.should_show_first_run(Path("ignored.conf")))
        with patch.object(welcome, "read_config", return_value={"FirstRunComplete": "true"}):
            self.assertFalse(welcome.should_show_first_run(Path("ignored.conf")))

    def test_config_path_uses_xdg_config_home_or_home_default(self):
        with patch.dict("os.environ", {"XDG_CONFIG_HOME": "C:/user/config"}, clear=False):
            self.assertEqual(
                welcome.get_config_path(),
                Path("C:/user/config") / "GnuChanWelcome" / "welcome.conf",
            )
        with patch.dict("os.environ", {}, clear=True), patch.object(
            Path, "home", return_value=Path("C:/user")
        ):
            self.assertEqual(
                welcome.get_config_path(),
                Path("C:/user/.config/GnuChanWelcome/welcome.conf"),
            )

    def test_config_writer_atomically_marks_first_run_complete(self):
        config_path = Path("config") / "welcome.conf"
        with (
            patch.object(Path, "mkdir") as mkdir,
            patch.object(Path, "write_text") as write_text,
            patch.object(Path, "replace") as replace,
        ):
            welcome.save_first_run_complete(config_path)
            welcome.save_first_run_complete(config_path)
        self.assertEqual(mkdir.call_count, 2)
        mkdir.assert_called_with(parents=True, exist_ok=True)
        self.assertEqual(write_text.call_count, 2)
        payload = write_text.call_args.args[0]
        self.assertEqual(
            payload,
            "# GnuChan Welcome first-run state\nFirstRunComplete = true\n",
        )
        self.assertTrue(welcome.first_run_complete(welcome.parse_config(payload)))
        self.assertEqual(replace.call_count, 2)
        replace.assert_called_with(config_path)


class ProgramCatalogTests(unittest.TestCase):
    EXPECTED_PROGRAMS = {
        "GnuChanBrowser",
        "GnuChanBluetooth",
        "GnuChanDesktop",
        "GnuChanDM",
        "GnuChanDock",
        "GnuChanFetch",
        "GnuChanFileManager",
        "GnuChanGUI",
        "GnuChanImageViewer",
        "GnuChanLockScreen",
        "GnuChanNetworkManager",
        "GnuChanNotification",
        "GnuChanRunner",
        "GnuChanScreenShoot",
        "GnuChanSettings",
        "GnuChanSoftwareCenter",
        "GnuChanSound",
        "GnuChanSoundRecord",
        "GnuChanSS",
        "GnuChanTerm",
        "GnuChanTop",
        "GnuChanWelcome",
        "GnuChanWifi",
        "GnuChanWM",
    }

    def test_catalog_has_unique_names_and_required_configuration(self):
        names = [program.name for program in welcome.PROGRAMS]
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(set(names), self.EXPECTED_PROGRAMS)
        for program in welcome.PROGRAMS:
            with self.subTest(program=program.name):
                self.assertTrue(program.description)
                self.assertTrue(program.config_path)
                self.assertTrue(program.settings_category)
                self.assertIn(program.group, {group[0] for group in welcome.GROUPS})

    def test_default_palette_matches_gnuchan_settings_purple_theme(self):
        self.assertEqual(
            welcome.THEME,
            {
                "background": "#1a0b2e",
                "sidebar": "#140826",
                "selection": "#32143f",
                "panel": "#241033",
                "edge": "#7b2cbf",
                "text": "#e0c3fc",
                "muted": "#9d7bba",
                "accent": "#c77dff",
            },
        )

    def test_library_has_no_standalone_configuration_or_settings_category(self):
        gui = next(program for program in welcome.PROGRAMS if program.name == "GnuChanGUI")
        self.assertIn("library", gui.description)
        self.assertIn("not a launchable", gui.description)
        self.assertEqual(gui.config_path, "No standalone config (library)")
        self.assertEqual(
            gui.settings_category,
            "No standalone Settings category (library)",
        )

    def test_lock_screen_card_uses_project_name_and_internal_program_name(self):
        lock_screen = next(
            program for program in welcome.PROGRAMS
            if program.name == "GnuChanLockScreen"
        )
        self.assertIn("GnuChanSL", lock_screen.description)
        self.assertEqual(lock_screen.config_path, "~/.config/GnuChanSL/GnuChanSL.conf")
        self.assertEqual(lock_screen.settings_category, "GnuChanSL")
        self.assertNotIn("GnuChanSL", {program.name for program in welcome.PROGRAMS})

    def test_browser_template_and_unavailable_settings_category_are_clear(self):
        browser = next(
            program for program in welcome.PROGRAMS if program.name == "GnuChanBrowser"
        )
        self.assertIn("GnuChanBrowser.conf", browser.config_path)
        self.assertIn("not currently loaded", browser.config_path)
        self.assertEqual(
            browser.settings_category,
            "Not currently available in GnuChanSettings",
        )

    def test_welcome_card_points_to_its_config_and_settings_category(self):
        app = next(
            program for program in welcome.PROGRAMS if program.name == "GnuChanWelcome"
        )
        self.assertEqual(
            app.config_path,
            "~/.config/GnuChanWelcome/welcome.conf",
        )
        self.assertEqual(app.settings_category, "GnuChanWelcome")

    def test_new_app_cards_are_in_their_matching_groups(self):
        expected = {
            "GnuChanBluetooth": "System",
            "GnuChanImageViewer": "Apps",
            "GnuChanSoundRecord": "Apps",
        }
        for name, group in expected.items():
            with self.subTest(program=name):
                app = next(program for program in welcome.PROGRAMS if program.name == name)
                self.assertEqual(app.group, group)
                self.assertTrue(app.description)
                self.assertIn("No dedicated Settings", app.settings_category)
        bluetooth = next(
            program for program in welcome.PROGRAMS
            if program.name == "GnuChanBluetooth"
        )
        recorder = next(
            program for program in welcome.PROGRAMS
            if program.name == "GnuChanSoundRecord"
        )
        self.assertIn("config.json", bluetooth.config_path)
        self.assertIn("optional", bluetooth.config_path)
        self.assertEqual(
            recorder.config_path,
            "~/.config/GnuChanSoundRecord/config.conf",
        )

    def test_mouse_wheel_scroll_directions_and_partial_deltas(self):
        self.assertEqual(welcome.wheel_scroll_units(button=4), -1)
        self.assertEqual(welcome.wheel_scroll_units(button=5), 1)
        self.assertEqual(welcome.wheel_scroll_units(delta=120), -1)
        self.assertEqual(welcome.wheel_scroll_units(delta=-240), 2)
        self.assertEqual(welcome.wheel_scroll_units(delta=1), -1)
        self.assertEqual(welcome.wheel_scroll_units(delta=0), 0)

    def test_every_group_has_a_program(self):
        for group, _title, _description in welcome.GROUPS:
            with self.subTest(group=group):
                self.assertTrue(any(program.group == group for program in welcome.PROGRAMS))

    def test_shared_sound_and_system_settings_use_system_config(self):
        sound = next(program for program in welcome.PROGRAMS if program.name == "GnuChanSound")
        settings = next(program for program in welcome.PROGRAMS if program.name == "GnuChanSettings")
        self.assertEqual(sound.config_path, "~/.config/GnuChanSettings/system.conf")
        self.assertEqual(settings.config_path, sound.config_path)


if __name__ == "__main__":
    unittest.main()
