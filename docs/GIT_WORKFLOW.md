# Update and share the ECE198 project

Repository: https://github.com/Yousam2321/ece198-prototype

## Windows: set up GitHub Desktop once

1. Install GitHub Desktop from https://desktop.github.com/.
2. Open **File > Clone repository > URL**.
3. Enter `https://github.com/Yousam2321/ece198-prototype.git`.
4. Choose a new local folder, for example `C:\Users\yousa\Desktop\ece198-prototype`.
   Keep the older extracted ZIP folder separately; it may contain your CSV exports.
5. Clone. Open `HoldStation/HoldStation.ino` from this cloned folder in Arduino IDE.
   B2.2 is the station firmware version for the static completed-results screen.

## Each future firmware update

1. Finish/save the current physical session. Close Arduino IDE to avoid stale editor content.
2. In GitHub Desktop, select this repository on **main**.
3. Click **Fetch origin**, then **Pull origin** when offered.
4. Reopen the cloned folder's `HoldStation/HoldStation.ino`.
5. Select Arduino Uno and its USB port, then click **Upload**.
6. Check the OLED version label. Fetch/Pull updates your computer's files;
   the USB upload installs the firmware onto the Uno.

If Desktop reports local changes/conflicts, preserve or commit your changes before
pulling. Do not discard local work just to update. Close Serial Monitor and Python
serial scripts before uploading. Git updates do not erase CSV exports; test CSVs
are ignored by this repository. EEPROM summaries survive ordinary USB uploads,
but finish a running session before updating.

Official instructions:
- https://docs.github.com/en/desktop/adding-and-cloning-repositories/cloning-and-forking-repositories-from-github-desktop
- https://docs.github.com/en/desktop/working-with-your-remote-repository-on-github-or-github-enterprise/syncing-your-branch-in-github-desktop

## Terminal alternative (Git installed)

Run once from the parent folder where you want the project:
```sh
git clone https://github.com/Yousam2321/ece198-prototype.git
cd ece198-prototype
```

For later updates, from the cloned repository:
```sh
git pull --ff-only
```

An ordinary GitHub ZIP download is a snapshot. Clone once to use Fetch/Pull;
there is no need to download and extract a ZIP for each change.

## Team edits

Make a branch for a task, commit changes, push and open a pull request.
Coordinate before editing shared `PrototypeCore.h`. Its wearable/station copies
must stay identical; station-specific hold timing is in `HoldTiming.h`.
Run `python tests/run_tests.py` for portable tests and compile for the real board.
Do not commit identifiable participant data or credentials.
