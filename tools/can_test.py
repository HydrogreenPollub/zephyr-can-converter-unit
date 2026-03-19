"""
CCU CAN connectivity test — Candlelight USB adapter.

Listens on the CAN bus for a configurable duration and reports all frames
received from the CCU. Also sends a single DFU REQUEST frame so you can
verify the CCU receives and ACKs it (without committing any firmware).

Expected CCU periodic frames (if device is running):
  0x100  MCU_STATE         every 100 ms
  0x102  MCU_INPUTS        every 100 ms
  0x103  MCU_ANALOG_PEDALS every 100 ms
  0x104  MCU_ANALOG_FUEL_CELL every 100 ms
  0x105  MCU_ANALOG_POWERTRAIN every 100 ms
  0x106  MCU_ANALOG_ACCESSORY  every 100 ms
  0x10F  CCU_STATUS        every 1000 ms
  0x7E2  DFU_RSP           in reply to a DFU REQUEST

Usage:
    python can_test.py
    python can_test.py --bitrate 250000
    python can_test.py --listen-only        # do not send DFU REQUEST
    python can_test.py --duration 10        # listen for 10 s

Requirements:
    pip install python-can[gs_usb] libusb
"""

import argparse
import struct
import time
import sys
from collections import defaultdict

try:
    import can
except ImportError:
    print("Error: python-can not installed. Run: pip install python-can[gs_usb] libusb")
    sys.exit(1)


def _patch_libusb() -> None:
    try:
        import libusb as _libusb_pkg
        import usb.backend.libusb1 as _lb1
        dll_path = _libusb_pkg.dll._name
        _orig = _lb1.get_backend
        _lb1.get_backend = lambda find_library=None: _orig(
            find_library=lambda _: dll_path
        )
    except Exception:
        pass


_patch_libusb()

# ── Known frame IDs ────────────────────────────────────────────────────────────
_KNOWN_IDS = {
    0x100: "MCU_STATE",
    0x101: "MCU_FAULTS",
    0x102: "MCU_INPUTS",
    0x103: "MCU_ANALOG_PEDALS",
    0x104: "MCU_ANALOG_FUEL_CELL",
    0x105: "MCU_ANALOG_POWERTRAIN",
    0x106: "MCU_ANALOG_ACCESSORY",
    0x107: "MCU_ANALOG_DRIVE",
    0x108: "MCU_ANALOG_UNASSIGNED",
    0x110: "MCU_TIME_SYNC",
    0x10F: "CCU_STATUS",
    0x200: "PROTIUM_POWER",
    0x201: "PROTIUM_THERMAL",
    0x202: "PROTIUM_HYDROGEN",
    0x203: "PROTIUM_SETPOINTS",
    0x204: "PROTIUM_STASIS",
    0x205: "PROTIUM_MISC",
    0x206: "PROTIUM_STATE",
    0x7E2: "DFU_RSP",
}

_CCU_STATUS_NAMES = {0: "INIT", 1: "OPERATIONAL", 2: "WARNING", 3: "BUS_OFF", 4: "DFU"}
_DFU_STATUS_NAMES = {0: "OK", 1: "CRC_FAIL", 2: "WRITE_FAIL", 3: "SEQ_FAIL"}

DFU_CMD_ID  = 0x7E0
DFU_DATA_ID = 0x7E1
DFU_RSP_ID  = 0x7E2
CMD_REQUEST = 0x01


def _decode_frame(msg: "can.Message") -> str:
    fid = msg.arbitration_id
    raw = " ".join(f"{b:02X}" for b in msg.data[:msg.dlc])

    if fid == 0x10F and msg.dlc >= 1:
        state = msg.data[0] & 0x0F
        return f"status={_CCU_STATUS_NAMES.get(state, f'?({state})')}"
    if fid == DFU_RSP_ID and msg.dlc >= 1:
        code = msg.data[0]
        return f"dfu_status={_DFU_STATUS_NAMES.get(code, f'?({code:02X})')}"
    if fid == 0x100 and msg.dlc >= 1:
        status = msg.data[0] & 0x0F
        states = {0: "INIT", 1: "IDLE", 2: "STARTING", 3: "SHUTTING_DOWN", 4: "RUNNING"}
        return f"status={states.get(status, f'?({status})')}"
    return raw


