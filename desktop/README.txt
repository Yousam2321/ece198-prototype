ECE198 Exercise Results - Windows

Close the old app, extract the download and double-click dist/ECE198_Results.exe.
No Python installation is needed for the executable. Keep B2.5/B2.6 firmware on
Part B. Upload the new PartA/PartA.ino to Part A for its added EEPROM/history support.
Existing profiles, history and hold criteria remain in the same local database:
%LOCALAPPDATA%\ECE198_Results\results.sqlite3

1. Enter a participant label and Open profile.
2. Open results CSV (Part A, hold or chest exports), or Read Arduino. Close Serial Monitor
   first. Read Arduino auto-detects A/B, reading both B archives when relevant. Select only
   that participant's records and Keep selected sessions. Duplicate imports skip.
   Older firmware without chest support still supplies its hold records.
3. Enter targets only for the exercises loaded in this profile. Hold targets use
   seconds; chest targets use whole good-rep counts; Part A squats/lateral raises
   use whole recorded-rep counts. The two percentage fields
   are separate: hold consistency and chest good-rep percentage. Both start at
   an editable 90%; this is a prototype setting, not a clinical recommendation.
4. Review & confirm shows all criteria before scoring. Confirmed inputs collapse
   to leave room for results; Change criteria opens them again. Changing criteria
   or importing more data requires confirmation again.
5. Select a session. The summary names its exercise and shows the precise reason
   if it needs more time, good reps, consistency, good-rep percentage, or both.
6. View progression shows the selected exercise's percentage (B) or rep count (A). Clean report (PDF)
   includes separate exercise sections, stats, goals, reasons, time/rep graphs,
   and comparisons with previous/first sessions. Changed chest travel is flagged.
7. Export evaluated CSV includes raw data, derived scores, criteria and feedback.
   It can be re-imported without duplicating records. The PowerShell original CSV
   remains a raw-results export. Choose new names; reports never overwrite files.
8. Visible rows selects 7 or 10. Scroll the window on smaller screens.
9. Manage participants searches profiles and deletes a complete local profile,
   criteria and attempts only after confirmation. Exported files and board saves
   stay untouched. Search is bounded for large participant lists.

Hold evaluation (unchanged): BOTH in-zone duration and corrected consistency must
reach the confirmed goals. Consistency = in-band / (active - final departure).
Pauses and the last continuous departure are excluded; earlier excursions followed
by return remain. Legacy records without ending metadata are preserved, unscored.

Chest evaluation: BOTH good-rep count and good-rep percentage must reach the goals.
Good-rep percentage = good reps / completed attempts. Short attempts count against
it. Zero completed attempts is Cannot evaluate. The firmware requires 80% travel
and return within 20%, with endpoint confirmation. Unfinished reps discarded on
pause/dropout/finish are not recorded as attempts. Extra travel is allowed without
a maximum-distance penalty. Active duration and travel are shown, not scored.
Comparisons flag changed calibrated travel; they describe different movement
conditions rather than a normalized improvement score. No hidden weighted score.

Part A evaluation: the recorded rep count must reach its confirmed goal. A good
result means that count goal was met, not that full movement form was assessed.
The original detector does not save failed attempts, so there is no good-rep
percentage or hold consistency for A. Active time excludes pauses. Mean BPM uses
accepted beats during active exercise; missing beats display Unavailable. BPM
and active time do not change the verdict. Graphs compare the same exercise only.
Part A saves on long START (3 seconds) and before switching away from a paused,
started exercise. The latest 20 completed sessions remain in the board's EEPROM.
Use the updated exporter for ASAVED CSVs; older copies do not know this format.

Session numbers are per exercise, not dates or shared board IDs. Import/export
and report times are not session times. These measurements do not check both arms,
weights or full-body exercise form. Data stays on this computer; the app sends no
participant data to a server. Close the app before backing up its SQLite database.

Quick check: with a record of 9 good out of 12, goal 9 and 75% gives Good. Raising
those goals to 10 and 90% gives Not good yet - rep count + good-rep %.

Source dependencies: python -m pip install pyserial==3.5 reportlab==4.4.9
Source: python desktop/results_app.py
Windows build: desktop/build_windows.cmd

Delete one data row: select it in the table, click Delete selected row, then
confirm. This removes only that row from this participant's app history. The
participant and criteria remain, including when the last row is deleted. Reports
and comparisons use the remaining rows. Arduino saves and existing files are not
changed. Re-importing a source that contains the row can add it again.
