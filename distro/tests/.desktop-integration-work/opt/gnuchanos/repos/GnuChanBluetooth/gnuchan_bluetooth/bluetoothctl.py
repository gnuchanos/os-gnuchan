"""Safe, timeout-bounded access to BlueZ's bluetoothctl command line."""

from __future__ import annotations

import re
import subprocess
import threading
import time
from dataclasses import dataclass
from typing import Sequence


_ADDRESS = re.compile(r"[0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5}\Z")
_DEVICE_LINE = re.compile(
    r"^\s*Device\s+([0-9A-Fa-f]{2}(?::[0-9A-Fa-f]{2}){5})(?:\s+(.*))?\s*$",
    re.IGNORECASE,
)
_ERROR_TEXT = re.compile(
    r"\b(?:failed|error|not available|not ready|no default controller|"
    r"not authorized|not permitted)\b",
    re.IGNORECASE,
)


class BluetoothctlError(RuntimeError):
    """An actionable failure returned by bluetoothctl or the OS."""


@dataclass(frozen=True)
class Device:
    address: str
    name: str


def normalize_address(address: str) -> str:
    """Validate an address before it is passed as a bluetoothctl argument."""
    candidate = address.strip()
    if not _ADDRESS.fullmatch(candidate):
        raise ValueError("Select a device with a valid Bluetooth address")
    return candidate.upper()


def parse_devices(output: str) -> list[Device]:
    devices: list[Device] = []
    for line in output.splitlines():
        match = _DEVICE_LINE.match(line)
        if match:
            devices.append(Device(match.group(1).upper(), (match.group(2) or "").strip()))
    return devices


class BluetoothctlClient:
    """Issue argument-vector commands; never interpolate input into a shell."""

    def __init__(
        self,
        executable: str = "bluetoothctl",
        command_timeout: int = 12,
    ) -> None:
        self.executable = executable
        self.command_timeout = command_timeout

    def _run(self, arguments: Sequence[str], timeout: int | None = None) -> str:
        command = [self.executable, *arguments]
        limit = timeout if timeout is not None else self.command_timeout
        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=limit,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            output = exc.stdout or exc.stderr or ""
            if isinstance(output, bytes):
                output = output.decode("utf-8", errors="replace")
            detail = str(output).strip()
            suffix = f": {detail}" if detail else ""
            raise BluetoothctlError(
                f"bluetoothctl timed out after {limit} seconds{suffix}"
            ) from exc
        except FileNotFoundError as exc:
            raise BluetoothctlError(
                "bluetoothctl was not found. Install BlueZ and ensure bluetoothctl is on PATH."
            ) from exc
        except OSError as exc:
            raise BluetoothctlError(f"Could not run bluetoothctl: {exc}") from exc

        output = "\n".join(part.strip() for part in (result.stdout, result.stderr) if part.strip())
        if result.returncode != 0 or _ERROR_TEXT.search(output):
            detail = output or f"bluetoothctl exited with status {result.returncode}"
            raise BluetoothctlError(detail)
        return output

    def controller_info(self) -> str:
        output = self._run(("show",))
        if not output:
            raise BluetoothctlError("bluetoothctl returned no controller information")
        return output

    def devices(self, paired_only: bool = False) -> list[Device]:
        arguments = ("devices", "Paired") if paired_only else ("devices",)
        return parse_devices(self._run(arguments))

    def device_info(self, address: str) -> str:
        safe_address = normalize_address(address)
        output = self._run(("info", safe_address))
        if not output:
            raise BluetoothctlError(f"No information was returned for {safe_address}")
        return output

    def _device_command(self, command: str, address: str, timeout: int | None = None) -> str:
        safe_address = normalize_address(address)
        return self._run((command, safe_address), timeout=timeout)

    def pair(self, address: str) -> str:
        return self._device_command("pair", address, timeout=45)

    def trust(self, address: str) -> str:
        return self._device_command("trust", address)

    def connect(self, address: str) -> str:
        return self._device_command("connect", address, timeout=30)

    def disconnect(self, address: str) -> str:
        return self._device_command("disconnect", address)

    def remove(self, address: str) -> str:
        return self._device_command("remove", address)

    def scan(self, duration: int) -> list[Device]:
        if isinstance(duration, bool) or not 1 <= duration <= 60:
            raise ValueError("Scan duration must be between 1 and 60 seconds")

        try:
            scanner = subprocess.Popen(
                [self.executable],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
                bufsize=1,
            )
        except FileNotFoundError as exc:
            raise BluetoothctlError(
                "bluetoothctl was not found. Install BlueZ and ensure bluetoothctl is on PATH."
            ) from exc
        except OSError as exc:
            raise BluetoothctlError(f"Could not start Bluetooth scan: {exc}") from exc

        output_lines: list[str] = []
        reader = threading.Thread(
            target=lambda: output_lines.extend(scanner.stdout or ()),
            daemon=True,
        )
        reader.start()
        start_error: OSError | None = None
        try:
            if scanner.stdin is None:
                raise BluetoothctlError("Could not open bluetoothctl input for scanning")
            try:
                scanner.stdin.write("scan on\n")
                scanner.stdin.flush()
                time.sleep(duration)
            except OSError as exc:
                start_error = exc
        finally:
            try:
                if scanner.stdin is not None:
                    try:
                        scanner.stdin.write("scan off\nquit\n")
                        scanner.stdin.flush()
                    except OSError:
                        pass
                    finally:
                        scanner.stdin.close()
            finally:
                try:
                    scanner.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    scanner.terminate()
                    try:
                        scanner.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        scanner.kill()
                        try:
                            scanner.wait(timeout=2)
                        except subprocess.TimeoutExpired as exc:
                            raise BluetoothctlError(
                                "Could not stop the Bluetooth scan process"
                            ) from exc
                reader.join(timeout=2)

        if start_error is not None:
            raise BluetoothctlError(f"Bluetooth scan command failed: {start_error}") from start_error
        output = "".join(output_lines)
        if scanner.returncode != 0 or _ERROR_TEXT.search(output):
            raise BluetoothctlError(output.strip() or "Bluetooth scan failed")
        return self.devices()
