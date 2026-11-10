"""Tk desktop interface for Bluetooth device management."""

from __future__ import annotations

import queue
import threading
import tkinter as tk
from dataclasses import dataclass
from tkinter import messagebox, ttk
from typing import Callable

from .bluetoothctl import BluetoothctlClient, BluetoothctlError, Device
from .config import AppConfig


@dataclass(frozen=True)
class DeviceSnapshot:
    controller: str
    devices: list[Device]
    paired_addresses: set[str]


class BluetoothWindow:
    def __init__(self, root: tk.Tk, config: AppConfig) -> None:
        self.root = root
        self.config = config
        self.client = BluetoothctlClient(command_timeout=config.command_timeout)
        self.results: queue.Queue[tuple[str, object | None, Exception | None]] = queue.Queue()
        self.busy = False

        root.title("GnuChanBluetooth")
        root.geometry("760x510")
        root.minsize(580, 380)

        outer = ttk.Frame(root, padding=12)
        outer.pack(fill="both", expand=True)
        outer.columnconfigure(0, weight=1)
        outer.rowconfigure(2, weight=1)

        ttk.Label(outer, text="Bluetooth controller", font=("", 11, "bold")).grid(
            row=0, column=0, sticky="w"
        )
        self.controller_text = tk.StringVar(value="Loading controller status…")
        ttk.Label(
            outer,
            textvariable=self.controller_text,
            justify="left",
            wraplength=700,
        ).grid(row=1, column=0, sticky="ew", pady=(4, 10))

        table_frame = ttk.Frame(outer)
        table_frame.grid(row=2, column=0, sticky="nsew")
        table_frame.columnconfigure(0, weight=1)
        table_frame.rowconfigure(0, weight=1)
        self.devices_table = ttk.Treeview(
            table_frame,
            columns=("address", "name", "paired"),
            show="headings",
            selectmode="browse",
        )
        self.devices_table.heading("address", text="Address")
        self.devices_table.heading("name", text="Device")
        self.devices_table.heading("paired", text="Paired")
        self.devices_table.column("address", width=170, stretch=False)
        self.devices_table.column("name", width=350)
        self.devices_table.column("paired", width=75, stretch=False, anchor="center")
        self.devices_table.grid(row=0, column=0, sticky="nsew")
        scrollbar = ttk.Scrollbar(
            table_frame, orient="vertical", command=self.devices_table.yview
        )
        scrollbar.grid(row=0, column=1, sticky="ns")
        self.devices_table.configure(yscrollcommand=scrollbar.set)

        controls = ttk.Frame(outer)
        controls.grid(row=3, column=0, sticky="ew", pady=(10, 4))
        self.buttons: list[ttk.Button] = []
        self._button(controls, "Refresh", self.refresh)
        self._button(controls, "Scan", self.scan)
        self._button(controls, "Inspect", self.inspect)
        self._button(controls, "Pair", lambda: self.device_action("pair"))
        self._button(controls, "Trust", lambda: self.device_action("trust"))
        self._button(controls, "Connect", lambda: self.device_action("connect"))
        self._button(controls, "Disconnect", lambda: self.device_action("disconnect"))
        self._button(controls, "Remove", self.remove)

        self.status_text = tk.StringVar(value="Ready")
        self.status_label = ttk.Label(outer, textvariable=self.status_text)
        self.status_label.grid(row=4, column=0, sticky="ew", pady=(5, 0))
        self.details = tk.Text(outer, height=5, wrap="word", state="disabled")
        self.details.grid(row=5, column=0, sticky="ew", pady=(5, 0))
        self.root.after(100, self._poll_results)
        self.refresh()

    def _button(self, parent: ttk.Frame, label: str, command: Callable[[], None]) -> None:
        button = ttk.Button(parent, text=label, command=command)
        button.pack(side="left", padx=(0, 5))
        self.buttons.append(button)

    def _submit(self, label: str, work: Callable[[], object]) -> None:
        if self.busy:
            return
        self.busy = True
        self.status_text.set(f"{label}…")
        self.status_label.configure(foreground="")
        for button in self.buttons:
            button.configure(state="disabled")

        def run() -> None:
            try:
                self.results.put((label, work(), None))
            except Exception as exc:
                self.results.put((label, None, exc))

        threading.Thread(target=run, daemon=True).start()

    def _poll_results(self) -> None:
        try:
            label, result, error = self.results.get_nowait()
        except queue.Empty:
            self.root.after(100, self._poll_results)
            return

        self.busy = False
        for button in self.buttons:
            button.configure(state="normal")
        if error is not None:
            self.status_text.set(f"{label} failed: {error}")
            self.status_label.configure(foreground="#b00020")
            messagebox.showerror(f"{label} failed", str(error), parent=self.root)
        else:
            self.status_text.set(f"{label} completed")
            self.status_label.configure(foreground="")
            if isinstance(result, DeviceSnapshot):
                self._display_snapshot(result)
            elif isinstance(result, str):
                self._display_details(result)
        self.root.after(100, self._poll_results)

    def _get_snapshot(self) -> DeviceSnapshot:
        controller = self.client.controller_info()
        devices = self.client.devices()
        paired = {device.address for device in self.client.devices(paired_only=True)}
        return DeviceSnapshot(controller, devices, paired)

    def _display_snapshot(self, snapshot: DeviceSnapshot) -> None:
        self.controller_text.set(snapshot.controller)
        for item in self.devices_table.get_children():
            self.devices_table.delete(item)
        for device in snapshot.devices:
            self.devices_table.insert(
                "",
                "end",
                iid=device.address,
                values=(
                    device.address,
                    device.name or "(unnamed device)",
                    "Yes" if device.address in snapshot.paired_addresses else "No",
                ),
            )

    def _display_details(self, text: str) -> None:
        self.details.configure(state="normal")
        self.details.delete("1.0", "end")
        self.details.insert("1.0", text)
        self.details.configure(state="disabled")

    def _selected_address(self) -> str | None:
        selected = self.devices_table.selection()
        return selected[0] if selected else None

    def refresh(self) -> None:
        self._submit("Refresh", self._get_snapshot)

    def scan(self) -> None:
        def work() -> DeviceSnapshot:
            self.client.scan(self.config.scan_duration)
            return self._get_snapshot()

        self._submit(f"Scan ({self.config.scan_duration}s)", work)

    def inspect(self) -> None:
        address = self._selected_address()
        if address is None:
            messagebox.showinfo("Inspect device", "Select a device first.", parent=self.root)
            return
        self._submit(f"Inspect {address}", lambda: self.client.device_info(address))

    def device_action(self, action: str) -> None:
        address = self._selected_address()
        if address is None:
            messagebox.showinfo(action.title(), "Select a device first.", parent=self.root)
            return
        operation = getattr(self.client, action)

        def work() -> DeviceSnapshot:
            operation(address)
            return self._get_snapshot()

        self._submit(action.title(), work)

    def remove(self) -> None:
        address = self._selected_address()
        if address is None:
            messagebox.showinfo("Remove device", "Select a device first.", parent=self.root)
            return
        if not messagebox.askyesno(
            "Remove device",
            f"Remove {address} from this controller?",
            parent=self.root,
        ):
            return
        self.device_action("remove")
