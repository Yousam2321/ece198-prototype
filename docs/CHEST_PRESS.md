# Chest press - Solution B B2.6

Same Uno R3, HC-SR04, OLED and two buttons. No new wiring or parts.
The updated desktop app supports chest press. The LED extension is still deferred.

## Install

1. Close Serial Monitor and the results app so they release the COM port.
2. Extract the ZIP. Open `HoldStation/HoldStation.ino` in Arduino IDE.
   Keep the other files and the `exercises` subfolder beside it.
3. Choose **Arduino Uno** and the board's port (previously COM10), then **Upload**.
   The existing U8g2 library is still required. The default OLED driver is SH1106;
   keep `OLED_SH1106=0` in `Config.h` if your working display needs SSD1306.
4. MODE now cycles **Wall sit -> Plank -> Chest press -> Wall sit**.
   The screen shows version B2.6 for all three exercises.

## Calibrate using only buttons

Mount the sensor along the hand's path and leave it fixed throughout the session.
Either increasing or decreasing hand distance works.

1. Select **CHEST PRESS** with short MODE presses while idle or finished.
2. Put the hand in its **starting position**. Hold MODE for at least **1.2 seconds**,
   release, then hold the hand still during **CAL START 2s**.
3. The display changes to **Step 2/2: PRESS** / **Hold MODE at END**.
4. Move the hand to the **fully pressed position**. Hold MODE again for at least
   **1.2 seconds**, release and stay still during **CAL PRESS 2s**.
5. The display shows the required **Goal ...cm (80%)**. Return to the starting pose.
   Short GO begins the normal three-second countdown.

The two reference distances must be stable, between 12 and 150 cm, and at least
10 cm apart. A failed second capture can be retried without repeating the first.
Holding MODE again after successful calibration restarts the two-step calibration.
Changing exercise or power cycling requires calibration again. A new attempt in
this same mode retains both reference distances.

## Exactly what counts

Let calibrated travel be the distance between the two recorded positions.

- A good rep must reach **at least 80% of that travel**, then return to **within
  20% of the travel on either side of the starting position**.
- Both the pressed threshold and return are confirmed by **two consecutive valid
  filtered readings**. A held endpoint cannot count extra reps.
- **Good 3/4** means **3 good reps out of 4 completed attempts**.
- A meaningful attempt starts once movement reaches **30% of calibrated travel**.
  Returning early counts as a short attempt, but earns **no good rep**. Smaller
  adjustments are ignored. There is no minimum speed or maximum rep duration.
- **Last 25/24cm** means the last completed attempt travelled 25 cm; the minimum
  required press distance was 24 cm. Values shown on the OLED are rounded;
  counting uses the full unrounded measurements.
- Missing echoes and pauses discard the **unfinished** rep. Completed counts stay.
  After either interruption, return to the start before trying another rep.
- One distance sensor observes one target's distance; it does not check both arms,
  weight, joint angles or complete exercise posture.

Example: start at **80 cm**, fully pressed at **50 cm** -> calibrated travel **30 cm**.
A good rep reaches **56 cm or nearer**, then returns into **74-86 cm**. Stay on the
same side of the sensor and follow the calibrated direction.

## Controls and saving

- **Short GO:** start / pause / resume.
- **Hold GO >=1.2s and release:** finish and save. Finish works while running or
  paused. An unfinished rep is discarded. The completed screen stays fixed.
- **Short GO after DONE:** fresh countdown and zero rep counters; calibration kept.
- **Short MODE after DONE:** switch exercise; calibration cleared.
- Existing button-only erase confirmation still works: from idle/done, hold both
  buttons for 3 seconds, release both, then GO confirms or MODE cancels.
  It now clears **both hold and chest-press archives**. Exported PC files stay.

The Arduino retains the **latest 20 hold sessions and latest 10 chest-press
sessions**, independently. A new chest-press save does not overwrite a hold.
Chest records retain active time, good/attempt counts, both calibrated distances,
last completed travel and archive generation. Saves survive a power cycle.
An interrupted save is not reported as a valid record.

The updated app imports/evaluates all three exercises and reads both archives.
It also opens raw chest-press CSVs. For a technical inspection,
Serial Monitor at **115200**, New Line:

- `chest` selects chest press; `cal`, `go`, `finish` match the buttons.
- `rep_history` prints the saved chest-press summaries (without erasing anything).
- `history` continues to print **only hold summaries**, for the existing app.
- `version` prints `# ECE198_B_2.6`.

Chest live rows have a separate prefix and column meaning:

```text
REP_DATA,station,uptime_ms,chest_press,state,active_ms,distance_cm,start_cm,pressed_cm,good_reps,attempts,last_travel_cm,note
REPSAVED,id,chest_press,active_ms,good_reps,attempts,start_cm,pressed_cm,last_travel_cm,history_id
```

## Quick physical test

Use a flat target moved by hand for a repeatable first bench check, then test the
actual hand position. No weights are needed for this check.

| Test | Expected |
|---|---|
| Calibrate 80 cm start / 50 cm press | Goal about 24 cm; GO enabled only after both captures |
| Start -> 60 cm -> start | No good rep; one short attempt |
| Start -> 50 cm -> start | Exactly one good rep |
| Stay at 50 cm for several seconds | No extra reps |
| Repeat three full cycles | Good count increases by three |
| Press, pause, return, resume | Unfinished rep discarded; earlier good reps kept |
| Press, interrupt echo, return | No false good rep; return to start to re-arm |
| Finish, then move the target | DONE results stay fixed |
| Short GO after finishing | Counts zeroed; fresh countdown; references kept |
| Finish/save, power off/on, `rep_history` | Saved chest-press session still present |
| Check old holds in app | Existing wall-sit/plank records still import |
| Switch modes | All three modes select; recalibration required |

Automated validation runs the actual firmware loop against simulated echo pulses,
button input and EEPROM, plus focused threshold/noise/dropout tests. Uno builds
cover SH1106, SSD1306 and Serial-only configurations. Physical validation must
still be done on the assembled prototype.

## Cancel without saving (B2.6, all exercises)

1. Tap GO to pause.
2. Hold MODE for at least **2 seconds**, then release.
3. At **CANCEL SESSION?**, tap GO to discard or MODE to return to PAUSED.

Discard resets current timers and counts, keeps calibration, and never writes to
or clears saved EEPROM history. CANCELLED / NOT SAVED confirms the action.
Short GO then starts a fresh countdown. A long GO still finishes and saves during
a normal running/paused session, but is ignored inside the cancel confirmation.
The chest heading is shortened to CHEST B2.6 to leave more room on the first row.

## Export chest sessions to CSV in PowerShell

Close Serial Monitor and the app. From the **project root** (the folder containing
`tools`, not inside `HoldStation`), run:

```powershell
python tools/export_sessions.py --port COM10 --reps --output ".\saved_sessions\saved_sessions_$(Get-Date -Format yyyyMMdd_HHmmss).csv"
```

The CSV includes good/attempt counts, short attempts, good-rep percentage, active
time, reference positions, travel thresholds and the last completed travel.
Zero attempts gives a blank percentage. Export does not erase board history.
Omit `--reps` to export wall-sit/plank records using the existing format. These raw
chest CSVs open in Excel or the updated Exercise Results app.
