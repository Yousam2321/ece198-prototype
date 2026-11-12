# Update and share the team project

Repository: https://github.com/Yousam2321/ece198-prototype

## GitHub Desktop

1. Install GitHub Desktop and sign in.
2. File → Clone repository → URL: paste the repository URL above; choose a local folder.
3. For later updates, close files being edited, then Fetch origin → Pull origin.
4. Open `PartA/PartA.ino` or `HoldStation/HoldStation.ino` from this cloned folder and Upload to the matching board in Arduino IDE.

Pulling source does not change firmware on a board. The Windows executable is downloaded separately from a successful Build Windows results app run under Actions. Its existing profiles remain in the same local database; source updates do not clear them.

## Team edits

```sh
git clone https://github.com/Yousam2321/ece198-prototype.git
cd ece198-prototype
git switch -c part-a-history-tests
# Edit, build and test the assigned files.
git add PartA tests docs
git commit -m "Check Part A history with the original detector"
git push -u origin part-a-history-tests
```

Open a pull request and review the changes/checks before merging. Coordinate when editing shared desktop files.

| Teammate | Suggested focus |
|---|---|
| 1 | Original Part A wiring, mounting and rep accuracy |
| 2 | Part A EEPROM, active timing, CSV and power-cycle tests |
| 3 | Existing Part B wall sit, plank and chest regressions |
| 4 | App imports, goals, progression, PDF and profile/data deletion |

Do not commit participant data, exported CSV files, credentials or generated build folders. Old implementation versions are retained in Git history rather than duplicate source directories. No project license has been selected automatically.
