"""Export Solution B's saved EEPROM summaries to a new CSV file.
Finish the current session first: opening the Uno's port can reset the board.
Close Arduino Serial Monitor before running this script.
Example: python tools/export_sessions.py --port COM4 --output saved_sessions.csv
"""
import argparse
import csv
import datetime
import math
from pathlib import Path
import time

COLUMNS = ["session_id", "exercise", "active_ms", "in_band_ms",
           "active_seconds", "in_band_seconds", "reference_cm", "band_cm",
           "exported_at_utc"]


def parse_saved(line):
    fields = next(csv.reader([line]))
    if len(fields) != 7 or fields[0] != "SAVED":
        raise ValueError("Incomplete saved session received; retry the export.")
    _, identifier, exercise, active, in_band, reference, band = fields
    identifier, active, in_band = int(identifier), int(active), int(in_band)
    reference, band = float(reference), float(band)
    if (identifier < 1 or exercise not in ("wall_sit", "plank") or
            not 0 <= in_band <= active or
            not math.isfinite(reference) or not math.isfinite(band) or
            reference < 0 or band < 0):
        raise ValueError("Invalid saved session received; retry the export.")
    return dict(session_id=identifier, exercise=exercise, active_ms=active,
                in_band_ms=in_band, active_seconds=f"{active / 1000:.3f}",
                in_band_seconds=f"{in_band / 1000:.3f}",
                reference_cm=f"{reference:.1f}", band_cm=f"{band:.1f}")


def read_history(device, timeout=10, clock=time.monotonic):
    """Read history only; ordinary live DATA is not a saved summary.

    Current firmware prints the complete history synchronously before its next
    DATA row. That DATA row terminates the response, including on older firmware
    without an explicit END_HISTORY marker. Timeout never exports a partial list.
    """
    device.write(b"history\n")
    device.flush()
    deadline = clock() + timeout
    started, pending, rows = False, b"", {}
    while clock() < deadline:
        pending += device.readline()
        if len(pending) > 2048:
            raise ValueError("Unexpected serial data; check the baud rate/board.")
        if not pending.endswith(b"\n"):
            continue
        line = pending.decode("utf-8", errors="replace").strip()
        pending = b""
        if line.startswith("# SAVED,"):
            started = True
            rows = {}
        elif started and line.startswith("SAVED,"):
            row = parse_saved(line)
            previous = rows.get(row["session_id"])
            if previous is not None and previous != row:
                raise ValueError("Conflicting session IDs received; retry export.")
            rows[row["session_id"]] = row
        elif started and (line == "# No saved sessions" or
                          line == "# END_HISTORY" or
                          line.startswith("DATA,station,")):
            return [rows[key] for key in sorted(rows)]
        elif line.startswith("EVENT,") and "pause_before_history" in line:
            raise ValueError("Finish or pause the session before exporting.")
        elif line.startswith("EVENT,") and "command_not_available" in line:
            raise ValueError("Upload the Solution B firmware with history support first.")
    raise TimeoutError("No complete history received. Check the port and firmware; no CSV was saved.")


def write_csv(path, rows):
    exported = datetime.datetime.now(datetime.timezone.utc).isoformat()
    path.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation protects existing exports. Excel-friendly UTF-8 BOM.
    with path.open("x", newline="", encoding="utf-8-sig") as out:
        writer = csv.DictWriter(out, fieldnames=COLUMNS)
        writer.writeheader()
        for row in rows:
            writer.writerow(dict(row, exported_at_utc=exported))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", type=Path,
                        default=Path("saved_sessions_" +
                                     datetime.datetime.now().strftime("%Y%m%d_%H%M%S") + ".csv"))
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output already exists. Pick a different filename.")
    try:
        import serial
    except ImportError:
        parser.exit(1, "Install dependencies: python -m pip install -r requirements.txt\n")
    try:
        print("Connecting. The Uno may restart; completed saved sessions are retained.")
        with serial.Serial(args.port, 115200, timeout=0.25) as device:
            time.sleep(2)  # Allow the Uno bootloader/auto-reset to finish.
            device.reset_input_buffer()
            rows = read_history(device)
        write_csv(args.output, rows)
    except (serial.SerialException, OSError, ValueError, TimeoutError) as error:
        parser.exit(1, f"Export failed: {error}\n")
    print(f"Exported {len(rows)} session(s) to {args.output.resolve()}")
    print("The board's saved history was not erased. Export time is not session time.")


if __name__ == "__main__":
    main()
