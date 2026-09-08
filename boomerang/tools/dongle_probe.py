#!/usr/bin/env python3
# Copyright (c) 2026 MCRI. All rights reserved.
#
# SPDX-License-Identifier: LicenseRef-Proprietary
#
# This software is the confidential and proprietary information of
# MCRI. It must not be used, copied, redistributed or disclosed
# without prior written authorisation.

"""dongle_probe.py: talk to the dongle from the laptop.

This is the other half of nearly every milestone check. It speaks the same verb
framing the firmware does, so if the two disagree, one of you is wrong and the
error message will usually say which.

It needs only pyserial:

    python3 -m venv .venv
    source .venv/bin/activate        # Windows: .\\.venv\\Scripts\\Activate.ps1
    pip install -r tools/requirements.txt

Usage:

    python tools/dongle_probe.py --selftest            # no hardware needed
    python tools/dongle_probe.py --list                # find your port
    python tools/dongle_probe.py --status              # M2
    python tools/dongle_probe.py --set-mode uart       # M2
    python tools/dongle_probe.py --echo                # M1
    python tools/dongle_probe.py --loopback 65536      # M3 (needs a D6-D7 jumper)
    python tools/dongle_probe.py --flood               # M4
    python tools/dongle_probe.py --wedge               # M6

Add --port /dev/cu.usbmodemXXXX (macOS or Linux) or --port COM7 (Windows) if
the automatic search picks the wrong device.
"""

from __future__ import annotations

import argparse
import struct
import sys
import time

# --------------------------------------------------------------------------- #
# The wire protocol. Mirror of src/ctrl/verb_frame.h -- keep the two in step.
# --------------------------------------------------------------------------- #

SYNC = 0xC0
HEADER_LEN = 6
CRC_LEN = 2
MAX_ARGS = 256

VERB_GET_STATUS = 0xD0
VERB_SET_MODE = 0xD1
VERB_DEBUG_STALL = 0xDF

MODE_NAMES = {0: "BLE", 1: "UART"}
BLE_STATE_NAMES = {
    0: "disabled",
    1: "idle",
    2: "scanning",
    3: "connecting",
    4: "connected",
}
UART_STATE_NAMES = {0: "down", 1: "ready", 2: "errors"}

FLAG_BRIDGE_LOSSY = 1 << 0
FLAG_WDT_RESET = 1 << 1

STATUS_FORMAT = "<BBBbBBBBIIIIIBBH"
STATUS_SIZE = struct.calcsize(STATUS_FORMAT)
assert STATUS_SIZE == 32, "status layout must match struct dongle_status"


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no final XOR."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode(verb: int, request_id: int, args: bytes = b"") -> bytes:
    if len(args) > MAX_ARGS:
        raise ValueError(f"argument block too long: {len(args)}")
    head = struct.pack("<BBHH", SYNC, verb, request_id & 0xFFFF, len(args)) + args
    return head + struct.pack("<H", crc16(head))


def decode(buf: bytes):
    """Try to decode one frame from the head of buf.

    Returns (frame_or_None, bytes_to_discard). Mirrors the firmware's parser
    including its re-synchronisation rule, so the two can be reasoned about
    together.
    """
    if len(buf) < 1:
        return None, 0
    if buf[0] != SYNC:
        return None, 1
    if len(buf) < HEADER_LEN:
        return None, 0

    _, verb, request_id, arg_len = struct.unpack("<BBHH", buf[:HEADER_LEN])
    if arg_len > MAX_ARGS:
        return None, 1

    total = HEADER_LEN + arg_len + CRC_LEN
    if len(buf) < total:
        return None, 0

    want = struct.unpack("<H", buf[total - CRC_LEN : total])[0]
    if crc16(buf[: total - CRC_LEN]) != want:
        return None, total

    return {
        "verb": verb,
        "request_id": request_id,
        "args": bytes(buf[HEADER_LEN : HEADER_LEN + arg_len]),
    }, total


def parse_status(args: bytes) -> dict:
    if len(args) < STATUS_SIZE:
        raise ValueError(f"status payload is {len(args)} bytes, expected {STATUS_SIZE}")
    fields = struct.unpack(STATUS_FORMAT, args[:STATUS_SIZE])
    keys = (
        "status_version", "mode", "ble_state", "ble_rssi_dbm", "uart_state",
        "flags", "fw_major", "fw_minor", "bytes_to_roo", "bytes_to_host",
        "drops_to_roo", "drops_to_host", "reset_cause", "breadcrumb_mode",
        "breadcrumb_fault", "reserved",
    )
    return dict(zip(keys, fields))


# --------------------------------------------------------------------------- #
# Serial plumbing
# --------------------------------------------------------------------------- #

def _serial():
    try:
        import serial  # noqa: F401
        from serial.tools import list_ports  # noqa: F401
    except ImportError:
        sys.exit("pyserial is not installed. Run: pip install -r tools/requirements.txt")
    import serial
    from serial.tools import list_ports
    return serial, list_ports


