import unittest

from gnuchan_bluetooth.cli import _parser


class CliTests(unittest.TestCase):
    def test_program_name_matches_the_canonical_application_name(self):
        self.assertEqual(_parser().prog, "GnuChanBluetooth")


if __name__ == "__main__":
    unittest.main()
