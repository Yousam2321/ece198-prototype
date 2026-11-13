# ECE198 Group 23 — exercise monitoring prototype

Two independent Arduino programs and one Windows results app. Keep each sketch folder intact.

| Program | Exercises | Hardware / libraries |
|---|---|---|
| `PartA/PartA.ino` — A1.0 | Squats, lateral raises | Original analog accelerometer, 16×2 parallel LCD, START/EXERCISE buttons, MAX30102; LiquidCrystal and SparkFun MAX3010x |
| `HoldStation/HoldStation.ino` — B2.7 | Wall sit, plank, chest press | Uno R3, HC-SR04, OLED, GO/MODE buttons; U8g2 |
| `desktop/results_app.py` | Results from both boards | Windows executable, or Python with `requirements.txt` |

Part A is based on the team's uploaded **working two-exercise sketch**. Its detector functions, thresholds, LCD layout, pin mapping and button timings are preserved. The obsolete `WearableMonitor` implementation has been removed. Part B firmware is unchanged by the Part A integration. LED work remains deferred.

## Upload Part A

1. Install **LiquidCrystal by Arduino** and **SparkFun MAX3010x Pulse and Proximity Sensor Library** in Arduino IDE's Library Manager.
2. Open `PartA/PartA.ino`, keeping `PartASessions.h` alongside it.
3. Select your Part A board and its USB port, then Upload. Builds are checked for Uno R3 and Uno R4 Minima.
4. Keep the original working wiring. Pin assignments and tests are in [PART_A.md](docs/PART_A.md).

**Original controls:** briefly press/release START to start, pause or resume. Hold START for **1 second** to end. EXERCISE switches Squats / Lat Raises while stopped. A long START is handled while held; Part B has different finish timing.

**Added:** END saves before the original code clears the counts. Switching away from a started, paused exercise also saves before clearing it. Pausing alone does not finish/save a session. An unused exercise is not saved. The latest **20 Part A summaries** survive power cycles; newer saves replace the oldest. Unfinished RAM sessions are lost when power is removed. No automatic recovery or session calendar dates are claimed.

Each Part A record contains exercise, reps, active milliseconds excluding pauses, optional mean accepted BPM, beat-sample count, session ID and archive generation. The original LCD's rolling BPM display is preserved. Saved mean BPM uses accepted beats during active exercise only; no beats means **unavailable**, not zero.

## Use the Windows app

Download **ECE198-Results-Windows** from a successful [Build Windows results app](https://github.com/Yousam2321/ece198-prototype/actions/workflows/windows-app.yml) run, extract it, and double-click `dist/ECE198_Results.exe`. No Python installation is needed. Existing participant profiles remain at `%LOCALAPPDATA%\ECE198_Results\results.sqlite3`.

1. Choose a participant label and Open profile.
2. Open results CSV, or **Read Arduino**. Finish/save first and close Serial Monitor. Select either board's port; the app detects Part A or B. Connect each board separately to import both.
3. Keep only that participant's rows. Repeated imports skip identical records.
4. Enter targets for the loaded exercises, then **Review & confirm**.
5. Select an attempt for stats and reasons; use View progression, Clean report (PDF), or Export evaluated CSV.

| Exercise | Assessment |
|---|---|
| Wall sit / plank | In-band duration reaches the target **and** consistency reaches its requirement. Consistency = in-band / (active − final continuous departure). Pauses and final departure are excluded; earlier departures followed by return still count. |
| Chest press | Good-rep count reaches its target **and** good reps / completed attempts reaches its requirement. Firmware uses 80% press / 20% return thresholds. |
| Squats / lateral raises | Recorded rep count reaches its target. Active time and mean accepted BPM are supporting stats. The original Part A sketch does not record failed attempts or a good-rep percentage. |

Percentage defaults are editable prototype settings. Part A has no invented quality percentage or BPM-based verdict. Comparisons use the same exercise and current confirmed goals; board IDs are not calendar dates. CSV/report exports preserve raw measurements. Delete selected row removes one local row; Manage participants removes a complete profile after confirmation. Board saves and exported files are separate.

See [desktop/README.txt](desktop/README.txt) for details.

## Export saved sessions with PowerShell

Run commands from the **project root**, where `tools` and `requirements.txt` are visible. Replace COM10 with the connected board's actual port. Finish/save first and close Serial Monitor.

```powershell
python -m pip install -r requirements.txt
python tools/export_sessions.py --port COM10 --output ".\saved_sessions\saved_sessions_$(Get-Date -Format yyyyMMdd_HHmmss).csv"
```

Part A is detected automatically; `--part-a` can require it explicitly. On Part B the command above exports holds; add `--reps` for chest-press history. The app's Read Arduino reads both B archives in one connection. The exporter creates the chosen parent folder if needed, never overwrites an existing file, and does not erase the board. Export time is not session time.

At 115200 baud with Newline in Serial Monitor:

| Board | Commands |
|---|---|
| Part A | `version`, `history`, `erase CONFIRM` (history/erase only while stopped) |
| Part B | `help`, `version`, `history`, `rep_history`; button instructions below |

Part A history uses `ASAVED` rows and an explicit `# END_A_HISTORY` marker. The updated exporter and app support this format; old copies must be updated too. Part A erasure keeps the current RAM counters and the computer's profiles/CSVs. An erase increments archive generation so new records are distinct.

## Part B controls — B2.7

B2.7 updates the displayed version label only; exercise and session behaviour remains the same as B2.6.

GO short: start / pause / resume; from DONE, start another calibrated session. GO held at least 1.2 seconds and released while running/paused: finish and save. MODE selects/calibrates while stopped. While paused, hold MODE 2 seconds for Cancel session: GO confirms without saving, MODE returns. Wall sit/plank default to ±10 cm; chest press uses two-point calibration. See [EXERCISES.md](docs/EXERCISES.md).

Hold GO+MODE 3 seconds in IDLE/DONE to open Erase history. Release both, then GO confirms or MODE cancels. This clears both B archives: latest 20 holds and 10 chest sessions. The saved DONE display stays fixed. Keep power connected during any save/erase.

## Team updates and checks

Use [GitHub Desktop](docs/GIT_WORKFLOW.md) to clone once and pull updates instead of downloading ZIPs repeatedly. Pulling source does not update a board: open the matching sketch and click Upload. Do not upload Part A over the Part B board.

```sh
python tests/run_tests.py
python -m unittest discover -s tests -p 'test_*.py'
```

C++ tests require g++. Python tests require `requirements.txt`. CI compiles the firmware and builds/tests the Windows executable. [VALIDATION.md](VALIDATION.md) separates simulated/build checks from physical testing. No physical board is connected to CI; placement, echo quality, motion accuracy and pulse contact still need bench testing.

No license is chosen on the team's behalf. Third-party libraries are installed separately under their own licenses. Participant data and exports are excluded from Git.