def find_port(explicit: str | None) -> str:
    serial, list_ports = _serial()
    if explicit:
        return explicit

    candidates = list(list_ports.comports())
    for p in candidates:
        if p.product and "CBPM dongle" in p.product:
            return p.device
    for p in candidates:
        if p.manufacturer and "MCRI" in p.manufacturer:
            return p.device

    listing = "\n".join(f"  {p.device}  {p.description}" for p in candidates)
    sys.exit(
        "Could not identify the dongle automatically. Pass --port explicitly.\n"
        "Ports seen:\n" + (listing or "  (none)")
    )


class Link:
    """One open serial port, plus a frame reassembly buffer."""

    def __init__(self, port: str, timeout: float = 1.0):
        serial, _ = _serial()
        self.port_name = port
        self.ser = serial.Serial(port, baudrate=115200, timeout=timeout)
        # Baud rate is meaningless for USB CDC-ACM (there is no UART behind
        # it), but pyserial insists on a number. Do not read anything into it.
        self.buf = bytearray()

    def close(self):
        self.ser.close()

    def send(self, data: bytes):
        self.ser.write(data)
        self.ser.flush()

    def request(self, verb: int, args: bytes = b"", request_id: int = 1,
                timeout: float = 2.0):
        """Send a verb and wait for a reply carrying the same request id."""
        self.send(encode(verb, request_id, args))
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            chunk = self.ser.read(256)
            if chunk:
                self.buf.extend(chunk)
            while True:
                frame, drop = decode(bytes(self.buf))
                if frame is None and drop == 0:
                    break
                del self.buf[:drop]
                if frame and frame["request_id"] == request_id:
                    return frame
        return None


# --------------------------------------------------------------------------- #
# Checks, one per milestone
# --------------------------------------------------------------------------- #

def cmd_selftest() -> int:
    """Prove this script agrees with the reference host implementation."""
    failures = []

    if crc16(b"123456789") != 0x29B1:
        failures.append("CRC check value is wrong")

    expect = bytes([0xC0, 0xD0, 0x01, 0x00, 0x00, 0x00, 0x76, 0xF1])
    if encode(VERB_GET_STATUS, 1) != expect:
        failures.append("get_status frame does not match the reference vector")

    frame, consumed = decode(expect)
    if frame is None or consumed != len(expect) or frame["verb"] != VERB_GET_STATUS:
        failures.append("round trip failed")

    if decode(b"\x00" + expect)[1] != 1:
        failures.append("re-synchronisation should discard exactly one byte")

    for f in failures:
        print(f"FAIL  {f}")
    if failures:
        return 1
    print(f"PASS  probe self-test ({STATUS_SIZE}-byte status layout, CRC, framing)")
    return 0


def cmd_list() -> int:
    _, list_ports = _serial()
    ports = list(list_ports.comports())
    if not ports:
        print("no serial ports found")
        return 1
    for p in ports:
        print(f"{p.device}\t{p.description}\t{p.manufacturer or ''}")
    return 0


def cmd_status(link: Link) -> int:
    frame = link.request(VERB_GET_STATUS)
    if frame is None:
        print("FAIL  no reply to DONGLE_GET_STATUS")
        print("      M2 not implemented yet, or the intercept never fires.")
        return 1

    s = parse_status(frame["args"])
    print(f"firmware      v{s['fw_major']}.{s['fw_minor']} "
          f"(status layout v{s['status_version']})")
    print(f"mode          {MODE_NAMES.get(s['mode'], s['mode'])}")
    print(f"BLE           {BLE_STATE_NAMES.get(s['ble_state'], s['ble_state'])}"
          f"{f', RSSI {s['ble_rssi_dbm']} dBm' if s['ble_state'] == 4 else ''}")
    print(f"UART          {UART_STATE_NAMES.get(s['uart_state'], s['uart_state'])}")
    print(f"host -> Roo   {s['bytes_to_roo']} bytes, {s['drops_to_roo']} dropped")
    print(f"Roo -> host   {s['bytes_to_host']} bytes, {s['drops_to_host']} dropped")
    print(f"reset cause   0x{s['reset_cause']:08x}"
          f"{'  WATCHDOG' if s['flags'] & FLAG_WDT_RESET else ''}")
    print(f"breadcrumb    mode {s['breadcrumb_mode']}, fault {s['breadcrumb_fault']}")
    if s["flags"] & FLAG_BRIDGE_LOSSY:
        print("WARNING       the bridge has dropped data since boot")
    return 0


def cmd_set_mode(link: Link, mode: str) -> int:
    value = 0 if mode == "ble" else 1
    frame = link.request(VERB_SET_MODE, bytes([value]))
    if frame is None:
        print("FAIL  no reply to DONGLE_SET_MODE")
        return 1
    s = parse_status(frame["args"])
    got = MODE_NAMES.get(s["mode"])
    if s["mode"] != value:
        print(f"FAIL  asked for {mode.upper()}, dongle reports {got}")
        return 1
    print(f"PASS  mode is now {got}")
    return 0


