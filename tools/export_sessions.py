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
           "trailing_out_ms", "trailing_out_seconds", "adjusted_active_ms",
           "adjusted_active_seconds", "has_hold", "history_id", "exported_at_utc"]


REP_COLUMNS=['session_id','exercise','active_ms','active_seconds','good_reps','attempts',
             'short_attempts','good_rep_percent','start_cm','pressed_cm','calibrated_travel_cm',
             'required_travel_cm','return_tolerance_cm','last_travel_cm','history_id','exported_at_utc']


def parse_rep_saved(line):
    fields=next(csv.reader([line]))
    if len(fields)!=10 or fields[0]!='REPSAVED' or fields[2]!='chest_press':
        raise ValueError('Incomplete chest-press summary; retry export.')
    identifier,active,good,attempts,generation=map(int,[fields[1],fields[3],fields[4],fields[5],fields[9]])
    start,pressed,last=map(float,fields[6:9]);travel=abs(pressed-start)
    if (identifier<1 or active<0 or generation<1 or not 0<=good<=attempts<=65535 or
            any(not math.isfinite(v) for v in (start,pressed,last)) or
            not 12<=start<=150 or not 12<=pressed<=150 or travel<10 or last<0):
        raise ValueError('Invalid chest-press summary; retry export.')
    return dict(session_id=identifier,exercise='chest_press',active_ms=active,
                active_seconds=f'{active/1000:.3f}',good_reps=good,attempts=attempts,
                short_attempts=attempts-good,good_rep_percent=f'{100*good/attempts:.1f}' if attempts else '',
                start_cm=f'{start:.1f}',pressed_cm=f'{pressed:.1f}',calibrated_travel_cm=f'{travel:.1f}',
                required_travel_cm=f'{travel*.8:.1f}',return_tolerance_cm=f'{travel*.2:.1f}',
                last_travel_cm=f'{last:.1f}',history_id=generation)


def read_rep_history(device,timeout=10,clock=time.monotonic):
    # A CSV is returned only after the explicit terminator or empty-history marker.
    device.write(b'rep_history\n');device.flush()
    deadline=clock()+timeout;started=False;pending=b'';rows={}
    while clock()<deadline:
        pending+=device.readline()
        if len(pending)>2048:raise ValueError('Unexpected serial data; check baud rate/board.')
        if not pending.endswith(b'\n'):continue
        line=pending.decode('utf-8',errors='replace').strip();pending=b''
        if line.startswith('# REPSAVED,'):started=True;rows={}
        elif started and line.startswith('REPSAVED,'):
            row=parse_rep_saved(line);previous=rows.get(row['session_id'])
            if previous is not None and previous!=row:raise ValueError('Conflicting chest session IDs; retry export.')
            rows[row['session_id']]=row
        elif started and line in ('# END_REP_HISTORY','# No saved rep sessions'):
            return [rows[key] for key in sorted(rows)]
        elif line.startswith(('EVENT,','REP_EVENT,')):
            if 'pause_before_history' in line:raise ValueError('Finish or pause before exporting.')
            if 'command_not_available' in line:raise ValueError('Upload B2.5 or newer firmware for chest-press export.')
    raise TimeoutError('No complete chest history received; no CSV was saved.')


def parse_saved(line):
    fields = next(csv.reader([line]))
    if len(fields) not in (7, 10) or fields[0] != "SAVED":
        raise ValueError("Incomplete saved session received; retry the export.")
    _, identifier, exercise, active, in_band, reference, band = fields[:7]
    identifier, active, in_band = int(identifier), int(active), int(in_band)
    reference, band = float(reference), float(band)
    if (identifier < 1 or exercise not in ("wall_sit", "plank") or
            not 0 <= in_band <= active or not math.isfinite(reference) or
            not math.isfinite(band) or reference < 0 or band < 0):
        raise ValueError("Invalid saved session received; retry the export.")
    row = dict(session_id=identifier, exercise=exercise, active_ms=active,
               in_band_ms=in_band, active_seconds=f"{active / 1000:.3f}",
               in_band_seconds=f"{in_band / 1000:.3f}", reference_cm=f"{reference:.1f}",
               band_cm=f"{band:.1f}", trailing_out_ms="", trailing_out_seconds="",
               adjusted_active_ms="", adjusted_active_seconds="", has_hold="", history_id="")
    if len(fields) == 10 and any(fields[7:]):
        tail, has_hold, generation = map(int, fields[7:])
        if (not 0 <= tail <= active or in_band > active-tail or
                has_hold not in (0, 1) or generation < 1):
            raise ValueError("Invalid end-of-session metadata.")
        row.update(trailing_out_ms=tail, trailing_out_seconds=f"{tail / 1000:.3f}",
                   adjusted_active_ms=active-tail, adjusted_active_seconds=f"{(active-tail)/1000:.3f}",
                   has_hold=has_hold, history_id=generation)
    return row


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


def write_csv(path, rows, reps=False):
    exported = datetime.datetime.now(datetime.timezone.utc).isoformat()
    path.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation protects existing exports. Excel-friendly UTF-8 BOM.
    with path.open("x", newline="", encoding="utf-8-sig") as out:
        writer = csv.DictWriter(out, fieldnames=REP_COLUMNS if reps else COLUMNS)
        writer.writeheader()
        for row in rows:
            writer.writerow(dict(row, exported_at_utc=exported))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--reps",action="store_true",help="Export saved chest-press sessions instead of holds.")
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
            rows = read_rep_history(device) if args.reps else read_history(device)
        write_csv(args.output, rows, reps=args.reps)
    except (serial.SerialException, OSError, ValueError, TimeoutError) as error:
        parser.exit(1, f"Export failed: {error}\n")
    print(f"Exported {len(rows)} session(s) to {args.output.resolve()}")
    print("The board's saved history was not erased. Export time is not session time.")


if __name__ == "__main__":
    main()
