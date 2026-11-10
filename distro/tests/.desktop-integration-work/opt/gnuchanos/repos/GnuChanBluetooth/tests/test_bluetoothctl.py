import subprocess
import unittest
from unittest.mock import Mock, patch

from gnuchan_bluetooth.bluetoothctl import (
    BluetoothctlClient,
    BluetoothctlError,
    normalize_address,
    parse_devices,
)


ADDRESS = "AA:BB:CC:DD:EE:FF"


class AddressTests(unittest.TestCase):
    def test_normalizes_address(self):
        self.assertEqual(normalize_address("aa:bb:cc:dd:ee:ff"), ADDRESS)

    def test_rejects_non_address_input(self):
        for address in ("", "Device " + ADDRESS, ADDRESS + "; remove XX", "not-a-device"):
            with self.subTest(address=address), self.assertRaises(ValueError):
                normalize_address(address)

    def test_parses_device_names_without_treating_them_as_identity(self):
        devices = parse_devices(
            f"Device {ADDRESS} Speaker; connect XX\n"
            "Device 11:22:33:44:55:66\nnoise"
        )
        self.assertEqual(
            [(device.address, device.name) for device in devices],
            [(ADDRESS, "Speaker; connect XX"), ("11:22:33:44:55:66", "")],
        )


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.client = BluetoothctlClient(command_timeout=9)

    @patch("gnuchan_bluetooth.bluetoothctl.subprocess.run")
    def test_command_is_argument_vector_with_timeout(self, run):
        run.return_value = subprocess.CompletedProcess(
            ["bluetoothctl", "trust", ADDRESS], 0, stdout="trust succeeded", stderr=""
        )
        self.client.trust(ADDRESS.lower())
        args, kwargs = run.call_args
        self.assertEqual(args[0], ["bluetoothctl", "trust", ADDRESS])
        self.assertEqual(kwargs["timeout"], 9)
        self.assertNotIn("shell", kwargs)

    @patch("gnuchan_bluetooth.bluetoothctl.subprocess.run")
    def test_bad_address_never_runs_process(self, run):
        with self.assertRaises(ValueError):
            self.client.remove(ADDRESS + " && something")
        run.assert_not_called()

    @patch("gnuchan_bluetooth.bluetoothctl.subprocess.run")
    def test_timeout_is_reported(self, run):
        run.side_effect = subprocess.TimeoutExpired("bluetoothctl", 9, output="waiting")
        with self.assertRaisesRegex(BluetoothctlError, "timed out after 9 seconds"):
            self.client.controller_info()

    @patch("gnuchan_bluetooth.bluetoothctl.subprocess.run")
    def test_nonzero_and_bluez_errors_are_reported(self, run):
        run.return_value = subprocess.CompletedProcess(
            ["bluetoothctl", "show"], 1, stdout="", stderr="No default controller available"
        )
        with self.assertRaisesRegex(BluetoothctlError, "No default controller"):
            self.client.controller_info()

    @patch("gnuchan_bluetooth.bluetoothctl.subprocess.run")
    def test_devices_are_parsed(self, run):
        run.return_value = subprocess.CompletedProcess(
            ["bluetoothctl", "devices"],
            0,
            stdout=f"Device {ADDRESS} Headphones\n",
            stderr="",
        )
        self.assertEqual(self.client.devices()[0].name, "Headphones")
        run.assert_called_once()

    @patch("gnuchan_bluetooth.bluetoothctl.time.sleep")
    @patch("gnuchan_bluetooth.bluetoothctl.threading.Thread")
    @patch("gnuchan_bluetooth.bluetoothctl.subprocess.Popen")
    @patch("gnuchan_bluetooth.bluetoothctl.subprocess.run")
    def test_scan_is_bounded_stopped_and_refreshes_devices(
        self, run, popen, thread, sleep
    ):
        scanner = Mock()
        scanner.stdin = Mock()
        scanner.stdout = []
        scanner.returncode = 0
        scanner.poll.return_value = None
        popen.return_value = scanner
        thread_instance = thread.return_value
        run.return_value = subprocess.CompletedProcess(
            ["bluetoothctl", "devices"], 0, stdout=f"Device {ADDRESS} Headphones", stderr=""
        )
        self.assertEqual(self.client.scan(3)[0].address, ADDRESS)
        sleep.assert_called_once_with(3)
        scanner.stdin.write.assert_any_call("scan on\n")
        scanner.stdin.write.assert_any_call("scan off\nquit\n")
        scanner.wait.assert_called_once_with(timeout=5)
        thread_instance.start.assert_called_once()
        thread_instance.join.assert_called_once_with(timeout=2)
        self.assertEqual(run.call_args.args[0], ["bluetoothctl", "devices"])
        self.assertEqual(popen.call_args.args[0], ["bluetoothctl"])

    def test_scan_duration_is_bounded(self):
        with self.assertRaises(ValueError):
            self.client.scan(61)


if __name__ == "__main__":
    unittest.main()
