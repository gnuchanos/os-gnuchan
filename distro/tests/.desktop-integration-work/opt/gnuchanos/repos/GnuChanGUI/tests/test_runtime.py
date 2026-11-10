import unittest
from unittest.mock import call, patch

try:
    import GnuChanGUI as gui_module
    from GnuChanGUI import gcLibrary_original as gui_library
    from GnuChanGUI import GnuChanGUI
except ModuleNotFoundError as error:
    if error.name != "tkinter":
        raise
    gui_module = None
    gui_library = None
    GnuChanGUI = None


class ClosedWindow:
    def __init__(self):
        self.read_calls = 0
        self.closed = False

    def read(self, timeout=None):
        self.read_calls += 1
        return None, {}

    def close(self):
        self.closed = True


@unittest.skipIf(GnuChanGUI is None, "Tkinter is unavailable in this test environment")
class SetUpdateTests(unittest.TestCase):
    def test_window_close_without_callback_exits_event_loop(self):
        gui = GnuChanGUI()
        window = ClosedWindow()
        gui.GetWindow = window

        gui.SetUpdate(Update=lambda: None)

        self.assertEqual(window.read_calls, 1)
        self.assertTrue(window.closed)

    def test_window_close_runs_callback_once(self):
        gui = GnuChanGUI()
        window = ClosedWindow()
        calls = []
        gui.GetWindow = window

        gui.SetUpdate(Update=lambda: None, exitBEFORE=lambda: calls.append("exit"))

        self.assertEqual(calls, ["exit"])
        self.assertEqual(window.read_calls, 1)
        self.assertTrue(window.closed)


@unittest.skipIf(GnuChanGUI is None, "Tkinter is unavailable in this test environment")
class ThemeTests(unittest.TestCase):
    def test_theme_preset_keeps_property_and_custom_call_forms(self):
        with patch.object(gui_module, "theme") as apply_theme, patch.dict(
            gui_module.LOOK_AND_FEEL_TABLE, {}
        ):
            preset = gui_module.Themecolors().GnuChanOS
            self.assertTrue(callable(preset))
            self.assertEqual(apply_theme.call_args_list, [call("GnuchanTheme")])

            preset(themeName="CustomTheme", text="#123456")

            self.assertEqual(
                gui_module.LOOK_AND_FEEL_TABLE["CustomTheme"]["TEXT"], "#123456"
            )
            self.assertEqual(
                apply_theme.call_args_list,
                [call("GnuchanTheme"), call("CustomTheme")],
            )


@unittest.skipIf(GnuChanGUI is None, "Tkinter is unavailable in this test environment")
class SettingsPathTests(unittest.TestCase):
    def test_set_options_recreates_global_settings_for_path_only(self):
        old_path = gui_library.DEFAULT_USER_SETTINGS_GnuChanGUI_PATH
        old_filename = gui_library.DEFAULT_USER_SETTINGS_GnuChanGUI_FILENAME
        old_settings = gui_library.GnuChanGUI_user_settings

        try:
            with patch.object(gui_library, "UserSettings") as settings_factory:
                gui_library.set_options(GnuChanGUI_settings_path="custom-settings")

                self.assertEqual(
                    gui_library.DEFAULT_USER_SETTINGS_GnuChanGUI_PATH,
                    "custom-settings",
                )
                settings_factory.assert_called_once_with(
                    filename=old_filename, path="custom-settings"
                )
                self.assertIs(
                    gui_library.GnuChanGUI_user_settings,
                    settings_factory.return_value,
                )
        finally:
            gui_library.DEFAULT_USER_SETTINGS_GnuChanGUI_PATH = old_path
            gui_library.DEFAULT_USER_SETTINGS_GnuChanGUI_FILENAME = old_filename
            gui_library.GnuChanGUI_user_settings = old_settings


if __name__ == "__main__":
    unittest.main()