def run_test(bus: "can.BusABC", duration: float, send_dfu_request: bool) -> None:
    counts: dict[int, int] = defaultdict(int)
    first_seen: dict[int, float] = {}
    last_data: dict[int, bytes] = {}
    dfu_ack_received = False
    t0 = time.monotonic()

    if send_dfu_request:
        print("→ Sending DFU REQUEST (0-byte image, just to test ACK) ...", end=" ", flush=True)
        data = bytes([CMD_REQUEST]) + struct.pack("<I", 0)
        bus.send(can.Message(arbitration_id=DFU_CMD_ID, is_extended_id=True, data=data))
        print("sent")

    print(f"\n→ Listening for {duration:.0f} s ...\n")

    while time.monotonic() - t0 < duration:
        remaining = duration - (time.monotonic() - t0)
        msg = bus.recv(timeout=min(remaining, 0.2))
        if msg is None:
            continue

        fid = msg.arbitration_id
        now = time.monotonic() - t0
        counts[fid] += 1
        if fid not in first_seen:
            first_seen[fid] = now
        last_data[fid] = bytes(msg.data[:msg.dlc])

        label = _KNOWN_IDS.get(fid, f"0x{fid:03X}")
        decoded = _decode_frame(msg)
        print(f"  [{now:6.2f}s] {label:<28} id=0x{fid:03X}  dlc={msg.dlc}  {decoded}")

        if fid == DFU_RSP_ID and not dfu_ack_received:
            dfu_ack_received = True

    elapsed = time.monotonic() - t0
    print(f"\n── Summary ({elapsed:.1f} s) ──────────────────────────────────────")
    if not counts:
        print("  No frames received. Is the device powered and the CAN bus connected?")
    else:
        print(f"  {'Frame':<28} {'ID':>6}  {'Count':>6}  {'Rate /s':>8}  Last data")
        for fid in sorted(counts):
            label = _KNOWN_IDS.get(fid, "unknown")
            cnt = counts[fid]
            rate = cnt / elapsed
            raw = " ".join(f"{b:02X}" for b in last_data.get(fid, b""))
            print(f"  {label:<28} 0x{fid:03X}  {cnt:>6}  {rate:>7.1f}/s  {raw}")

    if send_dfu_request:
        print()
        if dfu_ack_received:
            print("  DFU REQUEST ACK: ✓ received (0x7E2 frame seen)")
        else:
            print("  DFU REQUEST ACK: ✗ NOT received (no 0x7E2 frame)")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="CCU CAN connectivity test (Candlelight / gs_usb)",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("--bitrate", "-b", type=int, default=500000,
                        help="CAN bitrate in bps")
    parser.add_argument("--index", type=int, default=0,
                        help="Adapter index (0 = first Candlelight adapter)")
    parser.add_argument("--duration", "-d", type=float, default=5.0,
                        help="Listen duration in seconds")
    parser.add_argument("--listen-only", action="store_true",
                        help="Do not send a DFU REQUEST, only listen")
    args = parser.parse_args()

    print("CCU CAN connectivity test  (Candlelight / gs_usb)")
    print(f"  Adapter index : {args.index}")
    print(f"  Bitrate       : {args.bitrate} bps")
    print(f"  Duration      : {args.duration} s")
    print(f"  Send DFU req  : {'no' if args.listen_only else 'yes'}")
    print()

    try:
        with can.Bus(interface="gs_usb", channel=0,
                     bitrate=args.bitrate, index=args.index) as bus:
            run_test(bus, args.duration, send_dfu_request=not args.listen_only)
    except can.CanError as e:
        print(f"\nCAN bus error: {e}")
        if "NoBackendError" in str(e) or "No backend" in str(e):
            print("Hint: pip install libusb")
        sys.exit(1)
    except Exception as e:
        if "NoBackendError" in type(e).__name__ or "No backend" in str(e):
            print(f"\nError: libusb backend not found. Fix: pip install libusb")
            sys.exit(1)
        raise


if __name__ == "__main__":
    main()
