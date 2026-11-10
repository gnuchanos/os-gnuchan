# GnuChanBluetooth

GnuChanBluetooth is a small Tk desktop front end for BlueZ's `bluetoothctl`.
It inspects the default controller and known devices, scans for nearby devices,
and offers pair, trust, connect, disconnect, inspect, and remove actions.

## Requirements

- Linux with BlueZ and `bluetoothctl` installed and a working user session.
- Python 3.10+ with Tkinter available.
- The current user must have permission to use the Bluetooth controller. The
  application does not request root access or change system permissions.

Pairing may require a confirmation, PIN, or passkey prompt from the desktop's
Bluetooth agent. GnuChanBluetooth does not install or replace an agent; when
BlueZ cannot complete that flow, the returned error is shown in the UI.

## Run from the checkout

```sh
python3 -m gnuchan_bluetooth
```

`--help` and `--version` are available without starting the window:

```sh
python3 -m gnuchan_bluetooth --help
python3 -m gnuchan_bluetooth --version
```

## Install and uninstall

The scripts are noninteractive and install only for the current user. They do
not use `sudo`, install dependencies, or modify distro/system files.

```sh
./scripts/install.sh
./scripts/uninstall.sh
```

The default executable/library prefix is `~/.local`; the desktop entry is
installed to `${XDG_DATA_HOME:-~/.local/share}/applications`. Override with
`PREFIX` and/or `XDG_DATA_HOME` for a different user-local location. Uninstall
removes the application files and desktop entry, but preserves configuration.
The installed command is named `GnuChanBluetooth`, like the other GnuChan
programs.

## Configuration

The optional JSON file is read from
`${XDG_CONFIG_HOME:-~/.config}/GnuChanBluetooth/config.json`. A different file
can be selected with `--config FILE`. Supported settings are:

```json
{
  "command_timeout": 12,
  "scan_duration": 8
}
```

Both values are integer seconds from 1 to 60. Defaults are used when the
configuration file does not exist. Invalid configuration is reported instead
of silently ignored.

## Safety and errors

- Bluetooth device identity is accepted only as a six-octet MAC address;
  displayed device names are never used as command arguments.
- BlueZ commands run as argument vectors without a shell. Every command has a
  finite timeout, including pairing and connection operations.
- A scan is explicitly stopped and its client process is reaped, even if an
  error occurs.
- Command failures and timeouts are displayed in the window and reported in a
  dialog. No command is run automatically on startup except read-only status
  inspection.
- Removing a device requires a confirmation.

## Tests

Run the unit tests and static checks with:

```sh
make build check test
```

Tests mock BlueZ so they do not scan, pair, or modify real devices.
