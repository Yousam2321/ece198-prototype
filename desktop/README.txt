ECE198 Hold Results — Windows

Double-click ECE198_Results.exe. No Python installation is needed for the executable.

1. Enter a participant label (for example P01), then Open profile.
2. Open results CSV, or Read Arduino after finishing/saving a session and closing
   Arduino Serial Monitor. Select only that participant's records and keep them.
3. Enter the target seconds for wall sit and plank and the required consistency.
   The prefilled 90% is an editable prototype criterion, not a clinical standard.
4. Review & confirm shows every evaluation rule before you accept it.
5. Select a session to see Good, Not good yet, or Cannot evaluate, with explanations.
   View progression compares the same exercise and tolerance. Export report lets
   you choose where to save a CSV assessment.

Stats: recorded in-band hold, target completion, active time, final ending excluded,
assessed time, uncredited time during the hold, corrected consistency, and change
from the previous and first comparable attempts.

Good requires both the entered in-band duration target and the confirmed minimum
consistency. There is no hidden weighted total. Earlier out-of-band periods count
against consistency if followed by returning in band. Pauses do not count. The final
continuous departure, including unavailable final readings, is treated as the end.
Missing echoes/slow sampling can reduce recorded in-band time. This is measured
proximity to a reference and duration, not a full-body posture/medical grade.

New end-corrected evaluations require B2.4 firmware and new sessions. B2.3 records
are preserved and can show raw time, but lack final-interval metadata, so corrected
consistency is unavailable. No ending time is guessed for old records.

Your participant profiles, chosen criteria and imported raw results stay on this PC:
%LOCALAPPDATA%\ECE198_Results\results.sqlite3
Repeated imports for the same participant skip matching records. Erasing Arduino
history does not erase the PC history. Comparison uses the current confirmed rules
for all compatible attempts; import time is not the actual exercise date. Keep
participant selection and sensor mounting consistent. Close the app before backing
up the database. The app does not send participant data to a server.

Source version: python desktop/results_app.py
Build on Windows: see desktop/build_windows.cmd