def cmd_echo(link: Link) -> int:
    """M1: whatever we send comes straight back."""
    payload = bytes(range(256)) * 4
    link.send(payload)

    got = bytearray()
    deadline = time.monotonic() + 3.0
    while len(got) < len(payload) and time.monotonic() < deadline:
        got.extend(link.ser.read(len(payload) - len(got)))

    if bytes(got) == payload:
        print(f"PASS  echoed {len(payload)} bytes byte-for-byte")
        return 0
    print(f"FAIL  sent {len(payload)} bytes, got {len(got)} back")
    if got:
        first = next((i for i, (a, b) in enumerate(zip(payload, got)) if a != b), None)
        if first is not None:
            print(f"      first difference at byte {first}")
    return 1


def cmd_loopback(link: Link, total: int) -> int:
    """M3: with D6 jumpered to D7, everything we send returns via the UART."""
    import os

    payload = os.urandom(total)
    got = bytearray()
    sent = 0
    chunk = 512
    deadline = time.monotonic() + 30.0

    while (sent < total or len(got) < total) and time.monotonic() < deadline:
        if sent < total:
            n = min(chunk, total - sent)
            link.send(payload[sent : sent + n])
            sent += n
        got.extend(link.ser.read(4096))

    if bytes(got) == payload:
        print(f"PASS  {total} bytes made the round trip unchanged")
        return 0

    print(f"FAIL  sent {total}, received {len(got)}")
    print("      Check the jumper between pads D6 and D7 first: with no wire")
    print("      the transmit half can be perfect and nothing comes back.")
    return 1


def cmd_flood(link: Link) -> int:
    """M4: overrun the bridge on purpose and insist that it says so."""
    before = link.request(VERB_GET_STATUS)
    if before is None:
        print("FAIL  cannot read status; finish M2 first")
        return 1
    b0 = parse_status(before["args"])

    blob = bytes(1024)
    for _ in range(64):
        link.send(blob)

    time.sleep(0.5)
    after = link.request(VERB_GET_STATUS, request_id=2)
    if after is None:
        print("FAIL  no status after the flood -- did the device wedge?")
        return 1
    b1 = parse_status(after["args"])

    moved = b1["bytes_to_roo"] - b0["bytes_to_roo"]
    dropped = b1["drops_to_roo"] - b0["drops_to_roo"]
    print(f"      {moved} bytes forwarded, {dropped} dropped during the flood")

    if dropped == 0:
        print("PASS? no drops. Either the bridge kept up, which is possible")
        print("      and good, or the counter is not wired up. Shrink")
        print("      CONFIG_CBPM_DONGLE_RING_SIZE and run this again: if the")
        print("      count still does not move, it is not counting.")
        return 0
    if not (b1["flags"] & FLAG_BRIDGE_LOSSY):
        print("FAIL  bytes were dropped but the lossy flag is clear")
        return 1
    print("PASS  loss was counted and flagged rather than hidden")
    return 0


def cmd_wedge(link: Link) -> int:
    """M6: ask the firmware to stall, and check it comes back and admits it."""
    print("      asking the dongle to stall a thread on purpose...")
    link.send(encode(VERB_DEBUG_STALL, 1))
    time.sleep(6.0)

    link.close()
    time.sleep(2.0)

    try:
        link2 = Link(link.port_name)
    except Exception as exc:  # the port disappears while the board reboots
        print(f"FAIL  the port did not come back: {exc}")
        return 1

    frame = link2.request(VERB_GET_STATUS)
    if frame is None:
        print("FAIL  no status after the expected reset")
        return 1
    s = parse_status(frame["args"])
    if s["flags"] & FLAG_WDT_RESET:
        print(f"PASS  watchdog reset recorded, breadcrumb mode "
              f"{s['breadcrumb_mode']}")
        return 0
    print(f"FAIL  the board is alive but reports reset cause "
          f"0x{s['reset_cause']:08x}, not a watchdog")
    return 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port; searched for if omitted")
    ap.add_argument("--selftest", action="store_true", help="check this script only")
    ap.add_argument("--list", action="store_true", help="list serial ports")
    ap.add_argument("--status", action="store_true", help="M2: read the status")
    ap.add_argument("--set-mode", choices=("ble", "uart"), help="M2: change mode")
    ap.add_argument("--echo", action="store_true", help="M1: echo round trip")
    ap.add_argument("--loopback", type=int, metavar="N",
                    help="M3: N bytes through the UART loopback jumper")
    ap.add_argument("--flood", action="store_true", help="M4: overrun on purpose")
    ap.add_argument("--wedge", action="store_true", help="M6: trigger the watchdog")
    args = ap.parse_args()

    if args.selftest:
        return cmd_selftest()
    if args.list:
        return cmd_list()

    link = Link(find_port(args.port))
    try:
        if args.status:
            return cmd_status(link)
        if args.set_mode:
            return cmd_set_mode(link, args.set_mode)
        if args.echo:
            return cmd_echo(link)
        if args.loopback:
            return cmd_loopback(link, args.loopback)
        if args.flood:
            return cmd_flood(link)
        if args.wedge:
            return cmd_wedge(link)
    finally:
        link.close()

    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
